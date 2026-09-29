% test_probe_fold_stratification.m - T-429 EXECFIX-02: probe grouped
%   5-fold fold-assignment property suite (structural, no frozen-value
%   dependency beyond the standard freeze machine-verification that the
%   'probe' entry runs anyway).
%
%   Verifies the real local_probe_eval code path through the public
%   'probe' entry (mission_v3.audits.identifiability('probe', S, idxA,
%   idxB)) on synthetic in-memory slice pools.  The EXECFIX-02 fold
%   assignment is deterministic and class-stratified: groups are assigned
%   WHOLE to folds (grouped CV by split_group_id unchanged; a group can
%   never span folds - the assignment is keyed by group id, so group
%   wholeness holds by construction and is pinned here by the per-case
%   fold validity results), and every test fold must contain BOTH
%   classes (the DGN-12 documented fold property "each fold carries
%   >= ~2 independent groups per class").
%
%   Cases (each runs the real probe on a synthetic pool):
%   P1  DEPTH-like class-pure pool: 12 pure-A + 12 pure-B groups,
%       near-equal slice counts, A block ordered before the B block in
%       group-id space (the T429 v2 corpus layout whose class-blind
%       assignment produced an invalid test fold).  Expected: status OK,
%       5 folds, fold_ba all finite and valid (every fold two-class).
%   P2  Extreme class-count skew: 6 pure-A groups of 300 slices vs
%       6 pure-B groups of 3 slices (interleaved group ids).  The former
%       class-blind size-balanced rule deterministically piles all B
%       groups into the two smallest-total folds (>= 3 invalid folds);
%       the stratified rule must give all folds valid.
%   P3  Shared-group pool (every group contributes slices of BOTH
%       classes, the SEG/DIR/TRAFFIC family-pool shape): assignment
%       balances total slices only; all folds valid (behavior kept).
%   P4  Mixed pool: 4 shared + 8 pure-A + 8 pure-B groups; all folds
%       valid.
%   P5  Determinism: the P1 pool probed twice gives an exactly equal
%       fold_ba vector (no RNG in the assignment; frozen bootstrap seed
%       replayed identically).
%   P6  Fail-closed semantics preserved: 3 pure-A + 3 pure-B groups with
%       5 folds (fewer groups per class than folds, i.e. coverage below
%       the DGN-12 mathematical necessity) still yields status
%       INSUFFICIENT with the invalid-fold reason - the repair must not
%       paper over genuine coverage shortfalls.
%   P7  BA_point aggregation: BA_point equals the mean of the per-fold
%       BA values on the P1 pool (fold vector is the decision input).
%
%   PASS summary on stdout (ALL PASS (n/n)); any failure raises
%   error('GATE:test_probe_fold_stratification FOLD: ...').
%
%   Discipline: ASCII-only source; no absolute paths; no chained
%   indexing; no global RNG (features are algebraic, no rand* calls);
%   no local functions (script suite style, matching run_all's run(...)
%   execution); ROOT via mfilename (4 x fileparts) with pwd fallback
%   (workspace AGENTS.md 20.2).

pf_root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(pf_root) || exist(fullfile(pf_root, 'config'), 'dir') ~= 7
    pf_root = pwd;
end
addpath(fullfile(pf_root, 'scripts'));

pf_n_pass = 0;
pf_n_total = 0;
pf_failed = cell(1, 0);

% ---------------- P1: DEPTH-like class-pure pool -----------------------
pf_n_groups_A = 12;
pf_n_groups_B = 12;
pf_cnt = 58;                       % near-equal slice counts (699/12 ~ 704/12)
pf_n = (pf_n_groups_A + pf_n_groups_B) * pf_cnt;
pf_F = zeros(pf_n, 3);
pf_G = zeros(pf_n, 1);
pf_row = 0;
for pf_g = 1:pf_n_groups_A
    for pf_j = 1:pf_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_g;         % A groups: global ids 1..12
    end
end
for pf_g = 1:pf_n_groups_B
    for pf_j = 1:pf_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 2.0 + 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_n_groups_A + pf_g;   % B groups: global ids 13..24
    end
end
pf_S1 = struct();
pf_S1.F = pf_F;
pf_S1.group_idx = pf_G;
pf_idxA1 = (1:(pf_n_groups_A * pf_cnt))';
pf_idxB1 = ((pf_n_groups_A * pf_cnt) + 1:pf_n)';

pf_r1 = mission_v3.audits.identifiability('probe', pf_S1, pf_idxA1, pf_idxB1);
pf_n_total = pf_n_total + 1;
pf_ok = strcmp(pf_r1.status, 'OK') ...
    && numel(pf_r1.fold_ba) == 5 ...
    && all(pf_r1.fold_valid) ...
    && all(isfinite(pf_r1.fold_ba));
if pf_ok
    pf_n_pass = pf_n_pass + 1;
    fprintf(['[test_probe_fold_stratification] PASS P1 DEPTH-like ', ...
        'pure pool: 5/5 folds two-class valid (fold_ba=[%.4f %.4f ', ...
        '%.4f %.4f %.4f])\n'], pf_r1.fold_ba);
else
    pf_failed{end+1} = 'P1'; %#ok<AGROW>
    fprintf('[test_probe_fold_stratification] FAIL P1\n');
end

% ---------------- P2: extreme class-count skew -------------------------
pf_a_cnt = 300;
pf_b_cnt = 3;
pf_n = 6 * pf_a_cnt + 6 * pf_b_cnt;
pf_F = zeros(pf_n, 3);
pf_G = zeros(pf_n, 1);
pf_row = 0;
pf_a_ids = [2 4 6 8 10 12];        % interleaved global group ids
pf_b_ids = [1 3 5 7 9 11];
for pf_g = 1:6
    for pf_j = 1:pf_a_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_a_ids(pf_g);
    end
end
for pf_g = 1:6
    for pf_j = 1:pf_b_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 2.0 + 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_b_ids(pf_g);
    end
end
pf_S2 = struct();
pf_S2.F = pf_F;
pf_S2.group_idx = pf_G;
pf_idxA2 = (1:(6 * pf_a_cnt))';
pf_idxB2 = ((6 * pf_a_cnt) + 1:pf_n)';

pf_r2 = mission_v3.audits.identifiability('probe', pf_S2, pf_idxA2, pf_idxB2);
pf_n_total = pf_n_total + 1;
pf_ok = strcmp(pf_r2.status, 'OK') ...
    && all(pf_r2.fold_valid);
if pf_ok
    pf_n_pass = pf_n_pass + 1;
    fprintf(['[test_probe_fold_stratification] PASS P2 extreme skew ', ...
        'pure pool: 5/5 folds two-class valid under 300:3 count ', ...
        'asymmetry\n']);
else
    pf_failed{end+1} = 'P2'; %#ok<AGROW>
    fprintf('[test_probe_fold_stratification] FAIL P2\n');
end

% ---------------- P3: shared-group pool (both classes per group) -------
pf_n_groups = 10;
pf_cnt = 10;
pf_n = pf_n_groups * pf_cnt;
pf_F = zeros(pf_n, 3);
pf_G = zeros(pf_n, 1);
pf_row = 0;
pf_half = pf_cnt / 2;
for pf_g = 1:pf_n_groups
    for pf_j = 1:pf_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        if pf_j <= pf_half
            pf_shift = 0.0;
        else
            pf_shift = 2.0;
        end
        pf_F(pf_i, 1) = pf_shift + 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_g;
    end
end
pf_S3 = struct();
pf_S3.F = pf_F;
pf_S3.group_idx = pf_G;
pf_rowA = zeros(pf_n_groups * pf_half, 1);
pf_rowB = zeros(pf_n_groups * pf_half, 1);
pf_ka = 0;
pf_kb = 0;
for pf_g = 1:pf_n_groups
    pf_base = (pf_g - 1) * pf_cnt;
    for pf_j = 1:pf_half
        pf_ka = pf_ka + 1;
        pf_rowA(pf_ka) = pf_base + pf_j;
        pf_kb = pf_kb + 1;
        pf_rowB(pf_kb) = pf_base + pf_half + pf_j;
    end
end

pf_r3 = mission_v3.audits.identifiability('probe', pf_S3, pf_rowA, pf_rowB);
pf_n_total = pf_n_total + 1;
pf_ok = strcmp(pf_r3.status, 'OK') ...
    && all(pf_r3.fold_valid);
if pf_ok
    pf_n_pass = pf_n_pass + 1;
    fprintf(['[test_probe_fold_stratification] PASS P3 shared-group ', ...
        'pool: 5/5 folds two-class valid (size-only balance kept)\n']);
else
    pf_failed{end+1} = 'P3'; %#ok<AGROW>
    fprintf('[test_probe_fold_stratification] FAIL P3\n');
end

% ---------------- P4: mixed shared + class-pure pool --------------------
pf_n_shared = 4;
pf_n_pure = 8;
pf_shared_cnt = 10;                % per shared group: 5 A + 5 B slices
pf_pure_cnt = 6;                   % per pure group
pf_n = pf_n_shared * pf_shared_cnt + (2 * pf_n_pure) * pf_pure_cnt;
pf_F = zeros(pf_n, 3);
pf_G = zeros(pf_n, 1);
pf_row = 0;
pf_gid = 0;
pf_rows_a = [];
pf_rows_b = [];
for pf_g = 1:pf_n_shared
    pf_gid = pf_gid + 1;
    for pf_j = 1:pf_shared_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        if pf_j <= pf_shared_cnt / 2
            pf_shift = 0.0;
        else
            pf_shift = 2.0;
        end
        pf_F(pf_i, 1) = pf_shift + 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_gid;
        if pf_j <= pf_shared_cnt / 2
            pf_rows_a(end+1, 1) = pf_i; %#ok<AGROW>
        else
            pf_rows_b(end+1, 1) = pf_i; %#ok<AGROW>
        end
    end
end
for pf_g = 1:pf_n_pure
    pf_gid = pf_gid + 1;
    for pf_j = 1:pf_pure_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_gid;
        pf_rows_a(end+1, 1) = pf_i; %#ok<AGROW>
    end
end
for pf_g = 1:pf_n_pure
    pf_gid = pf_gid + 1;
    for pf_j = 1:pf_pure_cnt
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 2.0 + 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_gid;
        pf_rows_b(end+1, 1) = pf_i; %#ok<AGROW>
    end
end
pf_S4 = struct();
pf_S4.F = pf_F;
pf_S4.group_idx = pf_G;

pf_r4 = mission_v3.audits.identifiability('probe', pf_S4, ...
    pf_rows_a(:), pf_rows_b(:));
pf_n_total = pf_n_total + 1;
pf_ok = strcmp(pf_r4.status, 'OK') ...
    && all(pf_r4.fold_valid);
if pf_ok
    pf_n_pass = pf_n_pass + 1;
    fprintf(['[test_probe_fold_stratification] PASS P4 mixed ', ...
        'shared+pure pool: 5/5 folds two-class valid\n']);
else
    pf_failed{end+1} = 'P4'; %#ok<AGROW>
    fprintf('[test_probe_fold_stratification] FAIL P4\n');
end

% ---------------- P5: determinism (no RNG in the assignment) ------------
pf_r5 = mission_v3.audits.identifiability('probe', pf_S1, pf_idxA1, pf_idxB1);
pf_n_total = pf_n_total + 1;
pf_ok = isequal(pf_r5.fold_ba, pf_r1.fold_ba) ...
    && strcmp(pf_r5.status, pf_r1.status);
if pf_ok
    pf_n_pass = pf_n_pass + 1;
    fprintf(['[test_probe_fold_stratification] PASS P5 identical ', ...
        're-probe reproduces the fold_ba vector exactly\n']);
else
    pf_failed{end+1} = 'P5'; %#ok<AGROW>
    fprintf('[test_probe_fold_stratification] FAIL P5\n');
end

% ---------------- P6: fail-closed preserved below design necessity -----
pf_n = 6 * 10;
pf_F = zeros(pf_n, 3);
pf_G = zeros(pf_n, 1);
pf_row = 0;
for pf_g = 1:3
    for pf_j = 1:10
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = pf_g;
    end
end
for pf_g = 1:3
    for pf_j = 1:10
        pf_row = pf_row + 1;
        pf_i = pf_row;
        pf_F(pf_i, 1) = 2.0 + 0.01 * mod(pf_i, 5);
        pf_F(pf_i, 2) = 0.05 * sin(pf_i / 3.0);
        pf_F(pf_i, 3) = 0.05 * cos(pf_i / 4.0);
        pf_G(pf_i) = 3 + pf_g;
    end
end
pf_S6 = struct();
pf_S6.F = pf_F;
pf_S6.group_idx = pf_G;
pf_idxA6 = (1:30)';
pf_idxB6 = (31:60)';

pf_r6 = mission_v3.audits.identifiability('probe', pf_S6, pf_idxA6, pf_idxB6);
pf_n_total = pf_n_total + 1;
pf_ok = strcmp(pf_r6.status, 'INSUFFICIENT') ...
    && isfield(pf_r6, 'reason') ...
    && ~isempty(strfind(char(pf_r6.reason), 'invalid fold'));
if pf_ok
    pf_n_pass = pf_n_pass + 1;
    fprintf(['[test_probe_fold_stratification] PASS P6 3+3 groups ', ...
        'below the 5-fold necessity still fails closed (INSUFFICIENT, ', ...
        'invalid-fold reason kept)\n']);
else
    pf_failed{end+1} = 'P6'; %#ok<AGROW>
    fprintf('[test_probe_fold_stratification] FAIL P6\n');
end

% ---------------- P7: BA_point aggregates the fold vector ---------------
pf_n_total = pf_n_total + 1;
pf_ok = abs(pf_r1.BA_point - mean(pf_r1.fold_ba)) < 1e-12;
if pf_ok
    pf_n_pass = pf_n_pass + 1;
    fprintf(['[test_probe_fold_stratification] PASS P7 BA_point equals ', ...
        'the mean of the 5 valid fold BA values\n']);
else
    pf_failed{end+1} = 'P7'; %#ok<AGROW>
    fprintf('[test_probe_fold_stratification] FAIL P7\n');
end

% ---------------- summary -----------------------------------------------
fprintf('[test_probe_fold_stratification] cases %d/%d passed\n', ...
    pf_n_pass, pf_n_total);
if pf_n_pass ~= pf_n_total
    error(['GATE:test_probe_fold_stratification FOLD: %d/%d case(s) ', ...
        'failed: %s'], numel(pf_failed), pf_n_total, ...
        strjoin(pf_failed, ', '));
end
fprintf('ALL PASS (%d/%d)\n', pf_n_pass, pf_n_total);
