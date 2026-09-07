function out = producer_prequal(mode, varargin)
% mission_v3.producer_prequal - T-020 MISSION_V3_PREQUALIFICATION producer
% (execution of the T-019 frozen pre-registration plan
% config/mission_v3/prequal_plan.json, self_hash e3fd066c..., status
% FROZEN_PRE_REGISTRATION).
%
%   ctx = mission_v3.producer_prequal('ctx')
%   out = mission_v3.producer_prequal('run' [, name-values])
%   row = mission_v3.producer_prequal('member', ctx, si, bdi, dag, rl, ...
%                                     fam, k [, pre])
%   out = mission_v3.producer_prequal('compare', ledger1, ledger2, ...
%           audit2Path, manifest2Path, outManifestPath, outAuditPath)
%
% MODES
%   'ctx'     provision the environment (config scripts into base, load
%             the Model E system, frozen schema/registry maps, lead grid,
%             producer self hash) and return the shared context struct.
%   'run'     one full prequalification pass over the frozen plan: 9
%             strata x 3 base draws x 2x2 family = 108 member voyages
%             (nominal).  Per slot: env_realize('sample') ->
%             family_expand('expand') -> per member: plan_compose ->
%             scenario_compile -> hotel guard
%             (GATE:MISSION_V3:PREQUAL:HOTEL_GUARD) -> injection
%             whitelist re-check + assignin -> gate_e.reset_bench_nominal
%             -> model.run_scenario_sim -> gate_ctrl.get_ds (1 Hz grid) ->
%             forecast_issue (arm profile; anchors 300:10:(t_end-300)) ->
%             kf_snapshots('build_grid') -> eligibility('judge') ->
%             producer_export('export_artifact').  Traffic-OFF members
%             compose a realization COPY with traffic events stripped
%             (the physical counterfactual world has no events; the truth
%             hash stays the as-drawn payload per the T-014 ruling).
%             Name-values:
%               'RunLabel'         'prequal' (default) | 'prequal_rerun'
%                                  | 'prequal_pilot'; names the dataset
%                                  dir datasets_v3/mission_v3/<label>/
%               'StratumFilter'    cellstr subset of plan strata (pilot)
%               'DrawsPerStratum'  whole number, default plan value 3
%               'AuditJsonPath'    default results/mission_v3/
%                                  prequal_strata_audit.json
%               'ManifestJsonPath' default results/mission_v3/
%                                  prequal_manifest.json
%               'Resume'           true (default): reuse complete ledger
%                                  rows whose artifact already exists
%                                  (crash resume; never a plan amendment)
%               'VerdictError'     true (default): raise
%                                  GATE:MISSION_V3:PREQUAL:REJECTION when
%                                  a frozen rejection threshold is
%                                  exceeded (audit/manifest are written
%                                  BEFORE the error, controlled FAIL)
%               'ProvisionBase'    true (default)
%   'member'  single member execution (unit-testable).  Optional pre
%             struct: field SimOnly=true stops right before the
%             simulation (returns the hotel guard fact only).
%   'compare' two-run determinism comparison (AC-438) of two run ledgers,
%             final audit/manifest rewrite with the consistency section,
%             then the controlled rejection error when the second-run
%             verdict is FAIL.
%
% FROZEN VALUES (never re-decided here, all read from the plan JSON):
%   formal seed, strata list, draw-range rule (stratum i 0-based uses
%   [i*1000,(i+1)*1000)), max sampling attempts per slot, FAILED-slot
%   retry max 2 (same slot / same config / same scientific seed),
%   rejection thresholds 0.50 per stratum and overall.
%
% R3/R4 RED LINE: no T-429 result metric (SBD / BA / confidence bound /
% confusion matrix or any other outcome metric) is ever computed or
% emitted; a field-name blacklist self-check runs before every JSON
% write (GATE:MISSION_V3:PREQUAL:T429).
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% ROOT resolved from mfilename with pwd fallback; thin dispatch + local
% functions; Model E is never modified (runtime injection only through
% the scenario_compile whitelist).

    if ~(ischar(mode) || isstring(mode))
        error('GATE:MISSION_V3:PREQUAL:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'ctx'
            out = local_make_ctx();
        case 'run'
            out = local_run(varargin);
        case 'member'
            out = local_member_wrapper(varargin);
        case 'compare'
            out = local_compare(varargin);
        otherwise
            error('GATE:MISSION_V3:PREQUAL:ARG', ...
                'unknown mode: %s', char(mode));
    end
end

%% ======================================================================
%  context
%% ======================================================================
function ctx = local_make_ctx()
    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    ctx = struct();
    ctx.ROOT = ROOT;
    ctx.MDL = 'HybridShip_ModelE';
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    plan = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'prequal_plan.json');
    ctx.plan = plan;

    sp = plan.seed_plan;
    ctx.formal_seed = char(sp.formal_generation_seed);
    smp = plan.sampling_policy;
    ctx.max_attempts = double(smp.max_attempts_per_slot);
    rp = plan.retry_policy;
    ctx.retry_max = double(rp.slot_failed_retry_max);
    rth = plan.rejection_threshold_frozen;
    ctx.thr_per_stratum = double(rth.per_stratum_max);
    ctx.thr_overall = double(rth.overall_max);
    sc = plan.scale;
    ctx.strata = local_cellstr_of(sc.strata);
    fp = plan.family_plan;
    ctx.draws_per_stratum = double(fp.base_draws_per_stratum);
    ctx.members_per_family = double(fp.members_per_family);

    w0 = mission_v3.eligibility('windows', 0);
    ctx.leads = (1:double(w0.n_lead_bins)) * double(w0.lead_bin_s);

    ctx.schema = local_read_json(fullfile(ROOT, 'config'), ...
        'dataset_v3_producer_schema.json');
    ctx.sigreg = local_read_json(fullfile(ROOT, 'config'), ...
        'dataset_v3_signal_registry.json');
    ctx.stream_map = local_stream_map(ctx);

    ctx.producer_sha = local_sha256_file([mfilename('fullpath'), '.m']);
    ctx.model_ready = false;
    ctx.run_label = 'prequal';
    ctx.artifact_dir = fullfile(ctx.ROOT, 'datasets_v3', 'mission_v3', ...
        'prequal');

    amend_path = fullfile(ROOT, 'results', 'mission_v3', ...
        'prequal_amendment_01.json');
    ctx.has_amendment = exist(amend_path, 'file') == 2;
    if ctx.has_amendment
        am = local_read_json_nodir(amend_path);
        am_sh = am.self_hash;
        ctx.amendment = struct('file', ...
            'results/mission_v3/prequal_amendment_01.json', ...
            'amendment_id', char(am.amendment_id), ...
            'self_hash', char(am_sh.value));
    end
end

function local_provision(ctx)
    ROOT = ctx.ROOT;
    cfg_dir = fullfile(ROOT, 'config');
    evalin('base', sprintf("run('%s')", ...
        fullfile(cfg_dir, 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", ...
        fullfile(cfg_dir, 'ModelE_Control_Parameters.m')));
    evalin('base', sprintf("run('%s')", ...
        fullfile(cfg_dir, 'ModelE_TestScenarios.m')));
    load_system(fullfile(ROOT, 'model', [ctx.MDL '.slx']));
    ctx.model_ready = true;
end

%% ======================================================================
%  one full pass
%% ======================================================================
function out = local_run(arg_cell)
    ov = local_parse_named(arg_cell);
    ctx = local_make_ctx();
    if logical(local_get(ov, 'ProvisionBase', true))
        local_provision(ctx);
    end

    label = char(local_get(ov, 'RunLabel', 'prequal'));
    ctx.run_label = label;
    run_dir = fullfile(ctx.ROOT, 'datasets_v3', 'mission_v3', label);
    ctx.artifact_dir = run_dir;
    if ~exist(run_dir, 'dir'), mkdir(run_dir); end
    audit_path = char(local_get(ov, 'AuditJsonPath', fullfile(ctx.ROOT, ...
        'results', 'mission_v3', 'prequal_strata_audit.json')));
    man_path = char(local_get(ov, 'ManifestJsonPath', ...
        fullfile(ctx.ROOT, 'results', 'mission_v3', ...
        'prequal_manifest.json')));
    ledger_path = fullfile(run_dir, '_run_ledger.json');

    strata = ctx.strata;
    filt = local_get(ov, 'StratumFilter', {});
    if ~isempty(filt)
        strata = local_cellstr_of(filt);
    end
    n_draws = ctx.draws_per_stratum;
    if ~isempty(local_get(ov, 'DrawsPerStratum', []))
        n_draws = double(local_get(ov, 'DrawsPerStratum', n_draws));
    end
    resume = logical(local_get(ov, 'Resume', true));
    verdict_error = logical(local_get(ov, 'VerdictError', true));

    t_run = tic;
    frozen = local_verify_frozen(ctx);

    prev = struct('voyages', {{}}, 'sampling', {{}}, 'retry', {{}});
    if resume && exist(ledger_path, 'file')
        prev = local_prev_ledger(ledger_path);
    end
    voy_c = prev.voyages;
    samp_c = prev.sampling;
    retry_c = prev.retry;

    % ---- PREQUAL-AMEND-01 NORMAL reuse evidence: re-execute the first
    % NORMAL member under the amended files and require bitwise payload
    % hash equality with the recorded artifact (hard gate on mismatch).
    reuse_ev = struct('verified', false, 'voyage_id', 'PREQ_S1_D0000_M1', ...
        'payload_hash_recorded', '', 'payload_hash_recomputed', '', ...
        'basis_note', ['PREQUAL-AMEND-01 section A: the global ', ...
        'chart_depth_voyage_base TruncNormal(6.0, 1.0, lower 3.5) ', ...
        'equals the pre-amendment NORMAL per-regime values, so the ', ...
        'three NORMAL strata draw streams are bitwise unchanged; the ', ...
        'existing NORMAL artifacts are reused as verified by this ', ...
        'payload hash comparison']);
    if resume && ctx.has_amendment && ~isempty(strata) ...
            && strcmp(char(strata{1}), char(ctx.strata{1}))
        old_row = '';
        for j = 1:numel(voy_c)
            r0 = voy_c{j};
            if strcmp(char(r0.voyage_id), char(reuse_ev.voyage_id)) ...
                    && ~isempty(char(r0.payload_hash))
                old_row = r0;
                break;
            end
        end
        if ~isempty(old_row)
            parts1 = strsplit(char(ctx.strata{1}), '__');
            tpl1 = strrep(char(parts1{1}), 'STRAT_', '');
            reg1 = char(parts1{2});
            samp1 = mission_v3.env_realize('sample', ctx.formal_seed, 0, ...
                tpl1, reg1, char(ctx.strata{1}));
            led1 = samp1.ledger;
            acc1 = led1(samp1.n_attempts);
            dag1 = acc1.draw;
            rl1 = samp1.realization;
            fam1 = mission_v3.family_expand('expand', dag1, ...
                'Realization', rl1);
            saved_dir = ctx.artifact_dir;
            dbg_dir = fullfile(run_dir, '_reuse_verify');
            if ~exist(dbg_dir, 'dir'), mkdir(dbg_dir); end
            ctx.artifact_dir = dbg_dir;
            row_new = mission_v3.producer_prequal('member', ctx, 1, ...
                dag1.base_draw_index, dag1, rl1, fam1, 1);
            ctx.artifact_dir = saved_dir;
            reuse_ev.payload_hash_recorded = char(old_row.payload_hash);
            reuse_ev.payload_hash_recomputed = char(row_new.payload_hash);
            reuse_ev.verified = strcmp(char(row_new.payload_hash), ...
                char(old_row.payload_hash));
            if ~reuse_ev.verified
                error('GATE:MISSION_V3:PREQUAL:NORMAL_REUSE', ...
                    ['NORMAL reuse basis violated: recomputed payload ', ...
                     'hash %s differs from the recorded %s'], ...
                    char(row_new.payload_hash), ...
                    char(old_row.payload_hash));
            end
        end
    end

    stratum_audit = struct([]);
    regime_acc = struct('regime', {{}}, 'pool', {}, 'peaks', {});

    for si_loop = 1:numel(strata)
        st = char(strata{si_loop});
        parts = strsplit(st, '__');
        if numel(parts) ~= 2
            error('GATE:MISSION_V3:PREQUAL:STRATUM', ...
                'stratum id not of the form <TPL>__<REG>: %s', st);
        end
        tpl = strrep(char(parts{1}), 'STRAT_', '');
        reg = char(parts{2});
        si_plan = local_stratum_index(ctx, st);
        draw_start = (si_plan - 1) * 1000;

        slot_fails = 0;
        attempts_st = 0;
        fam_ids = {};
        sg_ids = {};

        for slot = 1:n_draws
            samp = local_sample_logged(ctx, st, tpl, reg, draw_start, ...
                slot);
            attempts_st = attempts_st + samp.n_attempts;
            for jj = 1:numel(samp.new_rows)
                samp_c{end+1} = samp.new_rows{jj}; %#ok<AGROW>
            end
            if ~samp.accepted
                slot_fails = slot_fails + 1;
                draw_start = draw_start + ctx.max_attempts;
                local_flush(ledger_path, voy_c, samp_c, retry_c);
                continue;
            end
            s2 = samp.sample;
            led = s2.ledger;
            acc_row = led(s2.n_attempts);
            dag = acc_row.draw;
            rl = s2.realization;
            bdi = dag.base_draw_index;
            draw_start = bdi + 1;

            ur_acc = rl.u_river_mps;
            regime_acc = local_regime_acc(regime_acc, reg, ur_acc);

            fam = mission_v3.family_expand('expand', dag, ...
                'Realization', rl);
            fam_ids{end+1} = char(fam.family_id); %#ok<AGROW>
            sg_ids{end+1} = char(fam.split_group_id); %#ok<AGROW>

            for k = 1:double(fam.n_members)
                [row, voy_c, retry_c] = local_member_with_retry(ctx, ...
                    si_plan, st, tpl, reg, bdi, dag, rl, fam, k, ...
                    voy_c, retry_c, resume);
                voy_c{end+1} = row; %#ok<AGROW>
                local_flush(ledger_path, voy_c, samp_c, retry_c);
            end
        end

        sa = local_stratum_summary(st, si_plan, tpl, reg, n_draws, ...
            slot_fails, attempts_st, fam_ids, sg_ids, voy_c, samp_c, ...
            ctx);
        if isempty(stratum_audit)
            stratum_audit = sa;
        else
            stratum_audit(end+1) = sa; %#ok<AGROW>
        end
    end

    run_wall = toc(t_run);
    agg = local_overall(stratum_audit, voy_c, ctx, run_wall);

    plan_obj = ctx.plan;
    ph = plan_obj.self_hash;
    audit = struct();
    audit.schema = 'mission_v3_prequal_strata_audit_v1';
    audit.task = ['T-020 MISSION_V3_PREQUALIFICATION strata audit ', ...
        '(FR-557)'];
    audit.run_label = label;
    audit.generated_at = local_utc_now();
    audit.frozen_plan = struct('rel_path', ...
        'config/mission_v3/prequal_plan.json', ...
        'self_hash', char(ph.value));
    audit.formal_seed = ctx.formal_seed;
    rth = plan_obj.rejection_threshold_frozen;
    audit.thresholds = struct('per_stratum_max', ctx.thr_per_stratum, ...
        'overall_max', ctx.thr_overall, ...
        'definition', char(rth.definition), ...
        'accounting_note', ['draw-level accounting: attempts = ', ...
        'environment candidate draws tried per stratum; accepted = ', ...
        'accepted draws; each accepted draw expands to 4 member ', ...
        'voyages (12 accepted voyages per stratum = 3 draws x 2x2)']);
    audit.strata = local_pack(stratum_audit);
    audit.overall = agg;
    audit.regime_flow_statistics = local_regime_stats(regime_acc);
    if ctx.has_amendment
        audit.amendment = ctx.amendment;
        audit.normal_strata_reuse = reuse_ev;
    end
    audit.t429_self_check = local_t429_check(audit);

    man = local_manifest(ctx, label, frozen, voy_c, samp_c, retry_c, ...
        agg, run_wall, reuse_ev);

    local_write_json(audit_path, audit);
    local_write_json(man_path, man);
    local_write_json(ledger_path, struct('voyages', {voy_c}, ...
        'sampling', {samp_c}, 'retry', {retry_c}, 'throughput', ...
        man.throughput, 'verdict', char(agg.verdict)));

    try
        close_system(ctx.MDL, 0);
    catch
        % model not loaded (ProvisionBase=false); nothing to close
    end

    out = struct();
    out.run_label = label;
    out.run_dir = run_dir;
    out.audit_path = audit_path;
    out.manifest_path = man_path;
    out.ledger_path = ledger_path;
    out.audit = audit;
    out.manifest = man;
    out.verdict = char(agg.verdict);
    if verdict_error && strcmp(out.verdict, 'FAIL')
        error('GATE:MISSION_V3:PREQUAL:REJECTION', ...
            ['frozen rejection threshold exceeded (overall rate ', ...
             '%.4f vs threshold %.2f); audit and manifest written to ', ...
             '%s and %s (controlled FAIL by design, FR-539)'], ...
            double(agg.rejection_rate), ctx.thr_overall, audit_path, ...
            man_path);
    end
end

function samp = local_sample_logged(ctx, st, tpl, reg, draw_start, slot)
    samp = struct();
    samp.new_rows = {};
    try
        s = mission_v3.env_realize('sample', ctx.formal_seed, ...
            draw_start, tpl, reg, st, 'MaxAttempts', ctx.max_attempts);
        samp.accepted = true;
        samp.sample = s;
        samp.n_attempts = double(s.n_attempts);
        samp.err = '';
    catch ME
        if strcmp(local_err_id(ME), 'GATE:MISSION_V3:ENV:MAX_RETRY')
            samp.accepted = false;
            samp.sample = [];
            samp.n_attempts = ctx.max_attempts;
            samp.err = local_err_id(ME);
        else
            rethrow(ME);
        end
    end
    if samp.accepted
        s2 = samp.sample;
        sl = s2.ledger;
        for a = 1:double(s2.n_attempts)
            r = sl(a);
            d = r.disposition;
            row = struct('stratum_id', st, 'slot', double(slot), ...
                'attempt', double(r.attempt), ...
                'base_draw_index', double(r.base_draw_index), ...
                'disposition', char(d.disposition), ...
                'reason', char(d.reason), 'token_id', char(d.token_id));
            samp.new_rows{end+1} = row; %#ok<AGROW>
        end
    else
        row = struct('stratum_id', st, 'slot', double(slot), ...
            'attempt', double(ctx.max_attempts), ...
            'base_draw_index', double(draw_start + ctx.max_attempts - 1), ...
            'disposition', 'FAILED_GENERATION', 'reason', 'none', ...
            'token_id', ['env_max_retry: ' samp.err]);
        samp.new_rows{end+1} = row; %#ok<AGROW>
    end
end

function [row, voy_c, retry_c] = local_member_with_retry(ctx, si, st, ...
        tpl, reg, bdi, dag, rl, fam, k, voy_c, retry_c, resume)
    mem = fam.members(k);
    voyage_id = sprintf('PREQ_S%d_D%04d_M%d', si, bdi, k);

    if resume
        for j = 1:numel(voy_c)
            r0 = voy_c{j};
            if strcmp(char(r0.voyage_id), voyage_id) ...
                    && ~isempty(char(r0.payload_hash)) ...
                    && ~strcmp(char(r0.disposition), ...
                    'FAILED_GENERATION')
                art = fullfile(ctx.ROOT, char(r0.artifact_rel));
                if exist(art, 'file')
                    row = r0;
                    voy_c(j) = [];
                    return;
                end
            end
        end
    end

    n_try = ctx.retry_max + 1;
    row = [];
    for att = 1:n_try
        t_m = tic;
        try
            row = mission_v3.producer_prequal('member', ctx, si, bdi, ...
                dag, rl, fam, k);
            row.member_attempt = double(att);
            row.member_wall_s = toc(t_m);
            row.retry_of = double(max(0, att - 1));
            break;
        catch ME
            local_reraise_hard(ME);
            if att >= n_try
                row = local_blank_row(si, st, tpl, reg, bdi, fam, k, ...
                    voyage_id);
                row.member_attempt = double(att);
                row.member_wall_s = toc(t_m);
                row.retry_of = double(max(0, att - 1));
                row.disposition = 'FAILED_GENERATION';
                row.reason = 'none';
                row.detail = local_fail_detail(ME);
            else
                rr = mission_v3.eligibility('retry_record', voyage_id, ...
                    att, 'FAILED_GENERATION', att - 1);
                rr.detail = local_fail_detail(ME);
                rr.stratum_id = st;
                rr.voyage_id = voyage_id;
                retry_c{end+1} = rr; %#ok<AGROW>
            end
        end
    end
end

%% ======================================================================
%  one member execution (compile -> guard -> sim -> judge -> export)
%% ======================================================================
function row = local_member_wrapper(arg_cell)
    ctx = arg_cell{1};
    si = arg_cell{2};
    bdi = arg_cell{3};
    dag = arg_cell{4};
    rl = arg_cell{5};
    fam = arg_cell{6};
    k = arg_cell{7};
    pre = struct();
    if numel(arg_cell) >= 8
        pre = arg_cell{8};
    end
    if ~logical(ctx.model_ready)
        local_provision(ctx);
    end

    mem = fam.members(k);
    arms_v = mem.arms;
    voyage_id = sprintf('PREQ_S%d_D%04d_M%d', si, bdi, k);
    row = local_blank_row(si, char(dag.stratum_id), ...
        char(dag.template_id), char(dag.regime_id), bdi, fam, k, ...
        voyage_id);
    row.arms_traffic = char(arms_v.traffic);
    row.arms_forecast = char(arms_v.forecast);
    row.traffic_override = char(mem.traffic_override);

    use_rl = rl;
    if strcmp(row.traffic_override, 'OFF')
        use_rl.traffic = rl.traffic([]);
    end

    plan = mission_v3.plan_compose('compose', use_rl);
    comp = mission_v3.scenario_compile('compile', plan, use_rl);
    scn = comp.scn;

    p_base = dag.p_hotel_base_kW;
    d_hotel = dag.delta_p_hotel_unplanned;
    expect_hotel = double(p_base) + double(d_hotel);
    if abs(double(scn.hotel_kW) - expect_hotel) > 1e-9
        error('GATE:MISSION_V3:PREQUAL:HOTEL_GUARD', ...
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

    if isfield(pre, 'SimOnly') && logical(pre.SimOnly)
        row.scn_hotel_kW = double(scn.hotel_kW);
        row.plan_t_end_s = double(plan.t_grid_s(end));
        return;
    end

    t_sim = tic;
    simout = model.run_scenario_sim(ctx.MDL, scn, 0, scn.stop_time_s);
    row.sim_wall_s = toc(t_sim);
    ds = gate_ctrl.get_ds(simout);

    sel = local_grid1hz(ds.t);
    t1 = ds.t(sel);
    n1 = numel(t1);
    if n1 < 1
        error('PREQUAL_RUNTIME:EMPTY_STREAM', ...
            'voyage %s: simulation produced an empty time grid', ...
            voyage_id);
    end
    stream = struct();
    stream.time_s = t1(:);
    stream = local_fill_stream(ctx, ds, sel, n1, stream, voyage_id);
    stream.ctrl_mode = uint8(stream.ctrl_mode);
    stream.scenario_id = voyage_id;

    D = double(t1(end));
    row.duration_s = D;
    anchors = (300:10:(D - 300))';

    tg_col = plan.t_grid_s(:);
    cur_col = scn.current_value_mps(:);
    ou = rl.ou;
    npre = double(round(ou.prehistory_s));
    t_pre = (-npre:-1)';
    t_src = [t_pre; tg_col];
    u_src = [zeros(npre, 1); cur_col];

    prof = row.arms_forecast;
    nA = numel(anchors);
    fb_all = [];
    for m = 1:nA
        a = anchors(m);
        sl = t_src <= a;
        fb = mission_v3.forecast_issue('issue', u_src(sl), t_src(sl), ...
            a, ctx.leads, prof, 'FormalSeed', ctx.formal_seed, ...
            'BaseDrawIndex', double(bdi), 'RegimeId', row.regime_id, ...
            'DeltaWL', double(rl.water.delta_wl_m));
        if isempty(fb_all)
            fb_all = repmat(fb, nA, 1);
        end
        fb_all(m) = fb;
    end

    wtr = rl.water;
    kf = mission_v3.kf_snapshots('build_grid', plan, anchors, fb_all, ...
        'HChartM', double(wtr.h_chart_m));

    row_ok = false(nA, 1);
    for m = 1:nA
        mm = kf.missing_mask(m, :);
        cs = kf.contract_status{m};
        row_ok(m) = ~any(mm(:)) && strcmp(cs, 'OK');
    end
    voy = struct('duration_s', D, 'kf_issue_time_s', ...
        kf.issue_time_s(:), 'kf_row_ok', row_ok);
    elig = mission_v3.eligibility('judge', voy);

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
    meta.scenario_stratum = row.stratum_id;
    meta.generation_seed = double(scn.random_seed);
    meta.model_baseline_ref = ...
        'ModelE physical v1.1 + control v1.0 (P2 bench, law=0)';
    meta.producer_script_sha = ctx.producer_sha;
    meta.fixture_kind = 'NONE';
    meta.ood_axis = 'NONE';
    meta.ood_def_version = 'DGN-11-v1';
    meta.physics_domain_ref = 'ModelE validated domain (T-412 v1.1)';
    meta.h_chart_m = double(wtr.h_chart_m);
    meta.speed_limit_zones = plan.speed_limit_zones;
    meta.bridge_zones = plan.bridge_zones;
    meta.voyage_duration_s = D;
    meta.scenario_id = voyage_id;
    meta.forecast_provenance = fprov;
    meta.base_draw_id = char(fam.base_draw_id);
    meta.intervention = ['traffic=' row.arms_traffic ...
        ';forecast=' row.arms_forecast];
    meta.forecast_quality_profile_id = row.arms_forecast;
    meta.stratum = row.stratum_id;
    meta.dataset_domain = 'FORMAL';

    art_dir = ctx.artifact_dir;
    out_file = fullfile(art_dir, [voyage_id '.mat']);
    exp_res = mission_v3.producer_export('export_artifact', stream, kf, ...
        meta, elig, out_file);

    row.disposition = char(elig.disposition);
    row.reason = char(elig.reason);
    row.detail = char(elig.detail);
    row.n_stream = double(exp_res.n_stream_samples);
    row.n_kf_rows = double(exp_res.n_kf_rows);
    row.artifact_rel = ['datasets_v3/mission_v3/' char(ctx.run_label) ...
        '/' voyage_id '.mat'];
    row.artifact_sha256 = local_sha256_file(out_file);
    row.payload_hash = local_payload_hash(stream, kf, elig);
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
                error('GATE:MISSION_V3:PREQUAL:STREAM', ...
                    'voyage %s: logsout lacks %s (registry %s)', ...
                    voyage_id, nm, src);
            end
            col = ds.(nm);
            stream.(f) = col(sel);
        elseif strncmp(src, 'scenario:', 9)
            v = evalin('base', src(10:end));
            stream.(f) = repmat(double(v), n1, 1);
        else
            error('GATE:MISSION_V3:PREQUAL:STREAM', ...
                'voyage %s: unsupported source %s', voyage_id, src);
        end
    end
end

%% ======================================================================
%  two-run comparison (AC-438) + final audit/manifest
%% ======================================================================
function out = local_compare(arg_cell)
    ledger1 = char(arg_cell{1});
    ledger2 = char(arg_cell{2});
    audit2_path = char(arg_cell{3});
    man2_path = char(arg_cell{4});
    man_out = char(arg_cell{5});
    audit_out = char(arg_cell{6});

    l1 = local_read_json_nodir(ledger1);
    l2 = local_read_json_nodir(ledger2);
    v1 = local_cell_of(l1.voyages);
    v2 = local_cell_of(l2.voyages);

    mism = {};
    n_cmp = 0;
    for j = 1:numel(v2)
        r2 = v2{j};
        vid = char(r2.voyage_id);
        hit = '';
        for i = 1:numel(v1)
            r1 = v1{i};
            if strcmp(char(r1.voyage_id), vid)
                hit = char(r1.payload_hash);
                break;
            end
        end
        if isempty(hit)
            mism{end+1} = [vid ':missing_in_run1']; %#ok<AGROW>
        elseif ~strcmp(hit, char(r2.payload_hash))
            mism{end+1} = [vid ':payload_hash_mismatch']; %#ok<AGROW>
        else
            n_cmp = n_cmp + 1;
        end
    end
    consistent = isempty(mism);

    audit = local_read_json_nodir(audit2_path);
    audit.two_run_consistency = struct('enabled', true, ...
        'first_ledger', 'datasets_v3/mission_v3/prequal/_run_ledger.json', ...
        'second_ledger', ...
        'datasets_v3/mission_v3/prequal_rerun/_run_ledger.json', ...
        'voyages_compared', double(n_cmp), 'mismatches', {mism}, ...
        'payload_hash_consistent', logical(consistent));

    man = local_read_json_nodir(man2_path);
    man.two_run = audit.two_run_consistency;

    local_write_json(audit_out, audit);
    local_write_json(man_out, man);

    out = struct();
    out.consistent = consistent;
    out.voyages_compared = n_cmp;
    out.mismatches = {mism};
    if ~consistent
        error('GATE:MISSION_V3:PREQUAL:TWO_RUN', ...
            'two-run payload hash comparison found %d mismatches', ...
            numel(mism));
    end
    ov2 = audit.overall;
    if strcmp(char(ov2.verdict), 'FAIL')
        error('GATE:MISSION_V3:PREQUAL:REJECTION', ...
            ['frozen rejection threshold exceeded on the second run ', ...
             '(overall %.4f); final audit and manifest written ', ...
             '(controlled FAIL by design)'], ...
            double(ov2.rejection_rate));
    end
end

%% ======================================================================
%  per-stratum summary (recounted from ledgers; resume-safe)
%% ======================================================================
function sa = local_stratum_summary(st, si_plan, tpl, reg, n_draws, ...
    slot_fails, attempts_st, fam_ids, sg_ids, voy_c, samp_c, ctx)

    rows_st = local_rows_of_stratum(voy_c, st);
    n_acc = 0;
    n_rej = 0;
    n_fail = 0;
    rej_reasons = struct();
    for j = 1:numel(rows_st)
        r = rows_st{j};
        d = char(r.disposition);
        if strcmp(d, 'ACCEPT')
            n_acc = n_acc + 1;
        elseif strncmp(d, 'REJECTED', 8)
            n_rej = n_rej + 1;
            rs = char(r.reason);
            rej_reasons = local_bump(rej_reasons, rs);
        elseif ~isempty(d)
            n_fail = n_fail + 1;
            rej_reasons = local_bump(rej_reasons, d);
        end
    end
    tok_counts = struct();
    for j = 1:numel(samp_c)
        s = samp_c{j};
        if strcmp(char(s.stratum_id), st) ...
                && strncmp(char(s.disposition), ...
                'REJECTED_STRUCTURAL_HELDOUT', 28)
            tok = char(s.token_id);
            tok_counts = local_bump(tok_counts, tok);
        end
    end

    sa = struct();
    sa.stratum_id = st;
    sa.stratum_index = double(si_plan);
    sa.template_id = tpl;
    sa.regime_id = reg;
    sa.draw_start_base = double((si_plan - 1) * 1000);
    sa.slots_planned = double(n_draws);
    sa.accepted_draws = double(n_draws - slot_fails);
    sa.slots_failed = double(slot_fails);
    sa.env_attempts_total = double(attempts_st);
    sa.accepted_voyages = double(n_acc);
    sa.rejected_voyages = double(n_rej);
    sa.failed_voyages = double(n_fail);
    sa.rejected_reason_counts = local_nonempty(rej_reasons);
    sa.token_heldout_counts = local_nonempty(tok_counts);
    sa.n_families = double(numel(fam_ids));
    sa.n_split_groups = double(numel(unique(sg_ids)));
    sa.split_group_projection = struct( ...
        'unique_split_groups_current', ...
        double(numel(unique(sg_ids))), ...
        'lower_bound_reference', 14, ...
        'note', ['projection only: current unique split-group count ', ...
        'vs the 14 lower bound; no Stage-B quota satisfaction claim is ', ...
        'made here']);
    sa.rejection_rate = local_rate(attempts_st, n_draws - slot_fails);
    if local_rate_gt(sa.rejection_rate, ctx.thr_per_stratum)
        sa.verdict = 'FAIL';
    else
        sa.verdict = 'PASS';
    end
end

%% ======================================================================
%  frozen chain verification (fail-closed, run start)
%% ======================================================================
function frozen = local_verify_frozen(ctx)
    ROOT = ctx.ROOT;
    plan = ctx.plan;
    cons_wrap = plan.consumes;
    frozen = struct();

    reg_checks = struct([]);
    expected_map = cons_wrap.registries_frozen;
    rfiles = fieldnames(expected_map);
    for j = 1:numel(rfiles)
        rf_field = rfiles{j};
        % jsondecode strips the dot from JSON keys; restore '<name>_json'
        % to '<name>.json' deterministically
        rf = regexprep(rf_field, '_json$', '.json');
        expected = char(expected_map.(rf_field));
        obj = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), rf);
        sh = obj.self_hash;
        ok = isfield(obj, 'self_hash') && ...
            strcmp(char(sh.value), expected);
        row = struct('file', ['config/mission_v3/' rf], ...
            'expected_self_hash', expected, 'ok', logical(ok));
        if isempty(reg_checks)
            reg_checks = row;
        else
            reg_checks(end+1) = row; %#ok<AGROW>
        end
        if ~ok
            error('GATE:MISSION_V3:PREQUAL:FROZEN_REGISTRY', ...
                'frozen registry self_hash mismatch: %s', rf);
        end
    end
    frozen.registry_hash_checks = local_pack(reg_checks);

    fid_path = fullfile(ROOT, 'results', 'mission_v3', ...
        'qualification_stack_identity.json');
    frozen_identity = local_read_json_nodir(fid_path);
    fresh = mission_v3.stack_identity('collect', ROOT);

    fcomps = local_struct_cell(frozen_identity.components);
    ncomps = local_struct_cell(fresh.components);

    for j = 1:numel(fcomps)
        c = fcomps{j};
        nm = char(c.component);
        if strcmp(nm, 'mission_v3_generator')
            continue;
        end
        cj = local_find_comp(ncomps, nm);
        if isempty(cj)
            error('GATE:MISSION_V3:PREQUAL:STACK', ...
                'frozen stack component missing in fresh collect: %s', nm);
        end
        if ~strcmp(char(cj.sha256), char(c.sha256))
            error('GATE:MISSION_V3:PREQUAL:STACK', ...
                'frozen stack component changed since T-019: %s', nm);
        end
    end

    gen_dir = fullfile(ROOT, 'scripts', '+mission_v3');
    [h_all, names_all] = local_gen_hashes(gen_dir);
    keep = ~strcmp(names_all, 'producer_prequal.m');
    comp_wo = local_gen_composite(names_all(keep), h_all(keep));
    comp_all = local_gen_composite(names_all, h_all);
    gen_frozen = '';
    for j = 1:numel(fcomps)
        c = fcomps{j};
        if strcmp(char(c.component), 'mission_v3_generator')
            gen_frozen = char(c.sha256);
        end
    end
    if isempty(gen_frozen)
        error('GATE:MISSION_V3:PREQUAL:STACK', ...
            'frozen identity lacks the mission_v3_generator component');
    end
    if strcmp(comp_all, gen_frozen)
        match_form = 'with_producer';
        delta_note = ['frozen identity was captured with the T-020 ', ...
            'producer present (refreshed after PREQUAL-AMEND-01); the ', ...
            'current generator composite reproduces the frozen ', ...
            'composite bitwise'];
    elseif strcmp(comp_wo, gen_frozen)
        match_form = 'minus_producer';
        delta_note = ['the only generator delta vs the T-019 ', ...
            'qualified stack is the addition of producer_prequal.m ', ...
            '(the T-020 producer itself); all frozen-time generator ', ...
            'files reproduce the frozen composite bitwise'];
    else
        error('GATE:MISSION_V3:PREQUAL:STACK', ...
            ['generator composite does not reproduce the frozen ', ...
             'qualified composite in either accepted form (with or ', ...
             'without producer_prequal.m): scientific generator files ', ...
             'changed since the frozen capture']);
    end
    frozen.generator_frozen_composite = gen_frozen;
    frozen.generator_composite_minus_producer = comp_wo;
    frozen.generator_composite_with_producer = comp_all;
    frozen.generator_match_form = match_form;
    frozen.delta_note = delta_note;
end

function [hashes, names] = local_gen_hashes(gen_dir)
    lst = dir(fullfile(gen_dir, '*.m'));
    names = cell(1, 0);
    hashes = cell(1, 0);
    for i = 1:numel(lst)
        if lst(i).isdir, continue; end
        names{end+1} = lst(i).name; %#ok<AGROW>
        hashes{end+1} = local_sha256_file(fullfile(gen_dir, ...
            lst(i).name)); %#ok<AGROW>
    end
    [names, si] = sort(names);
    hashes = hashes(si);
end

function h = local_gen_composite(names, hashes)
    parts = cell(1, numel(names));
    for i = 1:numel(names)
        parts{i} = ['"' names{i} '":"' hashes{i} '"'];
    end
    canon = ['{' strjoin(parts, ',') '}'];
    h = local_sha256_bytes(unicode2native(canon, 'UTF-8'));
end

function cj = local_find_comp(comps, name)
    cj = [];
    for j = 1:numel(comps)
        c = comps{j};
        if strcmp(char(c.component), name)
            cj = c;
            return;
        end
    end
end

%% ======================================================================
%  manifest assembly
%% ======================================================================
function man = local_manifest(ctx, label, frozen, voy_c, samp_c, ...
        retry_c, agg, run_wall, reuse_ev)
    man = struct();
    man.schema = 'mission_v3_prequal_manifest_v1';
    man.task = ['T-020 MISSION_V3_PREQUALIFICATION manifest ', ...
        '(readiness only, no result metrics)'];
    man.run_label = label;
    man.generated_at = local_utc_now();
    plan_obj = ctx.plan;
    ph = plan_obj.self_hash;
    man.plan = struct('rel_path', ...
        'config/mission_v3/prequal_plan.json', ...
        'self_hash', char(ph.value), ...
        'status', 'FROZEN_PRE_REGISTRATION');
    man.stack = frozen;

    n_sim = 0;
    sim_wall_total = 0;
    vrows = {};
    for j = 1:numel(voy_c)
        r = voy_c{j};
        d = char(r.disposition);
        if ~strcmp(d, 'FAILED_GENERATION')
            n_sim = n_sim + 1;
        end
        sw = 0;
        if isfield(r, 'sim_wall_s')
            sw = double(r.sim_wall_s);
        end
        if ~isnan(sw)
            sim_wall_total = sim_wall_total + sw;
        end
        v = struct();
        v.voyage_id = char(r.voyage_id);
        v.base_draw_index = double(r.base_draw_index);
        v.family_id = char(r.family_id);
        v.split_group_id = char(r.split_group_id);
        v.stratum_id = char(r.stratum_id);
        v.arms_traffic = char(r.arms_traffic);
        v.arms_forecast = char(r.arms_forecast);
        v.traffic_override = char(r.traffic_override);
        v.disposition = d;
        v.reason = char(r.reason);
        v.detail = char(r.detail);
        v.artifact_rel_path = char(r.artifact_rel);
        v.artifact_sha256 = char(r.artifact_sha256);
        v.payload_hash = char(r.payload_hash);
        v.duration_s = double(r.duration_s);
        v.sim_wall_s = sw;
        v.attempts = double(r.member_attempt);
        vrows{end+1} = v; %#ok<AGROW>
    end
    man.voyages = vrows;
    man.sampling_ledger = local_pack_cell(samp_c);
    man.retry_ledger = local_pack_cell(retry_c);

    tp = struct();
    tp.voyages_simulated = double(n_sim);
    tp.sim_wall_total_s = double(sim_wall_total);
    tp.run_wall_total_s = double(run_wall);
    if run_wall > 0
        tp.voyages_per_hour = double(n_sim * 3600 / run_wall);
    else
        tp.voyages_per_hour = NaN;
    end
    if n_sim > 0
        tp.mean_sim_wall_s = double(sim_wall_total / n_sim);
    else
        tp.mean_sim_wall_s = NaN;
    end
    man.throughput = tp;
    man.overall = agg;
    if ctx.has_amendment
        man.amendment = ctx.amendment;
        man.normal_strata_reuse = reuse_ev;
    end
    man.t429_self_check = local_t429_check(man);
end

%% ======================================================================
%  helpers
%% ======================================================================
function row = local_blank_row(si, st, tpl, reg, bdi, fam, k, voyage_id)
    mem = fam.members(k);
    arms_v = mem.arms;
    row = struct();
    row.voyage_id = voyage_id;
    row.stratum_id = st;
    row.stratum_index = double(si);
    row.template_id = tpl;
    row.regime_id = reg;
    row.slot = NaN;
    row.base_draw_index = double(bdi);
    row.family_id = char(fam.family_id);
    row.split_group_id = char(fam.split_group_id);
    row.member_id = char(mem.member_id);
    row.arms_traffic = char(arms_v.traffic);
    row.arms_forecast = char(arms_v.forecast);
    row.traffic_override = char(mem.traffic_override);
    row.member_attempt = 1;
    row.retry_of = 0;
    row.disposition = '';
    row.reason = '';
    row.detail = '';
    row.duration_s = NaN;
    row.n_stream = 0;
    row.n_kf_rows = 0;
    row.sim_wall_s = NaN;
    row.member_wall_s = NaN;
    row.artifact_rel = '';
    row.artifact_sha256 = '';
    row.payload_hash = '';
end

function agg = local_overall(stratum_audit, voy_c, ctx, run_wall)
    att = 0;
    acc = 0;
    n_voy = 0;
    n_acc_v = 0;
    n_fail_v = 0;
    any_fail = false;
    for j = 1:numel(stratum_audit)
        s = stratum_audit(j);
        att = att + double(s.env_attempts_total);
        acc = acc + double(s.accepted_draws);
        n_acc_v = n_acc_v + double(s.accepted_voyages);
        n_fail_v = n_fail_v + double(s.failed_voyages);
        n_voy = n_voy + double(s.accepted_voyages) ...
            + double(s.rejected_voyages) + double(s.failed_voyages);
        if strcmp(char(s.verdict), 'FAIL')
            any_fail = true;
        end
    end
    agg = struct();
    agg.env_attempts_total = double(att);
    agg.accepted_draws = double(acc);
    agg.rejection_rate = local_rate(att, acc);
    agg.voyages_total = double(n_voy);
    agg.voyages_accepted = double(n_acc_v);
    agg.voyages_failed = double(n_fail_v);
    agg.run_wall_total_s = double(run_wall);
    overall_fail = local_rate_gt(agg.rejection_rate, ctx.thr_overall);
    if any_fail || overall_fail
        agg.verdict = 'FAIL';
    else
        agg.verdict = 'PASS';
    end
end

function r = local_rate(att, acc)
    if att > 0
        r = double(att - acc) / double(att);
    else
        r = NaN;
    end
end

function tf = local_rate_gt(r, thr)
    if isnan(double(r))
        tf = true;
    else
        tf = double(r) > double(thr);
    end
end

function sel = local_grid1hz(t)
    tt = double(t(:)) * 100;
    tt_r = round(tt);
    sel = (abs(tt - tt_r) <= 1e-6) & (mod(tt_r, 100) == 0);
end

function sm = local_stream_map(ctx)
    sm = struct();
    schema = ctx.schema;
    fams = schema.capability_families;
    tstr = fams.truth_stream;
    fl = local_struct_cell(tstr.fields);
    sigreg = ctx.sigreg;
    sigs = local_struct_cell(sigreg.signals);
    for i = 1:numel(fl)
        f = fl{i};
        if ~isfield(f, 'required') || ~logical(f.required)
            continue;
        end
        nm = char(f.name);
        if strcmp(nm, 'time_s')
            continue;
        end
        tyn = char(f.type);
        if ~isempty(strfind(tyn, 'string|int'))
            % scalar id field (scenario_id); filled at member assembly
            continue;
        end
        src = '';
        for j = 1:numel(sigs)
            sg = sigs{j};
            if strcmp(char(sg.name), nm)
                src = char(sg.source_signal);
                break;
            end
        end
        if isempty(src)
            error('GATE:MISSION_V3:PREQUAL:STREAM', ...
                'signal registry has no source for required field %s', nm);
        end
        row = struct('source', src);
        sm.(nm) = row;
    end
end

function c = local_struct_cell(x)
    % jsondecode returns arrays of JSON objects as struct arrays only when
    % every element shares the same field set; otherwise a cell array of
    % structs.  Normalize both shapes to a cell of scalar structs.
    c = {};
    if isstruct(x)
        for i = 1:numel(x)
            c{end+1} = x(i); %#ok<AGROW>
        end
    elseif iscell(x)
        for i = 1:numel(x)
            v = x{i};
            if isstruct(v)
                c{end+1} = v; %#ok<AGROW>
            end
        end
    end
end

function h = local_payload_hash(stream, kf, elig)
    lines = {'MISSION_V3_PREQUAL_PAYLOAD_V1'};
    lines = local_ser('stream', stream, lines);
    lines = local_ser('kf', kf, lines);
    lines{end+1} = ['elig.disposition=' char(elig.disposition)]; %#ok<AGROW>
    lines{end+1} = ['elig.reason=' char(elig.reason)]; %#ok<AGROW>
    lines{end+1} = ['elig.token_id=' char(elig.token_id)]; %#ok<AGROW>
    h = local_sha256_text(strjoin(lines, newline));
end

function lines = local_ser(name, val, lines)
    if isstruct(val)
        fn = fieldnames(val);
        fnc = strjoin(fn.', ',');
        lines{end+1} = sprintf('%s struct n=%d fields=[%s]', name, ...
            numel(val), fnc); %#ok<AGROW>
        for i = 1:numel(val)
            el = val(i);
            for j = 1:numel(fn)
                f = fn{j};
                lines = local_ser([name '.' f], el.(f), lines);
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
        error('GATE:MISSION_V3:PREQUAL:SER', ...
            'unsupported payload class at %s', name);
    end
end

function ok = local_t429_check(obj)
    blacklist = {'sbd', 'ba', 'confidence', 'confusion', 'accuracy', ...
        'precision', 'recall', 'f1', 'auc', 'roc'};
    names = local_walk_names(obj);
    for i = 1:numel(names)
        nm = lower(char(names{i}));
        for j = 1:numel(blacklist)
            pat = ['(^|_)' blacklist{j} '($|_)'];
            if ~isempty(regexp(nm, pat, 'once'))
                error('GATE:MISSION_V3:PREQUAL:T429', ...
                    'forbidden result-metric field name: %s', names{i});
            end
        end
    end
    ok = true;
end

function names = local_walk_names(obj)
    names = {};
    if isstruct(obj)
        fn = fieldnames(obj);
        for i = 1:numel(fn)
            names{end+1} = fn{i}; %#ok<AGROW>
        end
        for i = 1:numel(obj)
            el = obj(i);
            for j = 1:numel(fn)
                sub = local_walk_names(el.(fn{j}));
                names = [names, sub]; %#ok<AGROW>
            end
        end
    elseif iscell(obj)
        for i = 1:numel(obj)
            sub = local_walk_names(obj{i});
            names = [names, sub]; %#ok<AGROW>
        end
    end
end

function st = local_prev_ledger(path)
    obj = local_read_json_nodir(path);
    st = struct();
    st.voyages = local_cell_of(obj.voyages);
    st.sampling = local_cell_of(obj.sampling);
    st.retry = local_cell_of(obj.retry);
end

function c = local_cell_of(v)
    c = {};
    if isstruct(v)
        for i = 1:numel(v)
            c{end+1} = v(i); %#ok<AGROW>
        end
    elseif iscell(v)
        c = v;
    end
end

function rows = local_rows_of_stratum(voy_c, st)
    rows = {};
    for j = 1:numel(voy_c)
        r = voy_c{j};
        if strcmp(char(r.stratum_id), st)
            rows{end+1} = r; %#ok<AGROW>
        end
    end
end

function s = local_bump(s, key)
    if isfield(s, key)
        s.(key) = double(s.(key)) + 1;
    else
        s.(key) = 1;
    end
end

function s = local_nonempty(s)
    if isempty(fieldnames(s))
        s = struct();
        s.none = 0;
    end
end

function si = local_stratum_index(ctx, st)
    si = find(strcmp(ctx.strata, st), 1);
    if isempty(si)
        error('GATE:MISSION_V3:PREQUAL:STRATUM', ...
            'stratum not in the frozen plan list: %s', st);
    end
end

function c = local_cellstr_of(v)
    c = {};
    if iscell(v)
        for i = 1:numel(v)
            c{end+1} = char(v{i}); %#ok<AGROW>
        end
    elseif isstring(v)
        for i = 1:numel(v)
            c{end+1} = char(v(i)); %#ok<AGROW>
        end
    elseif ischar(v)
        c{1} = char(v);
    else
        error('GATE:MISSION_V3:PREQUAL:ARG', 'expected a cellstr value');
    end
end

function v = local_get(ov, name, def)
    v = def;
    if isfield(ov, name)
        v = ov.(name);
    end
end

function ov = local_parse_named(arg_cell)
    ov = struct();
    k = 1;
    while k <= numel(arg_cell)
        nm = arg_cell{k};
        if ~(ischar(nm) || isstring(nm)) || k + 1 > numel(arg_cell)
            error('GATE:MISSION_V3:PREQUAL:ARG', ...
                ['name-value arguments expected (got numel=%d, ', ...
                 'element %d class=%s)'], numel(arg_cell), k, class(nm));
        end
        ov.(char(nm)) = arg_cell{k+1};
        k = k + 2;
    end
end

function id = local_err_id(ME)
    if isfield(ME, 'identifier') && ~isempty(ME.identifier)
        id = char(ME.identifier);
    else
        id = 'UNKNOWN_ERROR';
    end
end

function d = local_fail_detail(ME)
    msg = char(ME.message);
    nl = strfind(msg, newline);
    if ~isempty(nl)
        msg = msg(1:nl(1)-1);
    end
    if numel(msg) > 160
        msg = [msg(1:160) '...'];
    end
    d = [local_err_id(ME) ': ' msg];
end

function local_reraise_hard(ME)
    id = local_err_id(ME);
    if ~isempty(id) && strncmp(id, 'GATE:', 5)
        rethrow(ME);
    end
end

function c = local_pack(v)
    c = {};
    if isstruct(v)
        for i = 1:numel(v)
            c{end+1} = v(i); %#ok<AGROW>
        end
    end
end

function c = local_pack_cell(v)
    c = {};
    if iscell(v)
        c = v;
    elseif isstruct(v)
        for i = 1:numel(v)
            c{end+1} = v(i); %#ok<AGROW>
        end
    end
end

function local_flush(ledger_path, voy_c, samp_c, retry_c)
    try
        local_write_json(ledger_path, struct('voyages', {voy_c}, ...
            'sampling', {samp_c}, 'retry', {retry_c}));
    catch
        % incremental flush is best-effort; the final write is authoritative
    end
end

function acc = local_regime_acc(acc, regime, ur)
    ur = double(ur(:));
    pk = max(abs(ur));
    idx = find(strcmp({acc.regime}, regime), 1);
    if isempty(idx)
        row = struct('regime', regime, 'pool', ur, 'peaks', pk);
        if isempty(acc)
            acc = row;
        else
            acc(end+1) = row; %#ok<AGROW>
        end
    else
        acc(idx).pool = [acc(idx).pool; ur]; %#ok<AGROW>
        acc(idx).peaks = [acc(idx).peaks; pk]; %#ok<AGROW>
    end
end

function rfs = local_regime_stats(acc)
    rfs = struct([]);
    for i = 1:numel(acc)
        a = acc(i);
        s = a.pool;
        pk = a.peaks;
        row = struct('regime', char(a.regime), ...
            'n_accepted_draws', double(numel(pk)), ...
            'n_samples', double(numel(s)), ...
            'u_mean', double(mean(s)), ...
            'u_median', double(median(s)), ...
            'u_q05', local_quantile(s, 0.05), ...
            'u_q25', local_quantile(s, 0.25), ...
            'u_q75', local_quantile(s, 0.75), ...
            'u_q95', local_quantile(s, 0.95), ...
            'draw_peak_abs_min', double(min(pk)), ...
            'draw_peak_abs_median', double(median(pk)), ...
            'draw_peak_abs_mean', double(mean(pk)), ...
            'draw_peak_abs_max', double(max(pk)), ...
            'note', ['U_river (river coordinate, downstream ', ...
            'positive) realized samples pooled over accepted draws; ', ...
            'quantiles of the pooled 1 Hz series; draw peaks are the ', ...
            'per-draw sup|U_river| over the full grid including ', ...
            'prehistory (PREQUAL-AMEND-01 section B verification)']);
        if isempty(rfs)
            rfs = row;
        else
            rfs(end+1) = row; %#ok<AGROW>
        end
    end
    rfs = local_pack(rfs);
end

function q = local_quantile(v, p)
    v = sort(double(v(:)));
    n = numel(v);
    if n == 0
        q = NaN;
        return;
    end
    if n == 1
        q = v(1);
        return;
    end
    x = (0:(n-1))' / (n - 1);
    q = interp1(x, v, p, 'linear');
end

function s = local_utc_now()
    s = char(datetime('now', 'TimeZone', 'UTC', ...
        'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'''));
end

function obj = local_read_json_nodir(path)
    txt = fileread(path);
    obj = jsondecode(txt);
end

function obj = local_read_json(p1, varargin)
    if nargin >= 3
        p = fullfile(p1, varargin{1}, varargin{2});
    else
        p = fullfile(p1, varargin{1});
    end
    txt = fileread(p);
    obj = jsondecode(txt);
end

function local_write_json(path, obj)
    local_t429_check(obj);
    txt = jsonencode(obj, 'PrettyPrint', true);
    tmp = [path '.tmp'];
    fid = fopen(tmp, 'w');
    if fid < 0
        error('GATE:MISSION_V3:PREQUAL:IO', ...
            'cannot open %s for writing', tmp);
    end
    fwrite(fid, txt, 'char');
    fclose(fid);
    movefile(tmp, path, 'f');
end

function h = local_sha256_text(s)
    bytes = unicode2native(s, 'UTF-8');
    h = local_sha256_bytes(bytes);
end

function h = local_sha256_file(path)
    fid = fopen(path, 'r');
    if fid < 0
        error('GATE:MISSION_V3:PREQUAL:IO', ...
            'cannot open file for hashing: %s', path);
    end
    raw = fread(fid, inf, 'uint8=>uint8');
    fclose(fid);
    h = local_sha256_bytes(raw);
end

function h = local_sha256_bytes(bytes)
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(bytes(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end
