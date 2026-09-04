function r = check_c5_energy_closure(ds, ctx, C_bus)
% gate_ctrl.check_c5_energy_closure — 判据 5：能量闭合（FR-455 判据 5，G5 口径函数复用）
%   |∫P_residual dt − ΔE_cap| / E_throughput < 0.1%（P_residual = 09 号母线残差 = C·V·dV/dt）。
    r.name = 'C5_energy_closure';
    r.label = ctx;
    r.checks = {};
    dtv = diff(ds.t);
    Pres = ds.P_bus_residual_kW(1:end-1) * 1000;               % W
    int_Pres = sum(Pres .* dtv);                                % J
    dEcap = 0.5 * C_bus * (ds.V_dc_V(end)^2 - ds.V_dc_V(1)^2);  % J
    E_thr = sum(max(ds.P_load_total_kW(1:end-1), 0) .* dtv) * 1000;  % J
    closure = abs(int_Pres - dEcap) / max(E_thr, 1);
    ok = closure < 1e-3;
    r.checks{end+1} = struct('item', 'closure_lt_0.1pct', 'pass', ok, ...
        'ratio', closure, 'int_Pres_J', int_Pres, 'dEcap_J', dEcap, 'E_throughput_J', E_thr); %#ok<SAGROW>
    r.pass = ok;
end