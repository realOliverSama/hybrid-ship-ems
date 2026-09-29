# T-429 EXECFIX-02 worktree 重跑运行注记（RUN_NOTE）

- generated_at: 2026-09-29（P5 施工段续，C6 阻断裁决落地执行）
- 执行者: 子代理（P5 施工）；主 Agent 裁决: 冻结链完整快照态重跑（类 worktree 隔离，零治理变更）

## 1. 运行树态

- 树: `/tmp/t429_eval_wt` = git worktree @ **8a1ed398**（"EXECFIX-01 evaluator conformance 修复 + T-020 遗漏件补提交"）
- 树内容 = 主 Agent 打入的 EXECFIX-02 补丁：
  - `scripts/+mission_v3/+audits/identifiability.m`（Fix 1: DIR cohort `reach (same)`；Fix 2: probe 类分层确定性贪心折分配 + `res.fold_assignment` provenance）
  - `scripts/+mission_v3/tests/run_all.m`（FD1 套件表 ten→eleven，追加 probe_fold_stratification）
  - `scripts/+mission_v3/tests/test_probe_fold_stratification.m`（新套件，7 case）
  - **执行期修正 1（测试 bug，两树同步）**：P2 段曾复用变量名 `pf_idxA/pf_idxB` 导致 P5（复用 P1 池）索引越界（>1392）；已改为 `pf_idxA1/pf_idxB1`（P1 专用）与 `pf_idxA2/pf_idxB2`（P2 专用）。仅测试文件，不触及审计实现。主树副本已同步修正。
- **执行期补齐 2（快照态缺件，置顶披露）**：8a1ed398 提交携带的 `results/mission_v3/qualification_stack_identity.json` 是 09-06 07:40 旧版（generator=dea417fc / composite=db5da613，文件 sha=a9923178…），而 t429_freeze_v2 钉的文件 sha=**bbd0a8fd…**（composite=19180c21）版本只存在于 09-07 02:33 gate 运行时的工作区、**从未入库**。处理：以 2d84ad2e 版文件为字节模板，枚举 generated_at 时间戳（2026-09-06 12:00:00～09-07 02:34:00 +0800，40742 候选）精确重建出 sha256==bbd0a8fd… 的文件字节（**generated_at=2026-09-06T23:19:01+0800**，sha256 碰撞概率 2^-256，即原文件字节）；拷入 worktree。零治理变更：未动 freeze/registry/语料/阈值/种子任何字节；该文件内容=EXECFIX-01 gate 运行时机器验证 ok 的那份登记（重建文件留档 `qualification_stack_identity.rebuilt_bbd0a8fd.json`）。佐证：补齐后 worktree Strict 重收集 composite=19180c21 与 freeze 期望一致（8a1ed398 树内容=freeze 态，主 Agent 时点判定被证实）。
- 语料：`datasets_v3` → 主树 symlink（内容不变）；运行时 manifest sha256 校验 ok（144 voyages，freeze 期望 bcf4e1a7… 命中）。

## 2. 三段机器结果（尾部日志）

### 2.1 回归 run_all('run')

```
[R] suite validate_registries  PASS cases=9
[R] suite seed_tree            PASS cases=8
[R] suite env_realize          PASS cases=8
[R] suite plan_compose         FAIL error=: GATE:test_plan_compose 1 case(s) failed
[R] suite forecast_issue       PASS cases=8
[R] suite kf_snapshots         PASS cases=7
[R] suite family_expand        PASS cases=8
[R] suite stack_identity       PASS cases=7
[R] suite producer_export      PASS cases=8
[R] suite negative             PASS cases=10
[R] suite probe_fold_stratification PASSED (cases=7)
[R] suites 10/11 passed
```

- `test_plan_compose` P2 失败 = **基点自带已知项**（8a1ed398 未含 3b83eafe 的 test 修复"排除 traffic 事件样本"；实测 `P2 bridge-vs-harbor: phases=5 plateau=2.450 (0.7*3.5) harbor_mode=1.579 zones=1: FAIL`——T429-AMEND-V2 traffic 事件时长 U[225,320] 冻结后 follow/plateau 模态可主导 bridge 实现窗，3b83eafe 已以纯 test 侧修复解决并声明与实现/OOD 改动无关）。与 EXECFIX-02 补丁零关联（补丁不触及 plan_compose 实现路径）。
- 主 Agent 停止条件所列 stack_identity / producer_export / negative 三套件在该树 **全部 PASS**（7/7、8/8、10/10）——时点判定证实。
- 过程注记：run_all 组合运行曾出现 2 次 MATLAB 进程瞬态崩溃（suite 9 附近，crash dump /Users/oliverlyu/matlab_crash_dump.63121-1）；单独重跑 producer_export/negative 均稳定 PASS，第三次组合运行全程跑完——判定为宿主偶发（多套件连跑内存/JVM 累积），非代码问题。

### 2.2 冻结机检 verify

```
[81] T429 verify_freeze: overall=1
```

无 FREEZE_VIOLATION。freeze_verification 全 ok：freeze self_hash ✓ / amendment_v2 ✓ / contrast registry ✓ / heldout token ✓ / 语料 manifest sha256+voyages=144 ✓ / 栈文件 sha256(bbd0a8fd) ✓ / 栈复合(19180c21) ✓。

### 2.3 全量 evaluate

```
出错 mission_v3.audits.identifiability (第 205 行): out = local_evaluate(ROOT);
（GATE:MISSION_V3:T429:FAIL 受控抛出——overall=false 设计内行为；产物先落盘）
```

- 新 verdict：**overall=False，FAIL_METRIC，required 0/6**（旧：FAIL_INSUFFICIENT_EVIDENCE 0/6）
- freeze_verification 全 ok=True；structural_conformance.all_ok=True（C1 DIR paired groups=60 ≥ min cov 10；C2 TRAFFIC 纯宿主 214/214；C3 DEPTH 严格选择 12+12）

## 3. 六个必需对比逐项新旧对照

| contrast | OLD (EXECFIX-01 v2conf) | NEW (EXECFIX-02) | 身份变化 |
|---|---|---|---|
| SEG_BRIDGE_VS_HARBOR | METRIC_FAIL；R=0.6815，BA=0.7656/lcb 0.6844 | **数值逐位不变** | 无（VALID FAIL 维持） |
| SEG_BERTH_VS_DEPARTURE | METRIC_FAIL；R=0.8231，BA=0.9900/lcb 0.9712（probe PASS，SBD FAIL） | **数值逐位不变** | 无 |
| DIR_UPSTREAM_VS_DOWNSTREAM | METRIC_FAIL；R=0.6252，BA=0.6866/lcb 0.6354；paired dP=−94.3 kW，sign 14/60 p=1.0 → pe FAIL；**INVALID/UNRESOLVED（实现缺陷）** | METRIC_FAIL；池 reach 匹配后 R=0.6679，BA=0.6390/lcb 0.5863；**paired dP=+85.6 kW，CI95 [+61.1,+113.1] 排零，sign 49/60 p=3.8e-7 → pe PASS**；probe FAIL(<0.80) | **INVALID → VALID METRIC_FAIL**（实现缺陷修复完成；物理方向效应确证，混合池 probe 判据未达冻结 A_min=0.80 → 按冻结 gate_profile 判 FAIL，科学结论） |
| DEPTH_LOW_ID_VS_NORMAL_ID | **INSUFFICIENT**（probe 无效折）；R=0.5461 | METRIC_FAIL；R=0.5461（SBD 不含折，不变）；probe 全折有效 fold_ba=[0.498 0.476 0.472 0.511 0.483]（≈chance），BA=0.4880/lcb 0.4618 | **INSUFFICIENT → VALID METRIC_FAIL**（折分层修复生效；SBD+probe 双判据真实未达 → 深度可辨识性负结果，触发预冻结后果 depth_still_fail_legal_cohort） |
| SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE | METRIC_FAIL；R=0.6217，BA=0.9547/lcb 0.9161 | **数值逐位不变** | 无 |
| SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE | METRIC_FAIL；R=0.5049，BA=0.7305/lcb 0.6648 | **数值逐位不变** | 无 |

- DIR 行 `cohort_applied` 含 `reach (same)` ✓；n_groups A/B=60/60（reach 匹配后全部保留，min cov 10 ✓）；mixed cells=60（=每 family 唯一 OPEN_WATER cell，符合预期）。
- DEPTH probe_status=OK，fold_ba 无 NaN ✓（INSUFFICIENT 根因消除）。
- **结论翻转检查**：四项共享组对比数值逐位不变（shared 组折划分与旧算法一致，符合设计）；唯 DIR/DEPTH 两项按修复设计变化；**零意外翻转**。
- overall 判定语义变化：0 项 INSUFFICIENT 残留，verdict FAIL_INSUFFICIENT_EVIDENCE → **FAIL_METRIC**（六项身份全部 clean：0 INVALID/UNRESOLVED、0 INSUFFICIENT）。T-429 overall 仍 FAIL → 按 P5 预案归档 NO-GO 边界（六项均为冻结语料上的真实科学结论），T-432 登记维持未解锁状态。

## 4. 产物清单（本目录）

- `identifiability_gate_result.json` — EXECFIX-02 全量运行机器产物（generated_at 2026-09-29T05:00:03Z）
- `identifiability_report.md` — 人读报告
- `regression_report.json` — run_all 10/11 状态留痕
- `qualification_stack_identity.rebuilt_bbd0a8fd.json` — 重建的 freeze 钉版栈登记（sha256=bbd0a8fd…，见 §1 补齐 2）

## 5. worktree 当前未收口状态（供主 Agent 处置）

- `/tmp/t429_eval_wt` 工作区含：EXECFIX-02 补丁（identifiability.m/run_all.m）、新测试（含 P5 索引修正）、栈登记文件（2d84ad2e git 版被重建的 bbd0a8fd 版覆盖）、run_all 运行产物（regression_report.json + plan_profile_examples PNG 刷新）。**未做任何 git commit**（按红线留给主 Agent）。
- 主树 EXECFIX-02 改动保持未动（identifiability.m/run_all.m/新测试；测试文件已同步 P5 索引修正）。
