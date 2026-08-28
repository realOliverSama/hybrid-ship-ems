%% ========================================================================
%  50_voyage_generator.m — ECMS Phase 2：参数化随机航次生成器（T-201，FR-201/202）
%  产出:
%    results/ecms_phase2/fleet/voyages_seed<ECMS_FLEET_SEED>.json  全部航次定义（入库）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）:
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/50_voyage_generator.m'))"
%  语义要点（design §2 / FR-201；v1.1 修订见 design Amendment Log）:
%    - 航段积木 7 类（离泊开头/靠泊结尾，中间 3~6 段可重复；总时长 60~120 min）；
%    - 解析预检（送达功率口径）：P = min(Hull_K_R·v_eff³/(0.97·0.95·0.60·0.97)/1000,
%      Mot_P_nom) + 100 kW，任一段 >850 kW 整航次拒绝重抽（逆水段 v_eff = v+1）；
%    - 每个航次逆水段最多 1 段（v1.1，避免多次饱和极端航次）；
%    - 种子 ECMS_FLEET_SEED（config/Model_Parameters.m，默认 20260825）固定；
%      同种子两次生成 -> 航次集逐位一致（断言，design §2 种子复现验证）。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[50] ===== ECMS Phase 2 — 参数化随机航次生成器（T-201，FR-201/202） =====\n');
fprintf('[50] ROOT = %s\n', ROOT);

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

seed = ECMS_FLEET_SEED;
N = 200;
outDir = fullfile(ROOT, 'results', 'ecms_phase2', 'fleet');
if ~exist(outDir, 'dir'), mkdir(outDir); end
jsonPath = fullfile(outDir, sprintf('voyages_seed%d.json', seed));

%% ======================= 生成 + 种子复现验证 =======================
t0 = tic;
V = ecms2.voyage_generator(seed, N);
fprintf('[50] 生成 %d 个航次（%.1f s），种子 %d\n', numel(V), toc(t0), seed);

% ---- 种子复现：同种子第二次生成，逐位一致断言（design §2）----
V2 = ecms2.voyage_generator(seed, N);
assert(isequal(V, V2), 'ECMS:T201', ...
    '种子复现失败：同种子 %d 两次生成结果不一致（随机流或算法不确定）', seed);
fprintf('[50] 种子复现验证 PASS：seed=%d 两次生成逐位一致\n', seed);

% ---- 航次质量统计 ----
durMinAll = [V.duration_min];
nSegAll = [V.n_seg];
pMaxAll = [V.P_max_est_kW];
fprintf('[50] 时长 %.0f~%.0f min（均值 %.1f）| 段数 %d~%d | P_max_est %.0f~%.0f kW\n', ...
    min(durMinAll), max(durMinAll), mean(durMinAll), min(nSegAll), max(nSegAll), ...
    min(pMaxAll), max(pMaxAll));
% 预检拒绝重抽统计
fprintf('[50] 平均重抽尝试 %.1f 次/航次（越界/超限拒绝后重抽）\n', mean([V.draw_attempts]));
% 段型分布
allSeg = [V.seg];
typesAll = {allSeg.type};
for t = {'depart','harbor','eco','rush','bridge','upstream','berth'}
    fprintf('[50]   段型 %-9s: %3d 段\n', t{1}, sum(strcmp(typesAll, t{1})));
end

%% ======================= JSON 写出（入库产物） =======================
meta = struct( ...
    'spec', 'nn-predictive-ecms-phase2 design §2 / FR-201', ...
    'simulation_generated', true, ...
    'seed', seed, 'N', numel(V), ...
    'kn2mps', 0.5144, ...
    'eta_chain', 0.97 * 0.95 * 0.60 * 0.97, ...
    'P_limit_kW', 850, 'Hull_K_R', Hull_K_R, ...
    'formula', 'P_delivered(kW) = min(Hull_K_R*v_eff^3/(0.97*0.95*0.60*0.97)/1000, Mot_P_nom=700) + 100; v_eff(逆水)=v+1; >850kW 拒绝', ...
    'precheck_revision', 'v1.1 送达功率口径（速度饱和≠降额；design Amendment Log v1.1）', ...
    'max_upstream_per_voyage', 1, ...
    'duration_min_range', [60 120], ...
    'ramp_accel_mps2', 0.15, 'ramp_decel_mps2', 0.05, ...
    'hotel_kW', 100, ...
    'param_hash', model.md5file(fullfile(ROOT, 'config', 'Model_Parameters.m')), ...
    'scenario_hash', model.md5file(fullfile(ROOT, 'config', 'Model_Scenarios.m')), ...
    'generated', datestr(now, 'yyyy-mm-dd HH:MM:SS'));

vc = cell(1, numel(V));
for i = 1:numel(V)
    jv = V(i);
    jv.seg = num2cell(jv.seg);      % struct 数组 -> cell（write_json_deep 支持嵌套）
    vc{i} = jv;
end
out = struct();
out.meta = meta;
out.voyages = vc;
ecms.write_json_deep(jsonPath, out);
fprintf('[50] 航次定义已写入 %s\n', jsonPath);

%% ======================= 回读自检（JSON 往返一致） =======================
back = jsondecode(fileread(jsonPath));
assert(numel(back.voyages) == N, 'ECMS:T201', 'JSON 回读航次数不一致');
for i = [1 round(N/2) N]
    assert(isequal(back.voyages(i).v_time_s(:).', V(i).v_time_s(:).'), ...
        'ECMS:T201', 'JSON 回读 v_time_s 不一致（voyage %d）', i);
    assert(isequal(back.voyages(i).v_value_mps(:).', V(i).v_value_mps(:).'), ...
        'ECMS:T201', 'JSON 回读 v_value_mps 不一致（voyage %d）', i);
end
fprintf('[50] JSON 回读自检 PASS（voyage 1/%d/%d 时间路点逐位一致）\n', round(N/2), N);

fprintf('\n[50] 完成：%s\n', jsonPath);
