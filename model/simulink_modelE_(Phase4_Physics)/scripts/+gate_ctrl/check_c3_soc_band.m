function r = check_c3_soc_band(ds, ctx)
% gate_ctrl.check_c3_soc_band — 判据 3：SOC 行为 Gate（FR-455 判据 3）
%   P2 臂 S20：|SOC_end − 0.60| ≤ 0.05（"未长期掏空电池"行为 Gate，非燃油公平比较带；
%   终端 SOC 公平性留 F 包）。ctx 为 P2/S20 时启用；其余臂/工况仅记录不判。
    r.name = 'C3_soc_band';
    r.label = ctx;
    r.pass = true;
    r.checks = {};
    if strcmp(ctx, 'P2/S20')
        dev = abs(ds.SOC(end) - 0.60);
        ok = dev <= 0.05;
        r.checks{end+1} = struct('item', 'P2_S20_SOC_end_within_0.05_of_0.60', ...
            'pass', ok, 'SOC_end', ds.SOC(end), 'deviation', dev); %#ok<SAGROW>
        r.pass = ok;
    end
end