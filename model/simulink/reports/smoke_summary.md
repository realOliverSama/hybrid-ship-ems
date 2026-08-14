# 烟雾测试汇总 (smoke_summary.md)

| 测试 | 检查项 | 结果 | 说明 |
|---|---|---|---|
| T01_still_water | no_nan_inf | PASS | 非有限信号:  |
| T01_still_water | speed_bounded | PASS | max|v|=5.73 < 10.49 |
| T01_still_water | soc_range | PASS | SOC=[0.598, 0.600] |
| T01_still_water | gen_bounds | PASS | P_gen=[0.0, 800.5] |
| T01_still_water | bat_bounds | PASS | P_bat=[-15.5, 740.2] |
| T01_still_water | voyage_reached | PASS | x_end=1200.0 / 1200 m |
| T01_still_water | stopped_early | PASS | t_end=256.5 s < 3600 s |
| T01_still_water | power_balance | PASS | max|e_balance|/P_load=0.0000 (<1%) |
| T01_still_water | limit_invariant | PASS | max(P_prop_actual-P_prop_limit)=-0.00 kW |
| T01_still_water | start_from_rest | PASS | v(0)=0.0000, x(0)=0.0000 |
| T01_still_water | cruise_reached | PASS | v_ss=5.669 / 目标5.660 m/s (功率受限可达 5.93 m/s) |
| T01_still_water | v_ripple_max | PASS | v_ripple=0.0281 <= 0.100 m/s |
| T01_still_water | fuel_kg_regress | PASS | fuel_kg: 22.54 vs 基线 22.54（偏差 0.00%，容差 5%） |
| T01_still_water | mean_load_kW_regress | PASS | mean_load_kW: 761.8 vs 基线 761.8（偏差 0.00%，容差 5%） |
| T01_still_water | v_ss_regress | PASS | v_ss: 5.669 vs 基线 5.669（偏差 0.00%，容差 2%） |
| T01_still_water | E_gen_kWh_regress | PASS | E_gen_kWh: 51.72 vs 基线 51.72（偏差 0.00%，容差 5%） |
| T01_still_water | t_end_regress | PASS | t_end: 256.5 vs 基线 256.5（偏差 0.00%，容差 5%） |
| T01_still_water | soc_end_regress | PASS | soc_end: 0.5977 vs 基线 0.5977（|Δ|=0.0000，容差 0.020） |
| T01_still_water | soc_min_regress | PASS | soc_min: 0.5976 vs 基线 0.5976（|Δ|=0.0000，容差 0.020） |
| T02_downstream | no_nan_inf | PASS | 非有限信号:  |
| T02_downstream | speed_bounded | PASS | max|v|=5.97 < 10.49 |
| T02_downstream | soc_range | PASS | SOC=[0.598, 0.600] |
| T02_downstream | gen_bounds | PASS | P_gen=[0.0, 800.5] |
| T02_downstream | bat_bounds | PASS | P_bat=[-66.9, 740.2] |
| T02_downstream | voyage_reached | PASS | x_end=1200.0 / 1200 m |
| T02_downstream | stopped_early | PASS | t_end=247.5 s < 3600 s |
| T02_downstream | power_balance | PASS | max|e_balance|/P_load=0.0000 (<1%) |
| T02_downstream | limit_invariant | PASS | max(P_prop_actual-P_prop_limit)=-0.00 kW |
| T02_downstream | start_from_rest | PASS | v(0)=0.0000, x(0)=0.0000 |
| T02_downstream | cruise_reached | PASS | v_ss=5.663 / 目标5.660 m/s (功率受限可达 6.93 m/s) |
| T02_downstream | downstream_lower_load | PASS | T02=585.4 kW vs T03=798.8 kW |
| T02_downstream | v_ripple_max | PASS | v_ripple=0.0839 <= 0.100 m/s |
| T02_downstream | fuel_kg_regress | PASS | fuel_kg: 17.26 vs 基线 17.26（偏差 0.00%，容差 5%） |
| T02_downstream | mean_load_kW_regress | PASS | mean_load_kW: 585.4 vs 基线 585.4（偏差 0.00%，容差 5%） |
| T02_downstream | v_ss_regress | PASS | v_ss: 5.663 vs 基线 5.663（偏差 0.00%，容差 2%） |
| T02_downstream | E_gen_kWh_regress | PASS | E_gen_kWh: 38.07 vs 基线 38.07（偏差 0.00%，容差 5%） |
| T02_downstream | t_end_regress | PASS | t_end: 247.5 vs 基线 247.5（偏差 0.00%，容差 5%） |
| T02_downstream | soc_end_regress | PASS | soc_end: 0.5980 vs 基线 0.5980（|Δ|=0.0000，容差 0.020） |
| T02_downstream | soc_min_regress | PASS | soc_min: 0.5976 vs 基线 0.5976（|Δ|=0.0000，容差 0.020） |
| T03_upstream | no_nan_inf | PASS | 非有限信号:  |
| T03_upstream | speed_bounded | PASS | max|v|=4.93 < 10.49 |
| T03_upstream | soc_range | PASS | SOC=[0.598, 0.600] |
| T03_upstream | gen_bounds | PASS | P_gen=[0.0, 800.5] |
| T03_upstream | bat_bounds | PASS | P_bat=[-0.5, 740.2] |
| T03_upstream | voyage_reached | PASS | x_end=1200.0 / 1200 m |
| T03_upstream | stopped_early | PASS | t_end=291.2 s < 3600 s |
| T03_upstream | power_balance | PASS | max|e_balance|/P_load=0.0000 (<1%) |
| T03_upstream | limit_invariant | PASS | max(P_prop_actual-P_prop_limit)=-0.00 kW |
| T03_upstream | start_from_rest | PASS | v(0)=0.0000, x(0)=0.0000 |
| T03_upstream | cruise_reached | PASS | v_ss=4.924 / 目标5.660 m/s (功率受限可达 4.93 m/s) |
| T03_upstream | downstream_lower_load | PASS | T02=585.4 kW vs T03=798.8 kW |
| T03_upstream | v_ripple_max | PASS | v_ripple=0.0379 <= 0.100 m/s |
| T03_upstream | fuel_kg_regress | PASS | fuel_kg: 26.76 vs 基线 26.76（偏差 0.00%，容差 5%） |
| T03_upstream | mean_load_kW_regress | PASS | mean_load_kW: 798.8 vs 基线 798.8（偏差 0.00%，容差 5%） |
| T03_upstream | v_ss_regress | PASS | v_ss: 4.924 vs 基线 4.924（偏差 0.00%，容差 2%） |
| T03_upstream | E_gen_kWh_regress | PASS | E_gen_kWh: 61.94 vs 基线 61.94（偏差 0.00%，容差 5%） |
| T03_upstream | t_end_regress | PASS | t_end: 291.2 vs 基线 291.2（偏差 0.00%，容差 5%） |
| T03_upstream | soc_end_regress | PASS | soc_end: 0.5976 vs 基线 0.5976（|Δ|=0.0000，容差 0.020） |
| T03_upstream | soc_min_regress | PASS | soc_min: 0.5976 vs 基线 0.5976（|Δ|=0.0000，容差 0.020） |
| T04_res_inc | no_nan_inf | PASS | 非有限信号:  |
| T04_res_inc | speed_bounded | PASS | max|v|=5.73 < 10.49 |
| T04_res_inc | soc_range | PASS | SOC=[0.598, 0.600] |
| T04_res_inc | gen_bounds | PASS | P_gen=[0.0, 800.5] |
| T04_res_inc | bat_bounds | PASS | P_bat=[-15.5, 740.2] |
| T04_res_inc | voyage_reached | PASS | x_end=2200.0 / 2200 m |
| T04_res_inc | stopped_early | PASS | t_end=452.0 s < 3600 s |
| T04_res_inc | power_balance | PASS | max|e_balance|/P_load=0.0000 (<1%) |
| T04_res_inc | limit_invariant | PASS | max(P_prop_actual-P_prop_limit)=-0.00 kW |
| T04_res_inc | start_from_rest | PASS | v(0)=0.0000, x(0)=0.0000 |
| T04_res_inc | cruise_reached | PASS | v_ss=5.196 / 目标5.660 m/s (功率受限可达 5.30 m/s) |
| T04_res_inc | res_demand_rise | PASS | P_demand 前=611.7 kW 后=697.4 kW |
| T04_res_inc | v_ripple_max | PASS | v_ripple=0.0373 <= 0.100 m/s |
| T04_res_inc | fuel_kg_regress | PASS | fuel_kg: 41.74 vs 基线 41.74（偏差 0.00%，容差 5%） |
| T04_res_inc | mean_load_kW_regress | PASS | mean_load_kW: 790.5 vs 基线 790.5（偏差 0.00%，容差 5%） |
| T04_res_inc | v_ss_regress | PASS | v_ss: 5.196 vs 基线 5.196（偏差 0.00%，容差 2%） |
| T04_res_inc | E_gen_kWh_regress | PASS | E_gen_kWh: 96.59 vs 基线 96.59（偏差 0.00%，容差 5%） |
| T04_res_inc | t_end_regress | PASS | t_end: 452 vs 基线 452（偏差 0.00%，容差 5%） |
| T04_res_inc | soc_end_regress | PASS | soc_end: 0.5977 vs 基线 0.5977（|Δ|=0.0000，容差 0.020） |
| T04_res_inc | soc_min_regress | PASS | soc_min: 0.5976 vs 基线 0.5976（|Δ|=0.0000，容差 0.020） |
| T05_res_dec | no_nan_inf | PASS | 非有限信号:  |
| T05_res_dec | speed_bounded | PASS | max|v|=5.83 < 10.49 |
| T05_res_dec | soc_range | PASS | SOC=[0.598, 0.600] |
| T05_res_dec | gen_bounds | PASS | P_gen=[0.0, 800.5] |
| T05_res_dec | bat_bounds | PASS | P_bat=[-33.6, 740.2] |
| T05_res_dec | voyage_reached | PASS | x_end=2600.0 / 2600 m |
| T05_res_dec | stopped_early | PASS | t_end=502.1 s < 3600 s |
| T05_res_dec | power_balance | PASS | max|e_balance|/P_load=0.0000 (<1%) |
| T05_res_dec | limit_invariant | PASS | max(P_prop_actual-P_prop_limit)=-0.00 kW |
| T05_res_dec | start_from_rest | PASS | v(0)=0.0000, x(0)=0.0000 |
| T05_res_dec | cruise_reached | PASS | v_ss=5.660 / 目标5.660 m/s (功率受限可达 6.68 m/s) |
| T05_res_dec | v_ripple_max | PASS | v_ripple=0.0556 <= 0.100 m/s |
| T05_res_dec | fuel_kg_regress | PASS | fuel_kg: 39.26 vs 基线 39.26（偏差 0.00%，容差 5%） |
| T05_res_dec | mean_load_kW_regress | PASS | mean_load_kW: 647.8 vs 基线 647.8（偏差 0.00%，容差 5%） |
| T05_res_dec | v_ss_regress | PASS | v_ss: 5.66 vs 基线 5.66（偏差 0.00%，容差 2%） |
| T05_res_dec | E_gen_kWh_regress | PASS | E_gen_kWh: 88.09 vs 基线 88.09（偏差 0.00%，容差 5%） |
| T05_res_dec | t_end_regress | PASS | t_end: 502.1 vs 基线 502.1（偏差 0.00%，容差 5%） |
| T05_res_dec | soc_end_regress | PASS | soc_end: 0.5980 vs 基线 0.5980（|Δ|=0.0000，容差 0.020） |
| T05_res_dec | soc_min_regress | PASS | soc_min: 0.5976 vs 基线 0.5976（|Δ|=0.0000，容差 0.020） |

**总体结论: 全部通过**
