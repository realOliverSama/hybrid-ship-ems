function out = allocate_draws(plan, ctx, face, varargin)
% fleet_v3.allocate_draws - AMENDMENT_03 Phase S: deterministic
% condition/stratum-level draw allocation (NO Model-E simulation).
%
%   out = fleet_v3.allocate_draws(plan, ctx, 'iid')
%   out = fleet_v3.allocate_draws(plan, ctx, 'ood')
%
% T432_ACTIVATION_AMENDMENT_03 (Final Ruling 2026-09-09, option A'):
% retires the per-family contiguous candidate-window ownership.  Each
% stratum/condition consumes its frozen 1000-index candidate block in
% canonical ascending order:
%
%   for every candidate base_draw_index (frozen seed/lineage derivation):
%     1. realize the normal candidate (env_realize single-candidate face)
%     2. apply every frozen rule (env ledger / ood_judge support-token-
%        physical-eligibility dispositions)
%     3. persist the disposition in the consumed-candidate ledger
%     4. retry FAILED_* only under the existing same-seed retry rule
%     5. advance after REJECTED_*
%   until the exact preregistered family quota is filled.
%
% UNREACHABLE is declared only when the entire frozen block is exhausted
% without filling the quota (a poor local sub-window is never family-
% level UNREACHABLE evidence).  Selection is independent of worker count
% and completion order: this allocator is single-threaded canonical
% order, the frozen plan drives every pass, and pool workers only
% simulate the already accepted base draws (Phase R).
%
% Scientific contracts are consumed, never restated: support bounds,
% token REQUIRE rules, physical-domain guards and eligibility live in
% mission_v3.env_realize / fleet_v3.ood_judge exactly as in the
% pre-amendment chain.  Rejection gates (per-stratum/overall <= 0.50)
% are evaluated by the caller (gen_iid/gen_ood) from the consumed
% ledger, unchanged.
%
% out fields (also written to <art_dir>/formal_draw_allocation_plan_<face>.json
% when out.art_dir is set by the caller via 'ArtDir'):
%   schema, corpus, seed_root, face, frozen_at,
%   quota, blocks(9x or 5x struct array), consumed, self_hash
%
% Errors: GATE:FLEETV3:ALLOC:* (fail-closed; UNREACHABLE only when the
% complete frozen block cannot fill the quota).

    if nargin < 4
        varargin = {};
    end
    art_dir = '';
    cond_sel = {};
    str_sel = {};
    for ai = 1:2:numel(varargin)
        if strcmpi(varargin{ai}, 'ArtDir')
            art_dir = char(varargin{ai + 1});
        elseif strcmpi(varargin{ai}, 'Conditions')
            cond_sel = cellstr(varargin{ai + 1});
        elseif strcmpi(varargin{ai}, 'Strata')
            str_sel = cellstr(varargin{ai + 1});
        end
    end

    mode = char(plan.mode);
    seed = char(plan.iid.seed);   % same formal seed root (IID/OOD)
    switch lower(face)
        case 'iid'
            corpus_face = 'iid';
        case 'ood'
            corpus_face = 'ood';
        otherwise
            error('GATE:FLEETV3:ALLOC:ARG', ...
                'unknown face: %s', face);
    end

    seg = face;   % alias (legacy naming inside this file)
    is_stage_b = strncmp(mode, 'stage_b', 7);
    out = struct();
    out.schema = 'fleet_v3_draw_allocation_plan_v1';
    out.corpus = mode;
    out.face = corpus_face;
    out.seed_root = char(plan.iid.seed_formal);
    out.frozen_at = char(datetime('now', 'Format', 'yyyy-MM-dd'));
    out.amendment = 'T432_ACTIVATION_AMENDMENT_03';
    out.allocator = 'canonical ascending consumption; worker-count invariant';
    out.self_hash = '';

    consumed_all = {};
    blocks = {};
    total_attempts = 0;
    total_accepted = 0;

    if strcmp(corpus_face, 'iid')
        out.quota_per_stratum = double(plan.iid.draws);
        if is_stage_b
            out.quota_per_stratum = 16;   % formal families per stratum
        end
        n_str = numel(plan.iid.strata);
        if ~isempty(str_sel)
            keep = [];
            for k = 1:numel(str_sel)
                hit = find(strcmp(cellstr(plan.iid.strata), str_sel{k}), 1);
                if isempty(hit)
                    error('GATE:FLEETV3:ALLOC:ARG', ...
                        'unknown stratum: %s', str_sel{k});
                end
                keep = [keep, hit]; %#ok<AGROW>
            end
            order = 1:n_str;
            n_str = numel(keep);
            order = order(keep);
        else
            order = 1:n_str;
        end
        stride = 1000;                     % frozen candidate block width
        blocks{end + 1} = [];
        out.blocks = struct([]);
        strata_cells = {};
        % OOD quota for stage_b comes from plan.ood.families_formal; IID
        % stage_b quota is the frozen 16 families per stratum.
        for si = order
            st = char(plan.iid.strata{si});
            tpl = local_stratum_template(st);
            reg = local_stratum_regime(st);
            if is_stage_b
                blk_start = (si - 1) * stride;
            else
                blk_start = double(plan.iid.draw_base);
            end
            [slots, consumed, attempts_st] = local_consume_iid( ...
                plan, seed, blk_start, stride, tpl, reg, st, ...
                out.quota_per_stratum, is_stage_b);
            total_attempts = total_attempts + attempts_st;
            total_accepted = total_accepted + numel(slots);
            consumed_all = [consumed_all, consumed]; %#ok<AGROW>
            sa = struct();
            sa.stratum_id = st;
            sa.block_start = blk_start;
            if is_stage_b
                sa.block_end = blk_start + stride - 1;
            else
                sa.block_end = blk_start + out.quota_per_stratum * 50 - 1;
            end
            sa.quota = out.quota_per_stratum;
            rej_n = 0;
            for cc = 1:numel(consumed)
                if ~strcmp(consumed{cc}.disposition, 'ACCEPTED')
                    rej_n = rej_n + 1;
                end
            end
            sa.rejection_rate = double(rej_n) / max(1, numel(consumed));
            sa.accepted_slots = numel(slots);
            sa.slots = slots;
            sa.consumed = consumed;
            sa.attempts = attempts_st;
            if numel(slots) < out.quota_per_stratum
                sa.verdict = 'UNREACHABLE';
                error('GATE:FLEETV3:ALLOC:UNREACHABLE', ...
                    ['stratum %s consumed its whole frozen block without ', ...
                     'filling %d family slots (consumed %d candidates, ', ...
                     '%d attempts); see the consumed ledger - escalate, ', ...
                     'do not widen'], st, out.quota_per_stratum, ...
                    numel(consumed), attempts_st);
            else
                sa.verdict = 'FILLED';
            end
            blocks{end + 1} = sa; %#ok<AGROW>
            strata_cells{end + 1} = sa; %#ok<AGROW>
        end
        out.iid.strata = [strata_cells{:}];
        out.blocks = blocks;
    else
        % OOD face: per frozen condition block, canonical ascending
        % consumption judged by fleet_v3.ood_judge.
        out.quota_per_condition = 1;
        if is_stage_b
            out.quota_per_condition = double(plan.ood.families_formal);
        end
        conds = cellstr(plan.ood.conditions);
        if ~isempty(cond_sel)
            keep = [];
            for k = 1:numel(cond_sel)
                hit = find(strcmp(conds, cond_sel{k}), 1);
                if isempty(hit)
                    error('GATE:FLEETV3:ALLOC:ARG', ...
                        'unknown condition: %s', cond_sel{k});
                end
                keep = [keep, hit]; %#ok<AGROW>
            end
            order = 1:numel(conds);
            order = order(keep);
        else
            order = 1:numel(conds);
        end
        n_cond = numel(conds);
        stride = 1000;
        cond_cells = {};
        for ci = order
            cid = char(conds{ci});
            [host, ov] = local_ood_host(plan, cid);
            if is_stage_b
                blk_start = 10000 + (ci - 1) * stride;
            else
                blk_start = double(plan.ood.draw_base);
            end
            [slots, consumed, attempts_st, judge_acc] = local_consume_ood( ...
                plan, ctx, cid, host, ov, seed, blk_start, stride, ...
                out.quota_per_condition, is_stage_b);
            total_attempts = total_attempts + attempts_st;
            total_accepted = total_accepted + numel(slots);
            consumed_all = [consumed_all, consumed]; %#ok<AGROW>
            sa = struct();
            sa.condition_id = cid;
            sa.block_start = blk_start;
            if is_stage_b
                sa.block_end = blk_start + stride - 1;
            else
                sa.block_end = blk_start + out.quota_per_condition * 50 - 1;
            end
            sa.quota = out.quota_per_condition;
            sa.accepted_slots = numel(slots);
            sa.slots = slots;
            sa.consumed = consumed;
            sa.attempts = attempts_st;
            sa.judge_accepted = judge_acc;
            if numel(slots) < out.quota_per_condition
                sa.verdict = 'UNREACHABLE';
                error('GATE:FLEETV3:ALLOC:UNREACHABLE', ...
                    ['condition %s consumed its whole frozen block ', ...
                     'without filling %d family slots (consumed %d ', ...
                     'candidates, %d attempts; guard/no-target/stress ', ...
                     'dispositions are in the consumed ledger); ', ...
                     'escalate, do not widen'], cid, ...
                    out.quota_per_condition, numel(consumed), attempts_st);
            else
                sa.verdict = 'FILLED';
            end
            blocks{end + 1} = sa; %#ok<AGROW>
            cond_cells{end + 1} = sa; %#ok<AGROW>
        end
        out.ood.conditions = [cond_cells{:}];
        out.blocks = blocks;
    end

    out.total_attempts = total_attempts;
    out.consumed = consumed_all;

    % ---- AMENDMENT_04 separate reporting (IID and OOD never merge) ----
    if strcmp(corpus_face, 'iid')
        overall_rate = local_rate(total_attempts, total_accepted);
        ps_rates = cell(1, numel(out.iid.strata));
        ps_pass = true;
        for bi = 1:numel(out.iid.strata)
            r_st = out.iid.strata(bi).rejection_rate;
            ps_rates{bi} = struct('stratum_id', ...
                out.iid.strata(bi).stratum_id, 'rate', r_st);
            if local_rate_gt(r_st, plan.iid.thr_stratum)
                ps_pass = false;
            end
        end
        overall_pass = ~local_rate_gt(overall_rate, plan.iid.thr_overall);
        out.report = struct();
        out.report.iid_rejection_gate = struct( ...
            'threshold', double(plan.iid.thr_overall), ...
            'per_stratum_threshold', double(plan.iid.thr_stratum), ...
            'per_stratum', ps_rates, ...
            'overall_rate', overall_rate, ...
            'pass', logical(overall_pass && ps_pass));
    else
        cond_eff = cell(1, numel(out.ood.conditions));
        g_pass = true;
        for bi = 1:numel(out.ood.conditions)
            sa = out.ood.conditions(bi);
            br = struct(); %#ok<NASGU>
            acc_n = 0; rej_n = 0;
            disp_counts = struct('ACCEPT', 0, 'STRESS_SET', 0, ...
                'REJECT_GUARD_BAND', 0, 'REJECT_NO_TARGET', 0, ...
                'FAILED_GENERATION', 0);
            for cc = 1:numel(sa.consumed)
                dname = sa.consumed{cc}.disposition;
                if isfield(disp_counts, dname)
                    disp_counts.(dname) = disp_counts.(dname) + 1;
                end
                if strcmp(dname, 'ACCEPT'); acc_n = acc_n + 1;
                else; rej_n = rej_n + 1; end
            end
            filled = acc_n;
            exhausted = (sa.attempts > 0) && (filled < sa.quota) && ...
                (cursor_report_end(sa) <= sa.block_end);
            cond_eff{bi} = struct('condition_id', sa.condition_id, ...
                'consumed', numel(sa.consumed), 'accepted', filled, ...
                'rejected', rej_n, ...
                'proposal_yield', double(filled) / max(1, numel(sa.consumed)), ...
                'disposition_counts', disp_counts);
            if filled < sa.quota; g_pass = false; end
        end
        out.report = struct();
        out.report.ood_proposal_efficiency = cond_eff;   % diagnostic only
        out.report.ood_quota_reachability_gate = struct( ...
            'quota_required', double(out.quota_per_condition), ...
            'pass', logical(g_pass));
    end
    % self_hash: sha256 over the canonical field-sorted JSON minus itself
    body = rmfield(out, 'self_hash');
    txt = jsonencode(body);
    out.self_hash = local_sha256(txt);

    if ~isempty(art_dir)
        if ~exist(art_dir, 'dir')
            mkdir(art_dir);
        end
        pf = fullfile(art_dir, ...
            ['formal_draw_allocation_plan_' corpus_face '.json']);
        fid = fopen(pf, 'w');
        if fid < 0
            error('GATE:FLEETV3:ALLOC:WRITE', ...
                'cannot write allocation plan: %s', pf);
        end
        fwrite(fid, jsonencode(out, 'PrettyPrint', true));
        fclose(fid);
        out.plan_file = pf;
    end
end

% ---------------------------------------------------------------------
function [slots, consumed, attempts_st] = local_consume_iid( ...
    plan, seed, blk_start, stride, tpl, reg, st, quota, is_stage_b)
% Canonical ascending IID consumption.  Each candidate is realized with
% MaxAttempts=1: one frozen candidate == one judged attempt (either the
% candidate is accepted as a family seed, or its disposition is recorded
% and the cursor advances).  FAILED_* retries the same candidate once
% under the existing same-seed rule, then records FAILED_GENERATION and
% advances (a permanent candidate-level failure must not stall the
% canonical order).
    slots = {};
    consumed = {};
    attempts_st = 0;
    cursor = double(blk_start);
    if is_stage_b
        block_end = double(blk_start) + stride - 1;
    else
        block_end = double(blk_start) + quota * 50 - 1;
    end
    while numel(slots) < quota && cursor <= block_end
        attempts_st = attempts_st + 1;
        [ok, samp] = local_try_sample(plan, seed, cursor, tpl, reg, st);
        if ok
            slots{end + 1} = struct('slot', numel(slots) + 1, ...
                'base_draw_index', double(cursor), ...
                'n_attempts_env', double(samp.n_attempts), ...
                'family_seed', seed);
            consumed{end + 1} = struct('bdi', double(cursor), ...
                'disposition', 'ACCEPTED', 'reason', 'none'); %#ok<AGROW>
            cursor = cursor + 1;
        else
            disp = local_first_disposition(samp);
            % same-seed retry once (existing retry rule), then record and
            % advance: the candidate identity is deterministic, so a
            % second identical realization either succeeds or the
            % candidate is recorded FAILED_GENERATION.
            [ok2, samp2] = local_try_sample(plan, seed, cursor, ...
                tpl, reg, st);
            attempts_st = attempts_st + 1;
            if ok2
                slots{end + 1} = struct('slot', numel(slots) + 1, ...
                    'base_draw_index', double(cursor), ...
                    'n_attempts_env', double(samp2.n_attempts), ...
                    'family_seed', seed);
                consumed{end + 1} = struct('bdi', double(cursor), ...
                    'disposition', 'ACCEPTED', 'reason', 'retry');
                cursor = cursor + 1;
            else
                fail_reason = local_first_disposition(samp2);
                if isfield(samp2, 'err_id')
                    fail_reason = samp2.err_id;
                end
                consumed{end + 1} = struct('bdi', double(cursor), ...
                    'disposition', 'FAILED_GENERATION', ...
                    'reason', fail_reason); %#ok<AGROW>
                cursor = cursor + 1;
            end
            clear disp samp samp2 ok ok2
        end
    end
end

% ---------------------------------------------------------------------
function [slots, consumed, attempts_st, judge_acc] = local_consume_ood( ...
    plan, ctx, cid, host, ov, seed, blk_start, stride, quota, is_stage_b)
% Canonical ascending OOD consumption: dag_sample -> env_realize ->
% fleet_v3.ood_judge, exactly the pre-amendment judgement chain (the
% allocator simply stops the cursor-advance at ACCEPT and freezes the
% plan; Phase R replays the identical chain per accepted draw).
    slots = {};
    consumed = {};
    attempts_st = 0;
    judge_acc = [];
    cursor = double(blk_start);
    if is_stage_b
        block_end = double(blk_start) + stride - 1;
    else
        block_end = double(blk_start) + quota * 50 - 1;
    end
    while numel(slots) < quota && cursor <= block_end
        attempts_st = attempts_st + 1;
        dag = mission_v3.dag_sample('draw', seed, cursor, ...
            char(host.template), char(host.regime), ...
            char(host.stratum), 'ParamOverride', ov);
        rl = mission_v3.env_realize('realize', dag, 'auto', ...
            'ParamOverride', ov);
        judge = fleet_v3.ood_judge(plan, cid, rl);
        disp_i = char(judge.disposition);
        consumed{end + 1} = struct('bdi', double(cursor), ...
            'disposition', disp_i, ...
            'reason', char(judge.reason)); %#ok<AGROW>
        switch disp_i
            case 'ACCEPT'
                slots{end + 1} = struct('slot', numel(slots) + 1, ...
                    'base_draw_index', double(cursor), ...
                    'ood_axis', char(judge.ood_axis), ...
                    'token_id', char(plan.ood.hosts.(cid).token_id), ...
                    'family_seed', seed);
                judge_acc = judge;
            case {'STRESS_SET', 'REJECT_GUARD_BAND', 'REJECT_NO_TARGET'}
                % advance (disposition already persisted)
            otherwise
                error('GATE:FLEETV3:ALLOC:DISPOSITION', ...
                    'unknown ood_judge disposition: %s', disp_i);
        end
        cursor = cursor + 1;
    end
end

% ---------------------------------------------------------------------
function [ok, samp] = local_try_sample(plan, seed, cursor, tpl, reg, st)
% Single-candidate env realization (MaxAttempts=1): the candidate is
% either accepted as a family seed or rejected with its frozen ledger
% disposition.  Any non-MAX_RETRY error is rethrown (contract errors
% must not be rebranded as rejections).
    ok = false;
    samp = struct('accepted', false, 'sample', [], ...
        'n_attempts', 1, 'new_rows', {{}});
    try
        s = mission_v3.env_realize('sample', seed, cursor, tpl, ...
            reg, st, 'MaxAttempts', 1);
        samp.n_attempts = double(s.n_attempts);
        if s.accepted
            ok = true;
            samp.accepted = true;
            samp.sample = s;
        else
            samp.new_rows = s.ledger;
        end
    catch ME
        % AMENDMENT_03 hotfix: catch ALL env-layer exceptions (MAX_RETRY
        % and any other) as FAILED_GENERATION with the identifier kept in
        % reason - a hard rethrow here crashes the whole canonical
        % allocation and stalls Stage B (observed 2026-09-10).  Contract
        % errors remain visible in the consumed ledger for audit.
        ok = false;
        samp.new_rows = {{}};
        samp.err_id = local_err_id(ME);
        samp.err_msg = char(ME.message);
    end
end

% ---------------------------------------------------------------------
function d = local_first_disposition(samp)
% Best-effort disposition of a rejected single-candidate attempt (from
% the env ledger when present).
    d = 'REJECTED';
    if isfield(samp, 'new_rows') && ~isempty(samp.new_rows)
        rows = samp.new_rows;
        if iscell(rows)
            r = rows{end};
        else
            r = rows(end);
        end
        if isstruct(r) && isfield(r, 'disposition')
            d = char(r.disposition);
        end
    end
end

% ---------------------------------------------------------------------
function id = local_err_id(ME)
    id = '';
    if isfield(ME, 'identifier')
        id = char(ME.identifier);
    end
end

% ---------------------------------------------------------------------
function t = local_stratum_template(st)
    parts = strsplit(st, '__');
    t = strrep(char(parts{1}), 'STRAT_', '');
end

function r = local_stratum_regime(st)
    parts = strsplit(st, '__');
    r = char(parts{2});
end

% ---------------------------------------------------------------------
function [host, ov] = local_ood_host(plan, cid)
% Frozen OOD host + proposal override - consumed verbatim from the
% pre-amendment gen_ood face (probe_t27 validated this chain).
    host = plan.ood.hosts.(cid);
    ov = plan.ood.proposal.(cid).override;
end

% ---------------------------------------------------------------------
function h = local_sha256(txt)
    h = java.security.MessageDigest.getInstance('SHA-256').digest( ...
        uint8(txt));
    h = lower(reshape(dec2hex(h), 1, []));
end

function e = cursor_report_end(sa)
% Block-end echo helper for the reachability report.
    e = sa.block_end;
    e = sa.block_end;
end

function r = local_rate(att, acc)
% Rejection rate = (attempts - accepted) / attempts (AMENDMENT_04: the
% numeric definition is consumed by the IID face gate report; the OOD
% face reports proposal efficiency as a diagnostic only).
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
