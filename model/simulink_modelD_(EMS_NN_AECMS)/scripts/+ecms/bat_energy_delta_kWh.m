function dE_kWh = bat_energy_delta_kWh(Pbat_dc, eta_dcdc, eta_bat, dt_s)
% ecms.bat_energy_delta_kWh — 电池存储能量单步变化（design §4.1 符号约定，施工前写死）
%
%  符号约定：P_bat_dc > 0 = 放电（电池向母排输出），< 0 = 充电（母排向电池输入）。
%  放电（P_bat_dc > 0）：E −= P_bat_dc / (η_dcdc × η_bat) × Δt/3600   [kWh]
%  充电（P_bat_dc < 0）：E += (−P_bat_dc) × η_dcdc × η_bat × Δt/3600   [kWh]
%
%  输出 dE_kWh：电池存储能量变化量（正 = 存储增加）。实现自检语义（单测锁死）：
%    P_bat_dc = 0  -> dE = 0
%    P_bat_dc > 0  -> dE < 0（放电必减）
%    P_bat_dc < 0  -> dE > 0（充电必增）
%  输入: Pbat_dc 母线侧电池功率 [kW]（标量或向量）；eta_dcdc/eta_bat 效率 [-]；
%        dt_s 步长 [s]。
%  输出: dE_kWh 存储能量变化 [kWh]（与输入同形状）。
    eta_rt = eta_dcdc * eta_bat;          % 充放共用 η_dcdc·η_bat（design §4.1）
    h = dt_s / 3600;                      % 秒 -> 小时
    dE_kWh = zeros(size(Pbat_dc));
    iDis = Pbat_dc > 0;                   % 放电：存储能量按 1/(η_dcdc·η_bat) 折算减少
    dE_kWh(iDis) = -Pbat_dc(iDis) ./ eta_rt * h;
    iChg = Pbat_dc < 0;                   % 充电：存储能量按 η_dcdc·η_bat 折算增加
    dE_kWh(iChg) = (-Pbat_dc(iChg)) .* eta_rt * h;
end
