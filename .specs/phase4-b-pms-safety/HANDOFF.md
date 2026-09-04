# HANDOFF — Phase 4-B 施工移交（新 session 开工提示词）

> 写给接手施工的 session（可能是不同模型）。本文档 + spec 四件是自包含开工包。
> 创建：2026-09-04（B 包 spec 四门禁收官 session）。

## 0. 开工仪式（必须先做）

```bash
git fetch origin && git status -sb && git log --oneline -3
git checkout feat/phase4-b-pms-safety   # 若不在该分支
```

- 工作区未跟踪目录 `firmware/.../Host/fault_out/`、`model/simulink_modelD_(EMS_NN_AECMS)/slprj_v1_stale/`
  为既定不入库产物，**不清理、不提交、不担心**；
- 工作区若有其他未提交改动 → 停止报告用户（§0 纪律）。

## 1. 授权状态

- spec `.specs/phase4-b-pms-safety/` 四门禁**全部用户书面批准**
  （brainstorm / requirements / design / tasks-approved）；
- `spec_validate phase=implementation` **14/14 通过**（2026-09-04）——施工授权生效；
- 施工范围：tasks.md **T-001~T-013 自主执行**；**T-014 用户评审门必须停下**；
- subagent：施工阶段用户已授权使用 delegate 并行（T-002/003/004 三链）；
  主 Agent 保留决策、builder 集成（T-007 独占）、验收（§14）。

## 2. 必读材料（按序，禁止凭记忆施工）

1. `.specs/phase4-b-pms-safety/brainstorm.md`（D-B1~D-B9 裁决原文 + 终审修正）
2. `.specs/phase4-b-pms-safety/requirements.md`（FR-428~457 / NFR-412~419 / AC-416~424；
   含 design 联动修订 FR-445 S2/S5）
3. `.specs/phase4-b-pms-safety/design.md`（DD-1~DD-9 + 偏差 D-B10~D-B15；**全部公式/参数/
   伪代码的唯一事实源**）
4. `.specs/phase4-b-pms-safety/tasks.md`（T-001~T-014 任务书 + 依赖图 + 执行约束）
5. A2 spec（`.specs/phase4-a2-model-e-construction/`）接口契约：FR-421（adapter 替换位）/
   FR-424（SOP 同名同义）/FR-426（七变体兼容）/design D-4（min up/down 属控制层）/
   D-13（禁 ensure_canonical）
6. `model/simulink_modelE_(Phase4_Physics)/README_Model.md`（模型结构现状）
7. `项目开发进展.md` 2026-09-04 条目（参考文献链 [1]~[19] + R2026a 工程坑清单）

## 3. 关键纪律（违反=返工）

- **Plant 零改动**：03/04/06/08/09/10/11/13/14/15 与 `ModelE_Physics_Parameters.m`
  一律不动；T-412 重跑背书，**禁改参数凑 PASS**；Gate FAIL → `error('GATE:*')` 停升级；
- **Model D 目录零改动**（sha256 持续证明）；
- `build_model_e.m` **主 Agent 独占**；delegate 只交独立文件（块源码字符串/单测/
  独立脚本），交接面 = design §5 函数签名/端口清单/伪代码；
- R2026a 坑：句柄式 addL / persistent 块 EMChart DISCRETE / From Workspace
  Interpolate=off / 数字开头脚本 `eval(fileread(...))` / 注入用 assignin /
  regexp `.*` 跨行 / struct() cell 展开；
- canonical 功率域 = **genset-side**（DD-1）；bus-side 量仅在 `X_b2g_*` 显式环节
  ÷η_rect=0.97 换算，禁混域运算；措辞纪律（battery genset-equivalent ≠ 物理过 rectifier）；
- Git：逐文件 add（禁 -A/.）、禁 push -f、禁 merge（B 收官 PR 由用户审批）；
- 每任务完成立即 `spec_task_check`；delegate 越界/歧义立即停升级（§5 格式）。

## 4. 冻结参数速查（施工时仍以 spec 原文为准）

Baseline Demand：LPF τ=60 s；SOC_lo=0.55（`<=`进）/SOC_recovered=0.60（`>=`出）/
初值 false；P_chg=100 kW（generator-equivalent）；non-firm 削减
`max(0,min(100,500·N_dispatch−P̄_load))` 只在 Baseline source。
Commitment：P_commit=LPF(P_load)（不含恢复偏置）；start>425 kW×30 s；
stop<350 kW×120 s 且 t_ON≥300 s；min_up=300/min_down=0；min-up 与 stop-hold
**并行计时**（最短 ON=恰 300 s）；仲裁 `veto>safety>normal`；v1 无 0DG。
Dispatch：`is_dispatching=(ON∧eligible)` 单一谓词分母输出共用；均分
clamp[125,500]；诊断 unserved/surplus/min_clamp_active。
Safety：S1a 非预期丢失 / S1b 启动失败 40 s / S2 `max(0,P_load−ΣP_DG_actual)>
SOP_dis`×2 s / S3 盈余（双健康机才 FORCE_STOP，否则 flag-only）/
S4 `P_load,req+50>P_firm,avail`×2 s / S5 EMS_HEALTH_TS∧isfinite，仅
EMS_AGGREGATE 模式报警，锁存；UnitDelay 0.01 s 防环；08 唯一推进限制器。
Manual：`manual_power_cmd_pct∈[0,1]` 线性 ×700 kW + rate limit 20 %/s；
M1 step 30→70%；M2 ramp 20→80%；M3 30→70→30%（120 s/240 s/540 s 段长，
start cmd≈178 s、DG2 ON≈208 s、stop cmd≈615 s）。
Gate：43 号（determinism/规则合法性+episode 一对一/SOC±0.05 P2 行为 Gate/Vdc 分层/
能量闭合/S1~S5 断言/审计/T-412 重跑）+ 44 号负向；基线 candidate→promotion
两阶段（DD-9）；三臂 R0/R5/P2，R0/R5 燃油不参与 PASS/FAIL。

## 5. Git 拓扑（重要）

- A2 已由 PR #19 merge 进 `Phase4`（SHA 重写式合并，新头 6940b426）；
  **树内容与 A2 头 4faf9a87 逐位一致**（已实证 `git diff --quiet` 为空）；
- B 分支基于旧 SHA 4faf9a87——因禁 force-push **不 rebase**；树等价 ⇒ 收官 PR
  （base=Phase4）应零冲突，创建 PR 时如发现意外冲突立即停止报告用户；
- B 分支远端跟踪正常（origin/feat/phase4-b-pms-safety）。

## 6. 挂账事项（不属于本施工包但须记住）

- A2 spec design.md **D-14 偏差条目补记**在 T-013 执行（仅追加，不动其他内容）；
- 500 t 实船确认挂账（`docs/data_request_list.md`），到位后走 spec 修订；
- 03 号脚本 MVP 遗留（ensure_canonical 依赖）适配/隔离待 D 包；
- Model D/E 绝对量不可比是设计意图（只允许方向性/守恒性对照）。

## 7. 收官动作（T-013/T-014）

- §15 汇报模板 + §18 开发日志（含延伸问答记录）+ §20 README 两级同步 +
  绝对路径扫描零命中；
- T-014：向用户提交双 Gate 证据 + candidate diff + 三臂报告；**书面批准后**才执行
  promotion（candidate→v1.1）+ 控制基线 v1.0 冻结 + spec implementation-complete +
  创建 B 包 PR 交用户审批。
