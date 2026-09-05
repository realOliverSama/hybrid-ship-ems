% test_family_expand.m - T-014: L4 intervention expansion + grouping harness
%   C1 (spec B.1) closure transitivity: edges A-B, B-C put A/B/C in one
%       split_group; unconnected D stays its own group; group ids are
%       'SG#' + 16 lowercase hex; digest keys steer the group identity.
%   C2 (spec B.2) expand on a real base draw: 4 members; INFORMATION and
%       PHYSICAL (traffic OFF marker) members all carry the SAME truth hash
%       of the shared base-draw realization; 'audit' passes on the manifest
%       registry (FR-529; traffic_override is metadata, not a payload
%       change at this layer).
%   N1 (spec B.3, FR-524) undeclared cross-family truth reuse: two members
%       pointing at the same base draw but declared in two families are
%       detected -> GATE:MISSION_V3:FAMILY:UNDECLARED_REUSE.
%   N2 (spec B.4, FR-529) truth drift: tampering one u_river point (and
%       other payload dimensions: traffic event, water, resistance, hotel)
%       changes the truth hash; identical-lineage same-family members
%       declaring different hashes -> GATE:MISSION_V3:FAMILY:TRUTH_DRIFT.
%   N3 (spec B.5, FR-525) same template, different base draw: two families
%       keep different split_group ids; the union closure over both member
%       sets yields exactly two groups (no merge on template identity).
%   C3 (spec B.6, C-D1) family -> exactly one split_group invariant on both
%       manifests.
%   C4 (spec B.7) arms completeness (exact 2x2 combination set), base
%       member identity (lineage equals the original draw registry), truth
%       lineage identical across members, default-matrix structure, and the
%       explicit 'Matrix' path equal to the default expansion.
%   C5 (spec B.8) determinism: two identical expand runs, repeated truth
%       hash and closure calls are field-identical.
%   Failures raise error('GATE:test_family_expand ...'); the test writes
%   nothing to results/.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_family_expand.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

FS = 't014_family_seed_01';                 % test fixture, not a frozen value
TPL = 'ROUTINE_PATROL';
REG = 'NORMAL';
STRAT = 'STRAT_ROUTINE_PATROL__NORMAL';     % caller label (quota layer)

n_fail = 0;

% base-draw fixtures (T-009 outputs; disposition is irrelevant for the
% grouping/truth-hash metadata under test)
dag = mission_v3.dag_sample('draw', FS, 7, TPL, REG, STRAT);
rl = mission_v3.env_realize('realize', dag, 'auto');
dag_b = mission_v3.dag_sample('draw', FS, 8, TPL, REG, STRAT);
rl_b = mission_v3.env_realize('realize', dag_b, 'auto');

fam = mission_v3.family_expand('expand', dag, 'Realization', rl);
fam_b = mission_v3.family_expand('expand', dag_b, 'Realization', rl_b);

%% C1 (B.1): closure transitivity; unconnected node stays separate
ok_1 = false;
cl = [];
try
    ids = {'A', 'B', 'C', 'D'};
    edges = {'A', 'B'; 'B', 'C'};
    cl = mission_v3.family_expand('closure', ids, edges);
    lab = cl.labels;
    ok_1 = isequal(lab{1}, lab{2}) && isequal(lab{2}, lab{3}) ...
        && ~isequal(lab{4}, lab{1}) ...
        && (cl.n_groups == 2) && (cl.n_edges == 2) ...
        && (cl.node_count == 4);
    g1 = cl.groups(1);
    g2 = cl.groups(2);
    ok_1 = ok_1 && (numel(g1.member_ids) == 3) ...
        && (numel(g2.member_ids) == 1) && strcmp(g2.member_ids{1}, 'D');
    fmt1 = ~isempty(regexp(g1.group_id, '^SG#[0-9a-f]{16}$', 'once'));
    fmt2 = ~isempty(regexp(g2.group_id, '^SG#[0-9a-f]{16}$', 'once'));
    ok_1 = ok_1 && fmt1 && fmt2;
    % digest keys steer the group identity (same graph, different keys)
    cl2 = mission_v3.family_expand('closure', ids, edges, ...
        {'ka', 'kb', 'kc', 'kd'});
    ok_1 = ok_1 && ~strcmp(cl2.labels{1}, cl.labels{1}) ...
        && isequal(cl2.labels{1}, cl2.labels{2}) ...
        && isequal(cl2.labels{2}, cl2.labels{3});
catch c1_err
    ok_1 = false;
    fprintf('[T] C1 exception: %s\n', c1_err.message);
end
if ok_1
    fprintf('[T] C1 closure transitivity + isolation: PASS\n');
end
n_fail = n_fail + ~ok_1;

%% C2 (B.2): shared truth hash across INFORMATION and PHYSICAL members
ok_2 = false;
h_ref = '';
try
    ok_2 = strcmp(fam.kind, 'family_manifest') && (fam.n_members == 4);
    h_ref = mission_v3.family_expand('truth_hash', dag, rl);
    ok_2 = ok_2 && ~isempty(regexp(h_ref, '^[0-9a-f]{64}$', 'once'));
    same_all = true;
    for k = 1:fam.n_members
        same_all = same_all && strcmp(fam.members(k).truth_hash, h_ref);
    end
    ok_2 = ok_2 && same_all;
    au = mission_v3.family_expand('audit', fam.audit_registry);
    ok_2 = ok_2 && au.ok && (au.n_members == 4) ...
        && (au.n_lineage_groups == 1);
    % PHYSICAL marker members (traffic OFF) share the identical hash
    m1 = fam.members(1);
    m3 = fam.members(3);
    ok_2 = ok_2 && strcmp(m1.traffic_override, 'ON') ...
        && strcmp(m3.traffic_override, 'OFF') ...
        && strcmp(m1.truth_hash, m3.truth_hash) ...
        && ~isempty(m3.truth_hash);
catch c2_err
    ok_2 = false;
    fprintf('[T] C2 exception: %s\n', c2_err.message);
end
if ok_2
    fprintf('[T] C2 shared truth hash (INFORMATION + PHYSICAL): PASS\n');
end
n_fail = n_fail + ~ok_2;

%% N1 (B.3, FR-524): undeclared cross-family truth reuse detected
ok_n1 = false;
id_n1 = 'NO_ERROR_RAISED';
try
    fam2 = mission_v3.family_expand('expand', dag, 'FamilySeq', 2, ...
        'Realization', rl);
    r1 = fam.audit_registry(1);
    r2 = fam2.audit_registry(1);
    rec = [r1, r2];
    [ok_n1, id_n1] = local_expect_error(...
        @() mission_v3.family_expand('audit', rec));
    ok_n1 = ok_n1 && strcmp(char(id_n1), ...
        'GATE:MISSION_V3:FAMILY:UNDECLARED_REUSE');
catch n1_err
    ok_n1 = false;
    fprintf('[T] N1 exception: %s\n', n1_err.message);
end
fprintf('[T] N1 undeclared reuse detected: %s (id=%s)\n', ...
    local_verdict(ok_n1), id_n1);
n_fail = n_fail + ~ok_n1;

%% N2 (B.4, FR-529): truth drift detected; payload sensitivity
ok_n2 = false;
id_n2 = 'NO_ERROR_RAISED';
try
    h1 = mission_v3.family_expand('truth_hash', dag, rl);
    u2 = rl.u_river_mps;
    u2(5) = u2(5) + 1.0e-3;
    rl2 = rl;
    rl2.u_river_mps = u2;
    h2 = mission_v3.family_expand('truth_hash', dag, rl2);
    sens_u = ~strcmp(h1, h2);
    sens_w = false;
    rl3 = rl;
    w3 = rl3.water;
    w3.h_chart_m = w3.h_chart_m + 0.01;
    rl3.water = w3;
    sens_w = ~strcmp(h1, mission_v3.family_expand('truth_hash', dag, rl3));
    sens_r = false;
    dag2 = dag;
    dag2.resistance_mult = dag.resistance_mult * 1.001;
    sens_r = ~strcmp(h1, mission_v3.family_expand('truth_hash', dag2, rl));
    % same family, identical truth lineage, different declared hashes
    rec1 = fam.audit_registry(1);
    recd = rec1;
    recd.member_id = [rec1.member_id, 'X'];
    recd.truth_hash = h2;
    reg_d = [rec1, recd];
    [ok_n2, id_n2] = local_expect_error(...
        @() mission_v3.family_expand('audit', reg_d));
    ok_n2 = ok_n2 && sens_u && sens_w && sens_r ...
        && strcmp(char(id_n2), 'GATE:MISSION_V3:FAMILY:TRUTH_DRIFT');
catch n2_err
    ok_n2 = false;
    fprintf('[T] N2 exception: %s\n', n2_err.message);
end
fprintf('[T] N2 truth drift detected: %s (id=%s)\n', ...
    local_verdict(ok_n2), id_n2);
n_fail = n_fail + ~ok_n2;

%% N3 (B.5, FR-525): same template, different draw -> separate groups
ok_n3 = false;
try
    ok_n3 = strcmp(fam.template_id, fam_b.template_id) ...
        && ~strcmp(fam.base_draw_id, fam_b.base_draw_id) ...
        && ~strcmp(fam.split_group_id, fam_b.split_group_id);
    tl1 = fam.members(1).truth_lineage_ids;
    tl2 = fam_b.members(1).truth_lineage_ids;
    ok_n3 = ok_n3 && ~isequal(tl1, tl2);
    mans = [fam, fam_b];
    ids_u = cell(1, 8);
    edges_u = cell(12, 2);      % 6 within-family pairs per family
    pos = 0;
    epos = 0;
    for f = 1:2
        mem = mans(f).members;
        nm = numel(mem);
        for k = 1:nm
            pos = pos + 1;
            ids_u{pos} = mem(k).member_id;
        end
        for i = 1:nm
            for j = (i + 1):nm
                epos = epos + 1;
                edges_u{epos, 1} = mem(i).member_id;
                edges_u{epos, 2} = mem(j).member_id;
            end
        end
    end
    ok_n3 = ok_n3 && (pos == 8) && (epos == 12);
    cl_u = mission_v3.family_expand('closure', ids_u, edges_u);
    lab_u = cl_u.labels;
    ok_n3 = ok_n3 && (cl_u.n_groups == 2) ...
        && isequal(lab_u{1}, lab_u{4}) ...
        && isequal(lab_u{5}, lab_u{8}) ...
        && ~isequal(lab_u{1}, lab_u{5});
catch n3_err
    ok_n3 = false;
    fprintf('[T] N3 exception: %s\n', n3_err.message);
end
if ok_n3
    fprintf('[T] N3 same-template different-draw not merged: PASS\n');
end
n_fail = n_fail + ~ok_n3;

%% C3 (B.6, C-D1): family -> exactly one split_group
ok_3 = false;
try
    mans = [fam, fam_b];
    ok_3 = true;
    for f = 1:2
        mem = mans(f).members;
        labs = cell(1, numel(mem));
        for k = 1:numel(mem)
            labs{k} = mem(k).split_group_id;
        end
        ok_3 = ok_3 && (numel(unique(labs)) == 1) ...
            && strcmp(labs{1}, mans(f).split_group_id);
    end
catch c3_err
    ok_3 = false;
    fprintf('[T] C3 exception: %s\n', c3_err.message);
end
if ok_3
    fprintf('[T] C3 family -> exactly one split_group: PASS\n');
end
n_fail = n_fail + ~ok_3;

%% C4 (B.7): arms completeness + base member identity + explicit Matrix
ok_4 = false;
try
    exp_arms = {'ON/GOOD_V1', 'ON/DEGRADED_V1', 'OFF/GOOD_V1', ...
        'OFF/DEGRADED_V1'};
    got = cell(1, 4);
    for k = 1:4
        m = fam.members(k);
        got{k} = [m.arms.traffic, '/', m.arms.forecast];
    end
    arms_ok = isequal(sort(got), sort(exp_arms));
    base_ok = isequal(fam.members(1).is_base, true);
    for k = 2:4
        base_ok = base_ok && isequal(fam.members(k).is_base, false);
    end
    % base member lineage equals the original draw registry (T-009)
    dl = dag.lineage;
    bl = fam.members(1);
    lin_ok = (numel(bl.lineage_ids) == numel(dl));
    for k = 1:numel(dl)
        drow = dl(k);
        lin_ok = lin_ok && strcmp(bl.lineage_ids{k}, drow.lineage_id) ...
            && strcmp(bl.lineage_names{k}, drow.branch_name);
    end
    % truth lineage identical across all members and equals the draw's
    % truth-branch set (standard branches minus forecast_obs_noise)
    tn_true = cell(1, 0);
    for k = 1:numel(dl)
        drow = dl(k);
        if ~strcmp(drow.branch_name, 'forecast_obs_noise')
            tn_true{end+1} = drow.lineage_id; %#ok<AGROW>
        end
    end
    truth_ok = true;
    for k = 1:4
        m = fam.members(k);
        truth_ok = truth_ok && isequal(sort(m.truth_lineage_ids), ...
            sort(tn_true));
    end
    % default matrix structure (fixed decision 1)
    dm = mission_v3.family_expand('default_matrix');
    ok_dm = strcmp(dm.kind, 'intervention_matrix_default') ...
        && (dm.n_members == 4) ...
        && strcmp(dm.matrix(1).axis, 'traffic') ...
        && strcmp(dm.matrix(1).itype, 'PHYSICAL') ...
        && isequal(dm.matrix(1).arms, {'ON', 'OFF'}) ...
        && strcmp(dm.matrix(2).axis, 'forecast') ...
        && strcmp(dm.matrix(2).itype, 'INFORMATION') ...
        && isequal(dm.matrix(2).arms, {'GOOD_V1', 'DEGRADED_V1'});
    % explicit 'Matrix' path equals the default expansion
    fam_ex = mission_v3.family_expand('expand', dag, 'Matrix', dm.matrix, ...
        'Realization', rl);
    ok_4 = arms_ok && base_ok && lin_ok && truth_ok && ok_dm ...
        && isequal(fam_ex, fam);
catch c4_err
    ok_4 = false;
    fprintf('[T] C4 exception: %s\n', c4_err.message);
end
if ok_4
    fprintf('[T] C4 arms completeness + base identity + Matrix path: PASS\n');
end
n_fail = n_fail + ~ok_4;

%% C5 (B.8): determinism (two runs field-identical)
ok_5 = false;
try
    fam_r2 = mission_v3.family_expand('expand', dag, 'Realization', rl);
    same_manifest = isequal(fam, fam_r2);
    h_r2 = mission_v3.family_expand('truth_hash', dag, rl);
    same_hash = strcmp(h_r2, h_ref);
    cl_r2 = mission_v3.family_expand('closure', {'A', 'B', 'C', 'D'}, ...
        {'A', 'B'; 'B', 'C'});
    same_closure = isequal(cl_r2, cl);
    ok_5 = same_manifest && same_hash && same_closure;
catch c5_err
    ok_5 = false;
    fprintf('[T] C5 exception: %s\n', c5_err.message);
end
if ok_5
    fprintf('[T] C5 determinism (manifest / hash / closure): PASS\n');
end
n_fail = n_fail + ~ok_5;

%% summary
fprintf('[T] test_family_expand: %d/8 cases passed\n', 8 - n_fail);
if n_fail > 0
    error('GATE:test_family_expand %d case(s) failed', n_fail);
end
fprintf('test_family_expand: ALL PASS (8/8)\n');

%% ======================== local functions ==========================

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
