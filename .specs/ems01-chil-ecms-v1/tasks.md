# EMS01 CHIL ECMS 固件 v1 — Tasks

Spec: `ems01-chil-ecms-v1`
Status: implementation-complete
Created: 2026-08-31
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`

## Implementation Plan

> 执行模式（沿用 phase25 传统，用户 2026-08-31 定）：主 Agent 只负责方案/监督/验收；
> 实施由 subagent（zai-coding-cn/glm-5.3-flash:high）完成。每个任务自带委派要素
> （Goal / Scope / Do Not Change / Validation / Acceptance / Escalate If），执行者不得越界。
>
> **通用约束（所有任务生效）**：
> - 分支 `models-research`；工程根
>   `firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS/`（下称 `$P`）；
>   模型工作区 `model/simulink_modelD_(EMS_NN_AECMS)/`（下称 `WS`）；
> - Python 一律用 `model/simulink/python/.venv_tf`（不得另建环境）；
> - Subagent 默认不得 `git add/commit/push`；完成后交还主 Agent 验收（AGENTS.md §14）；
> - **门禁阈值全部 design 固定**（L0 差分 ≥99% 同网格点且 max ≤20 kW；FR-504
>   max |Δq50| ≤0.25 kWh 且 RMSE 比 ≤1.1；层② ≥99% ≤1 网格步），
>   **不得为 PASS 调整**；阈值修改 = design 修订 = 主 Agent 决策；
> - 硬件授权边界：Level A/B（SRAM Debug）+ 唯一 Level C（FR-514 权重烧录，
>   专用外部加载器 `External_Loader/MX25UM25645G_ATK-CNN647B`，禁 Mass Erase，
>   不动 FSBL@0x70000000）；板上任务需用户在场；
> - `.ioc` 授权范围：05_Serial 复制改名 + Appli 上下文 XSPI2 增补 + NPU RIF 授权组文本增补（2026-08-31 用户批准扩展，见 design）；
>   授权期 Gate 用 `FW_GATE_ALLOW_IOC=1`；
> - Vendor 只读（EMS_Projects 例外）；不复制 prepare 脚本与 BSP 进工程
>   （Middlewares/AI 以原路径引用）；生成代码只改 USER CODE 区域；
> - 时钟树 Do Not Change（CPU 600MHz / NPU IC6 300MHz，NFR-508）；
> - 触碰 Do Not Change 或需要设计层判断时，立即停下按 AGENTS.md §5 格式 Escalate。
>
> **编号说明**：相对 design.md Rollout 节有一处顺序微调——"参考轨迹生成 + L0 差分"
> 由 T-506 提前为 T-504（内容不变），使移植错误在 AI 集成前的纯 PC 阶段暴露；
> 硬件无关任务（T-503~T-506）全部前置，板上操作合并为 T-507/T-508 一个会话。

- [x] 1. 工程骨架：复制 05_Serial → EMS01 + .ioc 增补 + 首次 Gate
  - ID: T-501
  - Requirement(s): FR-501, FR-512, NFR-503, NFR-504
  - Goal: `$P` 成为 fw_gate 合规的 CubeMX/CMake 工程：复制 `Projects/05_Serial/`
    （排除 `Appli/build*/`、`Binary/flash/` 等构建产物）→ 工程改名（`.ioc` 文件名、
    CMake 工程名引用）→ `.ioc` Appli 上下文增补 XSPI2（参考 `99_Applications/991/
    991_AI_People_Detection.ioc` 的 XSPI2 配置：MemorySize=256Mbit MACRONIX，
    八线 PN 引脚组）→ prepare 准备 → Debug + Release 构建通过 →
    `FW_GATE_ALLOW_IOC=1 python3 firmware/gate/fw_gate.py $P` exit 0。
  - Context: 05_Serial 是唯一"USART1+LED+KEY+CMake+fw_gate 原生合规"最小工程；
    XSPI2 增补是 DEBUG 模式读 NOR 权重的前提（991 DEBUG 模式同款）；
    prepare 自动处理 BSP 注入与 linker 选择（ATK AUTO 块）。
  - Scope: `$P`（全新增）；只读参考 `Projects/05_Serial/`、`99_Applications/991/`、
    `Tools/atk_stm32n6_cmake_prepare.py`。
  - Do Not Change: 05_Serial 原工程与任何 vendor 文件；linker 脚本与地址契约
    （prepare 自动配置，不得手改）；时钟树；`.ioc` 不得有 XSPI2 增补以外的语义性修改。
  - Validation: prepare 连续两次运行幂等（第二次无新增改动）；Debug/Release 构建
    0 error；fw_gate exit 0。
  - Acceptance: `firmware/gate/fw_gate_result.json` overall PASS；工程结构与设计
    §工程目录一致；README.md 结构节起步更新。
  - Escalate If: `.ioc` 需要超出 XSPI2 增补的修改；prepare 对 XSPI2 增补的 linker
    处理异常；构建错误根因指向 vendor 工程本身（非复制错误）。

- [x] 2. STEdgeAI 工具链 + 两模型部署包生成 + 编译记录归档
  - ID: T-502
  - Requirement(s): FR-502, FR-514, NFR-501, NFR-502, NFR-503
  - Goal: 核验/安装 **ST Edge AI Core v2.2.0** 本地 CLI（锁定版本，不得更换）；
    对 MLP-Q 与 workclf 分别执行 `stedgeai analyze --target stm32n6`（HW coverage /
    activation RAM / weight memory / SW-HYBRID fallback 算子清单，归档）与
    `stedgeai generate`（mpool 基址：MLP-Q 0x70200000 / workclf 0x70400000，
    模板复制自 991 `Model/my_mpools/stm32n6.mpool` 仅改 constants 池）→
    `network.c / network_ecblobs.h / network_atonbuf.xSPI2.raw` → objcopy →
    `Binary/network-data-mlpq.hex` / `network-data-workclf.hex`；
    `Model/<m>/generate-n6-model.sh` 可重复执行。
  - Context: tflite 源（只读，复制副本进 `$P/Model/<m>/`）：
    `WS/results/ecms_phase25/models/mlp_h64_32_lr3e-4_q/mlp_h64_32_lr3e-4_q_int8.tflite`（26.2KB）、
    `WS/results/ecms_phase25/classifier/workclf_v1_int8.tflite`（21.9KB，2026-08-31 t312 真 int8 重导出）；
    int8 量化已在 T-307 完成（val 校准），本任务不重新量化；
    workclf Conv1D 实测全部上 NPU（2026-08-31 更正，design R2；SW 仅 I/O 适配器+Pad+Softmax，如实记录）。
  - Scope: `$P/Model/`、`$P/Binary/`；编译记录归档 `$P/Model/<m>/`（analyze 输出 +
    generate 日志）；只读引用 tflite 源与 991 Model/ 模板。
  - Do Not Change: tflite 源文件；stedgeai 版本（v2.2.0）；mpool 除 constants 池
    基址/容量外的任何池定义；不得改模型导出形态规避 fallback。
  - Validation: `stedgeai --version` = 2.2.0；analyze 报告四项指标齐全；
    generate 产物齐全且 `network.c` 权重基址正确（0x70200000 / 0x70400000）；
    hex 地址抽查（head/tail 记录）。
  - Acceptance: 两模型部署包完整；analyze 报告与 generate 日志归档；
    MLP-Q Dense 全 HW（若有意外 fallback → Escalate）；workclf fallback 清单记录。
  - Escalate If: v2.2.0 无法安装/激活；MLP-Q 出现非预期 fallback；mpool 基址修改
    不被工具接受；generate 产物与 991 结构显著不符。

- [x] 3. 参数生成器 + 控制律 C 移植（REF 后端）+ L0 单元用例
  - ID: T-503
  - Requirement(s): FR-503, FR-506, FR-507, NFR-505, NFR-506
  - Goal: `Host/tools/gen_fw_params.py`（从 5 个只读源生成 `ems_params.c/.h`，
    sha256 溯源头，提交入库）+ `ems_ctrl.c/.h`（EML 逐条移植，平台无关，double，
    禁动态内存）+ `ems_infer_ref.c`（CPU double 前向：MLP-Q + workclf，
    因果 Conv 边界/P pooling 丢尾/GAP/旁路融合逐条复刻）+ `Host/tests/test_ctrl_diff.py`
    单元用例全过。
  - Context: 语义基准 = `WS/results/ecms_phase25/gating/eml_ecms_step_mode2.m`（函数
    `ecms_nn`）+ design.md §Data Model（滑窗/门控/ECMS/单位链全部细则）+
    scout 提取报告（11 条裸机复现风险）；常数源见 design §Data Model 表；
    BSFC 必须用 `WS/config/ecms_bsfc_override.mat` 501 点表（非 7 点默认表）。
  - Scope: `$P/Appli/App/`（ems_ctrl/ems_infer_ref/ems_params/ems_config）、
    `$P/Host/tools/`、`$P/Host/tests/`。
  - Do Not Change: EML 任何语义（平 fill 初始化、t<lastT 全复位、NaN 跳写指针前进、
    argmax/argmin 平局取首、SOC≤0.105 首个可行 Pg break + 末尾救急、迟滞 3-tick、
    BSFC 端点钳位与严格小于插值、mf/mbe 单位链 3.6e6 与 3600 不得"统一"）；
    常数值；协议语义。
  - Validation: 单元用例（覆盖上述全部风险点 + 充电禁止分支 + G0~G3 门控模式的
    K 计算）全过；host gcc `-Wall -Wextra` 零警告；ems_params 溯源 sha256 与源文件
    实算一致；交付 EML 行号 → C 行号的语义映射表。
  - Acceptance: 用例全过 + 映射表经主 Agent 抽查；代码可读性满足移植审查
    （主 Agent 对照 EML 审读关键段）。
  - Escalate If: 发现 design 风险清单之外的 EML 语义难以 C 复刻；单元用例失败根因
    指向 EML 语义理解分歧（不得自行"修正"EML 语义）。

- [x] 4. M2 参考轨迹生成 + L0 差分硬门禁
  - ID: T-504
  - Requirement(s): FR-506, FR-507, FR-509（L0 层）, NFR-506
  - Goal: `WS/scripts/66_export_fw_reference.m`（脚本驱动，禁 GUI）：law=7 G2
    （GATE_MODE=2 经 63_integrate_gate_v2.m 注入 chart）跑 S20 场景 + ≥1 段留出航次
    （v2 test split 航次，评估用途合法），逐 tick 导出参考 CSV
    `[t, P_load, SOC, v_cmd, v_ship, accel, x_ship, P_gen_ref]`（+ 可获得的
    ê/K/cls 内部量）→ `WS/results/ecms_phase25/fw_reference/`；
    随后在 PC 上跑 L0 差分：host gcc 编译 ems_ctrl + ems_infer_ref + ems_params，
    逐 tick 对比参考 P_gen_ref，**硬门禁：≥99% tick 同网格点，max |Δ| ≤20 kW**。
  - Context: G2 复现方式（scout §4）：`GATE_MODE=2; eval(fileread('scripts/
    63_integrate_gate_v2.m'))` + 64_run_matrix_v2.m:217-233 变量设置
    （EMS_CTRL_LAW=7/EMS_MODE=1/AI_Pred_Source=0 + SCN 场景变量）；
    63 脚本重写 chart Script 并 save_system（T-310 先例，脚本驱动允许）；
    MATLAB 执行可用 MATLAB MCP 或 -batch。
  - Scope: `WS/scripts/66_*`、`WS/results/ecms_phase25/fw_reference/`、
    `$P/Host/tests/`；允许按 63 脚本机制更新模型 chart（G2），其余模型文件不动。
  - Do Not Change: 判定口径 D4 与一切常数；G0/G1/G3 不需要导出；
    plant/控制律任何参数；参考轨迹生成后不得重新生成"直到好看"。
  - Validation: 参考 CSV 行数 = 仿真 tick 数；输入信号与 runs_gate_v2 缓存的
    对应航次一致（抽查）；L0 差分门禁通过（design 固定阈值）。
  - Acceptance: 参考 CSV + 差分报告（exact-match 率、max |Δ|、偏差 tick 清单）归档；
    门禁通过。
  - Escalate If: 差分超门禁（先修 ems_ctrl 重跑；连续失败或根因指向语义理解 →
    Escalate）；Simulink 复现 G2 运行与 runs_gate_v2 缓存结果不一致。

- [x] 5. CMake AI 集成 + NPU 后端 + INFER_TEST 链路 + 构建
  - ID: T-505
  - Requirement(s): FR-502, FR-503, FR-504（链路）, FR-514（只读段）, NFR-503
  - Goal: `Appli/CMakeLists.txt` 手写 AI 接线块（marker 注释包围，与 ATK AUTO 块并存：
    sources = App/*.c + 两模型 network.c + vendor npu_cache.c 原路径引用；
    includes = Middlewares/AI/Inc + Npu/Devices + Model 目录；
    link = NetworkRuntime1000_CM55_GCC.a）+ `ems_infer_npu.c`（双模型实例宏区分，
    LL_ATON 推理序列照 991 app.c:153-161）+ `ems_app.c` 与 main.c USER CODE
    （991 初始化序列：SystemIsolation_Config RIF/NPU → npu_config → npu_cache_config →
    DEBUG 模式 MX_XSPI2_Init + NOR memory-mapped）+ INFER_TEST 协议 handler +
    Debug/Release 构建通过。
  - Context: prepare 不自动接 Middlewares/AI（非扁平中间件）；991 应用层为 ThreadX
    但其初始化序列 bare-metal 可移植（scout 已定位行号）；DWT CYCCNT 使能用于
    infer_us/ctrl_us 测量（代码使能，不涉时钟树）；固件侧向 LL_ATON 输入缓冲喂
    归一化后数据，int8 量化由 tflite 内参数完成（输入语义需实测验证，design R8）。
  - Scope: `$P/Appli/`（CMakeLists 用户块、App/、Core USER CODE）；
    只读引用 vendor Middlewares/AI 与 991 源。
  - Do Not Change: ATK AUTO 三块（prepare 维护）；vendor 文件；991 初始化语义；
    linker/地址契约；时钟树；RIF 配置不得弱化（不得为省事放开全部权限）。
  - Validation: Debug/Release 构建 0 error；`readelf -S` 确认全部 ALLOC 段落在
    fw_gate 契约区间（权重绝对寻址不占段）；fw_gate exit 0（构建阶段）。
  - Acceptance: 构建通过 + 布局检查记录；初始化序列与 991 逐行对照表交付。
  - Escalate If: NetworkRuntime 链接/ABI 问题；991 初始化存在 ThreadX 硬依赖；
    RIF 配置需求超出 991 范围；XSPI2 DEBUG 初始化与 05_Serial 底座冲突。

- [x] 6. UART 协议 + 主机注入工具 + Python int8 参考工具
  - ID: T-506
  - Requirement(s): FR-510, FR-504（参考侧）, FR-509（工具侧）, NFR-503
  - Goal: `ems_proto.c/.h`（design §UART 帧表全实现：编解码/CRC16/文本行分流/
    超时与 STALE）+ `Host/ems_inject.py`（回放参考 CSV → 串口逐拍注入 → 采集
    RSP → 三层比对报告 fw_compare.csv/json）+ `Host/tools/gen_reference_int8.py`
    （Python tflite 解释器 int8 推理参考：FR-504 的 200 向量集 = val split
    seed=20260831 抽样，以及 T-508 注入轨迹全程的 int8 参考输出）+ 协议环回测试。
  - Context: FR-504 用 **val** 向量（数值一致性检查，非模型评估，规避 test 纪律
    争议）；M2 注入用 test 留出航次属评估用途（与 T-310 FR-318 同性质，合法）；
    int8 参考必须复用 T-307 同款 tflite 与归一化（口径一致）；
    主机工具依赖限 pyserial/numpy + tflite runtime（.venv_tf 已有）。
  - Scope: `$P/Appli/App/ems_proto.*`、`$P/Host/`。
  - Do Not Change: 协议语义（帧格式/消息类型/单位）；T-307 int8 口径；
    .venv_tf 以外环境。
  - Validation: 环回测试（正/反 CRC、截断帧、垃圾字节流恢复、日志/帧分流）全过；
    gen_reference_int8 抽查 10 向量与 T-307 t307_int8_report.json 数值口径一致；
    ems_inject.py --help 文档完整。
  - Acceptance: 测试全过 + 工具可重跑（NFR-503）。
  - Escalate If: int8 参考与 T-307 报告数值对不上（可能 tflite runtime 版本差异 →
    返回主 Agent，不得自行换 runtime）。

- [x] 7. 板上会话：权重烧录 + M1 验证 + 换模型流程走通（需用户在场）
  - ID: T-507
  - Requirement(s): FR-504, FR-505, FR-511, FR-513（M1）, FR-514, NFR-501, NFR-502, NFR-504
  - Goal: 按授权执行唯一 Level C + Level B 会话：
    ① Level A 读取 NOR 0x70200000/0x70400000 区域现状（记录）；
    ② 权重烧录：STM32CubeProgrammer + `External_Loader/MX25UM25645G_ATK-CNN647B`，
    写入 `Binary/network-data-mlpq.hex`（@0x70200000）与
    `network-data-workclf.hex`（@0x70400000），记录镜像 sha256/地址/加载器版本；
    ③ SRAM Debug 加载 Debug appli（debug-stm32n647-sram 工作流）；
    ④ FR-504：ems_inject.py 跑 INFER_TEST ×200 向量，板载（归一化+NPU int8）
    vs Python int8 reference——**硬门禁：max |Δq50| ≤0.25 kWh，RMSE 比 ≤1.1**；
    workclf 类别一致率 ≥99%（logits max |Δ| 报告）；
    ⑤ FR-505：DWT CYCCNT 实测两模型推理时延（预算 ≤10ms / 合计 ≤20ms）；
    ⑥ FR-511：换模型流程实际走通一遍（重新 generate 同一 MLP-Q → 重新构建 →
    SRAM 加载 → INFER_TEST 复测一致性）并文档化；
    ⑦ 产出 **M1 部署报告** `WS/results/ecms_phase25/deploy/m1_deploy_report.md`
    （含 T-311 全部验收项：int8 大小/activation RAM/weight memory/HW coverage/
    fallback 清单/实测时延/量化精度差/板上 vs Python 一致性）。
  - Context: 本任务完成即满足 phase25 T-311 验收，主 Agent 引用 M1 报告勾选 T-311；
    板上操作分阶段：烧录与 Debug 各自确认后执行（AGENTS.md §11 阶段隔离）。
  - Scope: `$P`、`WS/results/ecms_phase25/deploy/`（报告）；调试工具按
    debug-stm32n647-sram / release 相关 skill 纪律。
  - Do Not Change: FSBL 与 NOR 其它区域；Option Bytes；Boot 配置；
    **禁 Mass Erase**；不得烧录 appli/FSBL（v1 不烧 Release）；
    不得为修复推理失败改时钟/RIF/MPU（Escalate）。
  - Validation: 烧录记录完整（镜像/地址/加载器）；FR-504 硬门禁通过；
    时延数据归档；换模型流程复测一致。
  - Acceptance: M1 部署报告含 T-311 全部验收项且硬门禁通过；主 Agent §14 验收后
    引用报告勾选 phase25 T-311。
  - Escalate If: 烧录工具/外部加载器异常（停止，不得试随机地址/随机配置）；
    NPU 推理失败且根因不明；FR-504 系统性超差（疑输入语义/量化配置问题）。

- [x] 8. 板上 M2 注入比对 + 故障注入（同会话延续，需用户在场）
  - ID: T-508
  - Requirement(s): FR-508, FR-509, FR-513（M2）, NFR-504
  - Goal: ① REF 模式注入（层① 板上复核，预期与 L0 一致）；② NPU 模式注入
    S20 片段 + ≥1 留出航次：层② board-NPU vs host-int8（**硬门禁 ≥99% tick
    ≤1 网格步**）；层③ board-NPU vs Simulink 参考（分布报告，报告性不设门禁）；
    ③ FR-508 故障注入：NaN 输入 / 停发超时 / 垃圾帧 / CMD_RESET 恢复，逐项记录
    回退行为是否符合 design 状态机；④ 产出 **M2 移植验证报告**
    `WS/results/ecms_phase25/deploy/m2_port_report.md`。
  - Scope: `$P`、`WS/results/ecms_phase25/deploy/`（报告）。
  - Do Not Change: 同 T-507；门禁阈值 design 固定不得调整；
    不得为改善层③分布改任何常数。
  - Validation: 层②硬门禁通过；故障注入四项行为记录；报告含三层比对全部数据。
  - Acceptance: M2 报告归档且层②通过；主 Agent §14 验收。
  - Escalate If: 层②超门禁；故障注入暴露 design 状态机未覆盖的行为（设计层）。

- [x] 9. 收尾：Gate + 文档同步 + 提交
  - ID: T-509
  - Requirement(s): FR-512, FR-513, NFR-507
  - Goal: 最终 `python3 firmware/gate/fw_gate.py $P` exit 0；文档同步：
    `$P/README.md`（结构/构建/验证命令/产物 + NOR 容量更正 **32MB**）、
    `WS/README_Model.md`（§3.3 迟滞口径修正：m=3 控制 tick = 3s，以 EML 实现为准；
    §5 产物表登记 m1/m2 报告与 fw_reference）、`EMS_Projects/README.md`（如有
    工程清单变化）、`.specs/README.md`（本 spec 状态）；`项目开发进展.md` 当日记录；
    主 Agent 按 §14 验收真实 diff 后提交（§15 模板）。
  - Scope: 上述文档文件；git 操作仅主 Agent。
  - Do Not Change: 门禁阈值；报告数据；与任务无关的文件。
  - Validation: fw_gate exit 0；绝对路径扫描（AGENTS.md §20.2：
    `grep -rE '[A-Za-z]:[\\/]|/Users/|/home/'` 排除 URL/env 无命中）；
    `git status` 干净（除约定不入库产物）。
  - Acceptance: 主 Agent §14/§15 验收通过；phase25 T-311 已勾选关闭。
  - Escalate If: gate FAIL 且根因非本任务改动。

## Verification Checklist

- [x] Requirements IDs are referenced by tasks（T-501~T-509 ↔ FR-501~FR-514 / NFR-501~508）。
- [x] Design testing strategy is represented by tasks（L0=T-503/504、L1=T-507、L2=T-508、
  Gate=T-501/505/509）。
- [x] Each task has a clear validation command or manual check。
- [x] Rollback/migration：全部新增文件，既有资产不动；ems_params 重新生成即回滚；
  权重烧录区与 FSBL/appli 不重叠。

## Execution Notes

- 任务按 T-501 → T-509 顺序执行；T-503/T-504 与 T-502 可并行（无相互依赖，
  但同一 Working Tree 同一时间只允许一个写权限 Agent，并行须主 Agent 编排）。
- T-507/T-508 合并为一个板上会话（用户在场），前置条件：T-501~T-506 全部验收。
- 每个任务验证通过后立即勾选；偏离 design 须先记录于 design.md（design 修订）。
- 实施由 subagent（zai-coding-cn/glm-5.3-flash:high）完成；主 Agent 负责每个
  任务的验收（AGENTS.md §14）与最终提交（§8 Multi-Agent Git 规则）。
- 完成 T-507 后，主 Agent 引用 M1 报告勾选 `.specs/nn-predictive-ecms-phase25/`
  T-311（Phase 2.5 收口链条：T-311 → T-312 另议，见 requirements OQ-4 决议）。
