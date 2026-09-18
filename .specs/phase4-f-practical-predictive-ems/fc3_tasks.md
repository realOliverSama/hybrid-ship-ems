# FC-3 Tasks（DRAFT，待 FC-3 Tasks Gate 评审）

状态：**DRAFT——本文件即 FC-3 Tasks Gate 评审对象**。
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
| T-FC3-0 | 冻结源推导（`fc3_pmin_envelope_derivation.json` 定稿 + ε 推导 + 1Hz 裁决语义）与 `FC3_REPLAY_INPUT_VIEW` schema（白名单字段清单） | Design Gate | .specs 两工件 + schema JSON | 不改任何科学工件 |
| T-FC3-1 | FC-3 checker 实现（`ppe.feasibility_contract_v3`）：E_unserved 硬判（D2）、derate 诊断化、跟踪诊断（D3）、动态 PMIN 包络（D1）、其余谓词族与 v2 逐位等价 | T-FC3-0 | scripts/+ppe/feasibility_contract_v3.m | 不接入任何正式/重放路径（Gate 后才接） |
| T-FC3-2 | 净化重放输入视图实现：终端记录→白名单投影（不可变），静态+运行时守卫（D6） | T-FC3-0 | python/ppe/fc3_input_view.py | 被禁字段访问 fail-closed |
| T-FC3-3 | Q-FC3-1..20 资格化套件（全合成/非正式 fixture；含包络数值对照与长渐近案例） | T-FC3-1, T-FC3-2 | qualification/fc3_qual.json | 0 正式语料消费；Q-FC3-18/19 盲视证明 |
| T-FC3-4 | 锁定重放工具：只读 epoch-2 证据包 → `readjudication_epoch1/` 输出（D4 字段族、D5 parity SHA 绑定、1Hz 保守裁决） | T-FC3-3 | python/ppe/fc3_replay.py | 不改写任何原始工件 |
| T-FC3-5 | 重放资格化/不可变性证明：重放前后原始证据哈希复验 + 输入视图盲视 + 身份绑定抽检 | T-FC3-4 | qualification/fc3_replay_qual.json | Q-FC3-11/12/18/19/20 |
| T-FC3-6 | FC-3 重放预执行冻结（新冻结根绑定全部 v3 工件 + 原根 8d6106ff + 台账快照） | T-FC3-5 | freeze/fc3_readjudication_freeze.json + GATE | overall=true 才可进人工门 |
| T-FC3-7 | 执行 FEASIBILITY_RE-ADJUDICATION_EPOCH_1 | **人工 Tasks/Replay Gate** | readjudication_epoch1/ 全量输出 | 只读原始证据 |
| T-FC3-8 | 重放验收（192/192、身份/盲视/不可变性、原始分类并存核验） | T-FC3-7 | ACCEPTANCE + 归档 | 不做效应分析 |
| T-FC3-9 | 效应分析资格门 | T-FC3-8 | 门裁决 | 独立人工授权 |

## 禁止项（全程）

不重跑 Simulink；不改 Model-E/PMS/C*/Predictor/任务/轨迹/FEC；不计算任何
B/L/R/节油/CI/臂排名；重放不读 comparative fuel/J_FEC；原始 epoch-2 分类与
工件一字不动。
