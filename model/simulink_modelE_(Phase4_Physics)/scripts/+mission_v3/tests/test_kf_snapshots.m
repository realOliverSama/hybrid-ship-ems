% test_kf_snapshots.m - T-013: K/F issue-major snapshot builder (DGN-08,
% FR-558~562, tasks Gate R7)
%   P1 (spec B.1) layout: 'build' returns the export_pilot-aligned
%       issue-major struct (issue_time_s scalar/Mx1, lead_s == 10:10:300
%       per row, six content columns 1x30/Mx30, missing_mask logical,
%       contract_status cellstr, provenance); 'build_grid' assembles the
%       (M x 30) layout by per-issue independent 'build' calls (fixed
%       decision 7) - every grid row equals the standalone build output;
%       the hotel column equals the frozen registry p_hotel_base for the
%       plan template and the provenance carries the registry self_hash.
%   P2 (spec B.2) R7 causal invariant (tasks Gate R7): one T-009
%       realization R1 and a tampered copy R2 whose realized future
%       (t > t0) u_river series differs; the composed plans are bitwise
%       identical (plan facts are pre-declared) and the causal slices
%       truncate to identical Fblk; the snapshot function sees only
%       (plan, t0, Fblk, name-values), so all runs are bitwise identical.
%       Extra positional arguments (a realization struct or a realized
%       future array) raise GATE:MISSION_V3:KF:BAD_ARG - the signature
%       admits no realized-future channel.
%       NOTE (R7): a high statistical correlation between the plan and
%       the realized execution is the EXPECTED signature of faithful plan
%       execution; it is never by itself evidence of information leakage.
%       Causality is guaranteed structurally by the interface boundary.
%   P3 (spec B.3) s_plan integration correctness: a piecewise-constant
%       plan is integrated by hand (independent double loop, 1 s substeps,
%       right-endpoint values) and compared point-wise to s_plan/v_plan
%       (tolerance 1e-9).
%   P4 (spec B.4) missing snapshots: an issue time near the plan end
%       makes trailing leads exceed plan.t_grid_s(end); those entries are
%       whole-entry NaN in every content column, missing_mask true, row
%       contract_status 'FAILED_CONTRACT'; no extrapolation, no zero
%       fill.  An issue time beyond the plan end makes the whole row
%       missing.
%   P5 (spec B.5) zone sampling: a plan crossing speed-limit/bridge zone
%       boundaries is sampled at the bin right-endpoint time with
%       half-open windows; exact boundary memberships are asserted
%       (t = t_start inside, t = t_end outside).
%   P6 (spec B.6) gate detection: F content/grid mismatch ->
%       GATE:MISSION_V3:KF:FGRID; tampered lead grid (9:10:300 class or
%       wrong point count) -> GATE:MISSION_V3:KF:GRID; Fblk vintage
%       mismatch -> GATE:MISSION_V3:KF:VINTAGE; missing HChartM, unknown
%       option, wrong plan kind, off-grid issue time, struct-array Fblk in
%       'build', Fblk/issue count mismatch, negative issue time ->
%       GATE:MISSION_V3:KF:BAD_ARG.
%   P7 (spec B.7) determinism: two runs with identical inputs (including
%       non-empty zones) are bitwise identical (isequal on the full
%       result struct), build and build_grid alike.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_kf_snapshots.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing; no
%   global RNG; the test writes nothing to results/.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;

% frozen registry reads (values consumed, never hard-coded)
tplj = local_read_json(fullfile(ROOT, 'config', 'mission_v3', ...
    'mission_template_registry.json'));
tpl_lst = local_as_list(tplj.mission_templates);
hotel_routine = NaN;
for k = 1:numel(tpl_lst)
    t = tpl_lst{k};
    if strcmp(t.mission_template_id, 'ROUTINE_PATROL')
        hotel_routine = t.p_hotel_base_kW;
    end
end
reg_sh = tplj.self_hash;

STRAT_R = 'STRAT_ROUTINE_PATROL__NORMAL';      % caller label (quota layer)
LEADS = 10:10:300;

%% P1 (B.1): layout, export_pilot alignment, per-issue grid semantics
ok_p1 = false;
try
    vtab = 2.0 * ones(1, 1201);                % t_end = 1200, v = 2 m/s
    plan = local_make_plan(vtab, local_empty_zones(), local_empty_zones());
    t0 = 300;
    hc = 5.2;
    fb1 = local_make_fblk(t0);

    kf = mission_v3.kf_snapshots('build', plan, t0, fb1, 'HChartM', hc);

    want_fields = {'issue_time_s', 'lead_s', 'v_plan_mps', 's_plan_m', ...
        'h_chart_m', 'P_hotel_plan_kW', 'u_fcst_mps', 'wl_fcst_m', ...
        'in_speed_limit_zone', 'in_bridge_zone', 'missing_mask', ...
        'contract_status', 'provenance'};
    ok = isstruct(kf);
    for j = 1:numel(want_fields)
        ok = ok && isfield(kf, want_fields{j});
    end
    its = kf.issue_time_s;
    ok = ok && isnumeric(its) && isscalar(its) && (its == t0);
    lds = kf.lead_s;
    ok = ok && isequal(size(lds), [1, 30]) && isequal(lds, LEADS);
    six = {'v_plan_mps', 's_plan_m', 'h_chart_m', 'P_hotel_plan_kW', ...
        'u_fcst_mps', 'wl_fcst_m'};
    for j = 1:numel(six)
        col = getfield(kf, six{j});
        ok = ok && isequal(size(col), [1, 30]);
    end
    mm = kf.missing_mask;
    ok = ok && islogical(mm) && isequal(size(mm), [1, 30]) && ~any(mm(:));
    cs = kf.contract_status;
    ok = ok && iscell(cs) && isequal(size(cs), [1, 1]) ...
        && strcmp(cs{1}, 'OK');
    hv = kf.P_hotel_plan_kW;
    ok = ok && all(hv == hotel_routine);       % frozen registry value
    hcol = kf.h_chart_m;
    ok = ok && all(hcol == hc);
    fu = fb1.u_fcst_mps;
    fcol = kf.u_fcst_mps;
    ok = ok && isequal(fcol, fu(:).');         % F pass-through, no recompute
    fw = fb1.wl_fcst_m;
    wcol = kf.wl_fcst_m;
    ok = ok && all(wcol == fw);
    vpc = kf.v_plan_mps;
    ok = ok && all(vpc == 2.0);
    spc = kf.s_plan_m;
    ok = ok && isequal(spc, 20.0 .* (1.0:30.0));   % x0 = 0, v = 2
    zflg = kf.in_speed_limit_zone;
    bflg = kf.in_bridge_zone;
    ok = ok && ~any(zflg(:)) && ~any(bflg(:));
    pv = kf.provenance;
    ok = ok && isstruct(pv) && strcmp(pv.initial_x_plan_source, ...
        'default_zero') && strcmp(pv.hash_template_registry, reg_sh.value);

    % build_grid: (M x 1)/(M x 30) layout, rows == standalone builds
    t0v = (60:60:180)';
    fb_all = [local_make_fblk(60); local_make_fblk(120); ...
        local_make_fblk(180)];
    kfg = mission_v3.kf_snapshots('build_grid', plan, t0v, fb_all, ...
        'HChartM', hc);
    its_g = kfg.issue_time_s;
    ok = ok && isequal(size(its_g), [3, 1]) && isequal(its_g, t0v);
    lds_g = kfg.lead_s;
    ok = ok && isequal(size(lds_g), [3, 30]) ...
        && isequal(lds_g, repmat(LEADS, 3, 1));
    for j = 1:numel(six)
        col = getfield(kfg, six{j});
        ok = ok && isequal(size(col), [3, 30]);
    end
    mm_g = kfg.missing_mask;
    ok = ok && islogical(mm_g) && isequal(size(mm_g), [3, 30]) ...
        && ~any(mm_g(:));
    cs_g = kfg.contract_status;
    ok = ok && iscell(cs_g) && isequal(size(cs_g), [3, 1]);
    cmp_fields = [six, {'in_speed_limit_zone', 'in_bridge_zone', ...
        'missing_mask'}];
    for m = 1:3
        ok = ok && strcmp(cs_g{m}, 'OK');
        fbm = fb_all(m);
        kfb = mission_v3.kf_snapshots('build', plan, t0v(m), fbm, ...
            'HChartM', hc);
        for j = 1:numel(cmp_fields)
            fnm = cmp_fields{j};
            col_g = getfield(kfg, fnm);
            col_b = getfield(kfb, fnm);
            ok = ok && isequal(col_g(m, :), col_b);
        end
    end
    ok_p1 = ok;
    fprintf(['[T] P1 layout: build=1x30 grid=3x30 rows==build ', ...
        'hotel_reg=%d prov=%d: %s\n'], all(hv == hotel_routine), ...
        strcmp(pv.hash_template_registry, reg_sh.value), ...
        local_verdict(ok_p1));
catch p1_err
    fprintf('[T] P1 exception: %s\n', p1_err.message);
end
n_fail = n_fail + ~ok_p1;

%% P2 (B.2): R7 causal invariant (plan/t0/Fblk fixed, future tampered)
ok_p2 = false;
try
    r = mission_v3.env_realize('sample', 't013_r7_seed_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 60, ...
        'ExhaustAction', 'return_ledger');
    ok_scan = r.accepted;
    rl1 = r.realization;
    ou1 = rl1.ou;
    t0 = min(600, floor(ou1.t_end_s) - 400);
    ok_scan = ok_scan && (t0 >= 300);

    P1p = mission_v3.plan_compose('compose', rl1);

    % tamper ONLY the realized future (t > t0) of the u_river series
    rl2 = rl1;
    tgr = rl2.t_grid_s;
    ur = rl2.u_river_mps;
    fut = tgr > t0;
    ur(fut) = ur(fut) + 0.5;
    rl2.u_river_mps = ur;
    ur1 = rl1.u_river_mps;
    tampered = ~isequal(ur1, rl2.u_river_mps) && any(fut);

    P2p = mission_v3.plan_compose('compose', rl2);
    plan_inv = isequal(P1p, P2p);   % plan facts are pre-declared

    % causal Fblk from the t <= t0 truncated slice of each realization
    tg1 = rl1.t_grid_s;
    m1 = tg1 <= t0;
    fb_r1 = mission_v3.forecast_issue('issue', -ur1(m1), tg1(m1), t0, ...
        LEADS, 'GOOD_V1');          % upstream outbound along-track sign
    tg2 = rl2.t_grid_s;
    ur2 = rl2.u_river_mps;
    m2 = tg2 <= t0;
    fb_r2 = mission_v3.forecast_issue('issue', -ur2(m2), tg2(m2), t0, ...
        LEADS, 'GOOD_V1');
    slice_inv = isequal(fb_r1, fb_r2);   % futures differ, slices do not

    hc = 6.0;
    kf_a = mission_v3.kf_snapshots('build', P1p, t0, fb_r1, ...
        'HChartM', hc, 'InitialXPlanM', 0);
    kf_b = mission_v3.kf_snapshots('build', P2p, t0, fb_r2, ...
        'HChartM', hc, 'InitialXPlanM', 0);
    kf_c = mission_v3.kf_snapshots('build', P1p, t0, fb_r1, ...
        'HChartM', hc, 'InitialXPlanM', 0);
    inv_ab = isequal(kf_a, kf_b);
    inv_ac = isequal(kf_a, kf_c);

    % API boundary: extra positional arguments must be rejected
    [ok_x1, id_x1] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', P1p, t0, fb_r1, rl2), ...
        'GATE:MISSION_V3:KF:BAD_ARG');
    [ok_x2, id_x2] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', P1p, t0, fb_r1, rl2.u_river_mps), ...
        'GATE:MISSION_V3:KF:BAD_ARG');
    [ok_x3, id_x3] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build_grid', P1p, [t0; t0 + 60], ...
        [fb_r1; fb_r1], rl2.u_river_mps), 'GATE:MISSION_V3:KF:BAD_ARG');

    ok_p2 = ok_scan && tampered && plan_inv && slice_inv ...
        && inv_ab && inv_ac && ok_x1 && ok_x2 && ok_x3;
    fprintf(['[T] P2 R7 causal: accepted=%d tampered=%d plan_inv=%d ', ...
        'slice_inv=%d bit_inv=%d/%d bad_arg=%d/%d/%d: %s\n'], ...
        ok_scan, tampered, plan_inv, slice_inv, inv_ab, inv_ac, ...
        ok_x1, ok_x2, ok_x3, local_verdict(ok_p2));
catch p2_err
    fprintf('[T] P2 exception: %s\n', p2_err.message);
end
n_fail = n_fail + ~ok_p2;

%% P3 (B.3): s_plan integration correctness (hand-computed)
ok_p3 = false;
try
    vtab = zeros(1, 401);                      % t_end = 400
    vtab(1:100) = 2.0;                         % t in [0, 100)
    vtab(101:250) = 3.0;                       % t in [100, 250)
    vtab(251:401) = 1.5;                       % t in [250, 400]
    plan = local_make_plan(vtab, local_empty_zones(), local_empty_zones());
    t0 = 50;
    x0 = 100.0;
    fb = local_make_fblk(t0);
    kf = mission_v3.kf_snapshots('build', plan, t0, fb, ...
        'HChartM', 4.0, 'InitialXPlanM', x0);

    % independent hand integration: 1 s substeps, right-endpoint values
    expected_s = zeros(1, 30);
    expected_v = zeros(1, 30);
    for k = 1:30
        acc = x0;
        for j = 51:(50 + 10 * k)
            acc = acc + vtab(j + 1) * 1.0;
        end
        expected_s(k) = acc;
        expected_v(k) = vtab(50 + 10 * k + 1);
    end
    sp = kf.s_plan_m;
    vp = kf.v_plan_mps;
    ds = max(abs(sp - expected_s));
    dv = max(abs(vp - expected_v));
    ok = (ds <= 1e-9) && (dv <= 1e-9) ...
        && (sp(1) == 120.0) ...                % 100 + 10 * 2.0 exactly
        && all(diff(sp) >= 0);                 % non-negative plan speed
    ok_p3 = ok;
    fprintf('[T] P3 integration: max|ds|=%.3e max|dv|=%.3e s1=%g: %s\n', ...
        ds, dv, sp(1), local_verdict(ok_p3));
catch p3_err
    fprintf('[T] P3 exception: %s\n', p3_err.message);
end
n_fail = n_fail + ~ok_p3;

%% P4 (B.4): missing snapshots = FAILED_CONTRACT, no fill
ok_p4 = false;
try
    vtab = 2.0 * ones(1, 241);                 % t_end = 240
    plan = local_make_plan(vtab, local_empty_zones(), local_empty_zones());
    hc = 4.5;
    t0 = 200;                                  % leads 10..40 fit, 50..300 not
    fb = local_make_fblk(t0);
    kf = mission_v3.kf_snapshots('build', plan, t0, fb, 'HChartM', hc);

    want_mm = [false(1, 4), true(1, 26)];
    mm = kf.missing_mask;
    cs = kf.contract_status;
    six = {'v_plan_mps', 's_plan_m', 'h_chart_m', 'P_hotel_plan_kW', ...
        'u_fcst_mps', 'wl_fcst_m'};
    ok = isequal(mm, want_mm) && strcmp(cs{1}, 'FAILED_CONTRACT');
    for j = 1:numel(six)
        col = getfield(kf, six{j});
        ok = ok && all(isnan(col(want_mm)));   % whole-entry NaN, no fill
        ok = ok && all(~isnan(col(~want_mm)));
    end
    vp = kf.v_plan_mps;
    sp = kf.s_plan_m;
    ok = ok && isequal(vp(1:4), [2.0, 2.0, 2.0, 2.0]) ...
        && isequal(sp(1:4), [20.0, 40.0, 60.0, 80.0]);
    fu = fb.u_fcst_mps;
    fcol = kf.u_fcst_mps;
    ok = ok && isequal(fcol(1:4), fu(1:4));    % good leads keep real F
    fw = fb.wl_fcst_m;
    wcol = kf.wl_fcst_m;
    ok = ok && isequal(wcol(1:4), repmat(fw, 1, 4));
    sp_nan_tail = all(isnan(sp(want_mm)));

    % issue time beyond the plan end: the whole row is missing
    t0b = 500;
    fb_b = local_make_fblk(t0b);
    kf_b = mission_v3.kf_snapshots('build', plan, t0b, fb_b, ...
        'HChartM', hc);
    mm_b = kf_b.missing_mask;
    cs_b = kf_b.contract_status;
    ok = ok && all(mm_b(:)) && strcmp(cs_b{1}, 'FAILED_CONTRACT');
    for j = 1:numel(six)
        col = getfield(kf_b, six{j});
        ok = ok && all(isnan(col(:)));
    end
    zflg_b = kf_b.in_speed_limit_zone;
    ok = ok && ~any(zflg_b(:));
    ok_p4 = ok;
    fprintf(['[T] P4 missing: mask=%d/%d status=%s tail_nan=%d ', ...
        'all_missing=%d: %s\n'], sum(want_mm), numel(want_mm), cs{1}, ...
        sp_nan_tail, all(mm_b(:)), local_verdict(ok_p4));
catch p4_err
    fprintf('[T] P4 exception: %s\n', p4_err.message);
end
n_fail = n_fail + ~ok_p4;

%% P5 (B.5): zone sampling at bin right endpoints (half-open windows)
ok_p5 = false;
try
    vtab = ones(1, 401);                       % t_end = 400, v = 1 m/s
    slz = struct('segment_index', 2, 'role', 'cruise', 'reach', 'SHOAL', ...
        't_start_s', 120, 't_end_s', 240, 'v_limit_mps', 1.0, ...
        'reason', 'UKC_SHALLOW');
    ph0 = struct('name', '', 'frac', 0.0, 't_start_s', 0.0, ...
        't_end_s', 0.0, 'speed_law', '', 'v_start_mps', 0.0, ...
        'v_end_mps', 0.0);
    ph_empty = repmat(ph0, 1, 0);
    brg = struct('segment_index', 1, 'role', 'bridge', 'reach', 'BRIDGE', ...
        't_start_s', 60, 't_end_s', 120, 'v_host_mps', 1.0, ...
        'v_transit_mps', 0.7, 'phases', {ph_empty});
    plan = local_make_plan(vtab, slz, brg);
    t0 = 0;
    fb = local_make_fblk(t0);
    kf = mission_v3.kf_snapshots('build', plan, t0, fb, 'HChartM', 3.3);

    zflg = kf.in_speed_limit_zone;
    bflg = kf.in_bridge_zone;
    % speed limit zone [120, 240): leads 12..23 in, 11 and 24 out
    ok = ~zflg(11) && zflg(12) && zflg(23) && ~zflg(24);
    ok = ok && isequal(zflg(1:11), false(1, 11)) ...
        && isequal(zflg(12:23), true(1, 12)) ...
        && isequal(zflg(24:30), false(1, 7));
    % bridge zone [60, 120): leads 6..11 in, 5 and 12 out
    ok = ok && ~bflg(5) && bflg(6) && bflg(11) && ~bflg(12);
    ok = ok && isequal(bflg(1:5), false(1, 5)) ...
        && isequal(bflg(6:11), true(1, 6)) ...
        && isequal(bflg(12:30), false(1, 19));
    ok_p5 = ok;
    fprintf(['[T] P5 zones: slz@[11,12,23,24]=%d%d%d%d ', ...
        'brg@[5,6,11,12]=%d%d%d%d: %s\n'], zflg(11), zflg(12), zflg(23), ...
        zflg(24), bflg(5), bflg(6), bflg(11), bflg(12), ...
        local_verdict(ok_p5));
catch p5_err
    fprintf('[T] P5 exception: %s\n', p5_err.message);
end
n_fail = n_fail + ~ok_p5;

%% P6 (B.6): gate detection (FGRID / GRID / VINTAGE / BAD_ARG)
ok_p6 = false;
try
    vtab = 2.0 * ones(1, 1201);
    plan = local_make_plan(vtab, local_empty_zones(), local_empty_zones());
    t0 = 300;
    fb = local_make_fblk(t0);
    fu = fb.u_fcst_mps;

    % F content inconsistent with the declared lead grid -> FGRID
    fb_a = fb;
    fb_a.u_fcst_mps = fu(1:29);
    [ok_a, id_a] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, fb_a, 'HChartM', 5.0), ...
        'GATE:MISSION_V3:KF:FGRID');

    % tampered lead grids -> GRID
    fb_b = fb;
    fb_b.h_lead_s = 9:10:299;
    [ok_b, id_b] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, fb_b, 'HChartM', 5.0), ...
        'GATE:MISSION_V3:KF:GRID');
    fb_c = fb;
    fb_c.h_lead_s = 10:10:290;
    [ok_c, id_c] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, fb_c, 'HChartM', 5.0), ...
        'GATE:MISSION_V3:KF:GRID');

    % Fblk vintage mismatch -> VINTAGE
    fb_d = fb;
    fb_d.t0_s = t0 + 60;
    [ok_d, id_d] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, fb_d, 'HChartM', 5.0), ...
        'GATE:MISSION_V3:KF:VINTAGE');

    % argument contract violations -> BAD_ARG
    [ok_e, id_e] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, fb), ...
        'GATE:MISSION_V3:KF:BAD_ARG');                    % HChartM missing
    [ok_f, id_f] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, fb, ...
        'FutureRealization', struct()), 'GATE:MISSION_V3:KF:BAD_ARG');
    [ok_g, id_g] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, fb, 'Nope', 1.0), ...
        'GATE:MISSION_V3:KF:BAD_ARG');                    % unknown option
    bad_plan = plan;
    bad_plan.kind = 'not_a_plan';
    [ok_h, id_h] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', bad_plan, t0, fb, ...
        'HChartM', 5.0), 'GATE:MISSION_V3:KF:BAD_ARG');   % wrong plan kind
    [ok_i, id_i] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, 300.5, fb, ...
        'HChartM', 5.0), 'GATE:MISSION_V3:KF:BAD_ARG');   % off-grid t0
    [ok_j, id_j] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build', plan, t0, [fb; fb], ...
        'HChartM', 5.0), 'GATE:MISSION_V3:KF:BAD_ARG');   % Fblk not scalar
    [ok_k, id_k] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build_grid', plan, [60; 120], ...
        [fb; fb; fb], 'HChartM', 5.0), ...
        'GATE:MISSION_V3:KF:BAD_ARG');                    % count mismatch
    [ok_l, id_l] = local_expect_error_id(@() ...
        mission_v3.kf_snapshots('build_grid', plan, -60, fb, ...
        'HChartM', 5.0), 'GATE:MISSION_V3:KF:BAD_ARG');   % negative t0

    ok_p6 = ok_a && ok_b && ok_c && ok_d && ok_e && ok_f && ok_g ...
        && ok_h && ok_i && ok_j && ok_k && ok_l;
    fprintf(['[T] P6 gates: FGRID=%d GRID=%d/%d VINTAGE=%d ', ...
        'BAD_ARG=%d/%d/%d/%d/%d/%d/%d/%d: %s\n'], ok_a, ok_b, ok_c, ...
        ok_d, ok_e, ok_f, ok_g, ok_h, ok_i, ok_j, ok_k, ok_l, ...
        local_verdict(ok_p6));
catch p6_err
    fprintf('[T] P6 exception: %s\n', p6_err.message);
end
n_fail = n_fail + ~ok_p6;

%% P7 (B.7): determinism (two runs bitwise identical)
ok_p7 = false;
try
    vtab = 1.5 * ones(1, 901);                 % t_end = 900
    slz = struct('segment_index', 1, 'role', 'cruise', 'reach', 'SHOAL', ...
        't_start_s', 200, 't_end_s', 500, 'v_limit_mps', 1.0, ...
        'reason', 'UKC_SHALLOW');
    plan = local_make_plan(vtab, slz, local_empty_zones());
    t0 = 100;
    hc = 7.7;
    fb = local_make_fblk(t0);
    kf_1 = mission_v3.kf_snapshots('build', plan, t0, fb, ...
        'HChartM', hc, 'InitialXPlanM', 42.0);
    kf_2 = mission_v3.kf_snapshots('build', plan, t0, fb, ...
        'HChartM', hc, 'InitialXPlanM', 42.0);
    det_build = isequal(kf_1, kf_2);

    t0v = (60:60:180)';
    fb_all = [local_make_fblk(60); local_make_fblk(120); ...
        local_make_fblk(180)];
    kg_1 = mission_v3.kf_snapshots('build_grid', plan, t0v, fb_all, ...
        'HChartM', hc, 'InitialXPlanM', 42.0);
    kg_2 = mission_v3.kf_snapshots('build_grid', plan, t0v, fb_all, ...
        'HChartM', hc, 'InitialXPlanM', 42.0);
    det_grid = isequal(kg_1, kg_2);
    ok_p7 = det_build && det_grid;
    fprintf('[T] P7 determinism: build=%d grid=%d: %s\n', ...
        det_build, det_grid, local_verdict(ok_p7));
catch p7_err
    fprintf('[T] P7 exception: %s\n', p7_err.message);
end
n_fail = n_fail + ~ok_p7;

%% summary
fprintf('[T] test_kf_snapshots: %d/7 cases passed\n', 7 - n_fail);
if n_fail > 0
    error('GATE:test_kf_snapshots %d case(s) failed', n_fail);
end
fprintf('test_kf_snapshots: ALL PASS (7/7)\n');

%% ======================== local functions ==========================

function obj = local_read_json(ppath)
% Frozen registries are validator-clean (strict parser + self_hash
% cross-check in mission_v3.validate_registries); jsondecode consumption
% follows the seed_tree.m precedent.
    obj = jsondecode(fileread(ppath));
end

function lst = local_as_list(v)
% jsondecode list handling (dag_sample.m local_get_template precedent).
    if iscell(v)
        lst = v(:)';
    elseif isstruct(v)
        n = numel(v);
        lst = cell(1, n);
        for k = 1:n
            lst{k} = v(k);
        end
    else
        error('GATE:test_kf_snapshots', 'expected a JSON array');
    end
end

function p = local_make_plan(vtab, slz, brg)
% Minimal T-010-shaped plan consumed by kf_snapshots (kind, template_id,
% 1 s grid, non-negative speeds, zone tables).  Zone tables are injected
% so the tests can exercise membership exactly.
    p = struct();
    p.kind = 'mission_v3_plan';
    p.template_id = 'ROUTINE_PATROL';
    p.t_grid_s = 0:(numel(vtab) - 1);
    p.v_plan_mps = vtab;
    p.speed_limit_zones = slz;
    p.bridge_zones = brg;
end

function z = local_empty_zones()
    z = repmat(struct('t_start_s', 0.0, 't_end_s', 0.0), 1, 0);
end

function fb = local_make_fblk(t0)
% Real T-012 GOOD_V1 forecast at issue time t0 on the frozen lead grid,
% fed by a deterministic analytic causal slice (t <= t0 only).
    tg = -1800:1:(t0 + 600);
    u = 0.45 + 0.12 .* sin(tg ./ 233) + 0.05 .* cos(tg ./ 97);
    mask = tg <= t0;
    fb = mission_v3.forecast_issue('issue', u(mask), tg(mask), t0, ...
        10:10:300, 'GOOD_V1');
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
