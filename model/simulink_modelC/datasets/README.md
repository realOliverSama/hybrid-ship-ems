# 数据集划分说明

- 按**完整航次**划分（禁止先做重叠滑动窗口再随机划分）。
- 每个航次 1 s 采样，字段见 CSV 列名；`time_s` 为航次内时间，`scenario_id` 为工况号，`ems_mode` 为 EMS 模式。
- `P_load_future_max_60s`：当前时刻后 60 s 内的总负荷峰值 [kW]（负荷预测监督目标，末端不足 60 s 填 NaN）。
- train: S01_still_water, S01_still_water, S02_downstream, S02_downstream, S04_resistance_inc, S04_resistance_inc, S07_bridge_maneuver, S07_bridge_maneuver, S08_shoal_series, S08_shoal_series
- validation: S03_upstream, S03_upstream, S05_resistance_dec, S05_resistance_dec
- test: S06_resistance_step, S06_resistance_step, S09_emergency_sprint, S09_emergency_sprint
- 注意：当前 AI 预测源为 persistence，Baseline 与 AI 模式数据一致；接入真实预测后需重新生成。
