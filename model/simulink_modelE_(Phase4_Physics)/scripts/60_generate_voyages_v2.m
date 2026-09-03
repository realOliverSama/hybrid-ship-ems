%% ========================================================================
%  60_generate_voyages_v2.m — Phase 2.5：数据集 v2 参数化随机航次生成器（T-302，FR-301）
%  依据：.specs/nn-predictive-ecms-phase25/t301_dataset_v2_perturbation.md（M0~M3）
%  产出:
%    results/ecms_phase25/fleet/voyages_seed<V2_FLEET_SEED>.json  全部航次定义（入库）
%  执行（数字开头脚本须 eval(fileread()) 方式运行）:
%    cd model/simulink_modelD && matlab -batch \
%      "eval(fileread('scripts/60_generate_voyages_v2.m'))"
%  语义要点（t301 文档 / FR-301）:
%    - 段型积木 7 类同 v1；速度/水流改范围随机（M1/M2）；k_res 航次级 U[0.95,1.15]（M3）；
%    - M0 硬规则：非 upstream 段 P_calc>700 kW ⇒ 整航次拒绝重抽；
%      upstream 段 V_rel≤7.06 m/s（允许饱和，v1 语义）；
%    - 种子默认 20260829（V2_FLEET_SEED 可覆盖）；N 默认 400（V2_FLEET_N 可覆盖）；
%    - 同种子两次生成 -> 航次集逐位一致（断言）。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[60] ===== Phase 2.5 — 数据集 v2 参数化随机航次生成器（T-302，FR-301） =====\n');
fprintf('[60] ROOT = %s\n', ROOT);

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

if exist('V2_FLEET_SEED', 'var') && ~isempty(V2_FLEET_SEED)
    seed = V2_FLEET_SEED;
else
    seed = 20260829;
end
if exist('V2_FLEET_N', 'var') && ~isempty(V2_FLEET_N)
    N = V2_FLEET_N;
else
    N = 400;
end
outDir = fullfile(ROOT, 'results', 'ecms_phase25', 'fleet');
if ~exist(outDir, 'dir'), mkdir(outDir); end
jsonPath = fullfile(outDir, sprintf('voyages_seed%d.json', seed));

%% ======================= 生成 + 种子复现验证 =======================
t0 = tic;
V = ecms25.voyage_generator_v2(seed, N);
fprintf('[60] 生成 %d 个航次（%.1f s），种子 %d\n', numel(V), toc(t0), seed);

% ---- 种子复现：同种子第二次生成，逐位一致断言 ----
V2 = ecms25.voyage_generator_v2(seed, N);
assert(isequal(V, V2), 'ECMS:T302', ...
    '种子复现失败：同种子 %d 两次生成结果不一致（随机流或算法不确定）', seed);
fprintf('[60] 种子复现验证 PASS：seed=%d 两次生成逐位一致\n', seed);

%% ======================= 航次质量统计 =======================
durMinAll = [V.duration_min];
nSegAll = [V.n_seg];
pMaxAll = [V.P_max_est_kW];
kresAll = [V.k_res];
fprintf('[60] 时长 %.0f~%.0f min（均值 %.1f）| 段数 %d~%d | P_max_est %.0f~%.0f kW | k_res %.3f~%.3f\n', ...
    min(durMinAll), max(durMinAll), mean(durMinAll), min(nSegAll), max(nSegAll), ...
    min(pMaxAll), max(pMaxAll), min(kresAll), max(kresAll));
fprintf('[60] 平均重抽尝试 %.1f 次/航次（M0/时长/逆水计数拒绝后重抽）\n', mean([V.draw_attempts]));
allSeg = [V.seg];
typesAll = {allSeg.type};
for t = {'depart','harbor','eco','rush','bridge','upstream','berth'}
    fprintf('[60]   段型 %-9s: %3d 段\n', t{1}, sum(strcmp(typesAll, t{1})));
end
% 各段型速度/水流实际范围（M1/M2 生效证据）
for t = {'depart','harbor','eco','rush','bridge','upstream','berth'}
    m = strcmp(typesAll, t{1});
    if any(m)
        fprintf('[60]   段型 %-9s: v_kn %.2f~%.2f | current %.3f~%.3f m/s\n', t{1}, ...
            min([allSeg(m).v_kn]), max([allSeg(m).v_kn]), ...
            min([allSeg(m).current_mps]), max([allSeg(m).current_mps]));
    end
end
% M0 约束自检：非 upstream 段 P_calc 上限；upstream 段 V_rel 上限
pc = [allSeg(~strcmp(typesAll, 'upstream')).P_calc_kW];
maskUp = strcmp(typesAll, 'upstream');
if any(maskUp)
    vrUp = [allSeg(maskUp).v_mps] - [allSeg(maskUp).current_mps];
    fprintf('[60] M0 自检：非 upstream P_calc_max=%.1f kW（<=700）| upstream V_rel_max=%.3f m/s（<=7.06）\n', ...
        max(pc), max(vrUp));
else
    fprintf('[60] M0 自检：非 upstream P_calc_max=%.1f kW（<=700）| 本批无 upstream 段\n', max(pc));
end

%% ======================= JSON 写出（入库产物） =======================
meta = struct( ...
    'spec', 'nn-predictive-ecms-phase25 t301_dataset_v2_perturbation.md / FR-301', ...
    'simulation_generated', true, ...
    'seed', seed, 'N', numel(V), ...
    'kn2mps', 0.5144, ...
    'eta_chain', 0.97 * 0.95 * 0.60 * 0.97, ...
    'P_sat_kW', 700, 'V_rel_up_max', 7.06, 'P_limit_kW', 850, ...
    'Hull_K_R', Hull_K_R, 'Mot_P_nom', Mot_P_nom, ...
    'formula', 'P_calc(kW)=k_res*Hull_K_R*V_rel^3/eta_chain/1000; V_rel=v_mps-c_mps; M0: 非upstream段 P_calc>700 拒绝重抽, upstream段 V_rel>7.06 拒绝重抽(允许饱和); P_deliv_est=min(P_calc,Mot_P_nom)+100', ...
    'speed_ranges_kn', 'depart[2.5 3.5] harbor[4.5 5.5] eco[7 9] rush[10 11.5] bridge[4.5 5.5] upstream[10 11.5] berth[2.5 3.5]', ...
    'current_ranges_mps', 'depart[-0.3 0.3] harbor[-0.3 0.3] eco[-0.3 0.3] rush[-0.3 0.3] bridge[-0.3 0.3] upstream[-1.2 -0.6] berth[-0.3 0.3]', ...
    'k_res_range', [0.95 1.15], ...
    'duration_min_range', [60 120], ...
    'max_upstream_per_voyage', 1, ...
    'ramp_accel_mps2', 0.15, 'ramp_decel_mps2', 0.05, ...
    'hotel_kW', 100, ...
    'draw_order', 'nMid -> midTypes(upstream<=1) -> durations -> duration_bound -> speeds(v_kn->v_mps) -> currents -> k_res -> M0 precheck (reject=full redraw)', ...
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
fprintf('[60] 航次定义已写入 %s\n', jsonPath);

%% ======================= 回读自检（JSON 往返一致） =======================
back = jsondecode(fileread(jsonPath));
assert(numel(back.voyages) == N, 'ECMS:T302', 'JSON 回读航次数不一致');
for i = [1 round(N/2) N]
    assert(isequal(back.voyages(i).v_time_s(:).', V(i).v_time_s(:).'), ...
        'ECMS:T302', 'JSON 回读 v_time_s 不一致（voyage %d）', i);
    assert(isequal(back.voyages(i).v_value_mps(:).', V(i).v_value_mps(:).'), ...
        'ECMS:T302', 'JSON 回读 v_value_mps 不一致（voyage %d）', i);
    assert(abs(back.voyages(i).k_res - V(i).k_res) < 1e-12, ...
        'ECMS:T302', 'JSON 回读 k_res 不一致（voyage %d）', i);
end
fprintf('[60] JSON 回读自检 PASS（voyage 1/%d/%d 时间路点+k_res 逐位一致）\n', round(N/2), N);

fprintf('\n[60] 完成：%s\n', jsonPath);
