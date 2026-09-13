function out = gen_iid(plan, ctx)
% fleet_v3.gen_iid - IID qualification generation for the 73 Stage A
% pipeline (T-018 Phase 4-D).  Same scientific call chain as the C-side
% producer_prequal run face (env_realize('sample') -> family_expand ->
% per-member plan_compose -> scenario_compile -> simulation -> forecast
% -> kf -> eligibility -> export) but plan-driven: the environment
% acceptance face stays the frozen env_realize IID judgment (support
% bounds + structural heldout EXCLUDE), the voyage eligibility criteria
% are injected from the activation record, and the seed/draw segments
% come from the plan (activation stage_a "IID: stratum k=900..901";
% smoke segment k=950).
%
%   out = fleet_v3.gen_iid(plan, ctx)
%
% Segment interpretation (mechanical, per the frozen record text "IID:
% stratum k=900..901"): EVERY stratum starts its own draw segment at
% plan.iid.draw_base and advances within [base, base+draws) on accepts;
% a failed slot advances past the env_realize max-attempts block (the
% producer_prequal advance rule).  The formal Stage B rule "[i*1000,
% (i+1)*1000) k=0..15" is parameterized in plan.iid for Phase 2.
%
% IID meta: dataset_domain='FORMAL', ood_axis='NONE' (C corpus
% conventions).  Rejection-rate gates (per stratum / overall) come from
% the plan; a violation is a controlled FAIL
% (GATE:FLEETV3:IID:REJECTION).
%
% out fields: strata summary struct array, voyage rows, sampling rows,
% retry rows, rejection rates, verdict, artifact dir.
%
% Error ids: GATE:FLEETV3:IID:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; thin body + local
% functions; no chained indexed assignment.

    [strata_sel, draws, draw_base, seed] = local_segment(plan);

    mode = char(plan.mode);
    if strcmp(mode, 'smoke')
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'qualification_smoke', 'iid');
    elseif strcmp(mode, 'stage_b_iid')
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'formal', 'iid');
    else
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'qualification', 'stage_a_iid');
    end
    if ~exist(art_dir, 'dir')
        mkdir(art_dir);
    end

    voy_c = {};
    samp_c = {};
    retry_c = {};
    stratum_audit = struct([]);
    attempts_all = 0;
    draws_all = 0;
    t_run = tic;

    % strata are independent (each starts its own draw segment at
    % draw_base with local slot bookkeeping), so per-stratum task
    % splitting is semantically exact vs the serial loop; run_tasks
    % dispatches them in parallel (PCT) or serially, and this collector
    % merges in canonical task (= strata) order.
    args_cell = cell(1, numel(strata_sel));
    if strcmp(char(plan.mode), 'stage_b_iid')
        % FR-497 Stage B formal: stratum i draws k=0..15 starting at
        % i*draw_stride; each (stratum, family k) is an independent task
        % with a RESERVED attempt window [i*stride+k*W, (k+1)*W) so that
        % parallel slots can never sample the same draw index
        % (W = floor(stride/k_formal); rejected attempts advance within
        % the window only).  stage_a/smoke keep the sequential-advance
        % semantics (audited serial path).
        % AMENDMENT_03 two-phase workflow (Final Ruling 2026-09-09,
        % option A'): Phase S freezes a deterministic canonical-order
        % allocation plan; Phase R workers simulate only the accepted
        % base draws (no candidate search on workers; the same frozen
        % plan drives pass1 and pass2).
        plan_file = fullfile(art_dir, ...
            'formal_draw_allocation_plan_iid.json');
        if exist(plan_file, 'file')
            alloc = jsondecode(fileread(plan_file));
            fprintf('[fleet_v3] IID: consuming frozen allocation plan\n');
        else
            alloc = fleet_v3.allocate_draws(plan, ctx, 'iid', ...
                'ArtDir', art_dir);
            fprintf(['[fleet_v3] IID: allocation plan frozen ', ...
                '(%d consumed candidates, self_hash %.16s)\n'], ...
                numel(alloc.consumed), alloc.self_hash);
        end
        n_tasks = 0;
        for s_i = 1:numel(alloc.iid.strata)
            sa = alloc.iid.strata(s_i);
            st = char(sa.stratum_id);
            si_plan = find(strcmp(plan.iid.strata, st), 1);
            if isempty(si_plan)
                error('GATE:FLEETV3:IID:STRATUM', ...
                    'stratum %s not in the frozen plan list', st);
            end
            pb = plan;
            pb.iid.max_attempts = 1;   % Phase R: one candidate per draw
            slots_arr = sa.slots;
            for k = 1:numel(slots_arr)
                e = slots_arr(k);
                while iscell(e), e = e{1}; end
                if isstruct(e), e = e(1); end
                bdi = double(e.base_draw_index);
                n_tasks = n_tasks + 1;
                args_cell{n_tasks} = {pb, ctx, st, si_plan, 1, ...
                    bdi, seed, art_dir};
            end
        end
        args_cell = args_cell(1:n_tasks);
    else
        for si_loop = 1:numel(strata_sel)
            st = char(strata_sel{si_loop});
            si_plan = find(strcmp(plan.iid.strata, st), 1);
            if isempty(si_plan)
                error('GATE:FLEETV3:IID:STRATUM', ...
                    'stratum %s not in the frozen plan list', st);
            end
            args_cell{si_loop} = {plan, ctx, st, si_plan, draws, ...
                draw_base, seed, art_dir};
        end
    end
    outs = fleet_v3.run_tasks(@fleet_v3.task_iid_stratum, args_cell, ...
        local_worker_cap(), 'IID');

    for t = 1:numel(outs)
        o = outs{t};
        voy_c = [voy_c, o.voyages]; %#ok<AGROW>
        samp_c = [samp_c, o.sampling]; %#ok<AGROW>
        retry_c = [retry_c, o.retry]; %#ok<AGROW>
        if isempty(stratum_audit)
            stratum_audit = o.stratum_audit;
        else
            stratum_audit(end+1) = o.stratum_audit; %#ok<AGROW>
        end
        attempts_all = attempts_all + o.attempts;
        draws_all = draws_all + o.draws_ok;
    end

    rate = local_rate(attempts_all, draws_all);
    verdict = 'PASS';
    if local_rate_gt(rate, plan.iid.thr_overall)
        verdict = 'FAIL';
    end
    for i = 1:numel(stratum_audit)
        if strcmp(char(stratum_audit(i).verdict), 'FAIL')
            verdict = 'FAIL';
        end
    end

    out = struct();
    out.corpus = 'IID';
    out.mode = mode;
    out.artifact_dir = art_dir;
    out.seed = seed;
    out.draw_base = draw_base;
    out.draws = draws;
    out.strata = stratum_audit;
    out.voyages = voy_c;
    out.sampling = samp_c;
    out.retry = retry_c;
    out.rejection_rate = rate;
    out.verdict = verdict;
    out.run_wall_s = toc(t_run);
    out.n_simulated = local_n_simulated(voy_c);

    % incremental ledger (crash/resume evidence; written before any
    % controlled FAIL so a rejection-gate exposure keeps its records)
    local_write_json(fullfile(art_dir, '_iid_ledger.json'), ...
        struct('corpus', {out.corpus}, 'mode', {out.mode}, ...
        'seed', {out.seed}, 'voyages', {voy_c}, ...
        'sampling', {samp_c}, 'retry', {retry_c}, ...
        'rejection_rate', rate, 'verdict', verdict));

    if strcmp(verdict, 'FAIL')
        % AMENDMENT_04: IID rejection gate retains its frozen 0.50
        % thresholds, computed from the amended allocator's consumed
        % ledger.  A failure here is a population-distribution quality
        % failure - STOP per Final Ruling (escalate, do not widen).
        error('GATE:FLEETV3:IID:REJECTION', ...
            ['IID rejection gate FAIL (overall %.4f, threshold ', ...
             '%.2f; per-stratum gate %.2f) under the amended ', ...
             'allocator; ledger recorded in the returned struct ', ...
             '(controlled FAIL by design)'], ...
            rate, plan.iid.thr_overall, plan.iid.thr_stratum);
    end
end

%% ============================ segment ===============================

function [strata_sel, draws, draw_base, seed] = local_segment(plan)
    if strcmp(char(plan.mode), 'smoke')
        n = double(plan.smoke.iid_strata);
        if n > numel(plan.iid.strata)
            error('GATE:FLEETV3:IID:SEGMENT', ...
                'smoke asks for %d strata, plan has %d', n, ...
                numel(plan.iid.strata));
        end
        strata_sel = plan.iid.strata(1:n);
        draws = double(plan.smoke.iid_draws);
        draw_base = double(plan.smoke.draw_base);
        seed = char(plan.iid.seed);
    else
        strata_sel = plan.iid.strata;
        draws = double(plan.iid.draws);
        draw_base = double(plan.iid.draw_base);
        seed = char(plan.iid.seed);
    end
end

%% ========================= sample + retry ===========================

function samp = local_sample_logged(plan, seed, draw_start, tpl, reg, ...
        st, slot)
    samp = struct();
    samp.new_rows = {};
    try
        s = mission_v3.env_realize('sample', seed, draw_start, tpl, ...
            reg, st, 'MaxAttempts', plan.iid.max_attempts);
        samp.accepted = true;
        samp.sample = s;
        samp.n_attempts = double(s.n_attempts);
    catch ME
        if strcmp(local_err_id(ME), 'GATE:MISSION_V3:ENV:MAX_RETRY')
            samp.accepted = false;
            samp.sample = [];
            samp.n_attempts = plan.iid.max_attempts;
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
            'attempt', double(plan.iid.max_attempts), ...
            'base_draw_index', ...
            double(draw_start + plan.iid.max_attempts - 1), ...
            'disposition', 'FAILED_GENERATION', 'reason', 'none', ...
            'token_id', 'env_max_retry');
        samp.new_rows{end+1} = row; %#ok<AGROW>
    end
end

function [row, retry_c] = local_member_with_retry(plan, ctx, art_dir, ...
        si_plan, st, bdi, dag, rl, fam, k, retry_c)
    mem = fam.members(k);
    voyage_id = sprintf('%s_S%d_D%05d_M%d', local_prefix(plan), ...
        si_plan, bdi, k);

    n_try = double(plan.iid.retry_max) + 1;
    row = [];
    for att = 1:n_try
        t_m = tic;
        try
            row = fleet_v3.member_exec(ctx, voyage_id, dag, rl, fam, ...
                k, struct());
            row.member_attempt = double(att);
            row.member_wall_s = toc(t_m);
            row.retry_of = double(max(0, att - 1));
            row.artifact_dir = art_dir;
            break;
        catch ME
            local_reraise_hard(ME);
            if att >= n_try
                row = local_blank_row(si_plan, st, bdi, fam, k, ...
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

function p = local_prefix(plan)
    switch char(plan.mode)
        case 'smoke'
            p = 'FLTS';
        case 'stage_b_iid'
            p = 'FLTB';
        otherwise
            p = 'FLTA';
    end
end

function row = local_blank_row(si_plan, st, bdi, fam, k, voyage_id)
    mem = fam.members(k);
    arms_v = mem.arms;
    row = struct();
    row.voyage_id = voyage_id;
    row.base_draw_index = double(bdi);
    row.family_id = char(fam.family_id);
    row.split_group_id = char(fam.split_group_id);
    row.stratum_id = st;
    row.stratum_index = double(si_plan);
    row.template_id = char(fam.template_id);
    row.regime_id = char(fam.regime_id);
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
    row.ood_axis = 'NONE';
    row.ood_condition_id = '';
    row.ood_token_id = '';
end

%% =========================== summary ================================

function sa = local_stratum_summary(plan, st, si_plan, draws, ...
        slot_fails, attempts_st, voy_c, samp_c)
    rows_st = {};
    for j = 1:numel(voy_c)
        r = voy_c{j};
        if strcmp(char(r.stratum_id), st)
            rows_st{end+1} = r; %#ok<AGROW>
        end
    end
    n_acc = 0;
    n_rej = 0;
    n_fail = 0;
    rej_reasons = struct();
    n_windows_min = inf;
    for j = 1:numel(rows_st)
        r = rows_st{j};
        d = char(r.disposition);
        if strcmp(d, 'ACCEPT')
            n_acc = n_acc + 1;
        elseif strncmp(d, 'REJECTED', 8)
            n_rej = n_rej + 1;
            rej_reasons = local_bump(rej_reasons, char(r.reason));
        elseif ~isempty(d)
            n_fail = n_fail + 1;
            rej_reasons = local_bump(rej_reasons, d);
        end
        if isfield(r, 'duration_s') && isfinite(double(r.duration_s))
            w = mission_v3.eligibility('windows', ...
                double(r.duration_s));
            n_windows_min = min(n_windows_min, ...
                double(w.expected_canonical_windows));
        end
    end
    if isinf(n_windows_min)
        n_windows_min = 0;
    end
    tok_counts = struct();
    for j = 1:numel(samp_c)
        s = samp_c{j};
        if strcmp(char(s.stratum_id), st) ...
                && strncmp(char(s.disposition), ...
                'REJECTED_STRUCTURAL_HELDOUT', 28)
            tok_counts = local_bump(tok_counts, char(s.token_id));
        end
    end

    sa = struct();
    sa.stratum_id = st;
    sa.stratum_index = double(si_plan);
    sa.slots_planned = double(draws);
    sa.accepted_draws = double(draws - slot_fails);
    sa.slots_failed = double(slot_fails);
    sa.env_attempts_total = double(attempts_st);
    sa.accepted_voyages = double(n_acc);
    sa.rejected_voyages = double(n_rej);
    sa.failed_voyages = double(n_fail);
    sa.min_canonical_windows_over_members = double(n_windows_min);
    sa.rejected_reason_counts = local_nonempty(rej_reasons);
    sa.token_heldout_counts = local_nonempty(tok_counts);
    sa.rejection_rate = local_rate(attempts_st, draws - slot_fails);
    if local_rate_gt(sa.rejection_rate, plan.iid.thr_stratum)
        sa.verdict = 'FAIL';
    else
        sa.verdict = 'PASS';
    end
end

function n = local_worker_cap()
% Memory-bound parallel width: 8 GB machine - each pool worker holds a
% full Simulink model instance, so the default cap is 3 (override with
% FLEET_WORKERS; run_tasks serial-falls-back when PCT is unavailable).
    v = getenv('FLEET_WORKERS');
    if isempty(v)
        n = 3;
        return;
    end
    n = max(1, min(8, str2double(v)));
    if isnan(n)
        n = 3;
    end
end

function n = local_n_simulated(voy_c)
    n = 0;
    for j = 1:numel(voy_c)
        if ~strcmp(char(voy_c{j}.disposition), 'FAILED_GENERATION')
            n = n + 1;
        end
    end
end

%% ============================ helpers ===============================

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

function local_write_json(path, obj)
    txt = jsonencode(obj, 'PrettyPrint', true);
    tmp = [path '.tmp'];
    fid = fopen(tmp, 'w');
    if fid < 0
        error('GATE:FLEETV3:IID:IO', ...
            'cannot open %s for writing', tmp);
    end
    fwrite(fid, txt, 'char');
    fclose(fid);
    movefile(tmp, path, 'f');
end
