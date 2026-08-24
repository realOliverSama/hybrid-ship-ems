# 边缘AI自适应ECMS甲醇混合动力船舶EMS — Requirements

Spec: `adaptive-ecms-methanol`
Status: requirements-approved (v1.1，2026-08-24 用户批准)
Created: 2026-08-24
Brainstorm: `./brainstorm.md`

## Overview

研究方向：基于边缘 AI 自适应等效燃料消耗最小化（A-ECMS）的甲醇混合动力船舶能量管理策略。利用动力电池作为能量缓冲，使甲醇发电机组尽量运行在 BSFC 高效率区；直流母排下机组转速与推进负荷解耦；二维可变速 BSFC map 仅作扩展分析，主判定不依赖（外部评审修正）。后续阶段**不预设负荷预测**：Phase 2 首先研究基于状态反馈（SOC、负荷、工况等）的自适应等效因子；仅当 Fixed/A-ECMS 与 DP 上限存在明显差距、且消融实验证明未来信息能缩小差距时，才引入短时预测（承接前两轮 NO-GO 教训，外部评审修正）。

**本 spec 的实施范围 = Phase 0：离线收益上限验证。**

前序两条研究线（timed-feedforward / multistep-predictive）均因"收益上限在机理层面不存在"而 NO-GO，且都是在控制器施工完成后才发现。本方向的第一纪律（来自 brainstorm D1）：

> **先用离线最优（DP）验证节油收益上限存在，再投入任何控制器施工。**

Phase 0 的全部产出是：机器可读的 GO/NO-GO 证据（JSON + 报告）。ECMS 控制器设计（Phase 1）、预测增强（Phase 2）、STM32N6 边缘部署（Phase 3）均不在本 spec 实施范围内，仅在本文档中作为后续阶段登记。

## Scope

### In Scope

- 新建独立工程副本 `model/simulink_modelD/`（自 `model/simulink_modelC/` 复制），并按设备真实参数（`data/20260526设备参数.md`，源自 `20260526设备参数.docx`）完成参数修正；
- BSFC 曲线族构造：文献典型中速甲醇机曲线形状 + 双真实锚点标定（最优 427 g/kWh / 额定 453(+5%) g/kWh @ 500 kW·1500 rpm 单机）+ 低负荷恶化三档敏感性；
- 各场景直流母排需求负荷剖面 P_demand(t) 的提取（用修正后模型跑 Baseline EMS 导出）；
- 离线动态规划（DP）燃油下限求解器：SOC 状态网格 × P_gen 控制网格 ×（二维时含转速网格），含爬坡、SOC 边界、最低稳定负荷、电池功率限制、电量维持终端约束；
- 两种运行口径：**连续运行**（机组常开）与**允许启停**（含启动代价参数）分别给出上限；
- 敏感性矩阵：BSFC 低负荷恶化档位 × 最低稳定负荷档位 ×（单因素：爬坡率、电池容量）；
- 机器可读 GO/NO-GO 判定（JSON）+ Phase 0 研究报告。

### Out of Scope

- ECMS/A-ECMS 控制器的设计与 Simulink 实现（Phase 1，待 GO 后另行立项）；
- 任何神经网络训练、预测模型选型（Phase 2）；
- STM32N6 部署与固件修改（Phase 3）；
- 冻结目录 `model/simulink/`、`model/simulink_modelC/`、`model/simulink_modelC_(oracle_ms)/` 的任何修改；
- 双机组独立启停粒度（Phase 0 沿用聚合等效单机模型；双机粒度作为已知局限登记）；
- AC/DC 变流器类型硬件核实（用户线下行动项，不阻塞 Phase 0；若核实为定速，二维 map 退化为一维，代码路径须兼容）。

## User Stories

- As a 研究者（用户）, I want 在施工任何控制器之前得到 DP 离线节油上限的机器可读证据, so that 不再重蹈前两次"施工到底才发现上限不存在"的覆辙。
- As a 研究者, I want 上限结果附带对未知参数（低负荷 BSFC 形状、最低稳定负荷、爬坡率）的敏感性, so that 结论的可信边界清晰、论文审稿可辩护。
- As a 后续阶段的开发者, I want Phase 0 的负荷剖面、BSFC 曲线族、DP 求解器全部脚本化且可复现, so that Phase 1 ECMS 设计可直接复用这些基础设施。

## Functional Requirements

### 工程副本与参数修正

- **FR-001**: THE 施工 SHALL 新建 `model/simulink_modelD/` 独立工程副本（复制自冻结的 `model/simulink_modelC/`，排除 `.venv_tf`/`slprj`/`results` 等大体积产物，符号链接或路径重指按前例处理），副本内文件命名与内部引用统一改为 Model D 标识，不得对冻结目录产生任何写入。
- **FR-002**: WHEN 建立 modelD 参数表时, THE 副本 SHALL 完成以下源自设备文件的真实参数修正，且每项修正在参数元数据（ParamMeta）中登记来源为 `DRAWING: 20260526设备参数.docx`、置信度 `high`：
  - `Bat_Cap_kWh`：1200 → **1071.36**（535.68 kWh/侧 × 2，576 V × 930 Ah）；
  - `ASSUMED_Bat_DischargeLimit` / `ASSUMED_Bat_ChargeLimit`：1000 → **1050** kW（DC/DC 瓶颈 750 V × 700 A = 525 kW/侧 × 2；电池本体 ≥1000 A × 576 V = 576 kW/侧 非瓶颈），参数名去 ASSUMED 前缀或保留但置信度升 high；
  - BSFC 额定锚点：1000 kW 聚合点 = **453** g/kWh（替代现表 460；容差 +5% 即上限 475.7 作为敏感性上界）。
- **FR-003**: WHEN 参数修正完成后, THE modelD 工程 SHALL 通过自身 smoke 门禁（`03_run_smoke_tests.m` 等效物，overall=true）。修正导致原 gate 基线失效时，允许在 modelD 副本内重建 `gate_baseline_smoke.json`；基线重建属 🟡 操作，须用户批准后执行并在报告中声明。

### BSFC 曲线族

- **FR-004**: THE 工程 SHALL 提供脚本化 BSFC 曲线族生成器，满足：
  - 双锚点标定：最优点 **427 g/kWh**（厂家标称最低消耗率）；额定点 **453 g/kWh @ 单机 500 kW / 1500 rpm**；
  - 一维曲线（1500 rpm 定速线，P ∈ [0, 500] kW 单机）与二维 map（转速 × 功率）两种输出，共用同一形状参数族；**主判定链路（FR-012/013）仅使用一维定速曲线；二维 map 仅用于附加扩展分析，不进入主 Gate**（外部评审修正：防止人为构造的最优转速线制造虚假收益；待真实机组转速范围/整流器形式/二维燃耗 map 获取后方可升级）；
  - 低负荷恶化为显式参数，档位 = 25% 负荷处相对最优点的恶化幅度 **{+30%, +50%, +70%}**；
  - 二维 map 的转速维度形状采用文献典型中速机 map 形状（最优转速带 + 偏离惩罚参数化），转速范围为设计参数（初值建议 900~1500 rpm，待 design 定案）；
  - 生成器输出机器可读文件（.mat/.csv）+ 可视化 PNG（一维曲线三档叠加图、二维 map 等高线图）。
- **FR-005**: THE BSFC 生成器 SHALL 通过合理性自检：曲线在工作区内单调性/光滑性检查、锚点处取值与标定值偏差 < 0.1%、低负荷档满足 恶化(25%) > 恶化(50%) ≥ 0。

### 负荷剖面提取

- **FR-006**: WHEN 提取需求负荷剖面时, THE 工程 SHALL 用 modelD + Baseline EMS（无预测、无 ECMS）对每个入选场景仿真，导出直流母排侧需求功率 P_demand(t)（推进 + 日用，重采样至 1 s 均匀网格），同时导出 Baseline 的 P_gen(t)、P_bat(t)、SOC(t)、累计燃油，作为 DP 的输入与基线参照。P_demand(t) SHALL 为**控制不变的外生需求**：提取点位于欠压降额与 EMS 作用之前（推进需求指令侧），不得使用已被 Vdc 降额影响的实际服务负荷；并 SHALL 通过在两种不同控制律（如 EMS_MODE=0 与 EMS_MODE=1）下重复提取、逐点偏差 < 0.1% 来验证不变性；导出运行中若发生欠压降额事件，该剖面作废并标记（外部评审修正，承接前次 oracle 因果污染教训）。
- **FR-007**: THE 入选场景集 SHALL 默认 = {S01, S02, S03, S04, S07, S08, S09, S10}（8 个；S05 与 S04 同型排除，S06 为鲁棒性专用阶跃排除），集合在 config 中可配置。

### DP 离线求解器

- **FR-008**: THE DP 求解器 SHALL 以场景 P_demand(t) 为外生输入，求全程甲醇消耗最小值，约束至少包括：
  - 功率平衡：P_gen·η_rect + P_bat·η_dcdc（放）= P_demand + P_bat/η_dcdc（充）（效率取值与模型一致：η_rect=η_dcdc=0.97，电池充放效率 0.95）；
  - 机组：0 ≤ P_gen ≤ 1000 kW（聚合），|ΔP_gen| ≤ R_gen·Δt，最低稳定负荷 P_min_stable（参数化档位 {0%, 30%, 40%, 50%} × 500 kW 单机 → 聚合 ×2；连续口径下 P_gen ∈ {0 若允许停} ∪ [P_min_stable, P_max]）；
  - 电池：|P_bat| ≤ 1050 kW，SOC ∈ [0.10, 0.95]，容量 1071.36 kWh；
  - 终端电量维持：**|SOC(T) − SOC(0)| ≤ 0.005**（对称硬约束；外部评审修正：下界写法允许 DP 借用 0.005×1071.36 ≈ 5.36 kWh 初始电量，在 2~5% 节油判定量级下不可接受）；
  - 决策步长 Δt = 1 s；SOC 网格、P_gen 网格、（二维时）转速网格分辨率为 config 参数。
- **FR-009**: THE DP 求解器 SHALL 支持两种运行口径：(a) 连续运行——机组全程开机；(b) 允许启停——增加开机动作与启动代价参数（启动燃油 + 等效磨损成本，初值与单位在 design 定案）。两口径分别输出上限。
- **FR-010**: THE DP 结果 SHALL 满足内置正确性校验：能量守恒残差检查（电量平衡闭合误差 < 0.1%）；"DP 燃油 ≤ 同场景 Baseline 燃油"校验 SHALL 以**同物理可行域**为前提——先验证 Baseline 轨迹在当前 DP 配置约束（最低稳定负荷、爬坡、电池限值）下可行：可行则强制该校验（违反即报错）；不可行（如 Baseline 运行于 P < P_min_stable）则跳过并在 runs.csv 标记 `baseline_infeasible_conservative`（此时 DP 约束强于 Baseline，节油率被低估，比较方向保守——外部评审修正）。
- **FR-011**: WHEN 进行数值收敛性验证时, THE 工程 SHALL 对至少 1 个代表场景（建议 S07）做网格加密复算（SOC 与 P_gen 网格加倍），全程燃油差异 < 0.5% 方为通过；不通过则加密基准网格并重跑主矩阵。

### 敏感性与判定

- **FR-012**: THE 敏感性主矩阵 SHALL = 3（BSFC 低负荷恶化档，**一维定速曲线**）× 4（最低稳定负荷档 {0,30,40,50}%）× 2（运行口径）× 8（场景）；爬坡率 {20, 40, 80} kW/s 聚合与电池容量 {1071.36, 643, 1500} kWh（约 ×0.6/×1.4）作单因素附加扫描（固定中档 BSFC + 40% 最低稳定负荷）；二维 map 分析作为附加扩展项单独输出，不混入主矩阵、不参与判定（外部评审修正）。矩阵规模在 config 可调。
- **FR-013**: THE 工程 SHALL 产出机器可读判定文件 `results/ecms_phase0/gate_result.json`，字段至少含：overall、per-scenario 两口径节油率、判据逐项结果、所用参数集哈希/摘要。判定逻辑（对应 brainstorm D7）：
  - **GO_ROBUST**：连续运行口径下，**保守档 BSFC（+30% 恶化）**即满足节油上限 ≥5% 的场景数 ≥ 4/8，且最优解非平凡（非仅由 SOC 端点效应产生——检查 SOC 轨迹不贴下限滑行）；
  - **NO-GO**：三档 BSFC 全部配置下连续口径节油上限均 < 2%；NO-GO 结论措辞 SHALL 限定为"聚合单机口径下无收益空间"，并注明双机独立调度（N_on ∈ {0,1,2}）为未探索自由度（brainstorm D8）；
  - 其余（含仅中/高档达标、2~5% 区间、结果跨档翻转）→ 标记 `DECISION_REQUIRED`，交用户裁决；判定字段 SHALL 含 `tier`（ROBUST/CONDITIONAL/NONE）以区分稳健性层级（外部评审修正：避免研究成立与否由单一人为选定的中档曲线决定）；
  - gate 脚本只产出证据与标记，**不自动触发任何后续施工**。
- **FR-014**: THE 工程 SHALL 生成 Phase 0 研究报告（`reports/ECMS_phase0_upper_bound.md`），内容至少：方法与假设、参数修正清单及来源、BSFC 曲线族图、主矩阵结果表、敏感性分析、GO/NO-GO 证据、已知局限（聚合单机近似、替代 BSFC 图、参数待设备方确认项）、对 Phase 1 的建议。

### 复现与工程纪律

- **FR-015**: THE 全部流程 SHALL 脚本驱动（MATLAB R2026a `-batch`），禁止 GUI 手工状态；入口脚本按 `3x_` 编号段延续 modelD 内既有编号体系，不得改写冻结目录任何脚本。
- **FR-016**: WHEN 复算验证时, THE 同一 config 下 Phase 0 主矩阵 SHALL 可复现：两次独立运行的 summary 指标逐位一致（或在文档化容差内）。
- **FR-017**: 大型 .mat 实验数据不入库；仅提交 runs.csv / summary.json / gate_result.json / 报告 / PNG。`.venv_tf`、`slprj` 等按 .gitignore 既有策略处理。
- **FR-018**: THE 工程 SHALL 在 `docs/` 登记 BSFC 双锚点的完整可回溯来源：453(+5%) g/kWh 来自 `20260526设备参数.docx`（FR-002 已登记）；427 g/kWh 来自潍柴 WH17ME 官方产品页（product.weichai.com，访问日期 2026-08-24）——登记 URL 与访问日期，施工时尽可能存档原始页面 PDF/截图，确保论文引用可回溯。

## Non-Functional Requirements

- **NFR-001**: 冻结目录（`model/simulink/`、`model/simulink_modelC/`、`model/simulink_modelC_(oracle_ms)/`）全程只读；任何对冻结路径的写入即视为 gate 失败。
- **NFR-002**: 所有结论以机器可读产物（JSON/CSV）为准，禁止以 Agent 主观判断替代 gate 输出。
- **NFR-003**: DP 主矩阵全量运行时间应控制在单机 4 小时以内（预估单个 DP ≤ 分钟级；超限须优化网格或场景子集并记录）。
- **NFR-004**: 所有沿用假设值（爬坡率 20 kW/s×2、τ=5 s、电池效率 0.95、SOC 降额曲线等）在报告中集中登记为"待设备方确认"清单，不得散落在代码注释中。

## Acceptance Criteria

- [ ] AC-1（FR-001~003）：`model/simulink_modelD/` 存在且独立可运行；参数修正逐项可在 ParamMeta 中溯源；modelD smoke 门禁 overall=true。
- [ ] AC-2（FR-004/005）：BSFC 生成器输出一维三档 + 二维 map 文件与 PNG；自检全过；锚点偏差 < 0.1%。
- [ ] AC-3（FR-006/007）：8 场景 P_demand(t) 与 Baseline 参照数据齐备，能量守恒闭合。
- [ ] AC-4（FR-008~010）：DP 两口径求解完成；全部场景满足 DP ≤ Baseline 校验；电量平衡闭合。
- [ ] AC-5（FR-011）：S07 网格加密复算燃油差 < 0.5%。
- [ ] AC-6（FR-012/013）：主矩阵 + 单因素扫描完成；`gate_result.json` 生成且判定字段完整。
- [ ] AC-7（FR-014）：Phase 0 报告入库，含 GO/NO-GO 证据与局限清单。
- [ ] AC-8（FR-015~017、NFR-001）：`git status` 中无冻结目录改动；无大体积产物入库；复算一致。
- [ ] AC-9（FR-006/018）：P_demand 双控制律不变性验证通过（逐点 < 0.1%）；BSFC 双锚点来源登记入库。

## Edge Cases

- **DP 不可行**（某场景总能量需求超出机组+电池可供）：输出 `E_unserved` 并标记该场景为 INFEASIBLE，不得静默放宽约束；S09 应急冲刺为重点关注场景。
- **终端 SOC 约束导致不可行**：报告该场景在给定初始 SOC 下无电量维持解，允许登记后以 SOC(T) ≥ SOC_min 重算并明确标注"非电量维持"口径。
- **转速网格退化**：当硬件核实为定速时，转速网格 = {1500} 单点，二维代码路径须无需修改即可退化运行。
- **Baseline 与 DP 的 BSFC 口径不一致**（例如 Baseline 用旧表、DP 用新曲线族）：视为数据污染，禁止跨口径比较；Baseline 必须用与 DP 相同的 BSFC 曲线重算。
- **Baseline 在 DP 约束集下不可行**（如 Baseline 运行于 P < P_min_stable 档位）：按 FR-010 跳过 DP≤Baseline 校验并标记保守比较，禁止静默比较。
- **启停口径的聚合近似**：聚合模型启停 = 整机启停，与真实双机逐台启停不同；报告中必须声明该近似及其对上限方向的影响（聚合启停低估了双机粒度的灵活性）。

## Dependencies and Constraints

- 设备真实参数来源：`data/20260526设备参数.md`（MarkItDown 转换自 `20260526设备参数.docx`，单侧值 ×2 = 聚合）；
- BSFC 锚点来源：docx 额定点 453(+5%) g/kWh；潍柴 WH17ME 公开资料最优点 427 g/kWh（机型大概率为 6WH17C680ME5，500 kW @ 1500 rpm）；
- MATLAB R2026a（`/Applications/MATLAB_R2026a.app/bin/matlab -batch`；数字开头脚本须 `eval(fileread(...))`）；
- 场景定义沿用 `Model_Scenarios.m`（S01~S10，服务航速 11 kn，航程 5000 m）；
- 效率链沿用模型现值：η_rect = η_dcdc = 0.97，电池充/放 0.95；
- Git 纪律：全部工作于 `feat/adaptive-ecms-methanol` 分支；gate 基线重建、阈值修改属 🟡 须用户批准；
- 前序教训（约束设计自由度）：不得以"让结果好看"为目的调整 BSFC 锚点、场景集或判据；任何判据修改须回到本文档走修订流程。

## Open Questions

- [ ] OQ-1：AC/DC 变流器类型（二极管整流 vs 有源前端/变频耐受）——用户线下核实；影响二维 map 是主线还是扩展情景，不阻塞 Phase 0（代码须兼容退化）。
- [ ] OQ-2：最低稳定负荷、真实爬坡率、启动代价——建议向设备方索取；Phase 0 以参数化档位覆盖。
- [ ] OQ-3：二维 map 转速范围初值（建议 900~1500 rpm）与最优转速带位置——design 阶段定案，必要时纳入敏感性。
- [ ] OQ-4：启停口径的启动代价参数取值（启动燃油 + 等效磨损）——design 阶段定案，先以文献典型值 + 敏感性处理。
