function write_phase1_csv(csvPath, scen, exp, tau, m, ffActive, ffFirst)
% model.write_phase1_csv — Phase 1 指标 CSV 追加一行（T-007）
% 输入:
%   csvPath   CSV 文件路径（追加）
%   scen      工况名
%   exp       实验名（E0/E1/E2/E3）
%   tau       τ_margin [s]（主实验=10；扫描=5/15/20）
%   m         tff_metrics 结构
%   ffActive  前馈活动时长 [s]（E0=NaN；E1=重构；E2/E3=实测；NaN 写 'NaN'）
%   ffFirst   首次前馈激活时刻 [s]（NaN 写 'NaN'）
    fid = fopen(csvPath, 'a', 'n', 'UTF-8');
    if fid < 0, error('无法打开 %s', csvPath); end
    if isnan(m.P_ff_activated_s), pffs = 'NaN'; else, pffs = sprintf('%.2f', m.P_ff_activated_s); end
    if isnan(ffActive), fa = 'NaN'; else, fa = sprintf('%.2f', ffActive); end
    if isnan(ffFirst), ff = 'NaN'; else, ff = sprintf('%.2f', ffFirst); end
    fprintf(fid, '%s,%s,%d,%.4f,%.2f,%.2f,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.4f,%.4f,%s,%s,%s\n', ...
        scen, exp, tau, m.fuel_kg, m.Vdc_min, m.Vdc_max, m.SOC_end, m.SOC_min, ...
        m.E_bat_dis_kWh, m.P_bat_rms_kW, m.P_bat_peak_abs_kW, m.t_sat_s, ...
        m.E_unserved_kWh, m.v_rmse_mps, pffs, fa, ff);
    fclose(fid);
end
