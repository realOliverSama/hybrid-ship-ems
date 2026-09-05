% test_forecast_issue.m - T-012: causal forecast generator (DGN-06, FR-526~531)
%   P1 (spec B.1) analytic conditional mean vs numerical integration:
%       a GOOD_V1 issue consumes a real env_realize output slice
%       (u_river_mps/t_grid_s contract; upstream along-track mapping),
%       and u_fcst matches, to 1e-12 relative tolerance, a stepwise
%       exact-flow integration of the deterministic OU mean dynamics
%       dE[X]/dt = -(X - U_reach)/tau_u on the frozen 1 s injection grid
%       started from u_obs(t0); u_obs(t0) equals the slice value at t0.
%   P2 (spec B.2) machine-enforced causality gates (real injections):
%       a slice containing t > t0 points must raise
%       GATE:MISSION_V3:FCST:FUTURE_INPUT; a DEGRADED_V1 slice starting
%       after t0 - 1800 must raise GATE:MISSION_V3:FCST:NO_PREHISTORY
%       (R5: no lag without existing causal source history); a slice
%       starting exactly at t0 - 1800 is accepted (boundary inclusive).
%   P3 (spec B.3) GOOD decay shape: at h = tau_u the remaining distance
%       |u_fcst - U_reach| equals exp(-1)*|u_obs - U_reach| (equivalently
%       the decayed distance |u_obs - u_fcst| equals (1-exp(-1))*|...|),
%       both within +/-2%; u_fcst(h=0) == u_obs; h -> inf converges to
%       U_reach.
%   P4 (spec B.4) DEGRADED error structure is time-correlated, not iid:
%       a fixed formal-seed stream drives a long AR(1) eta path whose
%       lag-1 autocorrelation matches exp(-1/tau_eta) (+/-0.05) and whose
%       variance matches sigma_eta^2 (+/-40%); the voyage bias b_voyage
%       is constant across two issues of the same voyage/segment
%       direction when the caller passes the cached 'BVoyage'; an
%       internal draw returns b_voyage/b_wl for caller caching, the
%       passed-back values are honored, and a different base_draw_index
%       yields a different draw.
%   P5 (spec B.5) observability whitelist: every frozen FORBIDDEN class
%       (and an unknown class) raises GATE:MISSION_V3:FCST:SOURCE_CLASS;
%       a whitelist class is accepted; 'NoiseStream' together with
%       'FormalSeed'/'BaseDrawIndex' is an ARG error (fixed decision 7);
%       DEGRADED_V1 without any noise spec is an ARG error.
%   P6 (spec B.6) provenance completeness: mandated fields present
%       (generator_id, version, hash, issue_time_s, source_cutoff_s,
%       forecast_quality_profile_id, source_class,
%       provenance_class_formal, forecast_prehistory_s,
%       u_reach_mean_rule; wl_note for DEGRADED), hash equals the
%       'self_hash' mode value (64 lowercase hex), cutoff equals t0 for
%       GOOD and t0 - frozen lag for DEGRADED, generator/version/class/
%       prehistory match the frozen profiles.
%   P7 (spec B.7) determinism: two runs with identical inputs and
%       same-seed fresh noise streams (or identical formal seed specs)
%       are bitwise identical (isequal on the full result struct).
%   P8 (spec B.8) water-level channel: GOOD wl_fcst is exactly the
%       passed DeltaWL voyage constant; DEGRADED wl_fcst is exactly
%       DeltaWL + b_wl with the returned b_wl; different draws give
%       different b_wl.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_forecast_issue.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing; no
%   global RNG; the test writes nothing to results/.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;

% frozen registry reads (values consumed, never hard-coded)
profj = local_read_json(fullfile(ROOT, 'config', 'mission_v3', ...
    'forecast_quality_profiles.json'));
envj = local_read_json(fullfile(ROOT, 'config', 'mission_v3', ...
    'environment_regime_params.json'));

prof_profiles = profj.profiles;
prof_good = prof_profiles.GOOD_V1;
prof_deg = prof_profiles.DEGRADED_V1;
eta_pr = prof_deg.eta_ar1;
TAU_ETA = eta_pr.tau_s;
SIGMA_ETA = eta_pr.sigma_mps;
CUT_LAG_D = prof_deg.source_cutoff_lag_s;
DELTA_LAG = prof_deg.delta_lag_s;
good_uf = prof_good.u_forecast;
GOOD_CLASS = good_uf.u_obs_source_class;
env_reg = envj.regimes;
env_norm = env_reg.NORMAL;
du_norm = env_norm.delta_u;
TAU_U = du_norm.tau_s;
inj = envj.ou_process.injection;
GRID_DT = inj.grid_s;

STRAT_R = 'STRAT_ROUTINE_PATROL__NORMAL';      % caller label (quota layer)

%% P1 (B.1): analytic conditional mean vs numerical integration
ok_p1 = false;
try
    dag1 = mission_v3.dag_sample('draw', 't012_fcst_seed_01', 3, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R);
    out1 = mission_v3.env_realize('realize', dag1, 'auto');
    contract_ok = isfield(out1, 'u_river_mps') && isfield(out1, 't_grid_s');
    tg_all = out1.t_grid_s;
    ur_all = out1.u_river_mps;
    t0_1 = min(600, tg_all(end));
    mask1 = tg_all <= t0_1;
    tg1 = tg_all(mask1);
    along1 = -ur_all(mask1);        % upstream (outbound) along-track slice
    h1 = (0:300) * GRID_DT;
    r1 = mission_v3.forecast_issue('issue', along1, tg1, t0_1, h1, 'GOOD_V1');
    obs_ok = (tg1(end) == t0_1) && (r1.u_obs_mps == along1(end));
    ureach1 = r1.u_reach_mean_mps;
    amp1 = r1.u_obs_mps - ureach1;
    x_int = zeros(1, numel(h1));
    x_int(1) = r1.u_obs_mps;
    for kk = 2:numel(h1)
        x_int(kk) = ureach1 + (x_int(kk - 1) - ureach1) ...
            * exp(-GRID_DT / r1.tau_u_s);
    end
    denom1 = max(abs(amp1), 1e-3);
    rel1 = abs(r1.u_fcst_mps - x_int) ./ denom1;
    ok_p1 = contract_ok && obs_ok && (r1.tau_u_s == TAU_U) ...
        && all(rel1 <= 1e-12);
    fprintf(['[T] P1 analytic-vs-integral: contract=%d obs=%d ', ...
        'tau=%.0f maxrel=%.3e: %s\n'], contract_ok, obs_ok, ...
        r1.tau_u_s, max(rel1), local_verdict(ok_p1));
catch p1_err
    fprintf('[T] P1 exception: %s\n', p1_err.message);
end
n_fail = n_fail + ~ok_p1;

%% P2 (B.2): machine-enforced causality gates (real injections)
ok_p2 = false;
try
    % future-truth injection: slice carries 11 points beyond t0
    tg_f = -1800:1:610;
    u_f = 0.45 + 0.12 .* sin(tg_f ./ 233) + 0.05 .* cos(tg_f ./ 97);
    [ok_f, id_f] = local_expect_error(@() ...
        mission_v3.forecast_issue('issue', u_f, tg_f, 600, 0, 'GOOD_V1'));
    ok_f = ok_f && strcmp(id_f, 'GATE:MISSION_V3:FCST:FUTURE_INPUT');

    % DEGRADED lag whose causal source history does not exist in the slice
    tg_p = -100:1:600;
    u_p = 0.45 + 0.12 .* sin(tg_p ./ 233) + 0.05 .* cos(tg_p ./ 97);
    [ok_p, id_p] = local_expect_error(@() ...
        mission_v3.forecast_issue('issue', u_p, tg_p, 600, 0, 'DEGRADED_V1'));
    ok_p = ok_p && strcmp(id_p, 'GATE:MISSION_V3:FCST:NO_PREHISTORY');

    % boundary inclusion: slice starting exactly at t0 - 1800 is legal
    tg_b = -1200:1:600;
    u_b = 0.45 + 0.12 .* sin(tg_b ./ 233) + 0.05 .* cos(tg_b ./ 97);
    r_b = mission_v3.forecast_issue('issue', u_b, tg_b, 600, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_prehist_seed_01', ...
        'BaseDrawIndex', 0);
    rbs = r_b.obs;
    ok_b = (rbs.u_source_time_s == -1200) && (r_b.t0_s == 600) ...
        && (r_b.obs.delta_lag_s == DELTA_LAG);
    ok_p2 = ok_f && ok_p && ok_b;
    fprintf('[T] P2 causal gates: future=%d no_prehistory=%d boundary=%d: %s\n', ...
        ok_f, ok_p, ok_b, local_verdict(ok_p2));
catch p2_err
    fprintf('[T] P2 exception: %s\n', p2_err.message);
end
n_fail = n_fail + ~ok_p2;

%% P3 (B.3): GOOD decay shape
ok_p3 = false;
try
    tg3 = -1800:1:600;
    u3 = 0.45 + 0.12 .* sin(tg3 ./ 233) + 0.05 .* cos(tg3 ./ 97);
    h3 = [0, TAU_U, 25 * TAU_U];
    r3 = mission_v3.forecast_issue('issue', u3, tg3, 600, h3, 'GOOD_V1');
    ureach3 = r3.u_reach_mean_mps;
    A3 = r3.u_obs_mps - ureach3;
    rem_actual = abs(r3.u_fcst_mps(2) - ureach3);       % h = tau_u
    rem_expect = exp(-1) * abs(A3);
    decay_actual = abs(r3.u_obs_mps - r3.u_fcst_mps(2));
    decay_expect = (1 - exp(-1)) * abs(A3);
    shape_ok = abs(rem_actual - rem_expect) <= 0.02 * rem_expect ...
        && abs(decay_actual - decay_expect) <= 0.02 * decay_expect ...
        && (r3.u_fcst_mps(1) == r3.u_obs_mps) ...
        && (abs(r3.u_fcst_mps(3) - ureach3) <= 1e-9 * abs(A3) + 1e-15);
    ok_p3 = shape_ok && (r3.tau_u_s == TAU_U);
    fprintf(['[T] P3 GOOD shape: rem=%.6f (exp(-1)*|A|=%.6f) ', ...
        'decay=%.6f ((1-exp(-1))*|A|=%.6f) h0=%d tail=%d: %s\n'], ...
        rem_actual, rem_expect, decay_actual, decay_expect, ...
        r3.u_fcst_mps(1) == r3.u_obs_mps, ...
        abs(r3.u_fcst_mps(3) - ureach3) <= 1e-9 * abs(A3) + 1e-15, ...
        local_verdict(ok_p3));
catch p3_err
    fprintf('[T] P3 exception: %s\n', p3_err.message);
end
n_fail = n_fail + ~ok_p3;

%% P4 (B.4): DEGRADED error time correlation + voyage bias caching
ok_p4 = false;
try
    % (a) long AR(1) eta path from one fixed formal-seed stream
    t0_4 = 398200;
    tg4 = -1800:1:t0_4;
    u4 = 0.45 .* ones(size(tg4));
    r4 = mission_v3.forecast_issue('issue', u4, tg4, t0_4, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_eta_seed_01', 'BaseDrawIndex', 7);
    r4s = r4.obs;
    eta4 = r4s.eta_path;
    n4 = numel(eta4);
    ec4 = eta4 - mean(eta4);
    ss4 = sum(ec4 .^ 2);
    acf1_4 = sum(ec4(1:end-1) .* ec4(2:end)) / ss4;
    tgt4 = exp(-1 / TAU_ETA);
    var4 = var(eta4);
    ok_a = (n4 >= 400000) ...
        && (abs(acf1_4 - tgt4) <= 0.05) ...
        && (var4 >= 0.6 * SIGMA_ETA^2) && (var4 <= 1.4 * SIGMA_ETA^2);

    % (b) b_voyage constant across two issues of the same voyage/direction
    bv_given = 0.03;
    tg_6 = -1800:1:600;
    u_6 = 0.45 + 0.12 .* sin(tg_6 ./ 233) + 0.05 .* cos(tg_6 ./ 97);
    tg_9 = -1800:1:900;
    u_9 = 0.45 + 0.12 .* sin(tg_9 ./ 233) + 0.05 .* cos(tg_9 ./ 97);
    rb1 = mission_v3.forecast_issue('issue', u_6, tg_6, 600, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_bv_seed_01', 'BaseDrawIndex', 2, ...
        'BVoyage', bv_given);
    rb2 = mission_v3.forecast_issue('issue', u_9, tg_9, 900, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_bv_seed_01', 'BaseDrawIndex', 2, ...
        'BVoyage', bv_given);
    rb1s = rb1.obs;
    rb2s = rb2.obs;
    ok_b = (rb1s.b_voyage == bv_given) && (rb2s.b_voyage == bv_given) ...
        && rb1s.b_voyage_caller_supplied && rb2s.b_voyage_caller_supplied;

    % (c) internal draw returned for caller caching; cache honored;
    %     different base_draw_index yields a different draw
    rc1 = mission_v3.forecast_issue('issue', u_6, tg_6, 600, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_bseed_01', 'BaseDrawIndex', 3);
    rc1s = rc1.obs;
    rc2 = mission_v3.forecast_issue('issue', u_6, tg_6, 600, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_bseed_01', 'BaseDrawIndex', 3, ...
        'BVoyage', rc1s.b_voyage, 'BWl', rc1s.b_wl);
    rc2s = rc2.obs;
    rc3 = mission_v3.forecast_issue('issue', u_6, tg_6, 600, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_bseed_01', 'BaseDrawIndex', 4);
    rc3s = rc3.obs;
    ok_c = (~rc1s.b_voyage_caller_supplied) && (~rc1s.b_wl_caller_supplied) ...
        && (rc2s.b_voyage == rc1s.b_voyage) && (rc2s.b_wl == rc1s.b_wl) ...
        && (rc3s.b_voyage ~= rc1s.b_voyage);
    ok_p4 = ok_a && ok_b && ok_c;
    fprintf(['[T] P4 degraded noise: n=%d acf1=%.4f (tgt %.4f) ', ...
        'var_ok=%d bconst=%d cache=%d: %s\n'], n4, acf1_4, tgt4, ...
        ok_a, ok_b, ok_c, local_verdict(ok_p4));
catch p4_err
    fprintf('[T] P4 exception: %s\n', p4_err.message);
end
n_fail = n_fail + ~ok_p4;

%% P5 (B.5): observability whitelist + noise spec configuration
ok_p5 = false;
try
    tg5 = -1800:1:600;
    u5 = 0.45 + 0.12 .* sin(tg5 ./ 233) + 0.05 .* cos(tg5 ./ 97);
    bad_classes = profj.observability_forbidden;
    ok_bad = true;
    for k5 = 1:numel(bad_classes)
        cls = bad_classes{k5};
        [ok_e, id_e] = local_expect_error(@() ...
            mission_v3.forecast_issue('issue', u5, tg5, 600, 0, ...
            'GOOD_V1', 'SourceClass', cls));
        ok_bad = ok_bad && ok_e ...
            && strcmp(id_e, 'GATE:MISSION_V3:FCST:SOURCE_CLASS');
    end
    [ok_u, id_u] = local_expect_error(@() ...
        mission_v3.forecast_issue('issue', u5, tg5, 600, 0, 'GOOD_V1', ...
        'SourceClass', 'NOT_A_REAL_CLASS'));
    ok_u = ok_u && strcmp(id_u, 'GATE:MISSION_V3:FCST:SOURCE_CLASS');
    r_okc = mission_v3.forecast_issue('issue', u5, tg5, 600, 0, ...
        'GOOD_V1', 'SourceClass', 'CAUSALLY_ESTIMATED');
    r_okp = r_okc.provenance;
    ok_wl = strcmp(r_okp.source_class, 'CAUSALLY_ESTIMATED');
    rs5 = RandStream('Threefry4x64', 'Seed', 20260905);
    [ok_m, id_m] = local_expect_error(@() ...
        mission_v3.forecast_issue('issue', u5, tg5, 600, 0, ...
        'DEGRADED_V1', 'NoiseStream', rs5, 'FormalSeed', 't012_seed', ...
        'BaseDrawIndex', 0));
    ok_m = ok_m && strcmp(id_m, 'GATE:MISSION_V3:FCST:ARG');
    [ok_n, id_n] = local_expect_error(@() ...
        mission_v3.forecast_issue('issue', u5, tg5, 600, 0, 'DEGRADED_V1'));
    ok_n = ok_n && strcmp(id_n, 'GATE:MISSION_V3:FCST:ARG');
    ok_p5 = ok_bad && ok_u && ok_wl && ok_m && ok_n;
    fprintf(['[T] P5 whitelist/config: forbidden=%d unknown=%d ', ...
        'whitelist_accept=%d mutex=%d no_noise=%d: %s\n'], ...
        ok_bad, ok_u, ok_wl, ok_m, ok_n, local_verdict(ok_p5));
catch p5_err
    fprintf('[T] P5 exception: %s\n', p5_err.message);
end
n_fail = n_fail + ~ok_p5;

%% P6 (B.6): provenance completeness
ok_p6 = false;
try
    tg6 = -1800:1:600;
    u6 = 0.45 + 0.12 .* sin(tg6 ./ 233) + 0.05 .* cos(tg6 ./ 97);
    r6g = mission_v3.forecast_issue('issue', u6, tg6, 600, 0, 'GOOD_V1');
    r6d = mission_v3.forecast_issue('issue', u6, tg6, 600, 0, ...
        'DEGRADED_V1', 'FormalSeed', 't012_prov_seed_01', 'BaseDrawIndex', 5);
    sh6 = mission_v3.forecast_issue('self_hash');
    pv_g = r6g.provenance;
    pv_d = r6d.provenance;
    fn_g = fieldnames(pv_g);
    fn_d = fieldnames(pv_d);
    need6 = {'generator_id', 'version', 'hash', 'issue_time_s', ...
        'source_cutoff_s', 'forecast_quality_profile_id', 'source_class', ...
        'provenance_class_formal', 'forecast_prehistory_s', ...
        'u_reach_mean_rule'};
    prov_pr = profj.provenance;
    hash_ok = strcmp(pv_g.hash, sh6.value) && strcmp(pv_d.hash, sh6.value) ...
        && ~isempty(regexp(pv_g.hash, '^[0-9a-f]{64}$', 'once'));
    cut_ok = (pv_g.source_cutoff_s == 600) ...
        && (pv_d.source_cutoff_s == 600 - CUT_LAG_D) ...
        && (pv_g.issue_time_s == 600) && (pv_d.issue_time_s == 600) ...
        && (pv_d.source_cutoff_s <= pv_d.issue_time_s) ...
        && (r6g.obs.u_source_time_s == 600) ...
        && (r6d.obs.u_source_time_s == 600 - DELTA_LAG);
    id_ok = strcmp(pv_g.generator_id, prov_pr.generator_id) ...
        && strcmp(pv_d.generator_id, prov_pr.generator_id) ...
        && strcmp(pv_g.version, profj.version) ...
        && strcmp(pv_g.forecast_quality_profile_id, 'GOOD_V1') ...
        && strcmp(pv_d.forecast_quality_profile_id, 'DEGRADED_V1') ...
        && strcmp(pv_g.provenance_class_formal, 'CAUSAL_GENERATOR') ...
        && (pv_g.forecast_prehistory_s == profj.prehistory_s) ...
        && strcmp(pv_g.source_class, GOOD_CLASS) ...
        && strcmp(pv_d.source_class, 'CAUSALLY_ESTIMATED');
    ok_p6 = all(ismember(need6, fn_g)) && all(ismember(need6, fn_d)) ...
        && ismember('wl_note', fn_d) && hash_ok && cut_ok && id_ok;
    fprintf(['[T] P6 provenance: fields=%d wl_note=%d hash=%d ', ...
        'cutoff=%d ids=%d: %s\n'], ...
        all(ismember(need6, fn_g)) && all(ismember(need6, fn_d)), ...
        ismember('wl_note', fn_d), hash_ok, cut_ok, id_ok, ...
        local_verdict(ok_p6));
catch p6_err
    fprintf('[T] P6 exception: %s\n', p6_err.message);
end
n_fail = n_fail + ~ok_p6;

%% P7 (B.7): determinism
ok_p7 = false;
try
    tg7 = -1800:1:600;
    u7 = 0.45 + 0.12 .* sin(tg7 ./ 233) + 0.05 .* cos(tg7 ./ 97);
    h7 = [0, 30, 60];
    rs_a = RandStream('Threefry4x64', 'Seed', 424242);
    rs_b = RandStream('Threefry4x64', 'Seed', 424242);
    r7a = mission_v3.forecast_issue('issue', u7, tg7, 600, h7, ...
        'DEGRADED_V1', 'NoiseStream', rs_a);
    r7b = mission_v3.forecast_issue('issue', u7, tg7, 600, h7, ...
        'DEGRADED_V1', 'NoiseStream', rs_b);
    det_stream = isequal(r7a, r7b);
    r7c = mission_v3.forecast_issue('issue', u7, tg7, 600, h7, ...
        'DEGRADED_V1', 'FormalSeed', 't012_det_seed_01', 'BaseDrawIndex', 9);
    r7d = mission_v3.forecast_issue('issue', u7, tg7, 600, h7, ...
        'DEGRADED_V1', 'FormalSeed', 't012_det_seed_01', 'BaseDrawIndex', 9);
    det_seed = isequal(r7c, r7d);
    ok_p7 = det_stream && det_seed;
    fprintf('[T] P7 determinism: caller_stream=%d formal_seed=%d: %s\n', ...
        det_stream, det_seed, local_verdict(ok_p7));
catch p7_err
    fprintf('[T] P7 exception: %s\n', p7_err.message);
end
n_fail = n_fail + ~ok_p7;

%% P8 (B.8): water-level channel
ok_p8 = false;
try
    tg8 = -1800:1:600;
    u8 = 0.45 + 0.12 .* sin(tg8 ./ 233) + 0.05 .* cos(tg8 ./ 97);
    dwl8 = 0.37;
    r8g = mission_v3.forecast_issue('issue', u8, tg8, 600, [0, 100], ...
        'GOOD_V1', 'DeltaWL', dwl8);
    r8d = mission_v3.forecast_issue('issue', u8, tg8, 600, [0, 100], ...
        'DEGRADED_V1', 'FormalSeed', 't012_wl_seed_01', 'BaseDrawIndex', 1, ...
        'DeltaWL', dwl8);
    r8e = mission_v3.forecast_issue('issue', u8, tg8, 600, [0, 100], ...
        'DEGRADED_V1', 'FormalSeed', 't012_wl_seed_01', 'BaseDrawIndex', 2, ...
        'DeltaWL', dwl8);
    r8ds = r8d.obs;
    r8es = r8e.obs;
    wl_ok = (r8g.wl_fcst_m == dwl8) && isscalar(r8g.wl_fcst_m) ...
        && (r8d.wl_fcst_m == dwl8 + r8ds.b_wl) ...
        && (r8es.b_wl ~= r8ds.b_wl);
    ok_p8 = wl_ok;
    fprintf(['[T] P8 water level: good_const=%d degraded=%d ', ...
        'b_wl(d)=%.4f b_wl(e)=%.4f: %s\n'], ...
        r8g.wl_fcst_m == dwl8, r8d.wl_fcst_m == dwl8 + r8ds.b_wl, ...
        r8ds.b_wl, r8es.b_wl, local_verdict(ok_p8));
catch p8_err
    fprintf('[T] P8 exception: %s\n', p8_err.message);
end
n_fail = n_fail + ~ok_p8;

%% summary
fprintf('[T] test_forecast_issue: %d/8 cases passed\n', 8 - n_fail);
if n_fail > 0
    error('GATE:test_forecast_issue %d case(s) failed', n_fail);
end
fprintf('test_forecast_issue: ALL PASS (8/8)\n');

%% ======================== local functions ==========================

function obj = local_read_json(ppath)
% Frozen registries are validator-clean (strict parser + self_hash
% cross-check in mission_v3.validate_registries); jsondecode consumption
% follows the seed_tree.m precedent.
    obj = jsondecode(fileread(ppath));
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
