function out = gen_ood(plan, ctx)
% fleet_v3.gen_ood - OOD directed generation for the 73 pipeline
% (T-018 Phase 4-D; D5 six-segment pipeline per condition).
%
% Modes:
%   stage_a / smoke : 5 conditions x 1 family each (stage_a segment
%                     k=900 / smoke segment k=950)
%   stage_b_ood     : 5 conditions x 10 families each (formal segments
%                     ood.seed_base + c*seed_stride, family k gets the
%                     reserved attempt window [base+k*W, base+(k+1)*W),
%                     W = floor(seed_stride/families_formal)) - artifacts
%                     under datasets_v3/formal/ood (FR-497 Stage B)
%
% Tasks are per condition (stage_a/smoke) or per (condition, family)
% (stage_b) - independent by construction (per-task frozen seed
% segments), dispatched through fleet_v3.run_tasks (parallel PCT face or
% serial fallback) and merged in canonical task order.  The task body is
% the local function local_task below (passed to run_tasks via an
% anonymous handle; the handle resolves fleet_v3.gen_ood's local
% function on pool workers through the propagated package path).
%
% D5 six-segment pipeline per candidate: preregistered proposal sampler
% -> normal stochastic realization -> range/REQUIRE predicate ->
% physical-domain guard -> accept or reject-resample -> OOD
% provenance/meta.  Per-condition STRESS ledgers are written
% incrementally (FR-537: no candidate draw silently dropped) and merged
% into _ood_stress_ledger.json by the collector.
%
% out fields: corpus, mode, artifact_dir, seed, draw_base, conditions
% (struct array with judge/acceptance evidence), voyages, retry,
% stress_rows, n_stress_removed, run_wall_s.
%
% Error ids: GATE:FLEETV3:OOD:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; thin body + local
% functions; no chained indexed assignment.

    t_run = tic;
    draw_base = local_draw_base(plan);
    seed = char(plan.ood.seed);
    mode = char(plan.mode);

    if strcmp(mode, 'smoke')
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'qualification_smoke', 'ood');
    elseif strcmp(mode, 'stage_b_ood')
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'formal', 'ood');
    else
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'qualification', 'stage_a_ood');
    end
    if ~exist(art_dir, 'dir')
        mkdir(art_dir);
    end

    conds = plan.ood.conditions;
    task_fn = @(pb, c, a_dir, db, sd) local_task(pb, ctx, c, a_dir, ...
        db, sd);

    args_cell = cell(1, numel(conds));
    if strcmp(mode, 'stage_b_ood')
        % FR-497 Stage B formal: condition c families k=0..9 starting at
        % ood.seed_base + c*ood.seed_stride; each (condition, family) is
        % an independent task with a RESERVED attempt window
        % [base+k*W, base+(k+1)*W), W = floor(seed_stride/families_formal)
        W = floor(double(plan.ood.seed_stride) / ...
            double(plan.ood.families_formal));
        % AMENDMENT_03 two-phase workflow (Final Ruling 2026-09-09):
        % Phase S freezes the canonical-order allocation plan; Phase R
        % workers simulate only accepted draws (targeted rejection
        % sampling; candidate rejection rate is proposal-efficiency
        % diagnostic per AMENDMENT_04, never a population gate).
        plan_file = fullfile(art_dir, ...
            'formal_draw_allocation_plan_ood.json');
        if exist(plan_file, 'file')
            alloc = jsondecode(fileread(plan_file));
            fprintf('[fleet_v3] OOD: consuming frozen allocation plan\n');
        else
            alloc = fleet_v3.allocate_draws(plan, ctx, 'ood', ...
                'ArtDir', art_dir);
            fprintf(['[fleet_v3] OOD: allocation plan frozen ', ...
                '(%d consumed candidates, self_hash %.16s)\n'], ...
                numel(alloc.consumed), alloc.self_hash);
        end
        n_tasks = 0;
        for c_i = 1:numel(alloc.ood.conditions)
            sa = alloc.ood.conditions(c_i);
            cid = char(sa.condition_id);
            pb = plan;
            pb.ood.max_attempts = 1;   % Phase R: one candidate per draw
            slots_arr = sa.slots;
            if iscell(slots_arr)
                slots_arr = slots_arr(:);
            end
            for k = 1:numel(slots_arr)
                e = slots_arr(k);
                if isstruct(e), e = e(1); end
                bdi = double(e.base_draw_index);
                n_tasks = n_tasks + 1;
                args_cell{n_tasks} = {pb, cid, art_dir, bdi};
            end
        end
        args_cell = args_cell(1:n_tasks);
        task_fn = @(pb, c, a_dir, db) local_task(pb, ctx, c, a_dir, ...
            db, seed);
    else
        % stage_a/smoke serial face: one task per condition at draw_base.
        % (AMENDMENT_02 requalification fix: dac35051 left `cid(...)` here -
        % a function-call typo of the parallel branch's local char variable;
        % this face was never executed between that commit and today.)
        for c = 1:numel(conds)
            args_cell{c} = {plan, char(conds{c}), art_dir, ...
                draw_base};
        end
        task_fn = @(pb, c, a_dir, db) local_task(pb, ctx, c, a_dir, ...
            db, seed);
    end

    outs = fleet_v3.run_tasks(task_fn, args_cell, ...
        local_worker_cap(), 'OOD');

    voy_c = {};
    ood_retry_c = {};
    stress_rows = {};
    cond_audit = struct([]);
    for t = 1:numel(outs)
        o = outs{t};
        ca = struct();
        ca.condition_id = o.condition_id;
        ca.ood_axis = o.ood_axis;
        ca.token_id = o.token_id;
        ca.host_template = o.host_template;
        ca.host_regime = o.host_regime;
        ca.attempts_used = o.attempts_used;
        ca.accepted = o.accepted;
        ca.reject_guard_band = o.reject_guard_band;
        ca.reject_no_target = o.reject_no_target;
        ca.stress_set_removed = o.stress_set_removed;
        ca.accepted_voyages = o.accepted_voyages;
        ca.in_band_hit_rate = double(o.attempts_used == 1);
        ca.require_reachable = true;
        ca.sup_abs = o.sup_abs;
        ca.ht_effective = o.ht_effective;
        if isempty(cond_audit)
            cond_audit = ca;
        else
            cond_audit(end+1) = ca; %#ok<AGROW>
        end
        voy_c = [voy_c, o.voyages]; %#ok<AGROW>
        ood_retry_c = [ood_retry_c, o.retry]; %#ok<AGROW>
        stress_rows = [stress_rows, o.stress_rows]; %#ok<AGROW>
    end
    % combined stress ledger (per-condition task-local files remain
    % alongside for parallel-write safety)
    if ~isempty(stress_rows)
        local_write_json(fullfile(art_dir, ...
            '_ood_stress_ledger.json'), ...
            struct('n_removed', {double(numel(stress_rows))}, ...
            'rows', {stress_rows}));
    end

    out = struct();
    out.corpus = 'OOD';
    out.mode = mode;
    out.artifact_dir = art_dir;
    out.seed = seed;
    out.draw_base = draw_base;
    out.conditions = cond_audit;
    out.voyages = voy_c;
    out.retry = ood_retry_c;
    out.stress_rows = stress_rows;
    out.n_stress_removed = double(numel(stress_rows));
    out.run_wall_s = toc(t_run);

    local_write_json(fullfile(art_dir, '_ood_ledger.json'), ...
        struct('corpus', {out.corpus}, 'mode', {out.mode}, ...
        'seed', {out.seed}, 'voyages', {voy_c}, ...
        'stress_rows', {stress_rows}, 'retry', {ood_retry_c}));
end

%% ==================== per-condition task body ========================

function out = local_task(plan, ctx, cid, art_dir, draw_base, seed)
% One condition's directed generation (judge loop until accept, then the
% 4-member chain).  plan.ood.max_attempts is the per-task attempt
% budget (200 for stage_a/smoke; the reserved attempt window size for
% Stage B family tasks).
    addpath(fullfile(plan.modele_root, 'scripts'));
    fleet_v3.worker_provision(plan.modele_root, ctx.MDL);
    ctx.model_ready = true;   % worker_provision ran in THIS process

    host = plan.ood.hosts.(cid);
    ov = plan.ood.proposal.(cid).override;
    max_att = double(plan.ood.max_attempts);

    attempts = 0;
    n_reject_guard = 0;
    n_reject_target = 0;
    n_stress = 0;
    accepted = false;
    dag = [];
    rl = [];
    judge = [];
    stress_rows = {};

    for att = 1:max_att
        bdi = draw_base + (att - 1);
        dag = mission_v3.dag_sample('draw', seed, bdi, ...
            char(host.template), char(host.regime), ...
            char(host.stratum), 'ParamOverride', ov);
        rl = mission_v3.env_realize('realize', dag, 'auto', ...
            'ParamOverride', ov);
        judge = fleet_v3.ood_judge(plan, cid, rl);
        attempts = att;
        switch char(judge.disposition)
            case 'STRESS_SET'
                n_stress = n_stress + 1;
                stress_rows{end+1} = local_stress_row(plan, cid, ...
                    bdi, judge); %#ok<AGROW>
                % FR-537 audit precedent: persist removal rows
                % incrementally (per-condition file: parallel tasks must
                % not share a ledger path)
                local_write_json(fullfile(art_dir, ...
                    ['_ood_stress_ledger_' cid '.json']), ...
                    struct('condition', {cid}, 'n_removed', ...
                    {double(n_stress)}, 'rows', {stress_rows}));
                % D5: a domain violation is a removal row, never an OOD
                % sample; keep resampling the segment
            case 'REJECT_GUARD_BAND'
                n_reject_guard = n_reject_guard + 1;
            case 'REJECT_NO_TARGET'
                n_reject_target = n_reject_target + 1;
            case 'ACCEPT'
                accepted = true;
                break;
            otherwise
                error('GATE:FLEETV3:OOD:DISPOSITION', ...
                    'unknown judge disposition: %s', ...
                    char(judge.disposition));
        end
    end
    if ~accepted
        error('GATE:FLEETV3:OOD:UNREACHABLE', ...
            ['condition %s did not reach an accepted candidate within ', ...
             '%d attempts (%d guard-band rejects, %d no-target ', ...
             'rejects, %d STRESS_SET removals); bands, predicates and ', ...
             'perturbation magnitudes are frozen - escalate, do not ', ...
             'widen'], cid, max_att, n_reject_guard, n_reject_target, ...
            n_stress);
    end

    fam = mission_v3.family_expand('expand', dag, 'Realization', rl);
    bdi = dag.base_draw_index;
    voy_c = {};
    retry_c = {};
    n_members = double(fam.n_members);
    n_acc_member = 0;
    for k = 1:n_members
        meta_extra = local_meta_extra(plan, cid, judge);
        voyage_id = sprintf('%s_OOD_%s_D%05d_M%d', ...
            local_prefix(plan), cid, bdi, k);
        [row, retry_c] = local_member_with_retry(plan, ctx, art_dir, ...
            voyage_id, dag, rl, fam, k, meta_extra, retry_c);
        voy_c{end+1} = row; %#ok<AGROW>
        if strcmp(char(row.disposition), 'ACCEPT')
            n_acc_member = n_acc_member + 1;
        end
    end

    out = struct();
    out.condition_id = cid;
    out.ood_axis = char(judge.ood_axis);
    out.token_id = char(plan.ood.hosts.(cid).token_id);
    out.host_template = char(host.template);
    out.host_regime = char(host.regime);
    out.attempts_used = double(attempts);
    out.accepted = true;
    out.reject_guard_band = double(n_reject_guard);
    out.reject_no_target = double(n_reject_target);
    out.stress_set_removed = double(n_stress);
    out.accepted_voyages = double(n_acc_member);
    out.sup_abs = double(max(abs(rl.u_river_mps(:))));
    out.ht_effective = double(rl.water.ht_effective);
    out.voyages = voy_c;
    out.retry = retry_c;
    out.stress_rows = stress_rows;
end

function cid = cid(s)
    cid = char(s);
end

%% ========================== local helpers ===========================

function me = local_meta_extra(plan, cid, judge)
    me = struct();
    me.ood_final_domain = 'OOD';
    me.ood_axis = char(judge.ood_axis);
    me.ood_condition_id = char(judge.ood_condition_id);
    me.ood_token_id = char(judge.ood_token_id);
    me.heldout_token_registry_sha = char(plan.ood.registry_sha);
    me.ood_judge_reason = char(judge.reason);
    me.propulsion = judge.evidence.propulsion;
end

function [row, retry_c] = local_member_with_retry(plan, ctx, art_dir, ...
        voyage_id, dag, rl, fam, k, meta_extra, retry_c)
    retry_rows = {};
    n_try = double(plan.iid.retry_max) + 1;
    row = [];
    for att = 1:n_try
        t_m = tic;
        try
            row = fleet_v3.member_exec(ctx, voyage_id, dag, rl, fam, ...
                k, meta_extra);
            row.member_attempt = double(att);
            row.member_wall_s = toc(t_m);
            row.retry_of = double(max(0, att - 1));
            row.artifact_dir = art_dir;
            break;
        catch ME
            local_reraise_hard(ME);
            if att >= n_try
                row = local_blank_row(voyage_id, dag, fam, k);
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
                rr.stratum_id = char(dag.stratum_id);
                rr.voyage_id = voyage_id;
                retry_c{end+1} = rr; %#ok<AGROW>
            end
        end
    end
    retry_c = retry_rows;
end

function row = local_blank_row(voyage_id, dag, fam, k)
    mem = fam.members(k);
    arms_v = mem.arms;
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
    row.ood_axis = '';
    row.ood_condition_id = '';
    row.ood_token_id = '';
end

function r = local_stress_row(plan, cid, bdi, judge)
    r = struct();
    r.record_kind = 'STRESS_SET_REMOVAL';
    r.condition_id = cid;
    r.def_version = char(plan.ood.def_version);
    r.base_draw_index = double(bdi);
    r.disposition = 'STRESS_SET';
    r.reason = char(judge.reason);
    r.sup_abs = double(judge.evidence.sup_abs);
    r.ht_effective = double(judge.evidence.ht_effective);
    r.flow_axis = char(judge.evidence.flow_axis);
    r.note = ['physical-domain violation (D5): removed candidate, ', ...
        'never an Axis-A/B/C sample'];
end

function db = local_draw_base(plan)
    if strcmp(char(plan.mode), 'smoke')
        db = double(plan.smoke.draw_base);
    else
        db = double(plan.ood.draw_base);   % stage_a segment k=900
    end
end

function p = local_prefix(plan)
    switch char(plan.mode)
        case 'smoke'
            p = 'FLTS';
        case 'stage_b_ood'
            p = 'FLTB';
        otherwise
            p = 'FLTO';
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
        error('GATE:FLEETV3:OOD:IO', ...
            'cannot open %s for writing', tmp);
    end
    fwrite(fid, txt, 'char');
    fclose(fid);
    movefile(tmp, path, 'f');
end
