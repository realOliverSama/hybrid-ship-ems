# HANDOFF — Phase 4-D 数据集 v3（新 session 开工提示词）

> 写给接手 D 包的 session。本文件 + 路线文档 + Phase 4 现状即开工包。
> 创建：2026-09-04（B 包 PR #20 合并后，从 Phase4 头 8e9f05be 开出 D 分支）。
> **D 包 spec 尚未建立**——本包先走四门禁 spec 流程（brainstorm → requirements → design → tasks），spec 定稿后才施工。

## 0. 开工仪式（必须先做）

```bash
git fetch origin && git status -sb && git log --oneline -3
git checkout feat/phase4-d-dataset-v3   # 若不在该分支（已从 origin/Phase4 8e9f05be 开出）
```

- 工作区两个未跟踪目录 `firmware/.../Host/fault_out/`、`model/simulink_modelD_(EMS_NN_AECMS)/slprj_v1_stale/`
  为既定不入库产物，**不清理、不提交、不担心**；
- 工作区若有其他未提交改动 → 停止报告用户（§0 纪律）。

## 1. Phase 4 当前进度（截至 2026-09-04，本分支基点 = PR #20 合并后 Phase4）

| 包 | 状态 |
|---|---|
| A2（Model E 物理基座，T-401~T-412） | implementation-complete；T-412 Gate v1.1（candidate promotion，T-014 APPROVED）overall=true |
| B（PMS v2 + Safety-Interlock，T-413~T-418） | implementation-complete；43 控制 Gate overall=true（134 判据）+ 44 负向 7/7；PR #20 已并入 Phase4 |
| **C（工况生成器 v3，T-419~T-429）** | **未启动** |
| D（数据集 v3，T-430~T-435） | **本包（spec 未建）** |
| E/F | 未启动 |

**基线三件套（数据生成的事实源，禁止改动）**：
`config/gate_baseline_modele_v1_0.json`（A2 historical）/ `gate_baseline_modele.json`（current v1.1，fail-closed G6 96/96/0）/ `gate_baseline_modele_control.json`（B 控制基线 v1.0，134 判据 + negative 7-7 + S2 reason=4 端到端）。

**依赖关系（路线 §10）**：`B ─► C (T-419~T-429) ─► D (T-430~T-435)`——C 的 Mission v3 是 T-432
fleet 生成的硬前置；但 **T-430（观测变量）/T-431（Known-Future 契约）/T-433/T-434（窗口化/
分位数目标设计）为纯定义工作，可不依赖 C 先行**。brainstorm 时必须与用户裁决顺序：
（a）D 先做契约与窗口化设计、T-432 等就绪 C 后生成；或（b）C/D 交替推进。不要擅自替用户定。

## 2. D 包任务范围（路线文档 §5，backlog 级；spec 定稿时可增删改）

- **T-430** 新增观测变量定义（Historical）：P_load/speed/acceleration/RPM/P_gen/P_bat/SOC/Vdc 等（brainstorm 定稿）
- **T-431 Known-Future Information 定义（关键）**：逐变量三类标记——已知（route/planned speed/bridge/speed limit/chart depth）/ Forecast（current/water level）/ Unknown（future real P_load/traffic）；产物 `future_information_contract.md`；**兑现 backlog B1（P0）：突破纯历史外推 R²≈0.73 天花板**
- **T-432 生成 fleet v3**：Model E + Mission v3，500~1000 完整航次；第一轮 100 条 smoke 再正式生成（**硬依赖 C 包 Mission v3**）
- **T-433 Dataset v3 窗口化**：scalar target → trajectory target；History 300 s / Future 300 s；输出 P_10…P_300 共 30 点
- **T-434 Quantile/auxiliary target**：q10/q50/q90 + E_300 辅助目标（保持与旧研究连续性）
- **T-435 Dataset split / leakage audit**：voyage-level split、near-neighbor audit、no test tuning；新增 OOD set（stronger current / lower water level / unseen route combination）

## 3. 必读材料（按序，禁止凭记忆开工）

1. `docs/phase4_research_roadmap.md`：§1（Phase 定位与边界）+ §4（C 包全节——裁决 D/C 顺序）+ §5（D 包全节）+ §10（依赖图）
2. `docs/agent/AI_MODELING.md`（**涉及数据集强制读取**；数据纪律/训练红线以它为准）
3. `model/simulink_modelE_(Phase4_Physics)/README_Model.md`（模型现状：顶层 16 块含 07_PMS_v2/16_Safety_Interlock、§3A PMS v2 控制逻辑、§5 验证状态）
4. `docs/phase4/b_pms_safety/ruling_chain.md` + `b_arm_comparison_report.md`（B 包裁决链与三臂口径——数据集消费 P2 臂/基线三件套）
5. `项目开发进展.md` 2026-09-04 三条目（A2/B 收官 + 延伸问答记录）
6. Phase 2.5 既有数据集纪律资产（Model D 继承件中的 split/leakage audit 机制——适配前只读）

## 4. 挂账事项（D 包范围内的既定遗留，brainstorm 时逐项裁决）

- **03 号脚本 MVP 遗留**（`model.ensure_canonical` 依赖，Model E 内禁用）——适配或隔离，B 包挂账转 D
- **10b smoke 的 CCS 稳态带判据**为 Model D 时代口径（启动暂态必然越带）——重定界
- **E10 smoke 携带 E09 遗留注入**（A2 继承行为，无判据）——可加 bench nominal 恢复
- **05/30/60/66 号导出脚本**与 `datasets/`、`python/`、`legacy_reference/` 目录为 Model D 时代件——按 Dataset v3 需求重写/适配（禁直接沿用）
- **500 t 实船确认挂账**（`docs/data_request_list.md`）——到位后走 spec 修订
- B 包移交注意：`docs/` 下 `b_pms_safety/` 为 B 包证据目录（勿动）；`model/simulink_modelE_(Phase4_Physics)/docs/` 已清理（证据在仓库级）

## 5. 纪律（违反=返工）

- **spec 四门禁**：brainstorm（一次一问，逐项敲定）→ requirements → design → tasks；每门单独批准；tasks-approved 前禁施工（spec_validate implementation 前置）
- **Model E 模型零改动**：数据集从仿真导出，不改 Plant/EMS/PMS/任何已冻结参数；若任务需要改模型 → 停下升级用户裁决
- **基线冻结纪律**：T-412（物理 v1.1）与控制基线 v1.0 是数据生成的事实源；Gate FAIL → `error('GATE:*')` 停下升级，禁改参数凑数据
- **数据纪律**（T-435 + AI_MODELING.md）：voyage-level split、near-neighbor audit、no test tuning；Test Set 不参与训练或调参；OOD set 新增
- **R2026a 工程坑清单**（B 包实证续增）：Stateflow.State 赋 LabelString / Transition 无 Position·Priority / Data 显式 DataType+Size+InitialValue / logsout 恒值信号存 double / 向量 timeseries [1×k×N] / Dataset 无 NumElements 属性（用 numel/get）/ exist 对包函数恒 0（用 which）/ Selector 参数族 IndexOptionArray / Selector·From Workspace 常量不随 base 变量自动重编译 / 数字开头脚本 eval(fileread)
- **Git 红线**：逐文件 add（禁 -A/.）、禁 force-push、禁 `git checkout .`/`reset --hard`/`rm -rf`；无 force 需求时禁 rebase
- **禁绝对路径**（§20.2 扫描零命中）；§20 README 两级同步；每个任务完成立即 spec_task_check

## 6. 开工顺序建议

1. 读材料（§3）→ 向用户提出 **D/C 顺序裁决问题**（依赖说明见 §1）
2. spec brainstorm（建议从 T-431 Known-Future 契约起步——它是本包的关键创新点与 backlog B1 兑现）
3. 四门禁逐门批准 → tasks-approved 后 spec_validate implementation → 施工
4. 施工面预计：观测/契约文档 + `+dataset`/`+dataset_v3` 导出与窗口化代码（Model D 时代脚本重写）+ fleet smoke（依赖 C 就绪度）+ split/leakage/OOD 审计脚本