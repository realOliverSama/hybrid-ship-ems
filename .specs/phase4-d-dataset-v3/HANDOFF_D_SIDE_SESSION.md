# Phase 4-D 续开 Session 交接文档（D-15 必改 + formal C/D Gate + T-429 FAIL 影响注记）

> 写给接手 Phase 4-D 续开的 session。C 侧已全部收口（PR #22 已合并），本文档覆盖 D 侧剩余工作。
> 创建：2026-09-06。前置阅读：`.specs/phase4-d-dataset-v3/HANDOFF_D0_IMPLEMENTATION.md`（D0 交接，仍有效）。

## 0. 开工仪式

```bash
git fetch origin && git status -sb && git log --oneline -3
```

- 工作分支：`feat/phase4-d-dataset-v3`（D0 工作分支，续用）；
- Phase4 主分支已合并 PR #22（C 包 feat/phase4-c-mission-v3 → Phase4，2026-09-06T21:57Z）——C 包全部产物在 Phase4 上可用；
- 工作区两个不入库目录（`fault_out/`、`slprj_v1_stale/`）不清理。

## 1. C 侧收口状态（D 侧需要知道的）

### 1.1 交付物（Phase4 分支上已可用）

| 交付物 | 路径 | 说明 |
|---|---|---|
| 生成器包 | `scripts/+mission_v3/`（14 组件函数） | dag_sample/env_realize/plan_compose/scenario_compile/forecast_issue/kf_snapshots/family_expand/eligibility/producer_export/stack_identity/producer_confirm/validate_registries/+audits 两件 |
| 注册表九件 + 预注册计划 | `config/mission_v3/` | 各含 self_hash；AMENDMENT 链完整（PREQUAL-AMEND-01 / T429-AMENDMENT-V2） |
| 预资格语料 | `datasets_v3/mission_v3/prequal[_rerun]/`（本地不入库） | 108 航次 × 2 passes，payload hash 逐对一致 |
| confirmation 语料 | `datasets_v3/mission_v3/t429conf_v2[_rerun]/`（本地不入库） | 144 航次 × 2 passes（coverage 96 + DEPTH 24 + TRAFFIC 24） |
| 就绪包 | `results/mission_v3/cd_gate_readiness.json` | producer schema validator PASS（exit 0）；FINAL_FAIL 显式记录 |
| 审计/冻结/终判 | `results/mission_v3/` | t429_freeze_v1/v2、t429_final_verdict、prequal_amendment_01、t429_amendment_v2、d1a/d1b 诊断 等 |

### 1.2 T-429 FINAL FAIL（D 侧必须知悉）

- **overall FINAL FAIL（0/6 required contrasts）**：Day-3 最终裁决正式接受，不再第三轮。
- 逐项身份：BRIDGE/BERTH/SPRINT/TRAFFIC = VALID FAIL；DIR = INVALID/UNRESOLVED（matched-segment 实现缺陷）；DEPTH = INSUFFICIENT EVIDENCE（frozen grouped-CV 规则）。
- 终判记录：`results/mission_v3/t429_final_verdict.json`（self_hash eb389ae8…）。
- **对 D 的影响**：Dataset v3 的 contrast/OOD 框架所依赖的六对比在当前 Model E + H1 P_load_total 签名下不可辨识或证据不足。**D 侧 formal C/D Gate 不受影响**（那是接口合规性检查，非科学结论），但 **T-432 Stage B formal 语料的 OOD/contrast 标注**需在 downstream scientific-roadmap amendment 中决定如何处理。
- 三层根因（详细分析见 `项目开发进展.md` 2026-09-06 条目）：
  1. SBD（1−max NCC，幅度不变）对水平移位型对比结构性不足（R 0.50-0.82 << 1.5，含探针 0.92-0.97 的强信号对比）；
  2. DEPTH 深度阻力增量在 H1 P_load_total 签名层不可辨（BA≈0.50 随机）；
  3. DIR pooled probe 被跨航次方差淹没（配对诊断 0.944 但 pooled 0.59-0.64）。

### 1.3 C/D 就绪包

`results/mission_v3/cd_gate_readiness.json`（self_hash 8d55e989…）：
- producer schema validator PASS（exit 0）；
- 确认语料 manifest sha256 与 144 航次清单；
- stack composite 19180c21…（Strict）；
- T-432 Unblock Checklist 第①项 COMPLETE_WITH_T429_FINAL_FAIL。

## 2. 剩余工作序列

### 2.1 D-15 必改（formal C/D Gate 前置）

**现状**：`dataset_v3/cd_interface_gate.py` 的十项检查折叠为 5 个机器键（c01/c02/c07/c09/c10），③④⑤⑥⑧ 以违规消息前缀归因；⑤ oracle 分离为列名 marker 扫描；⑩ 独立运行 python 时缺省 pass（经 72 号路径 fail-closed）。

**必改内容**（design.md D-15 冻结原文）：
1. 十项检查逐项独立键（c01~c10 全拆开，不得折叠）；
2. ⑤ oracle 分离收紧：从列名 marker 扫描改为逐列显式角色断言（ORACLE 列在 FORMAL 域必须显式存在且与 INPUT_F 列名空间物理分离）；
3. ⑩ 独立运行 python 时缺省 fail（不经 72 号路径时不可缺省 pass）；
4. 每键独立 pass/fail + 违规详情。

**涉及文件**：`dataset_v3/cd_interface_gate.py`（唯一被修对象）。改后跑 `python -m dataset_v3.cd_interface_gate`（或 72 号路径）确认骨架自检仍过（NON-FORMAL 域）。

### 2.2 formal C/D Gate 执行（EXT-CD-GATE）

D-15 完成后：对 C 侧 confirmation/prequal 语料执行 formal C/D Gate（`cd_interface_gate.py` + C 侧 72 号路径）。Gate overall=true ⇒ EXT-CD-GATE 达成 ⇒ C spec 可标 implementation-complete。

### 2.3 T-432 Unblock Checklist 清点（`docs/phase4/d_dataset_v3/t432_unblock_checklist.json`）

| 项 | 状态 |
|---|---|
| ① C Mission-v3 完成 | ✅（PR #22 merged） |
| ② C/D Interface Gate overall=true | ⏳ 待 D-15 + formal Gate |
| ③ producer schema accepted | ✅ |
| ④ D0 pilot qualification PASS | ✅ |
| ⑤ formal fleet plan 预注册 | ⏳ 待 activation record |
| ⑥ OOD 定义预注册 | ⏳ 待 roadmap amendment |
| ⑦ 物理基线完整性 | ✅ |
| ⑧ 控制基线完整性 | ✅ |

### 2.4 Downstream scientific-roadmap amendment（T-432 activation 前用户裁决）

T-429 FINAL FAIL 后，D/E/F 的 contrast/OOD 框架需要一次路线裁决。裁决文件届时由主 Agent 起草（建议路径：`.specs/phase4-d-dataset-v3/scientific_roadmap_amendment_v1.md`），内容 = 三层负结果影响评估 + 可选路线。**裁决 deadline：T-019 Stage B formal fleet 生成前**。

三条候选路线（供预告，非最终选项）：
- (a) 照原设计继续：T-429 负结果作为已知限制入档，contrast/OOD 框架不变，formal 语料照生成；
- (b) 信号层补强：P_prop 专用通道 / SBD 度量校准预实验 → T-429 重设计（需 C 包 amendment）；
- (c) 对比框架降级：required contrasts 缩减至探针可达子集，OOD 框架重定义。

## 3. 纪律（与 D0 相同）

- Model E 零改动；`dataset_v3/` 既有件只读（D-15 修 cd_interface_gate.py 为唯一例外）；
- 逐文件 add；禁 force-push；Gate FAIL 即停升级；
- 绝对路径/ASCII 扫描零命中。

## 4. 完成定义

- D-15 完成 + formal C/D Gate overall=true → EXT-CD-GATE 达成 → C spec implementation-complete；
- T-432 checklist 全绿 → T-018 Stage A 解锁；
- roadmap amendment 用户裁决 → T-019 Stage B 解锁；
- T-020 审计冻结 → D 包 implementation-complete → E/F 解锁。
