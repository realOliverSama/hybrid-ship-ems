function r = check_g5_energy_closure(e01, e09)
% gate_e.check_g5_energy_closure — G5 能量闭合判据（design §5.3，physics_invariant）
%   |∫P_residual dt| / E_throughput < 0.1%（稳态段；电容储能变化 ΔE_cap 扣除口径）；
%   稳态 |P_residual| 均值 < 1 kW
%   P_residual 物理意义 = C·V·dV/dt（母线电容充放电功率）→ ∫P_res dt ≈ ΔE_cap = ½C(V_end²−V_init²)
r.name = 'G5_energy_closure';
r.gate_class = 'physics_invariant';
r.checks = {};

for k = 1:2
    if k == 1, s = e01; lbl = 'E01_cruise'; else, s = e09; lbl = 'E09_transient'; end
    ds = s.ds;
    dtv = diff(ds.t);
    Pres = ds.P_bus_residual_kW(1:end-1) * 1000;               % W
    int_Pres = sum(Pres .* dtv);                                % J
    Cbus = evalin('base', 'ASSUMED_C_bus');
    dEcap = 0.5 * Cbus * (ds.V_dc_V(end)^2 - ds.V_dc_V(1)^2);   % J（电容储能变化）
    E_thr = sum(max(ds.P_load_total_kW(1:end-1), 0) .* dtv) * 1000;  % J（吞吐能量）
    closure = abs(int_Pres - dEcap) / max(E_thr, 1);
    ok_closure = closure < 1e-3;                                % 0.1%
    r.checks{end+1} = struct('item', sprintf('closure_%s', lbl), 'pass', ok_closure, ...
        'ratio', closure, 'int_Pres_J', int_Pres, 'dEcap_J', dEcap, 'E_throughput_J', E_thr); %#ok<SAGROW>

    iSteady = ds.x_ship_m > 0.7 * max(ds.x_ship_m);
    mean_res = mean(abs(ds.P_bus_residual_kW(iSteady)));
    ok_steady = mean_res < 1.0;
    r.checks{end+1} = struct('item', sprintf('steady_mean_residual_lt_1kW_%s', lbl), 'pass', ok_steady, ...
        'mean_abs_residual_kW', mean_res); %#ok<SAGROW>
end

r.pass = all(cellfun(@(c) c.pass, r.checks));
end