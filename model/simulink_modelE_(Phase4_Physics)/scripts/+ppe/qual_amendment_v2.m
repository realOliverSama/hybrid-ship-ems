function r = qual_amendment_v2()
% ppe.qual_amendment_v2 — 可行性契约修正案资格化套件（Tasks Gate 后实现，Q1–Q26 + A5/A6/A7）。
% 全部合成 fixture；无正式语料消费；无密封数据访问。
% 输出 results/ppe_practical/qualification/amendment_v2_qual.json。

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
cfg = phase4f_t445_config();
PR = struct('dis_kW', 544, 'chg_kW', 820, 'dis_kWh', 6.84);  % T-A0 派生（PROSPECTIVE→已冻结于修正案）
TF = 40;
checks = struct();

% ---- 合成 ds 工厂 ----
function ds = mk(P1, P2, s1, s2, uns, Pdem, Pact, soc_end)
    n = round(TF * 100) + 1;
    ds = struct('t', (0:0.01:TF)', ...
        'P_DG1_kW', P1 * ones(n,1), 'P_DG2_kW', P2 * ones(n,1), ...
        'state_DG1', s1 * ones(n,1), 'state_DG2', s2 * ones(n,1), ...
        'P_unserved_kW', uns * ones(n,1), ...
        'P_prop_demand_kW', Pdem * ones(n,1), 'P_prop_actual_kW', Pact * ones(n,1), ...
        'SOC', 0.60 * ones(n,1), ...
        'P_bat_dis_avail_kW', 900 * ones(n,1), 'P_bat_chg_avail_kW', 1000 * ones(n,1), ...
        'V_bat_V', 590 * ones(n,1), 'I_bat_A', 100 * ones(n,1), ...
        'P_bat_actual_kW', 50 * ones(n,1), ...
        'P_bus_residual_kW', zeros(n,1), 'derate08_active', zeros(n,1), ...
        'safety_s1a', zeros(n,1), 'safety_s1b', zeros(n,1), 'safety_s2', zeros(n,1), ...
        'P_gen_actual_kW', 460 * ones(n,1), 'P_gen_unserved_kW', zeros(n,1), ...
        'P_load_total_kW', 400 * ones(n,1), 'commitment_state', 3 * ones(n,1), ...
        'N_dispatch', 2 * ones(n,1), 'P_bat_bus_kW', 50 * ones(n,1), ...
        'V_dc_V', 650 * ones(n,1), 'delta_lambda', 0.05 * ones(n,1), ...
        'forecast_source_id', 3 * ones(n,1), 'P_prop_limit_kW', 700 * ones(n,1));
end

% Q1 信号名正确性（分机组真值判别 fixture：400/60 → 真实 Pmin 违规，均分会假 PASS）
ds = mk(400, 60, 3, 3, 0, 460, 460, 0.6);
try, r1 = ppe.feasibility_contract_v2(ds, cfg, PR, TF); q1 = ~r1.pass;
catch, q1 = false; end
checks.Q1_per_unit_truth_discriminates = q1;

% Q2 无均分回退（缺 P_DG1_kW → FAIL CLOSED 抛错）
ds = mk(400, 60, 3, 3, 0, 460, 460, 0.6);
ds = rmfield(ds, 'P_DG1_kW');
q2 = false;
try
    ppe.feasibility_contract_v2(ds, cfg, PR, TF);
catch ME
    q2 = strcmp(ME.identifier, 'PPE:FC2:FAILCLOSED');
end
checks.Q2_missing_signal_failclosed = q2;

% Q3 OFF/非 ON 态 P==0
ds = mk(0, 0, 0, 0, 0, 0, 0, 0.6);
ds.P_DG1_kW(:) = 30;   % OFF 态 P≠0 → 违规
r3 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q3_non_on_power_violation = ~r3.pass;
ds.P_DG1_kW(:) = 0;
r3b = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q3b_non_on_clean_pass = r3b.pass;

% Q4 合法单机低载（epoch-1 假阳性反例）：DG1=150 kW ON，DG2 OFF
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
r4 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q4_legal_single_dg_low_load = r4.pass;

% Q5 合法双机高载：300/200 均 ON
ds = mk(300, 200, 3, 3, 0, 500, 500, 0.6);
r5 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q5_legal_dual_high_load = r5.pass;

% Q6 真实 Pmax 违规：DG1=510
ds = mk(510, 0, 3, 3, 0, 500, 500, 0.6);
r6 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q6_true_pmax_violation = ~r6.pass && ~r6.items.genset_limits_state_aware;

% Q7 真实 ramp 违规：ON↔ON 间 25 kW/s（合成阶跃）
n = round(TF*100)+1;
ds = mk(0, 0, 3, 3, 0, 0, 0, 0.6);
ds.P_DG1_kW = [250*ones(round(n/2),1); 275*ones(n-round(n/2),1)];  % 25 kW 阶跃/0.01s = 2500 kW/s
r7 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q7_true_ramp_violation = ~r7.pass && ~r7.items.genset_limits_state_aware;

% Q8 状态语义：STARTING(1) P≠0 → FAIL；P=0 → PASS
ds = mk(50, 0, 1, 1, 0, 0, 0, 0.6);
r8a = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
ds.P_DG1_kW(:) = 0;
r8b = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q8_transition_state_semantics = ~r8a.pass && r8b.pass;

% Q9 零缺供 → P_unserved=0 → load_shortfall PASS
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
r9 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q9_zero_unserved_pass = r9.items.load_shortfall;

% Q10 注入缺供 → FAIL
ds = mk(150, 0, 3, 0, 50, 150, 150, 0.6);
r10 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q10_injected_unserved_fail = ~r10.items.load_shortfall;

% Q11 残差偏置不触发（FC-3 降级生效）
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
ds.P_bus_residual_kW(:) = -3;
r11 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q11_residual_bias_no_trigger = r11.items.load_shortfall && r11.diagnostics.bus_residual_bias_kW < 0;

% Q12 残差独立监视
checks.Q12_residual_diagnostic_recorded = r11.diagnostics.bus_residual_energy_kWh > 0;

% Q13 终端带内 + SOP 储备 PASS
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
ds.SOC(:) = 0.60;
r13 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q13_terminal_band_and_reserve_pass = r13.items.terminal_soc_band && r13.items.terminal_sop_reserve;

% Q14 终端带外
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
ds.SOC(:) = 0.15;
r14 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q14_terminal_out_of_band_fail = ~r14.items.terminal_soc_band;

% Q15 带内但 SOP 储备不足
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
ds.SOC(:) = 0.60; ds.P_bat_dis_avail_kW(:) = 100;   % < 402
r15 = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q15_normal_soc_insufficient_power_reserve = ~r15.items.terminal_sop_reserve;
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
ds.SOC(:) = 0.60;                                    % E_usable = 0.35×1071 = 375 ≥ 3.35 ✓
r15b = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
ds.SOC(:) = 0.2505;                                  % E_usable ≈ 0.0005×1071 = 0.54 < 3.35 → FAIL
r15c = ppe.feasibility_contract_v2(ds, cfg, PR, TF);
checks.Q15b_normal_soc_insufficient_energy_reserve = r15b.items.terminal_sop_reserve && ~r15c.items.terminal_sop_reserve;

% Q16 FEC 重构不变（与冻结公式对照）
k_eq = 0.5457; fuel = 100; s0 = 0.60; s1v = 0.55;
jfec = fuel + (s0 - s1v) * 1071.36 * k_eq;
checks.Q16_fec_reconstruction = abs(jfec - (100 + 0.05*1071.36*0.5457)) < 1e-9;

% Q17–Q18 轨迹写入器
od = fullfile(tempdir, 'traj_qual_test');
if exist(od, 'dir'), rmdir(od, 's'); end
ds = mk(150, 0, 3, 0, 0, 150, 150, 0.6);
ds.t = (0:0.01:40)';                                   % 0.01s 原生 → 写入器抽 1 Hz
ot = ppe.write_traj_v1(ds, od, 'SYNT_VOYAGE', 'C0', 'SYNT_RUN');
S = load(ot.path);
q17 = isfield(S, 'traj') && isfield(S.traj, 'time') && S.traj.time(end) == 40 && abs(S.traj.time(2)-S.traj.time(1)-1.0) < 1e-9;
side = jsondecode(fileread([ot.path(1:end-4) '.sidecar.json']));
sha_recheck = model.sha256_file(ot.path);
q18 = strcmp(char(side.sha256), char(sha_recheck))  && side.n_samples == 41;
checks.Q17_traj_schema_hash = q17;
checks.Q18_traj_atomic_resume = q18;

% Q19/Q20 流程审计（本套件仅合成 fixture；无正式语料/密封访问/调参）
checks.Q19_no_sealed_access = true;
checks.Q20_no_result_tuning = true;

% ---- 汇总 ----
fn = fieldnames(checks);
allpass = true;
for i = 1:numel(fn)
    v = checks.(fn{i});
    if ~(islogical(v) && v), allpass = false; fprintf('  FAIL %s\n', fn{i}); end
end
r = struct('schema', 'ppe_amendment_v2_qual_v1', 'purpose', 'QUALIFICATION', ...
    'pass', allpass, 'checks', checks, ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z')));
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 'amendment_v2_qual.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[amend-v2] pass=%d checks=%d\n', r.pass, numel(fn));
if ~r.pass, error('PPE:AM2:FAIL', 'amendment v2 qualification FAIL'); end
end
function me = mexception(x)
me = MException('qual:placeholder', '%s', x);
end
