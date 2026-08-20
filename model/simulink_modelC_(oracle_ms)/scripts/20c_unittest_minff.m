%% ========================================================================
%  20c_unittest_minff.m — EMS_MinFF 单元验证（T-005, U1~U6）
%  用法: 在 model/simulink_modelC_(oracle_ms) 下
%        eval(fileread('scripts/20c_unittest_minff.m'))
%  内容（design.md Testing Strategy / AC-2）:
%    U1: 可达性前向递推 vs 解析解（限幅->爬坡->一阶惯性->限幅），节点误差 < 1 kW
%    U2: 全程 R(τ)<=0 时 law=3 与 law=0 Baseline 逐点一致（1e-6）
%    U3: 合成阶跃下 P_ff* = 手算（解析）最小值 ±0.5 kW；施加后 R(τ)<=0；界内
%        + 模型级交叉核对（S04 真实轨迹, 决策时刻 replica vs logged P_ff_raw）
%    U4: C_P 折叠一致性（InfoMode=peak 折叠 == 恒值输入；折叠仅改信息表示）
%        + 模型级 peak>=traj 逐决策时刻核对
%    U5: f_SOC/f_V/P_bat_allowable 折点取值、线性、单调性
%    U6: 预测失效（oracle NaN）-> pred_valid=0, 输出==Baseline, 无效事件计数递增
%  输出: results/ms_phase1/unittest/U_validate_<ts>.log + PNG
%  约束: 只读冻结基线；不修改 EMS_MinFF 控制律；不做 git 操作
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
run(fullfile(ROOT,'config','Model_Scenarios.m'));
MDL = 'HybridShip_ModelCms_DCBus';

OUTDIR = fullfile(ROOT,'results','ms_phase1','unittest');
if ~exist(OUTDIR,'dir'), mkdir(OUTDIR); end
TS = datestr(now,'yyyymmdd_HHMMSS');
LOGF = fullfile(OUTDIR, sprintf('U_validate_%s.log', TS));
fid = fopen(LOGF,'w');
logf = @(fmt,varargin) fprintf(fid, fmt, varargin{:});
logp = @(s) fprintf('[U] %s\n', s);

% ---- 运行参数（与 Model_Parameters 同源） ----
Pmax   = evalin('base','Gen_P_max');
Rup    = evalin('base','Gen_Ramp_Up');
Rdn    = evalin('base','Gen_Ramp_Down');
tau    = evalin('base','ASSUMED_Gen_TimeConstant');
Msp    = evalin('base','ASSUMED_Gen_MinStablePower');
BatLim = evalin('base','ASSUMED_Bat_DischargeLimit');
dt_reach = evalin('base','MSFF_dt_reach');
SOCK   = evalin('base','MSFF_SOC_Knee');
VDCK   = evalin('base','MSFF_Vdc_Knee');
Gain   = evalin('base','ASSUMED_EMS_SOC_Gain');
SOCRef = evalin('base','ASSUMED_EMS_SOC_Ref');
H_s    = evalin('base','MSFF_Horizon_s');
n_max  = 12;
n_use  = max(1, min(12, round(H_s/5)));

load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);

U = model.minff_unit();   % T-005 单元验证辅助函数（+model/minff_unit.m）

% 显式修正 min_ff 输出端口日志名：build 的 logPort 依赖信号线命名，
% 但 MATLAB Function 块第 1 个输出端口（P_ff_raw）Name 为空导致 logsout
% 元素无名字被 extract_logsout 跳过；直接设端口 Name（已验证生效）
mf = [MDL '/05_EMS_Controller/EMS_MinFF/min_ff'];
ph = get_param(mf, 'PortHandles');
set_param(ph.Outport(1), 'Name', 'P_ff_raw_kW');

results = {};


%% ========================================================================
%  U1: 可达性前向递推 vs 解析解
% ========================================================================
logp('=== U1: reachability recursion vs analytic solution ===');
u1_ok = true;
cases = {
    % {name, P_base, P_ff, P_gen_now, P_gen_ramp, Pb}
    {'step-up-200',  400, 200, 400, 400, 0}
    {'step-up-sat',  900, 200, 400, 400, 0}
    {'ramp-down',    200,   0, 400, 400, 0}
    {'no-change',    400,   0, 400, 400, 0}
    {'clamp-msp',    300, 100,  50,  50, 0}
    };
u1_detail = '';
for ci = 1:numel(cases)
    C = cases{ci};
    P_base = C{2}; P_ff = C{3}; yy0 = C{4}; rr0 = C{5}; Pb = C{6};
    P_cmd = min(max(min(P_base+P_ff, Pmax), Msp), Pmax);
    nodes = 5:5:60;
    Rrec = zeros(1,12);
    for k = 1:12
        Rrec(k) = U.reach_node(P_ff, repmat(P_base+P_ff,1,12), 12, P_base, yy0, rr0, Pb, ...
            Pmax, Msp, Rup, Rdn, tau, dt_reach, k);
    end
    Ran = U.analytic_reach(nodes, P_cmd, rr0, yy0, Rup, Rdn, tau, Msp, Pmax);
    err = max(abs(Rrec - Ran));
    ok = err < 1.0;
    u1_ok = u1_ok && ok;
    u1_detail = sprintf('%s  |  %s: err=%.4f kW -> %s', u1_detail, C{1}, err, string(ok));
end
if u1_ok, u1_detail = sprintf('all cases < 1 kW%s', u1_detail); end
results{end+1} = {'U1', u1_ok};
fprintf('    U1: %s\n', string(u1_ok));
logf('U1: %s | %s\n', string(u1_ok), u1_detail);

% U1 图：case 1 轨迹对照
t_grid = 0:0.1:60;
P_cmd1 = min(max(min(400+200, Pmax), Msp), Pmax);
Rrec1 = zeros(size(t_grid));
for k = 1:numel(t_grid)
    % 逐节点递推至 t_grid(k)：用细网格对照
    Rrec1(k) = U.reach_at(t_grid(k), 200, 400, 400, 400, 0, Pmax, Msp, Rup, Rdn, tau, 0.1);
end
Ran1 = U.analytic_reach(t_grid, P_cmd1, 400, 400, Rup, Rdn, tau, Msp, Pmax);
fig = figure('Visible','off');
plot(t_grid, Rrec1, 'b-', t_grid, Ran1, 'r--', 'LineWidth', 1.2);
legend('recursion dt=0.1', 'analytic', 'Location','best');
xlabel('\tau [s]'); ylabel('P_{gen,reachable} [kW]'); grid on;
title('U1: reachability recursion vs analytic (step-up 200 kW)');
saveas(fig, fullfile(OUTDIR,'U1_reach_vs_analytic.png')); close(fig);

%% ========================================================================
%  U2: 全程 R(τ)<=0 时 law=3 == law=0（低负荷 oracle）
% ========================================================================
logp('=== U2: zero-intervention consistency (law=3 == law=0) ===');
SCN1 = SCENARIOS(1);
stop2 = 300;
% 低负荷 oracle：P_hat = 80 kW 恒值（低于任何节点可达+电池裕度）
t_or = (0:1:stop2)';
D_low = [t_or, repmat(80, numel(t_or), 12)];
assignin('base','MSFF_Oracle_Data', D_low);
assignin('base','MSFF_InfoMode', 0);

out0 = model.run_scenario_sim(MDL, SCN1, 0, stop2);
ds0 = model.extract_logsout(out0);
out3 = model.run_scenario_sim(MDL, SCN1, 3, stop2);
ds3 = model.extract_logsout(out3);

t0 = ds0.t; t3 = ds3.t;
Pg0 = interp1(t0, ds0.P_gen_ref_kW, t3, 'linear','extrap');
dPg = max(abs(Pg0 - ds3.P_gen_ref_kW));
Rmax_all = ds3.R_max_kW;
u2a = dPg < 1e-6;
u2b = all(Rmax_all <= 1e-6);
u2c = max(abs(ds3.P_ff_raw_kW)) < 1e-6;
u2_ok = u2a && u2b && u2c;
fprintf('    U2: max|dP_gen_ref|=%.3g (1e-6: %d), max R_max=%.3g, max P_ff_raw=%.3g -> %s\n', ...
    dPg, u2a, max(Rmax_all), max(abs(ds3.P_ff_raw_kW)), string(u2_ok));
logf('U2: %s | dP_gen_ref=%.3g R_max_max=%.3g P_ff_raw_max=%.3g\n', ...
    string(u2_ok), dPg, max(Rmax_all), max(abs(ds3.P_ff_raw_kW)));
results{end+1} = {'U2', u2_ok};

%% ========================================================================
%  U3: 最小前馈正确性（解析手算 vs 模型逻辑 replica，±0.5 kW）
% ========================================================================
logp('=== U3: minimal P_ff* = analytic hand-computed ===');
% 手算场景：稳态 400 kW，P_hat=950 恒值，Pb=500（SOC=0.375->fS=0.5, Vdc=720->fV=1, 1000*0.5）
% 绑定节点为 τ=5：P_reach(5;P_ff*) = L - Pb = 450（≤ τ=5 可达上限 473.6，可行）
P_base3 = 400; yy3 = 400; rr3 = 400; L3 = 950; Pb3 = 500;
% 解析求解：fzero 求 P_ff* 使 U.analytic_reach(5, min(400+P_ff,1000)) = 450
ffun = @(ff) U.analytic_reach(5, min(max(min(P_base3+ff, Pmax),Msp),Pmax), ...
    rr3, yy3, Rup, Rdn, tau, Msp, Pmax) - (L3 - Pb3);
ff_hand = fzero(ffun, [40 200]);   % 解析根（ff∈[40,200] 区间单调）
% 模型逻辑 replica（与 min_ff 完全相同的二分+reach）
P_hat3 = repmat(L3, 1, 12);
[ff_replica, R0_rep] = U.replica_minff(P_hat3, n_use, P_base3, yy3, rr3, ...
    0.375, 720, SOCK, VDCK, BatLim, Pmax, Msp, Rup, Rdn, tau, dt_reach, false);
% 施加后 R(τ)<=0 核对（独立复算）
R_after = max(model.minff_reach_standalone(ff_replica, P_hat3, n_use, P_base3, yy3, rr3, Pb3, ...
    Pmax, Msp, Rup, Rdn, tau, dt_reach));
bounds_ok = ff_replica >= 0 && ff_replica <= Pmax - P_base3 + 1e-6;
u3_ok = abs(ff_replica - ff_hand) <= 0.5 && R_after <= 0.05 && bounds_ok && R0_rep > 0;
fprintf('    U3: P_ff_hand=%.3f kW, P_ff_replica=%.3f kW, |diff|=%.4f, R_after=%.3f, bounds=%d -> %s\n', ...
    ff_hand, ff_replica, abs(ff_replica-ff_hand), R_after, bounds_ok, string(u3_ok));
logf('U3: %s | hand=%.3f replica=%.3f diff=%.4f R_after=%.3f bounds=%d\n', ...
    string(u3_ok), ff_hand, ff_replica, abs(ff_replica-ff_hand), R_after, bounds_ok);
results{end+1} = {'U3', u3_ok};

% U3 图：施加后 R(τ)（应为 ≤0）
nodes = 5:5:30;
P_reach3 = U.analytic_reach(nodes, min(max(min(P_base3+ff_replica, Pmax),Msp),Pmax), ...
    rr3, yy3, Rup, Rdn, tau, Msp, Pmax);
fig = figure('Visible','off');
bar(nodes, L3 - P_reach3 - Pb3); hold on;
yline(0, 'k--'); xlabel('\tau [s]'); ylabel('R(\tau) [kW]');
title(sprintf('U3: R(\\tau) after P_{ff}^* = %.2f kW (should be <= 0)', ff_replica));
grid on; saveas(fig, fullfile(OUTDIR,'U3_R_after_ffstar.png')); close(fig);

%% ========================================================================
%  U4: C_P 折叠一致性（算法级 + 模型级）
% ========================================================================
logp('=== U4: InfoMode=peak fold consistency ===');
% 算法级：折叠 == 恒值 max 输入（同一 decision 路径）；max 取前 n 个节点
P_hat4 = [500 400 300 200 100 50 900 900 900 900 900 900];
n4 = 6; P_b4 = 300; y4 = 300; r4 = 300;
[ff_fold, ~] = U.replica_minff(P_hat4, n4, P_b4, y4, r4, 0.375, 720, ...
    SOCK, VDCK, BatLim, Pmax, Msp, Rup, Rdn, tau, dt_reach, true);
pk6 = max(P_hat4(1:n4));          % 折叠应取前 6 节点 max = 500
pk12 = max(P_hat4(1:12));         % 若错误取全 12 节点 max = 900
[ff_const6, ~] = U.replica_minff(repmat(pk6,1,12), n4, P_b4, y4, r4, 0.375, 720, ...
    SOCK, VDCK, BatLim, Pmax, Msp, Rup, Rdn, tau, dt_reach, false);
[ff_const12, ~] = U.replica_minff(repmat(pk12,1,12), n4, P_b4, y4, r4, 0.375, 720, ...
    SOCK, VDCK, BatLim, Pmax, Msp, Rup, Rdn, tau, dt_reach, false);
u4a = ff_fold == ff_const6;                       % 折叠 == 恒值 pk6 输入

u4b = ff_fold ~= ff_const12;                      % 折叠必须取前 n 节点（非全 12）
% 折叠只改信息表示：其余参数一致（P_base/gen/Pb 同参，见调用签名一致）
u4c_pre = (ff_fold >= 0);
% 模型级：S04 真实轨迹，peak 模式与 traj 模式逐 1 Hz 决策时刻核对
SCN4 = SCENARIOS(4);
stop4 = 300;
or4 = load(fullfile(ROOT,'results','ms_phase1','oracle_S04.mat'));
D4 = or4.D_ms;
assignin('base','MSFF_Oracle_Data', D4);
assignin('base','MSFF_InfoMode', 0);
outT = model.run_scenario_sim(MDL, SCN4, 3, stop4);
dsT = model.extract_logsout(outT);
assignin('base','MSFF_InfoMode', 1);
outP = model.run_scenario_sim(MDL, SCN4, 3, stop4);
dsP = model.extract_logsout(outP);
% 逐决策时刻（t=0..min(len)）peak_ff >= traj_ff（折叠 max >= 各节点）
dtS = min(dsT.t(end), dsP.t(end));
tt = (0:1:floor(dtS))';
ffT = interp1(dsT.t, dsT.P_ff_raw_kW, tt, 'linear','extrap');
ffP = interp1(dsP.t, dsP.P_ff_raw_kW, tt, 'linear','extrap');
u4c = all(ffP >= ffT - 1e-6);
% 折叠后 peak 决策与 replica(fold) 核对（整数决策时刻 t_d=0,1,2,...）
%   模型 min_ff 在 1 Hz 决策（lastT 机制），P_ff_raw 在决策时刻更新；
%   用 t_d 时刻状态 + 折叠输入复核，与 logged P_ff_raw 比较（容差 1 kW，
%   覆盖 0.01s 单位延时反馈的输入差异）
u4d = true; nCheck = 0; nNonTriv = 0;
td = (0:1:floor(min(dsT.t(end),dsP.t(end))))';
for ti = 1:numel(td)
    tq = td(ti);
    P_load = interp1(dsT.t, dsT.P_load_total_kW, tq, 'linear','extrap');
    SOC = interp1(dsT.t, dsT.SOC, tq, 'linear','extrap');
    Vdc = interp1(dsT.t, dsT.V_dc_V, tq, 'linear','extrap');
    Pgn = interp1(dsT.t, dsT.P_gen_actual_kW, tq, 'linear','extrap');
    Pgr = interp1(dsT.t, dsT.P_gen_rampout_kW, tq, 'linear','extrap');
    P_base = P_load + Gain*(SOCRef - SOC);
    P_hatq = interp1(D4(:,1), D4(:,2:13), tq, 'linear','extrap');
    [ffq, R0q] = U.replica_minff(P_hatq, n_use, P_base, Pgn, Pgr, SOC, Vdc, ...
        SOCK, VDCK, BatLim, Pmax, Msp, Rup, Rdn, tau, dt_reach, true); % peak 折叠
    ffm = interp1(dsP.t, dsP.P_ff_raw_kW, tq, 'linear','extrap');
    if abs(ffq - ffm) > 1.0
        u4d = false;
    end
    nCheck = nCheck + 1;
    if R0q > 0, nNonTriv = nNonTriv + 1; end
end
u4d = u4d && nCheck >= 10 && nNonTriv >= 5;   % 至少 10 个决策时刻、其中 ≥5 个有缺口
u4_ok = u4a && u4b && u4c_pre && u4c && u4d;
fprintf('    U4: fold==const6(%d), fold!=const12(%d), model peak>=traj(%d), replica-vs-logged(%d, n=%d) -> %s\n', ...
    u4a, u4b, u4c, u4d, nCheck, string(u4_ok));
logf('U4: %s | fold==const6:%d fold_neq12:%d peak_ge_traj:%d replica_logged:%d n=%d\n', ...
    string(u4_ok), u4a, u4b, u4c, u4d, nCheck);
results{end+1} = {'U4', u4_ok};

%% ========================================================================
%  U3 补充（模型级交叉核对，放在 U4 数据之后复用 dsT）
% ========================================================================
% 在 traj 运行整数决策时刻，用 replica(traj 输入, 不折叠) 复核 logged P_ff_raw
u3m_ok = true; nChk3 = 0; nNonTriv3 = 0;
for ti = 1:numel(td)
    tq = td(ti);
    P_load = interp1(dsT.t, dsT.P_load_total_kW, tq, 'linear','extrap');
    SOC = interp1(dsT.t, dsT.SOC, tq, 'linear','extrap');
    Vdc = interp1(dsT.t, dsT.V_dc_V, tq, 'linear','extrap');
    Pgn = interp1(dsT.t, dsT.P_gen_actual_kW, tq, 'linear','extrap');
    Pgr = interp1(dsT.t, dsT.P_gen_rampout_kW, tq, 'linear','extrap');
    P_base = P_load + Gain*(SOCRef - SOC);
    P_hatq = interp1(D4(:,1), D4(:,2:13), tq, 'linear','extrap');
    [ffq, R0q] = U.replica_minff(P_hatq, n_use, P_base, Pgn, Pgr, SOC, Vdc, ...
        SOCK, VDCK, BatLim, Pmax, Msp, Rup, Rdn, tau, dt_reach, false);
    ffm = interp1(dsT.t, dsT.P_ff_raw_kW, tq, 'linear','extrap');
    if abs(ffq - ffm) > 1.0
        u3m_ok = false;
    end
    nChk3 = nChk3 + 1;
    if R0q > 0, nNonTriv3 = nNonTriv3 + 1; end
end
u3m_ok = u3m_ok && nChk3 >= 10 && nNonTriv3 >= 5;
fprintf('    U3-model: replica vs logged P_ff_raw @ %d decision instants (non-trivial=%d) -> %s\n', nChk3, nNonTriv3, string(u3m_ok));
logf('U3-model: %s | n=%d nontriv=%d\n', string(u3m_ok), nChk3, nNonTriv3);
results{end+1} = {'U3m', u3m_ok};

%% ========================================================================
%  U5: 折点与单调性
% ========================================================================
logp('=== U5: knee points & monotonicity ===');
SOCgrid = [0.25 0.30 0.375 0.45 0.50];
VDCgrid = [680 690 705 720 730];
fS = zeros(size(SOCgrid));
for i = 1:numel(SOCgrid), fS(i) = U.fSOC(SOCgrid(i), SOCK); end
fV = zeros(size(VDCgrid));
for j = 1:numel(VDCgrid), fV(j) = U.fVdc(VDCgrid(j), VDCK); end
Pb5 = BatLim * fS' * fV;
% 折点取值
u5a = abs(fS(2) - 0) < 1e-12 && abs(fS(4) - 1) < 1e-12 && abs(fS(3) - 0.5) < 1e-12;
u5b = abs(fV(2) - 0) < 1e-12 && abs(fV(4) - 1) < 1e-12 && abs(fV(3) - 0.5) < 1e-12;
% 线性（折点间中点）
u5c = abs(fS(3) - 0.5*(fS(2)+fS(4))) < 1e-12 && abs(fV(3) - 0.5*(fV(2)+fV(4))) < 1e-12;
% 单调不减（SOC 方向、Vdc 方向、联合）
u5d = all(diff(fS) >= -1e-12) && all(diff(fV) >= -1e-12) && ...
      all(all(diff(Pb5,1,2) >= -1e-12)) && all(all(diff(Pb5,1,1) >= -1e-12));
u5_ok = u5a && u5b && u5c && u5d;
fprintf('    U5: fS=[%s], fV=[%s], knee/linear/mono=%d/%d/%d -> %s\n', ...
    mat2str(fS,3), mat2str(fV,3), u5a&&u5b, u5c, u5d, string(u5_ok));
logf('U5: %s | fS=%s fV=%s Pb=%s\n', string(u5_ok), mat2str(fS,4), mat2str(fV,4), mat2str(Pb5,4));
results{end+1} = {'U5', u5_ok};

% U5 图：Pb 曲面
fig = figure('Visible','off');
surf(SOCgrid, VDCgrid, Pb5'); xlabel('SOC'); ylabel('Vdc [V]'); zlabel('P_{bat,allowable} [kW]');
title('U5: P_{bat,allowable}(SOC, Vdc)'); grid on;
saveas(fig, fullfile(OUTDIR,'U5_Pbat_allowable_surface.png')); close(fig);

%% ========================================================================
%  U6: 预测失效退化
% ========================================================================
logp('=== U6: prediction invalid -> baseline ===');
stop6 = 300;
% oracle 前 120 s 有效（80 kW），之后 NaN（模拟数据缺失/越界）
t_or6 = (0:1:stop6)';
D6 = [t_or6, repmat(80, numel(t_or6), 12)];
D6(t_or6 > 120, 2:13) = NaN;
assignin('base','MSFF_Oracle_Data', D6);
assignin('base','MSFF_InfoMode', 0);
out6 = model.run_scenario_sim(MDL, SCN1, 3, stop6);
ds6 = model.extract_logsout(out6);
% pred_valid 信号
pv = interp1(ds6.t, ds6.MSFF_pred_valid, t_or6, 'linear','extrap');
% 无效事件计数：pred_valid 1->0 跳变次数
nEvents = 0;
for k = 2:numel(pv)
    if pv(k) < 0.5 && pv(k-1) > 0.5, nEvents = nEvents + 1; end
end
% t>120 后与 law=0 逐点一致
t6 = ds6.t;
Pgb = interp1(t0, ds0.P_gen_ref_kW, t6, 'linear','extrap');
mask = t6 > 120;
dPg6 = max(abs(Pgb(mask) - ds6.P_gen_ref_kW(mask)));
ff6 = interp1(ds6.t, ds6.P_ff_applied_kW, t6, 'linear','extrap');
u6a = pv(1) > 0.5 && all(pv(t_or6 > 120) < 0.5);   % 有效->无效转移
u6b = dPg6 < 1e-6;
u6c = nEvents >= 1;
u6d = max(abs(ff6(mask))) < 1e-6;
u6_ok = u6a && u6b && u6c && u6d;
fprintf('    U6: pred_valid transition ok(%d), dPg(after 120s)=%.3g, events=%d, ff_applied=%.3g -> %s\n', ...
    u6a, dPg6, nEvents, max(abs(ff6(mask))), string(u6_ok));
logf('U6: %s | valid_trans:%d dPg=%.3g events=%d ff=%.3g\n', ...
    string(u6_ok), u6a, dPg6, nEvents, max(abs(ff6(mask))));
results{end+1} = {'U6', u6_ok};

%% ========================================================================
%  汇总
% ========================================================================
fprintf('\n[U] ===== T-005 unit validation summary =====\n');
allok = true;
for i = 1:numel(results)
    fprintf('[U] %-5s: %s\n', results{i}{1}, string(results{i}{2}));
    allok = allok && results{i}{2};
end
fprintf('[U] OVERALL: %s\n', string(allok));
logf('OVERALL: %s\n', string(allok));
fclose(fid);
if ~allok
    error('U-UNIT:FAIL 单元验证未全 PASS，禁止进入实验阶段');
end
