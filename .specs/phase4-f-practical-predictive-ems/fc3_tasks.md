# FC-3 Tasks（TG1–TG7 已并入，**TASKS APPROVED**）

状态：**FC-3 Tasks Gate：APPROVED WITH REQUIRED CORRECTIONS TG1–TG7（已全部
并入本稿）；授权自主执行 T-FC3-0..T-FC3-6；T-FC3-7 执行保持 BLOCKED。**
实现语言决策：FC-3 全层（checker/视图/重放/资格化）采用 Python 单一实现；
v2 非目标谓词忠实性由"逐行移植参考实现 + 与 192 条记录内嵌的真实
v2-MATLAB 判定向量全量差分"双重证明（见 TG4 落地）。
上游：FC-3 Design **APPROVED**（D1–D6 已并入 `fc3_feasibility_contract_amendment.md`）。
执行约束：实现类任务在 FC-3 Tasks Gate 通过后才可动工；重放类任务（T-FC3-7+）
另有独立人工门。

## 任务 DAG

```
T-FC3-0 源推导冻结 + 重放输入 schema ─┬─> T-FC3-1 FC-3 checker 实现
（包络推导工件冻结；                 │      （动态包络 + E_unserved 硬判 +
  FC3_REPLAY_INPUT_VIEW schema）      │        跟踪诊断 + 谓词不变族）
                                     └─> T-FC3-2 净化重放输入视图
                                            （白名单投影 + 静态/运行时守卫）
T-FC3-1 ─┬─> T-FC3-3 Q-FC3 资格化套件（Q-FC3-1..20，合成 fixture）
T-FC3-2 ─┘        │
                  └─> T-FC3-4 锁定重放工具（只读 epoch-2 证据；D4 双状态并列；
                         D5 parity 按 SHA 绑定；1Hz 离散裁决语义）
                         │
                         └─> T-FC3-5 重放资格化/不可变性证明
                                （原始证据前后哈希复验；输入视图盲视证明）
                                │
                                └─> T-FC3-6 FC-3 重放预执行冻结
                                       （新冻结根：绑定 checker/视图/工具/
                                         资格化工件 + 原冻结根 8d6106ff）
―――――― 人工 / ChatGPT Tasks/Replay Gate ――――――
T-FC3-7 执行 FEASIBILITY_RE-ADJUDICATION_EPOCH_1        [BLOCKED]
T-FC3-8 重放验收（192/192 重放输出 + 身份/盲视/不可变性核验）[BLOCKED]
T-FC3-9 效应分析资格门                                   [BLOCKED]
```

## 任务清单

| ID | 任务 | 依赖 | 产出 | 约束 |
|---|---|---|---|---|
| T-FC3-0 | 冻结源推导（含 TG1：192/192 锁定证据结构普查——仅限白名单结构/可行性字段的充分性证明；TG3：精确离散 PMIN 裁决语义冻结）（`fc3_pmin_envelope_derivation.json` 定稿 + ε 推导 + 1Hz 裁决语义）与 `FC3_REPLAY_INPUT_VIEW` schema（白名单字段清单） | Design Gate | .specs 两工件 + schema JSON | 不改任何科学工件 |
| T-FC3-1 | FC-3 checker 实现（`ppe.feasibility_contract_v3`）：E_unserved 硬判（D2）、derate 诊断化、跟踪诊断（D3）、动态 PMIN 包络（D1）、其余谓词族与 v2 逐位等价 | T-FC3-0 | scripts/+ppe/feasibility_contract_v3.m | 不接入任何正式/重放路径（Gate 后才接） |
| T-FC3-2 | **物化**净化重放输入投影（每 run 一个不可变 JSON，被禁字段**物理缺席**；臂身份与检查器载荷分离——wrapper 保留溯源、checker 核心不接收臂身份） | T-FC3-0 | fc3_replay_input_view/<run_id>.json | 绑定源 run/traj/audit/视图 schema/视图自身 SHA |
| T-FC3-3 | Q-FC3-1..28 资格化（TG7 增补 21–28：确定性/ON 重置/精确 dt 语义/全量盲视/无臂分支/非目标逐位差分/陈旧哈希 fail-closed/不可修改）+ 非目标 v2/v3 差分回归（合成 fixture + 192 视图对记录内嵌 v2 向量，不产生最终重判状态） | T-FC3-1, T-FC3-2 | qualification/fc3_qual.json | 措辞修正（TG1）：**0 结果承载裁决；仅为证明重放充分性而允许对不可变 epoch-2 证据包做结构检视** |
| T-FC3-4 | 锁定重放工具：只读 epoch-2 证据包 → `readjudication_epoch1/` 输出（D4 字段族、D5 parity SHA 绑定、1Hz 保守裁决） | T-FC3-3 | python/ppe/fc3_replay.py | 不改写任何原始工件 |
| T-FC3-5 | **192/192 全量**（TG6 禁抽样）：run_id 唯一性/冻结根/终端身份/traj+audit+view SHA 逐字节复验/必需证据存在/被禁字段缺席/原始工件前后哈希不变 → fc3_replay_input_manifest.json（每 run 一行） | T-FC3-4 | qualification/fc3_replay_qual.json + manifest | 清单 SHA 入 T-FC3-6 冻结 |
| T-FC3-6 | 预执行冻结（TG7 全清单：checker/推导/ε/离散策略/schema/视图实现/192 清单/普查/重放工具/资格化/差分工件/原根 8d6106ff/parity 工件/语料身份/台账快照）+ 显式断言 SIMULINK_RERUN_REQUIRED=false、LOCKED_REPLAY_ELIGIBLE=true、FUEL_OUTCOMES_ACCESSIBLE_TO_FC3_CHECKER=false、ORIGINAL_EPOCH2_EVIDENCE_MUTABLE=false——任一不可证明即 overall=false 并 STOP | T-FC3-5 | freeze/fc3_readjudication_freeze.json + GATE | 任何断言失败 → STOP |
| T-FC3-7 | 执行 FEASIBILITY_RE-ADJUDICATION_EPOCH_1 | **人工 Tasks/Replay Gate** | readjudication_epoch1/ 全量输出 | 只读原始证据 |
| T-FC3-8 | 重放验收（192/192、身份/盲视/不可变性、原始分类并存核验） | T-FC3-7 | ACCEPTANCE + 归档 | 不做效应分析 |
| T-FC3-9 | 效应分析资格门 | T-FC3-8 | 门裁决 | 独立人工授权 |

## 禁止项（全程）

不重跑 Simulink；不改 Model-E/PMS/C*/Predictor/任务/轨迹/FEC；不计算任何
B/L/R/节油/CI/臂排名；重放不读 comparative fuel/J_FEC；原始 epoch-2 分类与
工件一字不动。


## TG 落地补充

- **TG2**：视图物化 + 溯源/载荷分离（checker 核心载荷不含 arm）；序列数据经
  白名单通道守卫式加载器从冻结轨迹读取（轨迹文件物理上不含任何燃料通道）。
- **TG3**：离散语义精确冻结式 `dt_lower = max(0, t_sample − t_first_observed_ON − 1.0)`
  （每个连续 ON episode 独立取原点；OFF/SYNC/重启重置）；规范策略 = 全部
  航次统一 1Hz（不与高速率混用）；ε=0.11kW 附机器可读证明。
- **TG5**：`readjudicated_status ∈ {READJUDICATED_FEASIBLE,
  READJUDICATED_HARD_INFEASIBLE, READJUDICATION_INVALID}`；三重状态并列呈现。
- 谓词复算/继承分类：RECOMPUTED_1HZ（service/PMIN 包络/PMAX/非ON/SOC 硬界/
  终端带与储备/P-over-SOP/task_complete）、RECOMPUTED_NATIVE_AUDIT（ramp、
  电池 V/I）、INHERITED_V2_RECORDED（safety_hard_events、energy_closure_c5
  ——其 0.01s 原生输入不可由 1Hz 证据重构，按不可变记录携带并标注溯源）。
