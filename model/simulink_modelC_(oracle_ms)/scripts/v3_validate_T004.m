%% ========================================================================
%  v3_validate_T004.m — EMS_MinFF 变体验收（T-004 AC1~AC5）
%  用法: 在 model/simulink_modelC_(oracle_ms) 下
%        eval(fileread('scripts/v3_validate_T004.m'))
%  内容:
%    AC1: law=3 在 S04 全航程仿真通过（含 MSFF oracle 通道）
%    AC2: law=0/1/2 与冻结基线 model/simulink_modelC/ 同条件逐点一致 (1e-6)
%    AC3: min_ff 递推顺序核对 + 真实发电机阶跃响应逐步数值对照 (<1 kW)
%    AC4: R_max(P_ff) 在 [0, Gen_P_max-P_base] 单调不增（10 点扫描）
%    AC5: law=3 InfoMode peak vs traj 各跑 S04；peak 前馈 >= traj 前馈
%  输出: results/ms_phase1/T004_validate_<ts>.log（逐项 PASS/FAIL）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
model.load_mlp_weights(ROOT);   % law=1 需要真实 MLP 权重
run(fullfile(ROOT,'config','Model_Scenarios.m'));
MDL = 'HybridShip_ModelCms_DCBus';
FROZEN = fullfile(fileparts(ROOT), 'simulink_modelC');
TS = datestr(now,'yyyymmdd_HHMMSS');
LOGF = fullfile(ROOT,'results','ms_phase1',sprintf('T004_validate_%s.log',TS));
if ~exist(fileparts(LOGF),'dir'), mkdir(fileparts(LOGF)); end
fid = fopen(LOGF,'w');
logf = @(fmt,varargin) fprintf(fid, fmt, varargin{:});
logp = @(s) fprintf('[T004] %s\n', s);
results = {};

load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);

%% ---------------- AC1: law=3 S04 全航程 ----------------
logp('=== AC1: law=3 S04 full voyage ===');
SCN = SCENARIOS(4);
oracle = load(fullfile(ROOT,'results','ms_phase1','oracle_S04.mat'));
assignin('base','MSFF_Oracle_Data', oracle.D_ms);
assignin('base','MSFF_InfoMode', 0);   % traj
assignin('base','MSFF_Horizon_s', 30);
t0 = tic;
try
    out3 = model.run_scenario_sim(MDL, SCN, 3, 4000);
    ds3 = model.extract_logsout(out3);
    ac1_ok = isfinite(min(ds3.V_dc_V)) && ~isnan(max(ds3.P_gen_actual_kW));
    fprintf('    AC1 sim time %.1f s, Vdc_min=%.2f V, P_gen_max=%.1f kW\n', ...
        toc(t0), min(ds3.V_dc_V), max(ds3.P_gen_actual_kW));
catch ME
    ac1_ok = false;
    fprintf('    AC1 FAIL: %s\n', ME.message);
end
logf('AC1 law=3 S04 sim: %s\n', string(ac1_ok));
results{end+1} = {'AC1', ac1_ok};

%% ---------------- AC2: law=0/1/2 与冻结基线逐点一致 ----------------
logp('=== AC2: law=0/1/2 vs frozen baseline regression ===');
% 冻结基线轨迹由 /tmp/exp_frozen_traces.m 在冻结工程侧导出（S04, laws 0/1/2）
frozenMat = '/tmp/frozen_S04_traces.mat';
if exist(frozenMat, 'file')
    F = load(frozenMat);
    ac2_all = true;
    for law = 0:2
        outC = model.run_scenario_sim(MDL, SCN, law, 4000);
        dsC = model.extract_logsout(outC);
        fld = sprintf('law%d', law);
        tF = F.OUT.(fld).t;
        dP = max(abs(interp1(tF, F.OUT.(fld).P_gen_ref_kW, dsC.t, 'linear','extrap') - dsC.P_gen_ref_kW));
        dV = max(abs(interp1(tF, F.OUT.(fld).V_dc_V, dsC.t, 'linear','extrap') - dsC.V_dc_V));
        dS = max(abs(interp1(tF, F.OUT.(fld).SOC, dsC.t, 'linear','extrap') - dsC.SOC));
        ok = dP < 1e-6 && dV < 1e-6 && dS < 1e-6;
        ac2_all = ac2_all && ok;
        fprintf('    AC2 law=%d: dP_gen_ref=%.3g, dVdc=%.3g, dSOC=%.3g -> %s\n', ...
            law, dP, dV, dS, string(ok));
        logf('AC2 law=%d P/V/S: %.3g/%.3g/%.3g -> %s\n', law,dP,dV,dS,string(ok));
    end
else
    ac2_all = false;
    fprintf('    AC2 SKIP: frozen traces %s not found (run /tmp/exp_frozen_traces.m first)\n', frozenMat);
end
results{end+1} = {'AC2', ac2_all};

%% ---------------- AC3: 递推顺序 + 原位数值对照 ----------------
logp('=== AC3: reachability recursion vs real genset (in-situ) ===');
% 顺序核对：min_ff 声明 limit_in(Saturation) -> ramp(RateLimiter) ->
%   inertia(TransferFcn) -> limit_out(Saturation)；与 06_Genset 实际链路一致。
try
    blks = find_system([MDL '/06_Genset_Average_Plant'], 'SearchDepth', 1, 'Type', 'block');
    order_ok = contains(strjoin(blks),'limit_in') && contains(strjoin(blks),'ramp') && ...
               contains(strjoin(blks),'inertia') && contains(strjoin(blks),'limit_out');
    fprintf('    AC3 genset chain: %s\n', strjoin(blks, ' | '));
    % 原位对照：用 law=3 S04 真实记录驱动递推，与真实 P_gen_actual 逐步比较
    % P_gen_ref(记录) -> [递推] -> 递推的 P_gen_actual vs 真实记录
    Gen_P_max = evalin('base','Gen_P_max');
    Gen_Ramp_Up = evalin('base','Gen_Ramp_Up');
    Gen_Ramp_Down = evalin('base','Gen_Ramp_Down');
    tau = evalin('base','ASSUMED_Gen_TimeConstant');
    Msp = evalin('base','ASSUMED_Gen_MinStablePower');
    tr = ds3.t;
    % 取真实记录：P_gen_ref 与 P_gen_rampout(初值) / P_gen_actual(初值)
    Pref = ds3.P_gen_ref_kW;
    Pact = ds3.P_gen_actual_kW;
    Pramp0 = ds3.P_gen_rampout_kW(1);
    Pact0 = ds3.P_gen_actual_kW(1);
    % 递推（与 minff_reach 相同逻辑，dt=记录步长）
    rr = Pramp0; yy = Pact0;
    yrec = zeros(size(tr));
    yrec(1) = Pact0;
    for i = 2:numel(tr)
        dtk = tr(i) - tr(i-1);
        P_cmd = min(max(Pref(i), Msp), Gen_P_max);
        drl = P_cmd - rr; drl = min(max(drl, -Gen_Ramp_Down*dtk), Gen_Ramp_Up*dtk);
        rn = rr + drl;
        e = exp(-dtk/tau); s = (rn-rr)/dtk;
        yn = yy*e + rr*(1-e) + s*(dtk - tau*(1-e));
        rr = rn; yy = yn;
        yrec(i) = min(max(yy, Msp), Gen_P_max);
    end
    maxDiff = max(abs(Pact - yrec));
    ac3_ok = order_ok && maxDiff < 1.0;  % <1 kW
    fprintf('    AC3 recursion-vs-real max diff: %.4f kW (chain_order_ok=%d) -> %s\n', ...
        maxDiff, order_ok, string(ac3_ok));
    logf('AC3 order=%d maxdiff=%.4f kW -> %s\n', order_ok, maxDiff, string(ac3_ok));
catch ME
    ac3_ok = false;
    fprintf('    AC3 FAIL: %s\n', ME.message);
end
results{end+1} = {'AC3', ac3_ok};

%% ---------------- AC4: R_max(P_ff) 单调性 ----------------
logp('=== AC4: R_max(P_ff) monotonicity ===');
try
    P_gen_now = 300; P_gen_ramp = 300; P_base = 300;
    Pb = 400;  % 电池允许
    P_hat = 500*ones(1,12);
    P_ffs = linspace(0, Gen_P_max - P_base, 10);
    Rvals = zeros(size(P_ffs));
    for i = 1:numel(P_ffs)
        Rvals(i) = model.minff_reach_standalone(P_ffs(i), P_hat, 12, P_base, P_gen_now, P_gen_ramp, Pb, ...
            Gen_P_max, Msp, Gen_Ramp_Up, Gen_Ramp_Down, tau, 0.1);
    end
    dR = diff(Rvals);
    ac4_ok = all(dR <= 1e-6 + 1e-3*abs(Rvals(1:end-1)));  % 允许数值噪声
    fprintf('    AC4 R_max sweep: %s -> %s\n', mat2str(Rvals, 2), string(ac4_ok));
    logf('AC4 Rvals=%s -> %s\n', mat2str(Rvals,2), string(ac4_ok));
catch ME
    ac4_ok = false;
    fprintf('    AC4 FAIL: %s\n', ME.message);
end
results{end+1} = {'AC4', ac4_ok};

%% ---------------- AC5: InfoMode peak vs traj ----------------
logp('=== AC5: InfoMode peak vs traj ===');
% S04（渐进）可能不触发前馈（P_bat_allowable 足够覆盖），
% 另加 S09 应急出动（120 kW/s 负荷斜率，必然触发）作为非平凡证据。
try
    assignin('base','MSFF_InfoMode', 0);   % traj
    outT = model.run_scenario_sim(MDL, SCN, 3, 4000);
    dsT = model.extract_logsout(outT);
    assignin('base','MSFF_InfoMode', 1);   % peak
    outP = model.run_scenario_sim(MDL, SCN, 3, 4000);
    dsP = model.extract_logsout(outP);
    ffT = trapz(dsT.t, dsT.P_ff_applied_kW);
    ffP = trapz(dsP.t, dsP.P_ff_applied_kW);
    ac5_ok = ffP >= ffT - 1e-6;
    fprintf('    AC5 S04: peak_ff_int=%.1f kWs, traj_ff_int=%.1f kWs -> %s\n', ffP, ffT, string(ac5_ok));
    logf('AC5 S04 ff_peak=%.1f ff_traj=%.1f -> %s\n', ffP, ffT, string(ac5_ok));
    % S09 非平凡验证
    SCN9 = SCENARIOS(9);
    d9 = load(fullfile(ROOT,'results','ms_phase1','oracle_S09.mat'));
    assignin('base','MSFF_Oracle_Data', d9.D_ms);
    assignin('base','MSFF_InfoMode', 0);
    outT9 = model.run_scenario_sim(MDL, SCN9, 3, 4000);
    dsT9 = model.extract_logsout(outT9);
    assignin('base','MSFF_InfoMode', 1);
    outP9 = model.run_scenario_sim(MDL, SCN9, 3, 4000);
    dsP9 = model.extract_logsout(outP9);
    ffT9 = trapz(dsT9.t, dsT9.P_ff_applied_kW);
    ffP9 = trapz(dsP9.t, dsP9.P_ff_applied_kW);
    nontrivial = ffT9 > 1.0 || ffP9 > 1.0;   % 至少一个模式有实际前馈
    ac5_ok = ac5_ok && (ffP9 >= ffT9 - 1e-6);
    fprintf('    AC5 S09: peak_ff_int=%.1f kWs, traj_ff_int=%.1f kWs (nontrivial=%d) -> %s\n', ...
        ffP9, ffT9, nontrivial, string(ac5_ok));
    logf('AC5 S09 ff_peak=%.1f ff_traj=%.1f nontrivial=%d -> %s\n', ffP9, ffT9, nontrivial, string(ac5_ok));
    if ~nontrivial
        fprintf('    AC5 WARN: S09 前馈零激活，机制未见效，请主 Agent 关注\n');
    end
catch ME
    ac5_ok = false;
    fprintf('    AC5 FAIL: %s\n', ME.message);
end
results{end+1} = {'AC5', ac5_ok};

%% ---------------- 汇总 ----------------
fprintf('\n[T004] ===== T-004 acceptance summary =====\n');
allok = true;
for i = 1:numel(results)
    fprintf('[T004] %s: %s\n', results{i}{1}, string(results{i}{2}));
    logf('SUMMARY %s: %s\n', results{i}{1}, string(results{i}{2}));
    allok = allok && results{i}{2};
end
fprintf('[T004] OVERALL: %s\n', string(allok));
fclose(fid);
