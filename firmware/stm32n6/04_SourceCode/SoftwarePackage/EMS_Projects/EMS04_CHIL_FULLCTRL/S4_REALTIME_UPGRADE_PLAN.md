# S4_REALTIME_UPGRADE_PLAN — A 类实时性升级（UART 提速 + RTOS 化）与 fp16 推理路线决策计划

> 日期：2026-09-30 · 分支 feat/phase4-g · 性质：研究/工程**计划文档**（决策依据 → 方案 → 验收门 → 1Hz 账本；不含已实施的代码改动）
> 证据口径：文中行号对应当日工作树未提交态（feat/phase4-g）；`experiments/` 下契约文件不入库，属本地暂存资产。
> 结论先行：**执行层真 1 Hz 已由口径 v2 的 EXEC_REFRESH(0x12/0x13) 打通，但被单线程主循环的 DP 阻塞窗（~5.2 s）结构性拖慢（响应滞后 ~6–7 s）；
> A 类（UART 921600 + ThreadX 任务化）用既有已证组件把执行层刷新做到端到端 <10 ms（真 1 Hz 新鲜），10 s 决策拍占空 ~75%→~63%；
> 1 Hz 完整决策（推理+DP）在位级精确红线下不可达（P2 备忘定案），需 B 类（fp16 推理）叠加 C 类（DP 近似化，需另行裁决）才可重议。**

---

## 1. 背景与 1 Hz 账本

### 1.1 设计要求考古：10 s 拍从哪来，1 s 门控从哪来

| 层 | 冻结口径 | 证据 |
|---|---|---|
| 决策拍 10 s | FR-811 冻结 "decision interval 10 s"（与 horizon 300 s / K=30×10 s bin 粒度耦合，FR-873 禁止冻结后改动） | `.specs/phase4-f-practical-predictive-ems/requirements.md:111-122`（10 s 定义于 :114；FR-873 于 :343-345） |
| 契约侧 | `decision_period_s = 10`（ctrace 权威记录）；执行层 §4.3 "CODE_ECMS_ORACLE（1 s 门控；段内 e_hat≡d1 恒定）" | `experiments/softhil_ems03_2026-09-25/ems04_s1/LAW6_C2_CONSUMPTION_CONTRACT.md:21`（决策拍）与 `:155-157`（§4.3 执行层 1 s 门控） |
| 早期工程 | v3 闭环为 1 s/拍（Model D 植物子步 100×0.01 s，单步推进） | `.specs/HANDOFF-v3-closedloop.md:24,26`（"每控制拍（1 s）单步推进"、"1 Hz 实时节拍"） |
| 模型换型 | EMS04 契约明记 **"拍周期 10s ≠ Model D 1s"**——Model E PlantStep 子步结构需按实测定 | `experiments/softhil_ems03_2026-09-25/S4_FULLCTRL_CONSTRUCTION_SPEC.md:45` |
| 执行层 1 s 门控补齐 | 口径 v2（2026-09-29 用户裁决）：新增 CMD_EXEC_REFRESH(0x12, 16B)/RSP_EXEC_REFRESH(0x13, 12B) 附加命令，冻结契约零改动；板以 (SOC, P_load, 上拍 d1) 重算执行层 grid（与拍内执行层逐字同式） | `experiments/softhil_ems03_2026-09-25/S4_CALIBER_V2_EXEC_GATE.md`（§1 v1/v2 差异表、§2 协议）；固件实现 `Appli/App/ems04_tick_ctrl.c:386-390`（注释明记"口径 v2，契约 §4.3"）与命令码 `Appli/App/ems04_tick_ctrl.h:29-32` |

要点：**10 s 是冻结的决策粒度，1 s 是契约本来就要求的执行层门控粒度**。口径 v2 把执行层从 10 s ZOH 补齐到 1 s 门控，是向契约靠拢，不是改设计；本计划的 A 类升级解决的是"1 s 门控在板上的**及时性**"。

### 1.2 当前时延账（CPUF32 形态板实测）

偏差④修复后板交付形态为 REATT fp32 全 CPU（`EMS_REATT_FORM=CPUF32`，commit d2c5fb82）。单拍（TICK_CTRL）时延构成：

| 段 | 实测 | 证据 |
|---|---|---|
| REATT 推理（fp32 CPU） | **1.12 s/帧**（6 pilot 锚 CMD_REATT 回放，板==keras 6/6 maxdiff=0.0） | 项目开发进展.md 2026-09-29（十五续）P3e 板级回归段 |
| law6 DP（P2 J1+J2+J3 加速后） | **5.23 s**（6393→5233 ms，1.22×，realize 占 95%） | `Host/results/s4_c2/s4_p2_dp_accel_memo.md:11,39`；`README.md:113` |
| PMS/Safety | **4 ms**（3.87 ms，1000×0.01 s 内拍） | `Host/results/s4_c2/s4_p2_dp_accel_memo.md:13` |
| UART 下行（TICK_CTRL 13960 B） | **1.21 s** @115200 | `Host/results/s4_c2/s4_p2_dp_accel_memo.md:14` |
| **合计** | **≈7.5 s/拍**（1.12+5.23+0.004+1.21），10 s 拍占空 ≈75% | 本表直和；P2 备忘口径 6.7 s/拍为 SPLIT/int8 时代（推理 0.30 s），CPUF32 形态推理 +0.82 s |

执行层 1 s 门控（EXEC_REFRESH）现状：口径 v2 板跑 **270/270 帧零丢失**（`Host/results/s4_c3/s4_c3_exec1s.csv`，271 行 = 表头 + 270 数据点）；但主循环为单线程协作式轮询（`Appli/Core/Src/main.c:332-336`：`while(1){ ems_app_poll(); ems04_beat_poll(); ems_lcd_poll(); }`），DP 5.23 s 阻塞窗内的 EXEC_REFRESH 帧只能排队等待，**响应滞后 ~6–7 s**——1 s 下发的执行层刷新实际新鲜度退化为"拍间隙才处理"。grid 重算本身只要 0.04 ms（`Host/results/s4_c2/s4_p2_dp_accel_memo.md:12`；`ems04_tick_ctrl.c:406-419` 内 DWT 计时口径），瓶颈纯在调度不在计算。

### 1.3 1 Hz 账本结论（A 类可达 / 不可达边界）

| 目标 | A 类后可达性 | 依据 |
|---|---|---|
| **执行层 1 Hz 端到端刷新新鲜（真 1 Hz）** | ✅ 可达 | EXEC_REFRESH 全链 = RX 中断入环 + 快命令处理（grid 0.04 ms）+ 响应帧传输（921600 下 28 B 载荷 + 帧头 ≈0.45 ms）；RTOS 化后高优先级任务可抢占 DP 窗，端到端 **<10 ms** |
| 10 s 决策拍占空 | ~75% → **~63%**（UART 下行 1.21→0.15 s；组件直和 ≈6.5 s/拍≈65%，余量差异为上帧/调度开销口径，**精确值待板测回填**） | §2 收益表 |
| **1 Hz 完整决策（推理+DP 同拍完成）** | ❌ 位级精确红线下不可达 | P2 备忘定案：加速后板内计算（int8 时代口径）≈5.5 s > 1 s 拍周期 5.5×，"位级红线内结构性重复已穷尽"（`Host/results/s4_c2/s4_p2_dp_accel_memo.md:46-58`）；CPUF32 形态更甚（1.12+5.23+0.004≈6.35 s）。需 **B 类 fp16 推理** + **C 类 DP 近似化（降阶/近似/离线策略表——备忘 §4 已预告，属改变 DP 冻结语义的设计决策，需另行裁决）** |

---

## 2. A 类之一：UART 提速（.ioc 主导改造）

### 2.1 现状与证据（三层叠加的波特率现状）

当前 USART1 波特率由三层代码共同决定，属历史演积：

1. **CubeMX 生成层**：`.ioc` 未设 BaudRate 键（`EMS01_CHIL_ECMS.ioc:1152-1154` 仅 VirtualMode），生成 `MX_USART1_UART_Init()` 硬编码 115200（`Appli/Core/Src/main.c:455`）；时钟前提 USART1=CLKP@64 MHz（`.ioc:524-525`）。
2. **ATK 模板遗产层**：`uart_init(115200)`（`Appli/Core/Src/main.c:255`）——与 MX_USART1_UART_Init 构成**双重初始化**，板上实际以后执行的 uart_init 生效。
3. **D6 运行时覆盖层**：`#if defined(EMS_REATT_UART_BAUD) && (EMS_REATT_UART_BAUD != 115200)` 块运行时重初始化 huart1（`Appli/Core/Src/main.c:257-266`），宏经 CMake cache 接线（ems_config.h:23-32）。

两层手写（2、3）即本节要退役的对象。**用户 2026-09-30 指令：CubeMX 配置部分零手写**——波特率回归 `.ioc` 单一事实源。

⚠ 文档漂移登记：`main.c:259-260` 注释写 "ems_config.h，默认 921600"，而 `ems_config.h:30-31` 实际默认 **115200**（2026-09-22 D6 bring-up 修正后裁决值）。退役手写覆盖块时一并修正该注释。

### 2.2 收益与前提

- **收益算术**：13960 B 下行 1.21 s → **0.15 s** @921600（13960×10/921600≈0.151 s）。拍墙钟 7.5→6.5 s，占空 ~75%→~63%（§1.3）。CMD_REATT 长帧（13924 B）1.21 s/帧的回放吞吐同比例收益。
- **链路前提（硬约束，不得跳过）**：**板载 CH340 USB-UART 桥在 921600 已被两次板上实证否决**——
  ① 2026-09-22 D6 bring-up："921600 实测 CH340 不可靠：高速垃圾字节流"（`Appli/App/ems_config.h:27-29` 成文事故教训）；
  ② 复制 EMS03 当日条目：首烧 921600 版 → Flash boot 完全静默（PING 帧全坏字节被 CRC 丢弃），RCA 后回退 115200 重烧（项目开发进展.md 相应条目）。
  因此 0.15 s 收益的成立前提是**换用非 CH340 链路**（外置 USB-UART 桥 / 板上其他 UART 复用）或经板测证实某中间波特率（如 460800）在 CH340 上可靠。此项属硬件链路决策，见 §6 开放问题 O-1。

### 2.3 改造方案（.ioc 主导 + 手写覆盖退役计划）

| 步骤 | 内容 | 纪律 |
|---|---|---|
| R1 | `.ioc` USART1 增设 BaudRate（目标 921600，或链路裁决后的中间值） | `.ioc` 修改属 🟡 需用户授权（AGENTS.md §11）；方向已由用户 2026-09-30 指令给出，具体编辑仍走授权+Gate 流程；生成代码零手写（CubeMX 配置部分零手写指令） |
| R2 | 退役 D6 运行时覆盖块（`main.c:257-266`）与 `ems_config.h:30-32` 宏默认 | 删除后 `EMS_REATT_UART_BAUD` 编译开关随之退役；CMakeLists 接线同步清理 |
| R3 | 处置 ATK `uart_init(115200)`（`main.c:255`）双重初始化 | 与 R1 同批：CubeMX 主导后保留单一 `MX_USART1_UART_Init()` 路径；`uart_init` 属模板遗产，退役需确认无其他调用面后移除调用（文件保留与否按 §20 README 登记） |
| R4 | 保留 CKPER 补调 `PeriphCommonClock_Config()`（`main.c:220-228`） | Flash boot 下 CLKP 漂移教训（2026-09-22 板上实锤）——波特率翻 8 倍后时钟确定性前提更重要，此块不得随清理误删 |
| R5 | host 工具链同步 | P2 备忘 §4 已预告："需 host 工具链同步，本阶段不动（C2/C3/C4 工具链冻结于 115200）"。A 类实施时 host 侧波特率参数化与板侧同批切换，冻结基线 CSV 不重录（新口径登记） |

### 2.4 验证

- fw_gate（Debug + Release-XIP 双形态）exit 0；`.ioc` 变更后 `FW_GATE_ALLOW_IOC=1` 重跑（AGENTS.md §12）。
- 板上链路验收：PING 探活 + CMD_REATT 长帧回放零 NACK/零 CRC 错 + EXEC_REFRESH 1 Hz 连续 270 帧零丢失（口径对齐 §5.1）。
- 时延复测：13960 B 下行传输实测 ≈0.15 s（**待板测回填**，方法见 §5.1）。

---

## 3. A 类之二：RTOS 化（ThreadX）

### 3.1 选型证据链

| 证据 | 内容 |
|---|---|
| CubeMX 数据库 | 本机 STM32CubeMX 安装（`/Applications/STMicroelectronics/STM32CubeMX.app/Contents/Resources/db`，`package.xml` Release=**DB.6.0.180**）`mcu/config/` 下：`ThreadX-STM32N6xx_Configs.xml` **存在**；`FREERTOS-STM32{F0,F1,F2,F3,F4,F7,G0,G4,H7,H7RS,L0,L1,L4,L5,MP1,MP13,MP2,WB,WB0,WL,WL3,WL4}xx_Configs.xml` 共 22 个家族，**无 N6 变体** ⇒ STM32N6 无 FreeRTOS 官方支持（CubeMX 无法生成 FreeRTOS 中间件配置） |
| 固件包 | `firmware/stm32n6/04_SourceCode/SoftwarePackage/STM32Cube_FW_N6_V1.4.0/Middlewares/ST/cmsis_rtos_threadx/`（含 `cmsis_os2.c` = CMSIS-RTOS2 API 的 ThreadX 实现）**随包提供**；同包**不含 FreeRTOS** 中间件 |
| 用户裁决 | **2026-09-30 用户裁决：可以使用 ThreadX**。FreeRTOS 不支持 N6 作为结论写入本文（上述数据库证据） |

### 3.2 API 层选择：CMSIS-RTOS2（不直绑 ThreadX 原生 API）

- 应用代码经 `cmsis_os2.c`（CMSIS-RTOS2 标准 API）使用内核：任务/队列/信号量层与内核解耦，EMS 应用层不出现 `tx_*` 原生调用；
- 理由：① 与固件包内既有 ThreadX 集成方式一致（ST 官方中间件形态即 CMSIS-RTOS2 包装）；② 保留未来内核可替换性（若后续 CubeMX 为 N6 引入其他 RTOS，应用层迁移面最小）；③ CMSIS-RTOS2 语义（osDelay/osKernelLock 等）与 HAL 时基协作路径清晰；
- ThreadX 内核源与配置（`tx_user.h`、tick 频率、中断优先级掩码 BASEPRI 语义）作为工程资产纳入 `EMS_Projects/EMS04_CHIL_FULLCTRL/Middlewares/`（vendor 包保持只读，工作区例外见 AGENTS.md §11）。

### 3.2.1 ThreadX 生成链路验证结论（2026-09-30 host 实验，GO）

对工作树 .ioc 副本（/tmp）追加 7 组字段（+13 行，出处 = 固件包官方参考 `Tx_Thread_Creation.ioc` + CubeMX DB `ThreadX-STM32N6xx_Configs.xml`）后 headless 生成 **59s 一次成功（Round 1，GO）**：

- **无需 X-CUBE-AZRTOS-N6 扩展包**（DB 原生组件）；**无需迭代**；
- **HAL timebase = TIM6**（工程零 TIM 占用 + ST 官方示例同选）：生成 `stm32n6xx_hal_timebase_tim.c`；ThreadX 自身 tick 仍走 SysTick（`tx_initialize_low_level.S`），两者不冲突；
- 产物：+194 文件（内核 186 文件自动复制进工程 `Middlewares/ST/threadx/` GNU 端口，md5 与固件包一致；App 侧 8 文件：`app_threadx.c`/`app_azure_rtos.c`/`tx_user.h` 等）；main.c 的 `while(1)` 保留但不可达（`MX_ThreadX_Init()` 后 scheduler 接管），USER CODE 区保留；
- **CMake 全自动接线**（`Appli/mx-generated.cmake`：宏 2 + include 3 + 独立 `threadx` target 171 源）；真实工程重新生成即得，EMS 自有 CMake 段不受影响；
- **资源增量（两版 Appli 真实编译通过，Release）**：FLASH +5,296 B / RAM +3,952 B（2KB 静态池 + 1KB timer 栈 + 内核结构）；
- **唯一缺口：CMSIS-RTOS2 包装层**（`cmsis_os2.c` + `cmsis_os2.h`）不是 CubeMX 组件，需手动接线（建议段已在实验报告，工程集成时落盘）；
- 工程集成前置：§2 的 .ioc 回写策略落地（本轮已落地 UART 波特率与 XSPI 迁移，见 75ff918d）+ 全量重生成的 hal_conf.h 宏恢复项（§2.3）。

### 3.3 任务拆分（主循环三分结构 → 任务化）

现状单线程轮询（`main.c:332-336`）拆为三任务 + 既有中断层：

| 任务 | 优先级 | 职责 | 时延预算 |
|---|---|---|---|
| **快命令任务**（高） | 高 | EXEC_REFRESH(0x12)/PING/PROF(0x10) 等小帧命令：解析→grid 重算→回帧 | grid 0.04 ms（`ems04_tick_ctrl.c:410-419` DWT 口径）+ 帧传输 ≈0.45 ms（921600，28 B 载荷+帧头）⇒ 端到端 **<10 ms** 目标 |
| **协议任务**（普通） | 中 | 重命令同步执行：TICK_CTRL(0x0E，含 REATT 1.12 s + DP 5.23 s + PMS)、REATT(0x0A)、DPDBG(0x14) | 10 s 拍语义不变；执行期间让出 CPU |
| **LCD 任务** | 低 | 1 Hz 重绘节流（`ems_lcd.h:17` `ems_lcd_poll` 内部节流语义原样迁移） | 1 Hz |
| **中断层**（保留） | — | RX 每字节中断入环形缓冲（`ems_app.c:542-560`，**T-507 bug#4 修复先例**："主循环被 TX 阻塞不再丢字节"，`ems_app.c:574`）；RTT 拍（`ems04_beat_poll` 数据源） | 结构不动，仅把"主循环排水"改为"队列投递" |

- **命令分发按类型分流快慢队列**：分发点即 `ems_app.c` 的 `ems_dispatch` switch（REATT :407 / TICK_CTRL :416 / PROF :430 / EXEC_REFRESH :439 / DPDBG :453）——按命令码把帧载荷投入快/慢队列，重命令在协议任务内保持同步执行语义（拍内时序、DWT 分账口径不变）。
- **可抢占性收益**：DP 5.23 s 窗内 EXEC_REFRESH 不再排队，~6–7 s 滞后 → <10 ms（§1.2 结构性缺陷的定点消除）。

### 3.4 共享状态治理

| 共享量 | 现状 | 治理 |
|---|---|---|
| `m_last_d1`（f64，`ems04_tick_ctrl.c:84` 声明；`:245` 拍内写；`:406` EXEC_REFRESH 读——口径 v2 的 λ 门控输入） | 单线程下天然一致 | 跨任务读：协议任务写、快命令任务读，f64 在 M55 上非单指令原子 ⇒ **临界区（内核互斥）或 seqlock 快照**二选一；快照语义 = "读到最近完整拍值"，禁止半拍值 |
| `ems_proto_st` 分发状态 / 快慢队列 | 主循环私有 | 队列投递点加锁；`ems_dispatch` 内静态缓冲（如 `s4_rsp` :418）随任务归属划分，不跨任务共享 |
| `ems_lcd_notify_live`（EXEC_REFRESH → LCD live 通道，`ems04_tick_ctrl.c` exec refresh 内调用） | 直调 | 改为 LCD 任务消费的 notify 队列（LCD 重绘耗时不得反压快命令任务） |
| RX 环形缓冲 `ems_rx_ring` | 中断写/主循环读（head/tail volatile） | 保留结构；消费端从 `ems_app_poll` 移入协议/快命令共用的帧装配前置级（单消费者），head/tail 语义不变 |

### 3.5 迁移风险控制（红线约束下的最小侵入）

- **HAL 时基**：HAL tick 与 ThreadX tick 的归属必须单一化（RTOS 模式下 HAL 时基改由内核 tick 供给或保留独立定时器），`HAL_Delay` 在任务上下文的语义复核；
- **构建形态矩阵**：本工程多形态（`EMS_FW_MODE`/`EMS_REATT_FORM`/`EMS04_FP_PARITY`/`EMS_N1_LADDER`/`EMS04_XIP_BOOT`/S4Smoke/S4XIP）——RTOS 化作为新增编译维度时，**冻结回归基线的形态（CPUF32 板交付形态）必须逐形态回归**（清单见 §5.2）；SRAM 直载 Debug 形态与 flash boot XIP 形态的启动序列（FSBL/HyperRAM/NOR memory-mapped 顺序契约，`main.c:212-249` 注释链）在任务化后不得重排；
- **DMA 与任务并发**：LTDC/DMA2D（LCD）、NPU、XSPI 在单线程下无并发，任务化后 LCD 帧缓冲写与 NPU arena、HyperRAM 诊断读的并发面需 MPU/cache 维度复核（XSPI/HyperRAM 读路径异常已定性为板级限制——结果数据禁入 HyperRAM 的使用纪律继续有效）；
- **位级红线**：任务化不得改变任何控制路径数值语义（DP/PMS/grid 逐位输出不变）；调度只改变"何时处理"，不改变"算什么"。

---

## 4. B 类候选之一：fp16 推理路线

### 4.1 原理与预期收益

- 现状：REAT-T fp32 全 CPU（reatt8_f32）单帧推理 **1.12 s**（§1.2）；权重 4,052,912 B 入 ELF、激活 476,204 B（项目开发进展.md 十五续 P3 修复①）。
- fp16 量化（weights fp16 存储与计算）预期收益两路：
  1. **权重带宽减半**（≈4.05 MB → ≈2.03 MB，XIP NOR 取指/缓存压力同步下降）；
  2. **M55 Helium fp16 向量路径**（Cortex-M55 的 Helium MVE 支持 fp16 向量 MAC，较 fp32 标量/混合路径有吞吐上限收益）。
- **收益量级需板测确认**（Helium fp16 收益依赖算子覆盖与编译器向量化质量，不预设倍数）；推理时延目标：与 DP 5.23 s 相比属次要项，fp16 的独立价值在为"1 Hz 完整决策"凑量级（§1.3）与 ROM 减半。

### 4.2 与 int8 的边界（为什么是 fp16 而不是 int8）

- int8 PTQ 已被 RCA **定性不可用**：两独立管线（TFLite `Optimize.DEFAULT` / ORT QDQ）锚级 **−31% / −46%** 输入依赖系统性失真，低负载锚最重（`S4_PRED_PHYSICS_RCA.md` §2 证据表、"三方一致"证伪 §2.1）；reattention 架构（attention+LayerNorm+GELU）为量化脆弱架构，NPU 物理只支持定点图 ⇒ int8 路线整体关闭（含 NPU）。
- fp16 是 int8 与 fp32 之间的折中候选：fp16 动态范围（~1e-5 相对精度、指数位与 fp32 同宽）不存在 per-tensor 激活范围失配机制，理论上规避 int8 的失真病根；**但"理论上"不作数——精度结论只认锚级对拍**（§4.3）。
- int8 形态保留为回归资产（CMake `EMS_REATT_FORM` 互斥编入，int8 形态不删除）；fp16 拟以同机制新增形态（如 `EMS_REATT_FORM=CPUF16`），不触碰冻结的 CPUF32 交付形态。

### 4.3 验收门提案

| 门 | 提案阈值 | 状态 |
|---|---|---|
| 36 锚对拍 | **MAE ≤ 1.0 kW / max ≤ 5.0 kW**（对拍对象：keras fp32 冻结管线输出） | **已回填（worker-2，2026-09-29）**：36 锚 mean30 口径 MAE **0.0125 kW** / max **0.0775 kW**，corr 0.99999999；全步口径 MAE 0.0155 / max 0.3056 kW——远优于门限，verdict `PASS_CANDIDATE`（`experiments/deviation4_rca/p6_fp16_report.json`） |
| 位级回归 | CPUF32 形态 6 pilot 锚 board==keras maxdiff=0.0 基线不退化（fp16 为新形态，本门保 CPUF32 路径零回归） | 待 fp16 形态实施后回归 |
| 门禁纪律 | 阈值为**提案**，正式定版须主 Agent 依据 worker-2 数据裁决；禁凑判据（口径 v2 §4 纪律沿用） | — |

### 4.4 板测计划

1. host 侧 fp16 导出与对拍（**已完成**：`reatt_raw_fp16.tflite` 2,131,748 B = 0.514×fp32，sha256 `5c836e10…`，锚级对拍过 §4.3 提案门，`p6_fp16_report.json`）→ 产物与结论已回填本文 §4.3；
2. 达门后新增 `EMS_REATT_FORM=CPUF16` 固件形态（类比 CPUF32 引入流程：analyze/generate → host 逐位/锚级对拍 → CMake 互斥编入）；
3. 板测：6 pilot 锚 CMD_REATT 回放（对拍 keras）+ CMD_PROF 分账推理段时延（DWT 口径，对拍 1.12 s 基线）+ C3 单航次 G6 门；
4. 全过 → 登记 §18 日志与本文回填，CPUF32 交付形态不动。

### 4.5 stedgeai 工具链支持性（2026-09-29 实测，本节为工具链事实登记）

**结论：`UNSUPPORTED`**——stedgeai 4.0.1-20581 对 **fp16 权重 tflite 的 N6 全 CPU 形态（无 `--st-neural-art`）连 analyze 都无法通过**，fp16 路线的板载化在本工具链版本上不可实现（非 STORAGE_ONLY：不存在"加载时反量化"的降级路径可退）。

| 项 | fp32（reatt8_f32 先例，对照） | fp16（reatt_raw_fp16.tflite，2,131,748 B） |
|---|---|---|
| analyze | PASS：weights 4,052,912 B / 激活 375,432 B / macc 189,482,352 / ram 375,432 B | **FAIL（exit 255）**：`INTERNAL ERROR: 'FLOAT16'`（处理至 10/171 算子即崩，首个 fp16 权重 tensor 所在 conv 层） |
| generate（同参 + `--no-inputs-allocation --no-outputs-allocation`） | PASS（既有可部署形态） | **FAIL（exit 255）**：同一 `INTERNAL ERROR: 'FLOAT16'`，无任何 kernel 生成 |
| fp16/Helium kernel 证据 | — | **无**（未生成任何代码，谈不上 fp16 计算路径）；提速依据（§4.1 Helium fp16 向量 MAC + NOR 带宽减半）在本工具链上**无法兑现** |

实验纪律与佐证：

- 参数与 reatt8_f32 先例完全一致（`--target stm32n6`，无 `--st-neural-art`）；同目录同参数 fp32 analyze PASS 且数字与 `Model/reatt8_f32/reatt8_analyze_report.txt` 存档逐项一致——失败归因于模型内容而非环境/参数；
- 模型结构定性（tflite 解析）：104 个 f16 权重 tensor + 483 个 f32 激活 tensor + 131 个 i32 元数据，确认"weights fp16 存储 / 激活 fp32"的 weights-only fp16 转换如预期；
- CLI 无 fp16 开关（`--input/--output-data-type` 仅 `float32|int8|uint8`）；官方 `supported_ops_tflite.html` 支持矩阵无 float16 声明，release note 中 float16 仅涉 I/O 数据类型强制解释（且要求模型 I/O 本身为 FLOAT32），均与权重 fp16 转换无关；
- 报错原文未给出版本要求；是否测试更新版 ST Edge AI 属工具链升级决策，**上报主 Agent 裁决**（本 worker 不自行升级）。

对 fp16 路线的含义：§4.3 锚级精度门（host 侧 TFLite runtime 对拍，`PASS_CANDIDATE`）与 §4.4 板测链在第 2 步（`EMS_REATT_FORM=CPUF16` 生成）处被工具链阻断；当前 fp16 唯一已验证收益是 host 侧存储减半（0.514×fp32）。板载 fp16 的可能出路（未来 stedgeai 版本 / 手写 CMSIS-NN/Helium kernel 路线）属架构层新决策，超出本文既有方案范围，待主 Agent 裁决后另行立项。

---

## 5. 验收门汇总

### 5.1 1 Hz 端到端延迟实测方法

- **定义**：端到端延迟 = host 发出 CMD_EXEC_REFRESH 首字节 → 收到 RSP_EXEC_REFRESH 尾字节的 RTT（host 墙钟，µs 时间戳）。
- **分桶**：① DP 阻塞窗内下发（TICK_CTRL 处理中）；② 窗外。A 类前基线：零丢失（270/270）但窗内滞后 ~6–7 s；**A 类验收门：两桶 P99 均 <10 ms**（窗口期消除即任务化生效的直接证据）。
- **配套统计**：exec1s_diff 观测面（口径 v2 §4：`exec1s_n / exec1s_diff_n / exec1s_max_diff`）继续生效，任务化后数值面必须与单线程版逐位一致（同一固件数值语义）。
- **UART 提速复测**：13960 B 下行传输时间（发包→帧解析完成，板侧 RS-TRACE 或 host 侧分桶）实测 **（待板测回填）**，对拍算术值 0.151 s。
- 板窗口前提：dev boot + SRAM 直载排查形态（SWD 可用），与遗留① 取证共用板窗口（§6 O-3）。

### 5.2 全回归清单（A 类 / B 类各自全过后方可提交）

| # | 门 | 通过判据 | 现行基线 |
|---|---|---|---|
| 1 | fw_gate（`python3 firmware/gate/fw_gate.py <项目路径>`） | exit 0（`.ioc` 变更轮次 `FW_GATE_ALLOW_IOC=1`） | 7/7 PASS 惯例 |
| 2 | 固件 selftest（closedloop / C4） | 10/10、20/20 | 十五续汇总 |
| 3 | S1 C2 流 replay | 377/377 逐拍位级（max_abs_diff=0.0） | P2 冻结基线 |
| 4 | 板级 preamble 100 拍 | 100/100 逐拍逐字段一致（vs preP2/CPUF32 现行基线，按形态取对应基线） | P2 备忘 §3 |
| 5 | C3 单航次（口径 v2） | G6=True 零停机 + exec1s 观测面数值与单线程版逐位一致 | C3 v3 541/541 |
| 6 | C4 pilot 电池 | 3/3 overall=True 零回归 | 口径 v2 板跑 |
| 7 | P3 formal 电池 | 4/4 overall=True | 0c4fce1a |
| 8 | 6 pilot 锚 CMD_REATT | 板==keras maxdiff=0.0（CPUF32）/ 达 §4.3 门（fp16） | 十五续 6/6 |
| 9 | EXEC_REFRESH 端到端 | §5.1 两桶 P99 <10 ms | （A 类新增门） |
| 10 | LCD 目视 | 1 Hz 刷新、live 通道数字刷新、历史环 10 s 粒度不变 | f26e083d 修订 |

### 5.3 红线（升级不得触碰）

- **位级精确红线**：任何加速/重构（RTOS 化、fp16 之外的数值路径）输出与冻结实现位级一致；近似化（C 类）不在此红线内，但须另行裁决后才可立项；
- **FR-811/FR-873 冻结**：decision interval 10 s、DP 冻结语义、λ/SOC 网格等不因实时性升级重定（`.specs/phase4-f-practical-predictive-ems/requirements.md:111-122,343-345`）；
- **口径纪律**：v1（10s ZOH 执行层）PASS 结论保留档案化；升级后新跑按新口径登记，不重写历史结论（S4_CALIBER_V2_EXEC_GATE §5 惯例）；
- Simulink/门禁/提交纪律沿用 AGENTS.md §9/§12（Gate FAIL 不允许提交）。

---

## 6. 风险与开放问题

| # | 类型 | 内容 | 处置 |
|---|---|---|---|
| O-1 | **开放问题（A 类 UART 的最大风险）** | 921600 的 0.15 s 收益与板载 CH340 两次板上否决（`ems_config.h:27-29`；首烧 921600 静默回退）直接冲突——.ioc 主导改造本身机械可行，链路不可行则收益归零 | 需用户裁决链路方案：外置 USB-UART 桥 / 换用板上其他 UART / 板测中间波特率（460800 等）；裁决前 §2 只完成 R1-R5 的无破坏部分（治理与参数化），切换动作挂起 |
| O-2 | 开放问题（B 类之后） | 1 Hz 完整决策需 C 类 DP 近似化（降阶/离线策略表），属改变 DP 冻结语义的设计决策 | 需另行裁决，本文只登记不设计（P2 备忘 §4 同口径） |
| O-3 | 遗留①关联 | 板 DP vs host DP 155/541 拍档位分歧取证命令 **CMD_DPDBG(0x14)/RSP_DPDBG(0x15) 已实施**（`Appli/App/ems04_tick_ctrl.c/.h`（:38-41 命令码）+ `ems_app.c` 分发 :453），待板窗口执行（dev boot + SRAM 直载排查，SWD 可用） | 与 §5.1 板窗口共用；RTOS 化实施前后各跑一次 DPDBG 可顺带验证任务化对 DP 数值路径零影响 |
| R-1 | 技术风险 | ThreadX 移植：HAL 时基/内核 tick 归属、中断优先级（BASEPRI 临界区语义）与既有 RX 中断、DWT 自愈重武装（每拍入口，`ems04_tick_ctrl.c` TICK_CTRL handler）的相互作用 | §3.5 风险控制 + §5.2 全回归；RTOS 引入作为独立 commit 维度，逐形态验证 |
| R-2 | 技术风险 | `m_last_d1` f64 撕裂/半拍值（§3.4）——口径 v2 的 λ 门控输入错值将直接污染 P_gen_ref | 快照机制 + exec1s_diff 观测面逐位一致性作回归门（§5.2 #5） |
| R-3 | 技术风险 | fp16 精度：int8 教训表明"理论应无损"不成立；attention/LayerNorm 段 fp16 行为未板测 | §4.3 锚级对拍门 + §4.4 板测计划；不过门即回退 CPUF32（形态互斥，零包袱） |
| R-4 | 工程风险 | 长帧（CMD_REATT 13924 B）接收期与 DP 窗并发：RX 环 1024 B（`ems_app.c:547`）在 921600 下若 DP 窗内恰逢长帧注入，5.23 s 窗排水不及必溢出（92 KB/s×5.23 s≫1024 B）；现 host 工具串行化不触发 | host 侧保持"重命令不与拍窗并发"的既有纪律并写入 runner 契约；若未来需并发，环形缓冲扩容/流控另立设计项 |
| R-5 | 流程风险 | `.ioc` 修改（🟡）与 `uart_init`/覆盖块退役涉及生成代码重生成，可能牵动 `MX_USART1_UART_Init` 之外的外设初始化顺序 | 变更后结构 diff 检查（AGENTS.md §9 精神）+ fw_gate 双形态 + 板上三链路探活（PING/REATT 长帧/EXEC_REFRESH） |

---

## 附：证据文件索引（全部经存在性核实，2026-09-30）

- Spec：`.specs/phase4-f-practical-predictive-ems/requirements.md`、`.specs/HANDOFF-v3-closedloop.md`、`experiments/softhil_ems03_2026-09-25/S4_FULLCTRL_CONSTRUCTION_SPEC.md`、`experiments/softhil_ems03_2026-09-25/ems04_s1/LAW6_C2_CONSUMPTION_CONTRACT.md`、`experiments/softhil_ems03_2026-09-25/S4_CALIBER_V2_EXEC_GATE.md`
- 固件（本工程）：`Appli/Core/Src/main.c`、`Appli/App/ems_config.h`、`Appli/App/ems_app.c`、`Appli/App/ems04_tick_ctrl.c`、`Appli/App/ems04_tick_ctrl.h`、`Appli/App/ems_lcd.h`、`EMS01_CHIL_ECMS.ioc`
- 结果与分析：`Host/results/s4_c2/s4_p2_dp_accel_memo.md`、`Host/results/s4_c3/s4_c3_exec1s.csv`、`README.md`、`S4_PRED_PHYSICS_RCA.md`、`S4_XIP_BOOT_RCA.md`、项目开发进展.md（2026-09-29 十四续/十五续条目）
- RTOS 证据：`STM32Cube_FW_N6_V1.4.0/Middlewares/ST/cmsis_rtos_threadx/`（`cmsis_os2.c`）；CubeMX DB `/Applications/STMicroelectronics/STM32CubeMX.app/Contents/Resources/db/mcu/config/`（`ThreadX-STM32N6xx_Configs.xml` 存在；FREERTOS-* 22 家族无 N6；Release DB.6.0.180）

---

## 7. A 类实施进展登记（2026-10-01，实施会话回填）

### 7.1 §2/O-1 UART 链路裁决——已闭合

- 921600 **正式退役**：p5k 定量定案（BRR 0x45=927,536 baud，+0.645% 与 CH340@921600
  链路容差失配——交替位字节必坏、零 FE/NE/ORE、换 115200 档 3/3 位级完美 ACK；
  `experiments/deviation4_rca/p5k_uart_deadzone_rca.md`）；用户裁决 2026-10-01
  "不确认可用则使用默认 115200" → **终值 115200**（.ioc=HEAD+BaudRate 两行、
  main.c/ems_config.h 同步，已提交 e48aefad）。
- §2 的 0.15s 收益**不兑现**（伴随 921600 退役归零）；对 §5.1 的 1Hz 目标无影响：
  EXEC_REFRESH 帧极小（16B/12B），115200 下传输 ~1.4ms，瓶颈纯在调度（§3）。
- 附带证据：CH340 USB 枚举当晚掉线 3 次（串口/ST-LINK 共 5 次 USB 异常）——链路
  物理层稳定性问题另记，不阻塞 115200 结论。
- §2 的 R1-R5 状态：R1 完成（.ioc 主导 115200）；R2 完成（D6 覆盖块此前已退役）；
  R3 未动（uart_init 双重初始化仍在——后续清理项，本次零改动）；R4 完成（CKPER
  保留）；R5 完成（host 工具 --baud 参数化已入库）。

### 7.2 §3 ThreadX 任务化——集成代码完成，bring-up 阻塞一项（park）

- **已完成**（详见 `experiments/deviation4_rca/p5p_threadx_bringup.md`）：
  .ioc +18 行 ThreadX 字段组（hand-crafted，避开 6.18 重存规范化；headless 生成
  验证通过）→ 内核 173 源 + AZURE_RTOS + app_threadx + TIM6 时基入工程；
  cmsis_os2 包装层接线（§3.2 唯一缺口闭合，工程内副本 + EMS CMake 块）；
  tx_user.h 按 cmsis_os2.c 硬要求配置（64 优先级/joinable 扩展/裁剪项移除）；
  §3.3 任务拆分落地——**实现为四任务**：排水协议（Normal，永不执行命令体）/
  快命令（AboveNormal，ems04_fastq）/ 慢执行（BelowNormal，ems04_slowq 双缓冲
  2×13960B，重命令 FIFO 语义）/ LCD（Low）——较 PLAN 三任务的修正：重命令不得
  在排水任务内联（否则 DP 窗内 RX 环停摆，快帧无法入快队列，PLAN 原文的结构性
  缺口在实现评审中补上）；§3.4 治理落地（m_last_d1 seqlock、UART 发送 osMutex、
  快/慢队列）；链接脚本 `__RAM_segment_used_end__` PROVIDE（两脚本，零布局变化）。
  构建通过：S4Smoke SRAM 载体 + Debug（fw_gate 全 PASS）。
- **bring-up 阻塞（park，完整现场 p5p）**：RTOS 构建 ~339ms 确定性 HardFault
  @0xC——TIM6 IRQ 进入 HAL_TIM_IRQHandler 时 htim6.Instance==NULL；uwTick 冻结
  于 339；四线程未创建。已证伪：TIM6 遗留 pending 竞态（加固后复现）、时基回调
  缺失（已补，曾独立致死锁）、TIM 模块宏缺失（已补）。下会话切入路径三选一
  已写入 p5p §2（数据观察点抓 Instance 清零者 / TIMG 频率口径（Prescaler=0x800
  异常值）/ 改用 txgen 全量 main.c 重试）。
- 遗留①判决（§6/O-3）：pre-RTOS DPDBG 位级基线已录（−0.055，0xBFAC28F5C28F5C2A，
  flash CPUF32）；post-RTOS 对比待 bring-up。

### 7.3 §5.2 十门状态（本会话时点）

| # | 门 | 状态 |
|---|---|---|
| 1 | fw_gate | PASS（EMS04 全门，ALLOW_IOC=1 轮次已录） |
| 2-8 | selftest/replay/preamble/C3/C4/P3/6锚 | 未动（依赖 bring-up 后统一回归） |
| 9 | EXEC_REFRESH 双桶 P99<10ms | **未达**（RTOS bring-up park；基线脚本 `p5o_exec_p99.py` 就绪；pre-RTOS 窗内滞后 ~6-7s 为 PLAN §1.2 既有口径） |
| 10 | LCD 目视 | 未动（板在 RTOS park 态） |

**结案行不登记**：真 1Hz 两桶 P99 未实测达标，A 类未收口；D 类封存维持用户既定
裁决（与 A 类收口与否解耦）。

### 7.4 遗留①——已 CLOSED（本会话先行完成）

TICK_CTRL 线格式错位（off-by-4B）定案+修复（RCA §A.8，commits ad9ebc8f/03502b08）：
板上 DP/预测数学自始位级精确；用户裁决"修复先行"，ThreadX 回归基线 = 修复后口径。

### 7.5 bring-up 收口（2026-10-01 续，p5r）——三根因闭环，位级红线 post-RTOS PASS

**§7.2 的 bring-up 阻塞解除**。三轮 SWD 直证定位三个独立根因，全部修复并板验
（完整证据 `experiments/deviation4_rca/p5r_rtos_bringup_closure.md`）：

1. **"htim6.Instance 清零者"（p5p/p5q 双会话悬案）= `s_thr_attr[3]` 数组越界**：
   声明 3 元素（三任务草稿残留）而创建循环跑 4 次——i=3 的 36B 结构体赋值
   落在 s_tid 四连 + htim6 头 20B 上（板锤：0x341a5d70=0x800=stacks[3]、
   +8=8=osPriorityLow 逐字段吻合）。uwTick 冻结 ~339/340 = main() 走到
   MX_ThreadX_Init 的时刻。p5q 候选列表（内核段/LCD memset/队列 memset）作废，
   DWT watchpoint 手段无需执行。修复 = `[3]→[4]` 一处。
2. **newlib 堆与 ThreadX 池同基址双重分配**：`_sbrk`（sysmem.c，CubeMX 生成）
   与 `_tx_initialize_unused_memory` 都从 `&_end` 起长——DP 路径 calloc 的
   memset 把在线线程栈清零 → PC=0 IBUSERR（堆叠帧 LR 在 _calloc_r 内）。
   pre-RTOS 单主人无恙，post-RTOS 首次深路径 calloc 引爆。修复 = 堆基址
   上移池跨度+4KB（宏与 cmsis_os2.c 同源自洽）。⚠️ sysmem.c 无 USER CODE
   区，**重生成 .ioc 会丢此修复**（.ioc 已冻结）。
3. **Prot 任务忙旋饿死低优先级**：`osThreadYield()` 只轮转同优先级，Prot
   (Normal) 永不阻塞 ⇒ Slow/LCD 永无 CPU（板锤：DPDBG 入 slowq 后无人消费）。
   修复 = `osDelay(1)`（排水节拍 1ms，115200 下 11.5B/ms 远低于排水能力，
   快命令附加时延 ≤1ms 在门 #9 预算内）。

**bring-up 验证（终态 ELF 73cccc4b）**：KernelState=1(Ready)、
_tx_thread_created_count=5、s_tid 四连全非零、htim6.Instance=0x50001000、
uwTick 持续走。

**位级红线（§5.3）post-RTOS 判定 PASS**（p5a['910'] 同输入双通道）：
- RTT beat（通道一）：d1 = **0xBFAC28F5C28F5C2A（−0.055）** 与 p5g pre-RTOS
  基线逐位一致，flags=DP_OK；
- DPDBG SWD 环注入（通道二）：d1 同上，且 rc=0、J_window/soc_end/fuel/
  dlambda_all 全字段与 p5g 断点版 dump 一致（RCA §A.9 已追加）。
- 注入新手法登记：UART RX 环注入前 DCIMVAC(0xE000EF60) 按行失效 D-cache
  （UART 环无 RTT 的 cache_ci 自愈）。

**§5.2 门 #9：仍未达（park）**——CH340 本晚第 4 次掉线（USB 树 0x1a86 整体
消失），µs 级 host 墙钟不可测；SWD 替代精度不可用。复跑条件与命令见 p5r §6。
门 2-8/10 回归同样待板窗口。**结案行继续不登记**（真 1Hz 未实测）。

ELF 血统与 sha 勘误（p5r §0）：ac4a35af 消息中"新 ELF sha 2d10d5c2"不存在于
任何口径/目录——实际新 ELF = c6274ffe（p5q_final_note 标注正确）。

fw_gate（三处修改后）：7/7 全 PASS（Debug+Release）。

### 7.6 门 #9 首测（2026-10-01 夜，p5s）——P50 达标，P99 差一步：tick 100Hz→1000Hz + TX 边界碰撞登记

**测量通路**：CH340 串口恢复后走正规 UART（p5o 双线程版有 pyserial 并发读竞态，
换单线程 `p5s_exec_p99_v2.py`：TICK_CTRL 发出不等待，1Hz EXEC_REFRESH 贯穿 DP 窗，
单读取面按帧类型分类计时）。

**第四个 bring-up 根因（门 #9 首测 P50≈15.6ms 直接顶穿 10ms 门）**：
`TX_TIMER_TICKS_PER_SECOND=100`（tx_port.h 缺省）——`osDelay(1)`=10ms，Prot 排水
节拍变成 10ms。修复 = tx_user.h 254 行 100→1000（tx_port.h 无此宏、tx_api.h 为
#ifndef 缺省；**tx_user.h 曾在 71 行附近加 1000 但被 254 行 ST 模板自带的 100
二次定义覆盖——重定义警告是线索**）。关键配套：`TX_INCLUDE_USER_DEFINE_FILE`
必须同时给 threadx 内核库（CMakeLists 新增 `target_compile_definitions(threadx
PUBLIC ...)`）——否则 tx_user.h 对内核编译单元不可见，改了也白改（板锤：两次
重建 sha 不变）。修复后反汇编实证 `_tx_initialize_low_level` 装载值
`mov.w r1, #1000` ✓。ELF 19d5e2c6，fw_gate 7/7 PASS。

**门 #9 实测（1000Hz，p5s v2，REATT 推理 303ms 数值与 RTT/旧版逐位同）**：
- 窗外桶：P50 7.57-7.97 / P99 8.14-8.99ms（两轮，n=12/12）✓
- **DP 窗内桶：P50 7.48-7.57ms ✓**（结构性目标达成：快命令在 5.2s DP 窗内
  可抢占，任务化设计意图实证）；
- 窗内 P99 = 13.68 / 55.18ms ✗（两轮各 1 个离群，其余 39-40 样本 7.5-9.7ms）。
  离群定位 = **DP 完成瞬间**：慢任务 RSP_TICK_CTRL 96B 阻塞 TX（115200 下
  ~8.3ms）持 osMutex，与快任务 EXEC 应答撞车。按真实 1Hz 节拍折算，此类边界
  碰撞 ≈1.3% 样本 >1%，严格 P99 仍命中 → **门 #9 严格判定：未达**。
- 修复路径（下会话，范围有界）：应答发送中断化/DMA（TX 不再持锁阻塞），
  位级无涉、仅传输层。
- 教训：p5o 双线程读同一 pyserial 句柄有竞态（今晚两次 tick 线程崩溃的根因）；
  测量脚本必须单读取面。

**附带**：CH340 串口即板子供电——今晚 5 次掉线=5 次板子断电（SRAM 固件丢失、
USB 树抖动），是全部"诡异现象"的总根因；建议独立供电。UART REATT 全链
（13931B 帧→推理 303ms→q50 202-227kW→LCD 曲线点亮）在 RTOS 形态首次走通。

**§7.6 勘误与深化（同晚，C3 启动前）**：上段"修复路径=应答 TX 中断化/DMA"为
**错误分析，作废**。96B 应答占用的是**串口线本身**（96B×10bit÷115200=8.33ms
线时间），DMA/中断只省 CPU 占用不省线时间——执行层应答在物理上必须排在
96B 之后，影子是**单串口 115200 的线占用下限**，软件不可消除。正确表述：
- 影子事件率 ≈0.9%/样本（EXEC 恰落在 DP 应答发送窗内才中招），P99（1% 分位）
  处于刃缘——短样本测量（n=40，影子权重 2.5%）必然命中，长程统计（C3 全程
  5410 样本，权重 ≈0.9%）在刃缘两侧取决于精确率；
- 判据裁决项（属 Gate Threshold/判据解释，🟡，需用户裁决）：
  (a) 维持 P99<10ms 严格判据 → 以 C3 全程统计定案（刃缘，可能过可能不过）；
  (b) 判据修订为"DP 计算窗内 P99<10ms 且零丢失"（影子=线占用物理事件，单列
      登记不进门）→ 已达标；
  (c) 硬件路径：执行层应答走第二串口，或重开 115200 波特率裁决（O-1 已闭）。
- C3 全程（541 拍，--exec-period 1，5410 个 EXEC 样本）已启动，兼作门 #5 与
  长程 P99 数据源。
