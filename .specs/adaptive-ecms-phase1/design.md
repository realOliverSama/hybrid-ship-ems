# Phase 1 实时 ECMS 控制器链 — Design

Spec: `adaptive-ecms-phase1`
Status: design-approved（依据用户 2026-08-24 批量授权——brainstorm E4）
Created: 2026-08-24
Requirements: `./requirements.md`（v1，按 E4 批准）

## 1. 总体架构

```text
S20 复合航次构造（40_build_composite_voyage.m）
        │  Baseline 仿真 + 剖面导出（复用 +ecms/run_profile_sim.m）
        ▼
DP 上限（复用 +ecms/dp_solve.m，连续口径，P_min ∈ {0, 400}）
        ▼
ECMS 变体施工（build_model_c.m 增加 EMS_CTRL_LAW=4/5/6）
        │  41_calibrate_lambda.m（λ / K_soc 标定）
        ▼
实验矩阵（42_run_phase1_matrix.m）→ runs_phase1.csv
        ▼
Gate 判定（43_phase1_gate_judge.m）→ results/ecms_phase1/gate_result.json
        ▼
报告 reports/ECMS_phase1_ecms_vs_dp.md（主 Agent 撰写）
```

新代码：`scripts/+ecms2/`（Phase 1 专用，与 Phase 0 的 `+ecms/` 只读复用关系）：
- `+ecms2/voyage_s20.m`——复合航次剖面定义（time-based waypoints）
- `+ecms2/ecms_law.m`——ECMS 单步决策核心（供 Simulink MATLAB Function 块与离线单元测试共用同一实现）
- `+ecms2/calibrate_lambda.m`——λ / K_soc 二分标定
- `+ecms2/oracle_future_energy.m`——由剖面预计算 E_future_H(t)（oracle 前瞻信号）

## 2. 复合航次 S20（FR-101/102）

### 2.1 剖面定义（草案 V1，参数化集中 config）

kn→m/s：3 kn=1.543，5 kn=2.572，8 kn=4.115，11 kn=5.658。

| 段 | t [s] | v_target [m/s] | 备注 |
|---|---|---|---|
| 1 离泊机动 | 0–300 | 1.543 | 起步斜坡由 accel limiter 平滑 |
| 2 港区限速 | 300–780 | 2.572 | |
| 3 加速 | 780–900 | →4.115 | |
| 4 经济巡航 | 900–2100 | 4.115 | 8 kn，L≈0.33（主战场） |
| 5 加速赶班 | 2100–2220 | →5.658 | |
| 6 服务巡航 | 2220–3120 | 5.658 | 11 kn 高效区边缘 |
| 7 桥区限速 | 3120–3720 | 2.572 | 减速+低速+出桥加速 |
| 8 出桥巡航 | 3720–4440 | 5.658 | |
| 9 逆水段 | 4440–5040 | 5.658 | 水流 −1.0 m/s |
| 10 减速进港 | 5040–5220 | →1.543 | |
| 11 靠泊机动 | 5220–5520 | 1.543 | 结束（t_end=5520 s ≈ 92 min） |

- 里程按 v³ 阻力与速度剖面自然累计（预计 ~21 km）；voyage stop 改为 **时间/里程双条件先到**（时间条件为主，里程做完整性核对）。
- 水流剖面：仅段 9 逆水 −1 m/s，其余 0（复用 current_knots 机制或时间序列水流——实现任选，以最小改动为准）。
- 酒店负荷 100 kW 全程恒定。

### 2.2 框架扩展

- 场景结构新增 `speed_profile_mode: 'time'` 与 `v_time_s/v_value_mps` 字段；01_Scenario 的速度指令源增加"时间序列表"路径（1-D Lookup on t），既有距离节点路径保留不变；
- **回归要求（FR-101）**：扩展后重跑 S01 Baseline，P_demand 轨迹须与 Phase 0 剖面逐位一致；
- 实现位置：`scripts/+model/build_model_c.m` 中 01_Scenario 构建段 + `config/Model_Scenarios.m` 新增 SCENARIOS(20)。注意 ensure_canonical：config 改动须同步 canonical_data 镜像。

## 3. ECMS 变体设计（FR-104~108）

### 3.1 单步决策核心（三变体共用）

```text
P_g* = argmin_{P_g ∈ 可行集} [ ṁ_f(P_g) + λ(t) · ṁ_bat,eq(P_g) ]
ṁ_f(P_g)      = P_g · bsfc_agg(P_g) / 3600 / 1000          [kg/s]（tier2 中档曲线）
ṁ_bat,eq(P_g) = P_bat_dc/(η_dcdc·η_bat) / 3600              P_bat_dc > 0（放电，正代价）
              = P_bat_dc·η_dcdc·η_bat / 3600                P_bat_dc < 0（充电，负代价=收益）
P_bat_dc      = P_demand − η_rect·P_g                        [kW]
可行集        = {0（怠速，ṁ_f = 6.795/3600 kg/s）} ∪ [P_min_stable : 20 : 1000] kW
              再经爬坡 |P_g − P_g,prev| ≤ R_gen·Δt_ctrl 与 SOC 边界截断
λ 单位 kg/kWh；基线 λ₀ ≈ k_eq = 0.4774（设计起点，标定修正）
```

- 控制周期 Δt_ctrl = 1 s（EMS 层既有周期；若既有 EMS 为 0.1 s 则保持，网格搜索每控制周期执行）；
- 单步 52 个网格点穷举，无迭代优化器（NFR-101）；
- SOC 边界保护：SOC ≤ 0.10+ε 时强制 P_g = max(可行)（禁放电）；SOC ≥ 0.95−ε 时禁充电——硬约束优先于 λ。

### 3.2 变体

| law | 名称 | λ(t) |
|---|---|---|
| 4 | Fixed ECMS | λ = λ₀（常数，FR-105 标定） |
| 5 | SOC-Adaptive ECMS | λ₀ + K_soc·(SOC_ref − SOC(t))，SOC_ref=0.60 |
| 6 | Oracle Predictive A-ECMS | law5 + K_E·ê_future(t)；ê_future = (E_future_H(t) − Ē_H)/E_scale |

- **ê_future**：由剖面 P_demand 离线预计算 E_future_H(t) = ∫_t^{t+H} P_d(τ)dτ（H=300 s，尾端截断为零填充），Ē_H/E_scale 为全航程均值/标幺基数；物理含义：未来高能量需求 → λ 升高 → 保留电池。K_E 在 S20 上标定（与 λ 同法，目标 SOC_end 入带且不劣化 C2 燃油）。
- law=6 的 E_future 信号经 From Workspace 输入（oracle 专用通道，与 AI_Pred_Source 既有通道分离，避免与 C1 即时前馈线混淆）。

### 3.3 Simulink 集成

- `build_model_c.m`：05_EMS_Controller 增加 law=4/5/6 变体（MATLAB Function 块封装 `ecms2.ecms_law` 的等价内联实现——Simulink 块内不能直接调包函数，需代码展开或 `coder.extrinsic`；**实施时优先评估 extrinsic 调用性能**，若不可接受则内联展开，两者须经同一单元测试锁定一致性）；
- EMS 输入增加：SOC、P_demand（母排侧测量）、E_future（law=6 专用）；输出 P_gen_ref 接既有 06_Genset 爬坡/惯性环节不变；
- BSFC 表经 ecms_bsfc_override.mat 机制注入 tier2（与 DP 严格同曲线，E5/FR-113）。

### 3.4 λ / K_soc 标定（FR-105/106）

- S01 场景，二分搜索：SOC_end < 0.595 → λ 上调（电池变贵）；SOC_end > 0.605 → 下调；容差 ±0.005，上限 40 次迭代；
- 产物：`results/ecms_phase1/lambda_calibration.json`（λ₀、K_soc、迭代轨迹、残差）；
- 跨场景迁移性不另行标定（OQ-12：观察同一 λ 在 S02/S07/S20 的表现即 RQ1 素材）。

### 3.5 单元测试（FR-108，`+ecms2/test_ecms_law.m`）

1. 恒定需求 700 kW 稳态：P_g* 与离线穷举最优点一致（<2%）；
2. λ→大：P_g* = 满功率上限（电池保护）；
3. λ=0：纯燃油瞬时最优（放电自由）；
4. 符号约定：放电正代价/充电负收益方向单测；
5. Simulink 块实现与离线实现同输入同输出（容差 0）。

## 4. 实验矩阵与指标（FR-109/110）

- 主矩阵：{C0,C1,C2,C3} × {S20,S01,S02,S07}；回归：{C0,C1,C2} × 8 场景；
- 指标：fuel_raw、fuel_corr（k_eq 同 Phase 0）、SOC_end、Vdc_min、欠压事件数、电池吞吐量 ∫|P_bat|dt；
- DP 标杆：同场景同 tier2 同 P_min=0 连续口径（S20 由 T-102 计算；S01/S02/S07 复用 Phase 0 runs）；
- 输出 `results/ecms_phase1/runs_phase1.csv`（每行：controller、scenario、fuel_raw/corr、capture_rate（对 C1/C2/C3）、SOC_end、Vdc_min、derate 事件、bat_throughput、wall_time、config_hash）。

## 5. Gate 判定（FR-110，43_phase1_gate_judge.m）

```text
capture_rate(Cx) = (fuel_C0_corr − fuel_Cx_corr) / (fuel_C0_corr − fuel_DP)   （S20 主判定）
oracle_increment_pp = saving(C3) − saving(C2)                                  （S20 主判定，其余场景附列）
recommendation:
  oracle_increment ≥ 0.5 pp → PROCEED_TO_PHASE2_NN
  0.2 ~ 0.5 pp            → DECISION_REQUIRED（交用户）
  < 0.2 pp                → NN_VALUE_LIMITED（Phase 2 转向部署价值论证）
```

注意：capture_rate 是诊断指标不是过门条件；Phase 1 的 GO 语义 = 完成 RQ1~3 测量（机器可读），判读归用户。

## Testing Strategy

| 层级 | 内容 | FR |
|---|---|---|
| 单元 | ecms_law 五项单测（§3.5） | FR-108 |
| 回归 | 场景框架扩展后 S01 剖面逐位一致 | FR-101 |
| 集成 | 10b smoke overall=true（每个新变体落地后重跑） | FR-112 |
| 标定 | λ 二分收敛（SOC_end 入带） | FR-105 |
| 复现 | 主矩阵双跑逐位一致（除 wall_time） | FR-114 |
| 门禁 | gate_result.json schema + recommendation 与数值一致性 | FR-110 |

## 7. 已知局限

- 复合航次功率标定基于 1000 t/11 kn 假定体系（E3：真实数据回来统一重跑）；
- oracle 前瞻信号是离线预计算的真值——仅用于上界验证，不代表可在线获得（E5）；
- ECMS 网格穷举（20 kW 步长）相对连续最优有量化损失（预估 <0.1%，由 §3.5-1 与 DP 交叉验证覆盖）；
- λ 单点标定（S01）的跨场景迁移风险已知并接受（作为 RQ1 观察项）。

## Requirements Traceability

| FR | 设计章节 |
|---|---|
| FR-101/102/103 | §2 |
| FR-104~108 | §3 |
| FR-109/110 | §4/§5 |
| FR-111 | §1（报告） |
| FR-112~114 | §6 |
| NFR-101~103 | §3.1（网格穷举）、§4（规模）、§7 |

## 9. 未决项

- OQ-11 已在本版定案（§3.2 law=6 形式）；OQ-12 单点标定（§3.4）；OQ-13 不阻塞。

## Design Amendment Log

- v1.1（2026-08-24，T-106 实施修订，主 Agent 裁决）：law=6 前瞻律由 **additive λ 形式（λ += K_E·ê）改为 SOC_ref 通道形式**——SOC_ref(t) = 0.60 + K_S·ê_future(t)，λ(t) = λ₀ + K_soc·(SOC_ref(t) − SOC(t))，并加 λ_eff 下限钳位 max(λ_eff, 0.43)。原因：v1 在 S20 上所有非零 K_E 档均劣于 law=5（fuel_corr 随 K_E 单调恶化），机理为 λ 悬崖（~0.41）非对称响应——ê<0 支路把 λ 压过放电阈值导致发电塌缩透支 SOC，ê>0 支路发电抬升有限。v2 经已标定的 K_soc 反馈通路作用，避开悬崖。K_S 标定结果：K_S=0.01 合格，S20 fuel_corr 330.66→324.96 kg（**oracle 增量 +1.63 pp**），证明预测经 SOC_ref 通道有真实经济性价值。若 v2 也失败则接受“此 law 族无 oracle 增量”的 RQ3 结论——实际未触发。
