# datasets_v3/ — Dataset v3 数据工作区（本地数据目录）

本目录存放 Dataset v3 各阶段的**数据集本体**（.mat/.npz/分配计划）。入库策略见
仓库根 `.gitignore` Phase 4-D 节：**`formal/` 正式区入库（2026-09-12 用户指令修订
DGN-01）**；`mission_v3/`、`pilot/`、`qualification*` 与 `SUPERSEDED_*` 隔离区
维持本地不入库。

工具（聚合/审计/freeze 代码）在仓库的 `dataset_v3/` 目录，勿混淆。

## formal/ — T-432 Stage B 正式数据（入库）

| 条目 | 内容 |
|---|---|
| `iid/` | IID 段 576 voyages 原始仿真产物 `FLTA_<stratum>_<draw>_<member>.mat`（~264MB；pass2 覆盖写，pass1 证据由 ledger payload_hash 锚定） |
| `ood/` | OOD 段 200 voyages 原始仿真产物 `FLTB_OOD_<condition>_<draw>_<member>.mat`（~93MB；同上覆盖写语义） |
| `formal_draw_allocation_plan_iid.json` | AMENDMENT_03 冻结 IID 分配计划（Phase S 产物，09-10 01:56 冻结） |
| `formal_draw_allocation_plan_ood.json` | AMENDMENT_03 冻结 OOD 分配计划（09-09 12:55 冻结） |
| `windows_v3_formal.npz` | T-020 freeze 产物：formal 训练窗口数据集（run_formal_freeze 生成） |
| `_iid_ledger.json` / `_ood_ledger.json` | 最近一轮（pass2）生成账本；两 pass 归档副本在 `results/dataset_v3/formal/pass{1,2}_{iid,ood}_ledger.json` |

不入库：`SUPERSEDED_STAGEB_SAMPLER_DIAGNOSTIC/`（废案隔离区）。

## mission_v3/ — Mission v3 producer 各阶段数据（不入库）

`prequal/`、`prequal_pilot/`、`prequal_rerun/`、`t429conf_v2/`、`t429conf_v2_pilot/`、
`t429conf_v2_rerun/`：mission_v3 预确认与 T-429 确认各轮的 voyage 数据（T-429/T-430）。

## pilot/、qualification/、qualification_smoke/ — pilot 与 Stage A 数据（不入库）

T-015 pilot E2E 与 T-432 Stage A（92/92）各轮数据；正式审计只认 `formal/`（T-020
Non-scope 条款：pilot 统计不得用于满足 formal 审计）。
