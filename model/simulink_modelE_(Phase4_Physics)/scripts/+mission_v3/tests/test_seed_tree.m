% test_seed_tree.m - T-008: mission_v3 scientific seed tree harness (DGN-07)
%   P1 (spec B.1) same lineage, same realization: identical lineage_id /
%       engine seed / branch_path across repeated derivation, and bitwise
%       identical Threefry4x64 draws from the derived engine seed.
%   P2 (spec B.2) branch independence: full policy registry has pairwise
%       distinct lineage_id and pairwise distinct engine seeds; verify passes
%       (also on the axes-extended registry); rebuild is struct-identical.
%   P3 (spec B.4) digest registry integrity: 64-char lowercase hex format,
%       uint32 engine-seed range, big-endian first-four-byte mapping, plus a
%       hand recomputation via java.security.MessageDigest on one sample and
%       a SHA-256("abc") known-vector check of the digest pipeline.
%   P4 (spec B.5) INFORMATION / PHYSICAL invariants: two INFORMATION members
%       share all truth branch lineages and differ exactly in their
%       forecast_obs_noise instances; two PHYSICAL members keep all 8
%       standard branches identical and differ only in intervention_<axis>
%       branches; member maps pass fail-closed verify.
%   N1 (spec B.3) tampered engine-seed duplicate  -> GATE:MISSION_V3:SEED:COLLISION
%   N2 (spec B.3) tampered lineage duplicate      -> GATE:MISSION_V3:SEED:*
%   N3 (fixed decision 4) missing required field  -> GATE:MISSION_V3:SEED:*
%   N4 (spec B.6) branch list comes from the frozen seed_tree_policy.json
%       (read independently here; never hard-coded)
%   Failures raise error('GATE:test_seed_tree ...'); the test writes nothing
%   to results/.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_seed_tree.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

FS = 'test_seed_tree_formal_seed_01';   % test fixture, not a frozen value
DRAW = 7;

pol = local_policy_probe(ROOT);
PV = pol.policy_version;
fprintf('[T] policy loaded: policy_version=%s, branches=%d\n', PV, numel(pol.branches));

reg = mission_v3.seed_tree('branches', FS, DRAW);

n_fail = 0;

%% P1 (B.1): same lineage -> same realization
ok_p = false;
try
    r1 = mission_v3.seed_tree('lineage', PV, FS, DRAW, 'water_level');
    r2 = mission_v3.seed_tree('lineage', PV, FS, DRAW, 'water_level');
    same_fields = strcmp(r1.lineage_id, r2.lineage_id) ...
        && (r1.engine_seed_uint32 == r2.engine_seed_uint32) ...
        && strcmp(r1.branch_path, r2.branch_path);
    rs1 = RandStream('Threefry4x64', 'Seed', r1.engine_seed_uint32);
    x1 = rand(rs1, [1, 8]);
    rs2 = RandStream('Threefry4x64', 'Seed', r2.engine_seed_uint32);
    x2 = rand(rs2, [1, 8]);
    ok_p = same_fields && isequal(x1, x2) && any(x1 ~= 0);
catch p_err
    fprintf('[T] P1 exception: %s\n', p_err.message);
end
if ok_p
    fprintf('[T] P1 same lineage same realization: PASS\n');
end
n_fail = n_fail + ~ok_p;

%% P2 (B.2): pairwise distinct lineages and engine seeds; verify passes
ok_2 = false;
try
    reg_ax = mission_v3.seed_tree('branches', FS, DRAW, {'water_level'});
    reg_b = mission_v3.seed_tree('branches', FS, DRAW);
    n = numel(reg);
    lins = cell(1, n);
    seeds = zeros(1, n);
    for k = 1:n
        row = reg(k);
        lins{k} = row.lineage_id;
        seeds(k) = double(row.engine_seed_uint32);
    end
    ok_2 = (n >= 2) ...
        && (numel(unique(lins)) == n) ...
        && (numel(unique(seeds)) == n) ...
        && isequal(reg, reg_b);
    v1 = mission_v3.seed_tree('verify', reg);
    v2 = mission_v3.seed_tree('verify', reg_ax);
    ok_2 = ok_2 && v1.ok && (v1.n_entries == n) && v2.ok ...
        && (v2.n_entries == n + 1);
catch p2_err
    fprintf('[T] P2 exception: %s\n', p2_err.message);
end
if ok_2
    fprintf('[T] P2 pairwise independence + verify: PASS\n');
end
n_fail = n_fail + ~ok_2;

%% P3 (B.4): digest format, uint32 range, big-endian first four bytes
ok_3 = false;
try
    fmt_ok = true;
    for k = 1:numel(reg)
        row = reg(k);
        fmt_ok = fmt_ok ...
            && ~isempty(regexp(row.lineage_id, '^[0-9a-f]{64}$', 'once')) ...
            && isa(row.engine_seed_uint32, 'uint32');
    end
    % SHA-256 known vector pins the java digest pipeline itself
    abc_hex = local_sha256_hex(unicode2native('abc', 'UTF-8'));
    known_vec = strcmp(abc_hex, ...
        'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
    % hand recomputation of one sample branch via java MessageDigest
    sample = mission_v3.seed_tree('lineage', PV, FS, DRAW, 'water_level');
    s = [PV, '|', FS, '|', num2str(DRAW), '|', 'water_level'];
    dg = local_sha256_digest(unicode2native(s, 'UTF-8'));
    hand_hex = lower(reshape(dec2hex(dg, 2)', 1, []));
    hand_seed = uint32(double(dg(1)) * 16777216 + double(dg(2)) * 65536 ...
        + double(dg(3)) * 256 + double(dg(4)));
    ok_3 = fmt_ok && known_vec ...
        && strcmp(hand_hex, sample.lineage_id) ...
        && (hand_seed == sample.engine_seed_uint32);
catch p3_err
    fprintf('[T] P3 exception: %s\n', p3_err.message);
end
if ok_3
    fprintf('[T] P3 digest format + big-endian mapping: PASS\n');
end
n_fail = n_fail + ~ok_3;

%% P4 (B.5): INFORMATION / PHYSICAL member invariants
ok_4 = false;
try
    m1 = mission_v3.seed_tree('member_branches', FS, DRAW, 'INFORMATION', {'u_current'});
    m2 = mission_v3.seed_tree('member_branches', FS, DRAW, 'INFORMATION', {'water_level'});
    noise_branch = 'forecast_obs_noise';
    truth_names = pol.branches(~strcmp(pol.branches, noise_branch));
    info_ok = true;
    for k = 1:numel(truth_names)
        nm = truth_names{k};
        r1 = local_find_by_name(m1, nm);
        r2 = local_find_by_name(m2, nm);
        rs = local_find_by_name(reg, nm);
        info_ok = info_ok && ~isempty(r1) && ~isempty(r2) && ~isempty(rs) ...
            && strcmp(r1.lineage_id, rs.lineage_id) ...
            && strcmp(r2.lineage_id, rs.lineage_id);
    end
    n1 = local_find_by_name(m1, 'forecast_obs_noise@u_current');
    n2 = local_find_by_name(m2, 'forecast_obs_noise@water_level');
    ns = local_find_by_name(reg, noise_branch);
    info_ok = info_ok && ~isempty(n1) && ~isempty(n2) && ~isempty(ns) ...
        && ~strcmp(n1.lineage_id, n2.lineage_id) ...
        && ~strcmp(n1.lineage_id, ns.lineage_id) ...
        && ~strcmp(n2.lineage_id, ns.lineage_id);

    p1 = mission_v3.seed_tree('member_branches', FS, DRAW, 'PHYSICAL', {'traffic'});
    p2 = mission_v3.seed_tree('member_branches', FS, DRAW, 'PHYSICAL', {'water_level'});
    phys_ok = true;
    reg_lins = cell(1, numel(reg));
    for k = 1:numel(reg)
        rrow = reg(k);
        reg_lins{k} = rrow.lineage_id;
    end
    for k = 1:numel(reg)
        rrow = reg(k);
        q1 = local_find_by_name(p1, rrow.branch_name);
        q2 = local_find_by_name(p2, rrow.branch_name);
        phys_ok = phys_ok && ~isempty(q1) && ~isempty(q2) ...
            && strcmp(q1.lineage_id, rrow.lineage_id) ...
            && strcmp(q2.lineage_id, rrow.lineage_id);
    end
    i1 = local_intervention_rows(p1);
    i2 = local_intervention_rows(p2);
    phys_ok = phys_ok && (numel(i1) == 1) && (numel(i2) == 1) ...
        && ~strcmp(i1(1).lineage_id, i2(1).lineage_id) ...
        && ~ismember(i1(1).lineage_id, reg_lins) ...
        && ~ismember(i2(1).lineage_id, reg_lins) ...
        && ~isempty(strfind(i1(1).branch_name, 'intervention_traffic')) ...
        && ~isempty(strfind(i2(1).branch_name, 'intervention_water_level'));

    v3 = mission_v3.seed_tree('verify', m1);
    v4 = mission_v3.seed_tree('verify', p1);
    ok_4 = info_ok && phys_ok && v3.ok && v4.ok;
catch p4_err
    fprintf('[T] P4 exception: %s\n', p4_err.message);
end
if ok_4
    fprintf('[T] P4 INFORMATION/PHYSICAL invariants: PASS\n');
end
n_fail = n_fail + ~ok_4;

%% N1 (B.3): tampered engine-seed duplicate must be detected (COLLISION)
t1 = reg;
row_a = t1(2);
row_b = t1(3);
row_b.engine_seed_uint32 = row_a.engine_seed_uint32;
t1(3) = row_b;
[ok_n1, id_n1] = local_expect_error(@() mission_v3.seed_tree('verify', t1));
ok_n1 = ok_n1 && strcmp(char(id_n1), 'GATE:MISSION_V3:SEED:COLLISION');
fprintf('[T] N1 seed collision detected: %s (id=%s)\n', local_verdict(ok_n1), id_n1);
n_fail = n_fail + ~ok_n1;

%% N2 (B.3): tampered lineage duplicate must be detected
t2 = reg;
row_c = t2(4);
row_d = t2(5);
row_d.lineage_id = row_c.lineage_id;
t2(5) = row_d;
[ok_n2, id_n2] = local_expect_error(@() mission_v3.seed_tree('verify', t2));
ok_n2 = ok_n2 && startsWith(char(id_n2), 'GATE:MISSION_V3:SEED:');
fprintf('[T] N2 lineage duplicate detected: %s (id=%s)\n', local_verdict(ok_n2), id_n2);
n_fail = n_fail + ~ok_n2;

%% N3: missing required field must be detected
t3 = rmfield(reg, 'lineage_id');
[ok_n3, id_n3] = local_expect_error(@() mission_v3.seed_tree('verify', t3));
ok_n3 = ok_n3 && startsWith(char(id_n3), 'GATE:MISSION_V3:SEED:');
fprintf('[T] N3 missing field detected: %s (id=%s)\n', local_verdict(ok_n3), id_n3);
n_fail = n_fail + ~ok_n3;

%% N4 (B.6): branch list source = frozen policy file, not hard-coded
ok_n4 = false;
try
    reg_names = cell(1, numel(reg));
    for k = 1:numel(reg)
        row = reg(k);
        reg_names{k} = row.branch_name;
    end
    ok_n4 = (numel(reg_names) == numel(pol.branches)) ...
        && isempty(setdiff(reg_names, pol.branches)) ...
        && isempty(setdiff(pol.branches, reg_names));
catch p5_err
    fprintf('[T] N4 exception: %s\n', p5_err.message);
end
if ok_n4
    fprintf('[T] N4 branch list matches policy file: PASS\n');
end
n_fail = n_fail + ~ok_n4;

%% summary
fprintf('[T] test_seed_tree: %d/8 cases passed\n', 8 - n_fail);
if n_fail > 0
    error('GATE:test_seed_tree %d case(s) failed', n_fail);
end
fprintf('test_seed_tree: ALL PASS (8/8)\n');

%% ======================== local functions ==========================

function pol = local_policy_probe(ROOT)
% Independent policy read for the test (jsondecode is safe for this file:
% plain string array, no duplicate keys / number-literal risk).
    ppath = fullfile(ROOT, 'config', 'mission_v3', 'seed_tree_policy.json');
    obj = jsondecode(fileread(ppath));
    pv = obj.policy_version;
    br = obj.branches;
    if ~ischar(pv) || ~iscell(br)
        error('GATE:test_seed_tree policy probe failed');
    end
    pol = struct();
    pol.policy_version = char(pv);
    pol.branches = br;
end

function h = local_sha256_hex(bytes)
% SHA-256 hex of raw bytes (same java pattern as validate_registries.m).
    dg = local_sha256_digest(bytes);
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end

function dg = local_sha256_digest(bytes)
    md = java.security.MessageDigest.getInstance('SHA-256');
    dg = typecast(md.digest(typecast(bytes(:), 'int8')), 'uint8');
end

function row = local_find_by_name(lst, nm)
% Find the single entry with branch_name == nm (empty when absent).
    row = [];
    for k = 1:numel(lst)
        e = lst(k);
        if strcmp(e.branch_name, nm)
            row = e;
            return;
        end
    end
end

function rows = local_intervention_rows(lst)
    rows = [];
    for k = 1:numel(lst)
        e = lst(k);
        if e.is_intervention
            if isempty(rows)
                rows = e;
            else
                rows(end+1) = e; %#ok<AGROW>
            end
        end
    end
end

function verdict = local_verdict(ok)
    if ok
        verdict = 'PASS';
    else
        verdict = 'FAIL';
    end
end

function [ok, id] = local_expect_error(fh)
% Expect the call to raise a GATE-prefixed error.
    ok = false;
    id = 'NO_ERROR_RAISED';
    try
        fh();
    catch err
        ident = char(string(err.identifier));
        id = ident;
        if startsWith(ident, 'GATE:')
            ok = true;
        end
    end
end
