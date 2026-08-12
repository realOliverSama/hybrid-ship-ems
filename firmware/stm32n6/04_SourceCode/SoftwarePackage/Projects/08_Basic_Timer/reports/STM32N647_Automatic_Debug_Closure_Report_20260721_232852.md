# STM32N647 第二层验收：Automatic Debug Closure 报告（D-00/D-10 准备阶段）

- **报告生成时间**：2026-07-21 23:28:52（本地）
- **执行范围**：D-00（第一层结果与环境预检）、D-10（基线与隔离测试资产）
- **当前状态**：**暂停于"一次性授权门"**；D-20 及以后未执行；未复位、未加载任何测试固件、未擦写 Flash
- **执行器**：Pi（embedded-debugger MCP，probe-rs 后端）

---

## 1. D-00：第一层结果与环境预检

### 1.1 第一层结论继承

| 项目 | 结论 | 证据来源 |
|---|---|---|
| Safe Debug Regression（T-00 ~ T-15） | **PASS** | 本会话内的原始工具输出（connect/get_status/halt/run/read_memory/step/disconnect 原文逐条记录）；T-13 修复后硬件回归 PASS（`0xFFFFFFFE` 在写硬件前被拒绝，MCP error -32602）；T-15 人工 LED 观察由用户确认"两盏 LED 均正常闪烁（400ms/1s）" |

注：第一层未生成独立报告文件，原始证据为本会话工具输出记录。本报告引用的关键原始输出均可复核。

### 1.2 工具能力枚举（重连后实测 24 个工具）

| 能力 | 工具 | 状态 |
|---|---|---|
| 会话生命周期 | `connect` / `disconnect` / `probe_info` / `list_probes` | ✅ 可用 |
| 执行控制 | `halt` / `run` / `step` / `reset` / `get_status` | ✅ 可用 |
| 断点 | `set_breakpoint`（hardware，含新地址校验）/ `clear_breakpoint` | ✅ 可用 |
| 内存 | `read_memory`（size 字段）/ `write_memory` | ✅ 可用 |
| 故障诊断 | `diagnose_fault`（CFSR/HFSR/MMFAR/BFAR/SHCSR/CPUID/ICSR + PC/SP/LR） | ✅ 可用 |
| 异常展开 | `unwind_exception`（栈帧映射） | ✅ 可用 |
| 通用核心寄存器读取 | 无 | ⚠️ **已知限制**（CFSR 等由 diagnose_fault 覆盖；R0-R12/xPSR/MSP/PSP/CONTROL/PRIMASK/BASEPRI/FAULTMASK 不可读） |
| **ELF/SRAM 加载** | **无** | ❌ **缺失，见第 5 节** |
| Flash 烧录 | `flash_erase` / `flash_program` / `flash_verify` / `run_firmware` | 存在但**本层禁用** |

### 1.3 探针在线检查

```
Found 1 debug probe(s):
1. STLink V2  VID:PID = 0483:3748  Serial: 30  Probe Type: "ST-LINK"
```

---

## 2. 工程发现清单（D-10 前置）

| 项目 | 值 |
|---|---|
| 工程根目录 | `/Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/08_Basic_Timer` |
| 版本控制 | **非 git 工程**（`git rev-parse` 失败）→ 无法给出 commit/dirty 状态；以文件 mtime + 备份文件佐证修改历史 |
| `.ioc` | `08_Basic_Timer.ioc`（44.0K） |
| 顶层 CMake | `CMakeLists.txt`（ExternalProject 包装）→ 实际工程在 `Appli/` |
| CMake presets | `CMakePresets.json`（Debug/Release，Ninja）；`Appli/CMakePresets.json`（binaryDir `Appli/build/<preset>`） |
| toolchain 文件 | `gcc-arm-none-eabi.cmake`（arm-none-eabi- 前缀，由 PATH 决定具体 GCC） |
| linker script（Debug=本层使用） | `Appli/STM32N647XX_LRUN_RAMxspi1.ld`：RAM(xrw) ORIGIN=**0x34000400** LENGTH=2047K，EXTRAM(rw) 0x90000000/32M |
| linker script（Release，本层不用） | `STM32N647XX_ROMxspi2_RAMxspi1.ld`（XSPI2 NOR Flash） |
| startup | `Appli/Core/Startup/startup_stm32n647xx.s` |
| 芯片 | STM32N647（启动文件 stm32n647xx；CubeMX 工程 STM32N647X0H 系列） |
| 程序位置 | **内部 SRAM**，LOAD vaddr=0x34000000（段内文件偏移 0 起含 ELF 头；向量表在 0x34000400） |
| 构建依赖（工程内） | `Drivers/`（CMSIS + HAL，10M） |
| 构建依赖（工程外） | `SoftwarePackage/Drivers/BSP/{BTIM,HyperRAM,LED,SYS}`，经 `Appli/CMakeLists.txt` 的 `ATK STM32N6 AUTO BSP` 块以 `../../../Drivers/BSP` 相对路径引用 → **同级副本自动继承该路径** |
| 已有定制痕迹 | `Appli/CMakeLists.txt` 含 `ATK STM32N6 AUTO LINKER/BSP` 块（相对 `CMakeLists.txt.atk-backup`：+105/-47）；存在多个 `.atk-backup` 文件 |
| 原工程 LED 契约 | LED1：主循环 `HAL_Delay(200)` 翻转 → 400ms 周期；LED0：TIM6 中断回调翻转 → 1s 周期 |

### 2.1 工具链（实测路径与版本）

| 工具 | 路径 | 版本 |
|---|---|---|
| arm-none-eabi-gcc（**构建基线使用，ST 捆绑**） | `~/Library/Application Support/stm32cube/bundles/gnu-tools-for-stm32/14.3.1+st.2/bin/arm-none-eabi-gcc` | GNU Tools for STM32 **14.3.rel1**.20251027-0700（14.3.1） |
| arm-none-eabi-gcc（homebrew，仅用于对照实验） | `/opt/homebrew/bin/arm-none-eabi-gcc` | Arm GNU Toolchain 14.2.Rel1（14.2.1） |
| nm/objdump/readelf/addr2line | `/opt/homebrew/bin/arm-none-eabi-*` | 14.2.Rel1（只读分析，与编译器版本无关） |
| cmake | CubeIDE 插件 `.../externaltools.cmake.macos64_1.1.0.202409170845/tools/bin/cmake` | 3.28.1 |
| ninja | CubeIDE 插件 `.../externaltools.ninja.macos64_1.1.0.202511101424/tools/bin/ninja` | 1.11.1 |

---

## 3. D-10：基线建立

### 3.1 关键发现：工具链敏感性（已解决）

- 首次干净构建使用 homebrew GCC 14.2.1 → 产物与运行中固件在 0x34006340 处**不匹配**。
- 对比 `compile_commands.json` 发现运行中固件由 **ST 捆绑 GCC 14.3.1** 编译。
- 换用 ST 工具链重建后：与运行中固件的 LOAD 段（filesz 均为 0x85cc）**仅相差 2 字节**，位于 ELF 文件头 `e_shoff` 字段（文件偏移 0x20，段头表偏移量 0x2C30D0 vs 0x2C3118）——**纯 ELF 元数据，非可执行内容**；全部可执行字节逐一相同。
- 结论：当前源码树 + ST 14.3.1 工具链可复现运行中固件（除 ELF 头元数据）。

### 3.2 基线 ELF（本层验收基准）

| 项目 | 值 |
|---|---|
| 路径 | `/Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/08_Basic_Timer/Appli/build-l2acceptance-st/08_Basic_Timer_Appli.elf` |
| SHA-256 | **`38ea1f13b5831df6288b03641dea1f07a3f7ec64645b1c4ae0885f84fcee69cf`** |
| 大小 | 2.8M（含 debug info，未 strip） |
| 入口 | 0x340080d1（Thumb；实际 0x340080d0） |
| LOAD 段 | off=0x0, vaddr=paddr=0x34000000, filesz=0x85cc, memsz=0x90c8, RWE, align 0x1000 |
| 构建命令 | `cmake -S Appli -B Appli/build-l2acceptance-st -G Ninja -DCMAKE_TOOLCHAIN_FILE=../gcc-arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=Debug` + `cmake --build`（PATH 前置 ST 14.3.1 工具链）；configure/build 退出码均 0 |
| 构建日志 | `reports/baseline_st_configure_20260721_232852.log`、`reports/baseline_st_build_20260721_232852.log`（有 1 条链接器 warning：LOAD segment RWX，源自 LRUN 链接脚本 RAM(xrw) 属性，属工程既有行为） |

### 3.3 参照：运行中固件对应的历史 ELF

| 路径 | SHA-256 | mtime |
|---|---|---|
| `Appli/build/Debug/08_Basic_Timer_Appli.elf` | `ddf589a855878f60451647a549b972e68a24c131eb8245c5e48f2995e208541b` | Jul 20 21:28:32 |
| `Appli/build/08_Basic_Timer_Appli.elf` | `2b2bbccbc2e96bbde2e02346c18fb0ee0958fb7abed38d1a5b07e594fa3f54ae` | Jul 20 21:19:20 |

两者在 0x34006340 处均与硬件回读的 24 字节（`80 B4 00 AF … EC 85 00 34`）一致。

### 3.4 基线运行时证据（第一层已有）

- 目标 Running，LED1≈400ms / LED0≈1s（用户确认）；
- 代码区 0x34006340 起 24 字节三次读取逐字节一致；
- 单步现场 PC=0x3400634A/0x34006350，SP=0x341FFFCC/0x341FFFD0。

---

## 4. D-10：隔离副本与故障夹具

### 4.1 隔离副本（原工程未动任何源文件）

- 路径：`/Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/08_Basic_Timer_L2Test_20260721_232852`（11M，rsync 排除全部 build 目录）
- BSP 相对路径 `../../../Drivers/BSP` 在同级副本中解析到同一共享目录，无需复制。
- 副本内新增/修改（均在副本内）：
  - 新增 `Appli/Core/Inc/fault_fixture.h`、`Appli/Core/Src/fault_fixture.c`；
  - `Appli/Core/Src/main.c` 三处最小钩子：include 夹具头文件；`btim` 初始化后调用 `fault_fixture_pre_loop_hook()`；主循环中 `LED1_TOGGLE()` 后调用 `fault_fixture_loop_hook()`，`HAL_Delay(200)` 改为 `HAL_Delay((uint32_t)led1_toggle_half_period_ms)`；
  - `Appli/CMakeLists.txt` 增加 `FAULT_VARIANT` cache 变量（空=行为中性）与夹具源文件。

### 4.2 故障夹具设计（单宏选择）

| 变体宏 | 机制 | 触发符号（noinline，可定位） | 预期表现 |
|---|---|---|---|
| `FAULT_HARDFAULT` | `fault_trigger_hardfault()` 内 `.inst 0xDE00`（UDF #0，永久未定义指令 → UNDEFINSTR UsageFault → 默认未使能 → 升级为 HardFault） | `fault_trigger_hardfault` @ 0x340080ee（UDF 在 +0x4，已 objdump 验证编码 `de00`） | 主循环首次执行即 HardFault |
| `FAULT_LED_PERIOD` | `led1_toggle_half_period_ms`（volatile int32，ELF 可定位）初值 50（100ms 全周期） vs 契约 200（400ms） | `led1_toggle_half_period_ms` @ 0x340085ec，`.data` 初值 `32 00 00 00`（已验证） | LED1 周期 100ms ≠ 400ms 契约 |
| `FAULT_STARTUP_STALL` | 进入主循环前调用 `fault_startup_stall()`：写 `startup_stall_marker=0x57A11ED1` 后死循环 nop；不触碰调试口/时钟/安全配置 | `fault_startup_stall` @ 0x340080f0、`startup_stall_marker` @ 0x34008708（已 objdump 验证存 marker + `b.n` 自旋） | LED 不工作，halt 后 PC 落在命名停滞函数 |

### 4.3 四个测试 ELF（全部 Debug/LRUN SRAM 配置，ST 14.3.1 构建，退出码均 0）

| ELF | 绝对路径（副本下 `Appli/build-<name>/08_Basic_Timer_Appli.elf`） | SHA-256 | 入口 | LOAD 段 |
|---|---|---|---|---|
| fixture-baseline（无故障对照=修复态） | `…/Appli/build-fixture-baseline/08_Basic_Timer_Appli.elf` | `670190e0fdfc79e329a25135941dfa62f7e4f43f2c10015996aa83665e4cfc2d` | 0x34008091 | vaddr 0x34000000, filesz 0x85f0, memsz 0x90e8, RWE |
| fault-hardfault | `…/Appli/build-fault-hardfault/08_Basic_Timer_Appli.elf` | `ba7e2807b22c767c161691b3ae87e0b11a3ea2f7a36bd935e43bf3ee0301ea63` | 0x34008091 | 同上 |
| fault-led-period | `…/Appli/build-fault-led-period/08_Basic_Timer_Appli.elf` | `144e564591f378a0fd97e15b137242b8a586a76688d0f3a801f5d0dec287b25f` | 0x34008091 | 同上 |
| fault-startup-stall | `…/Appli/build-fault-startup-stall/08_Basic_Timer_Appli.elf` | `7a52ad07f9f025fd57e1b4f0a8d8e40d187dc902faed899f89e6991d44f604e8` | 0x34008091 | vaddr 0x34000000, filesz 0x8610, memsz 0x9110, RWE |

（路径前缀：`/Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/08_Basic_Timer_L2Test_20260721_232852`）

**SRAM 范围核验**：全部 LOAD 段 vaddr 均为 0x34000000，最大结束地址 0x34009110，远小于链接脚本 RAM 上界 0x34200000（ORIGIN 0x34000400 + 2047K）→ ✅ 全部位于获准 SRAM 范围（0x34000000–0x34200000）。

---

## 5. ❗ 关键阻塞发现：SRAM 加载能力缺失（D-00 工具验证项）

按第二层规则"验证 ELF/SRAM 加载能力"与"如果当前 MCP 只能通过 Flash 加载固件，停止并标记 BLOCKED"：

| 事实 | 证据 |
|---|---|
| `flash_program` / `run_firmware` 只面向 Flash | 工具描述："Program file to flash memory" / "erase, program, run"；本层明确禁用 |
| `write_memory` 可写 SRAM，但受 `memory.max_write_size=4096` 限制需分块；更关键的是**没有寄存器写工具**，写入镜像后无法设置 PC/SP 至入口地址启动 | MCP 24 工具枚举；`src/config.rs` 默认值 |
| 裸 `reset` 后 BootROM 路径不会自动运行 SRAM 中的应用（当前固件系 IDE 调试会话加载并启动）；第二层 D-20 也预期"裸 reset 可能清除 SRAM 程序" | 平台事实；当前固件来源 |

**结论：以当前 MCP 工具面，无法自主完成"加载 ELF 到 SRAM 并启动"**，D-20/D-30/D-40/D-50 的加载-运行闭环在此状态下为 **BLOCKED: SRAM loading unavailable**。

**可选解除路径（需用户裁决，不属于本准备阶段执行）**：
- (a) 授权扩展 embedded-debugger-mcp：新增 SRAM ELF 加载工具（probe-rs 写 LOAD 段 + `write_core_reg` 设置 PC/SP + run），完成后重验 D-00 该项并回到授权门；
- (b) 接受第二层 BLOCKED，不推进。

---

## 6. 一次性授权门（按指令暂停于此）

### 6.1 展示材料

- **基线 ELF**：`/Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/08_Basic_Timer/Appli/build-l2acceptance-st/08_Basic_Timer_Appli.elf`
  SHA-256：`38ea1f13b5831df6288b03641dea1f07a3f7ec64645b1c4ae0885f84fcee69cf`（与运行中固件可执行内容逐字节一致）
- **三个测试 ELF**：路径/SHA-256/LOAD 段/入口见 4.3 表（全部位于 SRAM 0x34000000–0x34200000）
- **计划使用的 MCP 工具**：`connect / get_status / halt / run / step / reset / set_breakpoint / clear_breakpoint / read_memory / diagnose_fault / unwind_exception / disconnect`（+ 视 5.(a) 裁决而定的 SRAM 加载能力）
- **reset 预期效果**：STM32N647 Development boot 下裸 reset 大概率不会自动重启 SRAM 应用（ BootROM 路径）；复位测试与基线恢复依赖 SRAM 重载能力 —— **当前缺失，见第 5 节**
- **基线恢复方法**：重新加载基线 ELF（`38ea1f13…`）到 SRAM 并启动 —— 同样依赖第 5 节能力
- **声明**：全程不执行 `flash_erase` / `flash_program` / mass erase / option bytes / RDP / 安全生命周期修改

### 6.2 暂停点

**当前不请求进入 D-20**：因第 5 节的 SRAM 加载 BLOCKED 未解除，即使获得授权也无法完成加载-运行闭环。请用户裁决：

1. **(a) 授权扩展 MCP** 增加 SRAM ELF 加载工具（预计改动：probe-rs 后端写段 + 设置 PC/SP + run，含单元测试，类似 T-13 修复流程）；完成后我重新展示本授权门并请求"授权第二层验收"；或
2. **(b) 接受第二层 BLOCKED**，本次验收到此为止；或
3. **(c) 指出我不知道的既有 SRAM 加载途径**（例如某工具/参数组合），我将验证后更新本报告。

---

## 7. 本阶段对原工程的全部写入（透明披露）

| 路径 | 性质 |
|---|---|
| `08_Basic_Timer/reports/`（新建） | 本报告 + 4 份构建日志 |
| `08_Basic_Timer/Appli/build-l2acceptance/`（新建） | homebrew GCC 对照构建（证明工具链敏感性） |
| `08_Basic_Timer/Appli/build-l2acceptance-st/`（新建） | **基线 ELF 所在目录** |
| `08_Basic_Timer_L2Test_20260721_232852/`（新建，工程同级） | 隔离测试副本（含 4 个变体构建目录与其内 reports/） |

未删除、未修改原工程任何既有文件/目录；未触碰目标硬件（本阶段仅一次只读 `list_probes`）。

## 8. 结论摘要

```text
Safe Debug Regression:   PASS（第一层继承）
Automatic Debug Closure: BLOCKED（D-00/D-10 完成；D-20+ 因 SRAM 加载能力缺失阻塞，等待用户裁决）
Release Acceptance:      BLOCKED
```

| ID | 状态 | 说明 |
|---|---|---|
| D-00 | PASS（含 1 项 BLOCKED 发现） | 工具枚举完成；SRAM 加载能力缺失已定位 |
| D-10 | PASS | 基线可复现（工具链敏感性已解决）；4 个 ELF 构建并通过符号/范围核验 |
| D-20 ~ D-60 | BLOCKED | 等待第 5 节裁决 |
