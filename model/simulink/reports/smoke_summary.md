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

**总体结论: 全部通过**
