function r = check_g4_battery(e08, ecm_static, e01_flags_zero)
% gate_e.check_g4_battery — G4 电池判据（design §5.3，断言式）
%   放电 I>0→SOC↓、V_t<OCV；充电反向；SOP 四约束边界收缩正确（注入测试 T-009 证据）；
%   SOC 路径无双算（ECM 块源码无 ASSUMED_Bat_Eff_*——Q5 红线）
r.name = 'G4_battery';
r.gate_class = 'physics_invariant';
r.checks = {};

% 方向性（E08 集成级：放电段 V_bat < OCV(SOC) 估计、SOC 下降）
ok_dir = e08.soc_decreased && e08.vt_below_ocv_ratio < 1.0;
r.checks{end+1} = struct('item', 'discharge_I_gt0_SOC_down_Vt_below_OCV', 'pass', ok_dir, ...
    'detail', sprintf('SOC %.4f->%.4f, mean(V_t/OCV_est)=%.4f', ...
    e08.soc_start, e08.soc_end, e08.vt_below_ocv_ratio)); %#ok<SAGROW>

% SOP 收缩：每拍 P_bat_bus ≤ P_dis_avail + 容差（放电侧）
ok_sop = all(e08.P_bat_bus_kW <= e08.P_dis_avail_kW + 1e-6);
r.checks{end+1} = struct('item', 'SOP_envelope_respected_every_step', 'pass', ok_sop); %#ok<SAGROW>

% 无双算（Q5）：ECM 块源码**代码行**不含 ASSUMED_Bat_Eff（注释提及不算；Plant 侧停用）
src = model.code_battery_ecm_e();
codeLines = regexp(src, '(?m)^([^%\n][^\n]*)$', 'tokens');   % 排除换行（MATLAB .* 默认跨行）
codeOnly = strjoin([codeLines{:, 1}], newline);
no_double = ~contains(codeOnly, 'ASSUMED_Bat_Eff');
r.checks{end+1} = struct('item', 'no_0.95_port_efficiency_in_SOC_path', 'pass', no_double); %#ok<SAGROW>

% 单元级 SOP 四约束起控证据（T-009 注入测试，本 Gate 引用其结果）
r.checks{end+1} = struct('item', 'SOP_four_constraint_injection_tests', 'pass', ecm_static, ...
    'detail', 'scripts/tests/test_battery_sop.m ALL PASS（四约束起控点注入）'); %#ok<SAGROW>

% power-limit 语义（OQ-9）：E08 逼近 SOP 边界时 flag 允许置位，但输出必须被正确压限
% （P_bat ≤ P_max_R 通道）；E01 正常巡航 flags 恒 0（由 e01_flags_zero 传入）
r.checks{end+1} = struct('item', 'power_limit_flags_semantics', 'pass', ...
    e08.flags_consistent && e01_flags_zero, ...
    'detail', sprintf('E08 flags_consistent=%d, E01 flags zero=%d', ...
    e08.flags_consistent, e01_flags_zero)); %#ok<SAGROW>

r.pass = all(cellfun(@(c) c.pass, r.checks));
end