function r = feasibility_contract_v2(ds, cfg, P_reserve, T_frozen, C_bus)
% ppe.feasibility_contract_v2 — 修正版硬可行性评估（feasibility contract amendment）。
%
% 输入：
%   ds       extract_logsout 输出（必需信号见 FAIL CLOSED 清单）
%   cfg      phase4f_t445_config()（冻结阈值只读）
%   P_reserve 终端储备 struct('dis_kW',402,'chg_kW',168,'dis_kWh',3.35)
%             （T-A0 设计基准事件推导，PROSPECTIVE；reserve_derivation_TA0.json）
%   T_frozen 冻结任务时长 [s]
%   C_bus    可选；提供时计算 energy_closure_c5，否则该分量记 true + 诊断注记
%
% 与 t445.hard_feasibility 的差异（amendment FC-1/2/3，Design Gate A1–A7）：
%   FC-1 genset_limits：分机组真值 P_DG1_kW/P_DG2_kW + 状态感知（非 ON 态 P==0；
%        ON 带内；ramp 仅 ON↔ON；断路器事件豁免）；无均分回退；缺信号 FAIL CLOSED。
%   FC-2 load_shortfall：判据 = P_unserved_kW（真实服务缺口）；
%        P_bus_residual_kW 降级为电气一致性诊断（不入硬判据）。
%   FC-3 terminal：运行带 [0.25,0.90] + 终端 SOP/能量储备（非 ±0.005 能量中性；
%        epoch-1 违约正确定性 = LEGACY_TERMINAL_ENERGY_NEUTRALITY_CONTRACT_VIOLATION）。
%   对偶诊断：P_service_gap = max(P_prop_demand − P_prop_actual, 0)。
% 容差 TOL=1e-6 kW：合成零输入 fixture 实测本底 <1e-9 kW，高出 3 个量级（推导记录 Q-fixture）。

if ~isfield(cfg, 'hard_feasibility'), error('PPE:FC2:CFG', 'cfg.hard_feasibility 缺失'); end
hf = cfg.hard_feasibility;
pmin = hf.genset_limits.p_min_kW; pmax = hf.genset_limits.p_max_kW;
rmp  = hf.genset_limits.ramp_kW_per_s + hf.genset_limits.ramp_tol_kW_per_s;
TOL  = 1e-6;

need = {'P_DG1_kW','P_DG2_kW','state_DG1','state_DG2','P_unserved_kW', ...
        'P_prop_demand_kW','P_prop_actual_kW','SOC','P_bat_dis_avail_kW', ...
        'P_bat_chg_avail_kW','V_bat_V','I_bat_A','P_bat_actual_kW'};
for i = 1:numel(need)
    if ~isfield(ds, need{i})
        error('PPE:FC2:FAILCLOSED', '必需信号缺失（无均分回退）：%s', need{i});
    end
end

dt = [diff(ds.t(:)); 0];
s1 = ds.state_DG1(:); s2 = ds.state_DG2(:);
p1 = ds.P_DG1_kW(:);  p2 = ds.P_DG2_kW(:);
on1 = (s1 == 3); on2 = (s2 == 3);
n = numel(ds.t);

% ---- FC-1a 非 ON 态 P==0（状态×约束表：0/1/2/4 恒 0）----
non_on = ~(on1 | on2);
non_on_power_s = sum(dt .* non_on .* ((abs(p1) > TOL) | (abs(p2) > TOL)));

% ---- FC-1b ON 态带内（Pmin/Pmax 分量分列）----
lo1 = on1 & (p1 < pmin - TOL);  hi1 = on1 & (p1 > pmax + TOL);
lo2 = on2 & (p2 < pmin - TOL);  hi2 = on2 & (p2 > pmax + TOL);
pmin_viol_s = sum(dt .* (lo1 | lo2));
pmax_viol_s = sum(dt .* (hi1 | hi2));

% ---- FC-1c ramp（仅 ON↔ON 区间；断路器事件豁免）----
if n >= 2
    rate1 = abs(diff(p1)) ./ max(dt(1:end-1), 1e-12);
    rate2 = abs(diff(p2)) ./ max(dt(1:end-1), 1e-12);
    iv_on = on1(1:end-1) & on1(2:end) & on2(1:end-1) & on2(2:end);
    ramp_s = sum(dt(1:end-1) .* iv_on .* (rate1 > rmp | rate2 > rmp));
else
    ramp_s = 0;
end
genset_ok = (pmin_viol_s == 0) && (pmax_viol_s == 0) && (ramp_s == 0) && (non_on_power_s == 0);

% ---- FC-2 服务缺供（权威信号 P_unserved_kW + 对偶诊断）----
Puns = ds.P_unserved_kW(:);
short_e = sum(dt .* Puns) / 3600;
derate_s = 0;
if isfield(ds, 'derate08_active'), derate_s = sum(dt .* (ds.derate08_active(:) > 0.5)); end
svc_gap = max(ds.P_prop_demand_kW(:) - ds.P_prop_actual_kW(:), 0);
gap_e = sum(dt .* svc_gap) / 3600;
load_ok = (short_e <= TOL) && (derate_s == 0) && (gap_e <= TOL);

% ---- FC-3 母线残差诊断（降级，不入硬判据）----
res_e = 0; res_bias = NaN;
if isfield(ds, 'P_bus_residual_kW')
    Pres = ds.P_bus_residual_kW(:);
    neg = Pres < -1.0;
    res_e = sum(dt(neg) .* abs(Pres(neg))) / 3600;
    if any(neg), res_bias = median(Pres(neg)); end
end

% ---- 终端：运行带 [0.25,0.90] + SOP/能量储备（FC-4）----
dSOC = abs(ds.SOC(end) - ds.SOC(1));
band_ok = (ds.SOC(end) >= 0.25 - 1e-12) && (ds.SOC(end) <= 0.90 + 1e-12);
sop_dis_ok = ds.P_bat_dis_avail_kW(end) >= P_reserve.dis_kW - TOL;
sop_chg_ok = ds.P_bat_chg_avail_kW(end) >= P_reserve.chg_kW - TOL;
e_res_ok = (ds.SOC(end) - 0.25) * 1071.36 >= P_reserve.dis_kWh - 1e-9;
term_ok = band_ok && sop_dis_ok && sop_chg_ok && e_res_ok;

% ---- 其余冻结判据（原样保留）----
soc_out = (ds.SOC(:) < hf.soc_hard_bounds.soc_min - 1e-12) | (ds.SOC(:) > hf.soc_hard_bounds.soc_max + 1e-12);
soc_hard_s = sum(dt .* soc_out);
bat_tol = hf.battery_sop_v_i.duration_tol_s;
Pb = ds.P_bat_actual_kW(:);
Pav = zeros(size(Pb)); chg = Pb < 0;
Pav(chg) = ds.P_bat_chg_avail_kW(chg); Pav(~chg) = ds.P_bat_dis_avail_kW(~chg);
p_over_s = sum(dt .* (abs(Pb) > Pav + 1e-9));
v_out_s = sum(dt .* ((ds.V_bat_V(:) < hf.battery_sop_v_i.v_min_V - 1e-12) | (ds.V_bat_V(:) > hf.battery_sop_v_i.v_max_V + 1e-12)));
i_out_s = sum(dt .* (abs(ds.I_bat_A(:)) > hf.battery_sop_v_i.i_abs_max_A + 1e-12));
bat_ok = (p_over_s <= bat_tol) && (v_out_s <= bat_tol) && (i_out_s <= bat_tol);
task_ok = abs(ds.t(end) - T_frozen) <= hf.task_complete.abs_t_end_tol_s;

s_hard = 0;
if isfield(ds, 'safety_s1a'), s_hard = s_hard + sum(ds.safety_s1a(:) > 0.5); end
if isfield(ds, 'safety_s1b'), s_hard = s_hard + sum(ds.safety_s1b(:) > 0.5); end
if isfield(ds, 'safety_s2'),  s_hard = s_hard + sum(ds.safety_s2(:) > 0.5); end

c5_ok = true; c5_ratio = 0;
if nargin >= 5 && ~isempty(C_bus) && isfield(ds, 'P_bus_residual_kW')
    c5 = gate_ctrl.check_c5_energy_closure(ds, 't445', C_bus);
    c5_ok = c5.pass; c5_ratio = c5.checks{1}.ratio;
end

items = struct('task_complete', task_ok, 'load_shortfall', load_ok, ...
    'soc_hard_bounds', soc_hard_s == 0, 'terminal_soc_band', band_ok, ...
    'terminal_sop_reserve', sop_dis_ok && sop_chg_ok && e_res_ok, ...
    'battery_sop_v_i', bat_ok, 'genset_limits_state_aware', genset_ok, ...
    'energy_closure_c5', c5_ok, 'safety_hard_events', s_hard == 0);

r = struct();
r.schema = 'hard_feasibility_v2';
r.pass = all(struct2array(items));
r.items = items;
r.violations = struct('genset_pmin_s', pmin_viol_s, 'genset_pmax_s', pmax_viol_s, ...
    'genset_ramp_s', ramp_s, 'non_on_power_s', non_on_power_s, ...
    'shortfall_energy_kWh', short_e, 'service_gap_energy_kWh', gap_e, ...
    'derate08_s', derate_s);
r.diagnostics = struct('bus_residual_energy_kWh', res_e, ...
    'bus_residual_bias_kW', res_bias, ...
    'service_gap_note', 'P_service_gap 为对偶一致性诊断（覆盖任何原因的推进缺口）', ...
    'below_min_note', 'below_min_*/min_clamp_* 为 PMS 调度诊断，非机组硬违规（amendment FC-1.4）', ...
    'energy_closure_note', 'C_bus 未提供时 c5 分量记 true（epoch-2 runner 必须提供）');
r.detail = struct('dSOC', dSOC, 'SOC_end', ds.SOC(end), ...
    'P_dis_avail_end', ds.P_bat_dis_avail_kW(end), 'P_chg_avail_end', ds.P_bat_chg_avail_kW(end), ...
    'P_reserve', P_reserve, 'c5_ratio', c5_ratio);
end
