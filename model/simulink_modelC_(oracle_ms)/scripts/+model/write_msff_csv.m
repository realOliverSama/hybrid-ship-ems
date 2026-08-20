function write_msff_csv(csvPath, meta, m, status)
% model.write_msff_csv — Phase 1 多步前馈 runs.csv 追加一行（T-006, FR-012）
% 输入:
%   csvPath  CSV 路径（不存在则写表头）
%   meta     实验元数据 struct：
%            exp_id   实验标识（如 '1A' / '1B'）
%            scenario 工况名（S04/S06/S07/S09）
%            controller 对照 {'C0','CP','CMS'}
%            anchor   锚点 {'fast','nominal','slow','-'}（参数扫描为 '-'）
%            lambda_P  λ_P [-]
%            R_gen    R_gen_max [kW/s]
%            tau_gen  τ_gen [s]
%            SOC0     初始 SOC [-]
%            horizon_s horizon [s]（C0 为 0）
%            info_mode 信息模式 {'traj','peak','-'}（C0 为 '-'）
%   m        msff_metrics 输出 struct；失败时传空 struct（NaN 行）
%   status   'ok' 或 'failed:<原因>'
% 约定: 失败运行必须落盘 NaN 行 + 失败原因，禁止静默丢弃。

    % ---- 表头 ----
    cols = {'exp_id','scenario','controller','anchor','lambda_P','R_gen', ...
            'tau_gen','SOC0','horizon_s','info_mode', ...
            'fuel_kg','Vdc_min','Vdc_max','SOC_end','SOC_min', ...
            'E_bat_dis_kWh','E_bat_throughput_kWh','P_bat_rms_kW','P_bat_peak_abs_kW', ...
            't_sat_s','E_unserved_kWh','v_rmse_mps', ...
            'N_UV','T_UV','N_stress','T_stress', ...
            'ff_active_s','ff_energy_kJ','status'};

    needHeader = ~exist(csvPath, 'file') || dir(csvPath).bytes == 0;
    fid = fopen(csvPath, 'a', 'n', 'UTF-8');
    if fid < 0, error('write_msff_csv: 无法打开 %s', csvPath); end
    if needHeader
        fprintf(fid, '%s\n', strjoin(cols, ','));
    end

    % ---- 元数据 ----
    md = struct('exp_id','','scenario','','controller','','anchor','-', ...
                'lambda_P',NaN,'R_gen',NaN,'tau_gen',NaN,'SOC0',NaN, ...
                'horizon_s',0,'info_mode','-');
    for f = fieldnames(meta)'
        md.(f{1}) = meta.(f{1});
    end

    % ---- 指标（失败/缺失字段 → NaN）----
    mf = {'fuel_kg','Vdc_min','Vdc_max','SOC_end','SOC_min', ...
          'E_bat_dis_kWh','E_bat_throughput_kWh','P_bat_rms_kW','P_bat_peak_abs_kW', ...
          't_sat_s','E_unserved_kWh','v_rmse_mps', ...
          'N_UV','T_UV','N_stress','T_stress','ff_active_s','ff_energy_kJ'};
    vals = NaN(1, numel(mf));
    if ~isempty(m) && isstruct(m)
        for i = 1:numel(mf)
            if isfield(m, mf{i}), vals(i) = m.(mf{i}); end
        end
    end

    % ---- 格式化输出 ----
    row = {md.exp_id, md.scenario, md.controller, md.anchor, ...
           num2str(md.lambda_P), num2str(md.R_gen), num2str(md.tau_gen), ...
           num2str(md.SOC0), num2str(md.horizon_s), md.info_mode};
    for i = 1:numel(vals)
        v = vals(i);
        if isnan(v), row{end+1} = 'NaN'; %#ok<AGROW>
        elseif mod(v, 1) == 0 && isfinite(v) && abs(v) < 1e9
            row{end+1} = sprintf('%.0f', v); %#ok<AGROW>
        else
            row{end+1} = sprintf('%.4f', v); %#ok<AGROW>
        end
    end
    row{end+1} = status; %#ok<AGROW>
    fprintf(fid, '%s\n', strjoin(row, ','));
    fclose(fid);
end
