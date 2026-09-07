function out = gen_ood(plan, ctx)
% fleet_ood note: see the header of fleet_v3.plan_load for the provenance
% face; this file drives the OOD directed generation (T-018 Phase 4-D).
%
% fleet_v3.gen_ood - OOD directed generation for the 73 Stage A pipeline,
% the D5 six-segment pipeline per condition:
%   (1) preregistered proposal sampler  -> ParamOverride parsed from the
%       frozen record (A1/A2: host FLOOD u_river_mag TN; B1:
%       chart_depth_voyage_base TN; C1~C3: none, natural host sampling)
%   (2) normal stochastic realization   -> mission_v3.dag_sample('draw')
%       + mission_v3.env_realize('realize') per candidate draw index
%   (3) range/REQUIRE predicate check   -> fleet_v3.ood_judge steps
%       (canonical registry predicates via fleet_v3.predicate_eval)
%   (4) physical-domain guard           -> hard bounds (|U|<=2.8,
%       ht>=1.3) + the conservative 700 kW propulsion capacity check
%       (fleet_v3.propulsion_feasibility_check); violations are
%       STRESS_SET removal rows (D5: never an Axis-A/B/C sample)
%   (5) accept or reject-resample       -> realized-value membership
%       bands (A1/A2 sup|U|, B1 ht_effective), guard-band rejection
%       (FR-519), canonical REQUIRE tokens for C conditions, token
%       priority over simultaneous numerical hits
%   (6) OOD provenance/meta             -> dataset_domain='OOD',
%       ood_axis / ood_condition_id / ood_token_id /
%       ood_def_version='DGN-11-v1' / physics_domain_ref /
%       heldout_token_registry_sha (fleet_v3.export_ood)
%
%   out = fleet_v3.gen_ood(plan, ctx)
%
% Accepted candidates run the SAME member chain and activation criteria
% as the IID corpus (family_expand default matrix, 4 members, eligibility
% name-value injection from the plan).  Each condition has its own draw
% segment starting at the plan draw base (activation stage_a "OOD:
% condition segment k=900"; smoke segment k=950); rejected candidates
% never occupy a family slot - the draw index advances, within
% plan.ood.max_attempts per family.  An unreachable condition within the
% attempt budget is a controlled failure (GATE:FLEETV3:OOD:UNREACHABLE):
% bands/predicates/perturbations are never widened (escalation path).
%
% Error ids: GATE:FLEETV3:OOD:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; thin body + local
% functions; no chained indexed assignment.

    families = local_families(plan);
    draw_base = local_draw_base(plan);
    seed = char(plan.ood.seed);
    max_att = double(plan.ood.max_attempts);

    mode = char(plan.mode);
    if strcmp(mode, 'smoke')
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'qualification_smoke', 'ood');
    else
        art_dir = fullfile(plan.modele_root, 'datasets_v3', ...
            'qualification', 'stage_a_ood');
    end
    if ~exist(art_dir, 'dir')
        mkdir(art_dir);
    end

    conds = plan.ood.conditions;
    voy_c = {};
    ood_retry_c = {};
    cond_audit = struct([]);
    stress_rows = {};
    t_run = tic;

    for c = 1:numel(conds)
        cid = char(conds{c});
        host = plan.ood.hosts.(cid);
        ov = plan.ood.proposal.(cid).override;

        draw_start = draw_base;
        attempts = 0;
        n_reject_guard = 0;
        n_reject_target = 0;
        n_stress = 0;
        accepted = false;
        dag = [];
        rl = [];
        judge = [];

        for att = 1:max_att
            bdi = draw_start + (att - 1);
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
                    % FR-537 audit precedent: no candidate draw is
                    % silently dropped - persist removal rows
                    % incrementally so a later controlled failure
                    % cannot lose the evidence chain (T-018 A2 finding)
                    local_write_json(fullfile(art_dir, ...
                        '_ood_stress_ledger.json'), ...
                        struct('condition', {cid}, 'n_removed', ...
                        {double(n_stress)}, 'rows', {stress_rows}));
                    % D5: a domain violation is a removal row, never an
                    % OOD sample; keep resampling the segment
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
                ['condition %s did not reach an accepted candidate ', ...
                 'within %d attempts (%d guard-band rejects, %d ', ...
                 'no-target rejects, %d STRESS_SET removals); bands, ', ...
                 'predicates and perturbation magnitudes are frozen - ', ...
                 'escalate, do not widen'], cid, max_att, ...
                n_reject_guard, n_reject_target, n_stress);
        end

        fam = mission_v3.family_expand('expand', dag, 'Realization', rl);
        bdi = dag.base_draw_index;
        n_members = double(fam.n_members);
        n_acc_member = 0;
        if families ~= 1
            % Phase 1 note: stage_a and smoke both run one family per
            % condition; the formal Stage B 10-family loop is the Phase 2
            % extension of exactly this block.
            error('GATE:FLEETV3:OOD:FAMILIES', ...
                'Phase 1 supports one family per condition (got %d)', ...
                families);
        end
        for k = 1:n_members
            meta_extra = local_meta_extra(plan, cid, judge);
            voyage_id = sprintf('%s_OOD_%s_D%05d_M%d', ...
                local_prefix(plan), cid, bdi, k);
            [row, retry_rows] = local_member_with_retry(plan, ctx, ...
                art_dir, voyage_id, dag, rl, fam, k, meta_extra);
            voy_c{end+1} = row; %#ok<AGROW>
            for rr_i = 1:numel(retry_rows)
                ood_retry_c{end+1} = retry_rows{rr_i}; %#ok<AGROW>
            end
            if strcmp(char(row.disposition), 'ACCEPT')
                n_acc_member = n_acc_member + 1;
            end
        end

        ca = struct();
        ca.condition_id = cid;
        ca.ood_axis = char(judge.ood_axis);
        ca.token_id = char(plan.ood.hosts.(cid).token_id);
        ca.host_template = char(host.template);
        ca.host_regime = char(host.regime);
        ca.attempts_used = double(attempts);
        ca.accepted = true;
        ca.reject_guard_band = double(n_reject_guard);
        ca.reject_no_target = double(n_reject_target);
        ca.stress_set_removed = double(n_stress);
        ca.accepted_voyages = double(n_acc_member);
        ca.in_band_hit_rate = double(attempts == 1);
        ca.require_reachable = true;
        ca.sup_abs = double(max(abs(rl.u_river_mps(:))));
        ca.ht_effective = double(rl.water.ht_effective);
        if isempty(cond_audit)
            cond_audit = ca;
        else
            cond_audit(end+1) = ca; %#ok<AGROW>
        end
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

    % removal ledger (D5 stress rows are kept inspectable, FR-537
    % audit precedent: no candidate draw is silently dropped)
    local_write_json(fullfile(art_dir, '_ood_ledger.json'), ...
        struct('corpus', {out.corpus}, 'mode', {out.mode}, ...
        'seed', {out.seed}, 'voyages', {voy_c}, ...
        'stress_rows', {stress_rows}, 'retry', {ood_retry_c}));
end

%% ========================== local helpers ===========================

function families = local_families(plan)
    if strcmp(char(plan.mode), 'smoke')
        families = double(plan.smoke.ood_families);
    else
        families = 1;   % stage_a: one family per condition
    end
end

function db = local_draw_base(plan)
    if strcmp(char(plan.mode), 'smoke')
        db = double(plan.smoke.draw_base);
    else
        db = double(plan.ood.draw_base);   % stage_a segment k=900
    end
end

function p = local_prefix(plan)
    if strcmp(char(plan.mode), 'smoke')
        p = 'FLTS';
    else
        p = 'FLTO';
    end
end

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

function [row, retry_rows] = local_member_with_retry(plan, ctx, ...
        art_dir, voyage_id, dag, rl, fam, k, meta_extra)
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
