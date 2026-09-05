function res = feasibility_sweep(varargin)
% mission_v3.audits.feasibility_sweep - T-005 READ-ONLY joint feasibility matrix sweep
% (Phase 4-C mission v3; DGN-05 R2/R3 pre-freeze evidence for the ID support
%  bound candidate envelope; FR-519/520; delegation spec
%  experiments/mission_v3/T-005_delegation_spec.md).
%
% PURPOSE
%   Sweep planned SOG x along-track relative current x H/T grid on Model E and
%   measure, per grid point: sim completion, safety/interlock trips, battery
%   SOP saturation, shallow-domain warnings, planned-speed reachability, plus
%   raw power/resistance/SOC evidence. Output is a machine-readable JSON at
%   results/mission_v3/id_feasibility_sweep.json (crash-resumable: per-point
%   incremental rewrite via tmp+rename; completed points are skipped on
%   restart). This function does NOT modify any Model E file: scenarios are
%   constructed at runtime as SCN structs and injected through the existing
%   base-workspace channels (gate_e.reset_bench_nominal + model.run_scenario_sim).
%
% FIXED DECISIONS IMPLEMENTED (delegation spec, 2026-09-05)
%   - Grid: SOG [2.0 3.0 4.0 5.0 6.0 7.522] m/s; adverse flow
%     [0 0.5 1.0 1.4 1.8 2.0 2.4 2.8] m/s (full cross product with H/T
%     [1.2 1.35 1.5 2.0 3.0]); following sampling only at flow
%     [1.5 1.8 2.0] x SOG [4.0 6.0] (6 points, fixed H/T = 1.5 boundary).
%   - H/T semantics: H_effective = h_chart (+ dWL = 0 here); H/T = h/draft,
%     draft = Hull_T_m = 2.188 m (config/ModelE_Physics_Parameters.m); the
%     h = H/T * Hull_T_m conversion follows the T-412 E05 S31..S36 family.
%   - Current injection: existing time-mode flow interface
%     (SCN.current_time_s / current_value_mps, 1 s grid, ZOH step table per
%     +model/build_model_e.m CODE_CURRENT). Sign convention: negative value =
%     adverse (against heading, drag direction, S03 upstream semantics),
%     positive = following (S02 downstream semantics).
%   - Speed plan: time-mode SCN.v_time_s / v_value_mps constant segment;
%     accel/decel ramps = existing defaults (SCN.accel/decel_ramp_mps2).
%   - Leg duration 600 s per point; hotel 100 kW; initial SOC 0.60;
%     voyage_distance_m = 40000 (S20 time-mode convention: no distance stop
%     inside a 600 s leg, max travelled distance ~4.6 km).
%   - PMS_SOURCE_MODE = 0 (BASELINE_POLICY) explicitly assigned (bench
%     convention of gate_e.reset_bench_nominal / 40-physics-gate family);
%     EMS_CTRL_LAW = 0 (Baseline) via model.run_scenario_sim mode argument.
%   - Feasibility criteria per point: sim_completed, no_safety_trip
%     (fault_latch steps == 0 AND safety force-stop events == 0),
%     no_sop_saturation (battery_power_limited steps == 0), no_domain_warning
%     (domain_warning steps == 0; explicit model signal, NFR-411), and
%     speed_achieved (mean SOG over last 120 s >= plan - 0.1 m/s).
%   - G2 cross-consistency with results/gate_result.json (checks.G2):
%     sprint reachability at SOG=7.522 vs full_power_speed_band value
%     (14.62 kn); adverse vs following propeller power direction vs
%     U_against_up_P_prop_up; shallow resistance monotonicity vs
%     shallow_mono R_by_tier.
%
% USAGE (single MATLAB process loop; never launch per point)
%   res = mission_v3.audits.feasibility_sweep();                       % full grid (246 pts)
%   res = mission_v3.audits.feasibility_sweep('Mode','pilot');         % 3 corner points
%   res = mission_v3.audits.feasibility_sweep('Mode','degraded');      % reduced grid
%   res = mission_v3.audits.feasibility_sweep('PointList', pl);        % explicit points
%   name-value: Mode, GridOverride, PointList, OutPath, LegDurationS, Resume
%
% ROOT resolution: 4 x fileparts from scripts/+mission_v3/audits/ + pwd
% fallback (workspace AGENTS.md section 20.2; no absolute paths anywhere).

    args = parseArgs(varargin);

    % ---- environment provisioning (idempotent; mirrors 41-style entries) --
    ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
    % Config provisioning MUST land in the BASE workspace: Parameter-scope
    % block data resolves in base at sim time (this function is a function,
    % so plain run() would bind config vars into the function workspace and
    % the model would compile without its physics parameters).
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
    MDL = 'HybridShip_ModelE';
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
    draftM = evalin('base', 'Hull_T_m');   % single source of truth for H/T

    if isempty(args.OutPath)
        args.OutPath = fullfile(ROOT, 'results', 'mission_v3', 'id_feasibility_sweep.json');
    end
    outDir = fileparts(args.OutPath);
    if ~exist(outDir, 'dir'), mkdir(outDir); end

    % ---- grid / point list ------------------------------------------------
    if ~isempty(args.PointList)
        pts = args.PointList;
        gridMode = 'custom_pointlist';
    else
        pts = buildPointList(args.Mode, args.GridOverride);
        gridMode = char(args.Mode);
    end
    nReq = numel(pts);

    % ---- resume support ---------------------------------------------------
    doneKeys = {};
    priorRecords = {};
    if args.Resume && exist(args.OutPath, 'file')
        try
            prev = jsondecode(fileread(args.OutPath));
            if isfield(prev, 'points') && ~isempty(prev.points)
                priorRecords = prev.points;
                if isstruct(priorRecords)   % jsondecode: homogeneous -> struct array
                    priorRecords = num2cell(priorRecords);
                end
                for i = 1:numel(priorRecords)
                    doneKeys{end+1} = priorRecords{i}.key; %#ok<AGROW>
                end
                fprintf('[T005] resume: %d prior point records found in %s\n', ...
                    numel(priorRecords), args.OutPath);
            end
        catch
            fprintf('[T005] resume: existing file unreadable, starting clean\n');
            priorRecords = {};
            doneKeys = {};
        end
    end

    % ---- main loop (single process, per-point incremental persistence) ----
    tAll = tic;
    records = priorRecords;
    nExec = 0;
    rampMps2 = evalin('base', 'ASSUMED_Speed_Ramp_Up');
    for ip = 1:nReq
        p = pts{ip};
        key = pointKey(p);
        if any(strcmp(doneKeys, key))
            fprintf('[T005] skip (done) %2d/%d %s\n', ip, nReq, key);
            continue;
        end
        tPt = tic;
        rec = runPoint(MDL, p, args.LegDurationS, draftM, rampMps2, ip, key);
        rec.wall_time_s = round(toc(tPt), 2);
        records{end+1} = rec; %#ok<AGROW>
        nExec = nExec + 1;
        fprintf('[T005] %3d/%d %s feasible=%d wall=%.1fs\n', ...
            ip, nReq, key, double(rec.feasible), rec.wall_time_s);
        persistDoc(args.OutPath, records, gridMode, pts, args.LegDurationS, ...
            draftM, nReq, rampMps2);
    end
    wallTotal = toc(tAll);

    % ---- summary + G2 cross-check + final persist -------------------------
    doc = persistDoc(args.OutPath, records, gridMode, pts, args.LegDurationS, ...
        draftM, nReq, rampMps2);
    doc = addSummary(doc, records);
    doc.g2_cross_check = g2CrossCheck(ROOT, records);
    doc.degradation = struct('applied', false, 'reason', '', ...
        'note', 'set by caller if Fixed Decision 10 degrade rule triggered');
    persistDocRaw(args.OutPath, doc);

    nFeas = sum(cellfun(@(r) r.feasible, records));
    fprintf('[T005] done: %d requested / %d executed(now) / %d feasible | wall=%.0f s\n', ...
        nReq, nExec, nFeas, wallTotal);
    fprintf('[T005] JSON: %s\n', args.OutPath);

    res = struct('status', 'done', 'out_path', char(args.OutPath), ...
        'n_points_requested', nReq, 'n_points_executed_this_run', nExec, ...
        'n_feasible', nFeas, 'wall_total_s', round(wallTotal, 1), ...
        'doc', doc);
end

% =========================================================================
function a = parseArgs(argv)
    ip = 1;
    a = struct();
    a.Mode = "full";
    a.GridOverride = struct();
    a.PointList = {};
    a.OutPath = '';              % char: isempty('') drives default assignment
    a.LegDurationS = 600;
    a.Resume = true;
    while ip <= numel(argv)
        nm = argv{ip};
        if ip + 1 > numel(argv)
            error('feasibility_sweep:missingValue', 'name-value %s lacks value', nm);
        end
        val = argv{ip + 1};
        switch lower(char(nm))
            case 'mode',       a.Mode = string(val);
            case 'gridoverride', a.GridOverride = val;
            case 'pointlist',  a.PointList = val;
            case 'outpath',    a.OutPath = char(string(val));
            case 'legdurations', a.LegDurationS = val;
            case 'resume',     a.Resume = logical(val);
            otherwise
                error('feasibility_sweep:badArg', 'unknown name-value %s', nm);
        end
        ip = ip + 2;
    end
    valid = ["full", "pilot", "degraded", "custom"];
    if ~isempty(a.PointList)
        a.Mode = "custom";
    elseif ~any(a.Mode == valid)
        error('feasibility_sweep:badMode', 'Mode must be one of full|pilot|degraded|custom');
    end
end

% =========================================================================
function pts = buildPointList(mode, ov)
    % Full grid per delegation spec Fixed Decision 5 (246 points):
    % adverse main axis 6 SOG x 8 flows x 5 H/T + following sampling 6 points
    % (following at fixed H/T = 1.5 boundary; choice documented in report).
    sog  = [2.0 3.0 4.0 5.0 6.0 7.522];
    adv  = [0.0 0.5 1.0 1.4 1.8 2.0 2.4 2.8];
    hts  = [1.2 1.35 1.5 2.0 3.0];
    fmag = [1.5 1.8 2.0];
    fsog = [4.0 6.0];
    fht  = 1.5;
    if ~isempty(fieldnames(ov))
        if isfield(ov, 'sog_list'),  sog = ov.sog_list;  end
        if isfield(ov, 'adverse_list'), adv = ov.adverse_list; end
        if isfield(ov, 'ht_list'),   hts = ov.ht_list;   end
        if isfield(ov, 'following_mag_list'), fmag = ov.following_mag_list; end
        if isfield(ov, 'following_sog_list'), fsog = ov.following_sog_list; end
        if isfield(ov, 'following_ht'), fht = ov.following_ht; end
    end
    pts = {};
    if strcmp(mode, 'pilot')
        % Fixed Decision C.1 corner points
        pts{end+1} = mkPt(7.522, 'adverse',  2.8, 1.5);
        pts{end+1} = mkPt(4.0,   'adverse',  1.8, 1.35);
        pts{end+1} = mkPt(2.0,   'adverse',  0.0, 3.0);
        return;
    end
    if strcmp(mode, 'degraded')
        % Fixed Decision 10 pre-approved fallback (60 + boundary + following).
        % NOTE: spec text "boundary sweep H/T{1.35,1.5} x SOG all 6 tiers x
        % flow{0,1.8,2.8}" enumerates 36 points while the printed count says
        % 30; the explicit enumeration is followed (36), deviation declared.
        sogD  = [4.0 6.0 7.522];
        advD  = [1.4 1.8 2.0 2.8];
        for s = sogD
            for u = advD
                for h = hts
                    pts{end+1} = mkPt(s, 'adverse', u, h); %#ok<AGROW>
                end
            end
        end
        for s = sog
            for u = [0.0 1.8 2.8]
                for h = [1.35 1.5]
                    pts{end+1} = mkPt(s, 'adverse', u, h); %#ok<AGROW>
                end
            end
        end
        pts = [pts, followingPts(fmag, fsog, fht)];
        return;
    end
    % full grid
    for s = sog
        for u = adv
            for h = hts
                pts{end+1} = mkPt(s, 'adverse', u, h); %#ok<AGROW>
            end
        end
    end
    pts = [pts, followingPts(fmag, fsog, fht)];
end

function fp = followingPts(fmag, fsog, fht)
    fp = {};
    for s = fsog
        for u = fmag
            fp{end+1} = mkPt(s, 'following', u, fht); %#ok<AGROW>
        end
    end
end

function p = mkPt(s, dirn, u, h)
    p = struct('sog_mps', s, 'dir', dirn, 'flow_mps', u, 'ht_ratio', h);
end

function key = pointKey(p)
    key = sprintf('sog%.3f_%s_%.2f_ht%.2f', p.sog_mps, p.dir, p.flow_mps, p.ht_ratio);
end

% =========================================================================
function rec = runPoint(MDL, p, legDur, draftM, rampMps2, idx, key)
    rec = struct();
    rec.key = key;
    rec.sog_plan_mps = p.sog_mps;
    rec.flow_dir = p.dir;
    rec.flow_mag_mps = p.flow_mps;
    rec.ht_ratio = p.ht_ratio;
    rec.water_depth_m = p.ht_ratio * draftM;
    rec.leg_duration_s = legDur;
    % signed along-track relative current: negative = adverse (drag direction)
    if strcmp(p.dir, 'adverse')
        uSigned = -p.flow_mps;
    else
        uSigned = +p.flow_mps;
    end
    rec.current_component_mps = uSigned;

    % deterministic bench injection: case isolation (DGN-15) + explicit PMS
    % source; then existing SCN/VOYAGE_* assignment via model.run_scenario_sim
    gate_e.reset_bench_nominal();
    assignin('base', 'PMS_SOURCE_MODE', 0);   % BASELINE_POLICY (bench convention)

    SCN = scnForPoint(p, uSigned, legDur, rampMps2, idx, draftM);
    try
        out = model.run_scenario_sim(MDL, SCN, 0, legDur);
        ds = gate_ctrl.get_ds(out);
        rec = mergeStruct(rec, metricsFromDs(ds, p.sog_mps));
        rec.sim_completed = true;
        rec.err_msg = '';
    catch ME
        rec.sim_completed = false;
        rec.err_id = asciiOf(ME.identifier);
        rec.err_msg = asciiOf(ME.message);
        rec.feasible = false;
    end
end

function SCN = scnForPoint(p, uSigned, legDur, rampMps2, idx, draftM)
    % Runtime SCN construction (read-only discipline: nothing written back to
    % ModelE_TestScenarios.m). Field set mirrors the S31..S36 shallow-water
    % family; time-mode constant planned SOG + 1 s grid constant current.
    ramp = rampMps2;                           % existing default 0.05 m/s^2
    tt = (0:1:legDur)';
    SCN = struct();
    SCN.id = 900000 + idx;                     % unique per executed point
    SCN.name = ['MV3_' pointKey(p)];
    SCN.target_speed_mps = p.sog_mps;
    SCN.voyage_distance_m = 40000;             % S20 time-mode convention (no stop)
    SCN.river_current_mps = uSigned;
    SCN.resistance_profile = 'const';
    SCN.profile_code = 0;
    SCN.resistance_change_position_m = 0;
    SCN.resistance_change_amplitude = 0;
    SCN.resistance_change_ramp_time_s = 0;
    SCN.hotel_kW = 100;
    SCN.initial_SOC = 0.60;
    SCN.random_seed = 42;
    SCN.notes = 'mission_v3 T-005 feasibility sweep point (runtime constructed)';
    SCN.v_knots_x = [0 -1 -1 -1 -1 -1];
    SCN.v_knots_v = [p.sog_mps 0 0 0 0 0];
    SCN.accel_ramp_mps2 = ramp;
    SCN.decel_ramp_mps2 = ramp;
    SCN.resistance_bands = [-1 0 0; -1 0 0; -1 0 0; -1 0 0];
    SCN.current_knots_x = [0 -1 -1 -1];
    SCN.current_knots_v = [0 0 0 0];
    SCN.water_depth_m = p.ht_ratio * draftM;
    SCN.speed_profile_mode = 'time';
    SCN.v_time_s = [0; legDur];
    SCN.v_value_mps = [p.sog_mps; p.sog_mps];
    SCN.current_time_s = tt;
    SCN.current_value_mps = uSigned * ones(size(tt));
    SCN.stop_time_s = legDur;
end

% =========================================================================
function m = metricsFromDs(ds, sogPlan)
    t = ds.t;
    m120 = t >= (t(end) - 120);

    m = struct();
    m.sim_duration_s = t(end);
    % speed reachability (Fixed Decision 7: last 120 s mean >= plan - 0.1)
    m.sog_mean_last120_mps = mean(ds.v_ship_mps(m120));
    m.sog_mean_all_mps = mean(ds.v_ship_mps);
    m.sog_margin_mps = m.sog_mean_last120_mps - sogPlan;
    m.speed_achieved = m.sog_mean_last120_mps >= (sogPlan - 0.1);
    % convergence evidence: SOG slope over the last 120 s window (raw evidence
    % field; lets callers flag non-converged legs, e.g. steady-refinement runs)
    pf = polyfit(t(m120), ds.v_ship_mps(m120), 1);
    m.sog_slope_last120_mps2 = pf(1);

    % safety / interlock trips (16_Safety_Interlock; conventions of
    % gate_ctrl.metrics_of: level-signal rising-edge event counting)
    if isfield(ds, 'fault_latch')
        fl = ds.fault_latch;
        m.n_fault_latch_steps = sum(max(fl, [], 2) > 0.5);
        m.fault_latch_final_dg1 = fl(end, 1);
        m.fault_latch_final_dg2 = fl(end, 2);
    else
        m.n_fault_latch_steps = [];
        m.fault_latch_final_dg1 = [];
        m.fault_latch_final_dg2 = [];
    end
    if isfield(ds, 'safety_force_stop_req')
        m.n_force_stop_events = edgeEvents(max(ds.safety_force_stop_req, [], 2));
    else
        m.n_force_stop_events = [];
    end
    if isfield(ds, 'safety_force_start_req')
        m.n_force_start_events = edgeEvents(max(ds.safety_force_start_req, [], 2));
    else
        m.n_force_start_events = [];
    end
    m.no_safety_trip = isequal(m.n_fault_latch_steps, 0) && ...
        isequal(m.n_force_stop_events, 0);

    % battery SOP saturation (14_Battery_Converter; E08 semantics of the 40
    % gate: P_bat_bus <= P_dis_avail envelope + power_limited flag)
    if isfield(ds, 'battery_power_limited')
        m.n_sop_sat_steps = sum(ds.battery_power_limited > 0.5);
    else
        m.n_sop_sat_steps = [];
    end
    if isfield(ds, 'P_bat_actual_kW') && isfield(ds, 'P_bat_dis_avail_kW')
        m.sop_envelope_max_exceed_kW = max(ds.P_bat_actual_kW - ds.P_bat_dis_avail_kW);
    else
        m.sop_envelope_max_exceed_kW = [];
    end
    m.no_sop_saturation = isequal(m.n_sop_sat_steps, 0);

    % shallow-water domain warning (03_Propulsion_Plant domain_warning, NFR-411)
    if isfield(ds, 'domain_warning')
        m.n_domain_warning_steps = sum(ds.domain_warning > 0.5);
    else
        m.n_domain_warning_steps = [];
    end
    if isfield(ds, 'HT_ratio')
        m.ht_ratio_min_sim = min(ds.HT_ratio);
    else
        m.ht_ratio_min_sim = [];
    end
    m.no_domain_warning = isequal(m.n_domain_warning_steps, 0);

    % raw physics evidence
    m.P_prop_peak_kW = max(ds.P_prop_actual_kW);
    m.P_load_peak_kW = max(ds.P_load_total_kW);
    m.P_prop_mean_last120_kW = mean(ds.P_prop_actual_kW(m120));
    m.P_load_mean_last120_kW = mean(ds.P_load_total_kW(m120));
    m.R_hull_mean_last120_N = mean(ds.R_hull_N(m120));
    m.R_shallow_mean_last120_N = mean(ds.R_shallow_N(m120));
    m.soc_end = ds.SOC(end);
    m.soc_min = min(ds.SOC);
    m.fuel_total_kg = ds.fuel_total_kg(end);

    m.feasible = m.speed_achieved && m.no_safety_trip && ...
        m.no_sop_saturation && m.no_domain_warning;
end

function n = edgeEvents(levelSig)
    s = levelSig(:) > 0.5;
    n = sum([false; diff(s) == 1]);
end

function msg = asciiOf(msg)
    % JSON stays pure ASCII: non-printable / non-ASCII chars collapse to '?'
    msg = regexprep(char(msg), '[^[:print:][:space:]]', '?');
    msg = regexprep(msg, '\?+', '?');
end

function c = mergeStruct(a, b)
    c = a;
    fn = fieldnames(b);
    for i = 1:numel(fn)
        c.(fn{i}) = b.(fn{i});
    end
end

% =========================================================================
function doc = persistDoc(outPath, records, gridMode, pts, legDur, draftM, ...
    nReq, rampMps2)
    doc = struct();
    doc.schema = 'mission_v3_id_feasibility_sweep_v1';
    doc.task = 'T-005 read-only joint feasibility matrix sweep (DGN-05 R2/R3 pre-freeze evidence)';
    doc.generated_at = char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ss'));
    doc.readonly_discipline = ['Model E files untouched; runtime SCN struct ' ...
        'construction via gate_e.reset_bench_nominal + model.run_scenario_sim'];
    doc.grid = gridMeta(gridMode, pts, legDur, draftM, nReq, rampMps2);
    doc.points = records;
    persistDocRaw(outPath, doc);
end

function persistDocRaw(outPath, doc)
    tmp = [outPath, '.tmp'];
    fid = fopen(tmp, 'w');
    fwrite(fid, jsonencode(doc, 'PrettyPrint', true));
    fclose(fid);
    movefile(tmp, outPath, 'f');
end

function g = gridMeta(gridMode, pts, legDur, draftM, nReq, rampMps2)
    g = struct();
    g.mode = gridMode;
    g.planned_sog_mps = uniqueSort(catVals(pts, 'sog_mps'));
    selAdv = cellfun(@(r) strcmp(r.dir, 'adverse'), pts);
    g.adverse_flow_mps = uniqueSort(catVals(pts(selAdv), 'flow_mps'));
    fol = cellfun(@(r) strcmp(r.dir, 'following'), pts);
    if any(fol)
        g.following_flow_mps = uniqueSort(catVals(pts(fol), 'flow_mps'));
        hFol = catVals(pts(fol), 'ht_ratio');
        g.following_ht_ratio = hFol(1);
    else
        g.following_flow_mps = [];
        g.following_ht_ratio = [];
    end
    g.ht_ratio_list = uniqueSort(catVals(pts, 'ht_ratio'));
    g.draft_m = draftM;
    g.ht_to_depth_conversion = 'h = H/T * draft (T-412 E05 S31..S36 convention)';
    g.leg_duration_s = legDur;
    g.hotel_kW = 100;
    g.initial_soc = 0.60;
    g.voyage_distance_m = 40000;
    g.accel_ramp_mps2 = rampMps2;
    g.pms_source_mode = 0;
    g.ems_ctrl_law = 0;
    g.current_sign_convention = 'negative = adverse (against heading), positive = following';
    g.current_grid_s = 1;
    g.n_points_requested = nReq;
    g.n_points_recorded = numel(pts);
end

function v = uniqueSort(v)
    v = unique(v(:)');
end

function v = catVals(pts, fld)
    v = cellfun(@(r) r.(fld), pts);
end

% =========================================================================
function doc = addSummary(doc, records)
    s = struct();
    feas = cellfun(@(r) r.feasible, records);
    comp = cellfun(@(r) r.sim_completed, records);
    s.n_points = numel(records);
    s.n_feasible = sum(feas);
    s.n_infeasible = sum(~feas);
    s.n_sim_failed = sum(~comp);
    s.feasibility_rate = s.n_feasible / max(s.n_points, 1);

    % per-axis marginal views (executed grid meaning)
    s.by_sog = axisView(records, 'sog_plan_mps');
    s.by_adverse_flow = axisView(records(records2sel(records, 'adverse')), 'flow_mag_mps');
    s.by_ht_ratio = axisView(records, 'ht_ratio');

    % criteria failure counts (over executed points)
    s.criterion_fail_counts = critCounts(records);

    % candidate-interior infeasible list (adverse <= 1.8 & H/T >= 1.5, or
    % following <= 1.5 & H/T >= 1.5) - evidence only, no bound editing.
    infeas = {};
    for i = 1:numel(records)
        r = records{i};
        if r.feasible
            continue;
        end
        inCand = false;
        if strcmp(r.flow_dir, 'adverse') && r.flow_mag_mps <= 1.8 + 1e-9 ...
                && r.ht_ratio >= 1.5 - 1e-9
            inCand = true;
        end
        if strcmp(r.flow_dir, 'following') && r.flow_mag_mps <= 1.5 + 1e-9 ...
                && r.ht_ratio >= 1.5 - 1e-9
            inCand = true;
        end
        if inCand
            infeas{end+1} = struct('key', r.key, ...
                'sog_plan_mps', r.sog_plan_mps, 'flow_dir', r.flow_dir, ...
                'flow_mag_mps', r.flow_mag_mps, 'ht_ratio', r.ht_ratio, ...
                'sim_completed', r.sim_completed, ...
                'failed_criteria', failedCrit(r)); %#ok<AGROW>
        end
    end
    s.candidate_interior_infeasible = infeas;
    s.candidate_interior_infeasible_n = numel(infeas);
    doc.summary = s;
end

function sel = records2sel(records, dirn)
    sel = cellfun(@(r) strcmp(r.flow_dir, dirn), records);
end

function rows = axisView(records, fld)
    rows = {};
    if isempty(records)
        return;
    end
    vals = uniqueSort(cell2mat(cellfun(@(r) r.(fld), records, ...
        'UniformOutput', false)));
    for i = 1:numel(vals)
        sel = cellfun(@(r) abs(r.(fld) - vals(i)) < 1e-6, records);
        nSel = sum(sel);
        nFeas = sum(cellfun(@(r) r.feasible, records(sel)));
        rows{end+1} = struct('value', vals(i), 'n_points', nSel, ...
            'n_feasible', nFeas); %#ok<AGROW>
    end
end

function cc = critCounts(records)
    names = {'sim_completed', 'no_safety_trip', 'no_sop_saturation', ...
        'no_domain_warning', 'speed_achieved'};
    cc = struct();
    for i = 1:numel(names)
        cc.(names{i}) = sum(cellfun(@(r) ~hasTrue(r, names{i}), records));
    end
end

function tf = hasTrue(r, fld)
    if isfield(r, fld)
        v = r.(fld);
        tf = ~isempty(v) && islogical(v) && v;
    else
        tf = false;
    end
end

function fc = failedCrit(r)
    fc = {};
    if ~r.sim_completed
        fc{end+1} = 'sim_completed'; %#ok<AGROW>
        return;
    end
    if ~hasTrue(r, 'no_safety_trip'),    fc{end+1} = 'no_safety_trip';    end %#ok<AGROW>
    if ~hasTrue(r, 'no_sop_saturation'), fc{end+1} = 'no_sop_saturation'; end %#ok<AGROW>
    if ~hasTrue(r, 'no_domain_warning'), fc{end+1} = 'no_domain_warning'; end %#ok<AGROW>
    if ~hasTrue(r, 'speed_achieved'),    fc{end+1} = 'speed_achieved';    end %#ok<AGROW>
end

% =========================================================================
function gc = g2CrossCheck(ROOT, records)
    gc = struct();
    ref = g2Reference(ROOT);
    gc.reference_source = ref.source;
    gc.reference = ref.block;

    % 1) sprint reachability at SOG = 7.522 (14.62 kn = G2 full-power band)
    rS = findRec(records, 7.522, 'adverse', 0.0, 3.0);
    if isempty(rS)
        rS = findRecAny(records, 'sog_plan_mps', 7.522);
    end
    if ~isempty(rS) && ~isempty(ref.full_power_kn)
        gc.sprint_reachability = struct( ...
            'point', rS.key, ...
            'sog_mean_last120_mps', gv(rS, 'sog_mean_last120_mps'), ...
            'speed_achieved', gv(rS, 'speed_achieved'), ...
            'g2_full_power_speed_band_kn', ref.full_power_kn, ...
            'direction_consistent', gv(rS, 'speed_achieved'));
    else
        gc.sprint_reachability = struct('point', '', ...
            'direction_consistent', []);
    end

    % 2) adverse vs following power direction (G2 U_against_up_P_prop_up)
    pairs = {};
    for s = [4.0, 6.0]
        for u = [1.8, 2.0]
            ra = findRec(records, s, 'adverse', u, 1.5);
            rf = findRec(records, s, 'following', u, 1.5);
            if isempty(ra) || isempty(rf)
                continue;
            end
            pa = gv(ra, 'P_prop_mean_last120_kW');
            pf = gv(rf, 'P_prop_mean_last120_kW');
            ok = [];
            if ~isempty(pa) && ~isempty(pf)
                ok = (pa > pf);
            end
            pairs{end+1} = struct('sog_mps', s, 'flow_mag_mps', u, ...
                'P_adverse_kW', pa, 'P_following_kW', pf, ...
                'adverse_heavier', ok); %#ok<AGROW>
        end
    end
    gc.adverse_vs_following_power = pairs;

    % 3) shallow resistance monotonicity at zero adverse flow
    mono = {};
    sogs = uniqueSort(catVals(records, 'sog_plan_mps'));
    for s = sogs
        htsHere = [];
        rMean = [];
        for h = [3.0 2.0 1.5 1.35 1.2]
            r0 = findRec(records, s, 'adverse', 0.0, h);
            if ~isempty(r0)
                rv = gv(r0, 'R_hull_mean_last120_N');
                if ~isempty(rv)
                    htsHere(end+1) = h; %#ok<AGROW>
                    rMean(end+1) = rv; %#ok<AGROW>
                end
            end
        end
        if numel(rMean) >= 2
            d = diff(rMean);            % H/T descending -> R ascending
            ok = all(d > 0);
            mono{end+1} = struct('sog_mps', s, ...
                'ht_descending', htsHere, ...
                'R_hull_mean_N_ascending_ht_desc', rMean, ...
                'monotone_ok', ok); %#ok<AGROW>
        end
    end
    gc.shallow_R_monotonicity_adverse0 = mono;
end

function ref = g2Reference(ROOT)
    ref = struct('source', '', 'block', [], 'full_power_kn', []);
    p = fullfile(ROOT, 'results', 'gate_result.json');
    if ~exist(p, 'file')
        return;
    end
    try
        S = jsondecode(fileread(p));
        g2 = S.checks.G2;
        ref.source = 'results/gate_result.json checks.G2';
        ref.block = struct('name', g2.name, 'pass', g2.pass);
        ck = g2.checks;   % jsondecode yields cell for heterogeneous records
        for i = 1:numel(ck)
            if iscell(ck)
                it = ck{i};
            else
                it = ck(i);
            end
            if strcmp(it.item, 'full_power_speed_band_12.5_15.5_kn') ...
                    && isfield(it, 'value_kn')
                ref.full_power_kn = it.value_kn;
            end
        end
    catch
        ref = struct('source', 'unreadable', 'block', [], 'full_power_kn', []);
    end
end

function r = findRec(records, s, dirn, u, h)
    r = [];
    for i = 1:numel(records)
        ri = records{i};
        if abs(ri.sog_plan_mps - s) < 1e-6 && strcmp(ri.flow_dir, dirn) ...
                && abs(ri.flow_mag_mps - u) < 1e-6 && abs(ri.ht_ratio - h) < 1e-6
            r = ri;
            return;
        end
    end
end

function r = findRecAny(records, fld, val)
    r = [];
    for i = 1:numel(records)
        if abs(records{i}.(fld) - val) < 1e-6
            r = records{i};
            return;
        end
    end
end

function v = gv(r, fld)
    v = [];
    if isfield(r, fld)
        v = r.(fld);
    end
    if islogical(v)
        v = double(v);
    end
end
