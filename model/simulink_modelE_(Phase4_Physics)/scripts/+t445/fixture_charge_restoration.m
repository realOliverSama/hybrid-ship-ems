function r = fixture_charge_restoration()
% t445.fixture_charge_restoration — T-007 k_eq^E 参考充电/回补（DGN-F-06 §6.4）
% SUPERSEDED as T-007 pass criterion (Amendment C 2026-09-15).
% The 0.58→0.62 vs 0.9687 FAIL artifact is preserved and must not be overwritten.
% T-007 now qualifies energy-boundary identity (qualify_t007_identity).
ROOT = t445.modele_root();
failPath = fullfile(ROOT, 'results', 't445_oracle_gate', 'qualification', ...
    't007_keq_charge_restoration.json');
if exist(failPath, 'file')
    error('T445:T007:PRESERVED_FAIL', ...
        ['Charge-restoration FAIL artifact is preserved (Amendment C). ', ...
         'Do not rerun. Use t445 identity qualification.']);
end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_TestScenarios.m')));
cfg = phase4f_t445_config();
MDL = 'HybridShip_ModelE';
if bdIsLoaded(MDL)
    close_system(MDL, 0);
end
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

SCN = evalin('base', 'SCENARIOS(1)');
SCN.initial_SOC = 0.58;
SCN.voyage_distance_m = 8000;
stopT = 1800;
% 固定高 λ law=4：低负荷下机组满发，形成固定充电段（非 law=5 SOC 均衡）
assignin('base', 'ECMS_Lambda', 5.0);
t445.inject_scenario(MDL, SCN, 4, stopT);
out = sim(MDL);
ds = model.extract_logsout(out);
assignin('base', 'ECMS_Lambda', 0.425);

soc = ds.SOC(:); t = ds.t(:);
i0 = 1;
i1 = find(soc >= 0.62, 1, 'first');
assert(~isempty(i1), 'T445:T007:SOC', 'SOC never reached 0.62 (end=%.4f)', soc(end));
% rest: 30 s after crossing 0.62 or to end
t_rest = t(i1) + 30;
i2 = find(t >= t_rest, 1, 'first');
if isempty(i2), i2 = numel(t); end

Pbat = ds.P_bat_actual_kW(:);
dt = [diff(t); 0];
chg = Pbat < 0;
seg = false(size(t));
seg(i0:i1) = true;
E_bus_kWh = sum((-Pbat) .* dt .* chg .* seg) / 3600;
dSOC = soc(i1) - soc(i0);
cap = cfg.k_eq.bat_cap_kWh;
E_stored_kWh = cap * dSOC;
eta_m = E_stored_kWh / max(E_bus_kWh, 1e-12);
eta_ref = cfg.k_eq.eta_chg_ref;
err = eta_m - eta_ref;
pass = abs(err) <= 0.005;

r = struct();
r.schema = 't445_t007_keq_qualification_v1';
r.task = 'T-007';
r.purpose = 'QUALIFICATION';
r.fixture_id = 'KEQ-CHARGE-RESTORATION';
r.config_sha256 = cfg.config_sha256;
r.SOC_start = soc(i0);
r.SOC_cross = soc(i1);
r.t_cross_s = t(i1);
r.dSOC = dSOC;
r.E_bus_charge_kWh = E_bus_kWh;
r.E_stored_kWh = E_stored_kWh;
r.eta_chg_measured = eta_m;
r.eta_chg_ref = eta_ref;
r.abs_err = err;
r.abs_tol = 0.005;
r.pass = pass;
r.k_eq_status_next = 'CANONICAL';
if ~pass, r.k_eq_status_next = 'PROVISIONAL_PENDING_QUALIFICATION'; end
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));

qdir = fullfile(ROOT, 'results', 't445_oracle_gate', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 't007_keq_charge_restoration.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
ledger = fullfile(ROOT, 'results', 't445_oracle_gate', 'access_ledger.jsonl');
fid = fopen(ledger, 'a');
fprintf(fid, '%s\n', jsonencode(struct('purpose','QUALIFICATION','task','T-007', ...
    'artifact','qualification/t007_keq_charge_restoration.json','utc',r.utc)));
fclose(fid);
fprintf('[T-007] eta_m=%.6f eta_ref=%.4f err=%.6f pass=%d\n', eta_m, eta_ref, err, pass);
if ~pass
    error('T445:T007:FAIL', 'eta_chg_measured off ref by %.6f > 0.005 — STOP (R1)', err);
end
end
