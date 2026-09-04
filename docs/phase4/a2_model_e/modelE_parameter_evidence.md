# Model E v1 参数证据链（Phase 4-A2）

> spec：`.specs/phase4-a2-model-e-construction/`（brainstorm/requirements/design/tasks 四门禁，2026-09-04 用户批准）
> 参数事实源：`model/simulink_modelE_(Phase4_Physics)/config/ModelE_Physics_Parameters.m`（本文件是其证据链展开）
> 裁决记录：`项目开发进展.md` 2026-09-04 条目（Q1~Q5 + C_B 追加裁决 + 17 项参考文献）
> 文献编号 [n] 对应该条目的参考数据源清单。生成：T-013（施工归档）。

## 0. 诚实标记总纲（NFR-410）

- 500 t representative hull：`target_vessel_confirmed = false`（工作假定，data_request_list 挂账）；
- 全部 Literature-Surrogate 参数不标实测；推导值附推导式；
- 真实资料到位 → 走 spec 修订逐参数替换，不溯及已批准结论。

## 1. 船体几何与质量（FR-412）

| 参数 | 值 | provenance | 推导/出处 |
|---|---|---|---|
| Hull_Lpp_m | 53.45 m | DERIVED+LITERATURE_NOMINAL | Rhine Ship 86 [2]（Lpp=85.522/B=11.400/T=3.500/C_B=0.860）保持 L/B=7.4965、B/T 比例，按 Δ=ρ·L·B·T·C_B=500 t 守恒缩尺 λ=0.6250 |
| Hull_B_m | 7.13 m | 同上 | 11.400×0.6250 |
| Hull_T_m | 2.188 m | 同上 | 3.500×0.6250 |
| Hull_Sw_m2 | 554.2 m² | 同上 | 缩尺换算（S∝λ²·...按相似缩尺） |
| Hull_CB | 0.60 | LITERATURE_NOMINAL | 三腿佐证：Alexander 临界公式（巡航 Fn=0.247→临界 0.665）、Troost 最优回归（0.63/0.53 之间）、CCS《海上公务船船体结构指南》2021（Cb=0.6 公务船上界点）[CB-1][CB-2] |
| Ship_Mass | 5.0e5 kg | DERIVED | 500 t 工作假定（2026-08-25 用户裁决 + 2026-09-04 Q3 裁决：1.4 kW/t 公务船带内） |
| rho_water / nu_water | 1000 kg/m³ / 1.19e-6 m²/s | INHERITED/LITERATURE_NOMINAL | 内河淡水 15°C 近似 |

## 2. 阻力与浅水（FR-412/413/415）

| 参数 | 值 | provenance | 推导/出处 |
|---|---|---|---|
| Hull_K_R | 890 N/(m/s)² | DERIVED（用户裁决 Q4） | 1780×(500/1000) 线性 Δ 缩放；拒绝 Δ^(2/3)≈1121（Fn≈0.33 纯摩擦相似低估阻力）。K_R 为集总标定系数（设计点反演），非 build-up 物理量 |
| Hull_k_form | 0.2（1+k=1.2） | LITERATURE_RANGE 1.15~1.30 取中 | 船体形状因子惯例带 |
| Shallow_deltak_coef/exp | 0.644 / −1.72 | LITERATURE_NOMINAL | Δk=0.644·(H/T)^(−1.72)，Millward 1989 [4]（Zhang 2023 [1] 亦采用） |
| Shallow_c1/c2/c3 | 1.1680 / −0.5238 / −1.472 | LITERATURE_NOMINAL | Zhang 2023 [1] × Zeng 2019 [3]（Rhine86 标定族） |
| C_F / C_F* | ITTC-57 / Zhang 修正 | LITERATURE_NOMINAL | C_F=0.075/(log10Re−2)²；C_F*=0.08468/(log10Re−1.631)²·[1+c1/(log10Re+c2)·(H/T)^c3] |
| 域限 | H/T>3 → Δr=0；H/T<1.2 → clamp+警告 | design §4.1/FR-415 | 禁止静默外推（NFR-411） |

**已知局限（登记）**：c1/c2/c3 标定族 C_B=0.86（Rhine86 丰满货船）与本船 0.60 存在型差外推；未含浅水兴波临界区（Fr_h→1）分量——域限 + C 包 UKC 限速规避。

## 3. 伴流 w(H/T)（OQ-4/D-3）

| 参数 | 值 | provenance | 推导/出处 |
|---|---|---|---|
| Wake_w_deep | 0.10 | LITERATURE_NOMINAL（回归式） | Molland/Turnock/Hudson 双桨回归 w_T=(0.71−2.39C_B+2.33C_B²)+0.12C_B⁴(6.5−L/B)，代入 C_B=0.60、L/B=7.4965 → w_T≈0.099 → 冻结 0.10 |
| Zhang Δw 表 | {2.7:0.22, 2.0:0.27, 1.8:0.23, 1.6:0.20, 1.4:0.26, 1.2:0.32} | LITERATURE_NOMINAL（transferred surrogate） | Zhang 2023 [1] 双桨表；取相对变化量 Δw(H/T)=w_Zhang(H/T)−w_Zhang(2.7)；linear 插值；域 clamp [1.2, 2.7]；标定族 C_B=0.86 型差登记 |
| t（推力减额） | 不接线 | D-2 | 设计点锚定链已隐含推进损失，独立乘子会 double-count；Zhang t 表登记 LITERATURE_REFERENCE 备查 |

## 4. 推进链与螺旋桨重标定（FR-416/417，D-8）

### 4.1 效率链边界（审计）

**Elec_Eff = 0.92 = η_inv(0.98) × η_motor(0.96) × η_shaft(0.98) ≈ 0.9219，不含齿轮箱**（[5] LI 2026 甲醇串联混动港作船）。每因子在推进能量路径上恰好出现一次：

| 因子 | 值 | 出现位置 | 重复计损检查 |
|---|---|---|---|
| η_inv | 0.98 | Elec_Eff 乘子（电功率→机械功率） | 无其他出现 |
| η_motor | 0.96 | 同上（合并为 0.92 冻结值） | 无 |
| η_shaft | 0.98 | 同上 | 无 |
| η_gear (Gear_Eff) | 0.96（D-1 保持） | 转矩链 Gain（Gear_Ratio·Gear_Eff） | **不在** Elec_Eff 内——无重复 |
| η_prop | 0.60 | P_effective_design（设计点锚定用） | 仅标定链用，运行时不乘 |

### 4.2 螺旋桨四 K 重标定（w_deep=0.10，D-8）

推导链（设计点 11 kn 巡航桨设计点保留；**禁止沿用 Model D w=0.15 推导值**）：

```text
V_design   = 11/1.944            = 5.6584 m/s
P_shaft,nom = 700×0.92×0.96      = 618.24 kW
P_shaft,des = 618.24/1.15        = 537.60 kW（15% 海况裕量）
P_eff,des   = 0.60×537.60        = 322.56 kW
F_eff,des   = 322560/5.6584      = 56,999.9 N
n_prop      = 1500/6.08          = 246.7105 rpm → ω = 25.8287 rad/s
Q_prop,nom  = 537600/25.8287     = 20,809.5 N·m
V_A,des     = (1−0.10)×5.6584    = 5.0926 m/s   ← 与 Model D (w=0.15→4.8097) 的差异根
F_bollard   = 1.5×56999.9        = 85,499.9 N
Q_bollard   = 1.2×20809.5        = 24,971.4 N·m
K_Fw = 85499.9/25.8287²          = 128.107 N/(rad/s)²
K_Fv = (85499.9−56999.9)/5.0926² = 1099.02 N/(m/s)²
K_Qw = 24971.4/25.8287²          = 37.410 N·m/(rad/s)²
K_Qv = (24971.4−20809.5)/5.0926² = 160.47 N·m/(m/s)
```

自洽校验：设计点 F = K_Fw·ω²−K_Fv·V_A² = 57011 N ≈ F_eff,design（56,999.9 N，偏差 <0.02%）✓

螺旋桨 D、K_T/K_Q、P/D：**INTENTIONALLY UNRESOLVED**（A1 审计 keep_simplified；资料到位后走 spec 修订）。±20% OAT 敏感性（K_Fw/K_Fv/K_Qw/K_Qv/w_deep 五参数）见 prop_sensitivity_report.md。

## 5. 双发电机组（FR-418/419/420）

| 参数 | 值 | provenance | 出处 |
|---|---|---|---|
| DG_P_rated / n_rated / n_idle | 500 kW / 1500 rpm / 600 rpm | VENDOR | WH17ME 6WH17C680ME5 [16]；机组型谱 [17] |
| DG_P_min | 125 kW/unit（25%） | LITERATURE_NOMINAL | [6] Kim 2021 G_min=25%（A2 裁决替换 legacy 的 0） |
| DG_P_opt | 375 kW/unit（75%） | LITERATURE_NOMINAL | [6] G_opt=75% |
| DG_ramp | 20 kW/s/unit | engineering-calibrated | [9] Opila 2018 7 MW 机组 1.67% rated/s；本机组 4% rated/s 带内；冻结档 {10,20,50} |
| DG_tau | 5 s | engineering-calibrated | 无机型精确数据（顾问裁决 literature-plausible）；冻结档 {2,5,10} |
| t_starting / t_sync | 25 s / 5 s | LITERATURE_NOMINAL | [7] Perabo 2020（genset start-to-online ≈28~30 s 拆分）+ [8] Jeon 2019（同步 3~5 s） |
| t_cooldown | 30 s | surrogate | [7] 量级；**登记局限：COOLDOWN 怠速油耗未计** |
| C_start | 0.6 kg/start | DERIVED | 125 kW×0.6 kg/kWh×(30/3600 h)=0.625→冻结 0.6；event-level 均摊 STARTING+SYNC（只算一次，禁止双算）；冻结档 {0.5,1,2} |
| BSFC PCHIP | {125:600, 250:547, 375:447, 400:427, 500:453} g/kWh | LITERATURE_NOMINAL/DERIVED | [11] Fridell 2021 实测形状（620/565/441 @25/50/80%）× [16] WH17ME 427 g/kWh 锚定归一（k=427/441=0.9683）；100% 点 453 沿用项目 rated anchor；域守卫：ON 且 P≥125 才查表，禁止向 0 kW 外推 |

## 6. 电池 LFP 1RC ECM + SOP（FR-422/423/424）

**chemistry 声明**：LFP 为 literature surrogate（实船化学体系未确认）。cell→pack 缩放：K2 LFP26650P（2.6 Ah/3.2 V，[12] Tran 2021 HPPC 拟合 + [13] Pérez Vidal 2011 OCV）按 N_s=576/3.2=180、N_p=1860/2.6=715.38 连续等效缩放：R_pack=R_cell·N_s/N_p、C_pack=C_cell·N_p/N_s（推导值，非文献原值）。

### 6.1 九节点 SOC-LUT（pack 级 DERIVED；域 [0.1, 0.9]，域外 clamp+flag）

| SOC | OCV [V] | R_0 [mΩ] | R_1 [mΩ] | C_1 [F] |
|------|--------|----------|----------|---------|
| 0.1 | 541.08 | 8.66 | 18.87 | 1697 |
| 0.2 | 578.68 | 8.15 | 12.25 | 2070 |
| 0.3 | 585.99 | 7.70 | 9.86 | 2254 |
| 0.3~0.9 全表见参数文件 Bat_OCV_LUT_V/Bat_R0_LUT_ohm/Bat_R1_LUT_ohm/Bat_C1_LUT_F | | | | |

（完整九节点以 `ModelE_Physics_Parameters.m` 为准；本表为首尾节点示例。）

### 6.2 电气口径（Q5/OQ-9 裁决）

| 项 | 值 | provenance |
|---|---|---|
| Bat_Q_nom_Ah | 1860 Ah（≈1071.36 kWh @576 V） | DERIVED（930 Ah×2，与 DRAWING 一致） |
| Bat_eta_coul | 0.98（仅进 SOC 计量） | LITERATURE_NOMINAL [14] |
| V_min/V_max | 360 / 657 V | LITERATURE_NOMINAL（cell 2.00/3.65 V×180s [12]） |
| R_0 充电增益 | **1.0（充放电共用 R_0(SOC)）** | **ENGINEERING_SURROGATE（T-013 裁决 2026-09-04）**：charging gain ×1.15 无具体文献/实验支撑（顾问表 v1 仅表注一行，Tran 2021 HPPC [12] 未单列充电方向拟合），暂不人为引入；Bat_R0_chg_mult=1.0 保留定义，待真实 BMS 数据重评 |
| P_converter | 1050 kW **bus-side** | DRAWING（20260526 设备参数；OQ-9 冻结） |
| 端子换算 | 放电 1082.5 / 充电 1018.5 kW | DERIVED（方向感知 ÷η、×η） |
| I_bat_limit_surrogate_A | 1823 A | DERIVED（=1050 kW/576 V，**非独立 BMS 电流额定**） |
| SOC 域/降额 | [0.10, 0.95]；DisDerate 0.25→0.10 归零；ChgDerate 0.90→0.95 归零 | **ENGINEERING_SURROGATE（T-013 裁决）**：保守 BMS/SOP surrogate safety envelope（沿用 Model D 降额族数值，Model E 独立理由=电池绝对边界+DC/DC 约束的保守运行包络；待真实 BMS 策略数据重评） |
| 旧 η_bat=0.95 | Plant 侧停用（ECMS k_eq 引用保留） | Q5 裁决（防 double-count） |
| 1RC 求解 | I = 2P_t/(E+√(E²−4R_0P_t))，D<0 → P_actual=P_max,R=E²/(4R_0)+双 flag | OQ-9（无迭代、无代数环、HIL-friendly） |

## 7. 已知局限汇总（论文 Limitations 素材）

1. 500 t/几何为 representative hull（未确认实船）；
2. 浅水 c1/c2/c3 与 Δw 标定族 C_B=0.86 型差外推；未含 Fr_h→1 兴波临界分量；
3. 螺旋桨 keep_simplified（无 D/K_T/K_Q 图谱）；结论稳健域以 ±20% OAT 报告界定；
4. BSFC 375 kW 点为归一插值点（非独立实测）；COOLDOWN 与 ON 态爬坡过渡段怠速油耗未计；
5. 电池 LFP surrogate 化学体系；SOP 的电流/电压约束用开路估计；
6. PLACEHOLDER 机组组合逻辑不得用于经济性结论（FR-421）。

## 8. 敏感性与 Gate 结果引用（2026-09-04 夜班施工）

- **螺旋桨 ±20% OAT**（T-011，`prop_sensitivity_report.md` / `results/prop_sensitivity.json`）：
  15 cases 零失败。船速环锁 11 kn 下速度不敏感（恒 11.01 kn）；功率/油耗敏感度排序：
  **K_Fw（P −29%/+49%）≫ K_Qw（±27%）> K_Fv（±20%）≫ K_Qv（±7%）> w_deep（±2.6%）**。
  结论稳健域：速度-功率曲线形状对 K_Fw/K_Qw 高度敏感（keep_simplified 的核心不确定性），
  油耗方向性结论在 ±20% 全域内保持（幅度见报告表）——稳健域正式陈述待用户评审定稿。
- **T-412 整体物理 Gate**（T-012，`results/gate_result.json`）：**overall=true**（2026-09-04）。
  G1 结构/fixed-step/零 NaN/Model D sha256 零改动 ✓；G2 方向性+浅水六档单调零反例+功率带
  （298.4 kW∈[274,335]）✓；G3 机组时序 25/5/30 s 精确+C_start=0.600±0.01 kg+燃油可区分
  （23.7%>1%）✓；G4 电池方向性+SOP 包络+无双算 ✓；G5 能量闭合（<0.1%）✓；G6 确定性
  （两跑逐位一致）✓；负向验证（Hull_K_R 篡改→FAIL、LUT 运行时篡改→manifest 检测发现）✓。
  **基线冻结待用户评审**（design §5.1）。

## 9. 变更记录

| 日期 | 变更 | 依据 |
|---|---|---|
| 2026-09-04 | v1 初稿（参数表+效率链审计+重标定推导） | A2 spec 四门禁 + T-002/T-004/T-005 施工 |
| 2026-09-04 | 敏感性报告链接 + Gate overall=true 结果引用 | T-011/T-012 施工完成 |

## 9. 附加引用（T-013 裁决并入正式参考文献）

- **[18]** 中国船级社. 海上公务船船体结构指南[S]. 北京: 中国船级社, 2021. https://www.ccs.org.cn/ccswz/file/download?fileid=202311090263358506 （原 [CB-2]；C_B=0.60 公务船丰满度上界佐证）
- **[19]** SCHNEEKLUTH H, BERTRAM V. Ship Design for Efficiency and Economy[M]. 2nd ed. Oxford: Butterworth-Heinemann, 1998.（原 [CB-1]；Alexander 临界方形系数公式与 Troost 最优回归收录）
- 原任务书 [CB-3]（快速巡逻艇 Cb 统计旁证）未单独并表，作为 [19] 的船型统计旁证记录于开发日志 2026-09-04 条目。
- `Hull_k_form = 0.2`（1+k=1.2）：**ENGINEERING_SURROGATE**（T-013 裁决降级——1+k∈1.15~1.30 为行业工程带，无单篇文献直接锚定本船型；待 Rhine86 报告/ITTC 惯例类资料确认）。
