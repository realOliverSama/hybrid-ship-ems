# Phase 1 实时 ECMS 控制器链 — Requirements

Spec: `adaptive-ecms-phase1`
Status: requirements-approved（依据用户 2026-08-24 批量授权：后续任务不再逐个征求意见——brainstorm E4）
Created: 2026-08-24
Brainstorm: `./brainstorm.md`
上游: `adaptive-ecms-methanol`（Phase 0，implementation-complete，gate=DECISION_REQUIRED → 人工裁决 GO）

## Overview

Phase 0 已在严格同可行域下证明 DP 离线节油上限 3.6~4.6%（P_min=0 口径）。Phase 1 构建实时 ECMS 控制器链（C1 Fixed ECMS → C2 SOC-Adaptive ECMS → C3 Oracle Predictive A-ECMS），在复合航次（头条）与基础场景上以 DP 为标杆测量 optimality gap，回答 RQ1~RQ3（见 brainstorm）。NN 训练不在本 Phase（Phase 2 独立立项）。

## Scope

### In Scope

- 复合航次场景构造（时间序列航速剖面，~90 min / 11 航段，草案 V1 见 brainstorm E2）与场景框架扩展（突破 v_knots 6 节点上限）；
- 复合航次的 Baseline 仿真、P_demand 剖面导出（沿用 T-006 因果隔离规则）、DP 上限重算；
- Simulink 内新增 ECMS 控制器变体（EMS_CTRL_LAW=4/5/6），全部经 build 脚本生成，禁止手工 GUI 修改；
- 实验矩阵 C0/C1/C2/C3 + DP，机器可读结果与 Phase 1 gate；
- Phase 1 报告（含 RQ1~RQ3 答案与 Phase 2 建议）。

### Out of Scope

- 任何神经网络训练/推理（Phase 2）；STM32N6 部署（Phase 3）；
- 双机独立调度/左右舷分解（D8/用户裁决，聚合等效保持）；
- 二维可变速 BSFC（Phase 0 已证合成 map 无增量，D9）；
- 参数重标定（等真实船舶/机组数据，E3）；
- 冻结目录（`model/simulink/`、`model/simulink_modelC/`、`model/simulink_modelC_(oracle_ms)/`）任何修改。

## User Stories

- As a 研究者, I want 实时 ECMS 与 DP 上限的量化 gap, so that 论文能回答"实时算法离全局最优多远"。
- As a 研究者, I want Oracle 预测增量（C3−C2）的严格测量, so that Phase 2 的 NN 立项有量化必要性证据而非信念。
- As a 后续开发者, I want ECMS 变体全部脚本构建且有单元测试, so that Phase 2 的 NN 接入点（λ 或 SOC 目标通道）已固化且可回归。

## Functional Requirements

### 复合航次

- **FR-101**: THE 工程 SHALL 新增时间序列航速剖面场景支持（[t, v_target] 输入，突破 v_knots 6 节点限制），以不改既有 8 场景行为为前提（回归验证：S01 重跑结果与 Phase 0 逐位一致）。
- **FR-102**: THE 复合航次 SHALL 按 brainstorm E2 草案 V1 构造（11 航段、约 90 min、低负荷段 ~53%），场景 ID 为 S20_composite_voyage，定义集中于 config；航段参数（速度/时长/水流）全部可配。
- **FR-103**: WHEN 复合航次建成后, THE 工程 SHALL 对其执行 Baseline 仿真 + P_demand 导出（沿用 FR-006 因果隔离规则：降额上游、降额事件检查）+ DP 上限求解（连续口径，P_min=0 与 400 两档），结果纳入 `results/ecms_phase1/`。

### ECMS 控制器

- **FR-104**: THE 工程 SHALL 在 modelD 新增 EMS 变体（脚本构建，EMS_CTRL_LAW=4）实现 Fixed ECMS：每步在 P_gen 可行网格上求 `argmin [ṁ_f(P_g) + λ·P_bat_eq]`，λ 为标定常数（kg/kWh），输出 P_gen_ref 仍经既有爬坡/惯性环节；P_bat_eq 的符号与效率链按 design 定案并与 dp_solve 的 sign convention 一致。
- **FR-105**: THE 工程 SHALL 提供 λ 标定程序：在 S01 上以 SOC_end ∈ SOC₀±0.005 为目标二分搜索 λ，标定值与残差写入 config 与结果文件。
- **FR-106**: THE 工程 SHALL 新增 SOC-Adaptive ECMS（EMS_CTRL_LAW=5）：λ(t) = λ₀ + K_soc·(SOC_ref − SOC(t))，K_soc 标定程序同 FR-105。
- **FR-107**: THE 工程 SHALL 新增 Oracle Predictive A-ECMS（EMS_CTRL_LAW=6）：在 C2 基础上叠加由真实未来需求剖面（oracle，来自剖面文件，非在线预测）驱动的前瞻项，前瞻律形式按 design 定案；**该变体仅用于上界/增量验证，报告中不得表述为可实时实现**（E5 红线）。
- **FR-108**: WHEN ECMS 变体施工完成时, THE 工程 SHALL 通过单元测试：P_demand 恒定时稳态功率分配与离线最优点一致（偏差 <2%）；λ→∞ 时退化为电池保护（P_gen 全力）；λ=0 时退化为燃油瞬时最优；符号约定单测（放电/充电方向的 λ·P_bat_eq 符号正确）。

### 实验矩阵与判定

- **FR-109**: THE 实验矩阵 SHALL = {C0, C1, C2, C3} × {S20, S01, S02, S07} + 全 8 场景回归（C0/C1/C2），每运行输出 fuel、SOC_end、Vdc_min、欠压事件、电池吞吐量；DP 标杆值复用 Phase 0 工具链同口径计算。
- **FR-110**: THE 工程 SHALL 产出 `results/ecms_phase1/gate_result.json`：
  - RQ1：C1 捕获率 = (fuel_C0_corr − fuel_C1_corr)/(fuel_C0_corr − fuel_DP)（S20 主判定）；
  - RQ2：C2−C1 增量（pp）；
  - RQ3：C3−C2 增量（pp）——Phase 2 NN 立项的量化依据；
  - 判定字段：`ecms_capture_rate`、`oracle_increment_pp`、`recommendation`（PROCEED_TO_PHASE2_NN / NN_VALUE_LIMITED / DECISION_REQUIRED）；
  - 建议逻辑（design 定案精确阈值）：oracle_increment ≥ 0.5 pp → PROCEED_TO_PHASE2_NN；< 0.2 pp → NN_VALUE_LIMITED；其间 → DECISION_REQUIRED。
- **FR-111**: THE 工程 SHALL 生成 Phase 1 报告 `reports/ECMS_phase1_ecms_vs_dp.md`（方法、λ 标定、矩阵结果、RQ1~3 答案、局限、Phase 2 建议）。

### 工程纪律

- **FR-112**: 全部施工脚本驱动；modelD 的 10b smoke 门禁保持 overall=true（每引入新变体后重跑）；冻结目录只读。
- **FR-113**: ECMS 全部实验遵循 E5 诚信红线：对比组共用参数/场景/BSFC 曲线族（tier2 中档为主判定，tier1/3 敏感性附列）；oracle 信息不进入 C1/C2。
- **FR-114**: 复现性：同 config 主矩阵双跑逐位一致；大型 .mat 不入库，runs.csv/JSON/PNG/报告入库。

## Non-Functional Requirements

- **NFR-101**: ECMS 控制器单步计算量须满足未来嵌入式部署可行性（每步网格搜索 ≤ 200 点，无迭代优化器调用）——Phase 3 STM32N6 兼容性的前置约束。
- **NFR-102**: 全矩阵（含回归）仿真总时长 ≤ 2 h。
- **NFR-103**: 假设参数清单（爬坡率、τ、最低稳定负荷、电池效率）在报告中集中登记"待设备方确认"，与 Phase 0 清单合并维护。

## Acceptance Criteria

- [ ] AC-1（FR-101/102）：S20 复合航次建成，S01 回归逐位一致；
- [ ] AC-2（FR-103）：S20 剖面 + DP 上限产出，因果隔离与能量闭合检查通过；
- [ ] AC-3（FR-104~108）：C1/C2/C3 变体单元测试全过，10b smoke 绿；
- [ ] AC-4（FR-109）：矩阵完成，无缺项；
- [ ] AC-5（FR-110）：gate_result.json 字段完整，recommendation 与数值一致；
- [ ] AC-6（FR-111~114）：报告入库；复现一致；无冻结目录改动；无大产物入库。

## Edge Cases

- **λ 标定不收敛**（S01 上无 λ 使 SOC_end 入带）：Escalate，不得放宽终端带；
- **C1 节油为负**（ECMS 差于 Baseline）：允许存在（λ 失配或工况特性），如实记录并分析，不得调参掩盖；
- **S20 中出现降额事件**：剖面作废标记，Escalate（复合航次功率需求超装备能力需重新审查航段设计）；
- **电池 SOC 越限**（C1/C2 在某些场景触限）：记录事件数与时长；若触限导致结果不可比，标记该场景该变体结果无效并说明。

## Dependencies and Constraints

- 复用 Phase 0 全部工具链（+ecms/dp_*、bsfc_family、profiles、Ecms_Phase0_Config 模式）；
- BSFC 主判定用 tier2（+50% 中档），tier1/3 附列敏感性；
- 复合航次功率标定基于现有 1000 t/11 kn 体系（E3：真实数据回来后统一重跑）；
- 分支 `feat/adaptive-ecms-methanol` 继续；MATLAB R2026a batch；数字开头脚本 `eval(fileread())`；
- 用户批量授权见 brainstorm E4；🟡/🔴 纪律不变。

## Open Questions

- [ ] OQ-11：C3 oracle 前瞻律的具体形式（design 定案）；
- [ ] OQ-12：λ 标定是否需分场景（Phase 1 先单点 S01 标定，跨场景迁移性作为 RQ1 的一部分观察）；
- [ ] OQ-13：真实船舶/机组数据到达时间（不阻塞）。
