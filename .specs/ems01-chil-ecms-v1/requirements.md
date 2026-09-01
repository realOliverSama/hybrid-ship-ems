# EMS01 CHIL ECMS 固件 v1 — Requirements

Spec: `ems01-chil-ecms-v1`
Status: requirements-approved（2026-08-31 用户批准：OQ 五项决议 + 架构三决策
  [① 991 标准 NOR 权重布局 + 唯一 Level C 权重烧录授权；② 单线 USART1；③ 不动时钟树]）
Created: 2026-08-31
Brainstorm: `./brainstorm.md`
Branch: `models-research`
工程目录: `firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS/`

## Overview

Phase 2.5 出口形态（用户裁决 2026-08-30）：**Winner = MLP-Q G2**（MLP-Q int8 26.2KB +
工况识别器 int8 ~50KB + 逆水段门控；MIL capture 0.589，S20 节油 1.02%，FR-318 通过）。
Phase 3 技术主线（roadmap §8，用户裁决 2026-08-30）：离开 MIL（Stateflow EML 内联手术弃用），
在 Model D 上建 **CHIL 平台**——PC Simulink plant ↔ 步进同步 ↔ STM32N647 真实控制器
（NPU 跑 tflite 推理、MCU 跑 ECMS 控制律），换模型 = 换权重文件，plant 零改动。

EMS01_CHIL_ECMS 是 CHIL 平台的**控制器侧固件工程**（已于 2026-08-31 立项，工程章程见工程根
README.md）。本 spec 定义其 **v1**（用户批准 2026-08-31，A+C 合并方案）：

- **M1**：CubeMX/CMake 工程骨架 + ST Edge AI 工具链打通 + MLP-Q int8 **板上推理验证**
  —— 同时关闭 Phase 2.5 **T-311**（FR-319 真板验收，避免重复施工与重复板上授权）；
- **M2**：ECMS 控制律 + G2 门控 **CPU 移植**（含工况识别器 int8）+ **本地激励开环注入验证**
  （主机回放记录轨迹，固件输出与 Simulink law=7 G2 参考逐点比对）。

**接口一次定型**（用户批准 2026-08-31）：预测签名 302 维窗 → q10/q50/q90 → ê=q50 →
SOC_ref 调制 → ECMS → P_gen_ref；CHIL 信号集 7→1。Phase 2.5 全部已训练模型共享该签名，
后续换 tflite 权重即换模型（v2 热切换、v3 CHIL 闭环均不在本 spec）。

## Fixed Decisions（2026-08-31 用户批准，实施不得偏离）

1. 工程起点 = 复制 Vendor `Projects/05_Serial`（唯一"USART1+LED+KEY+CMake+fw_gate
   原生合规"的最小工程）→ `EMS_Projects/EMS01_CHIL_ECMS/`；
2. AI 集成 = 移植 `99_Applications/991` 模式：`Model/` 套件（network.c / network_ecblobs.h
   / user_neuralart.json / my_mpools / generate 脚本）+ `Middlewares/AI`
   （NetworkRuntime1000_CM55_GCC.a + NPU HAL + npu_cache.c）手写 CMake 接线（prepare
   不自动接 AI 中间件）+ 991 的 RIF/NPU/npu_cache 初始化序列 + legacy LL_ATON 推理 API
   （st-ai API 迁移留 v2 评估）；
3. 工具链 = **ST Edge AI Core v2.2.0 本地 CLI**（`stedgeai generate/analyze`，全程命令行
   可复现；与 vendor NetworkRuntime1000 / X-CUBE-AI 10.2.0 生态对齐；AI Studio /
   Developer Cloud 不进构建链）；
4. 权重部署 = **991 标准 NOR 布局**（权重 @0x70200000 起 XSPI2 memory-mapped、激活
   AXISRAM3~6、溢出 HyperRAM；mpool 沿用 vendor 默认）+ 唯一已授权 Level C 权重烧录
   （FR-514）；
5. 通信 = **单线 USART1**（板载 CH340C，115200 8N1 起可调高），协议帧与文本日志同链路
   帧头区分；USB/网口/CAN/双线均不引入 v1；
6. 时钟 = **vendor 默认不动**（Debug/Release CPU 600MHz，NPU IC6 300MHz；Do Not Change）；
7. M2 注入 = **逐拍流式**（固件自维护 300s 滑窗，主机只发当前拍标量），复现真实部署形态；
8. EMS01 章程 README 事实更正：外部 NOR = MX25UM25645G **256Mbit = 32MB**（非 256MB）。

## Scope

### In Scope（M1：骨架 + 工具链 + 板上推理验证）

1. EMS01 CubeMX/CMake 工程骨架：`.ioc` 首次生成（**需用户授权**）+ CMake 工程组织
   （结构模板 = Vendor `Projects/05_Serial` 类裸机 CMake 工程），
   `atk_stm32n6_cmake_prepare.py` 可重复准备，`fw_gate.py` 结构契约满足；
2. ST Edge AI 工具链打通（**ST Edge AI Core v2.2.0 本地 CLI**，全程命令行可复现）：
   `mlp_h64_32_lr3e-4_q_int8.tflite`（26.2KB）与 `workclf_v1_int8.tflite`（21.9KB）→
   `stedgeai generate --target stm32n6` → NPU 部署包（network.c / network_ecblobs.h /
   user_neuralart.json / 权重 bin）→ 接入 `Appli/CMakeLists.txt`（Vendor
   `Middlewares/AI/`：NetworkRuntime1000_CM55_GCC.a + NPU HAL，手写 CMake 接线）；
   权重按 991 标准 NOR 布局部署（FR-514）；
3. MLP-Q int8 板上推理验证（= T-311 / FR-319 全部验收项，走 SRAM Debug，Level A/B）；
4. M1 部署报告归档（同时作为 Phase 2.5 T-311 的验收证据）。

### In Scope（M2：控制律移植 + 本地激励验证）

5. ECMS 控制律 C 移植（SOC_ref 调制 + λ_eff + 网格穷举，1s 控制周期，裸机主循环）；
6. G2 门控移植：工况识别器 `workclf_v1_int8.tflite`（21.9KB）NPU 推理 + 逆水段
   （upstream_class_id=6）识别 → 预测关断（K=0，ê=0 ≡ law=5 回退），迟滞 3 窗（30s）；
7. 本地激励开环注入验证：主机经 UART 回放 Simulink law=7 G2 运行记录
   （7 个状态量逐拍注入），采集固件 P_gen_ref 输出，与 Simulink 参考逐点比对；
8. M2 移植验证报告 + 工程文档/README 同步 + fw_gate PASS + 提交。

### Out of Scope（明确排除）

- 模型热切换机制 / 外部 NOR 模型库（→ v2）；
- 集成模型（MLP+TCN 双模型）部署（→ v2）；
- TCN / N-BEATS / CNN-MLP 等其它已训练模型上板（→ v2 全模型大表）；
- CHIL 通信 + 步进同步协议（→ v3；被 HIL 资料 6 项缺失阻塞，roadmap §8.6）；
- RTOS（v1 裸机 1s 主循环；SoftwarePackage 仅有 ThreadX，无 FreeRTOS；后续需求再评估）；
- B 线 trajectory-aware EMS（多步轨迹输出改变预测签名，属规划内演进，另立 spec）；
- Release Programming（appli/FSBL 烧录）/ Option Bytes / Mass Erase 等 Level C 操作
  （v1 板上验证仅限 SRAM Debug；Release 构建只为过 fw_gate 地址契约，不烧录）。
  **唯一例外**：模型权重 `network-data.hex` 写入外部 NOR @0x70200000 起（MLP-Q /
  workclf 各一偏移），用户已授权（2026-08-31）——走专用外部加载器
  `External_Loader/MX25UM25645G_ATK-CNN647B`，禁 Mass Erase，不动 FSBL@0x70000000；
- 经纬恒润 HIL 机柜及其外设预留（用户 2026-08-31：除非未来明确考虑机柜，否则不考虑；
  v1 的 `.ioc` 不为 v3 通信做任何预留，v3 通信形式留待后续裁决）；
- Phase 4 / Model E-RT（phase4-model-e 为独立 draft spec，未批准不得实施）。

## User Stories

- 作为研究者，我希望 MLP-Q int8 在真实 N6 板上的推理输出与 Python reference 一致、
  时延与内存实测归档，以便关闭 T-311 并证明 NPU 部署链路端到端可信。
- 作为研究者，我希望 ECMS+G2 在固件中对同一输入轨迹的 P_gen_ref 输出与 Simulink
  law=7 G2 参考逐点一致，以便证明控制律移植语义保真（不因 MCU 实现改变控制含义）。
- 作为研究者，我希望"换 tflite → 重新生成部署包 → 重新构建 → SRAM 加载"的手动换模型
  流程文档化且实际走过一遍，以便 v2 热切换与全模型大表有工程基础。

## Functional Requirements

- **FR-501**: THE 工程 SHALL 以 `SoftwarePackage/Projects/` 中**最贴近 v1 需求的已验证例程**
  （含 `.ioc`，本开发板已验证配置；复制改造已经用户 2026-08-31 授权）为起点，适配组织为
  满足 `fw_gate.py` 结构契约的 CubeMX/CMake 工程（`.ioc` + 根 `CMakeLists.txt` +
  `CMakePresets.json` + `Appli/` + `Binary/`；Debug 0x34000400 / Release 0x70100400
  地址契约），且 `atk_stm32n6_cmake_prepare.py` 可重复准备不破坏既有内容；
  例程选择与改造清单在 design 中确定。
- **FR-502**: THE 工具链 SHALL 从 `mlp_h64_32_lr3e-4_q_int8.tflite` 生成 ST Edge AI NPU
  部署包并集成进 `Appli/` 构建；编译记录 SHALL 归档：int8 模型大小、activation RAM、
  weight memory、Neural-ART HW coverage、SW/HYBRID fallback 算子清单。
- **FR-503**: THE 固件推理输入拼装 SHALL 与 Python 完全一致：302 维
  `[P(t−299..t) 升序 | v_ship | accel]`，归一化参数取自 `fleet_norm_v2.json`，
  feature order / window order / scaling / datatype / tensor shape 零偏差
  （STM32N6_ARCHITECTURE §10）；预处理参数单一来源，禁止固件侧另抄一份。
- **FR-504**: WHEN 输入同一组测试向量，THE 板上 int8 推理输出 SHALL 与 Python int8
  reference 一致（数值容差在 design 中定义并固定）；与 fp32 的量化精度差 SHALL 记录
  （参考 T-307 口径：MLP-Q delta_rmse +0.37%）。
- **FR-505**: THE 固件 SHALL 实测并记录单次 MLP-Q int8 推理时延与（M2 后）工况识别器
  推理时延，落入 NFR-501 预算或记录偏差与处置。
- **FR-506**: THE ECMS 控制律 C 实现 SHALL 与 Simulink law=7 同语义：
  `SOC_ref_eff = SOC_ref + K_S·ê_NN`（ê_NN = (Ê_NN − Ē_H)/E_scale，常数取自
  `kS_calibration.json`）；`λ_eff = max(λ₀ + K_soc·(SOC_ref_eff − SOC), Lambda_Floor)`；
  P_gen_ref 网格穷举使等效油耗最小；1s 控制周期。
- **FR-507**: THE G2 门控 SHALL 与 T-309/T-310 定案一致：工况识别器（30 点降采样功率 +
  速度输入，7 类段型）识别逆水段（class_id=6）→ 预测关断 K=0 → ê=0 回退（≡ law=5
  行为）；迟滞 m=3 窗（30s）；门控常数禁调参。
- **FR-508**: IF 权重加载失败 / 推理输出 NaN·Inf / 识别器失败 / 通信帧错误·超时，
  THE 固件 SHALL 进入已定義安全回退（ê=0 ≡ law=5）并记录故障事件；
  不得假设 UART/推理/DMA 永不失败（STM32N6_ARCHITECTURE §13）。
- **FR-509**: THE 本地激励验证 SHALL 覆盖：主机经 UART 逐拍注入 Simulink law=7 G2
  运行记录的 7 个状态量（P_load_total_kW / SOC / v_cmd / v_ship / accel / x_ship / t），
  采集固件 P_gen_ref 输出并与参考逐点比对；样本 SHALL 含 S20 场景片段与 ≥1 段
  留出航次片段；比对报告归档（误差指标与阈值在 design 中固定）。
- **FR-510**: THE UART 接口 SHALL 为**单线 USART1**（板载 CH340C USB 转串口，
  默认 115200 8N1，可调高）：协议帧与文本日志同链路以帧头区分；
  波特率 / 帧格式 / 注入与输出协议 / 超时 / 错误处理 SHALL 文档化；
  协议语义变更属主 Agent 决策，Subagent 不得自行改变。
- **FR-511**: WHEN 换预测模型（同签名 tflite），THE 手动流程
  （重新生成部署包 → 重新构建 → SRAM 加载 → 一致性复测）SHALL 文档化并以至少
  一次实际走通为证（可用同一 MLP-Q 权重走流程）。
- **FR-512**: WHEN 功能完成准备提交，THE 工程 SHALL 通过
  `python3 firmware/gate/fw_gate.py <项目路径>` 且 exit code = 0；
  Gate FAIL 不允许提交；`.ioc` 经授权修改后以 `FW_GATE_ALLOW_IOC=1` 重跑。
- **FR-513**: THE 交付 SHALL 含两份归档报告：M1 部署报告（FR-502/504/505 全部数据，
  格式满足 Phase 2.5 T-311 验收要求）与 M2 移植验证报告（FR-509 比对结果 + 回退测试
  记录）；M1 报告 SHALL 可直接作为 `.specs/nn-predictive-ecms-phase25/tasks.md`
  T-311 的验收证据引用。
- **FR-514**: THE 权重部署 SHALL 采用 991 标准 NOR 布局：`stedgeai generate` 产出
  `network_atonbuf.xSPI2.raw` → objcopy → `Binary/network-data.hex`
  （MLP-Q @0x70200000，workclf 偏移在 design 中固定）；mpool 沿用 vendor 默认
  （激活 AXISRAM3~6 @0x34200000 起、权重 xSPI2、溢出 HyperRAM）；禁止把 ATON
  缓冲链为 ELF 段（fw_gate 布局契约）；DEBUG（SRAM Debug）模式 appli SHALL
  自行初始化 XSPI2 memory-mapped 以读取 NOR 权重（照 991 DEBUG 模式）；
  权重烧录 SHALL 用专用外部加载器 `External_Loader/MX25UM25645G_ATK-CNN647B`，
  在授权范围内执行（禁 Mass Erase、不动 FSBL）。

## Non-Functional Requirements

- **NFR-501**: 预测器 1 Hz，单次推理 ≤10 ms（NPU int8）；识别器同频或更低，合计 ≤20 ms
  （沿用 NFR-301，实测后允许修正并记录）。
- **NFR-502**: int8 权重 + 激活 RAM 落在 N6 预算内（沿用 NFR-302：Phase 2 参考 47KB 权重 /
  <4KB 激活，允许放大 1~2 数量级；MLP-Q 26KB + workclf ~50KB + tensor arena 以实测为准）；
  内存布局（SRAM / EXTRAM / tensor arena 放置）属系统设计，由主 Agent 定。
- **NFR-503**: 全部构建与验证脚本化/命令化，可复现；禁止 GUI 手工形成不可复现状态；
  构建、准备、Gate、验证命令文档化于工程 README。
- **NFR-504**: 阶段隔离（STM32N6_ARCHITECTURE §22）：Implement → Build → FW Gate →
  主 Agent Review → SRAM Debug → 硬件验证；Build/Gate PASS 不自动授权下一阶段；
  v1 硬件操作限 Level A（只读）+ Level B（SRAM Debug，需授权）+
  **唯一已授权 Level C**（FR-514 权重烧录，范围见 Out of Scope 例外条）；
  其余 Level C 禁止。
- **NFR-505**: 控制确定性：裸机 1s 主循环，推理 + 门控 + ECMS 计算在一拍内完成且
  执行时间可测可记录；无 busy-wait 替代同步机制。
- **NFR-506**: 语义一致性：MCU 与 Python/Simulink 的特征顺序、归一化、窗口顺序、
  张量形状、控制公式、门控常数保持一致；任何"为 MCU 方便"的语义重定义禁止
  （STM32N6_ARCHITECTURE §1/§10）。
- **NFR-507**: 文档同步：新增/修改文件同步登记所在目录 README（AGENTS.md §20）；
  Session 完成按 §18 记录 `项目开发进展.md`；正式任务按 §15 模板汇报。
- **NFR-508**: 时钟树为 Do Not Change：v1 沿用 vendor 验证配置（Debug/Release 均
  CPU 600MHz，NPU IC6 300MHz）；实测时延超预算时由主 Agent 单独决策（可参考 991
  的 CPU 800MHz / PLL2 1GHz 方案），不得顺手调整。

## Acceptance Criteria

- [ ] AC-1（M1）：fw_gate exit 0；STEdgeAI 编译记录归档（FR-502）；
  板上一致性报告（FR-504）与时延实测（FR-505）经主 Agent 按 §14 验收
  → 引用该报告勾选关闭 Phase 2.5 T-311。
- [ ] AC-2（M2）：FR-509 比对报告误差在 design 固定阈值内；FR-508 回退行为测试记录；
  工况识别器板上推理验证（同 FR-504/505 口径）。
- [ ] AC-3：FR-511 手动换模型流程文档 + 走通记录。
- [ ] AC-4：提交前 fw_gate exit 0（FR-512）；README/索引/进展日志同步无遗漏；
  `git status` 干净（除约定不入库产物）；§15 汇报。

## Edge Cases

- 启动初期历史窗未满 300s：行为在 design 中定义（零填充 or 回退 ê=0），不得隐式处理；
- 推理输出 NaN/Inf、识别器输出非法类别 → FR-508 回退；
- UART 帧 CRC 错误 / 超时 / 主机断连 → 保持安全回退并记录；
- 权重/部署包完整性校验失败 → 拒绝进入正常控制，保持回退；
- 段切换瞬态识别抖动 → 迟滞 3 窗机制（FR-507），不得在固件侧另设阈值；
- 注入轨迹中 SOC 越限/降额片段：固件只复现控制器响应，保护逻辑（欠压降额等）属
  plant 侧，不在 v1 固件范围（design 明确边界）。

## Dependencies and Constraints

- 只读引用（语义单一来源，禁止复制后漂移）：`results/ecms_phase25/models/
  mlp_h64_32_lr3e-4_q/mlp_h64_32_lr3e-4_q_int8.tflite`、`results/ecms_phase25/classifier/
  workclf_v1_int8.tflite`、`datasets/fleet_norm_v2.json`、`results/ecms_phase25/deploy/
  selected_model_weights_v2.json`、`kS_calibration.json`（Ē_H/E_scale/K_S）、T-309 门控常数；
- 参考轨迹来源：Simulink law=7 G2 运行记录（需用 MATLAB 脚本跑一次产生注入/参考数据，
  走 `model/simulink_modelD_(EMS_NN_AECMS)/` 现有脚本体系，禁止 GUI 手工操作）；
- Vendor 约束：SoftwarePackage 只读（EMS_Projects 为例外工作区）；不复制 prepare 脚本与
  BSP 进工程（复制例程骨架本身不在此列，BSP/中间件仍以引用方式接入）；
  生成代码只改 USER CODE 区域；`.ioc`：首次生成/复制改造及 prepare 自动 patch
  （marker 块 / linker 配置）已经用户授权（2026-08-31），后续语义性修改仍需单独授权；
  授权修改后以 `FW_GATE_ALLOW_IOC=1` 重跑 Gate；
- 工具链：ST Edge AI Core v2.2.0 本地 CLI（已定，Fixed Decisions #3），
  命令行可复现（NFR-503）；
- 硬件：AlienTek ATK-DNN647 板 + 外接 ST-LINK（SWD 10-pin，板载无 ST-LINK）；
  板上授权（2026-08-31）：Level A + Level B（SRAM Debug）+ 唯一 Level C
  （FR-514 权重烧录）；板上操作需用户在场并分阶段执行（AGENTS.md §11）；
- Git：分支 `models-research`；Subagent 不得 `git add/commit/push`；
  提交由主 Agent 验收后执行；
- 执行分工：主 Agent 决策/监督/验收；实现/构建/测试等执行委派 subagent
  （zai-coding-cn/glm-5.3-flash:high），委派须含 Goal/Scope/Do Not Change/Validation/
  Acceptance/Escalate If（AGENTS.md §4）。

## Open Questions（全部已决议，2026-08-31 用户答复）

- [x] OQ-1：ST Edge AI 工具链形态 → **已定（2026-08-31 调研完成）**：ST Edge AI Core
  v2.2.0 本地 CLI（与 vendor NetworkRuntime1000 / X-CUBE-AI 10.2.0 生态对齐，
  全程命令行可复现）；Developer Cloud 仅作前期 farm benchmark 参考，不进构建链。
- [x] OQ-2：`.ioc` 授权 → **已授权**；且采用更简办法：从 `SoftwarePackage/Projects/`
  选最贴近本工程需求的**已验证例程**（含 `.ioc`）复制改造为起点，减少工作量（FR-501）。
- [x] OQ-3：v3 预留 → **不预留**；暂不考虑经纬恒润机柜，除非以后明确考虑，否则都不考虑。
- [x] OQ-4：T-312 收口 → Phase 2.5 能闭环仿真且闭环性能指标已探明（T-310 已有
  capture/FR-318/fuel 口径）即可结束；M1 关闭 T-311 后即可安排 T-312 收尾。
- [x] OQ-5：EMS01 下 pi-spec 残留 → **用不到的删除**（已批准；正式 spec 在仓库根
  `.specs/ems01-chil-ecms-v1/`，经符号链接对本 Session 工具可见）。
