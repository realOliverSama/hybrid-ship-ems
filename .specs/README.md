# .specs/ — Spec-Driven Development 工作目录

> **入库策略**: 入库（spec 是项目决策与需求的可追溯记录）。

存放项目层规格（spec）：每个子目录对应一个研究/功能特性，走 brainstorm → requirements → design → tasks
四阶段流程（见各阶段文件内的 `Status:` 行，`*-draft` → `*-approved` 逐门批准）。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `config.yaml` | 文件 | SDD 项目配置（project.goal / tech_stack / principles / hooks） |
| `adaptive-ecms-methanol/` | 目录 | 边缘 AI 自适应 ECMS 甲醇混合动力船舶 EMS（当前活跃研究主线，同名分支） |
| `adaptive-ecms-phase1/` | 目录 | Phase 1 实时 ECMS 控制器链与 DP gap 测量 |
| `nn-predictive-ecms-phase2/` | 目录 | Phase 2 NN 预测型 A-ECMS（SOC_ref 前瞻通道） |
| `nn-predictive-ecms-phase25/` | 目录 | Phase 2.5 NPU 导向模型族优化（TCN 主推）+ 数据集 v2 内河工况扩展 + 工况识别门控（models-research 分支，tasks-approved / Amendment v1.1） |
| `phase4-model-e/` | 目录 | 【已作废 SUPERSEDED】Phase 4 Model E：面向 HIL 的模型重构与实时化（2026-08-31 全 draft，未批准未实施）；2026-09-03 被 `docs/phase4_research_roadmap.md` 新 Phase 4 路线取代（Q1 裁决：HIL 实时化推后为后续 Phase，其 T-401~T-407 编号作废）；目录保留存档 |
| `phase4-a1-model-e-audit/` | 目录 | Phase 4 工作包 A 第一段：Model E 建模与参数审计（`Phase4` 分支）——T-401 架构审计 + T-402 参数真实性审计（纯分析零施工，Model D 只读），产出两份审计报告 + 全参数 CSV 总表（含用户回填字段）作为 A2 施工段 spec 的 Fixed Decisions 输入 |
| `phase4-a2-model-e-construction/` | 目录 | Phase 4 工作包 A 第二段：Model E 施工（T-403~T-412，implementation-complete）——船体浅水/双机组/1RC ECM/SOP，T-412 物理 Gate overall=true，基线 v1.1 promoted |
| `phase4-b-pms-safety/` | 目录 | Phase 4 工作包 B：真实控制逻辑 / PMS v2 + Safety-Interlock（T-413~T-418，implementation-complete）——四层 PMS + 独立 Safety 层，43 控制 Gate（134 判据）+ 44 负向 7/7，控制基线 v1.0 冻结；PR #20 已并入 Phase4 |
| `phase4-d-dataset-v3/` | 目录 | Phase 4 工作包 D：数据集 v3（T-430~T-435）——观测契约 H1/H2/H3、Known-Future 契约（K/F/U + issue-time causality）、窗口化/target/分位数语义、voyage/group split + 三级泄漏审计 + OOD 框架；**四门禁全部批准（2026-09-04）**；D0（T-001~T-017）完成 + D-15 必改闭合 + formal C/D Gate 双语料 overall=true（EXT-CD-GATE，2026-09-07，PR #23 已并入 Phase4）；`scientific_roadmap_amendment_v1.md` **APPROVED（路线 c′，2026-09-07）**——T429 保持 FINAL_FAIL、contrast 层降级 DESCRIPTIVE_DIAGNOSTIC、OOD 三轴不变；**T-432 activation record FROZEN + checklist ALL_GREEN（2026-09-07，D1~D5 Final Ruling）→ T-432 READY**，T-018/T-019 可施工（73 号）；T-020（T-435-formal）待 Stage B；移交文档 `HANDOFF_D0_IMPLEMENTATION.md` / `HANDOFF_D_SIDE_SESSION.md` |
| `phase4-e-predictor-v3/` | 目录 | Phase 4 工作包 E：预测器 v3（T-436~T-444，单 spec）——五候选（MLP-Q-v3/TCN-Q/TFT/Transformer/REAT-T）+ T-442 架构保持消融 + T-443 三轴 OOD 诊断 + T-444 拆分里程碑（T-444a val-only 预选 → T-443 一次性终评 → T-444b 封印）；**四门禁全部批准（2026-09-13）**：brainstorm E-Q1~Q9、requirements（FR-570~611/NFR-440~446/AC-450~455，R1~R5）、design（DGN-E-01~E-18，R1~R9）、tasks（T-001~T-019，R1~R8）；单向证据流 val-only 选模 + test/OOD sealed 终评；canonical 正式训练环境 = AutoDL 4090D CUDA；**implementation-complete（2026-09-15 收官，T-001~T-019 全完成）**：WINNER = REAT-T（reatt-t018，val sg_macro pinball seed-median 3.9503，全局最低）；FINAL_WINNER_EVIDENCE_PACKAGE 96 成员封印，manifest 与全部证据成员已入库（`results/predictor_v3/`）；移交 Phase 4-F（T-445 起，feat/phase4-f 分支） |
| `ems01-chil-ecms-v1/` | 目录 | EMS01_CHIL_ECMS 固件 v1（Phase 3 CHIL 控制器侧，models-research 分支）：M1 = CMake 骨架 + STEdgeAI + MLP-Q int8 板上验证（兼关闭 phase25 T-311）✅；M2 = ECMS+G2 移植 + 本地激励验证 ✅（T-508 层① 100% exact / 层② ≥99% PASS / FR-508 四场景 PASS，报告 `m2_port_report.md`）；workclf 口径修订 v1.1 已实施并板上复验（层① 100% 保持、层③ 98.4%+、板载分类器对设计真值 75.8%；层② S20 98.877% 边际 FAIL 用户裁决 (a) 接受并记录）；热切换→v2、CHIL 通信→v3 |
| `ems01-chil-v3-closedloop/` | 目录 | EMS01 CHIL v3 — soft-HIL 真闭环实验（models-research 分支）：开环回放升级为真闭环（主机被控对象每拍推进 + 板载 P_gen 回馈功率平衡）；4 臂对照（板载 NPU / 板载 REF / ModelD 原生 law=7 / law=5）× S20+voyage_002；被控对象子集 codegen→C 推进器（P3 主案，受阻回退 P1）；lock-step 固件零改动。状态：brainstorm ✅ / requirements ✅（2026-09-08 用户批准）/ design ✅（2026-09-08 用户批准，含 §5 阈值与 OQ-1~5 定案，spec_validate 12/12）/ tasks 待起草。后继：v3 完成后立即编译期换型（FR-511 流程换入 phase25 同签名候选，design §11 登记） |
| `multistep-predictive-ems/` | 目录 | 面向参数不确定性的多步负荷预测增强型 EMS |
| `phase4-f-trajectory-oracle-gate/` | 目录 | Phase 4 工作包 F 首道 Gate：**T-445 CLOSED — `GATE_VALIDITY_FAIL`（2026-09-15）**。B2 代理未通过冻结 1% 资格化，未执行正式 36 航次语料，未出具轨迹信息价值 GO/NO-GO。方法学有效性失败，不是“轨迹预测无价值”。T-446 原定义阻断。后继路线见 `phase4-f-practical-predictive-ems/` |
| `timed-feedforward-ems/` | 目录 | Model C 定时前馈 EMS（Peak + Time-to-Peak 预测调度） |
| `model-b-dc-bus/` | 目录 | Model B 直流母线电气层 + 安全/节能指标卡 + S10 工况 |

每个 spec 目录内典型文件：`brainstorm.md`（意图与方案门）、`requirements.md`（需求）、
`design.md`（设计）、`tasks.md`（任务清单与勾选），部分含 `reviews/`（评审记录）。

`nn-predictive-ecms-phase25/` 附加文件：`t301_dataset_v2_perturbation.md`
（T-301 论证文档：数据集 v2 扰动机制清单与参数范围，FR-301/OQ-1，待用户批准）。

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）；
新增 spec 目录（即新特性立项）时必须同时在本 README 登记。
