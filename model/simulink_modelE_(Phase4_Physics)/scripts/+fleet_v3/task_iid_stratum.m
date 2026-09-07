function out = task_iid_stratum(plan, ctx, st, si_plan, draws, ...
        draw_base, seed, art_dir)
% fleet_v3.task_iid_stratum - one stratum's IID generation as an
% independent parallel task (T-018 Phase 4-D).
%
%   out = fleet_v3.task_iid_stratum(plan, ctx, st, si_plan, draws, ...
%                                   draw_base, seed, art_dir)
%
% Extracted verbatim from the gen_iid per-stratum body: strata are
% independent in the serial loop too (each stratum starts its own draw
% segment at draw_base and does its own slot bookkeeping), so per-stratum
% task splitting is semantically exact - no reordering, no shared state.
% Runs on pool workers (PCT) or serially; both faces call
% fleet_v3.worker_provision first (idempotent per-worker provision).
%
% out fields: stratum_id, stratum_index, stratum_audit, voyages (cell),
% sampling (cell), retry (cell), attempts, draws_ok.
%
% Error ids: GATE:FLEETV3:IID:<REASON> (unchanged from gen_iid).
%
% Discipline: ASCII-only source; no absolute paths.

    addpath(fullfile(plan.modele_root, 'scripts'));
    fleet_v3.worker_provision(plan.modele_root, ctx.MDL);
    ctx.model_ready = true;   % worker_provision ran in THIS process

    parts = strsplit(st, '__');
    if numel(parts) ~= 2
        error('GATE:FLEETV3:IID:STRATUM', ...
            'stratum id not of the form <TPL>__<REG>: %s', st);
    end
    tpl = strrep(char(parts{1}), 'STRAT_', '');
    reg = char(parts{2});

    voy_c = {};
    samp_c = {};
    retry_c = {};
    draw_start = draw_base;
    slot_fails = 0;
    attempts_st = 0;

    for slot = 1:draws
        samp = local_sample_logged(plan, seed, draw_start, tpl, ...
            reg, st, slot);
        attempts_st = attempts_st + samp.n_attempts;
        for jj = 1:numel(samp.new_rows)
            samp_c{end+1} = samp.new_rows{jj}; %#ok<AGROW>
        end
        if ~samp.accepted
            slot_fails = slot_fails + 1;
            draw_start = draw_start + plan.iid.max_attempts;
            continue;
        end
        s2 = samp.sample;
        led = s2.ledger;
        acc_row = led(s2.n_attempts);
        dag = acc_row.draw;
        rl = s2.realization;
        bdi = dag.base_draw_index;
        draw_start = bdi + 1;

        fam = mission_v3.family_expand('expand', dag, 'Realization', rl);
        for k = 1:double(fam.n_members)
            [row, retry_c] = local_member_with_retry(plan, ctx, ...
                art_dir, si_plan, st, bdi, dag, rl, fam, k, retry_c);
            voy_c{end+1} = row; %#ok<AGROW>
        end
    end

    sa = local_stratum_summary(plan, st, si_plan, draws, slot_fails, ...
        attempts_st, voy_c, samp_c);

    out = struct();
    out.stratum_id = st;
    out.stratum_index = double(si_plan);
    out.stratum_audit = sa;
    out.voyages = voy_c;
    out.sampling = samp_c;
    out.retry = retry_c;
    out.attempts = double(attempts_st);
    out.draws_ok = double(draws - slot_fails);
end

%% ================== extracted from gen_iid (verbatim) ================

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
    if strcmp(char(plan.mode), 'smoke')
        p = 'FLTS';
    else
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

%% ============================ helpers ================================

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
