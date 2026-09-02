# EMS01 CHIL v3 — soft-HIL 真闭环实验 Requirements

Spec: `ems01-chil-v3-closedloop`
Status: requirements-approved（2026-09-08 用户批准）
Created: 2026-09-08
Brainstorm: `./brainstorm.md`
输入: `../HANDOFF-v3-closedloop.md`（2026-09-08 晨）+ v1 前置状态
Branch: `models-research`
工程目录: `firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS/`
模型目录: `model/simulink_modelD_(EMS_NN_AECMS)/`

## Overview

v1（`ems01-chil-ecms-v1`）验证了"算得对"：层① 固件 REF vs Simulink 参考 100% exact、
层② NPU 端口误差受控、FR-508 回退状态机板上验证；其形态是**开环激励回放**（预录轨迹
回放 + 输出对拍，SOC 从未被板子输出影响）。

v3 把它升级为**真闭环 soft-HIL**：主机侧被控对象（plant）模型每控制拍（1 s）现场单步
推进——输入板上 P_gen → 功率平衡 → SOC(k+1)/v_ship(k+1)/油耗(k)——板载 EMS 的决策
实时驱动功率平衡，验证"控得好"，为论文收官章节提供闭环实验数据。

术语约定（brainstorm 裁决）：**统一"被控对象（plant）"**——被控对象模型（plant model）、
被控对象推进器（plant advancer）、被控对象子集模型（plant-only variant）；
HANDOFF 原文"植物"为误译，本 spec 全部产物按新术语书写。

## Fixed Decisions（2026-09-08 用户 brainstorm 裁决，实施不得偏离）

1. **层② S20 处置 = (a)**：层② S20 98.877%（<99%，62/5521 拍 >1 网格步）边际 FAIL
   接受并记录为 workclf 口径修订 v1.1 的已知代价，登记入 v3 基线说明；层② 门禁语义
   在 v3 中降为**参考指标**（不作 v3 门禁）；v3 验证重心 = 层① 移植保真 + 闭环指标；
   层③ 对拍继续报告性输出；**不放宽任何既有门禁数值**。
2. **被控对象推进器 = P3 主案**：被控对象子集模型（脚本构建，主 ModelD 不动）→
   Simulink Coder codegen → C → Python ctypes 单步推进 API；单一来源由构造保证。
   IF P3 受阻（codegen 链不通 / 子集提取无法保真 / 数值不可接受），SHALL 回退
   **P1**（Python 方程转录推进器 + vs Simulink 轨迹等价门禁）并**显式记录**受阻原因
   与决策；禁止静默换道。
3. **节拍 = lock-step**：每拍 主机发 CMD_TICK(k)[被控对象状态] → 收 RSP_TICK(k)
   [P_gen_ref] → 被控对象推进至 k+1 → 下一拍；**固件零改动**，CMD_TICK/RSP_TICK
   协议语义不变；1 Hz 实时节拍列入 Out of Scope。
4. **实验矩阵 = 4 臂 × 2 场景**（S20 + voyage_002）：
   (a) 板载 NPU 镜像 + codegen 被控对象闭环；
   (b) 板载 REF 镜像 + **同一** codegen 被控对象闭环（(a) vs (b) 零混杂隔离 NN 增益）；
   (b′) ModelD 原生闭环仿真 law=7 EML（零新工作量，兼作被控对象仪器闭环交叉验证）；
   (c) ModelD 原生闭环仿真 law=5（SOC 自适应 ECMS 基线；law=5 无固件实现，走原生仿真）。
5. **指标两层结构**：仪器级（保真验证，门禁用）+ 研究级（对比结论，论文数据）；
   SOC 公平性约定：油耗对比必须伴随 SOC 终点偏移，主对比要求 |ΔSOC_end| 在容差带内
   （数值 design 定），超带时补充 SOC 修正油耗次级指标并显式标注。
6. **主模型不动**：`HybridShip_ModelD_DCBus.slx` 保持现状；被控对象子集为独立脚本
   构建的变体（Simulink 全脚本驱动红线）。

## Scope

### In Scope

1. **被控对象子集模型**：脚本构建 plant-only 变体（复用 `build_model.m` 同源构建函数族
   与 `Model_Parameters.m` 参数表），包含闭环相关全部动力学：功率平衡、SOC 积分、
   油耗积分、船速/航程积分、螺旋桨水动力/船体阻力、电池充放电限值与 SOC 降额逻辑、
   工况管理器（距离触发 v_cmd 剖面、P_load 调度）；
2. **codegen 链（P3）**：Simulink Coder 生成 C + 单步推进 API + Python ctypes wrapper，
   全脚本化可复现；P1 回退路径（FR-603）保留；
3. **两道仪器门禁**（机器可读）：
   G-P1 子集 vs 完整 ModelD 开环对照（提取保真）；
   G-P2 codegen C vs 解释执行子集模型轨迹等价（§22/66 教训：轨迹级容差，不承诺位级）；
4. **闭环 harness**（Host/ Python，基于 `ems_inject.py` / `ems_fault_inject.py` 骨架扩展）：
   lock-step 循环、被控对象推进器集成、逐拍轨迹与事件记录；
5. **4 臂 × 2 场景闭环实验**执行：板上 (a)(b) 双镜像会话 + 原生 (b′)(c) 仿真运行；
6. **两层指标**计算与机器可读结果 + **闭环实验报告**（论文素材：四臂对比图表、
   临界工况行为、方法学描述）；
7. **板上会话**（用户在场或远程授权 + 令牌；双镜像 SRAM 加载沿袭 v1 令牌机制）；
8. 文档同步（两级 README 登记、进展日志、任务汇报）。

### Out of Scope

- 1 Hz 实时节拍、真 HIL / 快速原型 / 实时目标机（未来工作）；
- 路线 B 分类器改进（用户 D-6 已裁决延后另立项）；
- law=5 固件化实现；
- 层② NPU↔解释器差异收敛研究；
- AI 建模层任何改动（不重训、不换模型、不动口径——三口径契约 v1.1 已闭合）；
- v2 模型热切换 / NOR 模型库。

## User Stories

- 作为论文作者，我想要**真闭环 soft-HIL 实验数据**（板载 NN-EMS 决策驱动 SOC/油耗的
  4 臂对比），以便完成论文核心章节"闭环验证控得好"的论证。
- 作为论文作者，我想要 (a) vs (b) **零混杂对比**（同一被控对象、同一 harness、同一板），
  以便把性能差异归因于 EMS 后端（NPU NN vs 软件参考律）而非实现差异。
- 作为研究工程师，我想要**机器可读的仪器门禁**（子集保真 / codegen 等价 / 闭环保真），
  以便闭环结果建立在经验证的测量仪器上，链条可复现、可追溯、可断言。
- 作为论文作者，我想要**临界工况行为记录**（SOC 触限、降额介入、FR-508 回退代价），
  以便讨论安全降级语义与边界行为。

## Functional Requirements

- **FR-601**: THE 系统 SHALL 以脚本方式构建被控对象子集变体模型（plant-only variant），
  复用 `build_model.m` 同源构建函数族与 `Model_Parameters.m` 参数表（参数单一来源）；
  主模型 `HybridShip_ModelD_DCBus.slx` SHALL 保持不动；子集 SHALL 包含功率平衡、
  SOC 积分、油耗积分、船速/航程动力学、螺旋桨/船体、电池限值与 SOC 降额、
  工况管理器（距离触发 v_cmd 剖面与 P_load 调度）全部闭环相关动力学。
- **FR-602**: THE 系统 SHALL 以 Simulink Coder 将被控对象子集模型生成 C 代码，提供
  单步推进 API（输入：当前被控对象状态 + P_gen + 工况外生量；输出：k+1 状态 + 油耗率），
  Python 侧经 ctypes 调用；生成与构建 SHALL 全脚本化、可复现、产物哈希可记录。
- **FR-603**: IF P3 链路受阻（codegen 不可用 / 子集提取无法保真 / 数值不可接受），
  THE 系统 SHALL 回退 P1（Python 方程转录推进器，RK3/ode3 同阶同步长）并在
  design 修订与实验报告中显式记录受阻原因、决策过程与回退代价；禁止静默换道。
- **FR-604**: THE 被控对象子集模型 SHALL 通过仪器门禁 **G-P1（子集保真）**：以
  ref_G2 开环参考的 P_gen 序列回放，SOC / 油耗 / v_ship 轨迹 vs 完整 ModelD 输出
  一致性达到 design 固定阈值；结果机器可读（gate JSON）。
- **FR-605**: THE codegen C 被控对象 SHALL 通过仪器门禁 **G-P2（codegen 等价）**：
  同一回放输入下 vs 解释执行子集模型轨迹等价达到 design 固定阈值（轨迹级容差，
  不承诺位级——§22/66 教训：编译目标 vs 解释器存在 FP 微差）；结果机器可读。
- **FR-606**: THE Host 闭环 harness SHALL 实现 lock-step 闭环循环：
  CMD_TICK(k)（7 状态量，来自被控对象现场状态）→ RSP_TICK(k)（P_gen_ref 等）→
  被控对象推进至 k+1 → 记录 → 下一拍；SHALL 复用 v1 协议栈（SerialSession 非阻塞
  收向、帧/CRC/事件环语义不变）；固件 SHALL 零改动。
- **FR-607**: 闭环运行 SHALL 逐拍记录完整轨迹与事件（被控对象状态、P_gen、ê、K、
  cls_eff、flags、infer_us/ctrl_us、串口事件环）为机器可读产物（CSV/JSON）；
  运行元数据（镜像哈希、被控对象构建指纹、起始状态、时间戳）SHALL 一并记录。
- **FR-608**: IF 板载进入 FR-508 安全回退（ê=0 ≡ law=5 行为），THE 闭环 SHALL 继续
  运行（回退 = 安全降级持续控制，非停机），回退段 SHALL 由事件环界定并计入
  临界工况指标；回退语义验收（安全降级定义）在 design 中定义。
- **FR-609**: THE 系统 SHALL 执行 4 臂 × 2 场景闭环实验（Fixed Decisions 4 矩阵）；
  各臂 SHALL 记录任务油耗、SOC 终点与轨迹、v_ship 跟踪、ê 生效占比、回退事件；
  (a)(b) 臂 SHALL 使用同一 codegen 被控对象与同一 harness（除镜像外零变量）。
- **FR-610**: 闭环结果 SHALL 通过**仪器级指标**（门禁用，机器可读）：
  ① 闭环 (b) vs 开环 ref_G2（同控制律、同场景）：任务油耗 / SOC 终点 / v_ship
  一致性达 design 固定阈值（证明闭环接线正确）；
  ② 闭环 (b) vs (b′)：同一控制律下 codegen 被控对象 vs 解释执行 Simulink 的
  轨迹等价达 design 固定阈值（被控对象仪器闭环复验）。
- **FR-611**: THE 闭环实验报告 SHALL 给出**研究级指标**（论文数据）：
  任务油耗（主指标，(a)/(b)/(b′)/(c) 四臂两两对比）+ SOC 终点偏移（伴随指标）+
  |ΔSOC_end| 公平性判定（容差带 design 定；超带时补充 SOC 修正油耗次级指标并
  显式标注）+ 航速跟踪 RMSE（v_ship vs v_cmd）+ 临界工况行为（SOC 下限触及 /
  放电降额启动的时段数与时长、FR-508 回退次数与油耗/SOC 代价）。
- **FR-612**: THE v3 基线说明 SHALL 登记层② S20 98.877% 边际 FAIL 的 (a) 裁决
  （Fixed Decisions 1）；层② 在 v3 中为参考指标不作门禁；层③ 对拍在闭环报告中
  继续报告性输出。
- **FR-613**: THE 交付 SHALL 含：仪器门禁机器可读结果（G-P1/G-P2/仪器级 gate JSON）、
  4 臂闭环轨迹与指标产物、闭环实验报告（含对比图表）、被控对象子集构建与 codegen
  脚本、闭环 harness 工具；入库产物按 AGENTS.md §13 产物策略归档。
- **FR-614**: 板上会话 (a)(b) 臂 SHALL 在用户在场或远程授权 + 令牌下执行（沿袭 v1
  SRAM 加载令牌机制）；禁 Mass Erase、禁外部 NOR 通用擦写；双镜像会话 SHALL 计入
  用户时间预算并预先声明。

## Non-Functional Requirements

- **NFR-601**: 可复现性：全部构建/实验脚本化；lock-step 下同一输入序列闭环轨迹
  可复现（确定性断言）；被控对象 codegen 与 harness 构建产物哈希可记录、可复核。
- **NFR-602**: 单拍闭环周期（RSP RTT ~5ms 量级 + 被控对象单步推进 + 协议开销）
  SHALL ≪ 1s 控制拍；实测单拍耗时分布并记录（论文方法学材料）。
- **NFR-603**: 阶段隔离与板安全（沿袭 v1 NFR-504 + AGENTS.md §11）：实现 → 构建 →
  仪器门禁（G-P1/G-P2）→ 板会话授权；前一阶段 PASS 不自动授权后一阶段；
  Level C 操作仅限既有授权范围。
- **NFR-604**: 语义一致性：被控对象子集参数单一来源（`Model_Parameters.m`）；闭环
  harness 与 v1 协议语义一致（帧/CRC/事件/超时不变）；禁止为闭环 PASS 调整任何
  物理假设、控制常数或阈值（AGENTS.md §6/§9/§10）。
- **NFR-605**: 文档同步：新增/修改文件登记所在目录 README（AGENTS.md §20；Model/
  Host 两级文档按需更新）；Session 按 §18 记进展日志；正式任务按 §15 模板汇报。
- **NFR-606**: 路径纪律（AGENTS.md §20.2）：新增脚本禁硬编码绝对路径——MATLAB 用
  `mfilename('fullpath')` 推导 + pwd 兜底；Python 用 `__file__` 推导仓库/工程根。

## Acceptance Criteria

- [ ] **AC-601**: FR-601/604 —— 被控对象子集构建脚本可重复执行，G-P1 门禁 PASS
  （机器可读 gate JSON，阈值经用户批准）。
- [ ] **AC-602**: FR-602/605 —— P3 codegen 链走通（生成产物哈希入库），G-P2 门禁
  PASS（机器可读）；IF 走了 FR-603 回退则本条以 P1 等价门禁 + 回退记录替代。
- [ ] **AC-603**: FR-606/607 —— 闭环 harness 通过 selftest（无板 mock/pty 层面），
  逐拍轨迹与元数据产物格式机器可读。
- [ ] **AC-604**: FR-608/609/610 —— 4 臂 × 2 场景全部执行，仪器级指标门禁 PASS
  （机器可读）；FR-508 回退段在闭环中有事件界定与代价统计。
- [ ] **AC-605**: FR-611/613 —— 闭环实验报告完整（四臂对比 + SOC 公平性判定 +
  临界工况 + 图表），产物按 §13 入库。
- [ ] **AC-606**: FR-612 —— v3 基线说明登记层② (a) 裁决，闭环报告含层③ 报告性输出。
- [ ] **AC-607**: FR-614 —— 板会话授权纪律合规（令牌记录、双镜像时间预算声明）。

## Edge Cases

- **被控对象数值发散 / SOC 触限**（闭环中 EMS 决策导致）：记录为**实验结果**（临界工况
  行为），harness 不得静默裁剪轨迹或重置状态；若数值发散源于被控对象仪器缺陷则
  escalate 重过 G-P1/G-P2。
- **串口断连 / 帧错误 mid-run**：会话失败语义（整跑重做 or 断点续跑）design 定；
  事件环证据必须保留；不得以部分轨迹冒充完整场景。
- **codegen 数值差异超 G-P2 容差**：escalate 主 Agent——候选（修订 codegen 配置 /
  换求解器形态 / 回退 P1），不得自行放宽容差。
- **子集提取发现隐藏耦合**（被控对象与 EMS 内部状态交叉引用）：escalate，不得硬拆；
  由主 Agent 重新分析提取边界。
- **板上 REF 镜像与 NPU 镜像会话间隔**（双镜像 SRAM 加载）：加载失败/哈希不匹配时
  停止并报告，不得带病继续注入。

## Dependencies and Constraints

- **v1 资产**：`Host/ems_inject.py` / `Host/ems_fault_inject.py`（协议栈/事件环骨架）、
  双镜像构建（`EMS_FW_MODE=REF/NPU`，层① 100% exact 已板上证明）、
  `ref_G2_*.csv`（v1.1 口径开环参考）、SerialSession 非阻塞收向（RTT ~5ms）。
- **ModelD 资产**：`scripts/+model/build_model.m` 函数族、`config/Model_Parameters.m`
  （69+ 参数单一来源）、`config/Model_Scenarios.m`（S20 / voyage_002 定义）、
  `results/ecms_phase25/deploy/` 参考链。
- **工具链**：MATLAB R2026a + Simulink Coder（已确认安装）；Python Host 环境（v1 现用）。
- **前置裁决**：层② S20 (a) 已裁决（2026-09-08）；workclf 口径 v1.1 已实施并板上复验。
- **板约束**：AGENTS.md §11 + SoftwarePackage/AGENTS.md 全部硬件安全条款适用。

## Open Questions

- [ ] **OQ-1**: G-P1 / G-P2 / 仪器级一致性的阈值与度量口径（终点偏差 / RMSE /
  逐拍网格率 / 轨迹距离的取舍与数值）——design 定，报用户批准。
- [ ] **OQ-2**: SOC 公平性容差带数值与 SOC 修正油耗折算方法（借电折算公式）——design 定。
- [ ] **OQ-3**: codegen 求解器形态：被控对象子集按原 ode3@0.01s 整体生成，还是
  离散化重构（保真 vs 生成体量/接口复杂度的取舍）——design 定。
- [ ] **OQ-4**: 闭环断点续跑 vs 整跑重做的口径与判定条件——design 定。
- [ ] **OQ-5**: (b′)(c) 原生仿真的运行入口（复用既有脚本参数化 vs 新增封装脚本）——design 定。