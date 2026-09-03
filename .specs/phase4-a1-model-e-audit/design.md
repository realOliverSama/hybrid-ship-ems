# Phase 4-A1 — Model E 建模与参数审计 Design

Spec: `phase4-a1-model-e-audit`  
Status: design-approved  
Created: 2026-09-03  
Brainstorm: `./brainstorm.md`  
Requirements: `./requirements.md`

## Summary

A1 以"**JSON/CSV 为单一事实源 + 脚本渲染报告 + 机器可读完整性 Gate**"的方式执行纯审计：
架构审计条目以 JSON 手工撰写（agent 的分析结论），参数总表以 Python 脚本从
`Model_Parameters.m` / `Model_Scenarios.m` 解析并与既有 `parameter_audit.csv` 合并生成，
两份 Markdown 报告由脚本从 JSON/CSV 渲染（人读视图），最后由 Gate 脚本校验完备性并输出
`gate_result_a1.json`。Model D 工作区零改动（含零新增）——全部 A1 资产集中在新目录
`docs/phase4/a1_model_audit/`。

三个 Open Questions 定稿见 §3。

## Goals and Non-Goals

### Goals

- 产出 Model D 全顶层 subsystem 的处置决策清单（FR-401/402）与螺旋桨分支结论（FR-403）；
- 产出全参数 CSV 总表，五级来源分类 + 影响等级 + 用户回填字段（FR-404/405）；
- 产出假设影响分析报告（FR-406）；
- 机器可读完整性 Gate（FR-407），含 FAIL 路径负向验证；
- Model D 零改动证据链（FR-409）。

### Non-Goals

- 不修改/新增 Model D 工作区任何文件；不创建 Model E 工作区；
- 不获取/回填真实参数（用户分批回填，非完成条件）；
- 不评估实时性取舍（Real-Time Cost 仅信息性标注，NFR-403）。

## 1. 资产布局（OQ-2 定稿）

全部 A1 资产集中于新目录（入库）：

```text
docs/phase4/
├── README.md                        # Phase 4 文档目录索引（§20）
└── a1_model_audit/
    ├── README.md                    # 本目录索引（§20）
    ├── architecture_audit.json      # 架构审计单一事实源（手工撰写）
    ├── modelE_parameter_audit.csv   # 参数总表（脚本生成 + 人工复核 + 用户回填）
    ├── modelE_architecture_audit.md # 架构审计报告（脚本渲染）
    ├── modelE_parameter_audit.md    # 参数审计报告（脚本渲染 + 手工分析节）
    ├── gate_result_a1.json          # Gate 输出（机器可读）
    └── scripts/
        ├── build_parameter_table.py # 参数提取/合并 → CSV
        ├── render_audit_reports.py  # JSON/CSV → 两份 MD 报告
        └── gate_a1_completeness.py  # 完整性 Gate → gate_result_a1.json
```

理由：FR-408 允许 design 定稿新路径；选 `docs/phase4/` 使 Model D 连"新增文件"都为零
（最严格的 FR-409 解释），且 Phase 4 各工作包后续资产可沿用 `docs/phase4/<包>/` 结构。
`docs/README.md` 同步登记（§20）。

## 2. 单一事实源设计

| 产物 | 事实源 | 渲染/生成 | 手写内容 |
|---|---|---|---|
| 架构审计 | `architecture_audit.json` | `render_audit_reports.py` → MD | JSON 全部条目（agent 分析结论） |
| 参数总表 | `modelE_parameter_audit.csv` | `build_parameter_table.py` 生成初稿 | 人工复核修订 + 用户回填（直接在 CSV 上） |
| 参数审计报告 | `modelE_parameter_audit.md` | 脚本渲染摘要 + 手工撰写"假设影响分析"节 | 假设影响分析（FR-406） |
| Gate | `gate_result_a1.json` | `gate_a1_completeness.py` | 无 |

纪律：MD 报告可重跑再生成（幂等）；手工内容只存在于 JSON、CSV 与报告的
"假设影响分析"节（该节在渲染脚本中以"保留块"机制保护，重跑不覆盖）。

## 3. Open Questions 定稿

### OQ-1：CSV 列集与文件名

文件：`modelE_parameter_audit.csv`（UTF-8，逗号分隔，首行表头）。列集：

| 列 | 内容 | 生成方式 |
|---|---|---|
| `parameter` | 参数名（MATLAB 变量名） | 脚本提取 |
| `current_value` | 当前值（数值/字符串原样；派生参数标"派生:<计算式>"） | 脚本提取 |
| `unit` | 单位（来自行内注释或既有审计表） | 脚本+合并 |
| `module` | 所在模块/章节（§1 模型范围、§推进、§电池……；多处定义逐行登记） | 脚本提取 |
| `legacy_caliber` | 现行口径：DRAWING / LEGACY / ASSUMED | 脚本（章节标记+前缀） |
| `source_class5` | 五级分类：`drawing` / `vendor` / `literature` / `estimated` / `assumed` / `pending` | 默认映射 + 人工复核 |
| `impact` | 影响等级：`high` / `medium` / `low`（对论文结论的影响评估） | 人工复核 |
| `user_value` | 用户回填典型值 | 用户（留空） |
| `user_reference` | 用户回填文献/资料来源 | 用户（留空） |
| `notes` | 备注（pending 时必填） | 脚本+人工 |

### OQ-3：双等级刻度

三级刻度（简单、可门禁）：

- **Physical Fidelity** ∈ {高, 中, 低}：当前实现距离真实物理的保真度；
- **Real-Time Cost** ∈ {低, 中, 高}：未来实时化时的计算成本担忧（仅信息性，NFR-403）。

### 五级来源默认映射规则（FR-405）

| 现行口径 | 默认 source_class5 | 说明 |
|---|---|---|
| DRAWING | `drawing` | 但 v2.2 起部分"真实设备参数"（20260526 设备参数 docx）人工复核可升/改为 `vendor` |
| LEGACY | `estimated` | 备注"继承原模型 HybridShip0730，原始依据待追溯"；用户查到依据后升级 |
| ASSUMED | `assumed` | — |
| 无法判定 | `pending` | 备注必填（Gate 校验） |

映射为**默认值**，审计报告可对具体行 override 并写明理由（override 记录进报告）。

## 4. 执行方法

### 4.1 Subsystem 清点（FR-401）

以两个来源交叉清点 Model D 顶层 subsystem：
(a) builder 脚本 `scripts/+model/build_model_c.m` 的子系统创建段；
(b) 模型文件结构（只读，`find_system` 或 builder 清单元数据，不打开 GUI 不修改）。
清点结果写入 JSON 的 `subsystem_inventory` 段（名称、builder 位置、简述），
每项附处置决策、双等级、理由。ADD 类条目（路线文档 §2 对应的五类新模块：
水深/UKC、DG1/DG2、启停状态机、Battery 1RC ECM、SOP）单列 `additions` 段。

### 4.2 螺旋桨分支（FR-403）

JSON 顶层字段：

```json
"propeller_decision": {
  "choice": "keep_simplified | upgrade_ktkq",
  "basis": "可用资料清单或缺失声明",
  "a2_consequence": "对 A2 的 Fixed Decision 描述"
}
```

裁决依据 = 既有资料清点（`docs/reference/`、`docs/data_request_list.md` 状态、
用户提供的图纸信息）；资料不足即选 `keep_simplified` 并明确 A2 做敏感性验证。

### 4.3 参数提取（FR-404）

`build_parameter_table.py`（纯文本解析，不需 MATLAB）：

1. 逐行解析 `Model_Parameters.m` / `Model_Scenarios.m`：
   赋值语句 → 参数名+当前值；行内注释 → 单位/说明；章节注释的
   `[DRAWING]/[LEGACY]/[ASSUMED]` 标记 + `ASSUMED_` 前缀 → legacy_caliber；
2. 合并既有 `reports/parameter_audit.csv`（只读）：unit/source/confidence 信息补入；
3. 默认映射生成 source_class5，impact 初值按启发式（水动力/BSFC/电池→high 待复核）；
4. 幂等重跑：已存在 CSV 时保留人工列（source_class5 override / impact / user_* / notes），
   只更新脚本列（current_value / module / legacy_caliber）——**用户回填永不丢失**。

### 4.4 Gate 设计（FR-407）

`gate_a1_completeness.py` 校验项：

| 检查 | 规则 |
|---|---|
| G1 subsystem 覆盖 | JSON `subsystem_inventory` 非空；每项 disposition∈五选一、fidelity/rtcost∈三级、rationale 非空；与 builder 清点集合一致（100% 覆盖） |
| G2 ADD 清单 | `additions` 覆盖五类新模块（浅水/DG1/DG2/启停/电池ECM+SOP） |
| G3 螺旋桨结论 | `propeller_decision.choice` ∈ 二选一且 basis 非空 |
| G4 参数分类 | CSV 每行 source_class5 ∈ 六值枚举；`=pending` 时 notes 非空；无空行 |
| G5 报告存在 | 两份 MD 报告存在且非空（渲染后校验） |

输出 `gate_result_a1.json`：`{"overall": bool, "checks": {...}, "timestamp": ...}`；
overall=false 时退出码非零并在 stderr 列出缺项。

### 4.5 零改动证据（FR-409）

任务开始与结束时各记录一次：`git status --short model/` +
`Model_Parameters.m` / `Model_Scenarios.m` / builder / `.slx` 的 sha256 对照表，
附入架构审计报告附录。A1 全程对 Model D **只读**（连新增文件都没有——资产全在
`docs/phase4/`）。

## 5. Error Handling

- 解析脚本遇到无法识别的赋值行：记入 CSV（source_class5=pending + notes 说明），
  不静默丢弃；
- 既有 `parameter_audit.csv` 缺失或格式漂移：脚本报错退出（不猜格式）；
- Gate 任何检查 FAIL：非零退出 + stderr 缺项清单 + JSON overall=false；
- 渲染脚本保护"假设影响分析"保留块缺失时：报错而非覆盖。

## 6. Security and Privacy

无外部接口、无密钥、无网络访问；CSV 将承载用户提供的文献来源信息（入库，
属研究依据，与 `docs/reference/` 既有实践一致）。

## Testing Strategy

- **单元级**：解析脚本对小样本输入的提取正确性（手造 .m 片段对照）；
- **集成级**：完整跑通 extract → render → gate 链，Gate overall=true；
- **负向验证（AC-405）**：临时构造缺项副本（删一个 subsystem 条目 / 置空一格分类 /
  删螺旋桨结论），确认 Gate 正确 FAIL 且非零退出；
- **幂等验证（NFR-401）**：全流程连跑两遍，CSV/MD/JSON 逐位一致；
- **零改动验证（AC-406）**：前后 sha256 对照 + git status；
- **用户评审（AC-407）**：两份报告结论经用户批准。

## 8. Rollout and Migration

1. 本 spec tasks 批准后实施（无需并行，主 Agent 分析 + 可委派机械提取）；
2. A1 完成 ≠ 参数回填完成；用户在 CSV 上迭代回填，回填不改变 A1 已批准结论，
   但可触发 A2 spec 的范围微调（如螺旋桨结论因新资料翻转——走 A2 brainstorm 修订）；
3. 回退 = 删除 `docs/phase4/` 目录（Model D 从未被触碰）。

## Requirements Traceability

| Requirement | Design Decision | Validation |
|---|---|---|
| FR-401 | §4.1 双源清点 + JSON inventory | Gate G1 + AC-401 |
| FR-402 | §4.1 `additions` 段 | Gate G2 + AC-401 |
| FR-403 | §4.2 propeller_decision | Gate G3 + AC-402 |
| FR-404 | §4.3 + OQ-1 列集 | Gate G4 + AC-403 |
| FR-405 | §3 映射规则 + override 记录 | Gate G4 + AC-403 |
| FR-406 | §2 参数报告"假设影响分析"保留块 | 用户评审 AC-404 |
| FR-407 | §4.4 Gate 脚本 | AC-405（含负向） |
| FR-408 | §1 资产布局 + README 登记 | AC-406 附带检查 |
| FR-409 | §4.5 零改动证据链 | AC-406 |
| NFR-401 | §2 幂等设计 + §4.3 保留人工列 | 幂等验证 |
| NFR-402 | 全部脚本相对路径 + pwd 兜底 | 代码审查 |
| NFR-403 | OQ-3 刻度说明 | 报告评审 |
| NFR-404 | 审计只分类不替换（§0 Non-Goals） | 零改动证据 |

## Risks and Trade-offs

- **风险**：纯文本解析 `.m` 可能漏掉非常规赋值（如 struct 字段、计算表达式）。
  对策：解析器保守策略——识别不了的行不丢弃，以 pending 入表由人工复核（§5）；
  解析结果行数与既有 parameter_audit.csv（112 行）对照，数量级不符即报警。
- **权衡**：JSON 作为架构审计事实源增加了"写 JSON"的心智成本，但换来 Gate 可机检
  与报告可重生成——接受。
- **风险**：五级分类默认映射把 LEGACY 一律归 estimated 可能过于保守（部分 LEGACY
  实际有图纸依据）。对策：映射只是默认值，人工复核 override 是流程的一部分（§3）。
