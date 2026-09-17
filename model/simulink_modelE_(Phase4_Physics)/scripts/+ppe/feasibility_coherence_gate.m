function r = feasibility_coherence_gate()
% ppe.feasibility_coherence_gate — T-A6C 契约自洽门（B7：非结果性，合成/冻结输入）。
% 验证修正案契约内部可满足性；产出 S_terminal_admissible。
ROOT = ppe.ppe_root();
ta0 = jsondecode(fileread(fullfile(ROOT, '..', '..', '.specs', 'phase4-f-practical-predictive-ems', ...
    'reserve_derivation_TA0.json')));
checks = struct();

% C1 NORMAL_OPERATIONAL_SOC 非空
checks.normal_soc_nonempty = ta0.s_terminal_admissible.max > ta0.s_terminal_admissible.min;
% C2 放电事件存活区域非空
checks.discharge_survivable_nonempty = ta0.s_terminal_admissible.min <= 0.25;
% C3 充电事件存活区域非空
checks.charge_survivable_nonempty = ta0.s_terminal_admissible.max >= 0.90;
% C4 合集非空
checks.combined_nonempty = ~isempty(ta0.s_terminal_admissible.min);
% C5 派生储备数值存在且为正
dr = ta0.derived_reserves;
checks.reserves_positive = dr.P_reserve_dis_kW > 0 && dr.E_reserve_dis_kWh > 0 && dr.P_reserve_chg_kW > 0;
% C6 派生结果：SOP 自动满足（EV-1 SOC 谷/峰处的 SOP ≥ 峰值缺口/盈余）
ev1 = ta0.events.EV1_single_to_dual_transition_plus_load_step;
ev3 = ta0.events.EV3_load_rejection;
checks.sop_auto_satisfied = ev1.sop_at_min_kW >= ev1.peak_gap_kW && ev3.sop_at_max_kW >= ev3.peak_surplus_kW;
% C7 硬边界未穿越
checks.hard_bounds_respected = ev1.soc_min >= 0.10 && ev3.soc_max <= 0.95;

fn = fieldnames(checks); allpass = true;
for i = 1:numel(fn)
    v = checks.(fn{i});
    if ~(islogical(v) && v), allpass = false; fprintf('  COHERENCE FAIL %s\n', fn{i}); end
end

r = struct('schema', 'ppe_feasibility_contract_coherence_v1', 'pass', allpass, ...
    's_terminal_admissible', ta0.s_terminal_admissible, 'checks', checks, ...
    'derived_reserves', dr, ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z')));
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
fid = fopen(fullfile(qdir, 'feasibility_coherence_gate.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[coherence] pass=%d admissible=[%.2f,%.2f]\n', r.pass, ...
    r.s_terminal_admissible.min, r.s_terminal_admissible.max);
if ~allpass, error('PPE:COH:FAIL', '契约自洽门 FAIL'); end
end
