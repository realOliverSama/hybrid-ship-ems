# Dataset v3 观测变量契约（observation_contract.md）

> Spec: `.specs/phase4-d-dataset-v3`（T-430 / T-002；FR-461、FR-464、FR-465；design DGN-01/02/05/06）
> **事实源声明（FR-465）**：本文件是解释层，不是第二事实源。信号清单、role、aggregation、
> predictor_access 的**机器权威版本**为
> `model/simulink_modelE_(Phase4_Physics)/config/dataset_v3_signal_registry.json`
> （`schema=dataset_v3_signal_registry_v1`，self_hash 见文件内 `self_hash.value`，
> SHA256 over canonicalized content）。本文表格如与 registry 冲突，**以 registry 为准**并修本文件。
> 下文表格仅为可读性复述，禁止单独修改表格而不改 registry。

## 1. 三层观测分类法（H1/H2/H3）

H1/H2/H3 是**观测分类法**（哪些历史量在什么条件下可被预测器看到），与八类 storage role
（`INPUT_H1/INPUT_H2/INPUT_K/INPUT_F/TARGET/ORACLE/METADATA/FORBIDDEN`，FR-464）是两个维度。
映射规则（冻结，DGN-02 + design R1）：H1→`INPUT_H1`，H2→`INPUT_H2`；**H3 ≠ FORBIDDEN**——
PMS audit 族/EMS 内部信号映射 `FORBIDDEN`（存档供审计、永不可入 view），`scenario_id` 映射
`METADATA`，`resistance_multiplier` 等 scenario latent 映射 `METADATA`（predictor_access=metadata_only）。
H3 信号**物理保留**（provenance/OOD/error-attribution 合法审计用途），仅在 model view 白名单处被挡。

### 1.1 H1 — 默认进主 predictor（11 列）

| 信号 | 单位 | aggregation | source_signal | 部署可获得性论证 |
|---|---|---|---|---|
| P_load_total_kW | kW | time_mean | logsout | 母线侧功率表/电力分析仪，船舶既有电测标准配备 |
| v_ship_mps | m/s | time_mean | logsout | GPS/计程仪标准配备 |
| acceleration_mps2 | m/s² | time_mean | logsout | 由船速差分或惯性测量单元（IMU）获得；低频部署下可由 1 Hz 差分重构 |
| n_prop_rpm | rpm | time_mean | logsout | 轴转速传感器，推进系统标准配备 |
| P_hotel_kW | kW | time_mean | logsout | 辅机/生活负荷分路电量计 |
| v_current_mps | m/s | time_mean | logsout | 计程仪对水速度与 GPS 对地速度差、或海流计；历史值为测量量 |
| water_depth_m | m | time_mean | scenario:VOYAGE_WATER_DEPTH_M | 测深仪标准配备。Model E 中为逐航次常量（场景注入，未打点——Model E 零改动约束下由 producer 从场景元数据导出常量列） |
| P_prop_demand_kW | kW | time_mean | logsout | 推进控制器指令/变频器读数，驾驶台可读 |
| ctrl_mode | mode | last_mode | logsout | 操舵/操控模式指示，驾驶台状态量（0=AUTO/1=MANUAL） |
| v_ref_mps | m/s | interval_endpoint | logsout | 航速设定值（AUTO 模式下驾驶员/航线计划输入；MANUAL 模式聚合列 zero-fill，见 §3） |
| manual_power_cmd_pct | pct | interval_endpoint | logsout | 车钟/功率手柄指令（MANUAL 模式输入；AUTO 模式聚合列 zero-fill，见 §3） |

部署论证总结：H1 全部为"部署时真实可获得"的操作员/环境/负荷观测量——这是它们进入默认
特征集的资格依据（AI_MODELING §8：Feature 必须部署可得、无未来信息、无泄漏）。

### 1.2 H2 — extended-state（保留进 Dataset，非默认特征，10 列）

| 信号 | 单位 | aggregation | source_signal | 备注 |
|---|---|---|---|---|
| P_gen_actual_kW | kW | time_mean | logsout | 机组总实际出力 |
| P_bat_actual_kW | kW | time_mean | logsout | 电池母线侧实际功率（放正充负） |
| SOC | ratio | interval_endpoint | logsout | 状态积累量 → bin 右端点 |
| V_dc_V | V | time_mean | logsout | 母线电压 |
| state_DG1 | state | last_mode | logsout | GensetUnitE 现有编码 0=OFF/1=STARTING/2=SYNCHRONIZING/3=ON/4=COOLDOWN（禁新造枚举） |
| state_DG2 | state | last_mode | logsout | 同上 |
| V_bat_V | V | time_mean | logsout | 电池端电压 |
| I_bat_A | A | time_mean | logsout | 电池电流 |
| P_bat_dis_avail_kW | kW | interval_endpoint | logsout | SOP 四约束 min（bus-side）；约束快照 → 端点 |
| P_bat_chg_avail_kW | kW | interval_endpoint | logsout | 同上 |

H2 保留语义：extended-state 用于扩展状态实验/诊断（如含机组与电池内部状态的 ablation），
**不进默认特征集**；view 构建由 manifest 白名单按 role 控制（`INPUT_H2`→H2-view）。

### 1.3 H3 — predictor-forbidden 的历史/记录类信号（映射规则）

H3 成员（PMS audit 族、EMS 变体内部信号、scenario latent 变量）按 DGN-02 R1 映射为
storage role，物理保留、view 封锁：

| H3 成员族 | storage role | predictor_access | 审计用途 |
|---|---|---|---|
| PMS v2 audit 族（commitment_state、reason_code、audit_* 九元组、hold/online timer、N_dispatch、dispatch 诊断、enable/ref/cmd 通道、P_commit/P_gen_req_eff/P_load_gen、X_b2g 换算量、below_min/start_count、P_DG1/2_kW 等） | FORBIDDEN | forbidden | commitment 合法性审计、error attribution（预测误差 → 控制行为归因） |
| EMS 内部/输出（P_gen_ref_kW 等；AI 变体内部信号不导出） | FORBIDDEN | forbidden | 控制行为审计 |
| scenario_id | METADATA | metadata_only | provenance / strata / split 分层证据 |
| resistance_multiplier | METADATA | metadata_only | scenario latent：OOD/误差归因审计，预测器禁入 |
| domain_warning / HT_ratio | METADATA | metadata_only | 物理域证据（FR-486：OOD∩Model E 已验证物理域检查；越域标 PHYSICS_EXTRAPOLATION/STRESS_SET） |

完整机器清单见 registry（role=FORBIDDEN 55 条 + role=METADATA 4 条）。
Model E 的其余打点（燃油、母线残差、AI 内部等）不在 Dataset v3 契约池内，registry 未登记、producer 不导出。

## 2. aggregation 语义（DGN-05 + design R1 统一积分语义）

历史窗 300 s @ 1 Hz = 300 个因果 1 s bin，`H_j = (t₀−j, t₀−j+1]`（j=300..1，右闭无歧义，
FR-469）。逐列规则由 registry `aggregation` 字段驱动（Builder/validator 消费机器契约，
禁脚本内 hard-code）：

| 规则 | 语义 | 适用 |
|---|---|---|
| `time_mean` | bin (a,b] 内 **trapezoidal 时间加权均值** ∫v dt/(b−a)（design R1：统一 time-weighted 积分语义，非 mean(samples)；禁 raw[::100] 式 naive decimation） | 全部功率/速度/加速度/RPM/水深/流速/电压/电流列 |
| `interval_endpoint` | bin 右端点值 | SOC、SOP 可用功率、v_ref、manual_cmd（状态积累量/约束快照） |
| `last_mode` | bin 内最后采样值 | ctrl_mode、state_DG1/2（离散模式/状态） |
| `interval_any` | bin 内任一为真 | 事件/flag（仅 METADATA/ORACLE 列，如 domain_warning） |

能量语义自检：功率列 time_mean 满足 Σ_k P̄_k·Δt ≈ ∫P dt（unit test 相对容差 1e-9）；
该定义与 target 的 native-rate trapezoidal 能量差构建**同一积分语义**，二者交叉验证
（FR-476/DGN-08，相对差 <1e-9）——这是拒绝 `raw[::100]` 抽点的数学理由。

## 3. ctrl_mode 非激活通道表征（DGN-06 冻结：zero-fill + 模式指示）

`ctrl_mode` 决定哪个操作员指令通道语义激活；非激活通道的确定性表征冻结为：

- `ctrl_mode = AUTO(0)`：`manual_power_cmd_pct` 聚合列填 **0.0**；
- `ctrl_mode = MANUAL(1)`：`v_ref_mps` 聚合列填 **0.0**；
- `ctrl_mode`（last_mode）列始终存在，使两通道语义可区分；
- 确定性、无 NaN、无双写；
- ORACLE 层保留原始双通道真值（registry 条目 `v_ref_mps_unmasked`、
  `manual_power_cmd_pct_unmasked`，predictor_access=oracle_only）供审计。

## 4. 与 storage role / view 的关系

- model view 白名单由 manifest 按 registry role 构造：H1-view（11 列 INPUT_H1）、
  H2-view（INPUT_H1+INPUT_H2，21 列）、trajectory target view（Y[30]，由 TARGET 源
  P_load_total_kW 的 native stream 按 DGN-08 构建）、legacy scalar view（E_300）。
- FORBIDDEN/METADATA/ORACLE 进入任何 model view 必须被 builder 拒绝（负向测试覆盖，
  FR-464/AC-427）。
- K/F 类未来信息契约（`INPUT_K`/`INPUT_F`、issue-time causality x(t₀+k|t₀)、forecast
  provenance 六字段）见 `future_information_contract.md`（同一目录）。
