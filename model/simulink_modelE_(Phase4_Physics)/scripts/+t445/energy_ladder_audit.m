function r = energy_ladder_audit()
% t445.energy_ladder_audit — T-007 Amendment C 能量边界阶梯（不改 k_eq）
% 同一非正式 fixture：law=4 λ=5，SOC 0.58→0.62。保留原 FAIL 证据。
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
cfg = phase4f_t445_config();
MDL = 'HybridShip_ModelE';
if bdIsLoaded(MDL), close_system(MDL, 0); end
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.58;
SCN.voyage_distance_m = 8000;
stopT = 1800;
assignin('base', 'ECMS_Lambda', 5.0);
t445.inject_scenario(MDL, SCN, 4, stopT);
out = sim(MDL);
ds = model.extract_logsout(out);
assignin('base', 'ECMS_Lambda', 0.425);

t = ds.t(:);
soc = ds.SOC(:);
Pbus = ds.P_bat_actual_kW(:);   % bus-side; >0 discharge to bus
V = ds.V_bat_V(:);
I = ds.I_bat_A(:);              % >0 discharge
Pgen = ds.P_gen_actual_kW(:);
dt = [diff(t); 0];

i1 = find(soc >= 0.62, 1, 'first');
assert(~isempty(i1), 'T445:LADDER:SOC', 'SOC never reached 0.62');
i0 = 1;
seg = false(size(t)); seg(i0:i1) = true;
chg = (I < 0) & seg;            % realized charge current
dis = (I > 0) & seg;

Q = evalin('base', 'Bat_Q_nom_Ah');
eta_c = evalin('base', 'Bat_eta_coul');
eta_dcdc = evalin('base', 'ASSUMED_eta_dcdc');
Vnom = evalin('base', 'V_bat_nom');
cap = cfg.k_eq.bat_cap_kWh;
soc0 = soc(i0); soc1 = soc(i1);

% Energies on charge current only, all positive for charging input
E_bus = sum((-Pbus) .* dt .* (Pbus < 0) .* seg) / 3600;           % kWh into converter
E_term_VI = sum((V .* (-I)) .* dt .* chg) / 3.6e6;                % kWh, V*I in W
P_term_from_bus = Pbus * eta_dcdc;                                % charge: P_t = P_bus*eta
E_term_from_bus = sum((-P_term_from_bus) .* dt .* (Pbus < 0) .* seg) / 3600;
E_gen = sum(Pgen .* dt .* seg) / 3600;

E_SOC_nom = cap * (soc1 - soc0);
ocv_nodes = evalin('base', 'Bat_SOC_LUT_nodes');
ocv_lut = evalin('base', 'Bat_OCV_LUT_V');
soc_grid = linspace(soc0, soc1, 400);
ocv_s = interp1(ocv_nodes, ocv_lut, soc_grid, 'linear');
E_OCV = (Q / 1000.0) * trapz(soc_grid, ocv_s);                     % Ah*V/1000 = kWh

% Coulomb reconstruction
dSOC_I = zeros(size(I));
dSOC_I(I >= 0) = -I(I >= 0) .* dt(I >= 0) / (3600 * Q);
dSOC_I(I < 0)  = -eta_c * I(I < 0) .* dt(I < 0) / (3600 * Q);
dSOC_pred = sum(dSOC_I(seg));
coulomb_err = abs((soc1 - soc0) - dSOC_pred);

% Charge-only SOC increment from I
dSOC_chg_only = sum(dSOC_I(chg));
E_SOC_nom_chg_only = cap * dSOC_chg_only;

term_vs_VI = abs(E_term_VI - E_term_from_bus) / max(abs(E_term_from_bus), 1e-12);
eta_dcdc_meas = E_term_VI / max(E_bus, 1e-12);
dcdc_err = abs(eta_dcdc_meas - eta_dcdc);

chk_term = term_vs_VI <= 0.001;
chk_coul = coulomb_err <= 1e-4;
chk_dcdc = dcdc_err <= 0.005;
accounting_pass = chk_term && chk_coul && chk_dcdc;

ratio_nom_bus = E_SOC_nom / max(E_bus, 1e-12);
ratio_nom_bus_chg_only = E_SOC_nom_chg_only / max(E_bus, 1e-12);
unexplained_072 = (ratio_nom_bus < 0.80) && (ratio_nom_bus_chg_only < 0.80);

r = struct();
r.schema = 't445_t007_energy_ladder_v1';
r.task = 'T-007';
r.purpose = 'QUALIFICATION';
r.amendment = 'C_energy_boundary_ladder';
r.config_sha256 = cfg.config_sha256;
r.preserves_fail_artifact = true;
r.fail_artifact = 'qualification/t007_keq_charge_restoration.json';
r.SOC0 = soc0; r.SOC1 = soc1; r.t_cross_s = t(i1);
r.E_SOC_nom_kWh = E_SOC_nom;
r.E_OCV_kWh = E_OCV;
r.E_terminal_VI_kWh = E_term_VI;
r.E_terminal_from_bus_kWh = E_term_from_bus;
r.E_bus_converter_input_kWh = E_bus;
r.E_generator_electric_kWh = E_gen;
r.E_SOC_nom_charge_only_kWh = E_SOC_nom_chg_only;
r.dSOC_measured = soc1 - soc0;
r.dSOC_from_I = dSOC_pred;
r.coulomb_abs_err = coulomb_err;
r.term_vs_VI_rel_err = term_vs_VI;
r.eta_dcdc_frozen = eta_dcdc;
r.eta_dcdc_measured = eta_dcdc_meas;
r.eta_dcdc_abs_err = dcdc_err;
r.ratio_E_SOC_nom_over_E_bus = ratio_nom_bus;
r.ratio_charge_only_nom_over_bus = ratio_nom_bus_chg_only;
r.checks = struct('terminal_vs_VI_le_0p1pct', chk_term, ...
    'coulomb_abs_le_1e4', chk_coul, 'dcdc_vs_0p97_le_0p005', chk_dcdc);
r.accounting_pass = accounting_pass;
r.unexplained_loss_near_0p72 = unexplained_072;
r.k_eq_unchanged = true;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));

qdir = fullfile(ROOT, 'results', 't445_oracle_gate', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 't007_energy_ladder.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[T-007 ladder] accounting_pass=%d term_rel=%.4g coul=%.4g dcdc_err=%.4g\n', ...
    accounting_pass, term_vs_VI, coulomb_err, dcdc_err);
fprintf('  E_SOC_nom=%.4f E_OCV=%.4f E_term=%.4f E_bus=%.4f E_gen=%.4f\n', ...
    E_SOC_nom, E_OCV, E_term_VI, E_bus, E_gen);
fprintf('  ratio_net=%.4f ratio_chg_only=%.4f unexplained_072=%d\n', ...
    ratio_nom_bus, ratio_nom_bus_chg_only, unexplained_072);
if ~accounting_pass
    error('T445:T007:LADDER', 'accounting checks FAIL — STOP, do not change k_eq');
end
if unexplained_072
    error('T445:T007:LOSS', 'unexplained efficiency near 0.72 remains — STOP');
end
end
