# 边缘AI自适应ECMS甲醇EMS — Phase 0 Design

Spec: `adaptive-ecms-methanol`
Status: design-approved (v1.1，2026-08-24 用户批准)
Created: 2026-08-24
Requirements: `./requirements.md`（v1 已批准）

## 1. 总体架构与数据流

```text
model/simulink_modelD/（参数修正后的独立副本）
        │
        ▼ 30_export_load_profiles.m      （Baseline EMS 仿真，8 场景）
results/ecms_phase0/profiles/<SCN>.mat    P_demand(t) + Baseline 参照轨迹
        │
        ▼ 31_build_bsfc_family.m         （双锚点 + 恶化档位）
results/ecms_phase0/bsfc/bsfc_family.mat  1-D × 3 档 + 2-D map + PNG
        │
        ▼ 32_run_phase0_matrix.m         （主矩阵：DP 求解器循环）
results/ecms_phase0/runs/<run_id>.mat + runs.csv
        │
        ▼ 33_run_singlefactor_sweeps.m   （爬坡率 / 电池容量单因素）
        ▼ 34_dp_convergence_check.m      （S07 网格加密收敛检查）
        ▼ 35_phase0_gate_judge.m         （机器判定）
results/ecms_phase0/gate_result.json
        ▼ 36_make_phase0_report.m        （汇总图表）+ 主 Agent 撰写
reports/ECMS_phase0_upper_bound.md
```

核心代码包：`model/simulink_modelD/scripts/+ecms/`

| 文件 | 职责 |
|---|---|
| `+ecms/bsfc_family.m` | BSFC 曲线族生成（1-D 三档 + 2-D map），含自检 |
| `+ecms/dp_solve.m` | DP 核心求解器（单次问题实例） |
| `+ecms/dp_problem.m` | 问题结构体构建（场景剖面 × 配置组合 → problem struct） |
| `+ecms/load_profile.m` | 剖面加载与 1 s 重采样 |
| `+ecms/dp_checks.m` | 内置正确性校验（DP≤Baseline、能量闭合、非平凡性） |
| `+ecms/speed_cost_min.m` | 二维 map 下对给定 P_g 预求最优转速（见 §4.4 解耦） |

单一配置入口：`config/Ecms_Phase0_Config.m`——场景集、网格分辨率、恶化档位、最低稳定负荷档位、运行口径、效率、启动代价、转速范围，全部集中可配。

## 2. 工程副本与参数修正（FR-001~003）

- 副本来源：`model/simulink_modelC/`（冻结，只读）→ `model/simulink_modelD/`；排除 `.venv_tf`（符号链接复用）、`slprj`、`results`、`*.slxc`。
- 副本内 Model C 标识统一替换为 Model D（文件名与内部引用），沿用 oracle_ms 副本的既有改名模式。
- 参数修正（写入 modelD 的 `config/Model_Parameters.m`，ParamMeta 同步更新来源/置信度）：

| 参数 | 旧值 | 新值 | 依据 |
|---|---|---|---|
| `DRAWING_Bat_Cap_kWh_perString` | 600 | **535.68** | docx：576 V × 930 Ah |
| `Bat_Cap_kWh`（派生） | 1200 | **1071.36** | ×2 |
| `ASSUMED_Bat_DischargeLimit` | 1000 | **1050** | DC/DC 750V×700A×2（置信度升 high，改名去 ASSUMED 前缀在副本内允许） |
| `ASSUMED_Bat_ChargeLimit` | 1000 | **1050** | 同上（双向 DC/DC） |
| BSFC 额定锚点 | 460 @1000 kW | **453 @1000 kW** | docx 453(+5%)；容差上界 475.7 入敏感性 |

- BSFC 输入改造：modelD 的 `10_Fuel` 查表输入改为"若 `config/Ecms_Phase0_Config.m` 提供覆盖曲线则用覆盖值，否则用默认表"——保证 Baseline 与 DP 严格同口径（FR-EdgeCase：禁止跨口径比较）。
- modelD smoke 门禁：参数修正后原 gate 基线必然失效 → 在 modelD 内重建 `gate_baseline_smoke.json`（🟡 用户批准后执行，报告中声明）。
- **双机聚合裁决（brainstorm D8，2026-08-24）**：用户明确双机独立调度本期不讨论，Phase 0 保持聚合单机（默认两侧一同出力）；GPT 评审指出的“聚合可能产生假 NO-GO”风险以两点对冲：(1) gate/报告的 NO-GO 措辞限定聚合口径并声明双机自由度未探索；(2) 登记未来扩展路径——机组群最小燃耗函数 ṁ_f,min(P_total, N_on)，N_on ∈ {0,1,2}，DP 主状态不变、仅启停离散维扩展。

## 3. BSFC 曲线族设计（FR-004/005）

### 3.1 一维曲线（单机，1500 rpm，P ∈ [0, 500] kW）

负荷率 L = P/500。控制点 + pchip 保形插值：

| L | BSFC [g/kWh] | 说明 |
|---|---|---|
| 1.00 | **453** | 额定锚点（docx） |
| 0.90 | ≈438（两锚点线性） | 过渡 |
| ξ_opt = 0.80 | **427** | 最优锚点（潍柴 WH17ME 公开数据） |
| 0.50 | 427×(1+d50) | d50 = 0.35×d25 |
| 0.25 | 427×(1+d25) | **恶化档位 d25 ∈ {0.30, 0.50, 0.70}** |
| 0.10 | 427×(1+d10) | d10 = 1.6×d25 |

- ξ_opt、d50 系数 0.35、d10 系数 1.6 为 design 假定值（典型中速机曲线形状），集中登记于 config，报告"待确认"清单列出；
- 怠速燃油：P=0 且机组开机时 fuel_flow_idle = 3% × 额定燃油流量（226.5 kg/h × 0.03 ≈ 6.8 kg/h），参数化；仅连续口径且 P_min_stable=0 时起作用；
- 自检（FR-005）：锚点偏差 <0.1%；L ∈ [ξ_opt, 1] 单调不降检查；恶化档满足 d(0.10) > d(0.25) > d(0.50) ≥ 0；pchip 无过冲（加密采样验证）。

### 3.2 二维 map（转速 × 功率）——扩展分析专用，不进主 Gate

> 外部评审修正：主判定链路仅使用 §3.1 一维定速曲线（1500 rpm）；二维 map 仅作 conditional sensitivity / 论文情景。DP 可能通过人为构造的最优转速线获得虚假收益；待真实机组转速允许范围、整流器形式、二维燃耗 map 获取后，二维方可升级为正式分析。

```text
bsfc(n, P) = bsfc_1d(P) × (1 + k_n × ((n − n*(P)) / Δn_band)²)
n*(P)     = n_lo + (n_hi − n_lo) × (P/500)^α      最优转速线
```

- 初值：n ∈ [900, 1500] rpm，n_lo=900，n_hi=1500，α=0.7，Δn_band=300，k_n=0.10；
- 全部为 config 参数（OQ-3）；若硬件核实定速 → 转速网格退化为 {1500}，代码路径不变；
- 输出等高线 PNG + 最优转速线叠加图。

## 4. DP 求解器设计（FR-008~011）

### 4.1 问题形式化

- 时间：t = 0…N−1，Δt = 1 s；N 由场景仿真时长决定（≈ 900~1000）；
- 状态 x = (SOC, P_prev[, on])：
  - SOC 网格 [0.10 : δSOC : 0.95]，δSOC = 0.005（171 点；收敛检查减半）；
  - P_prev 网格 = 控制网格（见下），天然在格点上，无需插值；
  - on ∈ {0,1}：仅启停口径启用；
- 控制 u = P_g ∈ 可行集：`{0（仅启停口径停机）} ∪ [P_min_stable : δP : 1000]`，δP = 20 kW；可行集再受爬坡 |P_g − P_prev| ≤ R_gen×Δt 截断；
- 功率平衡与符号约定（sign convention，施工前写死）：

```text
P_d(t) = η_rect × P_g + P_bat_dc          P_bat_dc > 0 = 放电，< 0 = 充电
放电（P_bat_dc > 0）：E_store −=  P_bat_dc / (η_dcdc × η_bat) × Δt/3600   [kWh]
充电（P_bat_dc < 0）：E_store += (−P_bat_dc) × η_dcdc × η_bat × Δt/3600   [kWh]
η_rect = η_dcdc = 0.97，η_bat = 0.95（沿用模型效率链）
实现自检（单测锁死）：P_bat_dc = 0 → E_store 不变；放电必减、充电必增
```

- 约束：|P_bat_dc| ≤ 1050 kW；SOC ∈ [0.10, 0.95]（映射 E_store ∈ [107.1, 1017.8] kWh）；越界 = 不可行（成本 +Inf）；
- 单步成本：fuel = P_g × bsfc(P_g, n*) / 3600 / 1000 × Δt [kg]（+ 启停口径 on: 0→1 时加 C_start）；
- 终端成本：V_N(SOC) = 0 若 **|SOC − SOC₀| ≤ 0.005**（对称带，外部评审修正：防止 DP 单向借用初始电量 ≈5.36 kWh），否则 1e6（软不可行；若因此全不可行 → 该场景标记 INFEASIBLE，见 Edge Cases）；
- 求解：后向归纳 + V 对 SOC 线性插值；控制循环内向量化状态维（MATLAB 矩阵运算）。

### 4.2 两口径

- **连续运行**：on 恒 1，P_g ∈ [P_min_stable, 1000]；
- **允许启停**：on ∈ {0,1} 进状态维；停机时 P_g ≡ 0 且 fuel = 0；启动代价 C_start = 启动燃油 + 等效磨损折算，初值 = 2 kg/次（文献典型量级，OQ-4，入敏感性附加档 {1, 2, 5} kg）。

### 4.3 爬坡参数

主矩阵用 R_gen = 40 kW/s（聚合，沿用模型现值）；单因素扫描 {20, 40, 80} kW/s（FR-012）。二维 map 下转速准静态假设（转速无动态约束）——登记为局限。

### 4.4 转速解耦优化（关键简化）

转速不进状态维（无动态、无约束），因此对给定 P_g，最优转速选择只影响当步成本：

```text
bsfc_eff(P_g) = min over n grid of bsfc(n, P_g)
```

→ 预处理为等效一维成本函数，DP 控制维保持一维。**二维 map 不增加 DP 复杂度**，同时自动产出最优转速轨迹（论文素材）。

### 4.5 正确性校验（FR-010/011，内置不可绕过）

1. DP 最优燃油 ≤ 同场景同口径 Baseline 燃油——**条件校验**（外部评审修正，同物理可行域原则）：先验证 Baseline 轨迹在当前 DP 约束集（最低稳定负荷/爬坡/电池限值）内可行；可行则违反 → error；不可行则跳过并标记 `baseline_infeasible_conservative`；
2. 能量闭合：Σ(发电能量) − Σ(需求能量) − ΔE_battery，残差 < 0.1%；
3. 非平凡性：SOC 轨迹贴下限滑行比例 > 50% 时标记 `trivial_suspect`（供 gate 判据使用）；
4. 收敛性：S07 网格加密（δSOC→0.0025，δP→10 kW）复算，燃油差 < 0.5%。

## 5. Baseline 口径（FR-006/007）

- Baseline EMS（EMS_MODE=0，无预测）+ modelD 修正参数 + **同恶化档一维 BSFC 曲线**（通过 §2 覆盖机制注入）逐场景仿真；
- **P_demand 因果隔离（外部评审修正，承接前次 oracle 污染教训）**：P_demand 提取自推进需求指令侧（速度 PI 输出/降额作用之前），不得取母排实际服务负荷；不变性验证——同一 config 下分别以 EMS_MODE=0 与 EMS_MODE=1 提取，逐点偏差 < 0.1% 方为有效；导出运行中出现欠压降额事件 → 该剖面作废并标记；
- Baseline 终端 SOC 可能漂移 → 主比较口径为 **SOC 修正燃油**：

```text
fuel_base_corr = fuel_base + (SOC₀ − SOC_end) × Bat_Cap_kWh × k_eq
k_eq = bsfc_min / (η_rect × η_dcdc × η_bat) / 1000   [kg/kWh]
```

- 报告同时给 raw 与 corrected 两列；节油率 = (fuel_base_corr − fuel_dp) / fuel_base_corr。

## 6. 主矩阵与敏感性（FR-012）

主矩阵：3（d25 档，**一维定速曲线**）× 4（P_min_stable ∈ {0, 30, 40, 50}% × 单机 500 → 聚合 {0, 300, 400, 500} kW）× 2（口径）× 8（场景）= **192 个 DP 实例**。二维 map 分析为附加扩展项（conditional sensitivity），单独输出、不参与判定。
单因素（中档 d25=+50%、P_min=40%、连续口径）：R_gen ∈ {20, 40, 80} kW/s；Bat_Cap ∈ {643, 1071.36, 1500} kWh；C_start ∈ {1, 2, 5} kg（启停口径）。
每个实例输出：fuel_dp、fuel_base_corr、saving_pct、SOC 轨迹摘要、最优 P_g 轨迹、on 轨迹（启停口径）、运行时间。
汇总 `runs.csv`（每实例一行，含全部配置参数列）——入库；`runs/*.mat` 轨迹详情——大文件不入库。

## 7. Gate 判定逻辑（FR-013）

`35_phase0_gate_judge.m` 读取 runs.csv → `gate_result.json`：

```json
{
  "overall": "GO" | "NO-GO" | "DECISION_REQUIRED",
  "criteria": {
    "tier": "ROBUST" | "CONDITIONAL" | "NONE",
    "robust_ge5pct_scenarios": <int>,
    "mid_ge5pct_scenarios": <int>,
    "all_configs_lt_2pct": <bool>,
    "nontrivial_check_pass": <bool>
  },
  "per_scenario": {...},
  "config_hash": "...",
  "generated": "..."
}
```

判定映射（brainstorm D7 + 外部评审三档稳健性修正）：连续口径**保守档（+30%）** ≥4/8 场景 saving ≥5% 且非平凡 → `GO_ROBUST`；三档全配置连续口径 <2% → `NO-GO`（措辞限定聚合单机口径，注明双机自由度未探索）；其余（含仅中/高档达标 → tier=CONDITIONAL）→ `DECISION_REQUIRED` 交用户。**gate 只产出证据，不触发后续施工。**

## Testing Strategy

| 层级 | 内容 | 对应 FR |
|---|---|---|
| 单元 | BSFC 自检（锚点/单调/无过冲） | FR-005 |
| 单元 | DP 玩具问题：手工可解的两步问题，验证 DP 给出理论最优 | FR-008 |
| 集成 | DP ≤ Baseline 全场景校验；能量闭合 | FR-010 |
| 集成 | S07 网格加密收敛 <0.5% | FR-011 |
| 复现 | 同 config 两次运行 runs.csv 逐位一致（FR-016） | FR-016 |
| 门禁 | modelD smoke overall=true（FR-003）；gate_result.json schema 校验 | FR-003/013 |
| 因果隔离 | P_demand 双控制律不变性逐点 <0.1%；欠压降额事件断言 | FR-006 |

## 9. 已知局限（报告必载）

1. BSFC 为替代图（双真实锚点 + 假定形状），定量结论以三档敏感性呈现；
2. 聚合单机模型（用户裁决保留，brainstorm D8）：启停口径低估双机粒度灵活性；GO 方向保守，但 NO-GO 须限定“聚合口径”措辞（假 NO-GO 风险已登记；未来扩展：N_on ∈ {0,1,2} 机组群最小燃耗函数）；
3. 转速准静态、无转速爬坡约束；
4. 爬坡率 20 kW/s×2、τ=5 s、电池效率 0.95 仍为假设值（待设备方确认）；
5. DP 为全知离线最优，是上限而非可达值；Phase 1 ECMS 可达比例另作评估。

## Requirements Traceability

| FR | 设计章节 |
|---|---|
| FR-001~003 | §2 |
| FR-004/005 | §3 |
| FR-006/007 | §5 |
| FR-008~011 | §4 |
| FR-012 | §6 |
| FR-013 | §7 |
| FR-014 | §1（36_make_phase0_report.m + 报告结构） |
| FR-015~017 | §1（脚本编号与产物策略）、§8 |
| NFR-001~004 | §2（冻结只读）、§7（机器可读）、§6（规模预估）、§9（假设登记） |

## 11. 未决项（不阻塞 tasks 起草）

- OQ-1（变流器类型）：用户线下核实；主判定已不依赖二维 map，该问题仅影响扩展分析是否可升级；
- OQ-3/OQ-4 初值已在 §3.2/§4.2 给出，实施中如需调整走 config，不改设计；
- 脚本编号段（30 起）需在实施 T-001 时核对 modelD 副本内既有编号，避让占用。

## Design Amendment Log

- v1.1（2026-08-24）：采纳 GPT 外部评审 8 项修正（原文存档 `reviews/gpt-design-review-20260824.md`）——(1) 二维 map 退出主 Gate 降为扩展分析；(2) DP/Baseline 同物理可行域原则（条件校验）；(3) 终端 SOC 对称带 |ΔSOC|≤0.005；(4) P_demand 因果隔离 + 双控制律不变性验证；(5) 电池充放电符号约定写死；(6) Gate 三档稳健性分级（GO_ROBUST / DECISION_REQUIRED / NO-GO）；(7) 427 g/kWh 锚点来源存档要求（FR-018）；(8) Phase 2 不预设负荷预测（先状态反馈自适应等效因子，预测需消融证明）。评审第 1 条（双机粒度）经用户裁决本期不讨论、保持聚合（brainstorm D8）。
- v1.2（2026-08-24，T-008 实施修正，主 Agent 批准）：终端软不可行惩罚 big-M 由 1e6 调整为 **1e4**（仍为最大燃油 ~100 kg 的 100 倍，惩罚意图不变）。原因：基网格下充电受限段价值函数存在亚网格宽度“尖谷”，谷心随时间漂移离网，网格行锚定的后向 V 被 1e6 级软墙斜率放大为系统性高估（V0 虚高 360 kg）；降 big-M 后 V0 偏差 −4.6 kg，前向轨迹燃油不变（82.05 kg），且由 T-012 收敛检查（δSOC=0.0025，偏差 0.14% < 0.5%）形式化兜底。轨迹贪心在真实离网 SOC 上求值，排序正确性不受网格锚定影响。
