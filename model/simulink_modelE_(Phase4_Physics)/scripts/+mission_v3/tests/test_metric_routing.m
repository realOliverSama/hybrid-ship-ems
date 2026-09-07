% test_metric_routing.m - T-022 v2 EXECFIX-01 C4: registry metric-profile
%   routing unit test (structural conformance; T429-AMENDMENT-V2 fix3a).
%
%   Proves that the identifiability evaluator routes each contrast's
%   metric profile from the frozen contrast REGISTRY ROW FIELDS only
%   (SBD_required / paired_physical_effect_required / gate_profile /
%   paired_effect_profile) and never from the contrast id:
%
%   M1  STANDARD_SHAPE mock row (SBD_required=true) routes to
%       metric_profile=STANDARD_SHAPE, judges {sbd, probe},
%       sbd_gate_bearing=true.
%   M2  PAIRED_LEVEL_SHIFT mock row (SBD_required=false,
%       paired_physical_effect_required=true, frozen gate_profile
%       sentence, sbd_role, paired_effect_profile) routes to
%       metric_profile=PAIRED_LEVEL_SHIFT, judges
%       {paired_physical_effect, probe}, sbd_gate_bearing=false.
%   M3  NO contrast-id hard-coding: the SAME field sets routed under
%       different synthetic contrast ids produce identical routing
%       decisions (contrast_id is only echoed back), and each mock row
%       keeps its routing when given the REAL registry id of the
%       opposite profile (DIR id on a STANDARD_SHAPE row must stay
%       STANDARD_SHAPE; a non-DIR id on a PAIRED_LEVEL_SHIFT row must
%       stay PAIRED_LEVEL_SHIFT).
%   M4  Fail-closed negative injections (each must raise the expected
%       gate id):
%       N1  SBD_required=false without paired_physical_effect_required
%             -> GATE:MISSION_V3:T429:REGISTRY
%       N2  both flags false
%             -> GATE:MISSION_V3:T429:REGISTRY
%       N3  SBD_required missing
%             -> GATE:MISSION_V3:T429:REGISTRY
%       N4  gate_profile sentence without the AND-probe declaration
%             -> GATE:MISSION_V3:T429:CONFORMANCE
%       N5  gate_profile missing entirely
%             -> GATE:MISSION_V3:T429:CONFORMANCE
%   M5  Live-registry smoke (whole frozen registry, all rows route,
%       no error): every row with SBD_required==false AND
%       paired_physical_effect_required==true routes to
%       PAIRED_LEVEL_SHIFT with
%       sbd_role==SECONDARY_DIAGNOSTIC_NOT_APPLICABLE_TO_LEVEL_SHIFT_FACTOR,
%       every other row routes to STANDARD_SHAPE with
%       sbd_gate_bearing=true; exactly one REQUIRED row carries the
%       paired-effect fields.  The assertions below key on the ROUTED
%       PROFILE values, never on contrast ids.
%
%   The mock rows are in-memory structs only (never written to disk).
%   PASS summary on stdout (ALL PASS (n/n)); any failure raises
%   error('GATE:test_metric_routing ROUTING: ...').
%
%   Discipline: ASCII-only source; no absolute paths; no chained
%   indexing; no global RNG; no local functions (script suite style
%   with inline case blocks, matching run_all's run(...) execution);
%   ROOT via mfilename (4 x fileparts) with pwd fallback (workspace
%   AGENTS.md 20.2).

mr_root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(mr_root) || exist(fullfile(mr_root, 'config'), 'dir') ~= 7
    mr_root = pwd;
end
addpath(fullfile(mr_root, 'scripts'));

mr_n_pass = 0;
mr_n_total = 0;
mr_failed = cell(1, 0);

% ---------------- mock rows (in-memory only) --------------------------
mr_std = struct();
mr_std.contrast_id = 'MOCK_SYNTHETIC_STANDARD';
mr_std.factor_axis = 'segment_role';
mr_std.class_A = 'alpha';
mr_std.class_B = 'beta';
mr_std.cohort_predicate = [];
mr_std.SBD_required = true;
mr_std.probe_required = true;
mr_std.minimum_group_coverage = 10;
mr_std.gate_role = 'REQUIRED';

mr_pair = struct();
mr_pair.contrast_id = 'MOCK_SYNTHETIC_PAIRED';
mr_pair.factor_axis = 'direction';
mr_pair.class_A = 'up';
mr_pair.class_B = 'down';
mr_pair.cohort_predicate = [];
mr_pair.SBD_required = false;
mr_pair.probe_required = true;
mr_pair.minimum_group_coverage = 10;
mr_pair.gate_role = 'REQUIRED';
mr_pair.sbd_role = 'SECONDARY_DIAGNOSTIC_NOT_APPLICABLE_TO_LEVEL_SHIFT_FACTOR';
mr_pair.paired_physical_effect_required = true;
mr_pair.paired_effect_profile = struct( ...
    'cohort_unit', 'same base_draw / split_group directional pair', ...
    'aggregation', 'group-level matched pure slices first', ...
    'sign_requirement', 'upstream mean P_load > return mean', ...
    'ci_rule', 'group bootstrap 95% CI must exclude zero', ...
    'sign_test_alpha', 0.05, ...
    'magnitude_threshold_policy', 'none');
mr_pair.gate_profile = ['PASS_MOCK = PASS_paired_physical_effect AND ', ...
    'PASS_probe (both required)'];

% ---------------- M1 / M2: base routing -------------------------------
mr_n_total = mr_n_total + 1;
r1 = mission_v3.audits.identifiability('route_metric', mr_std);
mr_ok = strcmp(r1.metric_profile, 'STANDARD_SHAPE') ...
    && r1.sbd_gate_bearing ...
    && numel(r1.judges) == 2 ...
    && strcmp(r1.judges{1}, 'sbd') ...
    && strcmp(r1.judges{2}, 'probe');
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf('[test_metric_routing] PASS M1 STANDARD_SHAPE routes SBD+probe\n');
else
    mr_failed{end+1} = 'M1'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M1\n');
end

mr_n_total = mr_n_total + 1;
r2 = mission_v3.audits.identifiability('route_metric', mr_pair);
mr_ok = strcmp(r2.metric_profile, 'PAIRED_LEVEL_SHIFT') ...
    && ~r2.sbd_gate_bearing ...
    && numel(r2.judges) == 2 ...
    && strcmp(r2.judges{1}, 'paired_physical_effect') ...
    && strcmp(r2.judges{2}, 'probe') ...
    && strcmp(r2.sbd_role, ...
    'SECONDARY_DIAGNOSTIC_NOT_APPLICABLE_TO_LEVEL_SHIFT_FACTOR');
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M2 PAIRED_LEVEL_SHIFT routes ', ...
        'paired_effect+probe\n']);
else
    mr_failed{end+1} = 'M2'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M2\n');
end

% ---------------- M3: contrast-id is never a routing input ------------
mr_n_total = mr_n_total + 1;
mr_std_b = mr_std;
mr_std_b.contrast_id = 'ZZZ_OTHER_SYNTHETIC_ID';
r1b = mission_v3.audits.identifiability('route_metric', mr_std_b);
mr_std_c = mr_std;
mr_std_c.contrast_id = 'DIR_UPSTREAM_VS_DOWNSTREAM';
r1c = mission_v3.audits.identifiability('route_metric', mr_std_c);
r1_echo = rmfield(r1, 'contrast_id');
r1b_echo = rmfield(r1b, 'contrast_id');
r1c_echo = rmfield(r1c, 'contrast_id');
mr_ok = isequal(r1_echo, r1b_echo) && isequal(r1_echo, r1c_echo);
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M3a identical fields under ', ...
        'different ids route identically\n']);
else
    mr_failed{end+1} = 'M3a'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M3a\n');
end

mr_n_total = mr_n_total + 1;
mr_pair_b = mr_pair;
mr_pair_b.contrast_id = 'SEG_BERTH_VS_DEPARTURE';
r2b = mission_v3.audits.identifiability('route_metric', mr_pair_b);
mr_pair_c = mr_pair;
mr_pair_c.contrast_id = 'SOME_SECONDARY_DIAGNOSTIC_ROW';
r2c = mission_v3.audits.identifiability('route_metric', mr_pair_c);
mr_ok = strcmp(r2b.metric_profile, 'PAIRED_LEVEL_SHIFT') ...
    && strcmp(r2c.metric_profile, 'PAIRED_LEVEL_SHIFT');
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M3b paired fields keep ', ...
        'routing under non-DIR ids\n']);
else
    mr_failed{end+1} = 'M3b'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M3b\n');
end

% ---------------- M4: fail-closed negatives ---------------------------
mr_n_total = mr_n_total + 1;
n1 = struct();
n1.contrast_id = 'MOCK_NEG_1';
n1.SBD_required = false;
n1.minimum_group_coverage = 10;
n1.gate_role = 'REQUIRED';
mr_ok = false;
try
    mission_v3.audits.identifiability('route_metric', n1);
catch mr_err
    mr_ok = strcmp(char(string(mr_err.identifier)), ...
        'GATE:MISSION_V3:T429:REGISTRY');
end
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M4/N1 SBD_required=false ', ...
        'without paired fields -> REGISTRY\n']);
else
    mr_failed{end+1} = 'N1'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M4/N1\n');
end

mr_n_total = mr_n_total + 1;
n2 = struct();
n2.contrast_id = 'MOCK_NEG_2';
n2.SBD_required = false;
n2.paired_physical_effect_required = false;
n2.minimum_group_coverage = 10;
n2.gate_role = 'REQUIRED';
mr_ok = false;
try
    mission_v3.audits.identifiability('route_metric', n2);
catch mr_err
    mr_ok = strcmp(char(string(mr_err.identifier)), ...
        'GATE:MISSION_V3:T429:REGISTRY');
end
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf('[test_metric_routing] PASS M4/N2 both flags false -> REGISTRY\n');
else
    mr_failed{end+1} = 'N2'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M4/N2\n');
end

mr_n_total = mr_n_total + 1;
n3 = struct();
n3.contrast_id = 'MOCK_NEG_3';
n3.minimum_group_coverage = 10;
n3.gate_role = 'REQUIRED';
mr_ok = false;
try
    mission_v3.audits.identifiability('route_metric', n3);
catch mr_err
    mr_ok = strcmp(char(string(mr_err.identifier)), ...
        'GATE:MISSION_V3:T429:REGISTRY');
end
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M4/N3 SBD_required missing ', ...
        '-> REGISTRY\n']);
else
    mr_failed{end+1} = 'N3'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M4/N3\n');
end

mr_n_total = mr_n_total + 1;
n4 = mr_pair;
n4.gate_profile = 'PASS_MOCK = PASS_paired_physical_effect only';
mr_ok = false;
try
    mission_v3.audits.identifiability('route_metric', n4);
catch mr_err
    mr_ok = strcmp(char(string(mr_err.identifier)), ...
        'GATE:MISSION_V3:T429:CONFORMANCE');
end
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M4/N4 gate_profile without ', ...
        'AND-probe -> CONFORMANCE\n']);
else
    mr_failed{end+1} = 'N4'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M4/N4\n');
end

mr_n_total = mr_n_total + 1;
n5 = rmfield(mr_pair, 'gate_profile');
mr_ok = false;
try
    mission_v3.audits.identifiability('route_metric', n5);
catch mr_err
    mr_ok = strcmp(char(string(mr_err.identifier)), ...
        'GATE:MISSION_V3:T429:CONFORMANCE');
end
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M4/N5 gate_profile missing ', ...
        '-> CONFORMANCE\n']);
else
    mr_failed{end+1} = 'N5'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M4/N5\n');
end

% ---------------- M5: live frozen registry smoke ----------------------
mr_n_total = mr_n_total + 1;
reg_path = fullfile(mr_root, 'config', 'mission_v3', ...
    'identifiability_contrast_registry.json');
reg = jsondecode(fileread(reg_path));
rows = {};
if isstruct(reg.contrasts)
    for k = 1:numel(reg.contrasts)
        rows{end+1} = reg.contrasts(k); %#ok<AGROW>
    end
elseif iscell(reg.contrasts)
    for k = 1:numel(reg.contrasts)
        if isstruct(reg.contrasts{k})
            rows{end+1} = reg.contrasts{k}; %#ok<AGROW>
        end
    end
end
mr_all_route = true;
mr_n_paired = 0;
mr_n_paired_required = 0;
mr_n_std = 0;
mr_paired_role_ok = true;
for k = 1:numel(rows)
    rk = mission_v3.audits.identifiability('route_metric', rows{k});
    if ~isfield(rk, 'metric_profile')
        mr_all_route = false;
        break;
    end
    if strcmp(rk.metric_profile, 'PAIRED_LEVEL_SHIFT')
        mr_n_paired = mr_n_paired + 1;
        if strcmpi(char(rows{k}.gate_role), 'REQUIRED')
            mr_n_paired_required = mr_n_paired_required + 1;
        end
        if ~strcmp(rk.sbd_role, ...
            'SECONDARY_DIAGNOSTIC_NOT_APPLICABLE_TO_LEVEL_SHIFT_FACTOR')
            mr_paired_role_ok = false;
        end
        if logical(rows{k}.SBD_required)
            mr_all_route = false;
        end
    else
        mr_n_std = mr_n_std + 1;
        if ~rk.sbd_gate_bearing
            mr_all_route = false;
        end
    end
end
mr_ok = mr_all_route ...
    && numel(rows) == mr_n_paired + mr_n_std ...
    && numel(rows) > 0;
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M5a every live registry row ', ...
        'routes without error (%d rows: %d paired-effect / %d ', ...
        'standard-shape)\n'], numel(rows), mr_n_paired, mr_n_std);
else
    mr_failed{end+1} = 'M5a'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M5a\n');
end

mr_n_total = mr_n_total + 1;
mr_ok = mr_n_paired == 1 && mr_n_paired_required == 1;
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M5b exactly one REQUIRED row ', ...
        'routes paired-effect\n']);
else
    mr_failed{end+1} = 'M5b'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M5b\n');
end

mr_n_total = mr_n_total + 1;
mr_ok = mr_paired_role_ok;
if mr_ok
    mr_n_pass = mr_n_pass + 1;
    fprintf(['[test_metric_routing] PASS M5c paired-effect sbd_role is ', ...
        'the frozen secondary sentence\n']);
else
    mr_failed{end+1} = 'M5c'; %#ok<AGROW>
    fprintf('[test_metric_routing] FAIL M5c\n');
end

% ---------------- summary ---------------------------------------------
fprintf('[test_metric_routing] cases %d/%d passed\n', mr_n_pass, ...
    mr_n_total);
if mr_n_pass ~= mr_n_total
    error('GATE:test_metric_routing ROUTING: %d/%d case(s) failed: %s', ...
        numel(mr_failed), mr_n_total, strjoin(mr_failed, ', '));
end
fprintf('ALL PASS (%d/%d)\n', mr_n_pass, mr_n_total);
