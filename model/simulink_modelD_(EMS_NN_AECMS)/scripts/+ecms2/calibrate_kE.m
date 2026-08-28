function res = calibrate_kE(~)
% ecms2.calibrate_kE — 【已取代】law=6 v1（K_E 直接加法 λ + K_E·ê）标定入口
%
%  design §3.2 原始形式（v1）已于 S20 标定证伪：K_E>0 全部导致 fuel_corr 劣于 law=5
%  （透支电池，ê<0 支路压过 λ 悬崖使 P_g 塌缩到怠速、电池全载放电），结果见
%  results/ecms_phase1/kE_calibration.json（RQ3 记录）。
%  主 Agent 已批准 design v1.1 修订（方案 B）：law=6 改走 SOC_ref 通道
%  （SOC_ref(t)=0.60+K_S·ê，λ_eff 下限钳位 0.43）——v2 标定请运行：
%      res = ecms2.calibrate_kS();
%  本函数保留为 v1 历史代码（不删除），运行即中止并指向 v2。
    error('ECMS2:CALKE:SUPERSEDED', [ ...
        'law=6 v1（additive λ）已被 design v1.1 修订取代（SOC_ref 通道）。' ...
        'v1 标定记录见 results/ecms_phase1/kE_calibration.json；v2 标定请运行 ecms2.calibrate_kS()']);
end
