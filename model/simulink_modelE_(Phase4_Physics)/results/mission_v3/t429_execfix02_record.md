# T-429 EXECFIX-02 记录（P5 审计实现修复轮 · 2026-09-29）

> 性质：**evaluator conformance repair**（EXECFIX-01 同类先例），非第三轮证据生成——
> 语料/registry/判据/阈值/种子/栈复合全部零改动。授权来源：用户 P5 指令
> （"按 +mission_v3 生成器修对比实现缺陷 → … → 重跑审计"）。
> Day-3 FINAL Ruling 的"no third T-429 round"指证据生成轮；本轮仅修审计实现并重跑评估。

## 1. 修复内容（2 项，均实现冻结文本的既定语义）

| # | 缺陷 | 修复 | 依据 |
|---|---|---|---|
| Fix 1 | DIR 对比 cohort 缺 reach 匹配（identifiability.m `local_contrast_specs` L1363-1366 + `local_paired_effect_eval` L2341-2351 组级均值无段级匹配）→ 段构成混杂反转符号（实现口径 5/60 正 vs reach 口径 55/60 正） | DIR cohort 增补 `local_pred('reach','same')`（1 行） | 冻结 registry DIR 行 `nuisance_controls: "same regime x same reach"` 原文 |
| Fix 2 | probe 折分配类盲贪心（`local_probe_eval` L2047-2059）在 DEPTH 24 个近等大纯类组上分出缺类测试折 → INSUFFICIENT | 确定性类分层贪心（shared 组退化原总量平衡） | DGN-12 既定折性质（每折 ≥2 独立组/类）的合格实现；冻结规则"grouped 5-fold by split_group_id"不变 |

新增 `tests/test_probe_fold_stratification.m`（7 case）+ run_all 接入。

## 2. 重跑树态（freeze 链完整性的合法满足路径）

- T432_ACTIVATION_AMENDMENT_01/02（3b83eafe/2497bc6c，用户授权链）使 HEAD 的栈复合
  与 t429_freeze_v2 的 C6 钉（composite 19180c21…）漂移——HEAD 上 verify 恒
  FREEZE_VIOLATION（C1~C5 全过，语料/registry 钉无损）。
- 处置：**在冻结链完整快照态重跑**——git worktree @ 8a1ed398（EXECFIX-01 提交点，
  C6 一致）+ EXECFIX-02 补丁（+audits/tests 被 FD2 裁决排除在栈复合外，零影响）
  + datasets_v3 symlink（语料字节不变，manifest sha256 运行时逐项机检过）。
  治理零变更：无 freeze amendment、无门禁机制改动、无判据改动。
- `qualification_stack_identity.json`：freeze 钉的 sha256=bbd0a8fd… 版从未入库
  （入库版 d1475a63 与钉版仅 generated_at 不同）；本提交把钉版字节（sha256 校验
  一致）纳入主树——恢复冻结链在主树的可验证性，无其他引用方受影响（grep 实查仅
  t429_freeze_v2/gate JSON 引用该哈希）。

## 3. 机器结果（worktree 三段）

- 回归 10/11 PASS（唯一 FAIL=plan_compose P2 为基点 8a1ed398 自带已知项，
  3b83eafe 已有 test 侧修复、与本轮无关；主树三套件 stack_identity/producer_export/
  negative 在快照态全 PASS——证实其 HEAD 失败纯由栈漂移）；新套件 7/7 PASS。
- verify：overall=1，无 FREEZE_VIOLATION（C6 栈复合在快照态通过）。
- 全量 evaluate：产物落盘后受控抛 `GATE:MISSION_V3:T429:FAIL`（设计内）。
  freeze_verification 8/8 ok、structural_conformance all_ok=1。

## 4. 终判（六项身份全部 clean：0 INVALID/UNRESOLVED、0 INSUFFICIENT）

| 对比 | EXECFIX-01 身份 | EXECFIX-02 身份 | 关键值 |
|---|---|---|---|
| DIR_UPSTREAM_VS_DOWNSTREAM | INVALID/UNRESOLVED（实现缺陷） | **VALID METRIC_FAIL** | paired effect **PASS**：ΔP=+85.6 kW、CI95 [+61.1,+113.1] 排零、sign 49/60 p=3.8e-7；probe BA_lcb95=0.586 < 0.80 → 按冻结 gate FAIL。方向效应真实存在，混合池 probe 判据未达=科学结论 |
| DEPTH_LOW_ID_VS_NORMAL_ID | INSUFFICIENT（实现伪影） | **VALID METRIC_FAIL** | probe_status=OK、fold_ba 5 折无 NaN（≈chance 0.47-0.51）；R=0.5461 不变。深度可辨识性负结果成立（amendment 预冻结后果 depth_still_fail_legal_cohort 适用） |
| 其余 5 项 | VALID FAIL | VALID FAIL（**逐位不变**） | shared 组折划分同旧算法，R/BA/LCB95 零漂移 |

- overall=False，verdict：FAIL_INSUFFICIENT_EVIDENCE → **FAIL_METRIC**（0/6）。
- **T-429 仍 FAIL ⇒ T-432 维持不解锁**（research integrity 项如实归档）；
  verdict 原表述"DIR INVALID / DEPTH 组数不足"按本记录更正为"均为 VALID 科学结论"。
- NO-GO 边界（论文材料）：Model E/H1 当前保真度下，六对比的可辨识性判定全部
  可判定且全 FAIL——其中 DIR 的方向物理效应经配对检验证实存在（+85.6 kW，
  p=3.8e-7），未达 probe 判据；DEPTH 在当前组规模/保真度下不可辨识。

## 5. 产物

- 主树：`identifiability.m`（Fix1/2）、`tests/run_all.m`、
  `tests/test_probe_fold_stratification.m`（新）、本记录、
  `identifiability_gate_result.json`/`identifiability_report.md`（新终态）、
  `qualification_stack_identity.json`（钉版字节）、`t429_execfix02_run_note.md`
  （worker 执行记录）、`t429_execfix02_regression_report.json`（快照态回归）、
  `t429_fix_spec_draft.md`（诊断规格，EXECFIX-02 设计前身）。
- 保全：EXECFIX-01 v2-conformance 证据在 git 历史（996c5e32/2d84ad2e）原样可溯。
