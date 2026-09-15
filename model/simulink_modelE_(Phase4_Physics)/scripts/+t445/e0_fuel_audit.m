function r = e0_fuel_audit()
% t445.e0_fuel_audit — Amendment E0 诊断（不改 Q4/Q6 门、不改 10 s 窗、不进 T-011）
%
% E0-1: 原生迹线 FuelModelE 积分 vs 10 s 均功率重构（非线性/分箱）
% E0-2: 匹配信息植物夹具 P_load(t)=Pbar_k 分段恒定（run-local，不落盘 .slx）
% E0-3: DeltaJ proxy / native / matched
% E0-4: 总误差 = 匹配输入代理误差 + 10 s 表示/非线性 + 未解释残差
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
cfg = phase4f_t445_config();
params = ecms2.ecms_params();
tbl = t445.policy_table_cached(cfg, params);
slx = fullfile(ROOT, 'model', 'HybridShip_ModelE.slx');
sha0 = model.sha256_file(slx);
MDL = 'HybridShip_ModelE';

P_bp = evalin('base', 'DG_BSFC_P_kW');
g_kWh = evalin('base', 'DG_BSFC_gkWh');
Cstart = evalin('base', 'DG_C_start_kg');
tstart = evalin('base', 'DG_t_starting_s');
tsync = evalin('base', 'DG_t_sync_s');
fm = struct('P_bp', P_bp, 'g_kWh', g_kWh, 'C_start', Cstart, ...
    't_start_s', tstart, 't_sync_s', tsync);

SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.60;
settle = 120; win = 300; stopT = settle + win; dt = 10;
KS = cfg.lambda_law.K_S_B2_num / cfg.lambda_law.K_S_B2_den;
t0s = (0:dt:(stopT-dt)).';
e0 = [t0s, zeros(size(t0s)), zeros(size(t0s))];

fprintf('[E0] native zero-correction 0–%.0f s\n', stopT);
outZ = t445.run_arm_b0b1('B1', SCN, 'StopTime', stopT, 'EHatData', e0, 'K_S', KS);
dsZ = model.extract_logsout(outZ);
[incZ, Pbar, SOC_s] = local_window_inc(dsZ, settle, stopT);
sZ = t445.b2_window_dp(Pbar, SOC_s, cfg, tbl, 'freeze_dlambda', 0, ...
    't0', settle, 'SOC_target', SCN.initial_SOC, 'N0', 2);
sO = t445.b2_window_dp(Pbar, SOC_s, cfg, tbl, 't0', settle, ...
    'SOC_target', SCN.initial_SOC, 'N0', 2);
t0w = (settle:dt:(stopT-dt)).';
nd = numel(sO.dlambda);
ehatO = [t0w(1:nd), zeros(nd,1), sO.dlambda];
if numel(t0w) > nd
    extra = t0w(nd+1:end);
    ehatO = [ehatO; [extra, zeros(size(extra)), sO.dlambda(end)*ones(size(extra))]];
end
ehatO = [e0(e0(:,1) < settle, :); ehatO];

fprintf('[E0] native optimized 0–%.0f s\n', stopT);
outO = t445.run_arm_b0b1('B1', SCN, 'StopTime', stopT, 'EHatData', ehatO, 'K_S', KS);
dsO = model.extract_logsout(outO);
incO = local_window_inc(dsO, settle, stopT);

e01Z = local_e01(dsZ, settle, stopT, dt, fm, incZ.fuel);
e01O = local_e01(dsO, settle, stopT, dt, fm, incO.fuel);
e01C = local_e01(dsZ, 0, 300, dt, fm, []);

% Matched-input: same model structure for the whole 0–420 s run.
% Prefix t<120 copies the already-realized native P_load (settled IC, not
% future information). Window t>=120 is the legal 10 s Pbar staircase.
% No SimState: injecting From Workspace changes the OP checksum.
t_st = (settle:dt:(stopT-dt)).';
P_st = Pbar(:);
if numel(P_st) > numel(t_st)
    P_st = P_st(1:numel(t_st));
elseif numel(P_st) < numel(t_st)
    P_st = [P_st; P_st(end)*ones(numel(t_st)-numel(P_st),1)];
end
[t_inj, P_inj] = local_build_inject(dsZ, settle, t_st, P_st);

cl = onCleanup(@() local_cleanup(MDL));
local_install_pload_stair(MDL);
fprintf('[E0] matched-input zero-correction 0–%.0f s (stair from %.0f)\n', stopT, settle);
dsZm = local_matched_run(MDL, SCN, stopT, e0, KS, t_inj, P_inj);
fprintf('[E0] matched-input optimized 0–%.0f s (stair from %.0f)\n', stopT, settle);
dsOm = local_matched_run(MDL, SCN, stopT, ehatO, KS, t_inj, P_inj);
clear cl;
local_cleanup(MDL);

incZm = local_window_inc(dsZm, settle, stopT);
incOm = local_window_inc(dsOm, settle, stopT);
stairZ = local_stair_check(dsZm, settle, stopT, dt, P_st);
stairO = local_stair_check(dsOm, settle, stopT, dt, P_st);

if bdIsLoaded(MDL)
    close_system(MDL, 0);
end
sha1 = model.sha256_file(slx);

dJ_proxy = sZ.fuel_proxy_kg - sO.fuel_proxy_kg;
dJ_native = incZ.fuel - incO.fuel;
dJ_matched = incZm.fuel - incOm.fuel;

dec6 = local_decomp(sZ.fuel_proxy_kg, incZ.fuel, incZm.fuel, e01Z);
dec4 = local_decomp(sO.fuel_proxy_kg, incO.fuel, incOm.fuel, e01O);

r = struct();
r.schema = 't445_t010_e0_fuel_audit_v1';
r.task = 'T-010';
r.amendment = 'E0_10s_nonlinear_fuel_audit';
r.purpose = 'QUALIFICATION';
r.not_a_pass_gate = true;
r.t010_status = 'OPEN_QUALIFICATION_FAILED';
r.t011_t016 = 'BLOCKED';
r.formal_corpus_observed = false;
r.reserve_used = false;
r.config_sha256 = cfg.config_sha256;
r.preserves_fail_artifact = true;
r.fail_artifacts = { ...
    'qualification/t010_b2_optimizer_qual.json'; ...
    'qualification/t010_b2_optimizer_qual_amd_d.json'; ...
    'qualification/t010_fuel_gap_d1.json'};
r.slx_sha256_before = sha0;
r.slx_sha256_after = sha1;
r.slx_unchanged = strcmp(sha0, sha1);
r.k_eq_E = cfg.k_eq.value_kg_per_kWh;
r.w_term = cfg.v_term.w_term;
r.k_eq_unchanged = abs(cfg.k_eq.value_kg_per_kWh - 0.5457) < 1e-12;
r.w_term_unchanged = abs(cfg.v_term.w_term - 1.17e5) < 1e-6;
r.law = 6;
r.action_space = 'delta_lambda_only';
r.oracle_bins_s = 10;
r.frozen_fuel_tol = 0.01;
r.matched_fixture = struct( ...
    'method', 'run_local_P_load_total_from_workspace', ...
    'prefix_t_lt_120', 'copy_native_realized_P_load', ...
    'window_t_ge_120', 'piecewise_constant_Pbar_10s', ...
    'simstate_used', false, ...
    'future_1Hz_to_optimizer', false);
r.identities = struct( ...
    'b2_window_dp_sha256', local_sha_m(ROOT, {'scripts','+t445','b2_window_dp.m'}), ...
    'fuel_model_e_unit_sha256', local_sha_m(ROOT, {'scripts','+t445','fuel_model_e_unit.m'}), ...
    'pms_predict_sha256', local_sha_m(ROOT, {'scripts','+t445','pms_predict.m'}), ...
    'qual_b2_optimizer_sha256', local_sha_m(ROOT, {'scripts','+t445','qual_b2_optimizer.m'}), ...
    'e0_fuel_audit_sha256', local_sha_m(ROOT, {'scripts','+t445','e0_fuel_audit.m'}));
r.e01_settled_zero = local_e01_pub(e01Z);
r.e01_settled_opt = local_e01_pub(e01O);
r.e01_startup_zero = local_e01_pub(e01C);
r.native_zero = local_summ(incZ, sZ, dsZ, settle, stopT);
r.native_opt = local_summ(incO, sO, dsO, settle, stopT);
r.matched_zero = local_matched_summ(incZm, dsZm, sZ, settle, stopT, stairZ);
r.matched_opt = local_matched_summ(incOm, dsOm, sO, settle, stopT, stairO);
r.deltaJ = struct( ...
    'proxy_kg', dJ_proxy, 'native_kg', dJ_native, 'matched_kg', dJ_matched, ...
    'sign_proxy', sign(dJ_proxy), 'sign_native', sign(dJ_native), ...
    'sign_matched', sign(dJ_matched), ...
    'sign_proxy_eq_native', (sign(dJ_proxy) == sign(dJ_native)) || (abs(dJ_proxy)<1e-9 && abs(dJ_native)<1e-9), ...
    'sign_proxy_eq_matched', (sign(dJ_proxy) == sign(dJ_matched)) || (abs(dJ_proxy)<1e-9 && abs(dJ_matched)<1e-9), ...
    'abs_proxy_minus_native', abs(dJ_proxy - dJ_native), ...
    'abs_proxy_minus_matched', abs(dJ_proxy - dJ_matched), ...
    'common_mode_bias_native_kg', abs((sZ.fuel_proxy_kg-incZ.fuel) - (sO.fuel_proxy_kg-incO.fuel)), ...
    'common_mode_bias_matched_kg', abs((sZ.fuel_proxy_kg-incZm.fuel) - (sO.fuel_proxy_kg-incOm.fuel)));
r.decomp_q6_zero = dec6;
r.decomp_q4_opt = dec4;
r.matched_vs_frozen_1pct = struct( ...
    'q6_abs_rel', abs(dec6.matched_rel), ...
    'q4_abs_rel', abs(dec4.matched_rel), ...
    'q6_within_1pct', abs(dec6.matched_rel) <= 0.01, ...
    'q4_within_1pct', abs(dec4.matched_rel) <= 0.01, ...
    'diagnostic_only_not_a_pass', true);
r.high_risk_incremental_sign_flip = ...
    (sign(dJ_proxy) ~= 0) && (sign(dJ_native) ~= 0) && (sign(dJ_proxy) ~= sign(dJ_native));
r.design_not_auto_amended = true;
r.frozen_file_or_model_modified = ~r.slx_unchanged;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 't445_oracle_gate', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
outp = fullfile(qdir, 't010_e0_fuel_audit.json');
fid = fopen(outp, 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fid = fopen(fullfile(ROOT, 'results', 't445_oracle_gate', 'access_ledger.jsonl'), 'a');
fprintf(fid, '%s\n', jsonencode(struct('purpose','QUALIFICATION','task','T-010-E0', ...
    'artifact','qualification/t010_e0_fuel_audit.json','utc',r.utc)));
fclose(fid);
fprintf('[E0] wrote %s slx_ok=%d high_risk=%d\n', outp, r.slx_unchanged, r.high_risk_incremental_sign_flip);
fprintf('[E0-1 zero] Jnat=%.4f Jmean=%.4f d=%.4f rel=%.4f expl=%.3f\n', ...
    e01Z.J_native, e01Z.J_meanDG, e01Z.J_meanDG_minus_native, e01Z.rel_repr, dec6.explained_fraction);
fprintf('[E0-1 opt ] Jnat=%.4f Jmean=%.4f d=%.4f rel=%.4f expl=%.3f\n', ...
    e01O.J_native, e01O.J_meanDG, e01O.J_meanDG_minus_native, e01O.rel_repr, dec4.explained_fraction);
fprintf('[E0-2] matched zero rel=%.4f opt rel=%.4f stair_max=%.4g/%.4g\n', ...
    dec6.matched_rel, dec4.matched_rel, stairZ.max_abs_err_kW, stairO.max_abs_err_kW);
fprintf('[E0-3] dJ proxy=%.4f native=%.4f matched=%.4f\n', dJ_proxy, dJ_native, dJ_matched);
fprintf('[E0-4 Q6] total=%.4f matched=%.4f repr=%.4f unexpl=%.4f expl=%.3f\n', ...
    dec6.total_proxy_minus_native_kg, dec6.matched_input_error_kg, ...
    dec6.repr_10s_nonlinear_kg, dec6.unexplained_kg, dec6.explained_fraction);
fprintf('[E0-4 Q4] total=%.4f matched=%.4f repr=%.4f unexpl=%.4f expl=%.3f\n', ...
    dec4.total_proxy_minus_native_kg, dec4.matched_input_error_kg, ...
    dec4.repr_10s_nonlinear_kg, dec4.unexplained_kg, dec4.explained_fraction);
if ~r.slx_unchanged
    error('T445:E0:SLX', 'slx sha changed — restore required');
end
end

function e = local_e01(ds, t0, t1, dtb, fm, fuel_log)
t = ds.t(:);
Pg1 = local_sig(ds, {'P_DG1_kW','P_gen1_kW'});
Pg2 = local_sig(ds, {'P_DG2_kW','P_gen2_kW'});
st1 = round(local_sig(ds, {'state_DG1'}));
st2 = round(local_sig(ds, {'state_DG2'}));
nB = round((t1-t0)/dtb);
Jnat = zeros(nB,1); Jmean = zeros(nB,1);
Jstart_n = zeros(nB,1); Jstart_m = zeros(nB,1);
Jidle_n = zeros(nB,1);
p1bar = zeros(nB,1); p2bar = zeros(nB,1);
s1r_all = zeros(nB,1); s2r_all = zeros(nB,1);
for k = 1:nB
    lo = t0 + (k-1)*dtb; hi = lo + dtb;
    m = t >= lo - 1e-12 & t < hi - 1e-12;
    if ~any(m), continue; end
    tk = t(m);
    idx = find(m);
    r1 = zeros(numel(idx),1); r2 = r1;
    for ii = 1:numel(idx)
        r1(ii) = t445.fuel_model_e_unit(Pg1(idx(ii)), st1(idx(ii)), ...
            'P_bp', fm.P_bp, 'g_kWh', fm.g_kWh, 'C_start_kg', fm.C_start, ...
            't_start_s', fm.t_start_s, 't_sync_s', fm.t_sync_s);
        r2(ii) = t445.fuel_model_e_unit(Pg2(idx(ii)), st2(idx(ii)), ...
            'P_bp', fm.P_bp, 'g_kWh', fm.g_kWh, 'C_start_kg', fm.C_start, ...
            't_start_s', fm.t_start_s, 't_sync_s', fm.t_sync_s);
    end
    if numel(tk) == 1
        Jnat(k) = (r1(1)+r2(1)) * dtb;
    else
        Jnat(k) = trapz(tk, r1+r2);
    end
    stA = st1(m); stB = st2(m);
    start1 = stA==1 | stA==2; start2 = stB==1 | stB==2;
    idle1 = stA==0 | stA==4; idle2 = stB==0 | stB==4;
    if numel(tk) == 1
        Jstart_n(k) = (r1(1)*double(start1(1)) + r2(1)*double(start2(1))) * dtb;
        Jidle_n(k) = (r1(1)*double(idle1(1)) + r2(1)*double(idle2(1))) * dtb;
    else
        Jstart_n(k) = trapz(tk, r1.*double(start1) + r2.*double(start2));
        Jidle_n(k) = trapz(tk, r1.*double(idle1) + r2.*double(idle2));
    end
    p1m = mean(Pg1(m)); p2m = mean(Pg2(m));
    s1r = local_mode(stA); s2r = local_mode(stB);
    p1bar(k) = p1m; p2bar(k) = p2m; s1r_all(k) = s1r; s2r_all(k) = s2r;
    Jmean(k) = dtb * ( ...
        t445.fuel_model_e_unit(p1m, s1r, 'P_bp', fm.P_bp, 'g_kWh', fm.g_kWh, ...
            'C_start_kg', fm.C_start, 't_start_s', fm.t_start_s, 't_sync_s', fm.t_sync_s) + ...
        t445.fuel_model_e_unit(p2m, s2r, 'P_bp', fm.P_bp, 'g_kWh', fm.g_kWh, ...
            'C_start_kg', fm.C_start, 't_start_s', fm.t_start_s, 't_sync_s', fm.t_sync_s));
    Jstart_m(k) = dtb * ( ...
        t445.fuel_model_e_unit(p1m, s1r, 'P_bp', fm.P_bp, 'g_kWh', fm.g_kWh, ...
            'C_start_kg', fm.C_start, 't_start_s', fm.t_start_s, 't_sync_s', fm.t_sync_s) * double(s1r==1||s1r==2) + ...
        t445.fuel_model_e_unit(p2m, s2r, 'P_bp', fm.P_bp, 'g_kWh', fm.g_kWh, ...
            'C_start_kg', fm.C_start, 't_start_s', fm.t_start_s, 't_sync_s', fm.t_sync_s) * double(s2r==1||s2r==2));
end
e = struct();
e.J_native = sum(Jnat);
e.J_meanDG = sum(Jmean);
e.J_meanDG_minus_native = e.J_meanDG - e.J_native;
e.rel_repr = e.J_meanDG_minus_native / max(abs(e.J_native), 1e-12);
e.J_start_native = sum(Jstart_n);
e.J_start_mean = sum(Jstart_m);
e.J_idle_native = sum(Jidle_n);
e.n_bins = nB;
e.bin_native = Jnat(:).';
e.bin_meanDG = Jmean(:).';
e.Pbar_DG1 = p1bar(:).';
e.Pbar_DG2 = p2bar(:).';
e.state_rep_DG1 = s1r_all(:).';
e.state_rep_DG2 = s2r_all(:).';
if isempty(fuel_log)
    e.plant_log_minus_Jnative = NaN;
else
    e.plant_log_minus_Jnative = fuel_log - e.J_native;
end
e.not_implementation_error_if_repr_only = true;
end

function e = local_e01_pub(e0)
e = e0;
end

function d = local_decomp(Jproxy, Jnative, Jmatched, e01)
total = Jproxy - Jnative;
matched_err = Jproxy - Jmatched;
repr_err = e01.J_meanDG_minus_native;
unexpl = total - matched_err - repr_err;
d = struct();
d.total_proxy_minus_native_kg = total;
d.matched_input_error_kg = matched_err;
d.repr_10s_nonlinear_kg = repr_err;
d.unexplained_kg = unexpl;
d.total_pct_of_native = 100 * total / max(abs(Jnative), 1e-12);
d.matched_pct_of_native = 100 * matched_err / max(abs(Jnative), 1e-12);
d.repr_pct_of_native = 100 * repr_err / max(abs(Jnative), 1e-12);
d.unexplained_pct_of_native = 100 * unexpl / max(abs(Jnative), 1e-12);
d.matched_rel = matched_err / max(abs(Jmatched), 1e-12);
d.repr_rel = e01.rel_repr;
d.plant_matched_minus_native_kg = Jmatched - Jnative;
if abs(total) < 1e-12
    d.explained_fraction = 1;
else
    d.explained_fraction = 1 - abs(unexpl)/abs(total);
end
d.explain_ge_90pct = d.explained_fraction >= 0.90;
d.unexplained_material = abs(unexpl) > 0.10 * max(abs(total), 1e-12);
end

function [t_inj, P_inj] = local_build_inject(dsZ, settle, t_st, P_st)
% Prefix: realized native P_load for t<settle (settled IC only).
% Window: legal 10 s Pbar staircase. Optimizer never sees 1 Hz future.
t = dsZ.t(:); P = dsZ.P_load_total_kW(:);
m = t < settle - 1e-12;
t_pre = t(m); P_pre = P(m);
t_inj = [t_pre; t_st(:)];
P_inj = [P_pre; P_st(:)];
end

function local_cleanup(MDL)
try
    local_restore_pload(MDL);
catch
end
if bdIsLoaded(MDL)
    close_system(MDL, 0);
end
end

function ds = local_matched_run(MDL, SCN, stopT, ehat, KS, t_inj, P_inj)
ROOT = t445.modele_root();
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
    local_install_pload_stair(MDL);
end
t445.inject_scenario(MDL, SCN, 6, stopT);
assignin('base', 'ECMS_EHat_Data', ehat);
assignin('base', 'ECMS_K_S', KS);
assignin('base', 'T445_PLOAD_STAIR', [t_inj(:), P_inj(:)]);
in = Simulink.SimulationInput(MDL);
in = t445.make_simin_zoh(in);
in = in.setModelParameter('StopTime', sprintf('%.4f', stopT));
in = in.setModelParameter('LoadInitialState', 'off');
out = sim(in);
ds = model.extract_logsout(out);
end

function local_install_pload_stair(MDL)
sys = [MDL '/04_Load_Measurement'];
if getSimulinkBlockHandle(sys, true) < 0
    error('T445:E0:LM', '04_Load_Measurement missing');
end
src = [sys '/T445_PLOAD_STAIR_SRC'];
if getSimulinkBlockHandle(src, true) > 0
    return;
end
tot = [sys '/total'];
outp = [sys '/P_load_total_kW'];
ph = get_param(tot, 'PortHandles');
ln = get(ph.Outport, 'Line');
if ln > 0, delete_line(ln); end
pos = get_param(tot, 'Position');
add_block('simulink/Sources/From Workspace', src, ...
    'Position', [pos(1) pos(2)+40 pos(3)+40 pos(4)+40], ...
    'VariableName', 'T445_PLOAD_STAIR', 'Interpolate', 'off', ...
    'OutputAfterFinalValue', 'HoldingFinalValue');
add_line(sys, 'T445_PLOAD_STAIR_SRC/1', 'P_load_total_kW/1');
if getSimulinkBlockHandle(outp, true) < 0
    error('T445:E0:OUT', 'P_load_total_kW outport missing');
end
end

function local_restore_pload(MDL)
if ~bdIsLoaded(MDL), return; end
sys = [MDL '/04_Load_Measurement'];
src = [sys '/T445_PLOAD_STAIR_SRC'];
try
    if getSimulinkBlockHandle(src, true) > 0
        ph = get_param(src, 'PortHandles');
        ln = get(ph.Outport, 'Line');
        if ln > 0, delete_line(ln); end
        delete_block(src);
    end
    tot = [sys '/total'];
    outp = [sys '/P_load_total_kW'];
    if getSimulinkBlockHandle(tot, true) > 0 && getSimulinkBlockHandle(outp, true) > 0
        ph = get_param(outp, 'PortHandles');
        ln = get(ph.Inport, 'Line');
        if isempty(ln) || ln <= 0
            add_line(sys, 'total/1', 'P_load_total_kW/1');
        end
    end
catch ME
    warning('T445:E0:RESTORE', '%s', ME.message);
end
end

function [inc, Pbar, SOC_s] = local_window_inc(ds, t0, t1)
t = ds.t(:); fuel = ds.fuel_total_kg(:); soc = ds.SOC(:);
P = ds.P_load_total_kW(:);
i0 = find(t >= t0 - 1e-9, 1, 'first');
i1 = find(t >= t1 - 1e-9, 1, 'first');
if isempty(i0), i0 = 1; end
if isempty(i1), i1 = numel(t); end
inc = struct('fuel', fuel(i1)-fuel(i0), 'dSOC', soc(i1)-soc(i0), ...
    'SOC0', soc(i0), 'SOC_end', soc(i1));
SOC_s = soc(i0);
if nargout < 2
    Pbar = [];
    return;
end
tt = (t0:t1-1).';
Pp = interp1(t, P, tt, 'linear', 'extrap');
W = t445.assemble_oracle_window(tt - t0, Pp, 'T_end', t1 - t0);
pb = W.Pbar{1}; Pbar = pb(:);
if numel(Pbar) > 30, Pbar = Pbar(1:30); end
end

function s = local_stair_check(ds, t0, t1, dtb, P_st)
t = ds.t(:); P = ds.P_load_total_kW(:);
nB = numel(P_st);
err = zeros(nB,1); meanP = zeros(nB,1);
for k = 1:nB
    lo = t0 + (k-1)*dtb; hi = lo + dtb;
    m = t >= lo - 1e-12 & t < hi - 1e-12;
    if ~any(m)
        err(k) = NaN; continue;
    end
    meanP(k) = mean(P(m));
    err(k) = meanP(k) - P_st(k);
end
s = struct();
s.max_abs_err_kW = max(abs(err(~isnan(err))));
s.mean_abs_err_kW = mean(abs(err(~isnan(err))));
s.bin_mean_P_load = meanP(:).';
s.bin_err = err(:).';
s.ok_1kW = s.max_abs_err_kW <= 1.0;
end

function s = local_summ(inc, sol, ds, t0, t1)
st = local_pms_stats(ds, t0, t1);
s = struct('fuel_plant', inc.fuel, 'fuel_proxy', sol.fuel_proxy_kg, ...
    'fuel_rel', (sol.fuel_proxy_kg-inc.fuel)/max(abs(inc.fuel),1e-12), ...
    'fuel_rel_abs', abs(sol.fuel_proxy_kg-inc.fuel)/max(abs(inc.fuel),1e-12), ...
    'dSOC_plant', inc.dSOC, 'dSOC_proxy', sol.SOC_H-sol.SOC0, ...
    'SOC0', inc.SOC0, 'SOC_end', inc.SOC_end, ...
    'dsoc_err', abs((sol.SOC_H-sol.SOC0)-inc.dSOC), ...
    'oob', sol.oob_query_count, ...
    'mean_N_on', st.mean_N_on, ...
    'mean_P_load', local_mean_sig(ds, 'P_load_total_kW', t0, t1), ...
    'mean_P_gen', local_mean_sig(ds, 'P_gen_actual_kW', t0, t1), ...
    'start_fuel_kg', st.start_fuel_kg, ...
    'idle_fuel_kg', st.idle_fuel_kg, ...
    'frac_start', st.frac_start, ...
    'frac_idle', st.frac_idle);
end

function s = local_matched_summ(inc, ds, sol, t0, t1, stair)
st = local_pms_stats(ds, t0, t1);
s = struct('fuel_plant', inc.fuel, 'fuel_proxy', sol.fuel_proxy_kg, ...
    'fuel_rel', (sol.fuel_proxy_kg-inc.fuel)/max(abs(inc.fuel),1e-12), ...
    'fuel_rel_abs', abs(sol.fuel_proxy_kg-inc.fuel)/max(abs(inc.fuel),1e-12), ...
    'dSOC_plant', inc.dSOC, 'dSOC_proxy', sol.SOC_H-sol.SOC0, ...
    'SOC0', inc.SOC0, 'SOC_end', inc.SOC_end, ...
    'dsoc_err', abs((sol.SOC_H-sol.SOC0)-inc.dSOC), ...
    'oob', sol.oob_query_count, ...
    'mean_N_on', st.mean_N_on, ...
    'mean_P_load', local_mean_sig(ds, 'P_load_total_kW', t0, t1), ...
    'mean_P_gen', local_mean_sig(ds, 'P_gen_actual_kW', t0, t1), ...
    'start_fuel_kg', st.start_fuel_kg, ...
    'idle_fuel_kg', st.idle_fuel_kg, ...
    'frac_start', st.frac_start, ...
    'frac_idle', st.frac_idle, ...
    'stair_max_abs_err_kW', stair.max_abs_err_kW, ...
    'stair_ok_1kW', stair.ok_1kW);
end

function st = local_pms_stats(ds, t0, t1)
t = ds.t(:);
st1 = round(local_sig(ds, {'state_DG1'}));
st2 = round(local_sig(ds, {'state_DG2'}));
m = t >= t0 - 1e-9 & t < t1 - 1e-9;
if ~any(m)
    st = struct('mean_N_on', NaN, 'start_fuel_kg', 0, 'idle_fuel_kg', 0, ...
        'frac_start', NaN, 'frac_idle', NaN);
    return;
end
on = double(st1(m)==3) + double(st2(m)==3);
startm = double(st1(m)==1|st1(m)==2) + double(st2(m)==1|st2(m)==2);
idlem = double(st1(m)==0|st1(m)==4) + double(st2(m)==0|st2(m)==4);
Cstart = 0.6;
if evalin('base', "exist('DG_C_start_kg','var')")
    Cstart = evalin('base', 'DG_C_start_kg');
end
tstart = 25; tsync = 5;
if evalin('base', "exist('DG_t_starting_s','var')")
    tstart = evalin('base', 'DG_t_starting_s');
    tsync = evalin('base', 'DG_t_sync_s');
end
tk = t(m);
if numel(tk) >= 2
    start_s = trapz(tk, startm);
else
    start_s = 0;
end
st = struct();
st.mean_N_on = mean(on);
st.frac_start = mean(startm > 0);
st.frac_idle = mean(idlem == 2);
st.start_fuel_kg = start_s * (Cstart / (tstart + tsync));
st.idle_fuel_kg = 0;
end

function v = local_mean_sig(ds, name, t0, t1)
t = ds.t(:);
if ~isfield(ds, name)
    v = NaN; return;
end
x = ds.(name)(:);
m = t >= t0 - 1e-9 & t < t1 - 1e-9;
if ~any(m), v = NaN; return; end
v = mean(x(m));
end

function v = local_sig(ds, names)
v = [];
for i = 1:numel(names)
    if isfield(ds, names{i})
        v = ds.(names{i})(:); return;
    end
end
error('T445:E0:SIG', 'missing %s', strjoin(names, ','));
end

function s = local_mode(x)
x = x(:);
u = unique(x);
c = zeros(size(u));
for i = 1:numel(u), c(i) = sum(x==u(i)); end
[~, j] = max(c); s = u(j);
end

function h = local_sha_m(ROOT, parts)
p = fullfile(ROOT, parts{:});
h = model.sha256_file(p);
end
