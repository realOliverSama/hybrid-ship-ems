# STM32N647 第二层验收最终封存报告（L2 Final Inheritance Record）

- **封存时间**：2026-07-22（由本次验收会话原始工具输出逐字整理；每条均给出证据定位）
- **结论**：
```text
Safe Debug Regression:   PASS
Automatic Debug Closure: PASS
Release Acceptance:      BLOCKED
```

## 1. 工具链封存

| item | value | evidence locator |
|---|---|---|
| MCP release binary | `/Users/oliverlyu/Developer/embedded-debugger-mcp-stm32n6/target/release/embedded-debugger-mcp` | 阶段 B 构建输出 |
| binary SHA-256 | `b06ecc3ef2f9f227dcbc99cd90c18337ba4f5c629d06ea1cc86fc929ccf58b9d` | 阶段 B `shasum -a 256` 原始输出 |
| tools | 26（含 load_elf_to_sram、read_special_registers；无公开写特殊寄存器工具） | MCP server 枚举 "MCP: 1/1 servers, 26 tools" |
| tests | cargo test 90 passed；clippy -D warnings 0；fmt clean | 阶段 B 验证输出 |
| 关键工具修复 | 断点地址四层校验；load_elf_to_sram；deterministic_reset 冷启动；run 前中断屏蔽归一化（PRIMASK/BASEPRI/FAULTMASK→0、CONTROL 保持、回读） | `reports/Sram_Loader_Implementation_Report_20260721.md`、`reports/Sram_Loader_Deterministic_Reset_Fix_Report_20260722.md`、阶段 B 报告 |

## 2. 资产 SHA-256 封存

| ELF | SHA-256 | LOAD 范围 |
|---|---|---|
| 基线（launch.json Debug，新鲜构建 2026-07-22 12:28:57） | `ddf589a855878f60451647a549b972e68a24c131eb8245c5e48f2995e208541b` | 0x34000000..0x340090C8 |
| fixture-baseline（修复态） | `670190e0fdfc79e329a25135941dfa62f7e4f43f2c10015996aa83665e4cfc2d` | 0x34000000..0x340090E8 |
| fault-hardfault | `ba7e2807b22c767c161691b3ae87e0b11a3ea2f7a36bd935e43bf3ee0301ea63` | 0x34000000..0x340090E8 |
| fault-led-period | `144e564591f378a0fd97e15b137242b8a586a76688d0f3a801f5d0dec287b25f` | 0x34000000..0x340090E8 |
| fault-startup-stall | `7a52ad07f9f025fd57e1b4f0a8d8e40d187dc902faed899f89e6991d44f604e8` | 0x34000000..0x34009110 |

## 3. D-20 ~ D-60 证据定位

| 项 | 证据（原始值） | 定位 |
|---|---|---|
| D-20 复位恢复 | reset-and-halt→Halted；reset-and-run→BootROM PC=0x18003A1A；3 轮"复位→重载→Running"全部 MATCH | 会话 `session_ce6bb5c0-…` 原始输出（2026-07-21 16:27–16:44 UTC） |
| D-30 HardFault ×3 | 三轮一致：CFSR=0x00010000（UNDEFINSTR 唯一）、HFSR=0x40000000（FORCED）、ICSR VECTACTIVE=3、LR=0xFFFFFFE9；stacked PC=**0x340080F2**（异常帧 @0x341FFF70 布局，LR=0x34008115）；addr2line → `fault_trigger_hardfault` fault_fixture.c:31；修复后 CFSR=0 | 阶段 C-D D-2 + 阶段 E R2/R3 `diagnose_fault` 原始 JSON 与 0x341FFF6C 起栈帧转储 |
| D-40 LED 周期 ×3 | 三轮：故障态 `led1_toggle_half_period_ms`@0x340085EC=**0x32（50）**（契约 200，符号经 nm 解析，定义 fault_fixture.c:14，使用点 main.c:130）；修复态=**0xC8（200）** | 阶段 E R1–R3 read_memory 原始输出 |
| D-50 启动停滞 ×3 | 三轮：PC=**0x340080FA** → `fault_startup_stall` fault_fixture.c:46；marker@0x34008708=**0x57A11ED1**；修复后 marker=0 且 PC 位于主循环 | 阶段 E R1–R3 halt/read_memory + addr2line 输出 |
| Plan==Readback | 每次加载均 MATCH（基线 `5b50dcc7…99d96`、fault-hf `8e4d5ed5…e2bed6`、fault-lp `80212c92…95b3c4`、fault-ss `ee4dc2c7…2fdc2b`、fixture `72121bb0…763d3a`） | 各次 load_elf_to_sram 原始输出 |
| 启动前 mask 归一化 | 每次启动：masks before PRIMASK=1/BASEPRI=0/FAULTMASK=0 → after 0/0/0；CONTROL before/after 一致（0x00 或 0x0C）；DHCSR.C_MASKINTS=0（归一化前置校验）；normalized=true | 各次 load 原始输出 "Interrupt masks before/after" 行 |
| 中断活性（自动证据） | **从锁定基线 ELF 动态解析符号地址**：SysTick_Handler=0x34007e1a、TIM6_IRQHandler=0x34007e28（nm 原始输出）；硬件断点命中 PC=0x34007E1A（首轮 ≤250ms）与 PC=0x34007E28（≤1.5s）；断点均已清除；命中后三 mask=0、CFSR/HFSR=0 | 阶段 C-3（基线）与 D-3（恢复后复查）原始输出 |
| 最终恢复 | 基线 SHA `ddf589a8…` 复核后加载，Plan==Readback MATCH，目标 Running，无活动会话/断点，LED 400ms/1s 经用户人工确认 | 阶段 E D-50 R3 末尾 + D-60.4 用户确认"确认恢复闪烁" |

## 4. 合规封存

零 flash_erase/flash_program/flash_verify/run_firmware；零 Option Bytes/RDP/OTP/安全生命周期/SAU/GTZC/RIF 修改；SRAM 写入严格限于 0x34000000–0x34200000 且仅经 load_elf_to_sram；零 write_memory/寄存器裸写；原始工程源文件零修改（故障夹具全部在隔离副本 `08_Basic_Timer_L2Test_20260721_232852`）。
