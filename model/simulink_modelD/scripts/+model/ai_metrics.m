% model.ai_metrics — AI 对比航次指标计算
function m = ai_metrics(ds)
dt = [diff(ds.t); 0.01];
m.fuel_kg = ds.fuel_total_kg(end);
m.E_gen_kWh = sum(ds.P_gen_actual_kW .* dt) / 3600;
m.E_bat_dis_kWh = sum(max(ds.P_bat_actual_kW, 0) .* dt) / 3600;
m.E_bat_chg_kWh = sum(max(-ds.P_bat_actual_kW, 0) .* dt) / 3600;
m.E_bat_throughput_kWh = m.E_bat_dis_kWh + m.E_bat_chg_kWh;
m.E_unserved_kWh = sum(ds.P_unserved_kW .* dt) / 3600;
m.SOC_end = ds.SOC(end);
m.SOC_min = min(ds.SOC);
m.v_rmse = sqrt(mean((ds.v_ship_mps - ds.v_cmd_mps).^2));
end
