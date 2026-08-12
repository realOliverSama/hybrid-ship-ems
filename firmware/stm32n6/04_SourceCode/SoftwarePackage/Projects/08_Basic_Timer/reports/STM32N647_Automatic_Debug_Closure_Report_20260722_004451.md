# STM32N647 第二层验收：Automatic Debug Closure 报告

- **报告时间**：2026-07-22 00:44（本地）；硬件操作窗口 2026-07-21 16:27–16:44 UTC
- **执行范围**：D-20（完成）、D-30 第 1 轮（执行并失败，根因已定位）、D-40/D-50/D-60（按规则 10 停止，未执行）
- **总体结论**：**Automatic Debug Closure: FAIL** —— 但失败根因是被测工具 `load_elf_to_sram` 的一个真实缺陷（启动前未做缓存维护），非测试夹具或流程错误

---

## 0. 历史记录更正（按用户指令）

| 项 | 错误记录 | 更正 |
|---|---|---|
| 原始基线 entry/Reset | D-10 记录为 0x340080d1（该值实际属于 homebrew GCC 对照构建 `build-l2acceptance`） | **0x34008081**（ST 14.3.1 基线 `build-l2acceptance-st`，经硬件加载与向量表双重确认） |
| 0x34008091 | — | 属于 fixture 系列（隔离副本 4 个构建） |

---

## 1. D-20：复位与恢复能力 — **PASS**

会话：`session_ce6bb5c0-4ee6-4f73-907a-ed7f4b5110d1`（已断开）。基线 ELF SHA-256 加载前复核 `38ea1f13…ee69cf` ✅。

### 1.1 基线加载
```
Segments: 0x34000000..0x340090C8  filesz=34252 memsz=37064 exec=true chunks=19 verified=37064B
Plan SHA-256 == Readback SHA-256 == ed963f42c1bdd254e7682286a305480a65d3747397811c8fe9f3f73522f5089d (MATCH)
Vector table: 0x34000400 (g_pfnVectors)  MSP: 0x34200000  Reset/entry: 0x34008081  Started: true
```

### 1.2 reset-and-halt
```
reset(halt_after_reset=true, hardware) → State: Halted, PC: 0x00000000, SP: 0x00000000
```
- Halted ✅；PC/SP 读出为 0 —— reset-and-halt 在取向量前即停住（BootROM 尚未初始化寄存器）。
- 附加只读取证：BootROM 向量表位于 **0x18000000**（SP=0x341037F0，Reset=0x180020C5）。

### 1.3 reset-and-run
```
reset(halt_after_reset=false) → State: Running, PC: 0x18003A1A, SP: 0x341037A0
get_status（约 2 分钟后）→ State: Halted, PC: 0x18003A1A, SP: 0x341037A0
```
- **平台事实**：Development boot 下裸 reset 后核心运行 BootROM（0x1800xxxx），随后**停在 BootROM 固定点 0x18003A1A**；SRAM 应用不会自动恢复运行。恢复 = 重新加载基线（已验证 3 次）。

### 1.4 三次"加载→复位→检查→重载"循环

| 循环 | reset 后状态 | 重载 | 最终 |
|---|---|---|---|
| 1 | BootROM PC=0x18003A1A SP=0x341037A0 | MATCH (ed963f42…) | Running ✅ |
| 2 | 完全相同（可复现） | MATCH | Running ✅ |
| 3 | 完全相同 | MATCH | Running ✅ |

无断点使用，无会话遗留（已 disconnect）。

---

## 2. D-30：HardFault 自动诊断闭环 第 1 轮 — **FAIL（工具缺陷）**

会话：`session_ce26dab5-15da-4500-b646-3806081a8d32`（已断开）。

### 2.1 加载与启动
- fault-hardfault ELF SHA-256 复核：`ba7e2807…01ea63` ✅（与锁定值一致）
- 加载成功：37096B，Plan==Readback `8e4d5ed5…e2bed6` (MATCH)，Reset=0x34008091，Started=true

### 2.2 期望 vs 实测

**期望**：UDF #0（0x340080F2，`fault_trigger_hardfault+0x4`）→ UNDEFINSTR → HardFault，现场 PC 落在触发函数。

**实测**（2 秒观察窗后 halt → PC=0x34007DDC, SP=0x341FFF54）：

`diagnose_fault` 原始结果（节选）：
```json
{
  "core": { "lr": "0xFFFFFFF1", "pc": "0x34007DDC", "sp": "0x341FFF54" },
  "scb_raw": { "cfsr": "0x00008200", "hfsr": "0x40000000", "bfar": "0xAF00B580",
               "icsr": "0x0400F003", "shcsr": "0x00000004", "cpuid": "0x411FD221" },
  "cfsr_flags": ["PRECISERR", "BFARVALID"],
  "hfsr_flags": ["FORCED"]
}
```

`unwind_exception` 原始结果（节选）：
```json
{ "frames": [
  { "function": "HardFault_Handler", "file": ".../stm32n6xx_it.c", "line": 89, "pc": "0x34007DDC" },
  { "function": "UNWIND: Error ... No Stack Pointer register ...", "pc": "0x34007DDC" }
] }
```

异常栈帧（0x341FFF58 起，经偏移推证）：`R0=0xAF00B580, LR=0x34007E33, PC=0x340065CA, xPSR=0x01000088`（IPSR=136=TIM6 IRQ 上下文）。

### 2.3 根因分析（证据链）

1. **并未走 UDF 路径**：CFSR 无 UNDEFINSTR；全栈无 0x340080F2；LR=0xFFFFFFF1 表明 HardFault 来自**Handler 模式**（中断上下文），非主循环线程。
2. **真实故障点**：`HAL_TIM_IRQHandler` 首条解引用（0x340065CA `ldr r3, [r3, #0]`，stm32n6xx_hal_tim.c:3840），R0=htim=**0xAF00B580** → 精确总线错误，BFAR 一致。0xAF00B580 即 `fault_fixture_loop_hook` 的机器码首字（`push {r7,lr}; add r7,sp,#0`），是不可能合法的 htim 指针。
3. TIM6_IRQHandler 的 literal pool（0x34007E48）经调试口回读为**正确值** 0x34008610（&htim6）——SRAM 内容无误（加载已回读验证）。
4. **决定性证据**：读 SCB->CCR（0xE000ED14）= **0x00030201 → I-cache（bit17）与 D-cache（bit16）均处于使能状态**。之前运行的固件（main 调用 `SCB_EnableICache/DCache`）使缓存保持开启；`load_elf_to_sram` 通过调试 AHB 重写 SRAM 后**未做任何缓存维护**（无 ICIALLU / D-cache clean-invalidate），核心启动后从**陈旧缓存行**取指/读数，执行了新旧镜像的混合内容，产生不可能的执行路径与荒谬寄存器值。

**根因结论**：被测工具 `load_elf_to_sram` **缺少启动前缓存维护**，在缓存使能的 Cortex-M（M55/M7 类）上属于架构性不安全。故障夹具（UDF 编码、钩子、调用链）经反汇编核验均正确，不是失败原因。

### 2.4 修复与复测状态
- 本用例的"修复"应针对**工具**（加载器增加缓存维护），属于对 embedded-debugger-mcp 的新一轮代码修改，**本次授权未覆盖**，未执行。
- fixture-baseline 复测未进行（规则 10 停止）。

### 2.5 按规则 10 的清理（全部完成）
```
load baseline(38ea1f13…) → Plan==Readback ed963f42… (MATCH) → Started: true
get_status → State: Running
disconnect → 成功，probe-rs 资源已清理（会话 12.8 分钟）
```
- 目标：Running（基线固件，与缓存内容一致，状态连贯）
- 断点：全程未设置，无残留
- 会话：无活动会话

---

## 3. D-40 / D-50 / D-60

**未执行**（按规则 10：D-30 失败 → 停止后续故障用例）。无证据生成，不记 SKIP/PASS。

---

## 4. 工具调用合规与证据完整性

| 项 | 状态 |
|---|---|
| Flash 擦写（flash_erase/program/verify/run_firmware） | **零调用**（本会话全部操作记录可复核） |
| 白名单外内存写入 | 无（仅 load 白名单内段 + 启动序列 VTOR 写） |
| reset 使用 | 仅 D-20 授权范围内 |
| 原始工程源文件 | 零修改（故障夹具全部在隔离副本） |
| 轮询/重试上限 | 遵守（无无限轮询；无重试） |
| ELF 加载前 SHA-256 复核 | 每次执行 ✅ |
| Plan/Readback MATCH | 每次加载均 MATCH ✅ |

## 5. 缺陷登记

| 级别 | 缺陷 | 证据 |
|---|---|---|
| **Critical** | `load_elf_to_sram` 启动前不做缓存维护：缓存使能的目标上，写 SRAM 后直接启动会执行陈旧缓存内容，行为不可预期 | 本报告 2.3（CCR=0x00030201、BFAR=0xAF00B580、不可能的 htim 指针） |
| Medium | `unwind_exception` 在异常上下文无法获取调用栈（"No Stack Pointer register" probe-rs 报错），HardFault 回溯断在第一帧 | 2.2 原始输出 |
| Low | reset-and-halt 后 PC/SP 读为 0（复位点过早，寄存器未初始化） | 1.2 |

**修复建议（待授权）**：在 `execute_sram_load` 启动序列中加入缓存维护 —— 写完成后/run 之前：SCB->ICSR/ICIALLU 全失效 I-cache，按 CCSIDR set/way 全量 clean+invalidate D-cache（或启动前强制 CCR.DC/IC=0 + 失效）；并补充"缓存使能目标"的集成测试策略。修复后重跑 D-30 起。

## 6. 三层验收结论

```text
Safe Debug Regression:   PASS（第一层结论未被本轮证据推翻）
Automatic Debug Closure: FAIL（D-20 PASS；D-30 因工具缺陷 FAIL；D-40/D-50/D-60 未执行）
Release Acceptance:      BLOCKED
```

**不得宣布 Release Ready。** 下一步最小事项：授权修复 `load_elf_to_sram` 缓存维护缺陷并重跑 D-30（无需重新准备资产，五个 ELF 与隔离副本仍然有效）。
