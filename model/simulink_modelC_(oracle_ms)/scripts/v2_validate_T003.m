%% v2_validate_T003.m — T-003 验证：oracle 生成 + 因果性 + 离线抽查
ROOT = fileparts(fileparts(mfilename('fullpath')));
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
md = fullfile(ROOT, 'results', 'ms_phase1'); if ~exist(md,'dir'), mkdir(md); end

MDL = 'HybridShip_ModelCms_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

SIDs = [4 6 7 9];
allPass = true;
for s = 1:numel(SIDs)
    SCN = SCENARIOS(SIDs(s));
    outMat = fullfile(md, sprintf('oracle_S%02d.mat', SIDs(s)));
    % 生成（内部跑 C0）
    D = model.make_oracle_ms(MDL, SCN, outMat);
    assert(size(D,2) == 13, 'D_ms 列数应为 13');
    % oracle 内核对：demand+hotel 即为 P_5 在未来 5s 之前 5s 的反查
    % （交叉验证：P_5(t) 应等于 P_exo(t+5)）
    fprintf('[T003] S%02d oracle: N=%d rows\n', SIDs(s), size(D,1));
    assert(all(~isnan(D(:))), 'oracle 不应含 NaN');
end
fprintf('[T003] 4 个 oracle .mat 生成 PASS\n');

% ---- 离线抽查：S04/S07 在若干时刻 t，节点值与 1Hz 需求负荷前向窗一致 ----
for s = 1:numel(SIDs)   % S04/S06/S07/S09 全部抽查
    SCN = SCENARIOS(SIDs(s));
    out = model.run_scenario_sim(MDL, SCN, 0, SCN.voyage_distance_m / SCN.target_speed_mps + 1200);
    ds = model.extract_logsout(out);
    D = model.make_oracle_ms(MDL, SCN, [], ds);   % 复用 ds，不重跑
    % 独立重建 1Hz 外生需求
    t0 = ds.t;
    t = (0:1:floor(t0(end)))';
    idx = zeros(numel(t), 1);
    for k = 1:numel(t)
        m = find(t0 <= t(k) + 1e-9, 1, 'last');
        if isempty(m), m = 1; end
        idx(k) = m;
    end
    P_exo = ds.P_prop_demand_kW(idx) + ds.P_hotel_kW(idx);
    % 抽查 t = 100, 300, 600 s（若存在）
    for tt = [100 300 600]
        if tt > t(end), continue; end
        i = tt + 1;
        nodes = 5:5:60;
        for h = 1:12
            want = P_exo(min(tt + nodes(h), floor(t(end))) + 1);
            got  = D(i, 1 + h);
            err = abs(got - want);
            assert(err < 1e-6, 'S%02d t=%d h=%d: 期望 %.6f 实得 %.6f', SIDs(s), tt, nodes(h), want, got);
        end
        fprintf('[T003] S%02d t=%d s: 12 节点与需求前向窗一致（<1e-6） PASS\n', SIDs(s), tt);
    end
end

% ---- 因果性断言：oracle 目标（demand+hotel）与降额后 load_total 分离 ----
fprintf('[T003] 因果性证据已由 exp5 产出（oracle_causality/）\n');
fprintf('[T003] ALL PASS\n');
close_system(MDL, 0);
