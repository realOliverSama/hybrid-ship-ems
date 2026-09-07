function out = nav_constraints(mode, varargin)
% mission_v3.nav_constraints - T-010 navigation constraint zones (DGN-01, FR-505/507)
%
%   zones = mission_v3.nav_constraints('zones', realization)
%   ph    = mission_v3.nav_constraints('phases', role, t0, w, v_host)
%   v     = mission_v3.nav_constraints('eval_phases', ph, tVec)
%
% 'zones': realization is a mission_v3.env_realize('realize', ...) output
%   struct (T-009).  The three navigation constraint families are derived as
%   pure deterministic transforms of that realization (no randomness, no
%   environment re-draw, no file writes):
%
%   speed_limit_zones (fixed decision 3, v1 proposal formula):
%     Shallow-water UKC speed limit on SHOAL-reach segments, active only when
%     ht_effective is in [1.5, 2.0):
%         v_limit = v_host * (0.5 + 0.5 * (ht - 1.5) / 0.5)
%     (HT = 1.5 -> 0.5*v_host; HT -> 2.0 -> continuous handover to v_host).
%     ht >= 2.0: no limit; ht < 1.5 cannot reach the plan layer (T-009
%     rejects it) -> fail-closed error.  Redundancy rule: no zone is
%     recorded when v_host <= v_limit.
%
%   bridge_zones (fixed decision 2, Day-1 Gate Ruling section A3
%   BRIDGE_OPERATION host semantics): one zone per bridge segment covering
%   exactly the T-009 segment window (window conservation, fixed decision 1)
%   with the five sub-phases laid out inside the host:
%     Approach 0.15 (v_host) / Deceleration 0.10 (v_host -> v_transit) /
%     ControlledTransit 0.55 (v_transit) / Exit 0.05 (v_transit -> v_host) /
%     Acceleration 0.15 (v_host),  v_transit = 0.7 * v_host.
%     The 1000 m bridge host length is the BRIDGE_OPERATION scene host
%     (template registry revision_t007), NOT a physical bridge-zone length.
%
%   traffic_windows (fixed decision 4): each T-009 traffic event
%     (t_start_abs_s / duration_s / speed_ratio) becomes a plan-layer
%     slowdown window: plateau v_plan = v_host * speed_ratio inside the
%     event window, a linear decrease over 10% of the duration before the
%     window and a linear recovery over 10% of the duration after it
%     (slowdown -> follow -> recover).  Events never modify the Plant
%     (DGN-04).  Overlaps are resolved downstream by min-composition in
%     plan_compose; this function records each event window independently.
%     Windows may overrun the source segment (v1 simplification, composed
%     globally on the voyage time axis).
%
%   Speed provenance: every v_host is the T-009 host speed estimate
%   (segments(i).planned_speed_mps, frozen plan speed table via dag_sample);
%   zone limits are host-relative and are NOT affected by the operator
%   profile (fixed decision 7 layering: mission constraints precede style).
%
%   Input validation is fail-closed: the realization must carry kind
%   'realize' and the consumed fields; ht_effective below the 1.5 IID
%   support floor is a contract violation (GATE:MISSION_V3:NAV:
%   HT_BELOW_SUPPORT).  Disposition policy (ACCEPT vs OOD-side realizations)
%   is owned by the caller, not by this function.
%
%   Determinism: pure function of (realization, frozen constants); identical
%   inputs give bitwise-identical output.  This function writes nothing to
%   disk and never touches the global RNG.
%
%   Error ids: GATE:MISSION_V3:NAV:<REASON>.
%
%   Discipline: ASCII-only source; no absolute paths; no chained indexing;
%   ROOT resolved from mfilename with pwd fallback; no global RNG.

    if (nargin < 1) || (~ischar(mode) && ~isstring(mode))
        error('GATE:MISSION_V3:NAV:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'zones'
            if nargin < 2
                error('GATE:MISSION_V3:NAV:ARG', ...
                    '''zones'' expects: realization');
            end
            out = local_zones(varargin{1});
        case 'phases'
            % Maneuver phase tables (departure/berth) shared with
            % plan_compose: single source of truth for the fixed-decision-6
            % profiles lives in this file.
            if nargin < 5
                error('GATE:MISSION_V3:NAV:ARG', ...
                    '''phases'' expects: role, t0, w, v_host');
            end
            role = varargin{1};
            t0 = varargin{2};
            w = varargin{3};
            v_host = varargin{4};
            if ~ischar(role) && ~isstring(role)
                error('GATE:MISSION_V3:NAV:ARG', 'role must be a string');
            end
            if ~isnumeric(t0) || ~isscalar(t0) || ~isfinite(t0) ...
                    || ~isnumeric(w) || ~isscalar(w) || ~isfinite(w) ...
                    || (w <= 0) || ~isnumeric(v_host) || ~isscalar(v_host) ...
                    || ~isfinite(v_host) || (v_host < 0)
                error('GATE:MISSION_V3:NAV:ARG', ...
                    '''phases'' expects numeric t0, w > 0, v_host >= 0');
            end
            switch char(role)
                case 'departure'
                    out = local_departure_phases(t0, w, v_host);
                case 'berth'
                    out = local_berth_phases(t0, w, v_host);
                otherwise
                    error('GATE:MISSION_V3:NAV:ARG', ...
                        'no maneuver phase table for role %s', char(role));
            end
        case 'eval_phases'
            % Evaluate a phase table (any role) at absolute times.
            if nargin < 3
                error('GATE:MISSION_V3:NAV:ARG', ...
                    '''eval_phases'' expects: phases, tVec');
            end
            ph = varargin{1};
            t_vec = varargin{2};
            if ~isstruct(ph) || ~isnumeric(t_vec)
                error('GATE:MISSION_V3:NAV:ARG', ...
                    '''eval_phases'' expects a phase struct array and a ', ...
                    'numeric time vector');
            end
            out = local_eval_phases(ph, t_vec);
        otherwise
            error('GATE:MISSION_V3:NAV:ARG', ...
                ['unknown mode: %s (nav_constraints implements ''zones'', ', ...
                 '''phases'', ''eval_phases'')'], char(mode));
    end
end

%% ========================== zones builder ==========================

function zones = local_zones(rl)
    rl = local_check_realization(rl);
    ht = rl.water.ht_effective;
    if ~isnumeric(ht) || ~isscalar(ht) || ~isfinite(ht)
        error('GATE:MISSION_V3:NAV:ARG', ...
            'water.ht_effective must be a finite scalar');
    end
    if ht < NAV_CONST_UKC_HT_LOW()
        error('GATE:MISSION_V3:NAV:HT_BELOW_SUPPORT', ...
            ['ht_effective %.6f below the 1.5 IID support floor; such ', ...
             'realizations are rejected by T-009 and cannot reach the ', ...
             'plan layer (fixed decision 3)'], ht);
    end

    nseg = numel(rl.segments);

    % ---- UKC speed-limit zones (fixed decision 3) ----
    blank_slz = struct('segment_index', 0, 'role', '', 'reach', '', ...
        't_start_s', 0.0, 't_end_s', 0.0, 'v_limit_mps', 0.0, ...
        'reason', '');
    slz = repmat(blank_slz, 1, 0);
    for i = 1:nseg
        sg = rl.segments(i);
        if ~strcmp(sg.reach, 'SHOAL') || strcmp(sg.role, 'standby')
            continue;
        end
        v_host = local_host_speed(sg);
        if (ht >= NAV_CONST_UKC_HT_LOW()) && (ht < NAV_CONST_UKC_HT_HIGH())
            frac = NAV_CONST_UKC_FACTOR_AT_LOW() + ...
                NAV_CONST_UKC_FACTOR_AT_LOW() * (ht - NAV_CONST_UKC_HT_LOW()) ...
                / (NAV_CONST_UKC_HT_HIGH() - NAV_CONST_UKC_HT_LOW());
            v_limit = v_host * frac;
            if v_host > v_limit   % no redundant zone when host already slower
                row = blank_slz;
                row.segment_index = i;
                row.role = sg.role;
                row.reach = sg.reach;
                row.t_start_s = sg.t_start_nominal_s;
                row.t_end_s = sg.t_start_nominal_s + sg.window_s;
                row.v_limit_mps = v_limit;
                row.reason = 'UKC_SHALLOW';
                slz = [slz, row];  %#ok<AGROW>
            end
        end
    end

    % ---- bridge zones (fixed decision 2) ----
    blank_bz = struct('segment_index', 0, 'role', '', 'reach', '', ...
        't_start_s', 0.0, 't_end_s', 0.0, 'v_host_mps', 0.0, ...
        'v_transit_mps', 0.0, 'phases', {repmat(blank_phase(), 1, 0)});
    bz = repmat(blank_bz, 1, 0);
    for i = 1:nseg
        sg = rl.segments(i);
        if ~strcmp(sg.role, 'bridge')
            continue;
        end
        v_host = local_host_speed(sg);
        v_transit = NAV_CONST_BRIDGE_TRANSIT_FACTOR() * v_host;
        ph = local_bridge_phases(sg.t_start_nominal_s, sg.window_s, ...
            v_host, v_transit);
        row = blank_bz;
        row.segment_index = i;
        row.role = sg.role;
        row.reach = sg.reach;
        row.t_start_s = sg.t_start_nominal_s;
        row.t_end_s = sg.t_start_nominal_s + sg.window_s;
        row.v_host_mps = v_host;
        row.v_transit_mps = v_transit;
        row.phases = ph;
        bz = [bz, row];  %#ok<AGROW>
    end

    % ---- traffic windows (fixed decision 4) ----
    blank_tw = struct('segment_index', 0, 'role', '', 'reach', '', ...
        't_start_rel_s', 0.0, 't_start_abs_s', 0.0, 'duration_s', 0.0, ...
        'speed_ratio', 0.0, 'v_host_mps', 0.0, 'v_follow_mps', 0.0, ...
        't_ramp_start_s', 0.0, 't_plateau_start_s', 0.0, ...
        't_plateau_end_s', 0.0, 't_ramp_end_s', 0.0);
    tw = repmat(blank_tw, 1, 0);
    n_ev = numel(rl.traffic);
    for e = 1:n_ev
        ev = rl.traffic(e);
        if ~isfinite_field(ev, 't_start_abs_s') || (ev.t_start_abs_s < 0)
            error('GATE:MISSION_V3:NAV:TRAFFIC_WINDOW', ...
                'traffic event %d has invalid t_start_abs_s', e);
        end
        if ~isfinite_field(ev, 'duration_s') || (ev.duration_s <= 0)
            error('GATE:MISSION_V3:NAV:TRAFFIC_WINDOW', ...
                'traffic event %d has invalid duration_s', e);
        end
        if ~isfinite_field(ev, 'speed_ratio') || (ev.speed_ratio <= 0) ...
                || (ev.speed_ratio > 1)
            error('GATE:MISSION_V3:NAV:TRAFFIC_RATIO', ...
                ['traffic event %d speed_ratio %.6f outside (0, 1]; a ', ...
                 'traffic encounter can only slow the plan down'], ...
                e, ev.speed_ratio);
        end
        si = ev.segment_index;
        if ~(isnumeric(si) && isscalar(si) && (si >= 1) && (si <= nseg) ...
                && (si == floor(si)))
            error('GATE:MISSION_V3:NAV:TRAFFIC_WINDOW', ...
                'traffic event %d has invalid segment_index', e);
        end
        sg = rl.segments(si);
        v_host = local_host_speed(sg);
        v_follow = v_host * ev.speed_ratio;
        d = ev.duration_s;
        row = blank_tw;
        row.segment_index = si;
        row.role = sg.role;
        row.reach = sg.reach;
        row.t_start_rel_s = ev.t_start_rel_s;
        row.t_start_abs_s = ev.t_start_abs_s;
        row.duration_s = d;
        row.speed_ratio = ev.speed_ratio;
        row.v_host_mps = v_host;
        row.v_follow_mps = v_follow;
        row.t_ramp_start_s = ev.t_start_abs_s - NAV_CONST_TRAFFIC_RAMP_FRAC() * d;
        row.t_plateau_start_s = ev.t_start_abs_s;
        row.t_plateau_end_s = ev.t_start_abs_s + d;
        row.t_ramp_end_s = ev.t_start_abs_s + d ...
            + NAV_CONST_TRAFFIC_RAMP_FRAC() * d;
        tw = [tw, row];  %#ok<AGROW>
    end

    % ---- assemble ----
    zones = struct();
    zones.kind = 'mission_v3_nav_zones';
    zones.speed_limit_zones = slz;
    zones.bridge_zones = bz;
    zones.traffic_windows = tw;
    zones.ht_effective = ht;
    zones.provenance = local_provenance();
end

%% ======================= sub-phase builders =========================

function ph = local_bridge_phases(t0, w, v_host, v_transit)
% Five BRIDGE_OPERATION sub-phases inside the bridge host window (fixed
% decision 2; Day-1 Gate Ruling section A3).  Phase names follow the ruling
% wording (ASCII); fractions sum to 1 (asserted).
    fracs = NAV_CONST_BRIDGE_PHASE_FRACS();
    names = NAV_CONST_BRIDGE_PHASE_NAMES();
    if abs(sum(fracs) - 1.0) > 1e-12
        error('GATE:MISSION_V3:NAV:CONST', ...
            'bridge phase fractions must sum to 1');
    end
    ph = repmat(blank_phase(), 1, numel(fracs));
    cum = 0.0;
    for k = 1:numel(fracs)
        p = blank_phase();
        p.name = names{k};
        p.frac = fracs(k);
        p.t_start_s = t0 + cum * w;
        cum = cum + fracs(k);
        p.t_end_s = t0 + cum * w;
        switch k
            case 1   % Approach
                p.speed_law = 'const';
                p.v_start_mps = v_host;
                p.v_end_mps = v_host;
            case 2   % Deceleration
                p.speed_law = 'ramp';
                p.v_start_mps = v_host;
                p.v_end_mps = v_transit;
            case 3   % ControlledTransit
                p.speed_law = 'const';
                p.v_start_mps = v_transit;
                p.v_end_mps = v_transit;
            case 4   % Exit
                p.speed_law = 'ramp';
                p.v_start_mps = v_transit;
                p.v_end_mps = v_host;
            case 5   % Acceleration
                p.speed_law = 'const';
                p.v_start_mps = v_host;
                p.v_end_mps = v_host;
        end
        ph(k) = p;
    end
end

function ph = local_departure_phases(t0, w, v_host)
% Departure (unmooring) profile inside the departure host window (fixed
% decision 6, FR-511, v1 proposal values).  NOT a mirror of berthing.
    fracs = NAV_CONST_DEPART_PHASE_FRACS();
    ph = repmat(blank_phase(), 1, numel(fracs));
    cum = 0.0;
    names = {'Startup', 'LowSpeedManeuver', 'FairwayAcceleration'};
    v_start = [NAV_CONST_DEPART_STARTUP_V(), NAV_CONST_DEPART_MANEUVER_V(), ...
        NAV_CONST_DEPART_MANEUVER_V()];
    v_end = [NAV_CONST_DEPART_STARTUP_V(), NAV_CONST_DEPART_MANEUVER_V(), v_host];
    laws = {'const', 'const', 'ramp'};
    for k = 1:numel(fracs)
        p = blank_phase();
        p.name = names{k};
        p.frac = fracs(k);
        p.t_start_s = t0 + cum * w;
        cum = cum + fracs(k);
        p.t_end_s = t0 + cum * w;
        p.speed_law = laws{k};
        p.v_start_mps = v_start(k);
        p.v_end_mps = v_end(k);
        ph(k) = p;
    end
end

function ph = local_berth_phases(t0, w, v_host)
% Berthing profile inside the berth host window (fixed decision 6, FR-511,
% v1 proposal values).  Structurally and numerically different from the
% departure sequence (non-mirror assertion in the unit test).
    fracs = NAV_CONST_BERTH_PHASE_FRACS();
    ph = repmat(blank_phase(), 1, numel(fracs));
    cum = 0.0;
    names = {'Deceleration', 'SlowApproach', 'FinalBerthing'};
    v_start = [v_host, NAV_CONST_BERTH_APPROACH_V(), ...
        NAV_CONST_BERTH_APPROACH_V()];
    v_end = [NAV_CONST_BERTH_SLOW_V(), NAV_CONST_BERTH_APPROACH_V(), ...
        NAV_CONST_BERTH_FINAL_V()];
    laws = {'ramp', 'const', 'ramp'};
    for k = 1:numel(fracs)
        p = blank_phase();
        p.name = names{k};
        p.frac = fracs(k);
        p.t_start_s = t0 + cum * w;
        cum = cum + fracs(k);
        p.t_end_s = t0 + cum * w;
        p.speed_law = laws{k};
        p.v_start_mps = v_start(k);
        p.v_end_mps = v_end(k);
        ph(k) = p;
    end
end

%% ==================== phase table evaluation ========================

function v = local_eval_phases(ph, t_vec)
% Evaluate a phase table at absolute times.  Phase membership is half-open
% [t_start, t_end); a time exactly at the final phase end binds to the last
% phase (defensive; grid points are strictly inside segment windows).
    v = zeros(1, numel(t_vec));
    nph = numel(ph);
    for j = 1:numel(t_vec)
        t = t_vec(j);
        k = 0;
        for q = 1:nph
            if (t >= ph(q).t_start_s) && (t < ph(q).t_end_s)
                k = q;
                break;
            end
        end
        if k == 0
            k = nph;   % exact final-boundary fallback
        end
        p = ph(k);
        if strcmp(p.speed_law, 'const')
            v(j) = p.v_start_mps;
        else
            span = p.t_end_s - p.t_start_s;
            if span <= 0
                v(j) = p.v_end_mps;
            else
                f = (t - p.t_start_s) / span;
                v(j) = p.v_start_mps + (p.v_end_mps - p.v_start_mps) * f;
            end
        end
    end
end

%% ===================== v1 proposal constants ========================
% Fixed numeric values live here as named constants with provenance notes
% (fixed decision 10).  Values traceable to the T-010 delegation spec and
% the Day-1 Gate Ruling; they are v1 proposals, not re-derivable quantities.

function f = NAV_CONST_BRIDGE_PHASE_FRACS()
% FD2 (Day-1 section A3 host semantics): Approach/Deceleration/
% ControlledTransit/Exit/Acceleration time fractions.
    f = [0.15, 0.10, 0.55, 0.05, 0.15];
end

function n = NAV_CONST_BRIDGE_PHASE_NAMES()
    n = {'Approach', 'Deceleration', 'ControlledTransit', 'Exit', ...
        'Acceleration'};
end

function c = NAV_CONST_BRIDGE_TRANSIT_FACTOR()
% FD2 v1 proposal: "safe speed keeping steerage way" quantified as the 0.7
% propulsion-limited transit factor; v_transit = 0.7 * v_host.
    c = 0.7;
end

function c = NAV_CONST_UKC_HT_LOW()
% FD3: UKC limit band lower bound (HT below this is rejected by T-009).
    c = 1.5;
end

function c = NAV_CONST_UKC_HT_HIGH()
% FD3: UKC limit band upper bound (exclusive); ht >= 2.0 is unlimited.
    c = 2.0;
end

function c = NAV_CONST_UKC_FACTOR_AT_LOW()
% FD3 v1 proposal formula: v_limit = v_host * (0.5 + 0.5*(ht-1.5)/0.5);
% factor 0.5 at HT = 1.5, continuous handover to 1.0 at HT -> 2.0.
    c = 0.5;
end

function c = NAV_CONST_TRAFFIC_RAMP_FRAC()
% FD4 v1 proposal: linear decrease over 10% of the event duration before
% the window and linear recovery over 10% after it.
    c = 0.1;
end

function c = NAV_CONST_DEPART_PHASE_FRACS()
% FD6 v1 proposal: startup / low-speed maneuver / fairway acceleration.
    c = [0.10, 0.30, 0.60];
end

function c = NAV_CONST_DEPART_STARTUP_V()
% FD6 v1 proposal: departure startup speed 1.0 m/s.
    c = 1.0;
end

function c = NAV_CONST_DEPART_MANEUVER_V()
% FD6 v1 proposal: departure low-speed maneuvering speed 1.5 m/s.
    c = 1.5;
end

function c = NAV_CONST_BERTH_PHASE_FRACS()
% FD6 v1 proposal: deceleration / slow approach / final berthing.
    c = [0.40, 0.40, 0.20];
end

function c = NAV_CONST_BERTH_SLOW_V()
% FD6 v1 proposal: berth deceleration target speed 1.5 m/s.
    c = 1.5;
end

function c = NAV_CONST_BERTH_APPROACH_V()
% FD6 v1 proposal: berth slow-approach speed 1.0 m/s.
    c = 1.0;
end

function c = NAV_CONST_BERTH_FINAL_V()
% FD6 v1 proposal: final berthing speed 0.5 m/s.
    c = 0.5;
end

%% ======================== validation helpers ========================

function rl = local_check_realization(rl)
    if ~isstruct(rl) || ~isfield(rl, 'kind') || ~strcmp(rl.kind, 'realize')
        error('GATE:MISSION_V3:NAV:ARG', ...
            'realization must be a mission_v3.env_realize ''realize'' struct');
    end
    need = {'segments', 'traffic', 'water'};
    for j = 1:numel(need)
        if ~isfield(rl, need{j})
            error('GATE:MISSION_V3:NAV:ARG', ...
                'realization lacks field: %s', need{j});
        end
    end
    if ~isfield(rl.water, 'ht_effective')
        error('GATE:MISSION_V3:NAV:ARG', ...
            'realization water lacks ht_effective');
    end
    nseg = numel(rl.segments);
    if nseg < 1
        error('GATE:MISSION_V3:NAV:ARG', 'realization has no segments');
    end
    seg_need = {'index', 'role', 'reach', 'window_s', 't_start_nominal_s', ...
        'planned_speed_mps'};
    for i = 1:nseg
        sg = rl.segments(i);
        for j = 1:numel(seg_need)
            if ~isfield(sg, seg_need{j})
                error('GATE:MISSION_V3:NAV:ARG', ...
                    'realization segment %d lacks field: %s', i, seg_need{j});
            end
        end
        if ~isfinite_field(sg, 'window_s') || (sg.window_s <= 0)
            error('GATE:MISSION_V3:NAV:ARG', ...
                'realization segment %d has invalid window_s', i);
        end
    end
    ev_need = {'segment_index', 't_start_rel_s', 't_start_abs_s', ...
        'duration_s', 'speed_ratio'};
    for e = 1:numel(rl.traffic)
        ev = rl.traffic(e);
        for j = 1:numel(ev_need)
            if ~isfield(ev, ev_need{j})
                error('GATE:MISSION_V3:NAV:ARG', ...
                    'traffic event %d lacks field: %s', e, ev_need{j});
            end
        end
    end
end

function v = local_host_speed(sg)
% T-009 host speed estimate; standby segments carry no speed (empty) and
% map to 0 m/s (stationary).
    v = 0.0;
    if ~isempty(sg.planned_speed_mps)
        v = sg.planned_speed_mps;
        if ~isnumeric(v) || ~isscalar(v) || ~isfinite(v) || (v < 0)
            error('GATE:MISSION_V3:NAV:ARG', ...
                'segment %d has invalid planned_speed_mps', sg.index);
        end
    end
end

function ok = isfinite_field(s, name)
    ok = isfield(s, name) && isnumeric(s.(name)) && isscalar(s.(name)) ...
        && isfinite(s.(name));
end

function p = blank_phase()
    p = struct('name', '', 'frac', 0.0, 't_start_s', 0.0, 't_end_s', 0.0, ...
        'speed_law', '', 'v_start_mps', 0.0, 'v_end_mps', 0.0);
end

function prov = local_provenance()
    prov = struct();
    prov.generator = ['mission_v3.nav_constraints (T-010; design DGN-01 ', ...
        'FR-505/507; delegation spec experiments/mission_v3/', ...
        'T-010_delegation_spec.md Required Work A)'];
    prov.fixed_decisions = ['FD2 bridge host sub-phases (fractions 0.15/', ...
        '0.10/0.55/0.05/0.15, v_transit = 0.7*v_host; Day-1 section A3); ', ...
        'FD3 UKC formula v_limit = v_host*(0.5+0.5*(ht-1.5)/0.5) on SHOAL ', ...
        'reach for ht in [1.5,2.0); FD4 traffic slowdown->follow->recover ', ...
        'with 10% ramps - all v1 proposal adjudications, source: T-010 ', ...
        'delegation spec Fixed Decisions'];
    prov.host_speed_source = ['v_host = T-009 segments(i).planned_speed_mps ', ...
        '(frozen plan speed table via dag_sample); zone limits are ', ...
        'host-relative and operator-independent (FD7 layering)'];
    prov.window_conservation = ['every zone window equals the T-009 ', ...
        'segment window (FD1); windows are never adjusted'];
    prov.overlap_policy = ['zone overlaps are resolved by ', ...
        'min-composition in plan_compose; traffic windows are recorded ', ...
        'per event and may overrun the source segment (v1)'];
    prov.determinism = 'pure function; no randomness; no file writes';
end
