function lam = law6_lambda_eff(lambda0, K_soc, SOC_ref, K_S, SOC, e_hat, lam_floor)
% t445.law6_lambda_eff — 冻结 law=6 λ 处理逐位复制品（仅 floor，无上限）
% 与 CODE_ECMS_ORACLE / ecms2.ecms_law 同一公式；不修改 .slx 或变体核心。
if nargin < 7 || isempty(lam_floor), lam_floor = 0.43; end
soc_ref_eff = SOC_ref + K_S * e_hat;
lam = lambda0 + K_soc * (soc_ref_eff - SOC);
if K_S ~= 0
    lam = max(lam, lam_floor);
end
end
