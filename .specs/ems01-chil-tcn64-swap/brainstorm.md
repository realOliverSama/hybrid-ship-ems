# Ems01 Chil Tcn64 Swap Brainstorm

Spec: `ems01-chil-tcn64-swap`  
Status: brainstorm-complete  
Created: 2026-09-02

## User Intent

【用户意图】v3 真闭环收官后（2026-09-02/09-08），按 v1 spec FR-511 流程执行编译期换型（档 1，非运行期热切换）：将 phase25 同签名候选预测模型换入 EMS01_CHIL_ECMS 固件（stedgeai 重生成 → 重构建 → SRAM 加载 → 一致性复测），验证更大模型入闭环的平台能力（roadmap §8.3/§8.4.7 议程）。载体：v1 FR-511 验收条目 + 新 spec。

【用户裁决】
- Q1 候选选型 = TCN-f64（tcn_f64，165,851 参数，int8 tflite 219.7KB，float val RMSE 9.8433 全场最优）。TCN-f16 因 int8 后 q_coverage 崩至 0.453 不推荐；h128-MLP 保守但增量信息少；mse 变体签名不符排除。
- Q2 REF 后端 = 方案 R1：开发 TCN C 前向（ems_infer_tcn.c：causal dilated Conv1D×14 + 残差 + LastTimeStep + Dense 头），保留 (b) 臂与四臂矩阵完整性；层① 改两段式口径（host gcc TCN-C vs keras float32 参考 [数值容差门] + 板上 REF vs host gcc [同源精确]）。层①容差门 = LSB 形式（按 TCN 输出张量 scale 重推导，沿 FR-504 形式）。

【关键事实（本 session 盘点）】
- 签名：TCN 与 MLP-Q 同一 302 特征语义（P 窗口 300 + v_ship + accel）、同一 fleet_norm_v2.json 归一化（deploy JSON 与其逐位一致，gen_fw_params 有断言）、同 q10/q50/q90 分位数输出；但 tflite 为双输入 [1,300,1]+[1,2]（MLP-Q 为拼接 [1,302]）——NPU 双输入填充在 workclf 已有先例。
- int8 口径下现行 MLP-Q（9.9659）仍优于 TCN-f64 int8（10.0353）——换型价值定位 = FR-511 流程实证 + 更大模型闭环评估，非"预期更省油"（研究诚信如实记录）。
- stedgeai analyze 预检（2026-09-02，/tmp/tcn_analyze，同 mlpq 配置 user_neuralart.json + stm32n6.mpool）：132 epochs 中 47 SW，但全部 14 CONV_2D + 3 FULLY_CONNECTED 均上 NPU HW（EC）；SW 仅为 Pad/SpaceToDepth/DepthToSpace/Quantize/Dequantize/Gather 数据搬运；macc 11.96M；activations 131.3KB @npuRAM3（448KB 池 29.31%）；NPU 权重 161.3KB @octoFlash 0x70200000（1MB 池 15.75%，与 mlpq 同基址，mpool 布局零改动）。
- 层①锚点必然重定义：EML 内嵌 MLP-Q，任何新模型不在 EML 中（Q2 已裁决两段式方案）。
- INFER_TEST 协议（302 raw 物理量）可不变：固件内归一化后拆分双输入。

【假设（随 spec 起草确认）】
1. 换型 = NOR mlpq 槽位（0x70200000）换入 TCN 权重 hex；MLP-Q 全部资产保留（冻结区不动），可按 FR-511 逆向换回。
2. 闭环复测矩阵 = 仅新跑 (a) TCN-NPU + (b) TCN-REF × 2 场景 = 4 组；(b′) MLP-Q 原生与 (c) law=5 引用 v3 收官权威数据（plantstep/harness 零改动 + lock-step 确定性 REPRO 保证跨会话可比）。
3. 时延预算沿 FR-505 修订版（TCN 实测 + workclf 42.5ms 合计 ≤60ms，1Hz 控制拍）；超限不放宽 → escalate（候选降级 TCN-f16 或用户重裁决）。
4. 层② S20 98.877% 边际 FAIL 处置沿袭 (a) 接受登记，阈值不放宽。
5. FR-504 ×200 板上一致性门禁按 TCN 新输出张量 scale 重推导 LSB（1 LSB = TCN q50 输出 scale × y_std）。
6. workclf v1.1 / EMS_GATE_MODE=2 / FR-508 状态机语义 / 冻结资产 / 主模型与 PlantStep 全部零改动；G-P1/G-P2 仪器链免重跑。
7. NOR 权重烧录 = Level C 板会话任务（专用外部加载器，仅 0x70200000 区域，禁 Mass Erase，单次令牌，用户在场）。
8. 部署 JSON 生成链新增一环：tcn_f64.keras → selected_model_weights_tcnq.json（新导出脚本）→ gen_fw_params.py 扩展生成 EMS_TCNQ_* 权重表（单一来源原则保持）。

【遗留疑点】TCN 板上实测时延（唯一待实测项，analyze 无 cycle 估算，估算 ~1-2ms 量级大概率入预算）；层② P_gen 网格一致率对新模型的实际表现（新数据点，无先验）。

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.
