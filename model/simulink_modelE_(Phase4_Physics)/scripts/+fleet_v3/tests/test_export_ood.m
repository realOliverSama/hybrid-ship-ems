%% test_export_ood.m - fleet_v3 OOD meta extension round-trip suite
% (T-018 Phase 4-D): the FORMAL bridge through the untouched
% producer_export face followed by the fleet_v3.export_ood rewrite, on a
% synthetic artifact.  Exercises the full schema/FR-500/FR-536 checks on
% the bridge and verifies the persisted OOD meta.  Failures raise
% GATE:FLEETV3:TEST:EXPORTOOD.
% Discipline: ASCII-only; no absolute paths (tempdir artifacts).

ra_root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ra_root) || ~exist(fullfile(ra_root, 'config'), 'dir')
    ra_root = pwd;
end
ra_act = fullfile(fileparts(fileparts(ra_root)), 'docs', 'phase4', ...
    'd_dataset_v3', 't432_activation_record.json');
ra_plan = fleet_v3.plan_load(ra_act, 'stage_a');

addpath(fullfile(ra_root, 'scripts'));

% ---- synthetic artifact content: a 1200 s fully covered voyage ----
ra_n = 1201;
ra_stream = struct();
ra_stream.time_s = (0:ra_n-1)';
% every schema-required truth_stream column (machine schema, T-020
% local_stream_map source of truth)
ra_stream.P_load_total_kW = zeros(ra_n, 1);
ra_stream.v_ship_mps = zeros(ra_n, 1);
ra_stream.acceleration_mps2 = zeros(ra_n, 1);
ra_stream.n_prop_rpm = zeros(ra_n, 1);
ra_stream.P_hotel_kW = zeros(ra_n, 1);
ra_stream.v_current_mps = zeros(ra_n, 1);
ra_stream.water_depth_m = zeros(ra_n, 1);
ra_stream.P_prop_demand_kW = zeros(ra_n, 1);
ra_stream.v_ref_mps = zeros(ra_n, 1);
ra_stream.manual_power_cmd_pct = zeros(ra_n, 1);
ra_stream.P_gen_actual_kW = zeros(ra_n, 1);
% full registry INPUT_H2 retention face (AMENDMENT_02: truth_stream
% fields 15 -> 24; the 9 previously schema-missing H2 columns, registry
% order, inserted after P_gen_actual_kW)
ra_stream.P_bat_actual_kW = zeros(ra_n, 1);
ra_stream.SOC = zeros(ra_n, 1);
ra_stream.V_dc_V = zeros(ra_n, 1);
ra_stream.state_DG1 = zeros(ra_n, 1);
ra_stream.state_DG2 = zeros(ra_n, 1);
ra_stream.V_bat_V = zeros(ra_n, 1);
ra_stream.I_bat_A = zeros(ra_n, 1);
ra_stream.P_bat_dis_avail_kW = zeros(ra_n, 1);
ra_stream.P_bat_chg_avail_kW = zeros(ra_n, 1);
ra_stream.resistance_multiplier = zeros(ra_n, 1);
ra_stream.ctrl_mode = zeros(ra_n, 1);
ra_stream.scenario_id = 'FLTO_TEST';

ra_dur = 1200;
ra_anchors = (300:10:(ra_dur - 300))';
ra_m = numel(ra_anchors);
ra_kf = struct();
ra_kf.issue_time_s = ra_anchors;
ra_kf.lead_s = repmat((1:30) * 10, ra_m, 1);
ra_kf.v_plan_mps = zeros(ra_m, 30);
ra_kf.s_plan_m = zeros(ra_m, 30);
ra_kf.h_chart_m = zeros(ra_m, 30);
ra_kf.P_hotel_plan_kW = zeros(ra_m, 30);
ra_kf.u_fcst_mps = zeros(ra_m, 30);
ra_kf.wl_fcst_m = zeros(ra_m, 30);

ra_voy = struct('duration_s', ra_dur, 'kf_issue_time_s', ra_anchors, ...
    'kf_row_ok', true(ra_m, 1));
ra_elig = mission_v3.eligibility('judge', ra_voy, ...
    'MinDurationS', ra_plan.qual.min_duration_s, ...
    'MinCanonicalWindows', ra_plan.qual.min_canonical_windows);
assert(strcmp(char(ra_elig.disposition), 'ACCEPT'), ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'fixture voyage must qualify');

ra_meta = struct();
ra_meta.voyage_id = 'FLTO_TEST';
ra_meta.voyage_family_id = 'FLTO_TEST';
ra_meta.route_template_id = 'ROUTINE_PATROL';
ra_meta.mission_template_id = 'ROUTINE_PATROL';
ra_meta.split_group_id = 'SG#TEST';
ra_meta.scenario_stratum = 'STRAT_ROUTINE_PATROL__FLOOD';
ra_meta.generation_seed = 1;
ra_meta.model_baseline_ref = 'ModelE physical v1.1 + control v1.0';
ra_meta.producer_script_sha = '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef';
ra_meta.fixture_kind = 'NONE';
ra_meta.ood_axis = 'A';
ra_meta.ood_def_version = 'DGN-11-v1';
ra_meta.physics_domain_ref = 'ModelE validated domain (T-412 v1.1)';
ra_meta.h_chart_m = 6.0;
ra_meta.speed_limit_zones = [];
ra_meta.bridge_zones = [];
ra_meta.voyage_duration_s = ra_dur;
ra_meta.scenario_id = 'FLTO_TEST';
ra_meta.forecast_provenance = struct( ...
    'forecast_generator_id', 'gen', 'forecast_generator_version', 'v1', ...
    'forecast_generator_hash', repmat('a', 1, 64), ...
    'issue_time_s', 0.0, 'source_cutoff_time_s', 0.0, ...
    'forecast_provenance_class', 'CAUSAL_GENERATOR');
ra_meta.base_draw_id = 'seed#0';
ra_meta.intervention = 'traffic=ON;forecast=GOOD_V1';
ra_meta.forecast_quality_profile_id = 'GOOD_V1';
ra_meta.stratum = 'STRAT_ROUTINE_PATROL__FLOOD';
ra_meta.dataset_domain = 'FORMAL';

ra_tmp = fullfile(tempdir, 'fleet_v3_test_ood', 'FLTO_TEST.mat');
if exist(fullfile(tempdir, 'fleet_v3_test_ood'), 'dir')
    delete(fullfile(tempdir, 'fleet_v3_test_ood', '*.mat'));
else
    mkdir(fullfile(tempdir, 'fleet_v3_test_ood'));
end

% ---- bridge export through the untouched producer_export face --------
ra_res = mission_v3.producer_export('export_artifact', ra_stream, ...
    ra_kf, ra_meta, ra_elig, ra_tmp);
assert(ra_res.ok, 'GATE:FLEETV3:TEST:EXPORTOOD', ...
    'bridge export failed');

ra_s = load(ra_tmp);
assert(strcmp(char(ra_s.meta.dataset_domain), 'FORMAL'), ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'bridge domain must be FORMAL');

% ---- OOD rewrite ----
ra_extra = struct('ood_axis', 'A', 'ood_condition_id', 'A1', ...
    'ood_token_id', '', ...
    'heldout_token_registry_sha', ra_plan.ood.registry_sha, ...
    'ood_judge_reason', 'numerical_band_membership', ...
    'propulsion', struct('feasible', true, 'p_demand_kW', 649.5));
ra_res2 = fleet_v3.export_ood(ra_tmp, ra_extra);
assert(strcmp(ra_res2.dataset_domain, 'OOD') && ra_res2.roundtrip_ok, ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'OOD rewrite failed');

ra_s2 = load(ra_tmp);
assert(strcmp(char(ra_s2.meta.dataset_domain), 'OOD'), ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'dataset_domain did not persist');
assert(strcmp(char(ra_s2.meta.ood_axis), 'A') ...
    && strcmp(char(ra_s2.meta.ood_condition_id), 'A1'), ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'OOD axis/condition did not persist');
assert(strcmp(char(ra_s2.meta.heldout_token_registry_sha), ...
    ra_plan.ood.registry_sha), ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'registry sha did not persist');
assert(isfield(ra_s2.meta, 'propulsion_check'), ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'propulsion evidence did not persist');
assert(isequal(ra_s2.stream, ra_stream) && isequal(ra_s2.kf, ra_kf), ...
    'GATE:FLEETV3:TEST:EXPORTOOD', 'stream/kf payload changed in rewrite');

% ---- negative: missing ood_extra field fails closed ----
ra_threw = false;
try
    fleet_v3.export_ood(ra_tmp, struct('ood_axis', 'A'));
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:EXPORT:ARG');
end
assert(ra_threw, 'GATE:FLEETV3:TEST:EXPORTOOD', ...
    'incomplete ood_extra did not fail closed');

% stage_check face: the OOD domain is outside the FR-500 enum on purpose
% (the rewrite path exists precisely because the frozen face validates
%  {FORMAL, PILOT} only) - assert that ordering is respected
ra_threw = false;
try
    mission_v3.producer_export('stage_check', ra_s2.meta);
catch ME
    ra_threw = ~isempty(strfind(char(ME.message), 'dataset_domain'));
end
assert(ra_threw, 'GATE:FLEETV3:TEST:EXPORTOOD', ...
    'producer_export face must stay untouched (OOD not accepted there)');

fprintf('test_export_ood: PASS (bridge + rewrite + persistence + fail-closed + untouched face)\n');
