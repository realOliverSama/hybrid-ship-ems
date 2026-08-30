%% ========================================================================
%  v5_validate_repro.m — T-004R 复现性硬门禁（R1~R4）
%  用法（两个独立 matlab -batch 进程 + 一个对照进程）：
%    V5_TAG=procA V5_MODE=run   matlab -batch "eval(fileread('scripts/v5_validate_repro.m'))"
%    V5_TAG=procB V5_MODE=run   matlab -batch "eval(fileread('scripts/v5_validate_repro.m'))"
%    V5_MODE=compare            matlab -batch "eval(fileread('scripts/v5_validate_repro.m'))"
%  门禁内容（T-004R 复现性修复验收）:
%    R1 跨会话逐位一致：law=3 CMS H30 本脚本内跑 2 次逐位一致（R1a）
%                       + 两独立进程各跑 1 次逐位一致（R1b）
%    R2 顺序无关性：Nominal S04 C0 在"先跑 5 个 law=3 后再跑"（R2a 同进程、
%                   R2b 跨进程 vs 全新会话直接跑）结果逐位一致
%    R3 law=3 激活确认（ref 级）：max|dP_gen_ref|>1 kW，ff_active_s>0，P_ff_applied max>0
%       （nominal 锚点 fuel 逐位相同属设计预期——ff 窗口内 gen 爬坡饱和，主 Agent 裁决 2026-08-21）
%    R4 Nominal 回归：C0 S04 vs 冻结基线 E0 全字段相对差 < 1e-6
%  输出: results/ms_phase1/repro/v5_<tag>.mat（ds + flags）、v5_<tag>.log、
%        compare 模式生成 v5_compare_<ts>.log（机器可读：每行 "X: PASS/FAIL"）
%  约束: 只读冻结基线；不修改控制律；不做 git 操作；不经 run_scenario_sim（全部 SimulationInput）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
run(fullfile(ROOT,'config','Model_Scenarios.m'));
MDL = 'HybridShip_ModelCms_DCBus';
FROZEN = fullfile(fileparts(ROOT), 'simulink_modelC', 'results', 'phase1', 'S04', 'E0.mat');

OUTDIR = fullfile(ROOT,'results','ms_phase1','repro');
if ~exist(OUTDIR,'dir'), mkdir(OUTDIR); end
TS = datestr(now,'yyyymmdd_HHMMSS');
MODE = getenv('V5_MODE'); if isempty(MODE), MODE = 'run'; end
TAG  = getenv('V5_TAG');  if isempty(TAG),  TAG  = 'procA'; end

%% ---------------- compare 模式（跨进程对照） ----------------
if strcmp(MODE,'compare')
    LOGF = fullfile(OUTDIR, sprintf('v5_compare_%s.log', TS));
    fid = fopen(LOGF,'w');
    logf = @(fmt,varargin) fprintf(fid, fmt, varargin{:});
    pA = load(fullfile(OUTDIR,'v5_procA.mat'));
    pB = load(fullfile(OUTDIR,'v5_procB.mat'));
    % R1b: 跨进程 law=3 CMS H30 逐位一致
    [ok1, d1] = model.ds_bitwise_equal(pA.ds_law3_1, pB.ds_law3_1);
    fprintf('R1b: %s | cross-process law3 CMS H30 %s\n', string(ok1), d1);
    logf('R1b: %s | %s\n', string(ok1), d1);
    % R2b: 跨进程 C0（A 污染后 vs B 全新会话直接跑）逐位一致
    [ok2, d2] = model.ds_bitwise_equal(pA.ds_c0_after, pB.ds_c0_fresh);
    fprintf('R2b: %s | A.c0_after vs B.c0_fresh %s\n', string(ok2), d2);
    logf('R2b: %s | %s\n', string(ok2), d2);
    % 汇总两进程自检 flags
    fA = pA.flags; fB = pB.flags;
    names = fieldnames(fA);
    allself = true;
    for i = 1:numel(names)
        va = fA.(names{i}); vb = fB.(names{i});
        ok = va && vb;
        allself = allself && ok;
        fprintf('%s(self): %s (A=%d B=%d)\n', names{i}, string(ok), va, vb);
        logf('%s: %s | A=%d B=%d\n', names{i}, string(ok), va, vb);
    end
    overall = ok1 && ok2 && allself;
    fprintf('OVERALL: %s\n', string(overall));
    logf('OVERALL: %s\n', string(overall));
    fclose(fid);
    if ~overall
        error('GATE:V5 复现性门禁 FAIL（详见 %s）', LOGF);
    end
    fprintf('[V5] 复现性对照门禁全部通过: %s\n', LOGF);
    return;
end

%% ---------------- run 模式（单进程自检 + 落盘 ds 供跨进程对照） ----------------
LOGF = fullfile(OUTDIR, sprintf('v5_%s_%s.log', TAG, TS));
fid = fopen(LOGF,'w');
logf = @(fmt,varargin) fprintf(fid, fmt, varargin{:});
logp = @(s) fprintf('[V5] %s\n', s);

load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);

SCN4 = SCENARIOS(4);
or4  = load(fullfile(ROOT,'results','ms_phase1','oracle_S04.mat'));
D4   = or4.D_ms;
genEff = struct('Gen_P_max',1000,'Gen_Ramp_Up',40,'Gen_Ramp_Down',40,'tau_gen',5);
stopFull = 5000/5.66 + 1800;
stopPoll = 600;   % 污染运行只需触发重编译，不必全航程

flags = struct();

%% R1a: law=3 CMS H30 本脚本内跑 2 次逐位一致
logp('=== R1a: in-script law=3 CMS H30 x2 bitwise ===');
t0 = tic;
ds_law3_1 = model.extract_logsout(model.run_ms_sim(MDL, SCN4, 3, 30, 0, D4, stopFull, genEff));
ds_law3_2 = model.extract_logsout(model.run_ms_sim(MDL, SCN4, 3, 30, 0, D4, stopFull, genEff));
[okR1a, dR1a] = model.ds_bitwise_equal(ds_law3_1, ds_law3_2);
flags.R1a = okR1a;
fprintf('    R1a: %s | %s (wall=%.0fs)\n', string(okR1a), dR1a, toc(t0));
logf('R1a: %s | %s\n', string(okR1a), dR1a);

%% R2a: C0 全新（同进程首跑） vs 5×law3 污染后再跑
logp('=== R2a: in-script C0 fresh vs C0 after 5x law=3 pollution ===');
t0 = tic;
ds_c0_fresh = model.extract_logsout(model.run_ms_sim(MDL, SCN4, 0, 0, 0, D4, stopFull, genEff));
% 污染：5 次 law=3，horizon/info 交替（触发参数切换重编译路径）
pollH = [10 15 45 60 30];
pollI = [1 0 1 0 1];
for i = 1:5
    model.run_ms_sim(MDL, SCN4, 3, pollH(i), pollI(i), D4, stopPoll, genEff);
end
ds_c0_after = model.extract_logsout(model.run_ms_sim(MDL, SCN4, 0, 0, 0, D4, stopFull, genEff));
[okR2a, dR2a] = model.ds_bitwise_equal(ds_c0_fresh, ds_c0_after);
flags.R2a = okR2a;
fprintf('    R2a: %s | %s (wall=%.0fs)\n', string(okR2a), dR2a, toc(t0));
logf('R2a: %s | %s\n', string(okR2a), dR2a);

%% R3: law=3 激活确认（ref 级证据；主 Agent 裁决 2026-08-21：fuel 判据在 nominal 锚点
%% 物理上不成立——ff 激活窗口内 gen 爬坡全程饱和，max|ΔP_gen_actual|=0 是设计预期的
%% “被爬坡吸收”行为。改为：ff_active>0 + max(P_ff_applied)>0 + max|ΔP_gen_ref|>1 kW）
logp('=== R3: law=3 activation evidence (ref-level) ===');
m3 = model.msff_metrics(ds_law3_1);
m0 = model.msff_metrics(ds_c0_fresh);
n0 = min(numel(ds_law3_1.t), numel(ds_c0_fresh.t));
refDiff = max(abs(ds_law3_1.P_gen_ref_kW(1:n0) - ds_c0_fresh.P_gen_ref_kW(1:n0)));
ffAct = isfinite(m3.ff_active_s) && m3.ff_active_s > 0;
ffMax = max(ds_law3_1.P_ff_applied_kW) > 0;
okR3 = refDiff > 1 && ffAct && ffMax;
flags.R3 = okR3;
fprintf('    R3: fuel3=%.6f fuel0=%.6f, max|d_ref|=%.2f kW, ff_active=%.3f s, ff_max=%.1f kW -> %s\n', ...
    m3.fuel_kg, m0.fuel_kg, refDiff, m3.ff_active_s, max(ds_law3_1.P_ff_applied_kW), string(okR3));
logf('R3: %s | fuel3=%.9f fuel0=%.9f max_d_ref=%.3f ff_active=%.3f ff_max=%.1f\n', ...
    string(okR3), m3.fuel_kg, m0.fuel_kg, refDiff, m3.ff_active_s, max(ds_law3_1.P_ff_applied_kW));

%% R4: C0 S04 vs 冻结基线 E0（1e-6）
logp('=== R4: Nominal C0 S04 vs frozen E0 ===');
F = load(FROZEN);
REGFIELDS = {'fuel_kg','Vdc_min','Vdc_max','SOC_end','SOC_min','E_bat_dis_kWh', ...
             'P_bat_rms_kW','P_bat_peak_abs_kW','t_sat_s','E_unserved_kWh','v_rmse_mps'};
maxrel = 0; worst = '';
for k = 1:numel(REGFIELDS)
    f = REGFIELDS{k};
    d = abs(F.m.(f) - m0.(f)) / max(abs(F.m.(f)), 1e-12);
    if d > maxrel, maxrel = d; worst = f; end
end
okR4 = maxrel < 1e-6;
flags.R4 = okR4;
fprintf('    R4: max-rel-diff=%.3g (worst=%s) -> %s\n', maxrel, worst, string(okR4));
logf('R4: %s | maxrel=%.3g worst=%s\n', string(okR4), maxrel, worst);

%% 落盘 ds + flags（供 compare 模式跨进程对照）
save(fullfile(OUTDIR, sprintf('v5_%s.mat', TAG)), 'ds_law3_1', 'ds_c0_fresh', 'ds_c0_after', 'flags', 'm3', 'm0');
logp(sprintf('saved v5_%s.mat + v5_%s_%s.log', TAG, TAG, TS));

allok = okR1a && okR2a && okR3 && okR4;
fprintf('\n[V5] %s 自检: R1a=%d R2a=%d R3=%d R4=%d -> %s\n', TAG, okR1a, okR2a, okR3, okR4, string(allok));
logf('OVERALL_%s: %s\n', TAG, string(allok));
fclose(fid);
if ~allok
    error('GATE:V5 %s 自检 FAIL（详见 %s）', TAG, LOGF);
end
fprintf('[V5] %s 自检全部通过: %s\n', TAG, LOGF);
