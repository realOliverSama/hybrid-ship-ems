% test_producer_export.m - T-015: producer export + voyage qualification
% criteria + six-class failure taxonomy (FR-535~540, DGN-10/15, FR-500)
%   P1 (spec C.1) duration boundary arithmetic (float two-point, FD2):
%       1199.99 s -> REJECTED_SAMPLING_ELIGIBILITY / DURATION_BELOW_MIN
%       (its 60 expected windows still pass the window criterion, so the
%       duration bound is the binding one); 1200.0 s -> duration_ok with
%       61 expected windows and full K/F coverage -> ACCEPT.
%   P2 (spec C.2) canonical window arithmetic (FD1, contract-derived
%       N(D) = floor((D-600)/10)+1, D >= 600; else 0):
%       1590 -> 100 (tasks.md anchor; contract derivation agrees, no
%       conflict), 1200 -> 61, 3000 -> 241 spot checks, plus 600 -> 1,
%       599.99 -> 0, 1500 -> 91, 1800 -> 121, 1599.99 -> 100 (floor
%       truncation); anchors 300:10:1290 for 1590 s; coverage split
%       valid/invalid with a missing K/F snapshot row.
%   P3 (spec C.3) three-condition AND semantics: full coverage -> ACCEPT;
%       one missing snapshot (invalid=1) -> FAILED_CONTRACT with
%       duration_ok/windows_ok true and invalid_ok false; contract
%       defect flags -> FAILED_CONTRACT; missing K/F coverage while
%       windows are expected -> fail-closed FAILED_CONTRACT; accepted ==
%       (duration_ok AND windows_ok AND invalid_ok) identity.
%   P4 (spec C.4) six-class taxonomy: enum completeness (six
%       dispositions + four reasons, env_realize structure), every class
%       constructed (ACCEPT / REJECTED_SAMPLING_ELIGIBILITY with all
%       three reasons / REJECTED_STRUCTURAL_HELDOUT token echo /
%       FAILED_CONTRACT / FAILED_GENERATION / FAILED_PHYSICAL_DOMAIN),
%       retry discrimination flags, precedence checks (generation
%       failure masks a duration rejection; contract defects mask a
%       heldout token; zero-window short voyage stays
%       DURATION_BELOW_MIN).
%   P5 (spec C.5) FR-500 stage isolation: FORMAL+PILOT_ONLY and
%       FORMAL+missing fixture_kind -> GATE:MISSION_V3:EXPORT:
%       STAGE_ISOLATION; FORMAL+NONE, PILOT+PILOT_ONLY, PILOT+NONE pass;
%       unknown domain -> BAD_ARG; 'export_artifact' refuses to write a
%       FORMAL artifact carrying PILOT_ONLY meta (nothing written).
%   P6 (spec C.6) artifact round-trip on a synthetic 1 Hz stream
%       (FD7): export to tempdir -> exactly {stream, kf, meta} in the
%       file -> field-identical read-back (meta embedded with the C-D1
%       extension fields + the FR-536 eligibility record, expected
%       windows 100 for the 1590 s stream); negatives: record/artifact
%       count mismatch -> ELIG_RECORD, formal without eligibility record
%       -> META, missing C-D1 meta field -> META, stream missing a
%       schema-required truth_stream column -> LAYOUT, tampered lead
%       grid -> LAYOUT.
%   P7 (spec C.7) retry records (FD4/FR-537/R8): FAILED_* slots allow
%       same-seed retry rows (two chained records correct:
%       slot_id/attempt/disposition/retry_of/seed_policy);
%       REJECTED_* and ACCEPT never produce retry rows
%       (GATE:MISSION_V3:ELIGIBILITY:RETRY); bad args rejected.
%   P8 (spec C.8) determinism: two judge runs bitwise identical; two
%       exports of identical inputs field-identical after read-back.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_producer_export.m')"
%   Discipline: ASCII only; no absolute paths (artifacts go to tempdir);
%   no chained indexing; no global RNG (synthetic streams are analytic);
%   no Model E simulation; the test writes only tempdir artifacts.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;
ANCH = (300:10:1290)';          % 1590 s canonical anchor grid (100 rows)
ANCH_1200 = (300:10:900)';      % 1200 s anchor grid (61 rows)
ANCH_1199 = (300:10:890)';      % 1199.99 s anchor grid (60 rows)

%% P1 (C.1): duration boundary 1199.99 reject / 1200 accept
ok_p1 = false;
try
    voy_a = local_make_voy(1199.99, ANCH_1199);
    j_a = mission_v3.eligibility('judge', voy_a);
    ok = strcmp(j_a.disposition, 'REJECTED_SAMPLING_ELIGIBILITY') ...
        && strcmp(j_a.reason, 'DURATION_BELOW_MIN') ...
        && ~j_a.criteria.duration_ok ...
        && ~j_a.criteria.accepted ...
        && (j_a.windows.expected_canonical_windows == 60) ...
        && j_a.criteria.windows_ok;    % windows pass; duration binds

    voy_b = local_make_voy(1200.0, ANCH_1200);
    j_b = mission_v3.eligibility('judge', voy_b);
    ok = ok && strcmp(j_b.disposition, 'ACCEPT') ...
        && j_b.criteria.duration_ok && j_b.criteria.windows_ok ...
        && j_b.criteria.invalid_ok ...
        && (j_b.windows.expected_canonical_windows == 61) ...
        && strcmp(j_b.reason, 'none');
    ok_p1 = ok;
    fprintf(['[T] P1 boundary: 1199.99->%s/%s 1200->%s win60/61: %s\n'], ...
        j_a.disposition, j_a.reason, j_b.disposition, local_verdict(ok_p1));
catch p1_err
    fprintf('[T] P1 exception: %s\n', p1_err.message);
end
n_fail = n_fail + ~ok_p1;

%% P2 (C.2): canonical window arithmetic (FD1 contract derivation)
ok_p2 = false;
try
    w1 = mission_v3.eligibility('windows', 1590);
    ok = (w1.expected_canonical_windows == 100) ...   % tasks.md anchor
        && (w1.anchor_first_s == 300) && (w1.anchor_last_s == 1290) ...
        && (w1.anchor_stride_s == 10) && (w1.window_span_s == 600) ...
        && (w1.history_span_s == 300) && (w1.future_span_s == 300) ...
        && (w1.n_lead_bins == 30) && (w1.lead_bin_s == 10) ...
        && w1.coverage_assessed == false;
    w2 = mission_v3.eligibility('windows', 1200);
    w3 = mission_v3.eligibility('windows', 3000);
    w4 = mission_v3.eligibility('windows', 600);
    w5 = mission_v3.eligibility('windows', 599.99);
    w6 = mission_v3.eligibility('windows', 1500);
    w7 = mission_v3.eligibility('windows', 1800);
    w8 = mission_v3.eligibility('windows', 1599.99);
    ok = ok && (w2.expected_canonical_windows == 61) ...
        && (w3.expected_canonical_windows == 241) ...
        && (w4.expected_canonical_windows == 1) ...
        && (w5.expected_canonical_windows == 0) ...
        && (w6.expected_canonical_windows == 91) ...
        && (w7.expected_canonical_windows == 121) ...
        && (w8.expected_canonical_windows == 100);    % floor truncation
    ok = ok && (w5.anchor_first_s ~= w5.anchor_first_s);  % NaN for D<600

    % coverage split: full anchors valid; one missing row invalid
    cov_full = struct('kf_issue_time_s', ANCH);
    wc = mission_v3.eligibility('windows', 1590, cov_full);
    ok = ok && wc.coverage_assessed ...
        && (wc.valid_canonical_windows == 100) ...
        && (wc.invalid_canonical_windows == 0);
    cov_miss = struct('kf_issue_time_s', (310:10:1290)');
    wd = mission_v3.eligibility('windows', 1590, cov_miss);
    ok = ok && (wd.valid_canonical_windows == 99) ...
        && (wd.invalid_canonical_windows == 1) ...
        && (wd.invalid_reason_counts.MISSING_KF_SNAPSHOT == 1);
    % a present but not-fully-covered row (NaN content) counts invalid
    rk = true(99, 1);
    rk(5) = false;
    cov_row = struct('kf_issue_time_s', (310:10:1290)', 'row_ok', rk);
    we = mission_v3.eligibility('windows', 1590, cov_row);
    ok = ok && (we.invalid_canonical_windows == 2);
    % provenance is registry/schema-driven (hashes echoed, not UNKNOWN)
    ok = ok && ~strcmp(w1.provenance.schema_self_hash, 'UNKNOWN') ...
        && ~strcmp(w1.provenance.registry_self_hash, 'UNKNOWN');
    ok_p2 = ok;
    fprintf(['[T] P2 windows: 1590=%d 1200=%d 3000=%d 600=%d 599.99=%d ', ...
        'cov 100/1/2 prov=%d: %s\n'], ...
        w1.expected_canonical_windows, w2.expected_canonical_windows, ...
        w3.expected_canonical_windows, w4.expected_canonical_windows, ...
        w5.expected_canonical_windows, ...
        ~strcmp(w1.provenance.schema_self_hash, 'UNKNOWN'), ...
        local_verdict(ok_p2));
catch p2_err
    fprintf('[T] P2 exception: %s\n', p2_err.message);
end
n_fail = n_fail + ~ok_p2;

%% P3 (C.3): unexpected_invalid=0 three-condition AND semantics
ok_p3 = false;
try
    voy = local_make_voy(1590, (310:10:1290)');   % one snapshot missing
    j = mission_v3.eligibility('judge', voy);
    ok = j.criteria.duration_ok && j.criteria.windows_ok ...
        && ~j.criteria.invalid_ok && ~j.criteria.accepted ...
        && strcmp(j.disposition, 'FAILED_CONTRACT') ...
        && (j.windows.invalid_canonical_windows == 1) ...
        && ~isempty(strfind(j.detail, 'INVALID_WINDOWS'));

    voy2 = local_make_voy(1590, ANCH);
    voy2.contract_defects = struct('missing_schema_fields', true, ...
        'missing_forecast_provenance', false, 'missing_stream_data', false);
    j2 = mission_v3.eligibility('judge', voy2);
    ok = ok && strcmp(j2.disposition, 'FAILED_CONTRACT') ...
        && ~j2.criteria.invalid_ok && ~j2.criteria.accepted ...
        && ~isempty(strfind(j2.detail, 'MISSING_SCHEMA_FIELDS'));

    voy3 = struct('duration_s', 1590.0);   % coverage not supplied
    j3 = mission_v3.eligibility('judge', voy3);
    ok = ok && strcmp(j3.disposition, 'FAILED_CONTRACT') ...
        && ~isempty(strfind(j3.detail, 'KF_COVERAGE_NOT_SUPPLIED'));

    voy4 = local_make_voy(1590, ANCH);
    j4 = mission_v3.eligibility('judge', voy4);
    ok = ok && j4.criteria.accepted ...
        && (j4.criteria.accepted == (j4.criteria.duration_ok ...
            && j4.criteria.windows_ok && j4.criteria.invalid_ok));
    ok_p3 = ok;
    fprintf(['[T] P3 AND: invalid1->%s defect->%s nocov->%s ', ...
        'identity=%d: %s\n'], j.disposition, j2.disposition, ...
        j3.disposition, ...
        (j4.criteria.accepted == (j4.criteria.duration_ok ...
        && j4.criteria.windows_ok && j4.criteria.invalid_ok)), ...
        local_verdict(ok_p3));
catch p3_err
    fprintf('[T] P3 exception: %s\n', p3_err.message);
end
n_fail = n_fail + ~ok_p3;

%% P4 (C.4): six-class taxonomy, all classes constructed, precedence
ok_p4 = false;
try
    tax = mission_v3.eligibility('taxonomy');
    want6 = {'ACCEPT', 'REJECTED_SAMPLING_ELIGIBILITY', ...
        'REJECTED_STRUCTURAL_HELDOUT', 'FAILED_CONTRACT', ...
        'FAILED_GENERATION', 'FAILED_PHYSICAL_DOMAIN'};
    ok = (numel(tax.disposition_enum) == 6) ...
        && (numel(tax.reason_enum) == 4);
    for k = 1:6
        ok = ok && any(strcmp(tax.disposition_enum, want6{k}));
    end
    ok = ok && any(strcmp(tax.reason_enum, 'DURATION_BELOW_MIN')) ...
        && any(strcmp(tax.reason_enum, 'SUPPORT_BOUND')) ...
        && any(strcmp(tax.reason_enum, 'PHYSICAL_DOMAIN_BOUND'));

    j_acc = mission_v3.eligibility('judge', local_make_voy(1590, ANCH));
    ok = ok && strcmp(j_acc.disposition, 'ACCEPT');

    v_short = local_make_voy(1199.99, ANCH_1199);
    j_short = mission_v3.eligibility('judge', v_short);
    ok = ok && strcmp(j_short.reason, 'DURATION_BELOW_MIN');

    v_sup = local_make_voy(1200.0, ANCH_1200);
    j_sup = mission_v3.eligibility('judge', v_sup, ...
        'MinCanonicalWindows', 65);        % activation-discipline override
    ok = ok && strcmp(j_sup.disposition, 'REJECTED_SAMPLING_ELIGIBILITY') ...
        && strcmp(j_sup.reason, 'SUPPORT_BOUND');

    v_pdb = local_make_voy(1590, ANCH);
    v_pdb.physical_domain_bound = true;
    j_pdb = mission_v3.eligibility('judge', v_pdb);
    ok = ok && strcmp(j_pdb.disposition, 'REJECTED_SAMPLING_ELIGIBILITY') ...
        && strcmp(j_pdb.reason, 'PHYSICAL_DOMAIN_BOUND');

    v_held = local_make_voy(1590, ANCH);
    v_held.heldout_token_id = 'C1_BRIDGE_HIGH_ID_BAND';
    j_held = mission_v3.eligibility('judge', v_held);
    ok = ok && strcmp(j_held.disposition, 'REJECTED_STRUCTURAL_HELDOUT') ...
        && strcmp(j_held.token_id, 'C1_BRIDGE_HIGH_ID_BAND');

    v_fc = local_make_voy(1590, (310:10:1290)');
    j_fc = mission_v3.eligibility('judge', v_fc);
    ok = ok && strcmp(j_fc.disposition, 'FAILED_CONTRACT');

    v_fg = local_make_voy(1590, ANCH);
    v_fg.generation_failed = true;
    v_fg.generation_fail_reason = 'SIM_RUNTIME';
    j_fg = mission_v3.eligibility('judge', v_fg);
    ok = ok && strcmp(j_fg.disposition, 'FAILED_GENERATION') ...
        && strcmp(j_fg.detail, 'SIM_RUNTIME') ...
        && j_fg.retry.allowed;

    v_fpd = local_make_voy(1590, ANCH);
    v_fpd.physical_domain_violation = true;
    v_fpd.physical_domain_violation_reason = 'POST_HOC_HT';
    j_fpd = mission_v3.eligibility('judge', v_fpd);
    ok = ok && strcmp(j_fpd.disposition, 'FAILED_PHYSICAL_DOMAIN') ...
        && strcmp(j_fpd.detail, 'POST_HOC_HT');
    ok = ok && strcmp(j_fpd.reason, 'none') ...
        && strcmp(j_held.reason, 'none');

    % all constructed dispositions are enum members
    got = {j_acc.disposition, j_short.disposition, j_sup.disposition, ...
        j_pdb.disposition, j_held.disposition, j_fc.disposition, ...
        j_fg.disposition, j_fpd.disposition};
    for k = 1:numel(got)
        ok = ok && any(strcmp(tax.disposition_enum, got{k}));
    end

    % precedence: generation failure masks a duration rejection
    v_pre1 = local_make_voy(300, []);
    v_pre1.generation_failed = true;
    j_pre1 = mission_v3.eligibility('judge', v_pre1);
    ok = ok && strcmp(j_pre1.disposition, 'FAILED_GENERATION');
    % zero-window short voyage without failures stays DURATION_BELOW_MIN
    j_pre1b = mission_v3.eligibility('judge', local_make_voy(300, []));
    ok = ok && strcmp(j_pre1b.disposition, 'REJECTED_SAMPLING_ELIGIBILITY') ...
        && strcmp(j_pre1b.reason, 'DURATION_BELOW_MIN');
    % contract defects mask a heldout token (FR-536 no masking)
    v_pre2 = local_make_voy(1590, (310:10:1290)');
    v_pre2.heldout_token_id = 'C2_HARBOR_LOW_HT';
    j_pre2 = mission_v3.eligibility('judge', v_pre2);
    ok = ok && strcmp(j_pre2.disposition, 'FAILED_CONTRACT');

    % retry discrimination: FAILED_* yes, REJECTED_*/ACCEPT no
    for k = 1:6
        d = tax.disposition_enum{k};
        if strncmp(d, 'FAILED_', 7)
            ok = ok && tax.slot_failure(k) && tax.retry_allowed(k) ...
                && strcmp(tax.retry_policy{k}, ...
                'SAME_SLOT_SAME_CONFIG_SAME_SCIENCE_SEED');
        elseif strncmp(d, 'REJECTED_', 9)
            ok = ok && ~tax.occupies_slot(k) && ~tax.retry_allowed(k) ...
                && strcmp(tax.retry_policy{k}, 'PREREGISTERED_RESAMPLE');
        else
            ok = ok && tax.occupies_slot(k) && ~tax.retry_allowed(k) ...
                && strcmp(tax.retry_policy{k}, 'NONE');
        end
    end
    ok_p4 = ok;
    fprintf(['[T] P4 taxonomy: ACC/DBM/SB/PDB/HELD/FC/FG/FPD + prec ', ...
        '%d%d%d enum6: %s\n'], ...
        strcmp(j_pre1.disposition, 'FAILED_GENERATION'), ...
        strcmp(j_pre2.disposition, 'FAILED_CONTRACT'), ...
        strcmp(j_pre1b.reason, 'DURATION_BELOW_MIN'), ...
        local_verdict(ok_p4));
catch p4_err
    fprintf('[T] P4 exception: %s\n', p4_err.message);
end
n_fail = n_fail + ~ok_p4;

%% P5 (C.5): FR-500 stage isolation
ok_p5 = false;
try
    m_ok = local_make_meta('FORMAL', 'NONE');
    r1 = mission_v3.producer_export('stage_check', m_ok);
    ok = r1.ok && strcmp(r1.dataset_domain, 'FORMAL') ...
        && strcmp(r1.fixture_kind, 'NONE');

    m_po = local_make_meta('FORMAL', 'PILOT_ONLY');
    [ok_a, id_a] = local_expect_error_id(@() ...
        mission_v3.producer_export('stage_check', m_po), ...
        'GATE:MISSION_V3:EXPORT:STAGE_ISOLATION');

    m_miss = local_make_meta('FORMAL', 'NONE');
    m_miss = rmfield(m_miss, 'fixture_kind');
    [ok_b, id_b] = local_expect_error_id(@() ...
        mission_v3.producer_export('stage_check', m_miss), ...
        'GATE:MISSION_V3:EXPORT:STAGE_ISOLATION');

    r2 = mission_v3.producer_export('stage_check', ...
        local_make_meta('PILOT', 'PILOT_ONLY'));
    r3 = mission_v3.producer_export('stage_check', ...
        local_make_meta('PILOT', 'NONE'));
    ok = ok && r2.ok && r3.ok ...
        && strcmp(r2.fixture_kind, 'PILOT_ONLY') ...
        && strcmp(r3.fixture_kind, 'NONE');

    [ok_c, id_c] = local_expect_error_id(@() ...
        mission_v3.producer_export('stage_check', ...
        local_make_meta('WEIRD', 'NONE')), ...
        'GATE:MISSION_V3:EXPORT:BAD_ARG');

    % export refuses to write a FORMAL artifact carrying PILOT_ONLY meta
    st = local_make_stream(1590);
    kf = local_make_kf(ANCH);
    voy = local_make_voy(1590, ANCH);
    jg = mission_v3.eligibility('judge', voy);
    ftmp = local_tmpfile('t015_p5');
    [ok_d, id_d] = local_expect_error_id(@() ...
        mission_v3.producer_export('export_artifact', st, kf, m_po, ...
        jg, ftmp), 'GATE:MISSION_V3:EXPORT:STAGE_ISOLATION');
    ok = ok && ~exist(ftmp, 'file');   % nothing written

    ok_p5 = ok && ok_a && ok_b && ok_c && ok_d;
    fprintf(['[T] P5 stage: FORMAL+NONE ok PILOT dual ok ', ...
        'PILOT_ONLY=%d miss=%d weird=%d export_guard=%d clean=%d: %s\n'], ...
        ok_a, ok_b, ok_c, ok_d, ~exist(ftmp, 'file'), ...
        local_verdict(ok_p5));
catch p5_err
    fprintf('[T] P5 exception: %s\n', p5_err.message);
end
n_fail = n_fail + ~ok_p5;

%% P6 (C.6): artifact round-trip + integrity negatives
ok_p6 = false;
try
    st = local_make_stream(1590);
    kf = local_make_kf(ANCH);
    meta = local_make_meta('FORMAL', 'NONE');
    voy = local_make_voy(1590, ANCH);
    jg = mission_v3.eligibility('judge', voy);
    meta_want = meta;
    meta_want.eligibility = jg;        % C-side FR-536 embedding

    f1 = local_tmpfile('t015_p6a');
    res = mission_v3.producer_export('export_artifact', st, kf, meta, ...
        jg, f1);
    ok = res.ok && res.roundtrip_ok ...
        && (res.expected_canonical_windows == 100) ...
        && (res.valid_canonical_windows == 100) ...
        && (res.invalid_canonical_windows == 0) ...
        && strcmp(res.mat_version, '-v7');

    winfo = whos('-file', f1);
    ok = ok && (numel(winfo) == 3) ...
        && all(ismember({'stream', 'kf', 'meta'}, {winfo.name}));
    s2 = load(f1);
    ok = ok && isequal(s2.stream, st) && isequal(s2.kf, kf) ...
        && isequal(s2.meta, meta_want);

    m2 = s2.meta;
    ew2 = m2.eligibility.windows;
    ok = ok && strcmp(m2.voyage_id, 'T015_SYNTH_VOYAGE_0001') ...
        && strcmp(m2.base_draw_id, 'T015_SYNTH_DRAW_0001') ...
        && strcmp(m2.split_group_id, 'T015_SYNTH_VOYAGE_0001') ...
        && strcmp(m2.forecast_quality_profile_id, 'GOOD_V1') ...
        && strcmp(m2.stratum, 'STRAT_ROUTINE_PATROL__NORMAL') ...
        && strcmp(m2.dataset_domain, 'FORMAL') ...
        && strcmp(m2.fixture_kind, 'NONE') ...
        && strcmp(m2.intervention, 'NONE') ...
        && strcmp(m2.eligibility.disposition, 'ACCEPT') ...
        && (ew2.expected_canonical_windows == 100);

    % negative: eligibility record disagrees with the artifact coverage
    f2 = local_tmpfile('t015_p6b');
    kf_bad = local_make_kf((310:10:1290)');
    [ok_a, id_a] = local_expect_error_id(@() ...
        mission_v3.producer_export('export_artifact', st, kf_bad, ...
        meta, jg, f2), 'GATE:MISSION_V3:EXPORT:ELIG_RECORD');

    % negative: formal without the FR-536 eligibility record
    f3 = local_tmpfile('t015_p6c');
    [ok_b, id_b] = local_expect_error_id(@() ...
        mission_v3.producer_export('export_artifact', st, kf, meta, ...
        [], f3), 'GATE:MISSION_V3:EXPORT:META');

    % negative: missing C-D1 extension field
    meta_nd = rmfield(meta, 'base_draw_id');
    f4 = local_tmpfile('t015_p6d');
    [ok_c, id_c] = local_expect_error_id(@() ...
        mission_v3.producer_export('export_artifact', st, kf, meta_nd, ...
        jg, f4), 'GATE:MISSION_V3:EXPORT:META');

    % negative: stream missing a schema-required truth_stream column
    st_bad = rmfield(st, 'P_gen_actual_kW');
    f5 = local_tmpfile('t015_p6e');
    [ok_d, id_d] = local_expect_error_id(@() ...
        mission_v3.producer_export('export_artifact', st_bad, kf, meta, ...
        jg, f5), 'GATE:MISSION_V3:EXPORT:LAYOUT');

    % negative: tampered lead grid
    kf_t = kf;
    kt = kf_t.lead_s;
    kt(1, :) = 9:10:299;
    kf_t.lead_s = kt;
    f6 = local_tmpfile('t015_p6f');
    [ok_e, id_e] = local_expect_error_id(@() ...
        mission_v3.producer_export('export_artifact', st, kf_t, meta, ...
        jg, f6), 'GATE:MISSION_V3:EXPORT:LAYOUT');

    if exist(f1, 'file'), delete(f1); end
    ok_p6 = ok && ok_a && ok_b && ok_c && ok_d && ok_e;
    fprintf(['[T] P6 roundtrip: vars3=%d fields=%d cd1=%d neg ', ...
        'ELIG=%d META=%d/%d LAYOUT=%d/%d: %s\n'], ...
        (numel(winfo) == 3), isequal(s2.meta, meta_want), ...
        strcmp(m2.eligibility.disposition, 'ACCEPT'), ...
        ok_a, ok_b, ok_c, ok_d, ok_e, local_verdict(ok_p6));
catch p6_err
    fprintf('[T] P6 exception: %s\n', p6_err.message);
end
n_fail = n_fail + ~ok_p6;

%% P7 (C.7): retry record structure + discrimination
ok_p7 = false;
try
    r1 = mission_v3.eligibility('retry_record', 'SLOT_T015_F0001', 2, ...
        'FAILED_CONTRACT', 1);
    ok = strcmp(r1.record_kind, 'RETRY') ...
        && strcmp(r1.slot_id, 'SLOT_T015_F0001') ...
        && (r1.attempt == 2) ...
        && strcmp(r1.disposition, 'FAILED_CONTRACT') ...
        && (r1.retry_of == 1) ...
        && strcmp(r1.seed_policy, 'SAME_SLOT_SAME_CONFIG_SAME_SCIENCE_SEED');

    r2 = mission_v3.eligibility('retry_record', 'SLOT_T015_F0001', 3, ...
        'FAILED_GENERATION', 2);
    ok = ok && (r2.attempt == 3) && (r2.retry_of == 2) ...
        && strcmp(r2.disposition, 'FAILED_GENERATION') ...
        && strcmp(r2.slot_id, r1.slot_id);

    [ok_a, id_a] = local_expect_error_id(@() ...
        mission_v3.eligibility('retry_record', 'SLOT_R', 1, ...
        'REJECTED_SAMPLING_ELIGIBILITY', 0), ...
        'GATE:MISSION_V3:ELIGIBILITY:RETRY');
    [ok_b, id_b] = local_expect_error_id(@() ...
        mission_v3.eligibility('retry_record', 'SLOT_R', 1, ...
        'REJECTED_STRUCTURAL_HELDOUT', 0), ...
        'GATE:MISSION_V3:ELIGIBILITY:RETRY');
    [ok_c, id_c] = local_expect_error_id(@() ...
        mission_v3.eligibility('retry_record', 'SLOT_A', 1, ...
        'ACCEPT', 0), 'GATE:MISSION_V3:ELIGIBILITY:RETRY');
    [ok_d, id_d] = local_expect_error_id(@() ...
        mission_v3.eligibility('retry_record', 'SLOT_F', 0, ...
        'FAILED_CONTRACT', 1), 'GATE:MISSION_V3:ELIGIBILITY:BAD_ARG');
    [ok_e, id_e] = local_expect_error_id(@() ...
        mission_v3.eligibility('retry_record', 'SLOT_F', 1, ...
        'NOT_A_CLASS', 1), 'GATE:MISSION_V3:ELIGIBILITY:TAXONOMY');
    ok_p7 = ok && ok_a && ok_b && ok_c && ok_d && ok_e;
    fprintf(['[T] P7 retry: chain %d/%d rows REJECTED_no=%d/%d/%d ', ...
        'badarg=%d tax=%d: %s\n'], (r1.attempt == 2), ...
        (r2.retry_of == 2), ok_a, ok_b, ok_c, ok_d, ok_e, ...
        local_verdict(ok_p7));
catch p7_err
    fprintf('[T] P7 exception: %s\n', p7_err.message);
end
n_fail = n_fail + ~ok_p7;

%% P8 (C.8): determinism (two runs field-identical)
ok_p8 = false;
try
    st = local_make_stream(1590);
    kf = local_make_kf(ANCH);
    meta = local_make_meta('FORMAL', 'NONE');
    voy = local_make_voy(1590, ANCH);

    j1 = mission_v3.eligibility('judge', voy);
    j2 = mission_v3.eligibility('judge', voy);
    det_judge = isequal(j1, j2);

    f1 = local_tmpfile('t015_p8a');
    f2 = local_tmpfile('t015_p8b');
    mission_v3.producer_export('export_artifact', st, kf, meta, j1, f1);
    mission_v3.producer_export('export_artifact', st, kf, meta, j2, f2);
    a = load(f1);
    b = load(f2);
    det_mat = isequal(a.stream, b.stream) && isequal(a.kf, b.kf) ...
        && isequal(a.meta, b.meta);
    if exist(f1, 'file'), delete(f1); end
    if exist(f2, 'file'), delete(f2); end
    ok_p8 = det_judge && det_mat;
    fprintf('[T] P8 determinism: judge=%d mat=%d: %s\n', ...
        det_judge, det_mat, local_verdict(ok_p8));
catch p8_err
    fprintf('[T] P8 exception: %s\n', p8_err.message);
end
n_fail = n_fail + ~ok_p8;

%% summary
fprintf('[T] test_producer_export: %d/8 cases passed\n', 8 - n_fail);
if n_fail > 0
    error('GATE:test_producer_export %d case(s) failed', n_fail);
end
fprintf('test_producer_export: ALL PASS (8/8)\n');

%% ======================== local functions ==========================

function voy = local_make_voy(dur_s, anchors)
    voy = struct('duration_s', double(dur_s), ...
        'kf_issue_time_s', double(anchors(:)));
end

function st = local_make_stream(dur_s)
% Synthetic 1 Hz truth stream (FD7) with registry/schema-anchored signal
% names; analytic deterministic content, no RNG.
    t = (0:1.0:double(dur_s))';
    n = numel(t);
    st = struct('time_s', t);
    st.P_load_total_kW = 400.0 + 20.0 .* sin(t ./ 60.0);
    st.v_ship_mps = 2.0 + 0.1 .* sin(t ./ 120.0);
    st.acceleration_mps2 = 0.01 .* cos(t ./ 120.0);
    st.n_prop_rpm = 300.0 + 10.0 .* sin(t ./ 90.0);
    st.P_hotel_kW = 100.0 .* ones(n, 1);
    st.v_current_mps = 0.4 + 0.05 .* sin(t ./ 233.0);
    st.water_depth_m = 10.0 .* ones(n, 1);
    st.P_prop_demand_kW = 250.0 + 15.0 .* sin(t ./ 45.0);
    st.ctrl_mode = zeros(n, 1);
    st.v_ref_mps = 2.0 .* ones(n, 1);
    st.manual_power_cmd_pct = zeros(n, 1);
    st.P_gen_actual_kW = 390.0 + 18.0 .* sin(t ./ 61.0);
    st.scenario_id = 'T015_SYNTH_SCENE';
    st.resistance_multiplier = 1.0 .* ones(n, 1);
end

function kf = local_make_kf(anchors)
% Issue-major export_pilot layout with deterministic content (all rows
% fully covered: finite everywhere).
    a = double(anchors(:));
    m = numel(a);
    leads = 10:10:300;
    kf = struct('issue_time_s', a, 'lead_s', repmat(leads, m, 1));
    kf.v_plan_mps = 2.0 .* ones(m, 30);
    kf.s_plan_m = repmat(2.0 .* a, 1, 30) + repmat(20.0 .* (1.0:30.0), m, 1);
    kf.h_chart_m = 10.0 .* ones(m, 30);
    kf.P_hotel_plan_kW = 100.0 .* ones(m, 30);
    kf.u_fcst_mps = repmat(0.4 + 0.05 .* sin(a ./ 233.0), 1, 30);
    kf.wl_fcst_m = 10.0 .* ones(m, 30);
end

function meta = local_make_meta(domain, fixture)
% Full producer-schema metadata face + C-D1 extension fields (DGN-15).
    meta = struct();
    meta.voyage_id = 'T015_SYNTH_VOYAGE_0001';
    meta.voyage_family_id = 'T015_SYNTH_VOYAGE_0001';
    meta.route_template_id = 'ROUTE_A';
    meta.mission_template_id = 'ROUTINE_PATROL';
    meta.split_group_id = 'T015_SYNTH_VOYAGE_0001';
    meta.scenario_stratum = 'STRAT_ROUTINE_PATROL__NORMAL';
    meta.generation_seed = 4150001;
    meta.model_baseline_ref = 'ModelE physical v1.1 + control v1.0';
    meta.producer_script_sha = ...
        '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef';
    meta.fixture_kind = fixture;
    meta.bridge_zones = [];
    meta.speed_limit_zones = [];
    meta.ood_axis = 'NONE';
    meta.ood_def_version = 'C-D5-v1';
    meta.physics_domain_ref = 'ModelE validated domain (T-412 v1.1)';
    prov = struct();
    prov.forecast_generator_id = 't015_test_generator';
    prov.forecast_generator_version = '1.0.0';
    prov.forecast_generator_hash = ...
        'fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210';
    prov.issue_time_s = 300.0;
    prov.source_cutoff_time_s = 300.0;
    prov.forecast_provenance_class = 'CAUSAL_GENERATOR';
    meta.forecast_provenance = prov;
    meta.base_draw_id = 'T015_SYNTH_DRAW_0001';
    meta.intervention = 'NONE';
    meta.forecast_quality_profile_id = 'GOOD_V1';
    meta.stratum = 'STRAT_ROUTINE_PATROL__NORMAL';
    meta.dataset_domain = domain;
    meta.voyage_duration_s = 1590.0;
    meta.scenario_id = 'T015_SYNTH_SCENE';
end

function f = local_tmpfile(tag)
    f = fullfile(tempdir, sprintf('%s_%d.mat', tag, feature('getpid')));
    if exist(f, 'file')
        delete(f);
    end
end

function verdict = local_verdict(ok)
    if ok
        verdict = 'PASS';
    else
        verdict = 'FAIL';
    end
end

function [ok, id] = local_expect_error_id(fh, want_id)
% Expect the call to raise exactly the wanted GATE-prefixed error id.
    ok = false;
    id = 'NO_ERROR_RAISED';
    try
        fh();
    catch err
        ident = char(string(err.identifier));
        id = ident;
        ok = strcmp(ident, want_id);
    end
end
