function res = propulsion_feasibility_check(u_sup_abs, ht_effective)
% fleet_v3.propulsion_feasibility_check - conservative analytic estimate
% of the propulsion power demand for an 11 kn head-sea (adverse current)
% voyage against the Model E 700 kW aggregated propulsion rating
% (T-018 Phase 4-D Stage A; activation record D5 pipeline segment 4:
% "physical-domain guard (700 kW propulsion capacity at 11 kn adverse;
% hard bounds |U|<=2.8, ht>=1.3)").
%
%   res = fleet_v3.propulsion_feasibility_check(u_sup_abs, ht_effective)
%
% INPUTS (realized generation-side values)
%   u_sup_abs     whole-voyage sup |U_river| [m/s] (river coordinate
%                 magnitude; env_realize v1 conservative sup incl.
%                 prehistory)
%   ht_effective  realized H/T = (h_chart + delta_wl) / draft
%
% MODEL (every coefficient read from config/ModelE_Physics_Parameters.m
% via the same base-workspace provision the 73 pipeline already runs;
% nothing re-derived here):
%   V_ship   = 11 kn = ASSUMED_TargetSpeed (5.65 m/s service speed)
%   V_rel    = V_ship + u_sup_abs
%              (conservative: the whole-voyage flow sup is opposed over
%              the full run at full service speed; in reality the sup is
%              transient and not every segment is adverse)
%   R_deep   = resistance_mult_hi * Hull_K_R * V_rel^2
%              (R_deep = res_mult * K_R * V_rel*|V_rel|; resistance_mult
%              pinned to its frozen upper support 1.05 - conservative)
%   shallow  Zhang/Rhine86 relative-increment chain, IDENTICAL formulas
%            to the Model E hull block (scripts/+model/code_hull_e.m):
%              Re   = V_rel*Lpp/nu_water  (floor 1e3 as in the block)
%              C_F  = 0.075/(log10(Re)-2)^2            (ITTC-57)
%              C_F* = 0.08468/(log10(Re)-1.631)^2
%                     * (1 + c1/(log10(Re)+c2)*HT_eff^c3)
%              dk   = 0.644*HT_eff^(-1.72)             (Millward)
%              dr   = (1+k+dk)*C_F*/((1+k)*C_F) - 1
%            HT_eff clamped at Shallow_HT_min_valid (1.2) with a domain
%            warning, exactly like the hull block (no silent
%            extrapolation; H/T < 1.2 is already rejected as STRESS_SET
%            by the OOD bounds guard before this check runs).
%   P demand = (1 + ASSUMED_Sea_Margin) * R_total * V_ship
%              / (ASSUMED_Prop_Efficiency * Gear_Eff)
%              (advance speed taken as the FULL ship speed V_ship, not
%              the wake-retarded speed - conservative for power demand)
%
% FEASIBILITY: res.feasible = (P_demand_kW <= Mot_P_nom = 700 kW).
% Monotonicity (unit-tested): dP/du > 0 for fixed H/T, and dP/d(1/HT) > 0
% for fixed u - a larger adverse flow or shallower water always demands
% more power, so the check can never flip feasibility in the wrong
% direction.
%
% CONSERVATISM (Stage A evidence, recorded in the manifest):
%   (i)   sup over the whole voyage (incl. prehistory) opposed at full
%         service speed for the entire run;
%   (ii)  resistance multiplier at its frozen support upper 1.05;
%   (iii) sea margin 15% on top;
%   (iv)  quasi-propulsive efficiency at the frozen 0.60 (band
%         0.55~0.65) plus gearbox 0.96;
%   (v)   shallow-water increment at the realized H/T with the hull
%         block's own clamp-and-warn semantics.
% No rudder/hydrodynamic-interaction or squat increments are added on
% top; the estimate is therefore an upper bound for straight-run
% resistance demand, suited to a reject-only guard.
%
% res fields: feasible, p_demand_kW, p_rating_kW, v_rel_mps, r_total_n,
%             ht_eff_used, domain_warning, inputs
% Error ids: GATE:FLEETV3:PROP:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; parameters consumed
% from the provisioned base workspace (fail-closed when absent).

    if ~isnumeric(u_sup_abs) || ~isscalar(u_sup_abs) || ~isfinite(u_sup_abs) ...
            || u_sup_abs < 0
        error('GATE:FLEETV3:PROP:ARG', ...
            'u_sup_abs must be a finite non-negative scalar');
    end
    if ~isnumeric(ht_effective) || ~isscalar(ht_effective) ...
            || ~isfinite(ht_effective) || ht_effective <= 0
        error('GATE:FLEETV3:PROP:ARG', ...
            'ht_effective must be a finite positive scalar');
    end

    p = local_params();

    v_ship = p.target_speed_mps;                 % 11 kn service speed
    v_rel = v_ship + double(u_sup_abs);
    ht_eff = double(ht_effective);
    domain_warning = false;
    if ht_eff > p.shallow_ht_deep_limit
        % deep-water asymptote: no shallow increment (block semantics)
        r_total = p.res_mult_hi * p.k_r * v_rel * v_rel;
    else
        if ht_eff < p.shallow_ht_min_valid
            ht_eff = p.shallow_ht_min_valid;     % clamp (block semantics)
            domain_warning = true;               % MODEL_DOMAIN_WARNING
        end
        r_deep = p.res_mult_hi * p.k_r * v_rel * v_rel;
        re_v = max(v_rel * p.lpp_m / p.nu_water, 1e3);
        lr = log10(re_v);
        c_f = 0.075 / (lr - 2)^2;
        c_fs = 0.08468 / (lr - 1.631)^2 ...
            * (1 + p.shallow_c1 / (lr + p.shallow_c2) ...
            * ht_eff^p.shallow_c3);
        dk = 0.644 * ht_eff^(-1.72);
        dr = (1 + p.k_form + dk) * c_fs / ((1 + p.k_form) * c_f) - 1;
        r_total = r_deep * (1 + dr);
    end
    p_demand_w = (1 + p.sea_margin) * r_total * v_ship ...
        / (p.prop_eff * p.gear_eff);
    p_demand_kW = p_demand_w / 1000.0;

    res = struct();
    res.feasible = (p_demand_kW <= p.rating_kW);
    res.p_demand_kW = p_demand_kW;
    res.p_rating_kW = p.rating_kW;
    res.v_rel_mps = v_rel;
    res.r_total_n = r_total;
    res.ht_eff_used = ht_eff;
    res.domain_warning = domain_warning;
    res.inputs = struct('u_sup_abs', double(u_sup_abs), ...
        'ht_effective', double(ht_effective), ...
        'v_ship_mps', v_ship, 'note', PROP_CONST_NOTE());
end

function out = local_params()
% Consume the frozen Model E physical parameters from the base
% workspace provisioned by the 73 pipeline (the same provision the C
% producer chain uses).  Fail-closed: a missing parameter is a pipeline
% bug, never silently defaulted.
    need = { ...
        'Hull_K_R',            'k_r'; ...
        'Hull_Lpp_m',          'lpp_m'; ...
        'nu_water',            'nu_water'; ...
        'Hull_k_form',         'k_form'; ...
        'Shallow_c1',          'shallow_c1'; ...
        'Shallow_c2',          'shallow_c2'; ...
        'Shallow_c3',          'shallow_c3'; ...
        'Shallow_HT_deep_limit', 'shallow_ht_deep_limit'; ...
        'Shallow_HT_min_valid',  'shallow_ht_min_valid'; ...
        'Mot_P_nom',           'rating_kW'; ...
        'Gear_Eff',            'gear_eff'; ...
        'ASSUMED_Sea_Margin',  'sea_margin'; ...
        'ASSUMED_Prop_Efficiency', 'prop_eff'; ...
        'ASSUMED_TargetSpeed', 'target_speed_mps'};
    out = struct();
    for i = 1:size(need, 1)
        if ~evalin('base', sprintf('exist(''%s'', ''var'')', need{i, 1}))
            error('GATE:FLEETV3:PROP:PARAM', ...
                ['Model E parameter %s not provisioned in the base ', ...
                 'workspace (run the 73 provision first)'], need{i, 1});
        end
        v = evalin('base', need{i, 1});
        if ~isnumeric(v) || ~isscalar(v) || ~isfinite(v)
            error('GATE:FLEETV3:PROP:PARAM', ...
                'Model E parameter %s is not a finite scalar', need{i, 1});
        end
        out.(need{i, 2}) = double(v);
    end
    % frozen resistance-multiplier support upper (conditional_dag_config
    % node resistance_mult; band [0.95, 1.05]).  Read the frozen node to
    % avoid restating the bound.
    rt = local_resistance_support();
    out.res_mult_hi = rt;
end

function hi = local_resistance_support()
    root = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(root) || ~exist(fullfile(root, 'config'), 'dir')
        root = pwd;
    end
    ppath = fullfile(root, 'config', 'mission_v3', ...
        'conditional_dag_config.json');
    if ~exist(ppath, 'file')
        error('GATE:FLEETV3:PROP:PARAM', ...
            'conditional_dag_config.json not found');
    end
    obj = jsondecode(fileread(ppath));
    nodes = obj.nodes;
    if isstruct(nodes)
        n = numel(nodes);
        lst = cell(1, n);
        for i = 1:n
            lst{i} = nodes(i);
        end
    elseif iscell(nodes)
        lst = nodes(:)';
    else
        error('GATE:FLEETV3:PROP:PARAM', 'dag nodes unreadable');
    end
    for i = 1:numel(lst)
        if strcmp(char(lst{i}.node_id), 'resistance_mult')
            pp = lst{i}.params;
            if ~isfield(pp, 'high')
                error('GATE:FLEETV3:PROP:PARAM', ...
                    'resistance_mult params lack high');
            end
            hi = double(pp.high);
            return;
        end
    end
    error('GATE:FLEETV3:PROP:PARAM', ...
        'resistance_mult node not in the frozen dag config');
end

function s = PROP_CONST_NOTE()
    s = ['conservative analytic propulsion-feasibility guard; Model E ', ...
        'frozen coefficients (K_R, shallow chain, sea margin, ', ...
        'quasi-propulsive efficiency, 700 kW rating); whole-voyage ', ...
        'flow sup opposed at 11 kn over the full run; resistance ', ...
        'multiplier at support upper; reject-only use (T-018 ', ...
        'Stage A; activation D5 pipeline segment 4)'];
end
