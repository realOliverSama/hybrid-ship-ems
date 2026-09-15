# Phase 4-F T-445 Trajectory Oracle Gate Requirements

Spec: `phase4-f-trajectory-oracle-gate`  
Status: requirements-draft  
Created: 2026-09-15  
Brainstorm: `./brainstorm.md`（T-445 Q1~Q4 全部 CLOSED / APPROVED，2026-09-15）

## Overview

T-445 是 Phase 4 工作包 F（预测式先进 EMS，roadmap §7 T-445~T-455）的**第一道 Gate**：在 Model E 仿真台上、**不用 NN**，直接给控制器真值未来负荷轨迹 P_future,true，量化「知道完整未来 30-bin 轨迹」相对「当前 E_300 scalar 预测」在**冻结 ECMS 控制族**内的开发期 oracle 燃油余量，产出机器可读的 **GO_TRAJECTORY_VALUE / NO_GO_TRAJECTORY_VALUE / INCONCLUSIVE** 裁决。**上限不存在则停止该方向**（roadmap 原纪律）；GO 才进入 T-446 预测式 EMS 架构设计。

科学问题的精确表述（Q1 裁决）：未来信息**维度**（perfect scalar vs perfect trajectory）的增量控制价值。预测器误差从正式 Gate 比较中剔除。

证据等级（Q4 裁决）：T-445 结果是 `DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE`，**不是**最终控制泛化证据；IID test split 保持封印，留给 T-452 终端控制器评估。

本文件只把已批准的 Q1~Q4 裁决翻译为 FR/NFR/AC，不新增科学裁决；design 期待定参数以 bounded obligation 形式集中在「Open Questions / 不得冻结事项」节列出。

## Scope

### In Scope

- 四臂 oracle 对比实验（B0/B1/B2/B3）在 Model E 仿真台的实现、执行与机器裁决；
- B2（trajectory-oracle optimized ECMS reference）的滚动视界优化器（离线慢算允许）；
- B3（离线全航程优化参照）的适配与 parity 资格化（未资格化则仅 context）；
- 航次语料选择（dataset v3 **validation** split，36 航次/9 split_group + 12 航次预留）；
- J_FEC 指标体系、k_eq^E 重算与效率边界审计、可行性/服务 parity 审计、干预审计、运行负担代理；
- 机器可读 Gate JSON + 统计推断（分层配对 split_group cluster bootstrap）+ 一次性扩样政策；
- 可选诊断臂（law=7 LEGACY_PRACTICAL_DIAGNOSTIC、HEURISTIC_TRAJECTORY_DIAGNOSTIC），均不入 GO/NO-GO。

### Out of Scope

- 任何 NN 入环（REAT-T/MLP-Q 接入控制器 = T-450+，本 spec 不消费 E 段 winner 模型）；
- 在线/实时 MPC 控制器施工（T-447~T-449 是后续工作包内容；B2 是**离线 oracle 参照**，非实时控制器）；
- OOD 鲁棒矩阵（T-453）、实时 profiling（T-454）、Paper Gate（T-455）；
- E-additional 候选（DLinear/TSMixer/TimeXer/K-only 臂）复训——待 Oracle Gate 后用户裁决；
- `predictor_runtime_period_s` 冻结（T-450~T-451 阶段事项）；
- dataset v3 任何字节改动（只读引用）。

## 术语与臂定义（锁定，Q1/Q2）

| 臂 | 定义 | 信息 | 控制 |
|---|---|---|---|
| **B0** | 现役 law=5 SOC-Adaptive ECMS | 无预测 | 冻结 ECMS 族 |
| **B1** | 完美 E_300 scalar oracle（law=6，E300_true(t0)=(10/3600)·Σ_{k=1..30} P_future_true,k(t0)，由 B2 同一轨迹派生） | 完美 scalar | 冻结 scalar ECMS 接口 |
| **B2** | trajectory-oracle optimized ECMS reference（Route B′） | 完美 30-bin 轨迹（滚动 300 s 窗） | 冻结 ECMS 执行族，仅经 Δλ_traj 监督变量 |
| **B3** | 离线全航程优化参照（直接功率 DP 类） | 全航程真值 | 理想化优化（parity 资格后审） |

- B1 规范表述：**「冻结 scalar ECMS 接口/控制架构下的 scalar-oracle 上界」**；任何文档不得称其为无条件"理论上界"（Q1）。
- B2 规范表述：`trajectory-oracle optimized ECMS reference` / `trajectory-oracle ECMS headroom reference`；除非后续 design 证明其在冻结容许 ECMS 监督策略空间上的全局最优性，否则不得称严格数学上界（Q2）。
- Headroom 号约定（正 = 可利用余量）：`H_scalar = J_FEC,B0 − J_FEC,B1`；`H_traj = J_FEC,B1 − J_FEC,B2`；`H_traj_pct = H_traj/J_FEC,B1×100%`；`H_arch = J_FEC,B1 − J_FEC,B3`。H_traj 释义 = 相对冻结 scalar-oracle ECMS 基准的轨迹 oracle 可利用余量，**不**释义为纯因果信息维度效应（B1/B2 信息→λ 映射不同，Q2）。

## User Stories

- 作为**论文作者**，我需要 T-445 给出机器可读的 GO/NO-GO 裁决与可复现证据链，使"轨迹信息 vs scalar 信息的控制价值"这一论断在答辩中可辩护；
- 作为**后续 T-446/T-447 设计者**，我需要 H_traj（同架构轨迹增量）与 H_arch（全航程优化天花板）的分解，以决定预测式 EMS 上层应采用 SOC_ref/λ 调度还是 MPC；
- 作为**评审者**，我需要语料、统计、阈值全部预注册且 test 封印未被触碰，使 Gate 结论无"结果导向"嫌疑。

## Functional Requirements

### 臂构造与 parity（Q1/Q2）

- **FR-720**: THE system SHALL provide B0 = Model E 现役 law=5 SOC-Adaptive ECMS 变体，控制律零修改。
- **FR-721**: THE system SHALL provide B1 = law=6 完美 scalar oracle，其标量真值 SHALL 由 B2 所用同一 oracle 轨迹派生：E300_true(t0) = (10/3600)·Σ_{k=1..30} P_future_true,k(t0)。
- **FR-722**: B1 在全部产出文档中的表述 SHALL 为「冻结 scalar ECMS 接口/控制架构下的 scalar-oracle 上界」，SHALL NOT 称无条件理论上界。
- **FR-723**: B2 SHALL 仅以 ECMS 监督变量 Δλ_traj 作用于冻结结构 λ_B2(t) = λ0 + K_soc·(SOC_ref − SOC(t)) + Δλ_traj*(t)；下游执行链（ECMS 瞬时 argmin → Commitment/PMS → Safety → Plant）SHALL 保持冻结。
- **FR-724**: B2 SHALL NOT 直接指挥 P_gen、P_bat、机组 commitment/启停、或直接功率调度轨迹（否则越出 ECMS 执行族、塌缩向 B3/MPC）。
- **FR-725**: B1 与 B2 SHALL 共享：oracle 真值源、t0 集合、300 s 视界语义、刷新节奏、航程末端窗口政策、Plant、燃油模型、电池模型、机组/PMS/Safety 约束、执行网格/作用限值。
- **FR-726**: IF law=7/MLP-Q 被运行，THEN 其产物 SHALL 仅标记 `LEGACY_PRACTICAL_DIAGNOSTIC` 且 SHALL NOT 进入 GO/NO-GO 逻辑。
- **FR-727**: IF 简单轨迹启发式臂（SOC_ref/λ heuristic）被运行，THEN 其产物 SHALL 仅标记 `HEURISTIC_TRAJECTORY_DIAGNOSTIC`（用于测量"不经优化可获得 B2 余量的几何"）且 SHALL NOT 进入 GO/NO-GO 逻辑。
- **FR-728**: 全部 oracle 臂产物 SHALL 携带 E5 类红线声明：oracle 前瞻非实时可实现，仅供上界/增量验证。

### B2 滚动视界纪律（Q2 Route B′）

- **FR-730**: AT 每个合法 oracle 刷新时刻 t0，B2 SHALL：(1) 使用 B2 自身当前闭环状态；(2) 仅暴露 [t0, t0+300 s] 合法 oracle 轨迹；(3) 优化视界内容许 Δλ 修正序列；(4) 只执行首个修正/控制区间；(5) 推进真实冻结 Model-E/PMS/Safety plant；(6) 在下一刷新时刻重解。
- **FR-731**: B2 SHALL NOT 使用超出合法 300 s 视界的信息预计算/回放全航程 λ 序列。
- **FR-732**: B2 允许离线/慢速计算（oracle 参照，非实时控制器）。
- **FR-733**: 刷新节奏与末端窗口机制的数值属 design 阶段义务；IF 选择 10 s，THEN B1 与 B2 SHALL 使用同一 10 s sample-and-hold oracle 更新语义。
- **FR-734**: 既有 `+ecms/dp_*` 工具 MAY 复用于燃油模型、SOC 转移、可行性约束、终端 parity、网格/插值/检查与通用优化机械件；直接优化发电机/电池功率的旧 DP SHALL NOT 自动成为 B2（直接调度/全航程优化属于 B3）。
- **FR-735**: B2 SHALL 包含冻结的终端能量/charge-sustaining 机制（终端 SOC 约束带 / 终端罚 / 终端价值近似，design 选定并冻结），使 B2 无法通过向滚动视界外输出 SOC 欠账获得表观省油。

### 指标与记账（Q3）

- **FR-740**: THE formal fuel-comparison metric SHALL be J_fuel := J_FEC；对臂 i：J_FEC,i = fuel_raw,i + (SOC0 − SOC_end,i)·Bat_Cap_kWh·k_eq^E，其中 Bat_Cap_kWh = 1071.36 kWh；fuel_raw 已含 Model E 启机燃油项且 SHALL 保持不改动。
- **FR-741**: k_eq^E SHALL 全局冻结、所有正式臂与所有航次共用；一般式 k_eq^E = (BSFC_ref^E/1000)/η_path^E [kg/kWh]；SHALL NOT 默认使用五 BSFC 锚点算术均值，除非该加权经论证并预注册；可接受的 design 期构造：连续工作区间加权 / 能量加权参考工况分布 / canonical 工作点或参考区域定义。
- **FR-742**: 冻结 k_eq^E 前 SHALL 显式审计 Model E 能量边界：确定哪些损耗已被电池/SOC 模型表达，仅计入适用项（整流/DC-DC/充电效率），SHALL NOT 机械沿用 Model D 电池效率因子；design SHALL 包含 `NO_DOUBLE_COUNTING_OF_ENERGY_LOSS` 检查。
- **FR-743**: raw 物理量 SHALL 与 FEC 永远并列报告：fuel_raw、SOC0、SOC_end、ΔSOC、电池能量修正量、J_FEC。
- **FR-744**: 在 FEC 排名之前，每个正式 run SHALL 满足统一预注册的 `TERMINAL_SOC_ADMISSIBILITY` 政策（容差带 design 期冻结，且必须先于观察任何 B1/B2 结果）；越限 run SHALL 判 `INFEASIBLE_OR_NONCOMPARABLE`，FEC SHALL NOT 用于"拯救"越限 run。
- **FR-745**: k_eq 稳健性 SHALL 强制报告：canonical k_eq^E、0.8×、1.2×、H_traj 与 H_arch 的符号稳定性；WHEN SOC 系数非零，SHALL 计算解析 break-even 等效燃油系数 k_eq_break_even = −(fuel_raw_B1 − fuel_raw_B2)/(Bat_Cap_kWh·(SOC_end_B2 − SOC_end_B1)) 并报告 canonical k_eq^E 距其距离；IF 终端 SOC 实质相等，THEN 报告 `K_EQ_INSENSITIVE_BY_SOC_PARITY`。
- **FR-746**: 每个正式臂 SHALL 报告并通过统一任务/物理可比性审计（可及项）：任务/航线完成、航次时长/到达一致性、速度参考跟踪 RMS 与最大误差、欠供负荷/甩负荷/推进需求缺口、SOC 硬界越限时间、电池 SOP/变换器/电压/电流硬限越限、机组 Pmin/Pmax/ramp 硬限越限、终端 SOC 容许性、母线/Plant 能量闭合残差；通过服务降级或硬约束越限获得的燃油"收益"SHALL 判无效。
- **FR-747**: 控制干预审计 SHALL 报告：Safety S1–S5 触发计数与（可及时）时长、PMS override 计数与（可及时）时长。
- **FR-748**: 运行负担诊断 SHALL 报告（仅代理，不得称已验证磨损/退化，除非后续批准范围引入已验证退化模型）：机组启停计数、TV_Pgen = Σ|P_gen[t]−P_gen[t−1]|、ramp 限值激活计数/时长、（可得时）ON-hours/低载运行时间；电池 E_bat_throughput = ∫|P_bat(t)|dt、EFC_proxy = E_bat_throughput/(2·Bat_Cap_kWh)。T-445 正式 Gate 目标保持 J_fuel = FEC，SHALL NOT 引入 fuel+α·battery_wear+β·genset_wear 类复合目标。
- **FR-749**: WHEN B3 被资格化，其 SHALL 使用与 B0/B1/B2 相同的 raw-fuel 定义、终端 SOC 记账、冻结 k_eq^E、FEC 公式、任务/可行性审计与运行负担报告，然后 H_arch 方可使用。

### 语料（Q4）

- **FR-760**: T-445 语料 SHALL 取自 Dataset-v3 **validation** split；SHALL NOT 拆封 IID test；SHALL NOT 使用正式 OOD 终评证据调参/选择 T-445 控制器设计；T-445 全部结果 SHALL 标记 `DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE`。
- **FR-761**: 初始语料 SHALL 冻结为 36 航次 = 9 个完整 split_group（不得使用 24 航次/6 组方案）；独立推断单元为 split_group（有效初始样本量 = 9 簇），报告 SHALL 携带 `SMALL_N_GATE_CAUTION`。
- **FR-762**: 波动分层 SHALL 满足：在生成任何 B0/B1/B2/B3 结果之前，(1) 仅用冻结负荷/mission 真值计算一个预注册、臂独立的负荷波动评分；(2) 按预注册规则聚合航次评分至 split_group；(3) 对 27 个 validation split_group 排名并等分为 LOW/MID/HIGH 各 9 组；评分中 SHALL NOT 使用控制器输出、燃油、SOC、headroom 或任何 B 臂结果；评分数学定义 design 期冻结且先于语料选择。
- **FR-763**: 语料选择 SHALL 确定性：冻结种子 `phase4f-t445-corpus-v1`；每层内按 SHA256 派生稳定排序；初始集 = LOW/MID/HIGH 各前 3 组（9 组 36 航次）；一次性预留集 = 各层次 1 组（3 组 12 航次）；SHALL NOT 结果导向替换预留组。
- **FR-764**: 语料清单（航次 ID、分组、层级、排序哈希）SHALL 在任何正式臂执行前冻结并以机器可读 manifest 落盘。

### 统计推断与 Gate 裁决（Q4）

- **FR-770**: 对每个通过冻结可行性/parity 协议的航次 v，SHALL 计算 h_v = 100·(J_FEC,B1(v) − J_FEC,B2(v))/J_FEC,B1(v)，并保持 B1/B2 完全配对。
- **FR-771**: 组内聚合 h_g = mean_{v∈g}(h_v)；正式推断单元 = split_group；SHALL 至少报告：split_group median h_g、macro mean、IQR/min/max、正组比例、LOW/MID/HIGH 分层汇总；主 Gate 点估计 = median_g(h_g)。
- **FR-772**: 推断 SHALL 使用分层配对 split_group cluster bootstrap：B=10000、冻结种子；每个 replicate 独立有放回重抽 LOW 3 组/MID 3 组/HIGH 3 组（扩样后各 4 组），保持组完整（含全部家族航次与配对 B1/B2 结果）；计算主统计量双侧 95% percentile CI；SHALL NOT：bootstrap 单窗口、以单航次为独立单元、破坏 split_group 家族闭包、B1/B2 独立重抽。
- **FR-773**: 预注册阈值 SHALL 冻结为 X = 2.0%（最小实用 oracle 余量目标）、Y = 0%（方向性证据底）；二者为项目预注册阈值，非普适文献常数。
- **FR-774**: WHEN median_g(h_g) ≥ 2.0% AND bootstrap 95% CI 下界 > 0% AND 全部正式可行性/parity 硬检通过，THE gate SHALL 判 `GO_TRAJECTORY_VALUE`；释义：oracle 轨迹臂显示足够开发期余量，支持进入预测控制器施工；SHALL NOT 声称在线学习控制器可达 ≥2%。
- **FR-775**: WHEN bootstrap 95% CI 上界 < 2.0% 且 Gate 其余有效，THE gate SHALL 判 `NO_GO_TRAJECTORY_VALUE`；释义：即使完美轨迹 oracle ECMS 参照的可信上限也未达预注册实用目标（B2 为乐观 oracle 参照，构成更强轨迹感知控制器开发的强 futility 信号）。
- **FR-776**: 其余有效情形 SHALL 判 `INCONCLUSIVE`，且仅可触发一次预注册扩样：启用冻结预留集（+3 组/+12 航次，每层各 1 组），对合并后 48 航次/12 组全量语料用**完全相同**的统计、bootstrap、阈值、可行性规则与代码重算；SHALL NOT 合并两个独立 CI/检验；不允许第二次扩样；扩样后仍不定 SHALL 判 `FINAL_INCONCLUSIVE / DECISION_REQUIRED` 并停止自动 Gate 流程；SHALL NOT 持续采样直至出现期望结果。
- **FR-777**: 机械/基建失败 MAY 在相同冻结臂/航次/配置身份下重跑并记 incident 记录；持续控制器/物理不可行（终端 SOC 不容许、任务/服务失败、硬物理/安全越限、持续优化器不可行）SHALL NOT 成对删除；design SHALL 冻结正式处理规则，默认期望 = 0 个持续正式 B1/B2 不可行 run；否则 SHALL 判 `GATE_VALIDITY_FAIL / AUDIT_REQUIRED`，不得静默缩小统计语料。
- **FR-778**: WHEN B3 经 parity 资格化，SHALL 满足数值一致性期望 J_FEC,B3 ≤ J_FEC,B2 + ε_num（等价 H_arch ≥ H_traj − ε_num；ε_num design 冻结）；实质违反 SHALL 判 `B3_DOMINANCE_AUDIT_FAIL` 并要求 solver/parity/模型审计，SHALL NOT 未经审计把 B3/B2 倒挂释读为轨迹科学优越性；IF B3 未资格化，THEN 报告 `H_arch = UNQUALIFIED_CONTEXT` 且 B3 SHALL NOT 影响 B1-vs-B2 Gate。
- **FR-779**: SHALL 同时报告非 Gate context 分解链：H_scalar = J_FEC,B0 − J_FEC,B1（scalar-oracle 价值）、H_traj（轨迹 oracle 可利用余量）、（B3 资格化时）H_arch（更广架构/全局优化余量，B2→B3 残差段）。

### 机器可读 Gate 与可复现性

- **FR-780**: Gate 结果 SHALL 为机器可读 JSON，至少含：verdict 枚举（GO_TRAJECTORY_VALUE / NO_GO_TRAJECTORY_VALUE / INCONCLUSIVE / FINAL_INCONCLUSIVE / GATE_VALIDITY_FAIL / B3_DOMINANCE_AUDIT_FAIL）、主统计量与 CI、分层汇总、语料 manifest 引用、配置哈希、代码 commit、全部种子、可行性审计汇总；Gate FAIL/非 GO 状态 SHALL 阻止后续依赖该 Gate 的提交（沿袭项目机器可读 Gate 纪律）。
- **FR-781**: 全部正式 run SHALL 可复现：冻结配置、冻结种子、冻结航次清单、代码 commit 钉版于结果 manifest；oracle 真值派生链（E300_true ← B2 轨迹）SHALL 可机器复验。
- **FR-782**: 正式结果观察顺序 SHALL 受控：TERMINAL_SOC_ADMISSIBILITY 容差带、k_eq^E、语料、阈值、统计代码均 SHALL 先于首次 B1/B2 正式结果观察冻结（预注册纪律）。

## Non-Functional Requirements

- **NFR-710**: Model E 全部操作 SHALL 脚本驱动；SHALL NOT 手工 GUI 修改模型形成不可复现状态；模型结构变更 SHALL 经 builder 脚本并做结构差异检查。
- **NFR-711**: dataset v3 零字节改动：`datasets_v3/`、`results/dataset_v3/` 只读；T-445 派生产物只写本 spec 授权的新目录。
- **NFR-712**: 执行环境 = Mac 本机 MATLAB（R2026a）+ 既有 `.venv_tf`；SHALL NOT 依赖云 GPU/AutoDL。
- **NFR-713**: E 段成果（winner 模型工件、input_contract、指标栈）本 spec 内只读且不消费；E-additional 候选不得擅自开工。
- **NFR-714**: 全语料（含一次性扩样预案）在 Mac 本机可隔夜完成；design SHALL 给出运行预算估算，执行后报告实际 wall time。
- **NFR-715**: 仓库内可执行脚本 SHALL NOT 硬编码绝对路径（AGENTS.md §20.2：MATLAB 用 mfilename 推导 + pwd 兜底）。
- **NFR-716**: Git 纪律沿袭 AGENTS.md §7/§8；Gate 非 GO 不允许提交依赖其通过的后续施工；本 spec 施工期提交遵循 feat/phase4-f 分支纪律。

## Acceptance Criteria

- [ ] **AC-610**（FR-721/725/781）：parity 不变量机器校验通过——B1 标量真值与 B2 轨迹派生值逐 t0 一致（数值容差内）、刷新节奏/视界/末端处理一致，校验报告落盘。
- [ ] **AC-611**（FR-723/724）：B2 作用边界结构测试通过——静态/运行时断言 B2 仅写 Δλ_traj 通道，无任何直达 P_gen/P_bat/commitment 的信号路径。
- [ ] **AC-612**（FR-730/731）：B2 信息合法性测试通过——审计日志证明每个 t0 优化器输入仅含 [t0, t0+300 s] 真值，无超窗未来泄漏。
- [ ] **AC-613**（FR-735）：终端能量纪律反作弊测试通过——构造"放空电池可省油"的合成反例场景，B2 不得利用（终端机制生效证据落盘）。
- [ ] **AC-614**（FR-740~742/745）：k_eq^E 冻结记录 + 效率边界审计 + NO_DOUBLE_COUNTING 检查通过；稳健性表（0.8×/1.2×/break-even 距离/K_EQ_INSENSITIVE 条款）出现在正式报告。
- [ ] **AC-615**（FR-760~764）：语料 manifest 冻结且可由种子机器复现；波动评分臂独立性证明；dataset v3 manifest 复核 test_sealed=true 未被触碰。
- [ ] **AC-616**（FR-770~772）：统计代码单测通过——合成已知效应数据上的配对/族闭包/分层重抽正确性；bootstrap B=10000 复现性（同种子同结果）。
- [ ] **AC-617**（FR-774~777/780）：Gate 裁决逻辑单测覆盖全部分支（GO/NO-GO/INCONCLUSIVE→扩样/FINAL_INCONCLUSIVE/GATE_VALIDITY_FAIL/B3_DOMINANCE_AUDIT_FAIL）；Gate JSON schema 校验通过。
- [ ] **AC-618**（FR-720/721）：B0/B1 锚点回归——参考航次上 B0/B1 行为与既有 law=5/law=6 基线一致（容差内），锚定 Phase A2/B 基线。
- [ ] **AC-619**（FR-746~748）：可行性/服务 parity 审计、干预审计、运行负担代理在每个正式臂×航次报告中齐全。
- [ ] **AC-620**（FR-779/780）：最终报告含 H_scalar/H_traj/H_arch context 分解、机器可读 Gate JSON、SMALL_N_GATE_CAUTION 与证据等级声明（DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE）。

## Edge Cases

- **优化器单点不可行**：单个 t0 窗内优化不可行的回退政策（如 Δλ_traj≡0 回退 + 计数）属 design 期义务；持续不可行走 FR-777（GATE_VALIDITY_FAIL），不得静默剔除。
- **航程末端窗口不足 300 s**：末端收缩政策 design 期冻结，B1/B2 必须同政策（FR-725/733）。
- **整组航次不可行**：某 split_group 全部航次未过可行性协议时的组级处理规则 design 期冻结（默认按 FR-777 判 GATE_VALIDITY_FAIL 方向，而非缩小语料）。
- **B3 未资格化**：H_arch = UNQUALIFIED_CONTEXT，Gate 仅由 B1/B2 决定（FR-778）。
- **航次时长边界**：语料航次若短于 history+horizon 覆盖需求，按 dataset v3 既有 eligibility 纪律处理（预注册，不临时发明）。

## Dependencies and Constraints

- Model E 冻结基线（Phase 4-A2 施工 + Phase 4-B PMS v2/Safety，控制基线 v1.0）——只读引用，B0/B1 复用现役 law=5/law=6 变体；
- `+ecms/dp_*` 离线工具链（Phase 1 遗产，FR-734 界定的复用边界）；
- Dataset v3 formal freeze（split 指派工件 ac4c4a76…；test_sealed=true）与 fleet_v3/mission_v3 航次工件；
- Mac 本机 MATLAB R2026a + `.venv_tf`（统计/后处理可用 Python）；
- E 段成果只读（本 spec 不消费）；roadmap §7 工作包 F 编号 T-445 对应本 spec；
- Git：feat/phase4-f 分支；机器可读 Gate 为最终工程门禁依据。

## Open Questions（design 期 bounded obligations——本门不冻结，design 批准前必须全部闭合）

- [ ] **OQ-F1**：刷新节奏数值（当前优选 10 s sample-and-hold）与航程末端窗口收缩机制（FR-733）；
- [ ] **OQ-F2**：Δλ_traj 容许集合（界/离散化/速率限制）与窗内优化问题形式（决策变量、约束、目标）（FR-723/730）；
- [ ] **OQ-F3**：B2 终端能量机制选型（终端 SOC 带/罚/价值近似）与参数（FR-735）；
- [ ] **OQ-F4**：`TERMINAL_SOC_ADMISSIBILITY` 容差带数值（FR-744）；
- [ ] **OQ-F5**：BSFC_ref^E 加权定义与 η_path^E 项集（含效率边界审计结论）（FR-741/742）；
- [ ] **OQ-F6**：负荷波动评分数学定义与航次→split_group 聚合规则（FR-762）；
- [ ] **OQ-F7**：ε_num（B3 dominance 容差，FR-778）与 B3 parity 资格化方法（FR-749）；
- [ ] **OQ-F8**：B2 窗内求解器构造（窗内 DP / 凸优化 / 其他）与单 t0 不可行回退政策；
- [ ] **OQ-F9**：B2 与 Simulink 闭环的交替推进机制（分段 sim 状态保存/恢复 vs 共仿真）与运行预算（NFR-714）。
