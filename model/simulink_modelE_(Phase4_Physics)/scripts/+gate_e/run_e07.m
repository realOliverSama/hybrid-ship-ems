function [E, sim_rec] = run_e07(mdl, f1, f2)
% gate_e.run_e07 — E07 双机燃油区分 runner（force 注入 1×500 vs 2×250 同总负荷）
    % assignin 式注入（SiN.setVariable 对 From Workspace 非确定，见 40 号 E06 注记）
    SCN7 = evalin('base', 'SCENARIOS(1)'); SCN7.voyage_distance_m = 3000; SCN7.initial_SOC = 0.60;
    gate_e.gate_assign_bench_inputs(1, f1, f2, [0 0 0; 1e5 0 0], SCN7);
    set_param(mdl, 'StopTime', '1500');
    out = sim(mdl);
    ds = model.extract_logsout(out);
    E = struct('fuel_total_kg', ds.fuel_total_kg(end), 'fuel_1_kg', ds.fuel_1_kg(end), 'fuel_2_kg', ds.fuel_2_kg(end));
    sim_rec = struct('name', 'E07', 'ds', ds);
end
