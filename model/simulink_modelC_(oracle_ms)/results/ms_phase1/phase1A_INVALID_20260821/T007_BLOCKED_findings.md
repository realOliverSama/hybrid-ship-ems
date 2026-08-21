# T-007 Phase 1A 阻塞调查记录（2026-08-21）

状态: **BLOCKED — 结果不可信，需主 Agent 决策**（T-004 实现缺陷）

## 现象

完整批次 `scripts/21_run_ms_horizon.m` 78 次仿真全部落盘、回归门禁 PASS（Nominal×S04 C0 与冻结基线 max-rel-diff=0），
但部分结果物理上不可能：

| 配置 | 批次值 | fresh 独立会话值 | 差异 |
|---|---|---|---|
| Nominal S04 law=3 (CP/CMS) | 74.93 / 81.52 | 91.172 (=baseline) | law=3 是否激活 MinFF 不一致 |
| Nominal S07 C0 | 61.59 | 78.358 (=frozen) | **C0 也错** — 船速轨迹从 t=0.5s 起分歧 |
| Fast S04 C0 | 81.97 | 91.62 | C0 错 |

同一配置、同一脚本逻辑，跨独立 MATLAB 会话结果不同 → **law=3 运行不可复现**。

## 根因

`MSFF_Horizon_s` / `MSFF_InfoMode` 在 T-004 中实现为 **Stateflow.Parameter 作用域数据**
（`setMFCode` 第三参数字段，`d.Scope='Parameter'`），是**编译期参数**，在 sim() 循环中改变 base workspace
值时，MATLAB Function 块图表**是否重新编译取决于 Simulink 的变更检测**，而 `set_ai_logging` 在每次
run 间改动模型端口记录状态进一步干扰检测 → 参数切换不生效 → 行为随机取决于会话历史。

同时确认的连带现象：**logsout 跨 sim 累积陈旧元素**（`P_ff_applied max=1000` 但 fuel=baseline 的
错位信号），以及 C0（law=0）在 S07 场景受污染（船速从 t=0.5s 分歧，但电气信号一致——疑似
`sim()` 复用陈旧编译状态/初始条件）。

## 证据文件

- `results/ms_phase1/phase1A/runs.csv`（78 行，其中 S07 全部 + Fast/Slow 锚点 C0 不可信）
- `results/ms_phase1/phase1A/ds_*.mat`（每 run 原始 ds，保留供调查）
- `/tmp/repro_s04.m`（稳定复现 law=3 激活：74.93/81.52）
- `/tmp/det_test7.m`（稳定复现 law=3 不激活：91.172）
- `/tmp/bisect2.m`（证明显式 assign EMS_CTRL_LAW=3 后 P_ff max=1000 但 fuel 不变 → logsout 错位）
- `/tmp/bisect3.m`（ActiveVariant=3 确认、P_ff_raw max=1000、R_max max=428）
- `/tmp/fix_test2.m`（close/reload 每 run 后 law=3 仍给 ffmax=1000 + fuel=baseline → 非 reload 问题）
- `/tmp/ff_when.m`（ff 仅 t=3-12s 冷启动段激活，被爬坡限幅吸收 → 物理自洽）

## 候选修复（需主 Agent 决策）

1. **推荐**: 将 `MSFF_Horizon_s` / `MSFF_InfoMode` 从 Parameter 改为 **chart 输入端口**
   （由上游 Constant 块或信号线驱动），编译期不变，运行时切换可靠。涉及 build_model_cms.m
   CODE_MIN_FF / CODE_AI_PREDICT_MS 签名 + 接线，属 T-004 返工。
2. 每次 run 前强制 `set_param(mdl,'SimulationCommand','update')` 强制重编译（验证是否可靠）。
3. 每次 run `close_system + load_system`（fix_test2 证明不够——仍错位，不推荐单独使用）。
4. 复查 `set_ai_logging` 对变体端口的 DataLogging 操作是否破坏编译缓存。

## 影响

- 本批次 phase1A 数据全部不可用（S04 law=3 值跨会话不一致；S07/Fast/Slow 的 C0 受污染）。
- T-007 验收 FAIL。T-004 需返工（根因在该任务）。T-008（horizon 选型）依赖 T-007 数据，顺延。
- 已确认物理事实（不受影响）: MinFF 在 Nominal 参数下仅冷启动段激活且被爬坡吸收 ≈ baseline；
  peak/traj 信息差异需在弱机参数区（Phase 1B）才显现。

## 建议下一步

主 Agent 决策候选修复 1~4 后，先修 T-004（含复现性回归：同一配置跨 2 会话必须逐位一致），
再重跑 T-007。
