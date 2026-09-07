%% test_predicate_eval.m - fleet_v3 canonical predicate evaluation suite
% (T-018 Phase 4-D; D4 registry-driven token consumption).  Tokens are
% loaded from the FROZEN registry (never hand-copied); positive/negative
% paths and the field whitelist guard are exercised on synthetic
% realized facts.  Failures raise GATE:FLEETV3:TEST:PREDICATE.
% Discipline: ASCII-only; no absolute paths.

ra_root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ra_root) || ~exist(fullfile(ra_root, 'config'), 'dir')
    ra_root = pwd;
end
ra_reg = jsondecode(fileread(fullfile(ra_root, 'config', 'mission_v3', ...
    'heldout_token_registry.json')));
ra_toks = ra_reg.tokens;
ra_by_id = struct();
for ra_i = 1:numel(ra_toks)
    ra_by_id.(char(ra_toks(ra_i).token_id)) = ra_toks(ra_i);
end

ra_c1 = ra_by_id.('C1_BRIDGE_X_HIGH_ID_ADVERSE_CURRENT');
ra_c2 = ra_by_id.('C2_HARBOR_X_LOW_ID_HT');
ra_c3 = ra_by_id.('C3_EMERGENCY_SPRINT_X_LOW_ID_HT');

% ---- C1 positive: bridge segment with adverse sup inside [1.4, 1.8] ----
ra_f = local_facts({'departure', 'bridge', 'return'}, ...
    [0.5, 1.6, 0.9], 2.6);
ra_r = fleet_v3.predicate_eval(ra_c1, ra_f);
assert(ra_r.triggered && ra_r.evidence.segment_index == 2 ...
    && abs(ra_r.evidence.value - 1.6) < 1e-12, ...
    'GATE:FLEETV3:TEST:PREDICATE', 'C1 positive path failed');

% ---- C1 band endpoints inclusive ----
ra_r = fleet_v3.predicate_eval(ra_c1, local_facts({'bridge'}, 1.4, 9.9));
assert(ra_r.triggered, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'C1 lower endpoint 1.4 must be inclusive');
ra_r = fleet_v3.predicate_eval(ra_c1, local_facts({'bridge'}, 1.8, 9.9));
assert(ra_r.triggered, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'C1 upper endpoint 1.8 must be inclusive');

% ---- C1 negatives: wrong role / out-of-band value ----
ra_r = fleet_v3.predicate_eval(ra_c1, local_facts({'harbor'}, 1.6, 9.9));
assert(~ra_r.triggered, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'C1 must not trigger on a non-bridge segment');
ra_r = fleet_v3.predicate_eval(ra_c1, local_facts({'bridge'}, 1.3, 9.9));
assert(~ra_r.triggered, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'C1 must not trigger below the band');
ra_r = fleet_v3.predicate_eval(ra_c1, local_facts({'bridge'}, 1.85, 9.9));
assert(~ra_r.triggered, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'C1 must not trigger above the band');

% ---- C2/C3 positive: voyage-scalar ht factor + role gate ----
ra_r = fleet_v3.predicate_eval(ra_c2, local_facts({'harbor', 'return'}, ...
    [0.3, 0.4], 1.7));
assert(ra_r.triggered && ra_r.evidence.segment_index == 1 ...
    && abs(ra_r.evidence.value - 1.7) < 1e-12, ...
    'GATE:FLEETV3:TEST:PREDICATE', 'C2 positive path failed');
ra_r = fleet_v3.predicate_eval(ra_c3, local_facts({'standby', 'rush'}, ...
    [0.0, 0.2], 1.7));
assert(ra_r.triggered && ra_r.evidence.segment_index == 2, ...
    'GATE:FLEETV3:TEST:PREDICATE', 'C3 positive path failed');

% ---- C2/C3 negatives: role present but ht out of [1.5, 2.0] ----
ra_r = fleet_v3.predicate_eval(ra_c2, local_facts({'harbor'}, 0.3, 1.45));
assert(~ra_r.triggered, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'C2 must not trigger below the ht band');
ra_r = fleet_v3.predicate_eval(ra_c2, local_facts({'harbor'}, 0.3, 2.05));
assert(~ra_r.triggered, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'C2 must not trigger above the ht band');

% ---- whitelist guard: a non-whitelisted predicate field fails closed --
ra_bad = ra_c1;
ra_bad.predicate = [ ...
    struct('field', 'segment.role', 'operator', '==', 'value', 'bridge'), ...
    struct('field', 'segment.u_signed_mean', 'operator', 'in_band', ...
    'value', 'HIGH_ID_BAND')];
ra_threw = false;
try
    fleet_v3.predicate_eval(ra_bad, local_facts({'bridge'}, 1.6, 2.6));
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:PRED:FIELD');
end
assert(ra_threw, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'non-whitelisted predicate field did not fail closed');

% ---- grammar guard: clause 1 must stay the role equality --------------
ra_bad2 = ra_c1;
ra_bad2.predicate = [ ...
    struct('field', 'voyage.ht_effective', 'operator', 'in_band', ...
    'value', 'HIGH_ID_BAND'), ...
    struct('field', 'segment.u_adverse_abs', 'operator', 'in_band', ...
    'value', 'HIGH_ID_BAND')];
ra_threw = false;
try
    fleet_v3.predicate_eval(ra_bad2, local_facts({'bridge'}, 1.6, 2.6));
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:PRED:GRAMMAR');
end
assert(ra_threw, 'GATE:FLEETV3:TEST:PREDICATE', ...
    'reordered predicate grammar did not fail closed');

fprintf('test_predicate_eval: PASS (registry tokens C1/C2/C3 positive+negative+whitelist+grammar)\n');

function f = local_facts(roles, adv, ht)
    f = struct('segment_roles', {roles}, ...
        'seg_u_adverse_abs', adv, 'ht_effective', ht);
end
