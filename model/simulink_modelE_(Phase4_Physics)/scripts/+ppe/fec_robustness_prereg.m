function r = fec_robustness_prereg()
% ppe.fec_robustness_prereg — T-A6 FEC 终端估值稳健性预注册（Design Gate A7，合成数据）。
% 在观察任何 epoch-2 结局之前预注册敏感性侧与 break-even 分析；禁止用于调参。
k_eq = 0.5457;                    % 冻结主判据
k_lo = 0.8 * k_eq; k_hi = 1.2 * k_eq;
Bat_Cap = 1071.36;

% 合成场景：两臂燃料/SOC 差异（合成数据，非正式语料）
fuel_A = 100; dSOC_A = 0.05;      % 臂 A：烧 100 kg，耗 5% SOC
fuel_B = 102; dSOC_B = 0.03;      % 臂 B：烧 102 kg，耗 3% SOC
jfec = @(f, dsoc, k) f + dsoc * Bat_Cap * k;
% break-even k_eq：使 jfec_A == jfec_B 的 k_eq
k_be = (fuel_B - fuel_A) / ((dSOC_A - dSOC_B) * Bat_Cap);

r = struct('schema', 'ppe_fec_robustness_prereg_v1', 'purpose', 'QUALIFICATION (pre-registration)', ...
    'k_eq_canonical', k_eq, 'k_eq_sensitivity', {struct('lo_0_8x', k_lo, 'hi_1_2x', k_hi)}, ...
    'k_eq_break_even_synth', round(k_be, 4), ...
    'note', 'break-even 仅诊断呈现；合成数据演示；正式 break-even 属 T-011B（未授权）', ...
    'pass', isfinite(k_be) && k_be > 0, ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z')));
qdir = fullfile(ppe.ppe_root(), 'results', 'ppe_practical', 'qualification');
fid = fopen(fullfile(qdir, 'fec_robustness_prereg.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[fec-robust] pass=%d k_eq=%.4f [%.4f, %.4f] break_even=%.4f\n', ...
    r.pass, k_eq, k_lo, k_hi, k_be);
end
