function out = task_ood(plan, ctx, cid, art_dir, draw_base, seed)
% fleet_v3.task_ood - one OOD condition's directed generation as an
% independent parallel task (T-018 Phase 4-D; D5 six-segment pipeline).
%
%   out = fleet_v3.task_ood(plan, ctx, cid, art_dir, draw_base, seed)
%
% Extracted verbatim from the gen_ood per-condition body: conditions are
% independent in the serial loop too (each condition has its own frozen
% seed segment starting at draw_base), so per-condition task splitting is
% semantically exact.  The per-candidate STRESS ledger is written to a
% per-condition file `_ood_stress_ledger_<cid>.json` (parallel tasks must
% not share a ledger path); the gen_ood collector merges them into the
% combined `_ood_stress_ledger.json` afterwards.  Runs on pool workers
% (PCT) or serially; both faces call fleet_v3.worker_provision first.
%
% out fields: condition_id, ood_axis, token_id, host_template,
% host_regime, attempts_used, accepted, reject_guard_band,
% reject_no_target, stress_set_removed, accepted_voyages, voyages (cell),
% retry (cell), stress_rows (cell), sup_abs, ht_effective.
%
% Error ids: GATE:FLEETV3:OOD:<REASON> (unchanged from gen_ood).
%
% Discipline: ASCII-only source; no absolute paths; no chained indexed
% assignment.

    addpath(fullfile(plan.modele_root, 'scripts'));
    fleet_v3.worker_provision(plan.modele_root, ctx.MDL);

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

%% ================== extracted from gen_ood (verbatim) ================

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
                retry_rows{end+1} = rr; %#ok<AGROW>
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

function p = local_prefix(plan)
    if strcmp(char(plan.mode), 'smoke')
        p = 'FLTS';
    else
        p = 'FLTO';
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
