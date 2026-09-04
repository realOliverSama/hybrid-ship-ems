# 螺旋桨参数族 ±20% OAT 敏感性报告（T-011，FR-417）

> 生成：41_run_prop_sensitivity.m（2026-09-04T04:15:32***）；工况 S01 静水深水 1500 m；nominal 档 = T-005 重标定值

| case | factor | v_steady [kn] | P_prop_elec [kW] | R_hull [N] | fuel [kg] |
|---|---|---|---|---|---|
| K_Fw_x0.8 | 0.80 | 11.008 | 444.8 | 28537.2 | 22.481 |
| K_Fw_x1.0 | 1.00 | 11.007 | 298.4 | 28532.8 | 18.073 |
| K_Fw_x1.2 | 1.20 | 11.007 | 211.8 | 28530.5 | 16.521 |
| K_Fv_x0.8 | 0.80 | 11.007 | 245.3 | 28530.8 | 17.406 |
| K_Fv_x1.0 | 1.00 | 11.007 | 298.4 | 28532.8 | 18.073 |
| K_Fv_x1.2 | 1.20 | 11.007 | 354.7 | 28534.8 | 19.863 |
| K_Qw_x0.8 | 0.80 | 11.007 | 218.8 | 28530.4 | 16.693 |
| K_Qw_x1.0 | 1.00 | 11.007 | 298.4 | 28532.8 | 18.073 |
| K_Qw_x1.2 | 1.20 | 11.008 | 378.0 | 28535.3 | 21.105 |
| K_Qv_x0.8 | 0.80 | 11.007 | 318.3 | 28533.6 | 18.666 |
| K_Qv_x1.0 | 1.00 | 11.007 | 298.4 | 28532.8 | 18.073 |
| K_Qv_x1.2 | 1.20 | 11.007 | 278.5 | 28532.0 | 17.824 |
| Wake_w_deep_x0.8 | 0.80 | 11.007 | 306.2 | 28533.1 | 18.306 |
| Wake_w_deep_x1.0 | 1.00 | 11.007 | 298.4 | 28532.8 | 18.073 |
| Wake_w_deep_x1.2 | 1.20 | 11.007 | 290.7 | 28532.6 | 17.921 |

nominal 档均值: v=11.01 kn, P=298.4 kW, fuel=18.073 kg

## 敏感性幅度（相对 nominal，factor 0.8/1.2）

- **K_Fw**: v ∈ [11.01, 11.01] kn（nominal 11.01）；P ∈ [211.8, 444.8] kW；fuel ∈ [16.521, 22.481] kg
- **K_Fv**: v ∈ [11.01, 11.01] kn（nominal 11.01）；P ∈ [245.3, 354.7] kW；fuel ∈ [17.406, 19.863] kg
- **K_Qw**: v ∈ [11.01, 11.01] kn（nominal 11.01）；P ∈ [218.8, 378.0] kW；fuel ∈ [16.693, 21.105] kg
- **K_Qv**: v ∈ [11.01, 11.01] kn（nominal 11.01）；P ∈ [278.5, 318.3] kW；fuel ∈ [17.824, 18.666] kg
- **Wake_w_deep**: v ∈ [11.01, 11.01] kn（nominal 11.01）；P ∈ [290.7, 306.2] kW；fuel ∈ [17.921, 18.306] kg

## 结论稳健域（2026-09-04 用户评审定稿）

<!-- MANUAL:conclusion_robustness -->
Within the ±20% one-at-a-time propeller-surrogate perturbation range, the model
preserved physical trend consistency and numerical stability; however, the
substantial power sensitivity to K_Fw (−29%/+49%) and K_Qw (±27%) identifies
propeller characterization as a dominant plant uncertainty. Robustness of
future EMS performance rankings must be evaluated separately.
<!-- /MANUAL:conclusion_robustness -->

**口径说明**：1×500 kW（BSFC 453）与 2×250 kW（BSFC 547）的燃油差 23.66% 为
**稳态段差**——按 S01 静水短工况（3000 m，t≈[0,1500]s）双机稳态爬坡完成后积分，
含 GensetUnitE 爬坡（20 kW/s）与一阶惯性（τ=5 s）动态过程，不含 STARTING/SYNC
启动燃油（两组工况各含一次 0.6 kg 启动均摊，对比中相消）。读者复算：稳态
fuel_rate 差 = |500×453 − 2×250×547|/3.6e6 ÷ (500×453/3.6e6) ≈ 20.8%（纯 BSFC），
Gate 实测 23.66% 含双机 droop/惯性动态分配的暂态贡献。
