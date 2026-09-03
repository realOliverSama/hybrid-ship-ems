function r = check_g2_hull_shallow(e01, e02, e03, e04, e05)
% gate_e.check_g2_hull_shallow — G2 船体/浅水判据（design §5.3）
%   V↑⇒R↑、H/T↓⇒R_total↑（固定 V；不要求 w 单调）、U_against↑⇒P_prop↑；
%   E02 极速带 [12.5,15.5] kn（design_derived）；E01 巡航电功率 [274,335] kW（design_derived）
r.name = 'G2_hull_shallow';
r.gate_class = 'physics_invariant';   % 单调性部分；极速/功率带为 design_derived（字段内分级）
r.checks = {};

% 方向性 1：V↑⇒R↑（E01 稳态 vs E02 极速稳态）
ok_v = e02.R_steady_N > e01.R_steady_N * 1.5;   % 极速阻力显著高于巡航（rel tol 由物理带保证）
r.checks{end+1} = struct('item', 'V_up_R_up', 'pass', ok_v, ...
    'detail', sprintf('R(11kn)=%.0f N < R(vmax)=%.0f N', e01.R_steady_N, e02.R_steady_N)); %#ok<SAGROW>

% 方向性 2：U_against↑⇒P_prop↑（E04 逆水 vs E03 顺水）
ok_dir = e04.P_prop_kW > e03.P_prop_kW;
r.checks{end+1} = struct('item', 'U_against_up_P_prop_up', 'pass', ok_dir, ...
    'detail', sprintf('P_downstream=%.1f < P_upstream=%.1f kW', e03.P_prop_kW, e04.P_prop_kW)); %#ok<SAGROW>

% 方向性 3：浅水六档 h↓⇒R_total↑（固定 V；零反例，rel tol 1e-6）
Rs = [e05.cases.R_hull_N];
hts = [e05.cases.HT_ratio];
mono_ok = true; n_viol = 0;
for k = 2:numel(Rs)                                  % 档序按 H/T 降序排列（E05 生成序）
    if hts(k) >= hts(k-1) || Rs(k) <= Rs(k-1) * (1 - 1e-6)
        mono_ok = false; n_viol = n_viol + 1;
    end
end
r.checks{end+1} = struct('item', 'shallow_mono_HT_down_R_up_zero_violation', 'pass', mono_ok, ...
    'violations', n_viol, 'R_by_tier', {Rs}); %#ok<SAGROW>

% 域外报警（E05 内不触发；触发记录为 informational）
warn_any = any([e05.cases.domain_warning]);
r.checks{end+1} = struct('item', 'domain_warning_not_triggered_in_valid_domain', 'pass', ~warn_any); %#ok<SAGROW>

% design_derived 内部 sanity（论文不得表述为独立实船验证）
ok_cruise = e01.P_prop_kW >= 274 && e01.P_prop_kW <= 335;
r.checks{end+1} = struct('item', 'cruise_power_band_274_335_kW', 'pass', ok_cruise, ...
    'gate_class', 'design_derived', 'value_kW', e01.P_prop_kW); %#ok<SAGROW>
ok_speed = e02.v_steady_kn >= 12.5 && e02.v_steady_kn <= 15.5;
r.checks{end+1} = struct('item', 'full_power_speed_band_12.5_15.5_kn', 'pass', ok_speed, ...
    'gate_class', 'design_derived', 'value_kn', e02.v_steady_kn); %#ok<SAGROW>

r.pass = all(cellfun(@(c) c.pass, r.checks));
end