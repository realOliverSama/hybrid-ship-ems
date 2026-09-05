% test_env_realize.m - T-009: dag_sample + env_realize harness (DGN-03/04, R1/R5)
%   P1 (spec C.1) OU stationarity / autocorrelation consistent with tau:
%       long path (>= 200k steps, fixed seed), empirical variance within
%       +/-40% of sigma^2 (registry-read) and lag-1 / lag-900 sample
%       autocorrelation within +/-0.05 of exp(-lag/tau).
%   P2 (spec C.2) prehistory >= 1800 s: grid starts at -1800, exactly 1800
%       negative-time points, t = 0 at index 1801, 1 s step from the
%       frozen injection grid, last point equals the realized t_end.
%   P3 (spec C.3) out-of-bounds candidates must reject with counting
%       (test params injected via the 'ParamOverride' name-value only;
%       frozen registry files untouched):
%         (a) OU sigma inflated -> whole-voyage flow sup above the
%             strictest axis bound -> SUPPORT_BOUND rows; the default
%             exhaust action raises GATE:MISSION_V3:ENV:MAX_RETRY
%         (b) HT <= 1.2 (domain floor) -> PHYSICAL_DOMAIN_BOUND rows
%         (c) HT in (1.2, 1.5) -> SUPPORT_BOUND rows
%   P4 (spec C.4) R1 round-trip sharing: one U_river realization per
%       voyage; outbound segments map -U_river (upstream), the return
%       segment +U_river (downstream), berth maps 0; per-segment summaries
%       recompute exactly from the single stored path; the OU fast-forward
%       count matches the dag draw; the return window lies later on the
%       same continuous path (zero redraw at heading reversal).
%   P5 (spec C.5) structural heldout tokens: HT in [1.5, 2.0] with a
%       harbor segment -> C2; EMERGENCY_RESPONSE (rush) -> C3; bridge
%       segment adverse component inside HIGH_ID_BAND -> C1; expected
%       token ids and the C1 band are read from the frozen heldout
%       registry (never hard-coded here).
%   P6 (spec C.6) determinism: two identical 'sample' runs give bitwise
%       identical realizations and ledgers (field-level comparison).
%   P7 (spec C.7) disposition structure completeness: six-class enum +
%       reason enum, evidence / lineage_summary fields, ACCEPT record with
%       attempt count and full-branch lineage summary.
%   P8 (spec C.8) truncation by rejection resampling (no clamp, FR-520):
%       200 base draws keep |delta_p_hotel_unplanned| inside the frozen
%       trunc bound with no value sitting exactly on the bound (clamping
%       would pile mass there) and with tail mass above 15 kW (for
%       TruncN(0, 10, +/-20) the probability that no draw out of 200
%       exceeds 15 is ~1e-13), u_river_mean >= registry lower bound.
%   Run (from the Model E workspace root):
%     matlab -batch "run('scripts/+mission_v3/tests/test_env_realize.m')"
%   Discipline: ASCII only; no absolute paths; no chained indexing; no
%   global RNG; the test writes nothing to results/.

ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

n_fail = 0;

% frozen registry reads (values consumed, never hard-coded)
envj = local_read_json(fullfile(ROOT, 'config', 'mission_v3', ...
    'environment_regime_params.json'));
tokj = local_read_json(fullfile(ROOT, 'config', 'mission_v3', ...
    'heldout_token_registry.json'));
dagj = local_read_json(fullfile(ROOT, 'config', 'mission_v3', ...
    'conditional_dag_config.json'));
polj = local_read_json(fullfile(ROOT, 'config', 'mission_v3', ...
    'seed_tree_policy.json'));

STRAT_R = 'STRAT_ROUTINE_PATROL__NORMAL';      % caller label (quota layer)
STRAT_E = 'STRAT_EMERGENCY_RESPONSE__NORMAL';  % caller label (quota layer)

%% P1 (C.1): OU stationarity / autocorrelation vs tau on a long path
ok_p1 = false;
try
    sigma1 = envj.regimes.NORMAL.delta_u.sigma_mps;
    tau1 = envj.regimes.NORMAL.delta_u.tau_s;
    dag_ou = mission_v3.dag_sample('draw', 't009_ou_stats_seed_01', 3, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R);
    out_ou = mission_v3.env_realize('realize', dag_ou, 400000);
    x = out_ou.x_ou_mps;
    nsteps = numel(x) - 1;
    var_est = var(x);
    xc = x - mean(x);
    ss = sum(xc.^2);
    acf1 = sum(xc(1:end-1) .* xc(2:end)) / ss;
    lagN = 900;
    acfN = sum(xc(1:end-lagN) .* xc(1+lagN:end)) / ss;
    tgt1 = exp(-1 / tau1);
    tgtN = exp(-lagN / tau1);
    ok_p1 = (nsteps >= 200000) ...
        && (var_est >= 0.6 * sigma1^2) && (var_est <= 1.4 * sigma1^2) ...
        && (abs(acf1 - tgt1) <= 0.05) ...
        && (abs(acfN - tgtN) <= 0.05);
    fprintf(['[T] P1 OU stats: steps=%d var=%.6f sigma^2=%.6f ', ...
        'acf1=%.4f tgt=%.4f acf900=%.4f tgt=%.4f: %s\n'], ...
        nsteps, var_est, sigma1^2, acf1, tgt1, acfN, tgtN, ...
        local_verdict(ok_p1));
catch p1_err
    fprintf('[T] P1 exception: %s\n', p1_err.message);
end
n_fail = n_fail + ~ok_p1;

%% P2 (C.2): prehistory grid assertions
ok_p2 = false;
try
    dag_ph = mission_v3.dag_sample('draw', 't009_prehist_seed_01', 5, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R);
    out_ph = mission_v3.env_realize('realize', dag_ph, 'auto');
    tg = out_ph.t_grid_s;
    grid_dt = envj.ou_process.injection.grid_s;
    ok_p2 = (tg(1) == -1800) ...
        && (out_ph.ou.prehistory_s == 1800) ...
        && (sum(tg < 0) == 1800) ...
        && (tg(1800) == -1) ...
        && (tg(1801) == 0) ...
        && (out_ph.ou.dt_s == grid_dt) ...
        && (tg(end) == out_ph.ou.t_end_s) ...
        && (out_ph.ou.n_points == numel(tg));
    fprintf(['[T] P2 prehistory: t0=%.0f n_neg=%d t(1800)=%.0f ', ...
        't(1801)=%.0f dt=%.0f t_end=%.0f: %s\n'], tg(1), sum(tg < 0), ...
        tg(1800), tg(1801), out_ph.ou.dt_s, tg(end), ...
        local_verdict(ok_p2));
catch p2_err
    fprintf('[T] P2 exception: %s\n', p2_err.message);
end
n_fail = n_fail + ~ok_p2;

%% P3 (C.3): out-of-bounds must reject, with counting (injected params)
ok_p3 = false;
try
    % (a) inflated OU sigma -> flow sup above the strictest axis bound
    ov_flow = struct('regimes', struct('NORMAL', ...
        struct('delta_u', struct('sigma_mps', 5.0))));
    r_a = mission_v3.env_realize('sample', 't009_flow_rej_seed_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 3, ...
        'ExhaustAction', 'return_ledger', 'ParamOverride', ov_flow);
    ok_a = (~r_a.accepted) && (r_a.n_attempts == 3) ...
        && local_rows_match(r_a.ledger, 3, ...
        'REJECTED_SAMPLING_ELIGIBILITY', 'SUPPORT_BOUND', '');
    if ok_a
        for k = 1:numel(r_a.ledger)
            row = r_a.ledger(k);
            ev = row.disposition.evidence;
            ok_a = ok_a && (ev.flow_sup_abs > ev.flow_bound);
        end
    end
    % (a+) default exhaust action must raise the frozen MAX_RETRY error
    [ok_e, id_e] = local_expect_error(@() mission_v3.env_realize('sample', ...
        't009_flow_rej_seed_01', 0, 'ROUTINE_PATROL', 'NORMAL', STRAT_R, ...
        'MaxAttempts', 2, 'ParamOverride', ov_flow));
    ok_e = ok_e && strcmp(char(id_e), 'GATE:MISSION_V3:ENV:MAX_RETRY');

    % (b) HT <= 1.2 domain floor -> PHYSICAL_DOMAIN_BOUND
    ov_ht = struct('chart_depth_voyage_base', ...
        struct('per_regime', struct('NORMAL', ...
        struct('mu', 3.0, 'sigma', 0.02, 'lower', 2.9))), ...
        'regimes', struct('NORMAL', ...
        struct('water_level_offset', struct('mu', 0.0, 'sigma', 0.02))));
    r_b = mission_v3.env_realize('sample', 't009_htlow_seed_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 4, ...
        'DraftM', 3.0, 'ExhaustAction', 'return_ledger', ...
        'ParamOverride', ov_ht);
    ok_b = (~r_b.accepted) && (r_b.n_attempts == 4) ...
        && local_rows_match(r_b.ledger, 4, ...
        'REJECTED_SAMPLING_ELIGIBILITY', 'PHYSICAL_DOMAIN_BOUND', '');
    if ok_b
        for k = 1:numel(r_b.ledger)
            row = r_b.ledger(k);
            ev = row.disposition.evidence;
            ok_b = ok_b && (ev.ht_effective <= ev.ht_domain_floor);
        end
    end

    % (c) HT in (1.2, 1.5) -> SUPPORT_BOUND
    r_c = mission_v3.env_realize('sample', 't009_htmid_seed_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 4, ...
        'DraftM', 2.222222222222222, 'ExhaustAction', 'return_ledger', ...
        'ParamOverride', ov_ht);
    ok_c = (~r_c.accepted) && (r_c.n_attempts == 4) ...
        && local_rows_match(r_c.ledger, 4, ...
        'REJECTED_SAMPLING_ELIGIBILITY', 'SUPPORT_BOUND', '');
    if ok_c
        for k = 1:numel(r_c.ledger)
            row = r_c.ledger(k);
            ev = row.disposition.evidence;
            ok_c = ok_c && (ev.ht_effective > ev.ht_domain_floor) ...
                && (ev.ht_effective < ev.ht_support_min);
        end
    end
    ok_p3 = ok_a && ok_e && ok_b && ok_c;
    fprintf('[T] P3 rejects+counting: flow=%d maxretry=%d htlow=%d htmid=%d: %s\n', ...
        ok_a, ok_e, ok_b, ok_c, local_verdict(ok_p3));
catch p3_err
    fprintf('[T] P3 exception: %s\n', p3_err.message);
end
n_fail = n_fail + ~ok_p3;

%% P4 (C.4): R1 round-trip sharing on one U_river path
ok_p4 = false;
try
    dag_r1 = mission_v3.dag_sample('draw', 't009_r1_seed_01', 11, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R);
    out_r1 = mission_v3.env_realize('realize', dag_r1, 'auto');
    segs = out_r1.segments;
    n_out = 0;
    ret_i = 0;
    berth_i = 0;
    signs_ok = true;
    max_out_end = 0;
    for i = 1:numel(segs)
        sg = segs(i);
        if strcmp(sg.role, 'return')
            ret_i = i;
            signs_ok = signs_ok && (sg.along_sign == 1) ...
                && strcmp(sg.direction, 'downstream');
        elseif strcmp(sg.role, 'berth') || strcmp(sg.role, 'standby')
            berth_i = i;
            signs_ok = signs_ok && (sg.along_sign == 0) ...
                && strcmp(sg.direction, 'none');
        else
            n_out = n_out + 1;
            signs_ok = signs_ok && (sg.along_sign == -1) ...
                && strcmp(sg.direction, 'upstream');
            if sg.idx_end > max_out_end
                max_out_end = sg.idx_end;
            end
        end
    end
    recomp_ok = true;
    for i = 1:numel(segs)
        sg = segs(i);
        if ~sg.on_grid
            recomp_ok = false;
            continue;
        end
        w = out_r1.u_river_mps(sg.idx_start:sg.idx_end);
        along = sg.along_sign * w;
        recomp_ok = recomp_ok ...
            && (sg.u_signed_min == min(along)) ...
            && (sg.u_signed_max == max(along)) ...
            && (sg.u_signed_mean == mean(along)) ...
            && (sg.u_abs_sup == max(abs(w))) ...
            && (sg.u_adverse_abs_sup == max(0.0, -min(along))) ...
            && (sg.u_following_abs_sup == max(0.0, max(along)));
    end
    single_ok = (out_r1.ou.ff_draws == dag_r1.current_truth_ff_n) ...
        && (numel(out_r1.u_river_mps) == out_r1.ou.n_points) ...
        && (numel(out_r1.x_ou_mps) == out_r1.ou.n_points) ...
        && (ret_i > 0) && (berth_i > 0) && (n_out >= 2) ...
        && (segs(ret_i).idx_start >= max_out_end) ...
        && (segs(ret_i).idx_end <= out_r1.ou.n_points);
    ok_p4 = signs_ok && recomp_ok && single_ok;
    fprintf(['[T] P4 R1 sharing: n_out=%d signs=%d recompute=%d ', ...
        'single_path=%d: %s\n'], n_out, signs_ok, recomp_ok, single_ok, ...
        local_verdict(ok_p4));
catch p4_err
    fprintf('[T] P4 exception: %s\n', p4_err.message);
end
n_fail = n_fail + ~ok_p4;

%% P5 (C.5): structural heldout tokens (registry-driven expectations)
ok_p5 = false;
try
    id_c1 = local_token_id_by_factor(tokj, 'BRIDGE_PASSAGE');
    id_c2 = local_token_id_by_factor(tokj, 'HARBOR');
    id_c3 = local_token_id_by_factor(tokj, 'EMERGENCY_SPRINT');
    band_c1 = local_token_band(tokj, 'BRIDGE_PASSAGE');
    ok_ids = ~isempty(id_c1) && ~isempty(id_c2) && ~isempty(id_c3) ...
        && ~isempty(band_c1);

    % C2: HT in [1.5, 2.0], ROUTINE_PATROL carries a harbor segment
    ov_c23 = struct('chart_depth_voyage_base', ...
        struct('per_regime', struct('NORMAL', ...
        struct('mu', 3.5, 'sigma', 0.02, 'lower', 3.4))), ...
        'regimes', struct('NORMAL', ...
        struct('u_river_mag', struct('mu', 0.5, 'sigma', 0.1, 'lower', 0), ...
        'water_level_offset', struct('mu', 0.0, 'sigma', 0.02))));
    r_c2 = mission_v3.env_realize('sample', 't009_c2_seed_01', 0, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 3, ...
        'DraftM', 2.0, 'ExhaustAction', 'return_ledger', ...
        'ParamOverride', ov_c23);
    ok_c2 = (~r_c2.accepted) && (r_c2.n_attempts == 3) ...
        && local_rows_match(r_c2.ledger, 3, ...
        'REJECTED_STRUCTURAL_HELDOUT', 'none', id_c2);

    % C3: same HT band, EMERGENCY_RESPONSE carries the rush segment
    r_c3 = mission_v3.env_realize('sample', 't009_c3_seed_01', 0, ...
        'EMERGENCY_RESPONSE', 'NORMAL', STRAT_E, 'MaxAttempts', 3, ...
        'DraftM', 2.0, 'ExhaustAction', 'return_ledger', ...
        'ParamOverride', ov_c23);
    ok_c3 = (~r_c3.accepted) && (r_c3.n_attempts == 3) ...
        && local_rows_match(r_c3.ledger, 3, ...
        'REJECTED_STRUCTURAL_HELDOUT', 'none', id_c3);

    % C1: bridge segment adverse component inside HIGH_ID_BAND while the
    % whole-voyage sup stays inside the strictest axis bound
    ov_c1 = struct('regimes', struct('NORMAL', ...
        struct('u_river_mag', struct('mu', 1.44, 'sigma', 0.01, ...
        'lower', 1.4), 'delta_u', struct('sigma_mps', 0.005))));
    dag_c1 = mission_v3.dag_sample('draw', 't009_c1_seed_01', 7, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'ParamOverride', ov_c1);
    out_c1 = mission_v3.env_realize('realize', dag_c1, 'auto', ...
        'ParamOverride', ov_c1);
    d1 = out_c1.disposition;
    br_i = 0;
    segs_c1 = out_c1.segments;
    for i = 1:numel(segs_c1)
        sg = segs_c1(i);
        if strcmp(sg.role, 'bridge')
            br_i = i;
        end
    end
    ok_c1 = strcmp(d1.disposition, 'REJECTED_STRUCTURAL_HELDOUT') ...
        && strcmp(d1.token_id, id_c1) ...
        && (d1.evidence.trigger_segment_index == br_i) ...
        && (br_i > 0) ...
        && (segs_c1(br_i).u_adverse_abs_sup >= band_c1.min) ...
        && (segs_c1(br_i).u_adverse_abs_sup <= band_c1.max) ...
        && (out_c1.disposition.evidence.flow_sup_abs ...
        <= out_c1.disposition.evidence.flow_bound);
    ok_p5 = ok_ids && ok_c2 && ok_c3 && ok_c1;
    fprintf('[T] P5 tokens: ids=%d C2=%d C3=%d C1=%d: %s\n', ...
        ok_ids, ok_c2, ok_c3, ok_c1, local_verdict(ok_p5));
catch p5_err
    fprintf('[T] P5 exception: %s\n', p5_err.message);
end
n_fail = n_fail + ~ok_p5;

%% P6 (C.6): determinism - two identical sample runs
ok_p6 = false;
try
    r1 = mission_v3.env_realize('sample', 't009_det_seed_01', 42, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 15, ...
        'ExhaustAction', 'return_ledger');
    r2 = mission_v3.env_realize('sample', 't009_det_seed_01', 42, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 15, ...
        'ExhaustAction', 'return_ledger');
    det_ok = r1.accepted && r2.accepted ...
        && (r1.n_attempts == r2.n_attempts);
    z1 = r1.realization;
    z2 = r2.realization;
    det_ok = det_ok ...
        && isequal(z1.u_river_mps, z2.u_river_mps) ...
        && isequal(z1.t_grid_s, z2.t_grid_s) ...
        && isequal(z1.segments, z2.segments) ...
        && isequal(z1.traffic, z2.traffic) ...
        && isequal(z1.water, z2.water) ...
        && isequal(z1.bounds, z2.bounds) ...
        && isequal(z1.tokens, z2.tokens) ...
        && isequal(z1.disposition, z2.disposition);
    led_ok = numel(r1.ledger) == numel(r2.ledger);
    if led_ok
        for k = 1:numel(r1.ledger)
            w1 = r1.ledger(k);
            w2 = r2.ledger(k);
            led_ok = led_ok ...
                && (w1.attempt == w2.attempt) ...
                && (w1.base_draw_index == w2.base_draw_index) ...
                && isequal(w1.draw, w2.draw) ...
                && isequal(w1.disposition, w2.disposition);
        end
    end
    ok_p6 = det_ok && led_ok;
    fprintf(['[T] P6 determinism: accepted=%d attempts=%d ', ...
        'realization=%d ledger=%d: %s\n'], r1.accepted, r1.n_attempts, ...
        det_ok, led_ok, local_verdict(ok_p6));
catch p6_err
    fprintf('[T] P6 exception: %s\n', p6_err.message);
end
n_fail = n_fail + ~ok_p6;

%% P7 (C.7): disposition structure completeness (ACCEPT record)
ok_p7 = false;
try
    r1 = mission_v3.env_realize('sample', 't009_det_seed_01', 42, ...
        'ROUTINE_PATROL', 'NORMAL', STRAT_R, 'MaxAttempts', 15, ...
        'ExhaustAction', 'return_ledger');
    z = r1.realization;
    d = z.disposition;
    want_disp = {'ACCEPT', 'REJECTED_SAMPLING_ELIGIBILITY', ...
        'REJECTED_STRUCTURAL_HELDOUT', 'FAILED_CONTRACT', ...
        'FAILED_GENERATION', 'FAILED_PHYSICAL_DOMAIN'};
    want_reason = {'SUPPORT_BOUND', 'PHYSICAL_DOMAIN_BOUND', ...
        'DURATION_BELOW_MIN', 'none'};
    fn_d = fieldnames(d);
    ev = d.evidence;
    fn_e = fieldnames(ev);
    ls = d.lineage_summary;
    fn_l = fieldnames(ls);
    struct_ok = all(ismember(want_disp, z.disposition_enum)) ...
        && all(ismember(want_reason, z.reason_enum)) ...
        && (numel(z.disposition_enum) == 6) ...
        && (numel(z.reason_enum) == 4) ...
        && all(ismember({'disposition', 'reason', 'token_id', ...
        'base_draw_index', 'stratum_id', 'lineage_summary', 'evidence'}, ...
        fn_d)) ...
        && all(ismember({'ht_effective', 'ht_domain_floor', ...
        'ht_support_min', 'flow_sup_abs', 'flow_axis', 'flow_bound', ...
        'has_return_segment', 'trigger_segment_index', ...
        'trigger_segment_role'}, fn_e)) ...
        && all(ismember({'formal_seed', 'branch_paths', ...
        'current_truth_lineage_id'}, fn_l));
    acc_ok = strcmp(d.disposition, 'ACCEPT') ...
        && strcmp(d.reason, 'none') ...
        && isempty(d.token_id) ...
        && (r1.n_attempts >= 1) ...
        && (numel(ls.branch_paths) == numel(polj.branches)) ...
        && ~isempty(regexp(ls.current_truth_lineage_id, ...
        '^[0-9a-f]{64}$', 'once')) ...
        && strcmp(ls.formal_seed, 't009_det_seed_01');
    row_ok = (r1.n_attempts <= numel(r1.ledger));
    if row_ok
        wacc = r1.ledger(r1.n_attempts);
        row_ok = (wacc.attempt == r1.n_attempts) ...
            && strcmp(wacc.disposition.disposition, 'ACCEPT') ...
            && (wacc.base_draw_index == 42 + r1.n_attempts - 1);
    end
    ok_p7 = struct_ok && acc_ok && row_ok;
    fprintf('[T] P7 disposition structure: enum=%d accept=%d ledger_row=%d: %s\n', ...
        struct_ok, acc_ok, row_ok, local_verdict(ok_p7));
catch p7_err
    fprintf('[T] P7 exception: %s\n', p7_err.message);
end
n_fail = n_fail + ~ok_p7;

%% P8 (C.8): truncation by rejection resampling, no clamp
ok_p8 = false;
try
    tr = 20.0;
    nodes = dagj.nodes;
    for k = 1:numel(nodes)
        nd = nodes{k};
        if strcmp(nd.node_id, 'delta_p_hotel_unplanned')
            tr = nd.params.trunc_abs_kw;
        end
    end
    urm_lower = envj.regimes.NORMAL.u_river_mag.lower;
    n_draw = 200;
    dph = zeros(1, n_draw);
    um = zeros(1, n_draw);
    ffn = zeros(1, n_draw);
    for k = 1:n_draw
        dg = mission_v3.dag_sample('draw', 't009_trunc_seed_01', k - 1, ...
            'ROUTINE_PATROL', 'NORMAL', STRAT_R);
        dph(k) = dg.delta_p_hotel_unplanned;
        um(k) = dg.u_river_mean;
        ffn(k) = dg.current_truth_ff_n;
    end
    ok_p8 = all(abs(dph) <= tr) ...
        && ~any(abs(dph) == tr) ...
        && (max(abs(dph)) > 15.0) ...
        && (abs(mean(dph)) < 3.0) ...
        && all(um >= urm_lower) ...
        && all(ffn >= 1);
    fprintf(['[T] P8 truncation: max|dph|=%.3f (bound %.1f) ', ...
        'mean=%.3f min u_mean=%.3f (lower %.1f): %s\n'], ...
        max(abs(dph)), tr, mean(dph), min(um), urm_lower, ...
        local_verdict(ok_p8));
catch p8_err
    fprintf('[T] P8 exception: %s\n', p8_err.message);
end
n_fail = n_fail + ~ok_p8;

%% summary
fprintf('[T] test_env_realize: %d/8 cases passed\n', 8 - n_fail);
if n_fail > 0
    error('GATE:test_env_realize %d case(s) failed', n_fail);
end
fprintf('test_env_realize: ALL PASS (8/8)\n');

%% ======================== local functions ==========================

function obj = local_read_json(ppath)
% Frozen registries are validator-clean (strict parser + self_hash
% cross-check in mission_v3.validate_registries); jsondecode consumption
% follows the seed_tree.m precedent.
    obj = jsondecode(fileread(ppath));
end

function ok = local_rows_match(lg, n_expected, disp_name, reason_name, tok_name)
% Every ledger row carries the expected disposition / reason / token id
% and the ledger has exactly n_expected rows (no draw dropped).
    ok = (numel(lg) == n_expected);
    for k = 1:numel(lg)
        row = lg(k);
        dd = row.disposition;
        ok = ok && strcmp(dd.disposition, disp_name) ...
            && strcmp(dd.reason, reason_name) ...
            && strcmp(dd.token_id, tok_name);
    end
end

function id = local_token_id_by_factor(tokj, factor_a)
    id = '';
    lst = tokj.tokens;
    for k = 1:numel(lst)
        t = lst(k);
        if strcmp(t.factor_a, factor_a)
            id = t.token_id;
            return;
        end
    end
end

function band = local_token_band(tokj, factor_a)
    band = [];
    lst = tokj.tokens;
    for k = 1:numel(lst)
        t = lst(k);
        if strcmp(t.factor_a, factor_a)
            band = t.support_band;
            return;
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
