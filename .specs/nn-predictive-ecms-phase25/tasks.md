# Phase 2.5 NPU 导向模型族优化与工况识别门控 — Tasks

Spec: `nn-predictive-ecms-phase25`
Status: tasks-approved（2026-08-28 外部评审 4+1 项修订后批准实施）
Created: 2026-08-28

> **路径迁移（2026-08-29）**：T-302 起的全部工作路径位于
> `model/simulink_modelD_(EMS_NN_AECMS)/`（旧 simulink_modelD 冻结于 Phase 2）。
> 本文各任务 Scope 中的 `scripts/...`、`results/ecms_phase25/...`、`python/...`
> 均指新文件夹下的相对路径。
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

## Implementation Plan

> 执行模式（用户 2026-08-28 定）：主 Agent 只负责方案/监督/验收；实施由低成本执行模型
> （subagent）完成。因此每个任务自带委派要素（Goal / Scope / Do Not Change / Validation /
> Acceptance / Escalate If），执行者不得越界。
>
> 通用约束（所有任务生效）：
> - 分支 `models-research`；工作目录 `model/simulink_modelD/`；Python 用
>   `model/simulink/python/.venv_tf`（不得另建环境）；
> - Subagent 默认不得 `git add/commit/push`；完成后交还主 Agent 验收；
> - test split 零参与训练/选模/调参；失败实验必须保留记录；
> - 触碰 Do Not Change 或需要设计层判断时，立即停下按 AGENTS.md §5 格式 Escalate。
>
> **Amendment Log v1.1（2026-08-28 外部评审裁决）**：① val RMSE 仅用于 v1 回归保护，
> v2 选模走 FR-317 三级协议（预测资格筛选→闭环→NPU Pareto）；② 工况识别器为候选
> 门控组件，不预设入最终架构，T-310 必跑 G0/G1/G2/G3 消融；③ S20 为 benefit 判定场景，
> 另加 FR-318 留出航次 non-degradation 强制门禁；④ T-311 最终验收必须真实 N6 板上
> int8 验证（离线估算仅中间证据）；⑤ T-303 增加跨 split 近邻重复审计（FR-316）。

- [x] 1. 数据集 v2 扰动机制物理论证（主 Agent 设计门，无代码）
  - ID: T-301
  - Requirement(s): FR-301, OQ-1
  - Goal: 产出 `docs/` 或 spec 附录级论证文档：内河船适用的数据集 v2 扰动机制清单
    （段参数范围/段内流速变化/交通避让减速/水位阻力影响的取舍与理由）、N 规模建议、
    对 y 分布影响的预估。
  - Scope: 只读 `scripts/50_voyage_generator.m`、`scripts/+ecms2/voyage_generator.m`、
    `config/Model_Parameters.m`、`config/Model_Scenarios.m`、Phase 2 manifest/报告。
  - Do Not Change: 任何代码；海况/波浪机制不得引入（用户已否决）。
  - Acceptance: 论证文档经用户/主 Agent 批准；OQ-1 关闭。
  - Escalate If: 扰动机制需要改动船体/阻力模型物理假设。

- [x] 2. 生成器扩展实现 + 数据集 v2 生成 + K-shape 覆盖度审计
  - ID: T-302
  - Requirement(s): FR-301, FR-302, FR-303
  - Goal: 按 T-301 批准清单实现生成器扩展（新种子、新输出目录 `results/ecms_phase25/`），
    跑 law=0 Baseline 闭环仿真产出 v2 fleet_profiles；实现 K-shape（或等效）审计脚本并
    出报告。
  - Context: 复用 `50_voyage_generator.m` / `ecms2.voyage_generator.m` 结构；
    v1 产物目录 `results/ecms_phase2/` 只读。
  - Scope: 允许新增 `scripts/60_*`、`scripts/+ecms25/`、`results/ecms_phase25/`；
    允许小改 `+ecms2/voyage_generator.m` 的参数化接口（保持 v1 种子调用结果逐位不变）。
  - Do Not Change: v1 种子/段型定义的任何现有行为；`results/ecms_phase2/` 内容；
    P_SAT_KW=800 等物理边界。
  - Validation: 新种子两次生成逐位一致断言；200/400 剖面零拒绝零降额；
    K-shape 审计报告产出。
  - Acceptance: manifest_v2 草案（含逐文件 md5）；审计报告含 v1 vs v2 覆盖对比。
  - Escalate If: 改动会改变 v1 生成结果；仿真出现降额/Vdc 越限无法通过参数范围解决。

- [ ] 3. 数据集 v2 窗口化 + 泄漏断言 + manifest 定稿
  - ID: T-303
  - Requirement(s): FR-302, FR-304, FR-316
  - Goal: `python/build_dataset_v2.py`（复制改造自 v1）：窗口化（HIST=300/HORIZON=300/
    STEP=10 不变）→ 航次级 split（新种子或沿用规则，记录于 manifest）→ fit-on-train
    归一化 → `datasets/fleet_windows_v2.npz` + `fleet_norm_v2.json` + `dataset_manifest_v2.json`；
    另含**跨 split 近邻重复审计**（FR-316）：跨 split 航次对相似度分布 + 最近邻清单 +
    疑似模板泄漏标记与处置记录。
  - Scope: `model/simulink_modelD/python/`、`datasets/`、`results/ecms_phase25/`。
  - Do Not Change: 窗口规则、目标公式、302 维特征布局、泄漏断言集（只能加不能减）。
  - Validation: 脚本内置断言全过；y 分布同族检查；manifest 完整；
    近邻重复审计报告产出（判定规则固定并写入 manifest）。
  - Acceptance: manifest_v2 泄漏检查 conclusion=PASS 且含近邻重复审计结论。
  - Escalate If: y 分布断言失败或窗口计数异常。

- [ ] 4. 训练管线扩展：--arch / --quantiles / --dataset 参数
  - ID: T-304
  - Requirement(s): FR-305, FR-306, FR-307, FR-317
  - Goal: `python/train_predictor.py` 增加 `--arch {mlp,tcn,cnn_mlp,nbeats}`、
    `--quantiles`、`--dataset {v1,v2}`；默认参数行为与 v1 完全一致（回归保护）。
  - Context: Phase 2 该脚本含 MLP/LSTM 构建、训练、training_log、selection 导出；
    本次扩展不得破坏 v1 复现。
  - Scope: `python/train_predictor.py`（可加新模块文件 `python/architectures.py`）。
  - Do Not Change: 默认参数行为；既有 training_log.csv 内容；
    选模规则（val RMSE 最小）仅保留于 v1 回归路径——v2 选模按 FR-317 三级协议
    （val 仅资格筛选，最终 Winner 由 T-310 闭环+NPU Pareto 裁决）。
  - Validation: `--arch mlp --dataset v1` 在 v1 npz 上复跑，val RMSE 与
    selection.json 记录（9.303 kWh）一致（容差 ±0.05，同 seed）。
  - Acceptance: 回归通过 + 新参数帮助文档。
  - Escalate If: 复跑偏差超容差（说明改动影响了默认路径）。

- [ ] 5. TCN 实现 + 训练 + val 选模（主推架构）
  - ID: T-305
  - Requirement(s): FR-305, FR-306, FR-309
  - Goal: TCN（因果扩张 Conv1D 残差堆叠，感受野 ≥300 的推导写入注释）+ MLP 头 +
    q10/q50/q90 pinball loss；在 v2 train 上训练、val 选配置；记录失败配置。
  - Scope: `python/architectures.py`、`results/ecms_phase25/models/`。
  - Do Not Change: split、归一化、test 数据读取方式；NPU 算子白名单（design D6）。
  - Validation: 前向 shape 单测；val RMSE/MAE/R²/under_freq/分位数校准全部记录。
  - Acceptance: 至少 3 组 TCN 配置全记录于 training_log_v2.csv；最优配置明确
    （仅为资格筛选排名，最终 Winner 由 T-310 按 FR-317 裁决）。
  - Escalate If: 需要白名单外算子才能达到可比精度。

- [ ] 6. CNN-MLP 与 N-BEATS 对照训练 + MLP 基线 v2 重训
  - ID: T-306
  - Requirement(s): FR-305, FR-306, FR-307
  - Goal: 两个对照架构同条件训练；MLP 基线在 v2 重训（公平基线）；汇总四架构对照表。
  - Do Not Change: 同 T-305。
  - Validation: 对照表含全部约定指标；公平纪律声明写入 selection_v2.json。
  - Acceptance: 四架构 × 指标对照表完成，含失败配置；对照结论仅构成资格筛选
    （FR-317①），不构成最终选模。
  - Escalate If: 某架构无法收敛或结果异常好（疑泄漏）——按 AI_MODELING §21 返回。

- [ ] 7. 规模扫描 + NPU 预算权衡曲线
  - ID: T-307
  - Requirement(s): FR-308, NFR-301, NFR-302
  - Goal: 主推架构 ≥3 规模点（参数量/int8 大小/val 指标/时延估算）；可用 STEdgeAI
    离线分析（如有）或算子数估算；产出权衡曲线图与预算报告。
  - Scope: `results/ecms_phase25/scaling/`。
  - Validation: 曲线单调性合理；报告含预算推导过程。
  - Acceptance: 权衡曲线 + 推荐规模点（供 T-309 闭环用）。
  - Escalate If: 需要实板数据才能定预算——返回主 Agent 决定是否借板实测或先估算。

- [ ] 8. 工况识别器：标签固定 + 训练 + 评估
  - ID: T-308
  - Requirement(s): FR-310, OQ-3
  - Goal: 首选生成器段型真值标签（7 类）；小型 Conv1D 分类器（历史窗口输入）；
    test 混淆矩阵 + 段切换瞬态误识别分析。
  - Do Not Change: 输入仅部署可得量（历史功率/船速）；不得用未来段信息做特征。
  - Validation: test 报告含 macro-F1、per-class recall、失败敏感类（桥区/靠离泊）recall、
    段切换窗口识别率、transition latency；混淆矩阵归档。
  - Acceptance: 上述指标齐全且失败敏感类 recall 可支撑门控用途；overall accuracy ≥80%
    不得作为唯一门槛；不达标返回主 Agent 讨论退化方案。
  - Note: 识别器为候选门控组件（FR-311 v1.1），不预设进入最终架构。
  - Escalate If: 标签来源需改为聚类方案（设计层变更）。

- [ ] 9. 失败模式门控设计（主 Agent 设计门，无代码）
  - ID: T-309
  - Requirement(s): FR-311
  - Goal: 主 Agent 撰写门控设计文档：门控方案、增益限幅、回退语义（law=5 或
    law=6(ê=0)）、迟滞机制、单测方案；**必须包含 G0（无门控）/G1（仅分位数）/
    G2（仅工况识别）/G3（组合）消融协议与决策规则**（G2/G3 相对 G1 有明确闭环增量，
    识别器才入最终架构）。
  - Acceptance: 用户/主 Agent 批准；OQ-3 遗留关闭。
  - Escalate If: 需要新增 law=8 或改动 law=4/5 内核（设计层，主 Agent 自决）。

- [ ] 10. 门控集成 + 闭环矩阵 v2 + capture 判定 + Phase 2.5 报告
  - ID: T-310
  - Requirement(s): FR-311, FR-312, FR-315, FR-317, FR-318
  - Goal: 按 T-309 批准方案集成（Simulink 脚本驱动）；G0~G3 消融矩阵全跑；
    S20 benefit 判定 + S01/S02/S07 消融 + ≥10 留出航次 **non-degradation 门禁**
    （FR-318：零新增降额/零 SOC 越限/节油差 median ≥ −0.5%、min ≥ −1.5%）；
    最终 Winner 按 FR-317 三级协议裁决（闭环合格者进入 NPU Pareto）；
    gate_result_v2.json；Phase 2.5 报告（含失败清单、与 Phase 2 对比）。
  - Scope: `scripts/63_*`、`scripts/64_*`、`results/ecms_phase25/`、`reports/`。
  - Do Not Change: 判定口径 D4；S20 为 benefit 判定场景，FR-318 留出门禁同为必要条件；
    任何门限（capture 0.50、留出阈值）不得为 PASS 调整。
  - Validation: 03_run_smoke_tests PASS；S01 回归；gate_result_v2 overall 字段。
  - Acceptance: 报告含 capture 复测、消融、留出分布、失败清单；结论诚实（PASS/
    PARTIAL/FAIL 均可）。
  - Escalate If: S01 回归失败或门控引入新失败模式。

- [ ] 11. NPU 部署验证：int8 量化 + STEdgeAI 编译 + N6 实测/估算
  - ID: T-311
  - Requirement(s): FR-313, FR-319, NFR-301, NFR-302
  - Goal: 最终选定模型 int8 量化（fp32 vs int8 精度对照）；STEdgeAI 编译记录
    （中间证据）；**最终验收必须真实 STM32N6 板上 int8 推理验证**（FR-319）：
    int8 模型大小 / activation RAM / weight memory / Neural-ART HW coverage /
    SW/HYBRID fallback 算子清单 / 实测时延 / 量化精度差 / 板上输出 vs Python reference
    一致性。部署报告归档。
  - Scope: `python/export/`、`results/ecms_phase25/deploy/`。
  - Do Not Change: 量化不得改变已选模型语义；预处理参数与 fleet_norm_v2.json 一致。
  - Validation: int8 vs fp32 test 指标差报告；编译日志归档。
  - Acceptance: 部署报告完整；时延落入 NFR-301 预算或记录偏差与处置。
  - Escalate If: STEdgeAI 编译失败且白名单内无法修复；或真实 N6 板不可用/
    板上操作未获分阶段授权（AGENTS.md §11）——停在中间证据归档并返回主 Agent，
    不得以估算充最终验收。

- [ ] 12. 复现检查 + 文档收尾
  - ID: T-312
  - Requirement(s): FR-314, FR-315, NFR-303
  - Goal: `scripts/65_repro_check_v2.m/.sh`：干净进程重跑关键链（数据集构建可跳过但
    manifest 校验必做）；同步所有受影响目录 README（AGENTS.md §20）；
    更新 `项目开发进展.md`。
  - Validation: repro PASS；README 登记无遗漏；`git status` 干净（除约定不入库产物）。
  - Acceptance: 主 Agent 按 §14/§15 模板验收。
  - Escalate If: repro 不一致。

## Verification Checklist

- [x] Requirements IDs are referenced by tasks（T-301~T-312 ↔ FR-301~FR-319/OQ）。
- [x] Design testing strategy is represented by tasks（单测/集成/闭环/repro 各有任务）。
- [x] Each task has a clear validation command or manual check。
- [x] Rollback/migration：v2 与 v1 并存、默认参数回归保护（T-304）即回退机制。

## Execution Notes

- `Status: tasks-approved`（2026-08-28，Amendment v1.1 四项约束修订 + 近邻重复审计
  增补后，用户批准实施）。
- 任务按序执行；T-301/T-309 为主 Agent 设计门（无代码）。
- 每个任务验证通过后立即勾选；偏离 design 须先记录于 design.md。
- 实施由低成本执行模型完成；主 Agent 负责每个任务的验收（AGENTS.md §14）。
