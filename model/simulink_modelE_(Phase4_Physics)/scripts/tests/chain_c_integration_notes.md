# 链 C 集成规格说明 — T-008 电池 1RC ECM + T-009 Battery SOP（给主 Agent）

交付工人：链 C（Phase 4-A2）。本文是 `14_Battery_Converter` 重构与 builder 集成的
交接规格，依据 design.md §4.5/§4.6 + OQ-9 + FR-422/423/424。交付的纯算法资产：

| 文件 | 角色 |
|---|---|
| `+model/battery_sop_limits.m` | SOP 四约束纯函数（测试对象 + block 内联母本） |
| `+model/code_battery_ecm_e.m` | BatteryECM block 源码字符串（`setMFCode` 直接可用） |
| `tests/test_battery_sop.m` | SOP 四约束起控点注入测试（ALL PASS） |
| `tests/test_battery_ecm.m` | block 源码落盘直调测试（ALL PASS，测的代码 = block 的代码） |

## 1. `14_Battery_Converter` 重构结构

```text
V_dc ──> droop（保留：V_nom 恒值 + Sum(+-) + Gain ASSUMED_K_droop）── P_droop_kW
   ──> BatteryECM（新 MATLAB Function block，源码 = model.code_battery_ecm_e()）
        内部完成：SOP 四约束 → clamp → 端子换算 → 方向感知 R_0 → 解析求 I（OQ-9）
        输出：P_bat_bus_kW / SOC_out / V_bat_V / I_bat_A / dSOC_dt /
              P_dis_avail_kW / P_chg_avail_kW / battery_power_limited /
              battery_discriminant_limit / ecm_lut_boundary_flag
   ──> SOC Integrator（保留：InitialCondition=Bat_SOC_init、限幅
        [ASSUMED_SOC_min, ASSUMED_SOC_max]=[0.10,0.95]；输入由旧 soc_rate 输出
        换为 BatteryECM 的 dSOC_dt——电荷积分口径，禁止能量积分回潮）
   ──> 子系统输出端口建议：
        P_bat_bus_kW、SOC（保留原端口名，下游 13_DC_Bus / 09_Bus_Residual_Monitor
        / EMS 接口零改动）；P_bat_dis_avail_kW / P_bat_chg_avail_kW（同名同义，
        值由 SOC 降额升级为四约束 min，design §4.7）；V_bat_V、I_bat_A、三个 flag
        （新增，见 §4）
```

**退役/删除的块**（重构后不得残留接线）：

- `bat_limits`（CODE_BAT_LIMITS）：SOC 降额可用功率 → 被 BatteryECM 内联 SOP 取代；
- `soc_rate`（CODE_SOC_RATE_B）：能量积分口径（∫P dt/E，含 ASSUMED_Bat_Eff_*）
  → 被 dSOC_dt 电荷积分取代（FR-423 禁止混合口径）；
- `neg_chg` / `lo_clamp` / `hi_clamp`（MinMax 链）：clamp 已内置于 BatteryECM 第 3 步
  `P_bus = min(max(P_droop, -P_chg_avail), P_dis_avail)`；
- `SOC_int` 的输入连线：`hi_clamp → soc_rate → SOC_int` 改为 `BatteryECM.dSOC_dt → SOC_int`。

**参数事实源**：全部来自 `config/ModelE_Physics_Parameters.m` §6（九节点 LUT、
Bat_Q_nom_Ah=1860、Bat_eta_coul=0.98、Bat_V_min/max=360/657、Bat_R0_chg_mult=1.15、
Bat_I_limit_surrogate_A=1823、Bat_P_conv_bus_max_kW=1050、ASSUMED_eta_dcdc=0.97、
SOC 域与降额族、Bat_SOC_init=0.60）。数值已冻结，builder 不得重议。

**Q5 红线确认**：block 源码不含 ASSUMED_Bat_Eff_Discharge/Charge（已全文检索）；
eta_coul 只出现在 dSOC_dt；端口功率无 0.95 乘子。ASSUMED_Bat_Eff_* 在参数文件中的
定义保留（ECMS k_eq 内部引用），Plant 侧停用。

## 2. BatteryECM 端口与参数清单（按签名顺序）

签名共 20 入参 / 10 出参；**全部参数均为 block 输入端口**（无 Parameter-scope Data，
`setMFCode(fn, model.code_battery_ecm_e())` 的 paramNames 传空 `{}` 即可）。
builder 需为 18 个常量入参各加一个 Constant（引用同名 base/模型工作区变量）：

| # | 输入端口 | 接线 |
|---|---|---|
| 1 | `P_droop_kW` | droop Gain 输出（信号） |
| 2 | `SOC` | SOC Integrator 输出（反馈） |
| 3 | `dt` | Constant `0.01`（与模型 fixed-step 一致） |
| 4 | `Bat_SOC_LUT_nodes` | Constant（0.1:0.1:0.9） |
| 5 | `Bat_OCV_LUT_V` | Constant（9 节点） |
| 6 | `Bat_R0_LUT_ohm` | Constant（9 节点） |
| 7 | `Bat_R1_LUT_ohm` | Constant（9 节点） |
| 8 | `Bat_C1_LUT_F` | Constant（9 节点） |
| 9 | `Bat_R0_chg_mult` | Constant（1.15） |
| 10 | `Bat_eta_coul` | Constant（0.98） |
| 11 | `Bat_Q_nom_Ah` | Constant（1860） |
| 12 | `Bat_V_min_V` | Constant（360） |
| 13 | `Bat_V_max_V` | Constant（657） |
| 14 | `Bat_I_limit_surrogate_A` | Constant（1823） |
| 15 | `Bat_P_conv_bus_max_kW` | Constant（1050） |
| 16 | `eta_dcdc` | Constant（ASSUMED_eta_dcdc=0.97） |
| 17 | `SOC_min` | Constant（ASSUMED_SOC_min=0.10） |
| 18 | `SOC_max` | Constant（ASSUMED_SOC_max=0.95） |
| 19 | `SOC_DisDerate_Start` | Constant（0.25） |
| 20 | `SOC_ChgDerate_Start` | Constant（0.90） |

出参 → 子系统出端口映射：`P_bat_bus_kW`、`SOC_out→SOC`、`P_dis_avail_kW→P_bat_dis_avail_kW`、
`P_chg_avail_kW→P_bat_chg_avail_kW`（以上保持既有端口名）；`V_bat_V`、`I_bat_A`、
`battery_power_limited`、`battery_discriminant_limit`、`ecm_lut_boundary_flag` 新增。

注意：LUT/标量参数走端口（签名即契约）；若 builder 误用 `setMFCode` 第三参声明同名
Parameter-scope Data 会与输入端口冲突——**paramNames 必须为空**。

## 3. 建议新增顶层输出（Terminator + logging）

`V_bat_V`、`I_bat_A`、`battery_power_limited`、`battery_discriminant_limit`、
`ecm_lut_boundary_flag` 建议透传到模型顶层接入 logging（To Workspace/logs-out），
供 T-412 G4 判据（V_t 方向性、SOP 收缩、power-limit 语义）与 NaN 审计直接取用；
暂不消费的 flag 可先接 Terminator。SOC Integrator、Bat_SOC_init、限幅语义不变。

## 4. known-limitation 草稿（随证据链文档归档）

1. **SOP 用开路估计 V_oc_est = OCV − V1 而非工作点 V_t**（OQ-9 冻结语义）：SOP 是
   限制值而非工作点解，避免 I↔V_t 代数环（NFR-409）。偏差由 |V1| 决定：1RC 稳态
   |V1| = |I|·R1 ≈ 1823×0.0089 ≈ 16 V（<3% of ~576 V）；充电时 V_oc_est 偏高使
   P_chg 略保守、放电略激进，量级 <3%，且四约束取 min 后多数工况由 P_converter
   （1050 kW）主导，误差被吸收。
2. **LUT 定义域 [0.1, 0.9] 域外 clamp**：SOC∈(0.9, 0.95] 用 0.9 节点值并置
   `ecm_lut_boundary_flag`（禁止外推）；SOC<0.1 由 Integrator 限幅兜底，下界 clamp
   不置 flag（任务书口径）。LFP OCV 曲线平缓段（0.5~0.9）插值误差被参数冻结事实源
   覆盖（LITERATURE surrogate，FR-422 chemistry 声明不变）。
3. **1RC 极化 Euler 离散（0.01 s）**：dt/(R1·C1) ≈ 0.01/21.7 ≈ 4.6e-4 ≪ 2，绝对稳定；
   极化瞬态精度受欧拉一步延迟影响（τ≈22 s，0.01 s 步长误差可忽略）。
4. **D<0 power-limit 语义**：请求超 P_max,R = E²/(4R0) 时 P_actual=P_max,R + 双 flag，
   不是报错也不是假装满足；实际参数化下 SOP P_voltage 通道先于判别式起控
   （P_voltage ≤ E²/(4R0·eta²) 与 eta 有关），判别式分支是最后一道数值防线。
5. **E<1e-3 退化防护**：E≈0（理论不可达：OCV≥541 V、|V1|≲16 V）时强制走 D<0 分支，
   I=E/(2R0)、P_actual=E²/(4R0)≈0，双 flag 置位——保证无 NaN/Inf 路径（NFR-411）。

## 5. 测试与验收锚点（已实际执行，关键数字）

- 解析根：OCV=550/V1=0/R0=0.007/P_t=50 kW(eta=1) → I=91.01452 A（手算 91.01449，
  ±0.1% 内）、V_t·I=50000.00 W 自洽；
- 判别式：恰在边界（fp 精确构造 R0=0.03125、E=500、eta=1、P_t=2e6 W，D=0）→
  不置 flag、I=8000 A；略超 → P_bat_bus=1940 kW=P_max_R×eta，双 flag=1；
- eta_coul 不对称：同 |I|=90 A 循环比值 = 0.980000；充电方向 R_0×1.15：|I_chg|=90.79
  < |I_dis|=91.01 A（同功率）；
- SOP 手算：P_current=1127.63 kW、P_voltage=371.13 kW、P_SOC(0.20)=700 kW、
  P_converter=1050 kW、充电镜像 892.21 kW；
- 零 NaN 扫描：ECM 9 组合 + SOP 210 组合全 isfinite 且非负。

## 6. 遗留事项（主 Agent 处理）

- 本链只新增 5 个文件、未改任何既有文件（AGENTS.md §20 README 登记与 builder 集成
  由主 Agent 统一执行；`scripts/tests/` 与 `+model/` 的索引条目待登记）；
- `buildBatteryConverter`（build_model_e.m 865-911 行）重构与 `CODE_BAT_LIMITS`/
  `CODE_SOC_RATE_B` 删除属主 Agent 的 builder 集成范围；
- 40 号 Gate（G4 电池判据）按 §5 锚点对齐；SOP 信号对 EMS 的暴露名
  （P_bat_dis_avail/chg_avail）同名同义不变（FR-426/§4.7）。