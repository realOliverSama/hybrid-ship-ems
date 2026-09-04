function res = export_pilot(ROOT)
% dataset_v3.export_pilot — Phase 4-D T-007：Dataset v3 pilot 导出主体（70 号薄入口调用）
%   按 registry（机器契约，FR-465）从 Model E logsout 导出 voyage-level native-rate
%   stream（truth+H1/H2+ORACLE+METADATA+FORBIDDEN 审计存档，realization-time 单轴）
%   + K/F issue-time snapshots（DGN-13 causal fixtures，全部 PILOT_ONLY，FR-500）。
%   前置：ROOT 下 config 三件已由薄入口载入 base（SCENARIOS/MANUAL_CMD_M*_TS 等）。
%   输出：datasets_v3/pilot/<voyage_id>.mat（-v7：stream/kf/meta 三 struct）
%         results/dataset_v3/pilot/pilot_export_manifest.json
%   纪律：Model E 零改动；禁绝对路径；failed voyage 记录后继续（不静默）；
%         任何 FAILED ⇒ res.failed>0，由薄入口 error('GATE:*')。
%
%   布局冻结（与 dataset_v3/windowing.build_windows 接口一致，issue-major）：
%   kf.issue_time_s (M×1)、kf.lead_s (M×30)、各 K/F 列 (M×30)（行=issue，列=lead）。

    REG_PATH = fullfile(ROOT, 'config', 'dataset_v3_signal_registry.json');
    reg = local_load_registry(REG_PATH);
    PRODUCER_SHA = model.sha256_file(fullfile(ROOT, 'scripts', '70_export_dataset_v3_pilot.m'));

    VOY = local_pilot_voyages();

    out_dir = fullfile(ROOT, 'datasets_v3', 'pilot');
    res_dir = fullfile(ROOT, 'results', 'dataset_v3', 'pilot');
    if ~exist(out_dir, 'dir'), mkdir(out_dir); end
    if ~exist(res_dir, 'dir'), mkdir(res_dir); end

    MDL = 'HybridShip_ModelE';
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));

    entries = {};
    for iv = 1:numel(VOY)
        V = VOY(iv);
        fprintf('[70] export %s (%s) ...\n', V.voyage_id, V.scenario_stratum);
        try
            gate_e.reset_bench_nominal();   % T-006 同款 case 隔离不变量
            SCN = local_scenario(V);
            entry = local_export_voyage(MDL, SCN, V, reg, out_dir, PRODUCER_SHA);
            entries{end+1} = entry; %#ok<AGROW>
            fprintf('[70]   OK dur=%.0f s anchors=%d cols=%d\n', ...
                entry.voyage_duration_s, entry.n_anchors, entry.n_stream_cols);
        catch e
            % failed voyage：记录原因后继续（禁静默补数，design Error Handling）
            entries{end+1} = struct('voyage_id', V.voyage_id, 'status', 'FAILED', ...
                'reason', e.message); %#ok<AGROW>
            fprintf('[70]   FAILED: %s\n', e.message);
            disp(getReport(e, 'extended', 'hyperlinks', 'off'));
        end
    end

    manifest = struct();
    manifest.schema = 'dataset_v3_pilot_export_manifest_v1';
    manifest.spec = 'phase4-d-dataset-v3 / T-007';
    manifest.stage = 'PILOT';
    manifest.generated_at = char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ'));
    manifest.producer_script_sha256 = PRODUCER_SHA;
    manifest.registry_sha256 = local_registry_self_hash(REG_PATH);
    manifest.n_voyages = numel(entries);
    statuses = cellfun(@(e) string(e.status), entries);
    manifest.n_failed = sum(statuses == "FAILED");
    manifest.voyages = entries;
    fid = fopen(fullfile(res_dir, 'pilot_export_manifest.json'), 'w');
    fwrite(fid, jsonencode(manifest, 'PrettyPrint', true)); fclose(fid);
    close_system(MDL, 0);
    fprintf('[70] manifest: %s（%d voyages，failed=%d）\n', ...
        fullfile(res_dir, 'pilot_export_manifest.json'), numel(entries), manifest.n_failed);

    res = struct('n_voyages', numel(entries), 'n_failed', manifest.n_failed, ...
        'manifest_path', fullfile(res_dir, 'pilot_export_manifest.json'));
end

%% ================= 局部函数 =================
function v = local_plan_value(pv_t, pv_v, x)
% 计划剖面查表：≥2 节点线性插值（外推=端点延拓）；单节点=常量
    if numel(pv_t) >= 2
        v = interp1(pv_t, pv_v, x, 'linear', 'extrap');
    else
        v = pv_v(1);
    end
end

function SCN = local_scenario(V)
    SCENARIOS = evalin('base', 'SCENARIOS');
    SCN = SCENARIOS(V.scn_index);
    SCN.voyage_distance_m = V.voyage_distance_m;
    SCN.initial_SOC = V.initial_SOC;
end

function entry = local_export_voyage(MDL, SCN, V, reg, out_dir, producer_sha)
% 单航次：跑 sim → 提取 stream/kf → 写 .mat + 返回 manifest 条目
    if V.use_manual
        assignin('base', 'VOYAGE_CTRL_MODE', 1);
        assignin('base', 'VOYAGE_MANUAL_CMD_TS', V.manual_ts);
    end
    out = model.run_scenario_sim(MDL, SCN, 0, V.stop_time_s);
    ds = model.extract_logsout(out);
    t = ds.t(:);
    t_end = t(end);

    % ---- stream 列（registry 驱动：logsout:* 唯一源 + scenario 常量）----
    stream = struct('time_s', t);
    n_cols = 0;
    src_names = local_unique_logsout_sources(reg);
    for i = 1:numel(src_names)
        nm = src_names{i};
        assert(isfield(ds, nm), 'logsout 缺列 %s（voyage %s）', nm, V.voyage_id);
        stream.(nm) = ds.(nm)(:);
        n_cols = n_cols + 1;
    end
    % scenario 常量列（water_depth_m：未打点，Model E 零改动下由场景元数据导出）
    stream.water_depth_m = repmat(double(ecms2.scn_field(SCN, 'water_depth_m', 20.0)), numel(t), 1);
    n_cols = n_cols + 1;

    % ---- anchor 网格 + K/F causal fixtures（DGN-13；issue-major 布局）----
    anchors = (300:10:(t_end - 300))';
    nA = numel(anchors);
    leads = (10:10:300)';
    nL = numel(leads);
    kf = struct('issue_time_s', anchors, 'lead_s', repmat(leads', nA, 1));
    kf.v_plan_mps = zeros(nA, nL); kf.s_plan_m = zeros(nA, nL);
    kf.h_chart_m = zeros(nA, nL);  kf.P_hotel_plan_kW = zeros(nA, nL);
    kf.u_fcst_mps = zeros(nA, nL); kf.wl_fcst_m = zeros(nA, nL);
    % 计划剖面查表（distance 模式：v_knots_x/v_knots_v，滤 -1 哨兵；time 模式：v_time_s 表）
    isTime = strcmp(ecms2.scn_field(SCN, 'speed_profile_mode', 'distance'), 'time');
    if isTime
        pv_t = SCN.v_time_s(:); pv_v = SCN.v_value_mps(:);
    else
        vx = SCN.v_knots_x(:); vv = SCN.v_knots_v(:);
        keep = vx >= 0;
        pv_t = vx(keep); pv_v = vv(keep);
    end
    h_chart = ecms2.scn_field(SCN, 'water_depth_m', 20.0);
    hotel_plan = ecms2.scn_field(SCN, 'hotel_kW', 100.0);
    for ia = 1:nA
        a = anchors(ia);
        i0 = find(t <= a + 1e-9, 1, 'last');           % t₀ 及以前（因果）
        % persistence forecast（仅消费 t₀ 及以前信息：F_{t0}=g(I_{<=t0})）
        kf.u_fcst_mps(ia, :) = repmat(ds.v_current_mps(max(i0, 1)), 1, nL);
        kf.wl_fcst_m(ia, :) = repmat(h_chart, 1, nL);
        % planned along-track SOG/位置：x_actual(t₀) + 计划表前向积分（1 s 子步）；
        % 禁复用 realized v_ref（design R5：计划表=先验计划，因果合法）。
        % 单节点计划表=恒计划 SOG（interp1 需 ≥2 网格点）。
        x_plan = ds.x_ship_m(max(i0, 1));
        tau = a;
        for k = 1:nL
            for sub = 1:10
                if isTime
                    tau = tau + 1;
                    v_p = local_plan_value(pv_t, pv_v, tau);
                else
                    v_p = local_plan_value(pv_t, pv_v, x_plan);
                    x_plan = x_plan + v_p * 1.0;
                end
            end
            % bin 右端点计划值（interval_endpoint 语义，R3-m3）：距离模式在子步推进后
            % 于最终计划位置处再取值一次（不前进）；时间模式循环末 tau 恰为右端点
            if ~isTime
                v_p = local_plan_value(pv_t, pv_v, x_plan);
            end
            % issue-major (ia, k)
            kf.v_plan_mps(ia, k) = v_p;
            kf.s_plan_m(ia, k) = x_plan;
            kf.h_chart_m(ia, k) = h_chart;
            kf.P_hotel_plan_kW(ia, k) = hotel_plan;
        end
    end

    % ---- forecast provenance 六字段（design R6；persistence: cutoff==issue）----
    prov = struct();
    prov.forecast_generator_id = 'pilot_persistence_fixture';
    prov.forecast_generator_version = '1.0.0';
    prov.forecast_generator_hash = producer_sha;
    % <600 s 航次合法产生零窗口/零快照（FR-472）；issue/cutoff 记 0（无 vintage）
    prov.issue_time_s = 0.0;
    prov.source_cutoff_time_s = 0.0;
    if nA > 0
        prov.issue_time_s = anchors(1);
        prov.source_cutoff_time_s = anchors(1);   % 每行 cutoff==该行 issue（persistence at issue）；此处记录首锚点
    end
    prov.forecast_provenance_class = 'FIXTURE_CAUSAL_STUB';

    % ---- meta ----
    meta = struct();
    meta.voyage_id = V.voyage_id;
    meta.scenario_id = sprintf('%d_%s', SCN.id, SCN.name);
    meta.scenario_stratum = V.scenario_stratum;
    meta.voyage_family_id = V.voyage_id;      % pilot：family/group/template 填 voyage 自身（D-D7 预留）
    meta.route_template_id = SCN.name;
    meta.mission_template_id = SCN.name;
    meta.split_group_id = V.voyage_id;
    meta.generation_seed = SCN.random_seed;
    meta.model_baseline_ref = 'ModelE physical v1.1 + control v1.0（P2 bench，law=0）';
    meta.producer_script_sha256 = producer_sha;
    meta.fixture_kind = 'PILOT_ONLY';         % FR-500：pilot fixture 永不入 formal
    meta.bridge_zones = [];                   % 静态空集如实记录（DGN-13）
    meta.speed_limit_zones = [];
    meta.ood_axis = 'NONE';
    meta.ood_def_version = 'DGN-11-v1';
    meta.physics_domain_ref = 'ModelE validated domain (T-412 v1.1)';
    meta.forecast_provenance = prov;
    meta.voyage_duration_s = t_end;
    meta.n_anchors = nA;

    fmat = fullfile(out_dir, [V.voyage_id '.mat']);
    save(fmat, 'stream', 'kf', 'meta', '-v7');

    entry = struct('voyage_id', V.voyage_id, 'status', 'OK', ...
        'file', ['datasets_v3/pilot/' V.voyage_id '.mat'], ...
        'scenario_stratum', V.scenario_stratum, 'fixture_kind', 'PILOT_ONLY', ...
        'voyage_duration_s', t_end, 'n_anchors', nA, 'n_stream_cols', n_cols, ...
        'forecast_provenance_class', 'FIXTURE_CAUSAL_STUB');
end

function names = local_unique_logsout_sources(reg)
% registry 驱动的 stream logsout 唯一源名清单（保持 registry 首现顺序）
    names = {};
    sigs = reg.signals;
    for i = 1:numel(sigs)
        if isstruct(sigs)
            src = sigs(i).source_signal;
        else
            src = sigs{i}.source_signal;
        end
        if strncmp(src, 'logsout:', 8)
            nm = src(9:end);
            if ~any(strcmp(names, nm)), names{end+1} = nm; end %#ok<AGROW>
        end
    end
end

function reg = local_load_registry(path)
    reg = jsondecode(fileread(path));
    if ~isfield(reg, 'signals')
        error('GATE:70:registry_invalid — registry 缺 signals');
    end
end

function h = local_registry_self_hash(path)
% registry self_hash 为 Python validator 权威维护（canonical json）；此处透传登记值
    reg = jsondecode(fileread(path));
    if isfield(reg, 'self_hash') && isfield(reg.self_hash, 'value')
        h = reg.self_hash.value;
    else
        h = 'UNKNOWN';
    end
end

function voy = local_pilot_voyages()
% pilot 航次表：S01~S10（默认距离）+ S20 + S31~S36 + M1~M3；strata=scenario family
    voy = struct('voyage_id', {}, 'scn_index', {}, 'voyage_distance_m', {}, ...
        'initial_SOC', {}, 'stop_time_s', {}, 'use_manual', {}, 'manual_ts', {}, ...
        'scenario_stratum', {});
    add = @(id, idx, dist, soc, stop, man, ts, strat) struct('voyage_id', id, ...
        'scn_index', idx, 'voyage_distance_m', dist, 'initial_SOC', soc, ...
        'stop_time_s', stop, 'use_manual', man, 'manual_ts', ts, 'scenario_stratum', strat);
    noTs = [];
    voy(end+1) = add('S01_still_water', 1, 5000, 0.60, 4000, false, noTs, 'stillwater');
    voy(end+1) = add('S02_downstream', 2, 5000, 0.60, 4000, false, noTs, 'current');
    voy(end+1) = add('S03_upstream', 3, 5000, 0.60, 4000, false, noTs, 'current');
    voy(end+1) = add('S04_resistance_inc', 4, 5000, 0.60, 4000, false, noTs, 'resistance');
    voy(end+1) = add('S05_resistance_dec', 5, 5000, 0.60, 4000, false, noTs, 'resistance');
    voy(end+1) = add('S06_resistance_step', 6, 5000, 0.60, 4000, false, noTs, 'resistance');
    voy(end+1) = add('S07_bridge_maneuver', 7, 5000, 0.60, 4000, false, noTs, 'bridge');
    voy(end+1) = add('S08_resistance_bands', 8, 5000, 0.60, 4000, false, noTs, 'resistance');
    % S09_emergency_sprint 移出 pilot 集（偏差 D-13）：其 v_ref 与 S01 同为常数表、
    % accel_ramp 不约束常数剖面 → 实现轨迹与 S01 逐位相同（L1 exact-duplicate 实证
    % 检出，窗口内容 hash 全等）——应急坡道语义属 C 包 Mission v3 真实坡道剖面
    voy(end+1) = add('S10_current_reversal', 10, 5000, 0.60, 4000, false, noTs, 'current');
    voy(end+1) = add('S20_composite', 20, 40000, 0.60, 6000, false, noTs, 'composite');
    for k = 31:36
        voy(end+1) = add(sprintf('S%02d_shallow', k), k, 5000, 0.60, 4000, false, noTs, 'shallow');
    end
    % M 工况（MANUAL；run_scenario_sim 距离上限保证到达设计段长前不停航）。
    % 剖面表消费 config 单一事实源（R3-m6：禁本地复制漂移）
    m1 = evalin('base', 'MANUAL_CMD_M1_TS');
    m2 = evalin('base', 'MANUAL_CMD_M2_TS');
    m3 = evalin('base', 'MANUAL_CMD_M3_TS');
    voy(end+1) = add('M1_step', 1, 4000, 0.60, 360, true, m1, 'manual');
    voy(end+1) = add('M2_ramp', 1, 4000, 0.60, 360, true, m2, 'manual');
    voy(end+1) = add('M3_staged', 1, 7500, 0.60, 960, true, m3, 'manual');
end
