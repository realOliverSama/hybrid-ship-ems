# Model B：直流母线电气层 + 安全/节能指标卡 + S10 工况 Requirements

Spec: `model-b-dc-bus`  
Status: requirements-approved  
Created: 2026-08-19  
Brainstorm: `./brainstorm.md`

## Overview

并行开发 Model B：在 Model A（`model/simulink/`，功率平衡架构）基础上加回直流母线电气层（750 V 母线电容、电池双向 DC/DC、发电机整流、推进逆变，全部平均值模型），使母线电压 V_dc(t) 可观测；落地"安全（外部航速/内部电压与动力储备）+节能（等效燃油）"指标卡；新增 S10 水流反转工况；在 Model B 上完成 Baseline（规则门限）vs 预测增强 EMS 对比；开发结束时交付两种控制逻辑的纯文本流程图。

## Scope

### In Scope

- Model B 新模型（`model/simulink/model/HybridShip_ModelB_DCBus.slx` 或等价命名），可由脚本从零重建；
- 电气层子系统：DC_Bus（母线电容积分）、Battery_Converter（双向 DC/DC，576 V 侧，下垂/电压模式）、Genset_Rectifier（发电机+整流平均值模型）、Propulsion_Inverter（推进逆变功率消耗）、Bus_Voltage_Monitor（电压观测/报警门限）；
- 与 Model A 共享：参数体系（新增参数进 `config/Model_Parameters.m` 并标 ASSUMED_）、工况库（`config/Model_Scenarios.m`）、EMS 变体（Baseline / AI Enhanced）；
- 指标卡计算与对比报告（扩展 07 对比脚本或新增 08 脚本）：v_rmse、v_drop_max、E_unserved、V_dc 波动/跌落深度/恢复时间、P_margin_min、T_reserve_low、SOC_min/SOC_end/T_soc_low、P_bat_max、fuel_kg、FEC（等效燃油，k_eq≈0.505 kg/kWh）；
- 新增 S10 水流反转工况（先逆水 −1 m/s，里程触发渐变为顺水 +1 m/s，默认 2500 m 处、15 s 过渡）；
- Model B 上运行 S01/S04/S07/S10 的 Baseline vs AI 对比，出图出报告；
- 两种 EMS 控制逻辑的纯文本流程图（交付于回复与 `reports/`）；
- 全部工作在新 git 分支上进行并提交推送。

### Out of Scope

- 开关级（PWM）变换器模型、纹波分析；
- 母线保护动作/断路器/接触器逻辑、故障注入；
- 发电机启停/并网/暖机逻辑；
- 对 Model A 模型、数据集、报告的任何修改（Model A 只读复用）；
- HIL 硬件联调、STM32N6 实机部署；
- 电池老化/低温降额算例（用户明确暂不考虑）；
- S10 以外的更多新工况（后续独立迭代）。

## User Stories

- As a 论文作者, I want 在 Model B 上观测母线电压在扰动工况下的跌落与恢复, so that 可以定量论证 EMS 对"内部安全"的作用。
- As a 论文作者, I want 用统一指标卡对比 Baseline 与预测 EMS（含等效燃油 FEC）, so that 对比结论公平、可复现、可写进论文。
- As a 论文作者, I want 新增水流反转工况 S10, so that 覆盖长江感潮/汊口段的真实运行场景并检验 EMS 对负荷先升后降的响应。
- As a 论文作者, I want 两种控制逻辑的纯文本流程图, so that 可直接放入论文/答辩材料。

## Functional Requirements

- **FR-001**: THE Model B SHALL 在 Model A 功率流架构基础上增加直流母线电气层：750 V 母线电容（C_bus，ASSUMED 沿用 20 F）积分模型 dV/dt = (P_in − P_out)/(C·V)，电池经双向 DC/DC（576 V 侧）接入，发电机经整流平均值模型接入，推进逆变作为功率消耗端。
- **FR-002**: THE Model B SHALL 实现母线电压一次控制：电池 DC/DC 采用电压下垂（droop）模式，在 EMS 功率参考之外对电压偏差做快速功率补偿；电压正常带默认 750 V ±5%（参数化，标 ASSUMED）。
- **FR-003**: THE Model B SHALL 记录并可导出 V_dc(t)、各支路电流/功率、P_unserved、SOC 等信号至 logsout，采样与 Model A 数据集一致（0.01 s 仿真步长，1 s 导出重采样）。
- **FR-004**: WHEN 母线电压偏离额定，THE Bus_Voltage_Monitor SHALL 按 CCS《船舶应用直流综合电力系统指南 2026》量化指标判定并记录：稳态允许带 −10%~+6%（即 675~795 V）、瞬态允许带 −20%~+7%（即 600~802.5 V）、恢复时间 ≤1.5 s；产生报警标志并计入指标（越限时长、最大偏差 ΔU_dc_max、恢复时间 t_recovery），但不得中断仿真。
- **FR-005**: THE Model B SHALL 复用 Model A 的 EMS Variant（Baseline / AI Enhanced，EMS_MODE 切换）与 AI 预测接口（AI_Pred_Source 0/1/2/3），且两种 EMS 在 Model B 上共用完全相同的电气物理边界。
- **FR-005b**: THE Model B SHALL 实现 EMS 三种工作模式（参数 EMS_WORK_MODE 选择）：0=手动（EMS 只监测不控制，发电机功率参考取手动设定值）；1=半自动（依据在线设备状态做规则分配）；2=全自动（自动能量分配，含预测增强）。任一模式下设备异常只允许报警，不允许强制切换模式。
- **FR-006**: THE 系统 SHALL 新增 S10 水流反转工况：v_current 自 −1 m/s 在里程触发点（默认 2500 m）起经 15 s 渐变为 +1 m/s；该工况加入 `config/Model_Scenarios.m`，不修改 S01–S09 语义。
- **FR-007**: THE 对比流水线 SHALL 计算并输出指标卡：外部安全（v_rmse、v_drop_max、E_unserved）、内部安全（ΔU_dc_max 最大跌落深度、稳态/瞬态越限时长、恢复时间 t_recovery（对照 CCS ≤1.5 s）、P_margin_min、T_reserve_low、SOC_min/SOC_end/T_soc_low、P_bat_max）、节能（fuel_kg、FEC = fuel_kg + ΔSOC×Bat_Cap×k_eq，k_eq = BSFC_avg/η_chg，标 ASSUMED）。
- **FR-008**: WHEN Model B 构建完成，THE 系统 SHALL 通过烟雾测试：S01 静水巡航母线电压维持在 CCS 稳态带（675~795 V）内、S10 水流反转全过程无 NaN/Inf、电压跌落最深点出现在负荷上升段且随后恢复（t_recovery ≤1.5 s 为参照判据）。
- **FR-009**: THE 交付物 SHALL 包含两种 EMS 控制逻辑（Baseline 规则门限、AI 预测增强）的纯文本流程图（ASCII，可直接放入论文），存放于 `reports/` 并同时在会话回复中给出。
- **FR-010**: WHEN Baseline 与 AI 在 Model B 上对比运行（S01/S04/S07/S10），THE 报告 SHALL 同时给出原始燃油与 FEC，并如实说明两者差异来源（SOC 末值差异的电量折算）。

## Non-Functional Requirements

- **NFR-001**: THE Model B SHALL 保持固定步长 ode3 @ 0.01 s 数值稳定（电气层动态时间常数不得小于步长的 5 倍，避免刚性）。
- **NFR-002**: THE 构建 SHALL 完全脚本化：删除 Model B 模型文件后可由构建脚本从零重建；禁止 GUI 手工修改不入脚本。
- **NFR-003**: THE 新增参数 SHALL 进入 `config/Model_Parameters.m` 并按 DRAWING/LEGACY/ASSUMED 分类标注；无来源参数一律 ASSUMED_ 前缀并计入 parameter_audit.csv。
- **NFR-004**: THE 开发 SHALL 在新 git 分支（如 `feat/model-b-dc-bus`）上进行，不修改 Model A 任何文件；提交信息清晰；完成后推送 origin。
- **NFR-005**: IF 对比结果显示 AI 无优势或处于劣势，THE 报告 SHALL 如实呈现，不得通过调整不合理参数制造优势。
- **NFR-006**: THE Model B 仿真速度 SHALL 保持在可批量运行水平（单航次 5000 m 仿真挂钟时间不超过 Model A 同工况的 3 倍）。
- **NFR-007**: THE Model B SHALL 面向“动力与控制验证”定位：采用聚合/平均值建模，不对部件逐个独立建模（无 PWM 开关级、无单机细节），以保证批量仿真的实时性。
- **NFR-008**: THE 船舶具体参数均为待与船舶设计方会议核定的预置值；核定前 SHALL 使用参数表中拟定值，缺少的参数允许合理预设并一律以 ASSUMED_ 标明，供核定时逐项替换。

## Acceptance Criteria

- [ ] Model B 可独立打开，`config/Model_Parameters.m` + `Model_Scenarios.m` 加载后 Update Diagram 通过（EMS_MODE=0/1 均通过）。
- [ ] 构建脚本删除模型后可从零重建 Model B。
- [ ] 烟雾测试通过：S01 巡航 V_dc ∈ [675, 795] V（CCS 稳态带）；S04/S10 扰动段 V_dc 出现可观测跌落且随后恢复（参照 t_recovery ≤1.5 s）；全程无 NaN/Inf。
- [ ] S01/S04/S07/S10 四工况 Baseline vs AI 对比报告生成，含完整指标卡（原始燃油 + FEC）。
- [ ] 纯文本流程图交付（会话回复 + `reports/ems_control_flowcharts.md`）。
- [ ] Model A 目录内容零改动（git diff 验证）；全部改动在独立分支提交并推送。

## Edge Cases

- 电池 SOC 到下限导致 DC/DC 放电降额 → 母线电压由下垂特性自然下垂，监视器记录越限；不得发散。
- 母线电压接近 0 或负值（极端失衡）→ 功率-电流换算需除法保护（V_floor 下限），仿真不得崩溃。
- S10 水流反转跨越 0 时相对水速符号变化 → 阻力方向正确（R = K·v_rel·|v_rel| 自带符号处理）。
- AI 预测无效/越界 → 自动回退 Baseline（沿用 Model A 既有门控逻辑）。
- 参数文件未加载就 Update Diagram → 报错信息应明确提示先运行 config 脚本。

## Dependencies and Constraints

- 依赖 Model A 现有资产（参数表、工况库、EMS 变体、+model 工具包）——只读复用；
- MATLAB R2026a + Simulink（MCP 会话在线）；Python 训练环境 `.venv_tf`（本规格不新增训练需求，沿用既有 MLP 权重）；
- git：基于最新 `main`（d977c9f8）开分支；注意仓库另有 Kimi 开发线，避免改同一批文件；
- 求解器固定 ode3 @ 0.01 s（与 Model A 一致，便于结果对照）。

## Open Questions

- [ ] S10 水流反转的里程点（默认 2500 m）与过渡时长（默认 15 s）是否确认？
- [ ] ~~母线电压正常带是否有规范依据？~~ 已解决：采用 CCS《船舶应用直流综合电力系统指南 2026》量化指标（稳态 −10%/+6%、瞬态 −20%/+7%、恢复 ≤1.5 s；该指南 2026-10-01 生效，现行检验为 2023 版，新设计按 2026 版准备）。
- [ ] 混合动力侧还需对照《船舶应用混合动力系统指南 2024》（预充电、充放电控制、PTO/PTH 等模式），本规格的 EMS 三模式（FR-005b）即与之对应；哪些条款纳入验证范围，设计阶段确定。
- [ ] 下垂系数 K_droop 的初值（暂沿用 0730 的 K_droop_bat=2 缩放到 750 V 等级）是否需要在设计阶段做一次小灵敏度扫描？
