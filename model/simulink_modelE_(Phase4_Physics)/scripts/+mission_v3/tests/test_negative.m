% test_negative.m - T-017: AC-436 causal/isolation negative injection suite
%   (FR-524, FR-526~533, FR-558~562, FR-504; design DGN-06/07/08 Error
%   Handling; delegation spec experiments/mission_v3/T-017_delegation_spec.md
%   Fixed Decisions FD1-FD5).
%
% Integration-level suite: every injection is built on the REAL component
% chain (dag_sample -> env_realize -> plan_compose -> scenario_compile ->
% forecast_issue -> kf_snapshots -> eligibility / producer_export /
% family_expand).  One accepted ROUTINE_PATROL x NORMAL draw assembles the
% full chain first (positive chain assembly, FD2); its intermediate
% products feed all ten injection cases.  No tested component is ever
% modified: an undetected injection is a component defect and fails the
% suite (FD5 / delegation spec Escalate If).
%
% Cases (FD1 ten-item AC-436 checklist; expected detection points):
%   N1  future-truth masquerading as INPUT_F
%         -> GATE:MISSION_V3:FCST:FUTURE_INPUT
%   N2  realized future pushed into the s_plan/v_plan signature
%         -> GATE:MISSION_V3:KF:BAD_ARG
%   N3  constraints along realized positions (dual assertion, FR-560):
%       (a) tamper the realized t > t0 segment, re-issue the K snapshot
%           from the same plan -> output bitwise unchanged;
%       (b) tamper plan.zones -> output changes
%         -> STRUCTURAL_ASSERTION_FR560
%   N4  lead grid offset 9:10:300 and vintage inversion (Fblk issued at
%       t0 feeding a row issued at t0+60)
%         -> GATE:MISSION_V3:KF:GRID / GATE:MISSION_V3:KF:VINTAGE
%   N5  missing snapshots silently filled: build with leads beyond the
%       plan end; missing entries must be NaN + missing_mask +
%       FAILED_CONTRACT, non-missing entries bitwise equal to a
%       fully-covered reference run on the extended plan (no fill)
%         -> STRUCTURAL_ASSERTION_FR562
%   N6  simulator-latent-privileged forecast source (DEGRADED_V1 issue
%       with SourceClass = SIMULATOR_LATENT_PRIVILEGED)
%         -> GATE:MISSION_V3:FCST:SOURCE_CLASS
%   N7  undeclared cross-family truth reuse: two real family_expand
%       products (FamilySeq 1 and 2) of one base draw share the truth
%       lineage but declare different families
%         -> GATE:MISSION_V3:FAMILY:UNDECLARED_REUSE
%   N8  GOOD/DEGRADED truth drift: one real family member declares the
%       truth hash recomputed from a minimally tampered realization
%         -> GATE:MISSION_V3:FAMILY:TRUTH_DRIFT
%   N9  PILOT_ONLY mixed into the formal path (stage_check face)
%         -> GATE:MISSION_V3:EXPORT:STAGE_ISOLATION
%   N10 FORBIDDEN/ORACLE sources into the forecast view, one per
%       forbidden class (FUTURE_TRUTH / ORACLE_STATE /
%       SIMULATOR_LATENT_PRIVILEGED)
%         -> GATE:MISSION_V3:FCST:SOURCE_CLASS
%
% Positive control (FD4): before every injection the same-chain call
% without the injection is executed and must succeed; a case only counts
% as detected when its positive control passed AND the expected gate id
% matched exactly.
%
% Report (FD3): results/mission_v3/negative_detection_report.json with
% {schema, generated_at, cases(10 x {case_id, ac436_item,
% injection_summary, expected_gate_id, observed_gate_id, detected}),
% detection_rate, notes}.  detection_rate < 1.0 raises
% error('GATE:test_negative ...').
%
% ac436_item transcription note: the AC-436 clause is written in Chinese
% in .specs/phase4-c-mission-v3/requirements.md; this ASCII-only script
% records a faithful English transcription plus the FR references.
%
% Run (from the Model E workspace root):
%   matlab -batch "run('scripts/+mission_v3/tests/test_negative.m')"
% Discipline: ASCII only; no absolute paths (the chain export artifact
% goes to tempdir and is deleted); no chained indexing; no global RNG;
% the only results/ write is the detection report (FD5 / T-017 spec).

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

REPORT_PATH = fullfile(ROOT, 'results', 'mission_v3', ...
    'negative_detection_report.json');
FS = 't017_negative_seed_01';           % test fixture, not a frozen value
TPL = 'ROUTINE_PATROL';
REG = 'NORMAL';
STRAT = 'STRAT_ROUTINE_PATROL__NORMAL'; % caller label (quota layer)
LEADS = 10:10:300;                      % frozen FR-561 grid (reference)

%% ================= 0. real-chain assembly (FD2) ======================
fprintf('[N] assembling the real chain (%s x %s) ...\n', TPL, REG);
max_scan = 60;
rl = [];
dag = [];
for k = 0:(max_scan - 1)
    r = mission_v3.env_realize('sample', FS, k, TPL, REG, STRAT, ...
        'MaxAttempts', 1, 'ExhaustAction', 'return_ledger');
    if ~r.accepted
        continue;
    end
    rl = r.realization;
    row_ok = r.ledger(r.n_attempts);
    dag = row_ok.draw;
    break;
end
if isempty(rl) || ~strcmp(rl.disposition.disposition, 'ACCEPT')
    error('GATE:test_negative chain assembly failed: no accepted %s x %s draw within %d scans', ...
        TPL, REG, max_scan);
end
plan = mission_v3.plan_compose('compose', rl);
scn_out = mission_v3.scenario_compile('compile', plan, rl);
if ~strcmp(scn_out.scn.speed_profile_mode, 'time') ...
        || ~mission_v3.scenario_compile('check_injection', ...
        scn_out.injection_list)
    error('GATE:test_negative chain assembly failed: scenario_compile output invalid');
end
fam = mission_v3.family_expand('expand', dag, 'Realization', rl);

D = plan.t_grid_s(end);                 % plan horizon (integer seconds)
t0 = 600;                               % chain issue time (t0 + 300 <= D)
if (D < 910) || (t0 + 300 > D)
    error('GATE:test_negative chain assembly failed: plan horizon %d s too short for the canonical window geometry', D);
end
tg_rl = rl.t_grid_s;
ur_rl = rl.u_river_mps;
h_chart = rl.water.h_chart_m;

% causal slice at t0 (R1 derived along-track component, outbound sign;
% forecast_issue is direction-agnostic, T-013 test precedent)
sl = tg_rl <= t0;
fb_pos = mission_v3.forecast_issue('issue', -ur_rl(sl), tg_rl(sl), t0, ...
    LEADS, 'GOOD_V1');

% canonical anchors and the full issue-major snapshot grid
anchors = (300:10:(D - 300))';
n_anch = numel(anchors);
fb_all = [];
for m = 1:n_anch
    a_m = anchors(m);
    sl_m = tg_rl <= a_m;
    fb_m = mission_v3.forecast_issue('issue', -ur_rl(sl_m), tg_rl(sl_m), ...
        a_m, LEADS, 'GOOD_V1');
    fb_all = [fb_all; fb_m]; %#ok<AGROW>
end
kf_all = mission_v3.kf_snapshots('build_grid', plan, anchors, fb_all, ...
    'HChartM', h_chart);
kf_pos = mission_v3.kf_snapshots('build', plan, t0, fb_pos, ...
    'HChartM', h_chart);
if ~strcmp(kf_all.contract_status{n_anch}, 'OK') ...
        || ~strcmp(kf_pos.contract_status{1}, 'OK')
    error('GATE:test_negative chain assembly failed: K/F snapshots not fully covered');
end

% voyage qualification judge on the artifact coverage (chain member)
voy = struct('duration_s', double(D), 'kf_issue_time_s', anchors);
elig = mission_v3.eligibility('judge', voy);
if ~strcmp(elig.disposition, 'ACCEPT')
    error('GATE:test_negative chain assembly failed: eligibility judged %s/%s, expected ACCEPT', ...
        elig.disposition, elig.reason);
end

% formal export of the chain artifact (positive; tempdir, then deleted)
meta_chain = local_make_meta(plan, fam, fb_pos, D, 'FORMAL', 'NONE');
stream_chain = local_make_stream(D);
f_art = fullfile(tempdir, sprintf('t017_chain_%d.mat', feature('getpid')));
if exist(f_art, 'file'), delete(f_art); end
res_exp = mission_v3.producer_export('export_artifact', stream_chain, ...
    kf_all, meta_chain, elig, f_art);
if ~res_exp.ok || ~res_exp.roundtrip_ok || ~exist(f_art, 'file')
    error('GATE:test_negative chain assembly failed: producer export not clean');
end
delete(f_art);
fprintf(['[N] chain assembled: D=%d s t0=%d s anchors=%d fam=%s ', ...
    'export=OK\n'], D, t0, n_anch, fam.family_id);

%% ================== case bookkeeping (FD3) ==========================
CASE_IDS = {'N1', 'N2', 'N3', 'N4', 'N5', 'N6', 'N7', 'N8', 'N9', 'N10'};
AC_ITEMS = { ...
    'AC-436: future-truth masquerading as INPUT_F (FR-526/530)', ...
    'AC-436: realized future masquerading as s_plan/v_plan (FR-558/559)', ...
    'AC-436: position constraints taken along realized future positions (FR-560)', ...
    'AC-436: lead grid offset or vintage inversion (FR-561)', ...
    'AC-436: formal missing snapshots silently filled (FR-562)', ...
    'AC-436: simulator-latent-privileged forecast source (FR-527)', ...
    'AC-436: undeclared cross-split_group truth reuse (FR-524)', ...
    'AC-436: GOOD/DEGRADED truth drift (FR-529)', ...
    'AC-436: PILOT_ONLY mixed into the formal path (FR-500/504)', ...
    'AC-436: FORBIDDEN/ORACLE sources into the view, per class (FR-527)'};
EXP_IDS = { ...
    'GATE:MISSION_V3:FCST:FUTURE_INPUT', ...
    'GATE:MISSION_V3:KF:BAD_ARG', ...
    'STRUCTURAL_ASSERTION_FR560', ...
    'GATE:MISSION_V3:KF:GRID / GATE:MISSION_V3:KF:VINTAGE', ...
    'STRUCTURAL_ASSERTION_FR562', ...
    'GATE:MISSION_V3:FCST:SOURCE_CLASS', ...
    'GATE:MISSION_V3:FAMILY:UNDECLARED_REUSE', ...
    'GATE:MISSION_V3:FAMILY:TRUTH_DRIFT', ...
    'GATE:MISSION_V3:EXPORT:STAGE_ISOLATION', ...
    'GATE:MISSION_V3:FCST:SOURCE_CLASS'};
SUMMARIES = cell(1, 10);
OBS_IDS = cell(1, 10);
DET = false(1, 10);

%% ==================== N1 (FCST:FUTURE_INPUT) ========================
% positive control: the causal truncated slice issues cleanly (fb_pos)
pos_n1 = ~isempty(fb_pos.u_fcst_mps) && strcmp(fb_pos.profile_id, 'GOOD_V1');
[hit_n1, id_n1] = local_expect_error_id(@() ...
    mission_v3.forecast_issue('issue', -ur_rl, tg_rl, t0, LEADS, ...
    'GOOD_V1'), 'GATE:MISSION_V3:FCST:FUTURE_INPUT');
SUMMARIES{1} = sprintf(['full realized t_grid (max=%.0f s > t0=%d s) ', ...
    'passed as the INPUT_F slice; positive control = truncated slice issue'], ...
    max(tg_rl), t0);
OBS_IDS{1} = id_n1;
DET(1) = pos_n1 && hit_n1;
fprintf('[N] N1 FUTURE_INPUT: pos=%d hit=%d id=%s\n', pos_n1, hit_n1, id_n1);

%% ==================== N2 (KF:BAD_ARG signature) =====================
% positive control: the (plan, t0, Fblk) signature builds cleanly (kf_pos)
pos_n2 = strcmp(kf_pos.contract_status{1}, 'OK');
[hit_n2, id_n2] = local_expect_error_id(@() ...
    mission_v3.kf_snapshots('build', plan, t0, fb_pos, rl), ...
    'GATE:MISSION_V3:KF:BAD_ARG');
SUMMARIES{2} = sprintf(['realization struct pushed as a 4th positional ', ...
    'argument of kf_snapshots build (causal signature admits no ', ...
    'realized-future argument); positive control = clean build at t0=%d'], ...
    t0);
OBS_IDS{2} = id_n2;
DET(2) = pos_n2 && hit_n2;
fprintf('[N] N2 KF:BAD_ARG: pos=%d hit=%d id=%s\n', pos_n2, hit_n2, id_n2);

%% ========== N3 (FR-560 dual assertion: zones come from plan) ========
% direction (a): tamper ONLY the realized future (t > t0), re-derive the
% Fblk from the identical truncated slice and rebuild the K snapshot on
% the SAME plan: the snapshot must be bitwise unchanged.
rl_t = rl;
ur_t = rl_t.u_river_mps;
fut = tg_rl > t0;
ur_t(fut) = ur_t(fut) + 0.5;
rl_t.u_river_mps = ur_t;
tampered_n3 = any(fut) && ~isequal(ur_rl, rl_t.u_river_mps);
sl_t = tg_rl <= t0;
fb_t = mission_v3.forecast_issue('issue', -rl_t.u_river_mps(sl_t), ...
    tg_rl(sl_t), t0, LEADS, 'GOOD_V1');
slice_inv_n3 = isequal(fb_pos, fb_t);   % t <= t0 information unchanged
kf_t = mission_v3.kf_snapshots('build', plan, t0, fb_t, 'HChartM', h_chart);
inv_n3 = isequal(kf_pos, kf_t);         % bitwise invariance (FR-560)

% direction (b): the plan IS the zone fact source - append a speed-limit
% window around one previously free lead and the snapshot must change.
slz_all = kf_pos.in_speed_limit_zone;
kz = find(~slz_all, 1, 'first');
if isempty(kz)
    bflg_all = kf_pos.in_bridge_zone;
    kz = find(~bflg_all, 1, 'first');
end
sens_n3 = false;
if isempty(kz)
    % defensive: every lead already inside a zone - clearing the plan
    % zones must then change the snapshot
    plan_z = plan;
    plan_z.speed_limit_zones = repmat(struct('segment_index', 0, ...
        'role', '', 'reach', '', 't_start_s', 0.0, 't_end_s', 0.0, ...
        'v_limit_mps', 0.0, 'reason', ''), 1, 0);
    kf_z = mission_v3.kf_snapshots('build', plan_z, t0, fb_pos, ...
        'HChartM', h_chart);
    sens_n3 = ~isequal(kf_pos, kf_z);
else
    tk = t0 + LEADS(kz);
    plan_z = plan;
    z_new = struct('segment_index', 0, 'role', 'NEGTEST', 'reach', '', ...
        't_start_s', tk - 5, 't_end_s', tk + 5, 'v_limit_mps', 1.0, ...
        'reason', 'T017_INJECTION');
    if isempty(plan.speed_limit_zones)
        plan_z.speed_limit_zones = z_new;
    else
        plan_z.speed_limit_zones = [plan.speed_limit_zones, z_new];
    end
    kf_z = mission_v3.kf_snapshots('build', plan_z, t0, fb_pos, ...
        'HChartM', h_chart);
    zz = kf_z.in_speed_limit_zone;
    sens_n3 = ~isequal(kf_pos, kf_z) && zz(kz);
end
pos_n3 = pos_n2;    % same-chain positive control (clean build)
SUMMARIES{3} = sprintf(['realized t>t0 segment shifted by 0.5 m/s and ', ...
    'the K snapshot rebuilt from the same plan: bitwise unchanged=%d; ', ...
    'plan.zones window injected at t0+%d s: output changed=%d'], ...
    inv_n3, LEADS(kz), sens_n3);
OBS_IDS{3} = 'STRUCTURAL_ASSERTION_FR560';
DET(3) = pos_n3 && tampered_n3 && inv_n3 && sens_n3;
fprintf(['[N] N3 FR-560 dual: tampered=%d bitwise_inv=%d ', ...
    'plan_sensitive=%d: %d\n'], tampered_n3, inv_n3, sens_n3, DET(3));

%% ============ N4 (KF:GRID lead offset / KF:VINTAGE inversion) =======
% positive control: clean build (kf_pos)
pos_n4 = pos_n2;
% (a) lead grid offset: a REAL forecast_issue product on 9:10:299
fb_grid = mission_v3.forecast_issue('issue', -ur_rl(sl), tg_rl(sl), t0, ...
    9:10:300, 'GOOD_V1');
[hit_grid, id_grid] = local_expect_error_id(@() ...
    mission_v3.kf_snapshots('build', plan, t0, fb_grid, ...
    'HChartM', h_chart), 'GATE:MISSION_V3:KF:GRID');
% (b) vintage inversion: the Fblk issued at t0 feeds the t0+60 row
[hit_vint, id_vint] = local_expect_error_id(@() ...
    mission_v3.kf_snapshots('build', plan, t0 + 60, fb_pos, ...
    'HChartM', h_chart), 'GATE:MISSION_V3:KF:VINTAGE');
SUMMARIES{4} = sprintf(['Fblk issued on the offset lead grid 9:10:299 ', ...
    'fed to build (hit=%s); Fblk issued at t0=%d fed to the t0=%d row ', ...
    '(hit=%s); positive control = clean build'], id_grid, t0, t0 + 60, ...
    id_vint);
OBS_IDS{4} = [id_grid, ';', id_vint];
DET(4) = pos_n4 && hit_grid && hit_vint;
fprintf('[N] N4 GRID/VINTAGE: pos=%d grid=%d(%s) vint=%d(%s)\n', ...
    pos_n4, hit_grid, id_grid, hit_vint, id_vint);

%% ========= N5 (FR-562 missing snapshots are never filled) ===========
% positive control: the same chain at a fully covered issue time
t0_c = 600;
kf_clean = mission_v3.kf_snapshots('build', plan, t0_c, fb_pos, ...
    'HChartM', h_chart);
pos_n5 = strcmp(kf_clean.contract_status{1}, 'OK') ...
    && ~any(kf_clean.missing_mask(:));
% injection: issue near the plan end so leads 160..300 fall beyond it
t0_o = D - 150;
sl_o = tg_rl <= t0_o;
fb_o = mission_v3.forecast_issue('issue', -ur_rl(sl_o), tg_rl(sl_o), ...
    t0_o, LEADS, 'GOOD_V1');
kf_o = mission_v3.kf_snapshots('build', plan, t0_o, fb_o, ...
    'HChartM', h_chart);
% reference run: the same (t0_o, fb_o) on a plan extended with its last
% plan speed - this is what a silent fill WOULD have produced
extra = t0_o + LEADS(end) - D;
plan_ext = plan;
tg_ext = plan.t_grid_s;
vp_ext = plan.v_plan_mps;
v_last = vp_ext(end);
plan_ext.t_grid_s = 0:(D + extra);
plan_ext.v_plan_mps = [vp_ext, repmat(v_last, 1, extra)];
kf_ext = mission_v3.kf_snapshots('build', plan_ext, t0_o, fb_o, ...
    'HChartM', h_chart);
want_mask = (t0_o + LEADS) > D;
mm_o = kf_o.missing_mask;
cs_o = kf_o.contract_status{1};
SIX = {'v_plan_mps', 's_plan_m', 'h_chart_m', 'P_hotel_plan_kW', ...
    'u_fcst_mps', 'wl_fcst_m'};
mask_ok_n5 = isequal(mm_o, want_mask) && strcmp(cs_o, 'FAILED_CONTRACT');
nan_ok_n5 = true;
keep_ok_n5 = true;
ref_finite_n5 = true;
for j = 1:numel(SIX)
    col_o = getfield(kf_o, SIX{j});
    col_e = getfield(kf_ext, SIX{j});
    nan_ok_n5 = nan_ok_n5 && all(isnan(col_o(want_mask)));
    keep_ok_n5 = keep_ok_n5 && isequal(col_o(~want_mask), ...
        col_e(~want_mask));            % non-missing bitwise = clean run
    ref_finite_n5 = ref_finite_n5 && all(isfinite(col_e(want_mask)));
end
zone_ok_n5 = ~any(kf_o.in_speed_limit_zone(want_mask)) ...
    && ~any(kf_o.in_bridge_zone(want_mask));
SUMMARIES{5} = sprintf(['issue t0=%d with leads beyond the plan end ', ...
    '(D=%d): missing_mask exact=%d, contract=%s, missing entries all ', ...
    'NaN=%d, non-missing entries bitwise equal to the fully covered ', ...
    'reference run=%d (reference finite on missing leads=%d, zone ', ...
    'flags false=%d); positive control = fully covered t0=%d build'], ...
    t0_o, D, mask_ok_n5, cs_o, nan_ok_n5, keep_ok_n5, ref_finite_n5, ...
    zone_ok_n5, t0_c);
OBS_IDS{5} = 'STRUCTURAL_ASSERTION_FR562';
DET(5) = pos_n5 && mask_ok_n5 && nan_ok_n5 && keep_ok_n5 ...
    && ref_finite_n5 && zone_ok_n5;
fprintf(['[N] N5 FR-562 no-fill: pos=%d mask=%d nan=%d keep=%d ', ...
    'ref=%d zone=%d\n'], pos_n5, mask_ok_n5, nan_ok_n5, keep_ok_n5, ...
    ref_finite_n5, zone_ok_n5);

%% ============ N6 (FCST:SOURCE_CLASS simulator-latent) ===============
% positive control: the legal DEGRADED_V1 issue on the same slice
pos_n6 = false;
fb_deg = mission_v3.forecast_issue('issue', -ur_rl(sl), tg_rl(sl), t0, ...
    LEADS, 'DEGRADED_V1', 'FormalSeed', FS, 'BaseDrawIndex', 0);
pos_n6 = ~isempty(fb_deg.u_fcst_mps) ...
    && strcmp(fb_deg.provenance.source_class, 'CAUSALLY_ESTIMATED');
[hit_n6, id_n6] = local_expect_error_id(@() ...
    mission_v3.forecast_issue('issue', -ur_rl(sl), tg_rl(sl), t0, LEADS, ...
    'DEGRADED_V1', 'FormalSeed', FS, 'BaseDrawIndex', 0, ...
    'SourceClass', 'SIMULATOR_LATENT_PRIVILEGED'), ...
    'GATE:MISSION_V3:FCST:SOURCE_CLASS');
SUMMARIES{6} = ['DEGRADED_V1 issue declaring a ', ...
    'SIMULATOR_LATENT_PRIVILEGED observation source (deployment ', ...
    'unobservable simulator latent state); positive control = same ', ...
    'issue with the default CAUSALLY_ESTIMATED class'];
OBS_IDS{6} = id_n6;
DET(6) = pos_n6 && hit_n6;
fprintf('[N] N6 SOURCE_CLASS latent: pos=%d hit=%d id=%s\n', ...
    pos_n6, hit_n6, id_n6);

%% ======== N7 (FAMILY:UNDECLARED_REUSE cross-family truth) ===========
% positive control: the single-family real registry audits cleanly
au_pos = mission_v3.family_expand('audit', fam.audit_registry);
pos_n7 = au_pos.ok;
% injection: a second REAL expand product (FamilySeq 2) of the same base
% draw and the same realized truth - different declared family, identical
% truth lineage
fam2 = mission_v3.family_expand('expand', dag, 'FamilySeq', 2, ...
    'Realization', rl);
reg7 = [fam.audit_registry, fam2.audit_registry];
tl_same_n7 = isequal(fam.audit_registry(1).truth_lineage, ...
    fam2.audit_registry(1).truth_lineage);
fam_diff_n7 = ~strcmp(fam.family_id, fam2.family_id);
[hit_n7, id_n7] = local_expect_error_id(@() ...
    mission_v3.family_expand('audit', reg7), ...
    'GATE:MISSION_V3:FAMILY:UNDECLARED_REUSE');
SUMMARIES{7} = sprintf(['real family_expand products of one base draw ', ...
    '(FamilySeq 1 and 2, same realized truth lineage) audited together: ', ...
    'lineage identical=%d, declared families differ=%d; positive ', ...
    'control = single-family audit'], tl_same_n7, fam_diff_n7);
OBS_IDS{7} = id_n7;
DET(7) = pos_n7 && tl_same_n7 && fam_diff_n7 && hit_n7;
fprintf('[N] N7 UNDECLARED_REUSE: pos=%d hit=%d id=%s\n', ...
    pos_n7, hit_n7, id_n7);

%% ============= N8 (FAMILY:TRUTH_DRIFT same lineage) =================
% positive control: the real registry audits cleanly (same as N7 face)
pos_n8 = pos_n7;
% injection: recompute the truth hash from a minimally tampered
% realization (one u sample + 1e-3 m/s) and let the INFORMATION member
% (traffic ON x forecast DEGRADED_V1) declare it
rl_d = rl;
ur_d = rl_d.u_river_mps;
ur_d(5) = ur_d(5) + 1.0e-3;
rl_d.u_river_mps = ur_d;
h_tam = mission_v3.family_expand('truth_hash', dag, rl_d);
h_ref = mission_v3.family_expand('truth_hash', dag, rl);
sens_n8 = ~strcmp(h_tam, h_ref);
reg8 = fam.audit_registry;
row8 = reg8(2);                          % member 2: (ON, DEGRADED_V1)
row8.truth_hash = h_tam;
reg8(2) = row8;
[hit_n8, id_n8] = local_expect_error_id(@() ...
    mission_v3.family_expand('audit', reg8), ...
    'GATE:MISSION_V3:FAMILY:TRUTH_DRIFT');
SUMMARIES{8} = sprintf(['one family member (ON x DEGRADED_V1) declares ', ...
    'the truth hash recomputed from a minimally tampered realization ', ...
    '(u(5) + 1e-3 m/s): hash changed=%d; positive control = ', ...
    'single-family audit'], sens_n8);
OBS_IDS{8} = id_n8;
DET(8) = pos_n8 && sens_n8 && hit_n8;
fprintf('[N] N8 TRUTH_DRIFT: pos=%d sens=%d hit=%d id=%s\n', ...
    pos_n8, sens_n8, hit_n8, id_n8);

%% ========= N9 (EXPORT:STAGE_ISOLATION PILOT_ONLY into formal) =======
% positive control: FORMAL + fixture_kind=NONE passes the FR-500 face
sc_pos = mission_v3.producer_export('stage_check', meta_chain);
pos_n9 = sc_pos.ok && strcmp(sc_pos.fixture_kind, 'NONE');
% injection: the same formal meta declaring PILOT_ONLY
meta_pi = meta_chain;
meta_pi.fixture_kind = 'PILOT_ONLY';
[hit_n9, id_n9] = local_expect_error_id(@() ...
    mission_v3.producer_export('stage_check', meta_pi), ...
    'GATE:MISSION_V3:EXPORT:STAGE_ISOLATION');
% and the export path refuses to write such an artifact at all
f_pi = fullfile(tempdir, sprintf('t017_pilot_%d.mat', feature('getpid')));
if exist(f_pi, 'file'), delete(f_pi); end
[hit_n9b, id_n9b] = local_expect_error_id(@() ...
    mission_v3.producer_export('export_artifact', stream_chain, kf_all, ...
    meta_pi, elig, f_pi), 'GATE:MISSION_V3:EXPORT:STAGE_ISOLATION');
no_write_n9 = ~exist(f_pi, 'file');
SUMMARIES{9} = sprintf(['FORMAL meta declaring fixture_kind=PILOT_ONLY ', ...
    'on the stage_check face (hit=%s) and on the export path, nothing ', ...
    'written=%d (hit=%s); positive control = FORMAL+NONE stage_check'], ...
    id_n9, no_write_n9, id_n9b);
OBS_IDS{9} = id_n9;
DET(9) = pos_n9 && hit_n9 && hit_n9b && no_write_n9;
fprintf('[N] N9 STAGE_ISOLATION: pos=%d check=%d export=%d nowrite=%d\n', ...
    pos_n9, hit_n9, hit_n9b, no_write_n9);

%% ==== N10 (FCST:SOURCE_CLASS forbidden classes into the view) =======
% positive control: GOOD_V1 default source class (OBSERVED) issues
pos_n10 = strcmp(fb_pos.provenance.source_class, 'OBSERVED');
FORB = {'FUTURE_TRUTH', 'ORACLE_STATE', 'SIMULATOR_LATENT_PRIVILEGED'};
hit_each_n10 = true(1, numel(FORB));
ids_n10 = cell(1, numel(FORB));
for q = 1:numel(FORB)
    [hit_each_n10(q), ids_n10{q}] = local_expect_error_id(@() ...
        mission_v3.forecast_issue('issue', -ur_rl(sl), tg_rl(sl), t0, ...
        LEADS, 'GOOD_V1', 'SourceClass', FORB{q}), ...
        'GATE:MISSION_V3:FCST:SOURCE_CLASS');
end
SUMMARIES{10} = sprintf(['GOOD_V1 issue with SourceClass overridden to ', ...
    'each forbidden class in turn (%s): all three rejected by the ', ...
    'observability whitelist; positive control = default OBSERVED ', ...
    'class issue'], strjoin(FORB, '/'));
OBS_IDS{10} = strjoin(ids_n10, ';');
DET(10) = pos_n10 && all(hit_each_n10);
fprintf('[N] N10 SOURCE_CLASS forbidden: pos=%d hits=%d/%d\n', ...
    pos_n10, sum(hit_each_n10), numel(FORB));

%% ================= summary, report, gate (FD3/FD5) ==================
n_det = sum(DET);
rate = double(n_det) / double(numel(DET));
fprintf('[N] ---- negative detection summary ----\n');
for q = 1:numel(DET)
    fprintf('[N] %s detected=%d expected=%s\n', CASE_IDS{q}, DET(q), ...
        EXP_IDS{q});
end
fprintf('[N] detection rate: %d/%d = %.2f\n', n_det, numel(DET), rate);

cases = repmat(struct('case_id', '', 'ac436_item', '', ...
    'injection_summary', '', 'expected_gate_id', '', ...
    'observed_gate_id', '', 'detected', false), 1, numel(DET));
for q = 1:numel(DET)
    row = struct('case_id', CASE_IDS{q}, 'ac436_item', AC_ITEMS{q}, ...
        'injection_summary', SUMMARIES{q}, 'expected_gate_id', ...
        EXP_IDS{q}, 'observed_gate_id', OBS_IDS{q}, 'detected', DET(q));
    cases(q) = row;
end
report = struct();
report.schema = 'mission_v3_negative_detection_report_v1';
report.generated_at = local_iso_utc();
report.cases = cases;
report.detection_rate = rate;
report.notes = ['T-017 AC-436 negative injection suite on the real ', ...
    'component chain (dag_sample -> env_realize -> plan_compose -> ', ...
    'scenario_compile -> forecast_issue -> kf_snapshots -> ', ...
    'eligibility / producer_export / family_expand); chain fixture: ', ...
    fam.family_id, ', template ', TPL, ', regime ', REG, ', plan ', ...
    'horizon ', sprintf('%d', D), ' s, chain issue time ', ...
    sprintf('%d', t0), ' s; every injection was preceded by a ', ...
    'same-chain positive control; ac436_item is a faithful ASCII ', ...
    'transcription of the Chinese AC-436 clause in ', ...
    '.specs/phase4-c-mission-v3/requirements.md (script discipline: ', ...
    'ASCII-only source); N3/N5 are structural assertions (FR-560/FR-562) ', ...
    'without a GATE error id by design'];

rdir = fileparts(REPORT_PATH);
if ~exist(rdir, 'dir')
    mkdir(rdir);
end
fid = fopen(REPORT_PATH, 'w');
if fid < 0
    error('GATE:test_negative cannot write the detection report: %s', ...
        REPORT_PATH);
end
fwrite(fid, jsonencode(report, 'PrettyPrint', true), 'char');
fclose(fid);
fprintf('[N] report written: results/mission_v3/negative_detection_report.json\n');

if n_det < numel(DET)
    error(['GATE:test_negative %d/%d AC-436 injections detected ', ...
        '(detection_rate=%.2f < 1.0); undetected items are component ', ...
        'defects, not test defects; see ', ...
        'results/mission_v3/negative_detection_report.json'], ...
        n_det, numel(DET), rate);
end
fprintf('test_negative: ALL DETECTED (%d/%d)\n', n_det, numel(DET));

%% ======================== local functions ===========================

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

function meta = local_make_meta(plan_s, fam_s, fb, dur_s, domain, fixture)
% Producer-schema metadata face + C-D1 extension fields (DGN-15), built
% from the real chain products (family/plan/forecast provenance).
    meta = struct();
    meta.voyage_id = fam_s.family_id;
    meta.voyage_family_id = fam_s.family_id;
    meta.route_template_id = 'ROUTE_A';
    meta.mission_template_id = char(plan_s.template_id);
    meta.split_group_id = fam_s.split_group_id;
    meta.scenario_stratum = char(plan_s.stratum_id);
    meta.generation_seed = 417017;
    meta.model_baseline_ref = 'ModelE physical v1.1 + control v1.0';
    meta.producer_script_sha = ...
        '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef';
    meta.fixture_kind = fixture;
    meta.bridge_zones = plan_s.bridge_zones;
    meta.speed_limit_zones = plan_s.speed_limit_zones;
    meta.ood_axis = 'NONE';
    meta.ood_def_version = 'C-D5-v1';
    meta.physics_domain_ref = 'ModelE validated domain (T-412 v1.1)';
    prov = struct();
    prov.forecast_generator_id = fb.provenance.generator_id;
    prov.forecast_generator_version = fb.provenance.version;
    prov.forecast_generator_hash = fb.provenance.hash;
    prov.issue_time_s = fb.provenance.issue_time_s;
    prov.source_cutoff_time_s = fb.provenance.source_cutoff_s;
    prov.forecast_provenance_class = fb.provenance.provenance_class_formal;
    meta.forecast_provenance = prov;
    meta.base_draw_id = fam_s.base_draw_id;
    meta.intervention = 'NONE';
    meta.forecast_quality_profile_id = fb.profile_id;
    meta.stratum = char(plan_s.stratum_id);
    meta.dataset_domain = domain;
    meta.voyage_duration_s = double(dur_s);
    meta.scenario_id = 'T017_NEGATIVE_SCENE';
end

function st = local_make_stream(dur_s)
% Synthetic 1 Hz truth stream (T-015 test precedent, FD7): analytic
% deterministic content with registry/schema-anchored signal names; the
% stream is a caller fact for the export chain member and is never an
% injection target of this suite.
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
    st.scenario_id = 'T017_NEGATIVE_SCENE';
    st.resistance_multiplier = 1.0 .* ones(n, 1);
end

function s = local_iso_utc()
% ISO-8601 UTC timestamp (report provenance only).
    dt = datetime('now', 'TimeZone', 'UTC');
    s = sprintf('%04d-%02d-%02dT%02d:%02d:%02dZ', year(dt), month(dt), ...
        day(dt), hour(dt), minute(dt), floor(second(dt)));
end
