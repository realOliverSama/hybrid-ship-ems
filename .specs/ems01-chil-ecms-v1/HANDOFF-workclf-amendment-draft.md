# HANDOFF：workclf 口径修复设计修订草案（只起草，不实施）

> 移交状态：ems01-chil-ecms-v1 已 implementation-complete（13/13 任务全勾，fw_gate 7/7）。
> 本任务 = 唯一遗留决策（workclf 部署口径修复路线）的**前置起草工作**。
> 铁律：**只写 draft 文档，不改任何源码/门禁/参考数据/模型**——未经用户批准不得实施任何修改。

## 必读（按序）

1. `docs/agent/AI_MODELING.md` **§22**（口径契约事故全记录：一个模型三种口径三种行为 + 路线 A–D 表 + 五条硬规则）
2. `model/simulink_modelD_(EMS_NN_AECMS)/results/ecms_phase25/deploy/m2_port_report.md` **§6/§8**（根因量化 + 遗留决策项）
3. 同目录 `m2_workclf_calib_report.md`（判别实验 E1–E5 完整证据）
4. `.specs/ems01-chil-ecms-v1/design.md`（现行 design，修订草案的基准）

## 任务

起草设计修订草案，写入 `.specs/ems01-chil-ecms-v1/design-amendment-workclf-caliber-draft.md`，
文件头明确标记：**"DRAFT — 待用户批准，未批准不得实施任何修改"**。内容：

### 主方案：路线 C（固件 workclf 喂入改归一化特征，对齐 T-312 训练口径）

1. **精确修改点清单**：
   - 固件：`Appli/App/ems_ctrl.c` workclf 喂入路径（pdec30 + v_ship + accel → 归一化后喂入；
     归一化常数 X_mean/X_std 已在 ems_params.c，注意只取 workclf 输入对应的分量）
   - EML 参考：`ecms_nn` Simulink 模型的分类器喂入是否同步改为归一化（**必须同步**，否则
     层①"复刻 EML"性质被破坏——这是本修订的核心链条，逐项列出）
   - 工具链口径：`Host/tools/gen_reference_int8.py`（MirrorCtrl workclf 特征段）、
     `Host/tests/test_ctrl_diff.py` / `test_l0_trace_diff.py` 的口径同步
   - int8 标定：**路线 C 下现有 workclf_v1 int8 无需重标定**（E4 实验已证 int8×归一化 ≈
     float×归一化）——但须复核 tflite 输入量化节点的标定集口径声明（AI_MODELING §22.3 规则 3）
2. **影响面分析**（核心）：固件 ≠ 旧 EML 后的参考再生成全链条：
   EML 改喂入 → MATLAB 重新生成 `fw_reference/ref_G2_*.csv`（层①②③参考全部换血）→
   **Golden Baseline 重建属 🟡 须用户批准**（AGENTS §12）→ L0 差分门禁口径复核
   （test_ctrl_diff 对比对象变为新 EML）→ 层①②③阈值含义复核（**阈值数值本身不得放松**，
   design 固定）
3. **重验证计划**（全部板会话需用户在场 + 令牌）：L0 差分 → REF/NPU 双镜像重构建 + fw_gate →
   层①②③ 重新注入（S20 + voyage_002 全程）→ 分类器对设计真值 acc 复测（双轨评估，
   AI_MODELING §22.3 规则 5）→ FR-508 回归
4. **预期效果声明**（基于 E3/E4 实验数据，不得夸大）：部署形态 upstream 判定对齐训练口径
   （S20 设计 10.9% vs 现 EML 行为 89%），预测门控 ê 生效时段占比变化 → 对层③分布的预期改善
   方向；同时如实说明 bridge/berth/depart 弱识别问题（acc 0.60–0.76）不会因 C 消失

### 对照方案：路线 B（raw 口径重训分类器 + 重标定）

同结构但简短：训练管线改造点（MirrorCtrl 特征构造复用为训练管道）、数据集口径
（raw 特征 + 设计段标签）、部署链重标定、与 C 的工作量/风险/论文价值对比。

### 收尾：C vs B 论文价值对比 + 推荐意见（标注"供用户裁决"）

- C：最小改动、保住"移植保真"叙事、但分类能力受限于现有模型（0.60–0.76）
- B：研究贡献更完整（口径统一到部署形态 + 分类器能力提升），但工作量大、需重走 T-312 全链
- 给出推荐及理由，最终由用户裁决

## 约束（违反 = 任务失败）

- 只新建/修改：上述 draft 文件 + `项目开发进展.md` 当日条目（§18 规则，draft 完成也须记录）
- 禁止：改源码 / 门禁阈值 / 参考数据 / 模型文件 / 既有报告数据
- 涉及 AI 建模判断先读 `docs/agent/AI_MODELING.md`；设计取舍不确定处列入"待用户决策点清单"，
  不得替用户决定
- 完成后向用户汇报：草案位置 + 需裁决决策点清单
