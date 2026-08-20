# STM32N6_ARCHITECTURE.md

本文件定义 STM32N647 固件、FreeRTOS、边缘 AI、通信、调试和硬件操作规则。

适用于：

```text
firmware/stm32n6/
```

同时必须遵守 Vendor 子树中的：

```text
firmware/stm32n6/04_SourceCode/SoftwarePackage/AGENTS.md
```

如果子树规则与本文件存在不同限制，以更严格的安全约束为准。

---

## 1. STM32N6 在系统中的角色

STM32N6 当前主要承担：

```text
Data Acquisition
        ↓
Preprocessing
        ↓
AI Inference
        ↓
EMS-related Processing
        ↓
CAN / Other Output
```

具体实现会随着项目推进调整。

Firmware 是整个 EMS 系统的执行层之一。

不得为了 MCU 本地实现方便而擅自重新定义：

- EMS 控制含义；
- AI 输入输出语义；
- CAN 信号含义；
- 系统时间尺度。

---

## 2. Vendor Package

以下目录：

```text
firmware/stm32n6/04_SourceCode/SoftwarePackage/
```

视为 Vendor Package。

原则：

```text
READ-ONLY
```

除非满足：

```text
明确的修复性 Patch
+
充分技术理由
+
用户明确批准
```

否则不得修改 Vendor 原文件。

优先通过：

- USER CODE 区域；
- Wrapper；
- 独立 Application；
- 项目自有扩展代码；

实现功能。

---

## 3. FreeRTOS 架构

以下属于系统级设计，由主 Agent决定：

- Task 划分；
- Task Priority；
- Task Period；
- ISR / Task Boundary；
- Queue；
- Semaphore；
- Task Notification；
- DMA；
- Buffer；
- Producer / Consumer；
- Timeout；
- Fault Handling；
- Watchdog Strategy。

Subagent 可以按照已经确定的架构实现代码。

不得为了方便编码自行：

- 合并 Task；
- 拆分 Task；
- 新增高优先级 Task；
- 改变 Task Period；
- 改变 Task Priority；
- 把耗时操作塞入 ISR；
- 使用 Busy Wait 替代既有同步机制。

这些行为如果确有必要，必须 Escalate。

---

## 4. 数据流

数据流应尽可能保持明确、可追踪。

推荐抽象：

```text
Peripheral / DMA
        ↓
Acquisition
        ↓
Buffer
        ↓
Preprocessing
        ↓
Inference
        ↓
EMS Interface
        ↓
CAN / Output
```

共享数据必须明确：

- Owner；
- Producer；
- Consumer；
- Lifetime；
- Update Rate；
- Synchronization。

避免多个 Task 无约束读写同一 Buffer。

---

## 5. ISR

ISR 原则：

```text
Short
Deterministic
Non-Blocking
```

ISR 中避免：

- AI Inference；
- 大量 `memcpy`;
- `printf`;
- 长循环；
- 阻塞式 RTOS API；
- 大量浮点运算；
- 复杂控制逻辑。

ISR 应尽可能：

```text
采集状态
→ 清中断
→ 通知 Task
→ 快速退出
```

---

## 6. DMA

DMA 设计必须明确：

- Buffer；
- Buffer Size；
- Alignment；
- Ownership；
- Completion Event；
- Error Handling；
- Cache Interaction。

不得使用“看起来能工作”的随机 Buffer 地址。

---

## 7. Cache Coherency

STM32N6 使用 DMA、NPU 或共享内存时，必须考虑 Cache Coherency。

涉及：

- Cache Clean；
- Cache Invalidate；
- Alignment；
- MPU Attribute；
- DMA-visible Memory；
- NPU-visible Memory；

时不得猜测。

如果现有架构没有明确说明：

```text
Escalate
```

不得通过随机增加：

```text
SCB_CleanDCache...
SCB_InvalidateDCache...
```

调用“试试看”。

Cache 操作错误可能产生难以复现的数据问题。

---

## 8. Memory Architecture

以下属于系统设计问题：

- Internal SRAM；
- External RAM；
- External NOR；
- AXI Memory；
- NPU Buffer；
- Tensor Arena；
- Linker Section；
- Task Stack；
- Heap；
- Shared DMA Buffer。

Subagent 可以：

- 查看 `.map`;
- 查看 ELF Section；
- 统计 Memory Usage；
- 调查 Overflow；
- 调查 Section Placement。

不得自行重新安排关键 Memory Section。

---

## 9. Linker / Boot Address

以下属于高风险配置：

```text
Linker Memory Map
Vector Table
Boot Address
External Flash Mapping
FSBL
Secure / Non-Secure Layout
```

当前项目的重要地址契约包括：

```text
0x34000400
0x70100400
```

具体语义以对应工程、Linker Script 和 Firmware Gate 为准。

不得：

- 猜测烧录地址；
- 为了通过 Linker 随意把 Section 放入 RAM；
- 为了成功启动而随意改变 Vector Table；
- 删除 FSBL 相关内容；
- 修改地址契约而不更新验证；
- 通过临时地址规避真实 Linker 问题。

涉及地址变化必须 Escalate。

---

## 10. AI Inference

AI 模型部署必须明确：

- Model Version；
- Input Shape；
- Input Datatype；
- Output Shape；
- Output Datatype；
- Scaling；
- Quantization；
- Tensor Memory；
- Preprocessing；
- Postprocessing；
- Inference Period。

MCU 与 Python 必须保持一致：

```text
Feature Order
Normalization
Scaling
Datatype
Window Order
Tensor Shape
```

Subagent 不应只验证：

```text
模型能够运行
```

还必须检查：

```text
模型运行的输入语义是否正确
```

---

## 11. UART

UART 设计应明确：

- Baud Rate；
- Frame Format；
- DMA / Interrupt / Polling；
- Buffer；
- Timeout；
- Parsing；
- Error Handling。

Subagent 可以实现已经定义的 UART 接口。

不得自行改变协议语义。

---

## 12. CAN

通信代码必须区分：

```text
Transport
Protocol
Application Semantics
```

Subagent 可以实现已经确定的 Transport 和 Protocol。

不得自行改变：

- CAN ID；
- DLC；
- Byte Order；
- Scaling；
- Offset；
- Unit；
- Message Meaning；
- Timeout Policy；
- Signal Validity。

如果发现协议定义不完整，应返回主 Agent。

---

## 13. Fault Handling

Firmware 不得假设：

- UART 永不失败；
- CAN 永不丢包；
- AI 永远返回有效值；
- Sensor 永远正常；
- DMA 永远完成；
- Queue 永远不会满。

至少需要考虑：

- Timeout；
- Invalid Data；
- NaN / Inf；
- Communication Loss；
- AI Failure；
- Buffer Overflow；
- Task Stall。

具体 Fail-Safe 策略属于系统设计，由主 Agent决定。

---

## 14. Watchdog

如果使用 Watchdog：

不得简单在任意主循环中无条件 Feed。

Watchdog 应与：

```text
系统健康状态
```

建立关系。

具体设计可能包括：

- 关键 Task Heartbeat；
- Task Deadline；
- Communication Health；
- Inference Health；
- Control Health。

具体策略由主 Agent决定。

---

## 15. 固件构建

固件构建必须使用：

```text
prepare-stm32n647-project
```

Skill。

Build 阶段：

- 离线；
- 不操作开发板；
- 不自动烧录；
- 不修改永久硬件配置。

Build PASS 不意味着获得上板权限。

---

## 16. Firmware Gate

固件功能完成后、提交前必须执行：

```bash
python3 firmware/gate/fw_gate.py <项目路径>
```

Gate 检查包括：

- Debug Build；
- Release Build；
- ELF Section Layout；
- Address Contract；
- `.ioc` 未授权修改；
- 违规副本；
- Warning Count；
- 其他 Firmware Contract。

结果：

```text
firmware/gate/fw_gate_result.json
```

属于运行时产物，不入库。

只有：

```text
exit code = 0
```

才允许提交。

---

## 17. `.ioc`

修改 `.ioc` 必须获得用户明确授权。

获得授权后：

```bash
FW_GATE_ALLOW_IOC=1
```

重新执行 Firmware Gate。

不得：

- 删除 Gate；
- 绕过 Gate；
- 通过修改生成文件隐藏 `.ioc` 变化；
- 自行认为 `.ioc` 修改“应该没问题”。

---

## 18. SRAM Debug

调试使用：

```text
debug-stm32n647-sram
```

Skill。

目标：

```text
Temporary SRAM Debug
```

该阶段原则上：

- 不进行永久 Flash 修改；
- 不擦写 External NOR；
- 不自动升级为 Release Programming。

不得使用通用 Flash MCP 工具替代 SRAM Debug 工作流。

---

## 19. Release Programming

Release 烧录使用：

```text
release-stm32n647-appli
```

Skill。

必须：

- 保留既有 FSBL；
- 禁止 Mass Erase；
- 核对镜像；
- 核对地址；
- 核对目标 Device；
- 遵循专用 Skill；
- 保持已有 Boot Chain。

不得因为一次烧录失败就尝试随机地址。

---

## 20. 硬件风险等级

### Level A — Read Only / Non-Destructive

在已授权的调试任务中可以执行：

- 查询 ST-LINK；
- 查询 Device ID；
- 查询 Firmware Version；
- 查询连接状态；
- 读取串口；
- 读取 Debug Log；
- 检查 ELF；
- 检查 HEX；
- 检查 BIN；
- 查看 Symbol；
- 查看 Section；
- 查看 Address；
- 执行非破坏性诊断命令。

### Level B — Temporary Control

包括：

- Debugger Attach；
- Halt；
- Resume；
- Reset；
- Breakpoint；
- SRAM Load。

必须：

- 与当前调试任务相关；
- 遵循专用 Skill；
- 不修改永久配置。

### Level C — Persistent / Potentially Destructive

包括但不限于：

- Internal Flash Programming；
- External NOR Programming；
- Flash Erase；
- Option Bytes；
- Boot Configuration；
- Security Configuration；
- Permanent Protection；
- 其他可能造成设备无法正常启动的操作。

Level C 操作前必须：

```text
调查
    ↓
主 Agent 核对
    ↓
确认镜像
确认地址
确认目标 Device
确认技术理由
确认专用 Skill
    ↓
需要时取得用户明确授权
    ↓
执行
```

Subagent 不得自行尝试候选配置。

---

## 21. 严格禁止

禁止：

```text
Mass Erase
```

禁止 Subagent：

- 猜测烧录地址；
- 试多个 Option Byte；
- 试多个 Boot Config；
- 试多个 Secure Setting；
- 擦除未知范围 External NOR；
- 使用通用工具绕过专用 Skill；
- 把 Firmware Gate FAIL 当成无关警告。

---

## 22. 阶段隔离

必须保持：

```text
Implement
    ↓
Build
    ↓
Firmware Gate
    ↓
Main Agent Review
    ↓
SRAM Debug
    ↓
Hardware Validation
    ↓
Release Programming
```

以下推理均无效：

```text
Build PASS
→ 自动允许操作开发板
```

```text
Firmware Gate PASS
→ 自动允许烧录
```

```text
SRAM Debug PASS
→ 自动允许 External Flash Programming
```

```text
Flash 成功
→ 功能一定正确
```

每个阶段具有独立目的和权限。

---

## 23. Subagent 可以执行的 Firmware 工作

在架构已经确定后，Subagent 可以：

- 编写 FreeRTOS Task；
- 编写 Driver Glue Code；
- 实现 Queue；
- 实现 Task Notification；
- 实现 Buffer；
- 编写 UART；
- 编写 CAN；
- 编写 AI Inference Wrapper；
- 修改普通业务逻辑；
- 修改已批准范围内的 CMake；
- 运行 Build；
- 分析 Compiler Log；
- 分析 Linker Log；
- 修复普通 Warning/Error；
- 执行 Firmware Gate；
- 整理验证结果。

---

## 24. 必须 Escalate 的 Firmware 问题

遇到以下情况必须返回主 Agent：

- 必须改变 FreeRTOS Task 架构；
- 必须改变 Task Priority；
- 必须改变 Task Period；
- 必须改变 ISR / Task Boundary；
- 必须修改 DMA Architecture；
- 必须修改 Cache；
- 必须修改 MPU；
- 必须修改 Linker Memory Map；
- 必须修改 Boot Address；
- 必须修改 TrustZone；
- 必须修改 Option Bytes；
- 必须修改 Clock Tree；
- 必须改变 CAN Protocol；
- 必须改变 AI 数据语义；
- 必须执行未授权 Flash / Erase；
- Firmware Gate 表明根因属于系统设计；
- 连续普通修复无法解决问题。

原则：

```text
不要为了让程序启动
而绕过系统级问题
```
