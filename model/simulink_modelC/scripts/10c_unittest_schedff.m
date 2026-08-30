%% ========================================================================
%  10c_unittest_schedff.m — T-006 C2 定时前馈变体单元级行为验证
%  U1 正常定时触发 (FR-003) / U2 来不及→立即 (FR-004)
%  U3 低SOC保守提前 (FR-005) / U4a NaN失效回退 + U4b 非全自动回退 (FR-006)
%  断言基于"离线重放 sched_ff 刷新逻辑"（用 C0 轨迹预测 t_fire），
%  与实测首次激活时刻比对，避免自证。
%  本脚本为纯脚本（无局部函数），按仓库惯例以 eval(fileread(...)) 运行。
%  输出: 图表 -> results/schedff_unittest/*.png；断言表 -> stdout
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
cd(ROOT);
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
run(fullfile(ROOT,'config','Model_Parameters.m'));
run(fullfile(ROOT,'config','Model_Scenarios.m'));
MDL = 'HybridShip_ModelC_DCBus';

W = load(fullfile(ROOT,'python','export','mlp_weights.mat'));
fns = fieldnames(W);
for i = 1:numel(fns)
    if startsWith(fns{i}, 'MLP_') && isnumeric(W.(fns{i}))
        assignin('base', fns{i}, double(W.(fns{i})));
    end
end
assignin('base', 'MLP_WeightsLoaded', 1);

load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','AI_Pred_Source',1);
assignin('base','AI_TPeak_Source',0);
assignin('base','ASSUMED_AI_DataValid',true);   % oracle 通道需要（默认 false）
assignin('base','EMS_WORK_MODE',2);

OUTDIR = fullfile(ROOT,'results','schedff_unittest');
if ~exist(OUTDIR,'dir'), mkdir(OUTDIR); end

SCN_BASE = SCENARIOS(1);            % S01 静水
SCN_BASE.voyage_distance_m = 2000;  % ~850 m / 150 s，不触发 Voyage_Stop

K_ff       = 1.0;    % ASSUMED_EMS_FF_Gain
FF_Max     = 400;    % ASSUMED_EMS_FF_Max_kW
r_max      = 40;     % Gen_Ramp_Up
tau_margin = 10;     % ASSUMED_TFF_TauMargin
P_PEAK     = 950;    % oracle 峰值（≤ Gen_P_max=1000-50，保证到位断言可行）

RES   = {};
PLOTS = {};

%% ---------- 0. C0 基线（law=0, 150 s）测 P_load 轨迹 ----------
fprintf('== [0] C0 基线 (law=0, 150 s) ==\n');
out0 = model.run_scenario_sim(MDL, SCN_BASE, 0, 150);
ds0  = model.extract_logsout(out0);
t_int = (0:floor(ds0.t(end)))';
P_load0 = interp1(ds0.t, ds0.P_load_total_kW, t_int, 'linear', 'extrap');
fprintf('[0] P_load 关键点: t=20:%.0f  40:%.0f  60:%.0f  80:%.0f  100:%.0f  120:%.0f kW\n', ...
    P_load0(21), P_load0(41), P_load0(61), P_load0(81), P_load0(101), P_load0(121));

%% ---------- 离线重放 sched_ff 刷新逻辑（U1 无偏置 / U3 低SOC偏置） ----------
% 与 CODE_SCHED_FF 完全一致的 1 Hz 刷新；两组独立事件状态并行推进
t_peak_int = max(0, 60 - t_int);
st1 = struct('active',false,'immediate',false,'tFire',0,'tPeakAbs',0,'dP',0);
st3 = st1;
fire_rec1 = -1; fire_rec3 = -1;   % 首次实际触发时刻（t 首次 ≥ tFire 或立即）
Tmax = numel(t_int) - 1;
for t = 0:Tmax
    Pl = P_load0(t+1); tp = t_peak_int(t+1);
    for bi = 1:2
        if bi == 1, st = st1; bias = false; else, st = st3; bias = true; end
        % 触发判定：用更新前的状态（更新后 immediate 分支可能改写 tFire）
        if bi == 1 && fire_rec1 < 0 && st.active && (st.immediate || t >= st.tFire - 1e-9)
            fire_rec1 = st.tFire;
        end
        if bi == 2 && fire_rec3 < 0 && st.active && (st.immediate || t >= st.tFire - 1e-9)
            fire_rec3 = st.tFire;
        end
        dP = min(max(P_PEAK - Pl, 0) * K_ff, FF_Max);
        if dP <= 0
            st.active=false; st.immediate=false; st.tFire=0; st.tPeakAbs=0; st.dP=0;
        else
            T_ramp = dP / r_max + tau_margin;
            if bias, T_ramp = T_ramp * 1.25; end
            if tp <= T_ramp
                st.active=true; st.immediate=true; st.dP=dP; st.tFire=t; st.tPeakAbs=t+tp;
            elseif st.active && ~st.immediate && t < st.tFire
                st.dP=dP; st.tFire = t + tp - T_ramp; st.tPeakAbs = t + tp;
            elseif st.active && t >= st.tFire
                st.dP = max(st.dP, dP); st.tPeakAbs = max(st.tPeakAbs, t + tp);
            else
                st.active=true; st.immediate=false; st.dP=dP; st.tFire=t+tp-T_ramp; st.tPeakAbs=t+tp;
            end
        end
        if bi == 1, st1 = st; else, st3 = st; end
    end
end
fire_pred1 = fire_rec1;   % U1 无偏置（无事件则保持 -1）
fire_pred3 = fire_rec3;   % U3 低SOC偏置

%% ---------- U1 正常定时触发 (FR-003) ----------
fprintf('\n== U1 正常定时触发 (FR-003) ==\n');
tU1 = (0:90)';
oracleU1 = [tU1, P_PEAK*ones(numel(tU1),1), max(0, 60 - tU1)];
assignin('base','AI_Oracle_Data', double(oracleU1));
SCN1 = SCN_BASE; SCN1.initial_SOC = 0.60;
out1 = model.run_scenario_sim(MDL, SCN1, 2, 80);
ds1  = model.extract_logsout(out1);
P_ff1 = ds1.P_ff_kW; t1 = ds1.t;
iFire1 = find(P_ff1 > 0.5, 1, 'first');
fire_act1 = t1(iFire1);
iW1 = t1 < (fire_pred1 - 0.5);
A11 = max(abs(P_ff1(iW1))) < 0.5;
A12 = abs(fire_act1 - fire_pred1) <= 1.5;
dP_th1 = min(max(P_PEAK - ds1.P_load_total_kW, 0)*K_ff, FF_Max);
iA1 = t1 >= fire_act1;
% 触发后 P_ff 应等于触发时刻锁存的 dP（巡航稳态下 dP≈150 恒定），并保持恒定
A13 = max(abs(P_ff1(iA1) - P_ff1(iFire1))) < 2 && abs(P_ff1(iFire1) - dP_th1(iFire1)) < 2;
Pg_ref1 = ds1.P_gen_ref_kW; Pg_ref0 = interp1(ds0.t, ds0.P_gen_ref_kW, t1, 'linear', 'extrap');
iB1 = t1 < (fire_act1 - 0.5);
A14 = max(abs(Pg_ref1(iB1) - Pg_ref0(iB1))) < 1e-9;
iG1 = t1 >= 55 & t1 <= 62;
A15 = max(ds1.P_gen_actual_kW(iG1)) >= 0.95*P_PEAK;
R1 = [A11 A12 A13 A14 A15];
RES(end+1,:) = {'U1', all(R1), sprintf('wait=%.4f act=%.2f pred=%.2f ff_const=%.4f ref0=%.2e genmax=%.1f', ...
    max(abs(P_ff1(iW1))), fire_act1, fire_pred1, max(abs(P_ff1(iA1)-P_ff1(iFire1))), ...
    max(abs(Pg_ref1(iB1)-Pg_ref0(iB1))), max(ds1.P_gen_actual_kW(iG1)))}; %#ok<AGROW>
PLOTS(end+1,:) = {t1, ds1, fire_act1, 'U1_normal_trigger'}; %#ok<AGROW>

%% ---------- U2 来不及→立即模式 (FR-004) ----------
fprintf('\n== U2 来不及→立即 (FR-004) ==\n');
tU2 = (0:30)';
oracleU2 = [tU2, P_PEAK*ones(numel(tU2),1), max(0, 10 - tU2)];
assignin('base','AI_Oracle_Data', double(oracleU2));
SCN2 = SCN_BASE; SCN2.initial_SOC = 0.60;
out2c = model.run_scenario_sim(MDL, SCN2, 2, 30);   % C2
ds2c  = model.extract_logsout(out2c);
out2b = model.run_scenario_sim(MDL, SCN2, 1, 30);   % C1 对照（同 oracle）
ds2b  = model.extract_logsout(out2b);
P_ff2 = ds2c.P_ff_kW; t2 = ds2c.t;
iFire2 = find(P_ff2 > 0.5, 1, 'first');
fire2 = t2(iFire2);
A21 = fire2 < 3;
dP2_t = min(max(P_PEAK - ds2c.P_load_total_kW, 0)*K_ff, FF_Max);
% 立即模式：ev_dP 在每个 1 Hz 刷新被改写（无 ratchet），P_ff 为 floor(t) 时刻
% 刷新的 dP 的分段保持值 → 与 dP_theory(floor(t)) 逐点一致
Pl_floor = interp1(t2, ds2c.P_load_total_kW, floor(t2), 'linear', 'extrap');
dP2_floor = min(max(P_PEAK - Pl_floor, 0)*K_ff, FF_Max);
iE2 = t2 <= 5;
A22 = max(abs(P_ff2(iE2) - dP2_floor(iE2))) < 5;
ff1_C1 = ds2b.P_gen_ref_kW - ds2b.P_load_total_kW;
ff_C2 = ds2c.P_gen_ref_kW - ds2c.P_load_total_kW;
iC2 = t2 <= 15;
A23 = all(ff_C2(iC2) >= ff1_C1(iC2) - 1e-9);
R2 = [A21 A22 A23];
RES(end+1,:) = {'U2', all(R2), sprintf('first=%.2f dP_err=%.2f conservative=%d', fire2, ...
    max(abs(P_ff2(iE2) - dP2_floor(iE2))), A23)}; %#ok<AGROW>
PLOTS(end+1,:) = {t2, ds2c, fire2, 'U2_immediate'}; %#ok<AGROW>

%% ---------- U3 低SOC保守提前 (FR-005) ----------
fprintf('\n== U3 低SOC提前 (FR-005) ==\n');
tU3 = (0:90)';
oracleU3 = [tU3, P_PEAK*ones(numel(tU3),1), max(0, 60 - tU3)];
assignin('base','AI_Oracle_Data', double(oracleU3));
SCN3 = SCN_BASE; SCN3.initial_SOC = 0.35;
out3 = model.run_scenario_sim(MDL, SCN3, 2, 80);
ds3  = model.extract_logsout(out3);
P_ff3 = ds3.P_ff_kW; t3 = ds3.t;
iFire3 = find(P_ff3 > 0.5, 1, 'first');
fire3 = t3(iFire3);
A31 = abs(fire3 - fire_pred3) <= 1.5;
A32 = fire3 < fire_act1 - 2;
iW3 = t3 < (fire_pred3 - 0.5);
A33 = max(abs(P_ff3(iW3))) < 0.5;
R3 = [A31 A32 A33];
RES(end+1,:) = {'U3', all(R3), sprintf('act=%.2f pred=%.2f (U1=%.2f, delta=%.2f) wait=%.4f', ...
    fire3, fire_pred3, fire_act1, fire_act1-fire3, max(abs(P_ff3(iW3))))}; %#ok<AGROW>
PLOTS(end+1,:) = {t3, ds3, fire3, 'U3_lowSOC'}; %#ok<AGROW>

%% ---------- U4a 预测数据失效（oracle 含 NaN → valid=0）→ 回退 C0 ----------
fprintf('\n== U4a NaN失效回退 (FR-006) ==\n');
tU4 = (0:60)';
oracleU4 = [tU4, NaN*ones(numel(tU4),1), 10*ones(numel(tU4),1)];
assignin('base','AI_Oracle_Data', double(oracleU4));
SCN4 = SCN_BASE; SCN4.initial_SOC = 0.60;
assignin('base','EMS_WORK_MODE',2);
out4c = model.run_scenario_sim(MDL, SCN4, 2, 60);
ds4c  = model.extract_logsout(out4c);
out4b = model.run_scenario_sim(MDL, SCN4, 0, 60);
ds4b  = model.extract_logsout(out4b);
t4 = ds4c.t;
Pg4c = ds4c.P_gen_ref_kW; Pg4b = interp1(ds4b.t, ds4b.P_gen_ref_kW, t4, 'linear','extrap');
A41 = max(abs(ds4c.P_ff_kW)) < 1e-9;
A42 = max(abs(Pg4c - Pg4b)) < 1e-9;
R4 = [A41 A42];
RES(end+1,:) = {'U4a', all(R4), sprintf('P_ff_max=%.2e dPg=%.2e valid_max=%.1f', ...
    max(abs(ds4c.P_ff_kW)), max(abs(Pg4c-Pg4b)), max(ds4c.AI_valid))}; %#ok<AGROW>
PLOTS(end+1,:) = {t4, ds4c, -1, 'U4a_NAN_fallback'}; %#ok<AGROW>

%% ---------- U4b 非全自动（EMS_WORK_MODE=1）→ gate 归零回退 ----------
fprintf('\n== U4b 非全自动回退 (FR-006) ==\n');
assignin('base','AI_Oracle_Data', double(oracleU1));
assignin('base','EMS_WORK_MODE',1);
SCN5 = SCN_BASE; SCN5.initial_SOC = 0.60;
out5c = model.run_scenario_sim(MDL, SCN5, 2, 60);
ds5c  = model.extract_logsout(out5c);
out5b = model.run_scenario_sim(MDL, SCN5, 0, 60);
ds5b  = model.extract_logsout(out5b);
t5 = ds5c.t;
Pg5c = ds5c.P_gen_ref_kW; Pg5b = interp1(ds5b.t, ds5b.P_gen_ref_kW, t5, 'linear','extrap');
A51 = max(abs(Pg5c - Pg5b)) < 1e-9;
% 注：P_ff_kW 为 gate 前 sched_ff 内部信号（WORK_MODE=1 时仍可能非零）；
% 生效前馈经 gate 归零，故仅以 P_gen_ref==Baseline 为 FR-006 判据
R5 = [A51];
RES(end+1,:) = {'U4b', all(R5), sprintf('dPg=%.2e (P_ff_pre-gate max=%.2e)', ...
    max(abs(Pg5c-Pg5b)), max(abs(ds5c.P_ff_kW)))}; %#ok<AGROW>
PLOTS(end+1,:) = {t5, ds5c, -1, 'U4b_semiauto_fallback'}; %#ok<AGROW>

%% ---------- 出图 ----------
for k = 1:size(PLOTS,1)
    t = PLOTS{k,1}; ds = PLOTS{k,2}; tfire = PLOTS{k,3}; nm = PLOTS{k,4};
    fig = figure('Visible','off','Position',[100 100 1000 800]);
    subplot(4,1,1); plot(t, ds.P_load_total_kW, 'k-', t, ds.P_pred_max_kW, 'b--');
    ylabel('kW'); legend('P_load','P_pred','Location','best'); title(nm,'Interpreter','none');
    if tfire > 0, xline(tfire,'r--','t_fire'); end
    subplot(4,1,2); plot(t, ds.P_ff_kW, 'r-'); ylabel('P_ff kW');
    if tfire > 0, xline(tfire,'r--','t_fire'); end
    subplot(4,1,3); plot(t, ds.P_gen_ref_kW, 'b-', t, ds.P_gen_actual_kW, 'g-');
    ylabel('kW'); legend('P_gen_ref','P_gen_actual','Location','best');
    if tfire > 0, xline(tfire,'r--','t_fire'); end
    subplot(4,1,4); plot(t, ds.V_dc_V, 'm-'); ylabel('V_dc V');
    if tfire > 0, xline(tfire,'r--','t_fire'); end
    xlabel('t [s]');
    saveas(fig, fullfile(OUTDIR, [nm '.png']));
    close(fig);
end

%% ---------- 汇总 ----------
fprintf('\n========== T-006 单元验证汇总 ==========\n');
allpass = true;
for k = 1:size(RES,1)
    s = RES{k,2}; if s, st='PASS'; else, st='FAIL'; end
    if ~s, allpass = false; end
    fprintf('%-4s  %s   %s\n', RES{k,1}, st, RES{k,3});
end
fprintf('总体: %s\n', string(allpass));
fprintf('图表: %s\n', OUTDIR);
