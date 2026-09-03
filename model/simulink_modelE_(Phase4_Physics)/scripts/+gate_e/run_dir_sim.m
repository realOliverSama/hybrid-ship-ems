function [E, sim_rec] = run_dir_sim(mdl, cur_mps)
% gate_e.run_dir_sim — E03/E04 顺逆水方向性 runner（design §5.2）
    % 水流语义由工况表驱动（S02 顺水表 +1 / S03 逆水表 -1）；river_current_mps 仅默认值
    if cur_mps > 0
        SCN = evalin('base', 'SCENARIOS(2)');
    else
        SCN = evalin('base', 'SCENARIOS(3)');
    end
    SCN.voyage_distance_m = 2000; SCN.initial_SOC = 0.60;
    assignin('base', 'Bat_SOC_init', 0.60);
    out = model.run_scenario_sim(mdl, SCN, 0, 2500);
    ds = model.extract_logsout(out);
    iS = ds.x_ship_m > 0.8 * max(ds.x_ship_m);
    E = struct('P_prop_kW', mean(ds.P_prop_actual_kW(iS)));
    sim_rec = struct('name', sprintf('E%02d', 3 + (cur_mps < 0)), 'ds', ds);
end
