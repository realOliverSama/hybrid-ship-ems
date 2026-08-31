# EMS01 CHIL ECMS 固件 v1（STM32N6 预测型 A-ECMS 控制器侧） Brainstorm

Spec: `ems01-chil-ecms-v1`  
Status: brainstorm-complete  
Created: 2026-08-31

## User Intent

## 用户意图
基于正点原子 STM32N647 开发板开发第一版 EMS 固件工程（EMS01_CHIL_ECMS v1），保持 models-research 分支（本地与 GitHub 同步）。分工：主 Agent 只决策/监督/验收，读文档与代码等执行工作委派 subagent（zai-coding-cn/glm-5.3-flash:high，已按用户指令加入 modelScope）。用 spec-driven-development 流程。

## 技术背景共识（CHIL 讲解后用户确认理解）
- Phase 3 主线 = 离开 MIL（Stateflow EML 内联手术弃用：TCN 16.6 万/N-BEATS 145 万参数无法内联）→ 在 Model D 上建 CHIL 平台（PC Simulink plant ↔ 步进同步 ↔ STM32N6 真实控制器）→ 跑全模型大表。HIL 台架（经纬恒润 + Model E-RT）属 Phase 4，并行不阻塞，汇合点 = plant 侧换台架。
- 换模型 = 换 N6 权重文件，plant 零改动；平台可信性锚点 = CHIL 复现 MIL 的 capture≈0.589、S20 节油≈1.02%。
- 用户问"EMS 控制工程代码与接口（针对 Model D）是否一次定型、之后只切预测模型即完成多模型 CHIL"→ 答"是"并划定边界后，**用户批准**。

## 已批准的方案（A+C 合并，用户 2026-08-31 批准）
v1 = M1 + M2：
- M1：CubeMX CMake 骨架（模板 05_Serial 类裸机工程）+ STEdgeAI 工具链打通（tflite→NPU 部署包，仓库内无先例）+ MLP-Q int8 板上推理验证（int8 大小/activation RAM/weight memory/Neural-ART HW coverage/SW-HYBRID fallback 算子清单/实测时延/量化精度差/板上 vs Python reference 一致性）= 关闭 Phase 2.5 T-311（FR-319 真板验收，需用户在场+分阶段授权）。
- M2：ECMS 控制律 + G2 门控移植（CPU，含工况识别器 int8 ~50KB）+ 本地激励开环注入验证（主机回放数据集 P_load 轨迹，UART 输出 P_gen_ref 与 Simulink 参考逐点比对）。
- 不进 v1：模型热切换（v2）、CHIL 通信+步进同步（v3，被 HIL 资料 6 项缺失阻塞）、RTOS（v1 裸机 1s 主循环；全 SoftwarePackage 仅 ThreadX 无 FreeRTOS）。
- 裁决理由：方案 B（直奔 CHIL）被 HIL 资料缺失堵死；纯 C（单做 T-311）= 工具链打通做两遍+多一次板上授权；纯 A = 板上验证两轮。

## 一次定型的接口（用户批准，边界明确）
输入 302 维窗 [P(t−299..t)|v_ship|accel]（fleet_norm_v2 归一化）→ q10/q50/q90 → ê=q50 → SOC_ref 调制 → ECMS 等效因子+网格穷举 → P_gen_ref；CHIL 信号集 7→1（P_load_total_kW/SOC/v_cmd/v_ship/accel/x_ship/t → P_gen_ref_kW）。Phase 2.5 全部已训练模型共享签名，换 tflite 即换模型。边界：① B 线 trajectory-aware EMS 故意改签名（规划内演进）；② 集成模型双模型部署属 v2。

## 关键事实（scout 只读调查）
- STEdgeAI 链路仓库内完全空白（git check-ignore + 全盘搜索验证非 .gitignore 遗漏；int8 tflite 已入库：mlp_q 26.2KB +0.37%、workclf ~50KB、tcn_f64 219.7KB 等，编译从未执行）。
- Vendor 无 CMake 版 AI 示例（991~995 全为 CubeIDE）；Middlewares/AI 有 NetworkRuntime1000_CM55_GCC.a + NPU HAL，接入 Appli/CMakeLists.txt 无参考。
- fw_gate 契约：Debug 0x34000400 / Release 0x70100400、Appli/ 结构、7 项检查；结构模板 05_Serial。
- 控制语义/ê 定义/门控逻辑必须与 Phase 2.5 出口形态一致（章程 Do Not Change）；.ioc 生成/修改需用户授权；Build/Gate/Debug/Flash 阶段隔离。

## 遗留疑问（进入 requirements 前需回答）
1. spec 存放位置：提议仓库根 .specs/ems01-chil-ecms-v1/（与既往 8 个 spec 一致），EMS01 下 pi-spec 自动生成的 .specs/config.yaml 模板如何处理。
2. .ioc 生成授权方式（新建工程的首次 .ioc 生成是否本次一并授权）。
3. HIL 资料 6 项提供时点（不阻塞 v1，阻塞 v3）。
4. T-312（Phase 2.5 报告）与 EMS01 的时间关系。

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
