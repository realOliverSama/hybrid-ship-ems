function r = check_c4_vdc(ds, ctx, isInjection)
% gate_ctrl.check_c4_vdc — 判据 4：Vdc 分层（FR-455 判据 4）
%   正常场景：全程 Vdc >= 600 V（硬 Gate，CCS 下限）；
%   注入场景：逐场景断言由 C6 承担（expected override 触发 / flag 一致 / 无 NaN 失控），
%   本函数仅做 no-NaN 基线 + 越限记录（越限须被正确记录 = 供 T-010 场景特定断言使用）。
    r.name = 'C4_vdc';
    r.label = ctx;
    r.checks = {};
    ok = true;
    nan_ok = all(isfinite(ds.V_dc_V));
    r.checks{end+1} = struct('item', 'Vdc_all_finite', 'pass', nan_ok, ...
        'Vdc_min', min(ds.V_dc_V)); %#ok<SAGROW>
    if ~nan_ok, ok = false; end
    if ~isInjection
        vdc_ok = min(ds.V_dc_V) >= 600;
        r.checks{end+1} = struct('item', 'Vdc_min_ge_600_normal_hard', ...
            'pass', vdc_ok, 'Vdc_min', min(ds.V_dc_V)); %#ok<SAGROW>
        ok = ok && vdc_ok;
    else
        r.checks{end+1} = struct('item', 'Vdc_violation_recorded_for_injection_scenario', ...
            'pass', true, 'Vdc_min', min(ds.V_dc_V), 'viol_s', sum(diff([ds.t; ds.t(end)]) .* (ds.V_dc_V < 600))); %#ok<SAGROW>
    end
    r.pass = ok;
end