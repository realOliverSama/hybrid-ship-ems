# S20 复合航次 — Baseline 剖面 + DP 上限报告（T-101/T-102）

ECMS Phase 1（spec: `adaptive-ecms-phase1`，FR-101/102/103）
生成：2026-08-24（40_build_composite_voyage.m 运行，结果目录 `results/ecms_phase1/`）

## 1. 场景框架扩展（T-101）

- `config/Model_Scenarios.m` 新增 `SCENARIOS(20)`（`S20_composite_voyage`），新字段：
  `speed_profile_mode='time'`、`v_time_s/v_value_mps`（时间航速路点）、
  `current_time_s/current_value_mps`（时间水流路点）、`stop_time_s=5520`；
- 参数唯一源 `scripts/+ecms2/voyage_s20.m`（design §2.1），config 通过调用该函数填充，
  40 脚本做单一源一致性断言；
- `+model/build_model_c.m` 01_Scenario 段扩展：`v_cmd_profile`/`current_profile`
  支持双模式（mode=0 距离节点 = 既有语义；mode=1 时间节点分段恒定，等效 1-D Lookup on t），
  子系统内部 Clock 提供 t；既有场景 mode=0 路径与原实现逐行等价；
- struct array 同构要求：既有 1~19 场景仅补惰性默认字段（`speed_profile_mode='distance'` 等），
  场景定义字面量未改动，S01 回归逐位一致（见 §3）证明行为不变；
- 模型由 `10_build_modelD.m` 从零重建（未修改 .slx 二进制）。
- canonical 镜像 `scripts/+model/canonical_data/Model_Scenarios.m` 已同步。

### 航次参数（design §2.1，kn→m/s：3→1.543，5→2.572，8→4.115，11→5.658）

| 段 | t [s] | v_target [m/s] | 备注 |
|---|---|---|---|
| 1 离泊机动 | 0–300 | 1.543 | |
| 2 港区限速 | 300–780 | 2.572 | |
| 3 加速 | 780–900 | →4.115 | |
| 4 经济巡航 | 900–2100 | 4.115 | 8 kn，低负荷主战场 |
| 5 加速赶班 | 2100–2220 | →5.658 | |
| 6 服务巡航 | 2220–3120 | 5.658 | 11 kn |
| 7 桥区限速 | 3120–3720 | 2.572 | 减速+低速+再加速 |
| 8 出桥巡航 | 3720–4440 | 5.658 | |
| 9 逆水段 | 4440–5040 | 5.658 | 水流 −1 m/s |
| 10 减速进港 | 5040–5220 | →1.543 | |
| 11 靠泊机动 | 5220–5520 | 1.543 | 结束（t_end=5520 s ≈ 92 min） |

路点按“段起点阶跃、Rate Limiter 平滑”实现（设计：过渡段由既有加速度 Rate Limiter 平滑；
时间路点间的过渡速率与限幅器不一致时以限幅器为准）。斜坡率：accel 0.15 m/s²（赶班/出桥快加速，
design 注“可用 0.15”）、decel 0.05 m/s²（巡航段 RAMP_NORM）。S20 场景字段为单值对，未按段
细分斜坡率（design §2.2 仅新增 v_time_s/v_value_mps 字段）。水流仅段 9 为 −1 m/s，其余 0。

航次终止：时间条件（StopTime=5520 s）为主；里程为核对项（实测 22.5 km vs 设计预估 ~21 km，
差异来自 Rate Limiter 平滑与逆水段船速回落，属核对信息非停机条件）。`voyage_distance_m=40000`
放宽，避免 11_Voyage_Stop_Logic 提前触发。

## 2. S20 Baseline 仿真结果（T-102）

| 指标 | 值 |
|---|---|
| 时长 | 5520 s（92.0 min） |
| 航程 | 22.54 km（预估 ~21 km） |
| P_demand 峰值 / 均值 / 最小 | 800 / 448 / 100 kW |
| 低负荷（<500 kW）时间占比 | **56.1%**（设计意图 ~53%，接受带 40~60%） |
| SOC_end | 0.578 |
| Vdc_min | 690 V（CCS 瞬态下限 600 V，无越限） |
| 降额事件 | 0（无欠压、无降额激活、无 P_unserved） |
| 能量闭合残差 | −0.0003%（<0.1% 通过） |
| Baseline 燃油（离线三档积分） | tier1 320.15 / **tier2 337.93** / tier3 355.71 kg |

### P_demand 时间曲线特征

- **0–780 s（离泊+港区）**：需求 ~100–180 kW（3/5 kn，几乎全低负荷）；起步 PI 短暂无饱和。
- **780–900 s（加速至 8 kn）**：需求爬升至 ~334 kW，随后 8 kn 经济巡航段（900–2100 s）稳定
  ~334 kW，全程 <500 kW（100% 低负荷，设计主战场）。
- **2100–2220 s（赶班加速至 11 kn）**：PI 饱和至 Mot_P_nom=700 kW，需求 800 kW；因 1000 t
  惯量，饱和持续至船速追平 ~5.658 m/s（约 2220 s 后数分钟），形成 2100 s 后长高负荷块。
- **2220–3120 s（11 kn 服务巡航）**：需求稳定 ~709 kW（>500 kW，0% 低负荷）。
- **3120–3720 s（桥区）**：减速至 2.572 m/s 后需求回落 ~159 kW（98.7% 低负荷）。
- **3720–4440 s（出桥巡航）**：~709 kW。
- **4440–5040 s（逆水 −1 m/s）**：需求饱和 800 kW（PI 顶满，v_ship 回落至 ~4.93 m/s——
  11 kn 顶水无法维持，物理自洽，与 S03 逆水行为一致），0% 低负荷。
- **5040–5220 s（减速进港）**：需求快速回落，~90% 低负荷。
- **5220–5520 s（靠泊）**：~110 kW（100% 低负荷）。

整体形态：三段高负荷平台（赶班+服务巡航、出桥巡航、逆水段）夹三段低负荷平台
（8 kn 经济巡航、桥区、进港靠泊），是检验 ECMS 功率预分配的典型复合剖面。

## 3. S01 回归（FR-101）

重建后重跑 S01 Baseline，与 `results/ecms_phase0/profiles/S01_still_water.mat` 对比
（`ecms2.check_s01_regression`）：

- 时长 927 s = 927 s；样本 928 = 928；
- 全部 11 个对比字段（P_demand/P_prop_demand/P_hotel/P_gen/P_bat/SOC/V_dc/v_ship/x_ship/
  P_load_total/P_bus_residual）max|Δ| = 0；
- **P_demand 逐位一致（容差 0）→ 通过**（扩展未改变既有场景行为）。

## 4. DP 上限（T-102，tier2 中档，连续口径）

| P_min | fuel_dp [kg] | fuel_base_corr [kg] | 节油率 | Baseline 可行 | 校验 |
|---|---|---|---|---|---|
| 0 kW | 315.83 | 349.01 | **9.51%** | 是 | dp_checks 全过（含 DP≤Baseline 硬校验） |
| 400 kW | 370.81 | 349.01 | −6.25%（信息性） | 否（3088 点 P_gen<400） | dp_checks 全过 |

说明：

- P_min=0：DP 在低负荷段可怠速/移峰，相对 SOC 修正 Baseline 节油 **9.51%**（比 Phase 0
  基础场景 3.6~4.6% 高，源于 S20 大量低负荷段的可优化空间）；SOC_end=0.596 落入终端带。
- P_min=400：Baseline 在“连续口径 P_gen≥400”约束下不可行（低负荷段 P_gen 低至 ~100 kW），
  故节油率为信息性（保守比较方向，dp_checks 按 design 跳过 DP≤Baseline 硬校验）；DP 结果
  显示强制最低负荷 400 kW 使燃油劣于自由运行 Baseline（−6.25%），量化了低负荷运行约束代价。
- 两档解的 `results/ecms_phase1/dp_S20_continuous_pmin000.mat` /
  `dp_S20_continuous_pmin400.mat` 保存 prob/sol/chk/cfgI，`chk.pass=true`、
  能量闭合残差 ~1e-14 量级。

## 5. 产物

- `results/ecms_phase1/profiles/S20_composite_voyage.mat`（字段与 Phase 0 剖面一致）
- `results/ecms_phase1/profiles/S20_summary.json`
- `results/ecms_phase1/dp_S20_continuous_pmin000.mat` / `dp_S20_continuous_pmin400.mat`
- `scripts/40_build_composite_voyage.m`（可复现入口）
- `scripts/+ecms2/voyage_s20.m` / `scn_field.m` / `check_s01_regression.m`

## 6. 已知说明 / 遗留

- 低负荷占比 56.1% 略高于 53% 设计意图，在验收带（40~60%）内；差异源于赶班段 PI 饱和时长
  与逆水段饱和段的分布（两段均为 >500 kW，压缩低负荷占比的机制与设计一致）。
- 航程 22.5 km 高于 design 预估 ~21 km（Rate Limiter 平滑使实际在节点间保持目标航速，
  且逆水段按对地速度累计；核对项，非停机判据）。
- S20 加入 SCENARIOS 数组后，Model A 流水线脚本（04_run_scenarios.m 等）若运行将包含
  S20（长时长仿真）；不影响任何 Gate（03/10b 均为显式场景列表）。
