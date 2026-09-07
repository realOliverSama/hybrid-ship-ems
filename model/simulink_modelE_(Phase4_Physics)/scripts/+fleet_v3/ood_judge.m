function judge = ood_judge(plan, cond_id, rl)
% fleet_v3.ood_judge - OOD acceptance judgment for one realized
% candidate (T-018 Phase 4-D; D5 six-segment pipeline segments 3~5,
% realized on the env_realize('realize', ...) output).
%
%   judge = fleet_v3.ood_judge(plan, cond_id, rl)
%
% This is the +fleet_v3 OOD face and is deliberately NOT the
% env_realize IID face: OOD candidates are expected to leave the IID
% support, so the env layer's disposition is consumed as evidence only.
% All thresholds come from the frozen activation record (plan, via
% fleet_v3.plan_load) and the frozen heldout_token_registry (canonical
% REQUIRE predicates via fleet_v3.predicate_eval - D4, never
% hand-copied).
%
% Judgment order (machine-determined; every non-ACCEPT outcome names the
% segment it comes from):
%   (1) domain bounds guard (D5 segment 4 hard bounds; activation
%       axis_b domain_bounds): ht_effective < 1.3 (hard floor 1.2 plus
%       stress margin [1.2, 1.3) both land here as STRESS_SET) OR
%       sup|U| > 2.8  ->  STRESS_SET (D5: physical-domain violation,
%       "never an Axis-A/B/C sample"; recorded as a removal row, no
%       artifact)
%   (2) guard band (activation axis_a guard_band_rule:
%       GUARD(direction) = (IID_SUPPORT_MAX(direction), 2.0]; direction
%       chosen by the env_realize rule - following when a return
%       segment exists, else adverse): realized sup inside the guard
%       band  ->  REJECT_GUARD_BAND (reject-resample; FR-519: neither
%       formal IID nor formal OOD)
%   (3) target membership (D5 segment 3 range/REQUIRE check):
%       C conditions: the condition's canonical token predicate must
%         trigger (registry-driven via fleet_v3.predicate_eval);
%       A1/A2: realized sup inside the condition band;
%       B1: realized ht_effective inside [1.3, 1.5).
%       No hit  ->  REJECT_NO_TARGET (reject-resample)
%   (4) token priority (activation axis_c.token_priority: "Axis C token
%       > simultaneous numerical-axis hit"): when an A/B candidate also
%       triggers any canonical REQUIRE token, the record is taken as an
%       Axis C sample (token axis wins; condition follows the token)
%   (5) 700 kW propulsion capacity guard (D5 segment 4; conservative
%       analytic estimate at 11 kn adverse, see
%       fleet_v3.propulsion_feasibility_check) on the accepted candidate
%       ->  STRESS_SET when the demand exceeds the rating
%   ACCEPT otherwise (then eligibility/windows/export belong to the
%       caller, same activation criteria as the IID corpus)
%
% rl is a mission_v3.env_realize('realize', ...) output; the consumed
% fields are u_river_mps (whole-grid sup), segments(.role),
% water.ht_effective.  rl.disposition is ignored on purpose (IID face).
%
% judge fields: disposition, reason, ood_axis, ood_condition_id,
%   ood_token_id, evidence (sup_abs, ht_effective, flow_axis, hit band,
%   predicate evidence, propulsion res), attempts_note
% Error ids: GATE:FLEETV3:JUDGE:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; thin body + local
% functions.

    if ~(isstruct(plan) && isscalar(plan))
        error('GATE:FLEETV3:JUDGE:ARG', 'plan must be a scalar struct');
    end
    cond_id = local_s(cond_id);
    if ~ismember(cond_id, plan.ood.conditions)
        error('GATE:FLEETV3:JUDGE:ARG', ...
            'condition %s not in the frozen plan conditions', cond_id);
    end
    if ~(isstruct(rl) && isscalar(rl)) || ~isfield(rl, 'u_river_mps') ...
            || ~isfield(rl, 'water') || ~isfield(rl, 'segments')
        error('GATE:FLEETV3:JUDGE:ARG', ...
            'rl must be a mission_v3.env_realize ''realize'' output');
    end

    sup = max(abs(double(rl.u_river_mps(:))));
    ht = double(rl.water.ht_effective);
    nseg = numel(rl.segments);
    roles = cell(1, nseg);
    adv_sup = zeros(1, nseg);
    has_return = false;
    for i = 1:nseg
        roles{i} = char(rl.segments(i).role);
        adv_sup(i) = double(rl.segments(i).u_adverse_abs_sup);
        if strcmp(roles{i}, 'return')
            has_return = true;
        end
    end

    judge = struct();
    judge.disposition = '';
    judge.reason = '';
    judge.ood_axis = '';
    judge.ood_condition_id = cond_id;
    judge.ood_token_id = '';
    judge.evidence = struct('sup_abs', sup, 'ht_effective', ht, ...
        'flow_axis', local_flow_axis(has_return), 'band_hit', '', ...
        'token_hit', '', 'predicate', [], 'propulsion', []);

    % ---- (1) domain bounds guard (hard bounds |U|<=2.8, ht>=1.3;
    %         [1.2,1.3) stress margin folds into the same STRESS_SET) --
    if ht < plan.ood.band_B1.min || sup > plan.ood.sup_domain_max
        judge.disposition = 'STRESS_SET';
        if ht < plan.ood.band_B1.min
            judge.reason = sprintf( ...
                'ht_effective %.17g below the axis B membership/stress ', ...
                'floor %.17g', ht, plan.ood.band_B1.min);
        else
            judge.reason = sprintf( ...
                'sup|U| %.17g above the hard domain bound %.17g', ...
                sup, plan.ood.sup_domain_max);
        end
        return;
    end

    % ---- (2) guard band (realized sup, direction rule as env_realize) --
    if strcmp(judge.evidence.flow_axis, 'following')
        gb = plan.ood.guard_following;
    else
        gb = plan.ood.guard_adverse;
    end
    if local_in_band(sup, gb)
        judge.disposition = 'REJECT_GUARD_BAND';
        judge.reason = sprintf( ...
            'sup|U| %.17g inside the %s guard band (%.17g, %.17g]', ...
            sup, judge.evidence.flow_axis, gb.min, gb.max);
        return;
    end

    % ---- (3) canonical REQUIRE token evaluation (registry-driven; all
    %         three tokens evaluated so a simultaneous hit can win the
    %         axis in step 4) --------------------------------------------
    facts = struct('segment_roles', {roles}, ...
        'seg_u_adverse_abs', adv_sup, 'ht_effective', ht);
    tok_hit = '';
    tok_res = [];
    for i = 1:numel(plan.ood.tokens)
        tk = plan.ood.tokens{i};
        if ~strcmp(char(tk.ood_action), 'REQUIRE')
            continue;
        end
        r = fleet_v3.predicate_eval(tk, facts);
        if isempty(tok_res)
            tok_res = r;
        else
            tok_res(end+1) = r; %#ok<AGROW>
        end
        if r.triggered && isempty(tok_hit)
            tok_hit = char(r.token_id);
            judge.evidence.predicate = r;
        end
    end
    judge.evidence.token_hit = tok_hit;

    % ---- (3b) numerical membership of the requested condition ---------
    band_hit = '';
    switch cond_id
        case 'A1'
            if local_in_band(sup, plan.ood.band_A1)
                band_hit = 'A1';
            end
        case 'A2'
            if local_in_band(sup, plan.ood.band_A2)
                band_hit = 'A2';
            end
        case 'B1'
            if local_in_band(ht, plan.ood.band_B1)
                band_hit = 'B1';
            end
        otherwise   % C1/C2/C3: membership IS the canonical token
            if ~isempty(plan.ood.hosts.(cond_id).token_id) ...
                    && strcmp(tok_hit, ...
                    plan.ood.hosts.(cond_id).token_id)
                band_hit = cond_id;
            end
    end
    judge.evidence.band_hit = band_hit;
    if isempty(band_hit)
        judge.disposition = 'REJECT_NO_TARGET';
        judge.reason = sprintf( ...
            ['realized evidence (sup=%.17g, ht=%.17g, token=%s) does ', ...
             'not meet the %s membership target'], ...
            sup, ht, tok_hit, cond_id);
        return;
    end

    % ---- (4) token priority: token axis beats a simultaneous
    %         numerical-axis hit (evidence.band_hit keeps the numerical
    %         hit, if any, as the simultaneity record) ------------------
    if ~isempty(tok_hit)
        judge.disposition = 'ACCEPT';
        judge.reason = 'token_priority_or_membership';
        judge.ood_axis = 'C';
        judge.ood_condition_id = local_condition_of_token(plan, tok_hit);
        judge.ood_token_id = tok_hit;
        % ---- (5) 700 kW propulsion capacity guard on the candidate ----
        prop = fleet_v3.propulsion_feasibility_check(sup, ht);
        judge.evidence.propulsion = prop;
        if ~prop.feasible
            judge.disposition = 'STRESS_SET';
            judge.reason = sprintf( ...
                ['propulsion demand %.4f kW exceeds the 700 kW rating ', ...
                 'at 11 kn adverse with sup|U|=%.17g'], ...
                prop.p_demand_kW, sup);
        end
        return;
    end

    % ---- accept on the numerical axis --------------------------------
    judge.disposition = 'ACCEPT';
    judge.reason = 'numerical_band_membership';
    switch band_hit
        case 'A1'
            judge.ood_axis = 'A';
        case 'A2'
            judge.ood_axis = 'A';
        case 'B1'
            judge.ood_axis = 'B';
        otherwise
            error('GATE:FLEETV3:JUDGE:INTERNAL', ...
                'accept path reached without a band hit (%s)', cond_id);
    end
    judge.ood_condition_id = band_hit;
    % ---- (5) 700 kW propulsion capacity guard on the candidate --------
    prop = fleet_v3.propulsion_feasibility_check(sup, ht);
    judge.evidence.propulsion = prop;
    if ~prop.feasible
        judge.disposition = 'STRESS_SET';
        judge.reason = sprintf( ...
            ['propulsion demand %.4f kW exceeds the 700 kW rating at ', ...
             '11 kn adverse with sup|U|=%.17g'], prop.p_demand_kW, sup);
    end
end

%% ========================== local helpers ===========================

function cid = local_condition_of_token(plan, token_id)
    cid = '';
    conds = plan.ood.conditions;
    for i = 1:numel(conds)
        if strcmp(char(plan.ood.hosts.(conds{i}).token_id), token_id)
            cid = conds{i};
            return;
        end
    end
    error('GATE:FLEETV3:JUDGE:TOKEN', ...
        'triggered token %s maps to no planned condition', token_id);
end

function ax = local_flow_axis(has_return)
% Same direction rule as the env_realize IID face: the strictest bound
% (following) applies when a return segment exists.
    if has_return
        ax = 'following';
    else
        ax = 'adverse';
    end
end

function tf = local_in_band(v, b)
    ok_min = (v > double(b.min)) || (logical(b.min_incl) && v == double(b.min));
    ok_max = (v < double(b.max)) || (logical(b.max_incl) && v == double(b.max));
    tf = ok_min && ok_max;
end

function s = local_s(v)
    if ischar(v)
        s = v;
    elseif isstring(v) && numel(v) == 1
        s = char(v);
    else
        error('GATE:FLEETV3:JUDGE:ARG', 'expected a char/string scalar');
    end
end
