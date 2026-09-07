function row = member_exec(ctx, voyage_id, dag, rl, fam, k, meta_extra)
% fleet_v3.member_exec - one member voyage execution for the 73 Stage A
% pipeline (T-018 Phase 4-D): compile -> hotel guard -> whitelist
% injection -> Model E simulation -> 1 Hz truth stream -> forecast issue
% -> K/F snapshots -> activation-criteria eligibility -> artifact
% export.  Assembled on the SAME call chain as the C-side
% producer_prequal member face (T-020) but plan-driven: the qualification
% criteria (min_duration_s / min_canonical_windows) are injected from
% the frozen activation record (FR-540 discipline) and the producer
% identity is the fleet_v3 member face.
%
%   row = fleet_v3.member_exec(ctx, voyage_id, dag, rl, fam, k, meta_extra)
%
% meta_extra (empty struct for IID members; OOD members carry):
%   ood_final_domain           'OOD'  (triggers the export_ood rewrite)
%   ood_axis                   'A' | 'B' | 'C'
%   ood_condition_id           'A1'..'C3'
%   ood_token_id               canonical token or ''
%   heldout_token_registry_sha frozen registry self_hash
%   ood_judge_reason           fleet_v3.ood_judge reason text
%   propulsion                 fleet_v3.propulsion_feasibility_check res
%
% IID meta is byte-compatible with the C corpus conventions:
% dataset_domain='FORMAL', ood_axis='NONE' (OOD def version and physics
% domain reference carried as in the C producer).
%
% The caller owns slot retry (same-slot/same-config/same-science-seed
% policy, FR-537/R8) and the sampling ledger; this function executes one
% attempt and either returns a complete row or raises.
%
% Error ids: GATE:FLEETV3:MEMBER:<REASON> (plus the untouched chain
% error ids from the mission_v3/gate_e/model layers).
%
% Discipline: ASCII-only source; no absolute paths; no chained indexed
% assignment (multi-level writes via temp variables).

    if ~(isstruct(ctx) && isscalar(ctx) && isfield(ctx, 'plan'))
        error('GATE:FLEETV3:MEMBER:ARG', ...
            'ctx must be a fleet_v3.ctx_build output');
    end
    plan = ctx.plan;
    if ~(isstruct(meta_extra) && isscalar(meta_extra))
        error('GATE:FLEETV3:MEMBER:ARG', ...
            'meta_extra must be a scalar struct (possibly empty)');
    end
    is_ood = isfield(meta_extra, 'ood_final_domain') ...
        && strcmp(char(meta_extra.ood_final_domain), 'OOD');
    if ~ctx.model_ready
        error('GATE:FLEETV3:MEMBER:CTX', ...
            ['ctx.model_ready is false - run fleet_v3.ctx_build with ', ...
             'provisioning before member execution']);
    end

    mem = fam.members(k);
    arms_v = mem.arms;

    use_rl = rl;
    if strcmp(char(mem.traffic_override), 'OFF')
        use_rl.traffic = rl.traffic([]);
    end

    plan_cmp = mission_v3.plan_compose('compose', use_rl, ...
        'OODMode', is_ood);
    comp = mission_v3.scenario_compile('compile', plan_cmp, use_rl);
    scn = comp.scn;

    % T-011 caller contract: replay-drift machine guard
    p_base = dag.p_hotel_base_kW;
    d_hotel = dag.delta_p_hotel_unplanned;
    expect_hotel = double(p_base) + double(d_hotel);
    if abs(double(scn.hotel_kW) - expect_hotel) > 1e-9
        error('GATE:FLEETV3:MEMBER:HOTEL_GUARD', ...
            ['voyage %s: scn.hotel_kW %.17g != p_hotel_base %.17g + ', ...
             'delta_p_hotel_unplanned %.17g (T-011 caller contract)'], ...
            voyage_id, double(scn.hotel_kW), double(p_base), ...
            double(d_hotel));
    end

    mission_v3.scenario_compile('check_injection', comp.injection_list);
    gate_e.reset_bench_nominal();
    il = comp.injection_list;
    for j = 1:numel(il)
        e = il(j);
        assignin('base', e.var_name, e.value);
    end

    t_sim = tic;
    simout = model.run_scenario_sim(ctx.MDL, scn, 0, scn.stop_time_s);
    row_sim_wall = toc(t_sim);
    ds = gate_ctrl.get_ds(simout);

    sel = local_grid1hz(ds.t);
    t1 = ds.t(sel);
    n1 = numel(t1);
    if n1 < 1
        error('GATE:FLEETV3:MEMBER:EMPTY_STREAM', ...
            'voyage %s: simulation produced an empty time grid', ...
            voyage_id);
    end
    stream = struct();
    stream.time_s = t1(:);
    stream = local_fill_stream(ctx, ds, sel, n1, stream, voyage_id);
    stream.ctrl_mode = uint8(stream.ctrl_mode);
    stream.scenario_id = voyage_id;

    D = double(t1(end));
    anchors = (300:10:(D - 300))';

    tg_col = plan_cmp.t_grid_s(:);
    cur_col = scn.current_value_mps(:);
    ou = rl.ou;
    npre = double(round(ou.prehistory_s));
    t_pre = (-npre:-1)';
    t_src = [t_pre; tg_col];
    u_src = [zeros(npre, 1); cur_col];

    prof = char(arms_v.forecast);
    nA = numel(anchors);
    fb_all = [];
    for m = 1:nA
        a = anchors(m);
        sl = t_src <= a;
        fb = mission_v3.forecast_issue('issue', u_src(sl), t_src(sl), ...
            a, ctx.leads, prof, 'FormalSeed', char(dag.formal_seed), ...
            'BaseDrawIndex', double(dag.base_draw_index), ...
            'RegimeId', char(dag.regime_id), ...
            'DeltaWL', double(rl.water.delta_wl_m));
        if isempty(fb_all)
            fb_all = repmat(fb, nA, 1);
        end
        fb_all(m) = fb;
    end

    wtr = rl.water;
    kf = mission_v3.kf_snapshots('build_grid', plan_cmp, anchors, ...
        fb_all, 'HChartM', double(wtr.h_chart_m));

    row_ok = false(nA, 1);
    for m = 1:nA
        mm = kf.missing_mask(m, :);
        cs = kf.contract_status{m};
        row_ok(m) = ~any(mm(:)) && strcmp(cs, 'OK');
    end
    voy = struct('duration_s', D, 'kf_issue_time_s', ...
        kf.issue_time_s(:), 'kf_row_ok', row_ok);
    elig = mission_v3.eligibility('judge', voy, ...
        'MinDurationS', double(plan.qual.min_duration_s), ...
        'MinCanonicalWindows', double(plan.qual.min_canonical_windows));

    fb_last = fb_all(end);
    prov = fb_last.provenance;
    fprov = struct();
    fprov.forecast_generator_id = char(prov.generator_id);
    fprov.forecast_generator_version = char(prov.version);
    fprov.forecast_generator_hash = char(prov.hash);
    fprov.issue_time_s = double(prov.issue_time_s);
    fprov.source_cutoff_time_s = double(prov.source_cutoff_s);
    fprov.forecast_provenance_class = char(prov.provenance_class_formal);

    meta = struct();
    meta.voyage_id = voyage_id;
    meta.voyage_family_id = char(fam.family_id);
    meta.route_template_id = char(dag.template_id);
    meta.mission_template_id = char(dag.template_id);
    meta.split_group_id = char(fam.split_group_id);
    meta.scenario_stratum = char(dag.stratum_id);
    meta.generation_seed = double(scn.random_seed);
    meta.model_baseline_ref = ...
        'ModelE physical v1.1 + control v1.0 (P2 bench, law=0)';
    meta.producer_script_sha = ctx.producer_sha;
    meta.fixture_kind = 'NONE';
    meta.ood_axis = 'NONE';
    meta.ood_def_version = 'DGN-11-v1';
    meta.physics_domain_ref = 'ModelE validated domain (T-412 v1.1)';
    meta.h_chart_m = double(wtr.h_chart_m);
    meta.speed_limit_zones = plan_cmp.speed_limit_zones;
    meta.bridge_zones = plan_cmp.bridge_zones;
    meta.voyage_duration_s = D;
    meta.scenario_id = voyage_id;
    meta.forecast_provenance = fprov;
    meta.base_draw_id = char(fam.base_draw_id);
    meta.intervention = ['traffic=' char(arms_v.traffic) ...
        ';forecast=' prof];
    meta.forecast_quality_profile_id = prof;
    meta.stratum = char(dag.stratum_id);
    meta.dataset_domain = 'FORMAL';

    % ---- OOD meta extension (bridge via FORMAL through the untouched
    %         producer_export face, then rewrite; D5 provenance) --------
    if is_ood
        meta.ood_axis = char(meta_extra.ood_axis);
    end

    [art_dir, art_rel] = local_art_dirs(ctx, is_ood);
    out_file = fullfile(art_dir, [voyage_id '.mat']);
    exp_res = mission_v3.producer_export('export_artifact', stream, ...
        kf, meta, elig, out_file);
    if is_ood
        exp_res = fleet_v3.export_ood(out_file, meta_extra);
    end

    row = struct();
    row.voyage_id = voyage_id;
    row.base_draw_index = double(dag.base_draw_index);
    row.family_id = char(fam.family_id);
    row.split_group_id = char(fam.split_group_id);
    row.stratum_id = char(dag.stratum_id);
    row.template_id = char(dag.template_id);
    row.regime_id = char(dag.regime_id);
    row.member_id = char(mem.member_id);
    row.arms_traffic = char(arms_v.traffic);
    row.arms_forecast = prof;
    row.traffic_override = char(mem.traffic_override);
    row.disposition = char(elig.disposition);
    row.reason = char(elig.reason);
    row.detail = char(elig.detail);
    row.duration_s = D;
    row.n_stream = double(exp_res.n_stream_samples);
    row.n_kf_rows = double(exp_res.n_kf_rows);
    row.sim_wall_s = row_sim_wall;
    row.artifact_rel = art_rel;
    row.artifact_sha256 = local_sha256_file(out_file);
    row.payload_hash = local_payload_hash(stream, kf, elig, meta_extra);
    if is_ood
        row.ood_axis = char(meta_extra.ood_axis);
        row.ood_condition_id = char(meta_extra.ood_condition_id);
        row.ood_token_id = char(meta_extra.ood_token_id);
    else
        row.ood_axis = 'NONE';
        row.ood_condition_id = '';
        row.ood_token_id = '';
    end
end

%% ========================== local helpers ===========================

function [d, rel] = local_art_dirs(ctx, is_ood)
% [datasets_v3 <seg> <sub>] artifact layout:
%   stage_a: datasets_v3/qualification/stage_a_iid | stage_a_ood
%   smoke:   datasets_v3/qualification_smoke/iid       | ood
    seg = 'qualification';
    sub = 'stage_a_iid';
    if strcmp(char(ctx.plan.mode), 'smoke')
        seg = 'qualification_smoke';
        sub = 'iid';
    end
    if is_ood
        if strcmp(char(ctx.plan.mode), 'smoke')
            sub = 'ood';
        else
            sub = 'stage_a_ood';
        end
    end
    d = fullfile(ctx.ROOT, 'datasets_v3', seg, sub);
    if ~exist(d, 'dir')
        mkdir(d);
    end
    rel = ['datasets_v3/' seg '/' sub '/' ];
end

function sel = local_grid1hz(t)
    tt = double(t(:)) * 100;
    tt_r = round(tt);
    sel = (abs(tt - tt_r) <= 1e-6) & (mod(tt_r, 100) == 0);
end

function stream = local_fill_stream(ctx, ds, sel, n1, stream, voyage_id)
    sm = ctx.stream_map;
    fnames = fieldnames(sm);
    for j = 1:numel(fnames)
        f = fnames{j};
        m = sm.(f);
        src = char(m.source);
        if strncmp(src, 'logsout:', 8)
            nm = matlab.lang.makeValidName(src(9:end));
            if ~isfield(ds, nm)
                error('GATE:FLEETV3:MEMBER:STREAM', ...
                    'voyage %s: logsout lacks %s (registry %s)', ...
                    voyage_id, nm, src);
            end
            col = ds.(nm);
            stream.(f) = col(sel);
        elseif strncmp(src, 'scenario:', 9)
            v = evalin('base', src(10:end));
            stream.(f) = repmat(double(v), n1, 1);
        else
            error('GATE:FLEETV3:MEMBER:STREAM', ...
                'voyage %s: unsupported source %s', voyage_id, src);
        end
    end
end

function lines = local_ser(name, val, lines)
    if isstruct(val)
        fn = fieldnames(val);
        lines{end+1} = sprintf('%s struct n=%d fields=[%s]', name, ...
            numel(val), strjoin(fn.', ',')); %#ok<AGROW>
        for i = 1:numel(val)
            el = val(i);
            for j = 1:numel(fn)
                lines = local_ser([name '.' f2c(fn{j})], el.(fn{j}), ...
                    lines);
            end
        end
    elseif iscell(val)
        lines{end+1} = sprintf('%s cell n=%d', name, numel(val)); %#ok<AGROW>
        for i = 1:numel(val)
            lines = local_ser(sprintf('%s{%d}', name, i), val{i}, ...
                lines);
        end
    elseif islogical(val) || isnumeric(val)
        v = double(val(:)');
        lines{end+1} = sprintf('%s num %dx%d', name, size(val, 1), ...
            size(val, 2)); %#ok<AGROW>
        lines{end+1} = sprintf('%.17g ', v); %#ok<AGROW>
    elseif ischar(val)
        lines{end+1} = sprintf('%s char %s', name, val); %#ok<AGROW>
    else
        error('GATE:FLEETV3:MEMBER:SER', ...
            'unsupported payload class at %s', name);
    end
end

function c = f2c(s)
    c = char(s);
end

function h = local_payload_hash(stream, kf, elig, meta_extra)
    lines = {'FLEET_V3_STAGE_A_PAYLOAD_V1'};
    lines = local_ser('stream', stream, lines);
    lines = local_ser('kf', kf, lines);
    lines{end+1} = ['elig.disposition=' char(elig.disposition)]; %#ok<AGROW>
    lines{end+1} = ['elig.reason=' char(elig.reason)]; %#ok<AGROW>
    lines{end+1} = ['elig.token_id=' char(elig.token_id)]; %#ok<AGROW>
    if isfield(meta_extra, 'ood_axis')
        lines{end+1} = ['ood.axis=' char(meta_extra.ood_axis)]; %#ok<AGROW>
        lines{end+1} = ['ood.condition=' ...
            char(meta_extra.ood_condition_id)]; %#ok<AGROW>
    end
    bytes = unicode2native(strjoin(lines, newline), 'UTF-8');
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(bytes(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function h = local_sha256_file(path)
    fid = fopen(path, 'r');
    if fid < 0
        error('GATE:FLEETV3:MEMBER:IO', ...
            'cannot open file for hashing: %s', path);
    end
    raw = fread(fid, inf, 'uint8=>uint8');
    fclose(fid);
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(raw(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end
