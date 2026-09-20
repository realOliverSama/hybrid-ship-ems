function r = hard_feasibility(ds, cfg, varargin)
% t445.hard_feasibility — DGN-F-12 §12.2 硬可行性逐项（合成或真 logsout）
p = inputParser;
p.addParameter('T_frozen_s', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('C_bus', [], @(x) isempty(x) || isnumeric(x));
p.parse(varargin{:});
opt = p.Results;
if nargin < 2 || isempty(cfg)
    cfg = phase4f_t445_config();
end
hf = cfg.hard_feasibility;
dt = [diff(ds.t); 0];
T_end = ds.t(end);
if isempty(opt.T_frozen_s), opt.T_frozen_s = T_end; end

task_ok = abs(T_end - opt.T_frozen_s) <= hf.task_complete.abs_t_end_tol_s;

if isfield(ds, 'derate08_active')
    derate_s = sum(dt .* (ds.derate08_active > 0.5));
else
    derate_s = 0;
end
if isfield(ds, 'P_bus_residual_kW')
    Pres = ds.P_bus_residual_kW;
else
    Pres = zeros(size(ds.t));
end
short_mask = Pres < hf.load_shortfall.p_residual_neg_thresh_kW;
short_kWh = sum(dt(short_mask) .* abs(Pres(short_mask))) / 3600;
load_ok = (derate_s == 0) && (short_kWh == 0);

soc_out = (ds.SOC < hf.soc_hard_bounds.soc_min - 1e-12) | ...
          (ds.SOC > hf.soc_hard_bounds.soc_max + 1e-12);
soc_hard_s = sum(dt .* soc_out);
soc_hard_ok = soc_hard_s == 0;

dSOC = abs(ds.SOC(end) - ds.SOC(1));
term_ok = dSOC <= hf.terminal_soc.abs_dsoc_max;

tol = hf.battery_sop_v_i.duration_tol_s;
Pbat = ds.P_bat_actual_kW;
Pav = zeros(size(Pbat));
chg = Pbat < 0;
Pav(chg) = ds.P_bat_chg_avail_kW(chg);
Pav(~chg) = ds.P_bat_dis_avail_kW(~chg);
p_over_s = sum(dt .* (abs(Pbat) > Pav + 1e-9));
v_out_s = sum(dt .* ((ds.V_bat_V < hf.battery_sop_v_i.v_min_V - 1e-12) | ...
                     (ds.V_bat_V > hf.battery_sop_v_i.v_max_V + 1e-12)));
i_over_s = sum(dt .* (abs(ds.I_bat_A) > hf.battery_sop_v_i.i_abs_max_A + 1e-12));
bat_ok = (p_over_s <= tol) && (v_out_s <= tol) && (i_over_s <= tol);

on1 = ds.state_DG1 == 3; on2 = ds.state_DG2 == 3;
if isfield(ds, 'P_gen1_kW'), Pg1 = ds.P_gen1_kW; else, Pg1 = ds.P_gen_actual_kW / 2; end
if isfield(ds, 'P_gen2_kW'), Pg2 = ds.P_gen2_kW; else, Pg2 = ds.P_gen_actual_kW / 2; end
pmin = hf.genset_limits.p_min_kW; pmax = hf.genset_limits.p_max_kW;
band1 = on1 & ((Pg1 < pmin - 1e-9) | (Pg1 > pmax + 1e-9));
band2 = on2 & ((Pg2 < pmin - 1e-9) | (Pg2 > pmax + 1e-9));
band_s = sum(dt .* (band1 | band2));
dP = [0; abs(diff(ds.P_gen_actual_kW)) ./ max(diff(ds.t), 1e-12)];
ramp_s = sum(dt .* (dP > hf.genset_limits.ramp_kW_per_s + hf.genset_limits.ramp_tol_kW_per_s));
genset_ok = (band_s == 0) && (ramp_s == 0);

if ~isempty(opt.C_bus) && isfield(ds, 'P_bus_residual_kW')
    c5 = gate_ctrl.check_c5_energy_closure(ds, 't445', opt.C_bus);
    c5_ok = c5.pass;
    c5_ratio = c5.checks{1}.ratio;
else
    c5_ok = true;
    c5_ratio = 0;
end

s_hard = 0;
if isfield(ds, 'safety_s1a'), s_hard = s_hard + sum(ds.safety_s1a(:) > 0.5); end
if isfield(ds, 'safety_s1b'), s_hard = s_hard + sum(ds.safety_s1b(:) > 0.5); end
if isfield(ds, 'safety_s2'),  s_hard = s_hard + sum(ds.safety_s2(:) > 0.5); end
safety_ok = s_hard == 0;

items = struct( ...
    'task_complete', task_ok, ...
    'load_shortfall', load_ok, ...
    'soc_hard_bounds', soc_hard_ok, ...
    'terminal_soc', term_ok, ...
    'battery_sop_v_i', bat_ok, ...
    'genset_limits', genset_ok, ...
    'energy_closure_c5', c5_ok, ...
    'safety_hard_events', safety_ok);
r = struct();
r.pass = all(struct2array(items));
r.items = items;
r.detail = struct('T_end', T_end, 'T_frozen', opt.T_frozen_s, ...
    'derate08_s', derate_s, 'shortfall_kWh', short_kWh, ...
    'soc_hard_s', soc_hard_s, 'dSOC', dSOC, ...
    'p_over_s', p_over_s, 'v_out_s', v_out_s, 'i_over_s', i_over_s, ...
    'genset_band_s', band_s, 'ramp_s', ramp_s, 'c5_ratio', c5_ratio, ...
    'safety_hard_count', s_hard);
end
