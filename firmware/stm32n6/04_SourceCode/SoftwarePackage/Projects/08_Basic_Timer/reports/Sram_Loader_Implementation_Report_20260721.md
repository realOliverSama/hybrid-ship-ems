# embedded-debugger-mcp 扩展报告：`load_elf_to_sram`

- **日期**：2026-07-21
- **授权范围**：方案 (a) — 修改源码、编写并运行自动测试、构建 release 二进制、重连 MCP、展示报告；**未接触开发板**（无连接/复位/SRAM 写入/固件启动/Flash 操作）
- **新 release 二进制**：`/Users/oliverlyu/Developer/embedded-debugger-mcp-stm32n6/target/release/embedded-debugger-mcp`
- **SHA-256**：`7252a5f2bdcc9f7a8e9cad7447593be8d17ac853b904382a9da2aeafcbf88f48`（17.4M；经 touch 后完整重建两次哈希一致，优化构建确定性可复核）

---

## 1. 设计说明与完整调用路径

```
MCP 工具 load_elf_to_sram  (src/tools/debugger_tools/sram_loader.rs)
  ├─ 参数解析：sram_start/sram_end/vector_table_address（parse_address，溢出即拒绝）
  ├─ resolve_allowed_file_path（与 flash 工具相同的路径/大小策略）
  ├─ 【纯函数，无硬件】plan_sram_load  (src/sram/elf_plan.rs)
  │    ├─ ELF32/LE/EM_ARM 校验（object 0.37 解析）
  │    ├─ 白名单合法性 + 禁止区硬阻断（Peripheral/ExternalDevice/PPB/Vendor_SYS）
  │    ├─ 每个 PT_LOAD：paddr==vaddr、filesz<=memsz、溢出检查、文件边界、
  │    │   整段 ⊆ 白名单、不与禁止区相交、PF_X 标记提取
  │    ├─ 向量表确定：g_pfnVectors 符号 → .isr_vector 段 → 显式 override；
  │    │   缺失/歧义 → 拒绝，不猜测
  │    ├─ 向量表校验：MSP 4 字节对齐且 ∈ [白名单起点, 白名单终点]（允许等于 RAM 顶端，
  │    │   满递减栈惯例）；Reset bit0=1（Thumb）；Reset&~1 ∈ 已加载可执行段；
  │    │   ELF entry ∈ 已加载可执行段
  │    └─ 计算镜像 SHA-256（段地址 + 文件字节 + zero-fill，确定性布局）
  ├─ 【硬件】execute_sram_load  (src/sram/executor.rs)
  │    ├─ 非 Halted → 先 halt（记录 halted_for_load）
  │    ├─ 分段分块写入（chunk ≤ min(参数, config.memory.max_write_size=4096)）
  │    ├─ BSS 分块清零（memsz-filesz）
  │    ├─ 分块回读逐字节比对 + 整体 SHA-256 与计划摘要比对
  │    ├─ start=true 才执行：prepare_sram_launch(VTOR/MSP/xPSR=0x01000000/PC=Reset)
  │    │   → run；start=false 永不调用 run
  │    └─ 任何阶段失败：带阶段名+地址+原因返回，核心保持 Halted，不回退 Flash
  └─ 结构化文本报告（段表/SHA-256 双证据/向量表/MSP/Reset/entry/started）
```

后端扩展（`DebugBackend` trait 新方法）：
- `prepare_sram_launch(SramLaunchState{vtor, msp, pc, xpsr})` — probe-rs 实现：确认核心 Halted（否则拒绝）→ `write_word_32(0xE000ED08, vtor)` → `write_core_reg(MSP/xPSR/PC)`；OpenOCD 实现：明确报"不支持，请用 probe-rs"（安全拒绝）。

## 2. 修改文件清单

| 文件 | 变更 |
|---|---|
| `Cargo.toml` | 新增直接依赖 `object 0.37`（read/elf）、`sha2 0.11`（此前为传递依赖） |
| `src/sram/mod.rs`（新） | 模块声明 |
| `src/sram/elf_plan.rs`（新，~830 行） | 纯函数 ELF 规划器 + 16 个单元测试 |
| `src/sram/executor.rs`（新，~470 行） | 目标侧执行器 + 6 个 Mock 后端测试 |
| `src/backend/mod.rs` | `SramLaunchState` 结构 + trait 方法 `prepare_sram_launch` |
| `src/backend/probe_rs_backend.rs` | probe-rs 实现（VTOR/MSP/xPSR/PC，Halted 前置断言） |
| `src/backend/openocd_backend.rs` | 安全拒绝实现 |
| `src/tools/types.rs` | `LoadElfToSramArgs`（session_id/elf_path/sram_start/sram_end/vector_table_address?/start?/chunk_size?） |
| `src/tools/debugger_tools/sram_loader.rs`（新） | MCP 工具实现 |
| `src/tools/debugger_tools.rs`、`session.rs` | 模块/路由注册；路由测试更新为 25 工具 |

## 3. 工具参数与返回结构

**参数**（JSON Schema）：
| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `session_id` | string | ✅ | 会话 ID |
| `elf_path` | string | ✅ | ELF 绝对路径 |
| `sram_start` | string | ✅ | 白名单起点（含），hex/dec |
| `sram_end` | string | ✅ | 白名单终点（不含），hex/dec |
| `vector_table_address` | string | 可选 | 仅当 ELF 无 g_pfnVectors 符号且无 .isr_vector 段时必填 |
| `start` | bool | 可选 | 默认 **false**（load_only，不运行） |
| `chunk_size` | int | 可选 | 默认 2048，钳制 ≤ 4096（config.memory.max_write_size） |

**返回**：文本报告含逐段地址范围/filesz/memsz/exec/chunks/verified、Plan 与 Readback 双 SHA-256（MATCH 判定）、向量表来源与地址、Initial MSP、Reset_Handler、ELF entry、halted_for_load、started。

## 4. 自动测试结果（全部通过）

```
cargo test: 75 passed (4 suites, ~2s)，0 warnings（含 deprecated 清理）
```

新增 22 个测试覆盖（对应需求 9）：

| 需求项 | 测试 |
|---|---|
| 四个合法 SRAM ELF | `real_fixture_elfs_parse_when_available`（env 传入目录时执行，已实测 4/4 通过） |
| 向量表 ≠ LOAD 起点 | `vector_table_differs_from_load_base`（VT@0x34000400，LOAD@0x34000000） |
| BSS/zero-fill | `valid_project_style_elf_is_accepted`（memsz-filesz=0x100）+ executor BSS 清零断言 |
| 多 PT_LOAD | `multiple_load_segments_accepted` |
| 段越界 / 溢出 / filesz>memsz | `segment_outside_whitelist_rejected`、`overflow_in_segment_end_rejected`、`filesz_greater_than_memsz_rejected` |
| Flash/外设/PPB 拒绝 | `flash_peripheral_ppb_addresses_rejected`（0x08000000/0x40000000/0xE000E000/0x70100000）+ 白名单与禁止区相交拒绝 |
| 非法 MSP | `misaligned_msp_rejected`、`msp_outside_ram_rejected` |
| 非 Thumb Reset | `non_thumb_reset_handler_rejected` |
| Reset 不在已加载区 | `reset_handler_outside_loaded_executable_rejected` + entry 一致性拒绝 |
| 回读不一致禁止启动 | `readback_mismatch_blocks_start`（run/launch 调用计数为 0） |
| ELF 损坏/不存在 | `truncated_and_garbage_elf_rejected`；路径不存在由 `resolve_allowed_file_path`+`fs::read` 错误覆盖 |
| start=false 不运行 | `load_only_never_runs_and_verifies`（run=0、launch=0） |
| 其他 | 启动顺序（VTOR/MSP/xPSR=0x01000000/PC）、奇数 chunk 分块、OpenOCD 安全拒绝、向量表来源三态（symbol/section/override/歧义拒绝）、非 ARM 机器拒绝 |

## 5. 四个真实 ELF 的静态解析结果（规划器实测输出）

| ELF | vector_table | initial_msp | reset_handler | entry | 计划镜像 SHA-256 |
|---|---|---|---|---|---|
| fixture-baseline | 0x34000400（符号） | 0x34200000 | 0x34008091 | 0x34008091 | `72121bb01ab30504291e18f7281c80de3808d0fd1d11835fdcc465ec5d763d3a` |
| fault-hardfault | 0x34000400 | 0x34200000 | 0x34008091 | 0x34008091 | `8e4d5ed5d4d5742d642cb9debce2517dcd96f72a65c6347c9f0eed7451e2bed6` |
| fault-led-period | 0x34000400 | 0x34200000 | 0x34008091 | 0x34008091 | `80212c92af86fddd89899be37ca79f7e7e6c98932a89d3b3de886dbce795b3c4` |
| fault-startup-stall | 0x34000400 | 0x34200000 | 0x34008091 | 0x34008091 | `ee4dc2c7e2d9d36e3c8424b40db4e0df3caecff5b3193cdd592be32e7b2fdc2b` |

注：此为**计划镜像**（文件字节+zero-fill 按段布局）的 SHA-256，用于与硬件回读摘要比对；与 ELF 文件本身的 SHA-256 不同。

## 6. 安全边界与已知限制

- **无通用寄存器写**：MSP/xPSR/PC 只能在完整加载+校验通过且 start=true 时，由 `prepare_sram_launch` 一次性编程；无独立的寄存器写工具。
- **白名单为调用方义务**：工具强制段 ⊆ 白名单 + 禁止区硬阻断（0x40000000-0x5FFFFFFF、0xA0000000-0xDFFFFFFF、0xE0000000-0xFFFFFFFF）；本工程调用约定白名单 = 0x34000000–0x34200000。
- **paddr≠vaddr 拒绝**：无法解释的映射不做猜测。
- **MSP 边界**：允许 MSP 等于白名单终点（满递减栈顶）；更严格的栈可用性（MSP 下方有足够空间）不校验。
- **VTOR 写入**：写 SCB->VTOR（0xE000ED08），属于调试寄存器写，非 Flash/option bytes。
- **xPSR 固定 0x01000000**（仅 T 位置位）：中断优先级/栈对齐状态由硬件复位值隐式决定，未编程 PRIMASK/FAULTMASK/CONTROL（遵循"最小必要启动状态"）。
- **OpenOCD 后端**：`prepare_sram_launch` 明确拒绝（不支持）。
- **start=true 前提**：工具只保证写入镜像与计划一致并正确设置启动状态；固件本身行为（时钟/外设初始化）不在本工具验证范围。

## 7. 工具枚举（25 个）

前 24 个不变（list_probes, connect, disconnect, probe_info, halt, run, reset, step, get_status, read_memory, write_memory, set_breakpoint, clear_breakpoint, diagnose_fault, unwind_exception, rtt_attach, rtt_detach, rtt_read, rtt_write, rtt_channels, flash_erase, flash_program, flash_verify, run_firmware），新增第 25 个 **`load_elf_to_sram`**。（路由测试 `combined_router_registers_all_tools` 已按 25 个断言并通过。）

## 8. 硬件授权门（重新展示，等待单独授权）

- **基线 ELF**：`/Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/08_Basic_Timer/Appli/build-l2acceptance-st/08_Basic_Timer_Appli.elf`
  SHA-256：`38ea1f13b5831df6288b03641dea1f07a3f7ec64645b1c4ae0885f84fcee69cf`（与运行中固件可执行内容逐字节一致）
- **三个测试 ELF**（副本 `08_Basic_Timer_L2Test_20260721_232852/Appli/build-*/`）：
  - fault-hardfault `ba7e2807…01ea63`；fault-led-period `144e5645…87b25f`；fault-startup-stall `7a52ad07…44f604e8`
  - 全部 LOAD ∈ 0x34000000–0x34200000，入口 0x34008091
- **计划使用的 MCP 工具**：`load_elf_to_sram`（白名单 0x34000000–0x34200000）、`connect/get_status/halt/run/reset/set_breakpoint/clear_breakpoint/read_memory/diagnose_fault/unwind_exception/disconnect`
- **reset 预期效果**：裸 reset 后 BootROM 不自动运行 SRAM 应用 → 复位测试流程为"load(start=true) → 验证 → reset → 重新 load 恢复基线"
- **基线恢复方法**：以基线 ELF（`38ea1f13…`）执行 `load_elf_to_sram(start=true)`
- **声明**：全程不擦写 Flash、不改 option bytes/RDP/安全生命周期

**当前暂停于此**。等待你单独授权"基线 ELF SRAM 加载干跑"，不会自行进入 D-20。
