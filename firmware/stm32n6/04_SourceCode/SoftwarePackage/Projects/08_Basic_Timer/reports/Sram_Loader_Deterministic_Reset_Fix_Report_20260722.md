# load_elf_to_sram 确定性启动修复报告（deterministic_reset）

- **日期**：2026-07-22
- **授权范围**：仅修改源码 + 自动测试 + 构建 release + 输出报告；**未接触开发板**
- **新 release 二进制**：`/Users/oliverlyu/Developer/embedded-debugger-mcp-stm32n6/target/release/embedded-debugger-mcp`
- **SHA-256**：`592c83fde87f59ca4ad9a12ab1f10ad7136d0788cbb004133503e2230bc4c6be`

---

## 1. 根因分析

D-30 第 1 轮失效（用户裁定为 INVALID）的直接技术根因：STM32N647（Cortex-M55）固件在 `main()` 中调用 `SCB_EnableICache()` / `SCB_EnableDCache()`，旧版 `load_elf_to_sram` 通过调试 AHB 重写 SRAM 后**直接设置 PC/MSP 并 run**，没有复位，也没有缓存维护。CPU 从**旧固件遗留的 I-cache/D-cache 缓存行**取指读数，执行了新旧镜像的混合内容（实测证据：TIM6 IRQ 中 `HAL_TIM_IRQHandler` 收到荒谬的 htim 指针 `0xAF00B580`，即新镜像中某函数的机器码首字；SCB->CCR=0x00030201 缓存双开）。

更一般地，"热加载"还继承了 **NVIC 未决/使能状态、外设运行状态、MPU/RIF 配置**等 —— 仅清理缓存不足以保证确定性。因此本次修复不做"末尾追加 cache clean"，而是实现**确定性冷启动**：启动路径必须经过 system reset。

## 2. 修改文件与关键设计

| 文件 | 变更 |
|---|---|
| `src/sram/executor.rs` | 重写。新增 `LaunchMode`（DeterministicReset/PreserveState）、`CacheState`、顺序化执行流；返回结构扩展（见第 4 节） |
| `src/tools/types.rs` | `LoadElfToSramArgs` 新增 `launch_mode`（可选，默认 deterministic_reset） |
| `src/tools/debugger_tools/sram_loader.rs` | launch_mode 解析/校验、新返回字段格式化、工具描述更新 |

### 新旧调用顺序对照

**旧（start=true，热加载，已废除）**：
```
halt(若运行) → write → zero-fill → readback → 设 VTOR/MSP/xPSR/PC → run
（缓存/NVIC/外设状态继承自旧固件 —— D-30 失效根因）
```

**新（deterministic_reset + start=true，唯一允许的启动路径）**：
```
a. 纯函数规划（ELF/白名单/向量表校验，不写目标）
b. system reset-and-halt（AIRCR.SYSRESETREQ：核心+NVIC+外设全复位）
c. 确认 Halted（否则零写入拒绝）
d. 读 SCB->CCR 记录 reset 后缓存状态
e/f. I-cache 或 D-cache 仍开 → 零写入安全拒绝
g. 写入 PT_LOAD + BSS zero-fill
h. 逐块回读 + Plan/Readback SHA-256 双证据
i. 设 VTOR/MSP/xPSR(=0x01000000 Thumb)/PC
j. 全部成功后才 run
```

**preserve_state**：仅允许 start=false（load-only）：不 reset；运行中则 halt 后写入+验证；明确报告"未 reset、未编程启动状态、之后启动需先 deterministic reset"。**preserve_state + start=true 在任何目标操作之前拒绝**。

组合矩阵：

| launch_mode | start | 行为 |
|---|---|---|
| deterministic_reset（默认） | true | 完整 a→j 冷启动序列 |
| deterministic_reset | false | a→h，不编程启动状态、不 run，报告 reset_performed=true |
| preserve_state | false | 不 reset，halt→写→验，不 run，报告 reset_performed=false |
| preserve_state | true | **拒绝**（stage='launch-mode'，零后端调用） |

## 3. 工具 schema（增量）

新增参数：
| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `launch_mode` | string | 可选 | `deterministic_reset`（默认）/ `preserve_state`；start=true 只接受 deterministic_reset |

其余参数不变（session_id/elf_path/sram_start/sram_end/vector_table_address?/start?/chunk_size?）。

## 4. 返回结构（新增字段）

`launch_mode`、`reset_performed`、`reset_kind`（"system reset-and-halt (AIRCR.SYSRESETREQ, core+NVIC+peripherals)"）、`cache_state_before_reset`、`cache_state_after_reset`（CCR 原值 + I/D 位解读）、`deterministic_start`、`halted_for_load`、`started`；失败时错误文本带 `stage='<阶段>'` 与地址/原因（reset-and-halt / reset-confirm / cache-check / write / zero-fill / readback / verify / digest / launch / run / launch-mode）。

## 5. 自动测试结果

```
cargo test: 80 passed (4 suites, ~2s)，0 warnings
fixture 集成测试（4 个真实 ELF 静态解析）: 1 passed
```

新增/更新 10 个执行器测试（对应需求 10）：

| 需求 | 测试 | 结果 |
|---|---|---|
| start=true 默认 deterministic_reset | `start_true_deterministic_reset_full_sequence`（断言严格顺序 reset→cache读→write→verify→launch→run） | ✅ |
| reset 失败零写入 | `reset_failure_means_zero_writes` | ✅ |
| reset 后非 Halted 零写入 | `not_halted_after_reset_means_zero_writes` | ✅ |
| reset 后缓存仍开安全拒绝 | `caches_still_on_after_reset_safely_rejected` | ✅ |
| readback 不一致禁止 run | `readback_mismatch_blocks_run_in_deterministic_mode` | ✅ |
| 寄存器设置失败禁止 run | `launch_failure_blocks_run` | ✅ |
| preserve_state+start=true 拒绝 | `preserve_state_with_start_true_rejected_before_any_target_call`（断言零后端调用） | ✅ |
| start=false 不意外运行 | `start_false_deterministic_resets_but_never_runs` + `start_false_preserve_state_loads_without_reset` | ✅ |
| 既有规划器测试（范围/向量表/MSP/Thumb/溢出） | elf_plan 16 个测试 | ✅ 全部通过 |
| 25 工具路由回归 | `combined_router_registers_all_tools` | ✅ |

## 6. 已知限制

1. **reset 后缓存检查依赖硬件行为**：正常 Cortex-M 系统复位会将 SCB->CCR 清零；若某硅片复位后缓存仍开，工具将安全拒绝而非强行继续（需求 4f 的意图）。
2. **preserve_state 不提供任何热启动保证**：其写验证通过调试 AHB 进行（与 CPU 缓存无关，结论可靠），但报告明确标注"之后启动需先 deterministic reset"。
3. **外设状态**：deterministic_reset 依赖 AIRCR.SYSRESETREQ 的芯片级复位语义（核心+NVIC+外设）；个别需上电才复位的寄存器（如某些 RIF/安全单元配置）不在保证范围内。
4. **OpenOCD 后端**：`prepare_sram_launch` 仍安全拒绝（未实现）。
5. **reset 在写之前、写之后不再 reset**：流程保证（需求 5）；复位清不掉 SRAM 内容，但若 reset 本身改变了 SRAM 内容（如 BootROM 使用 SRAM），目前不重新验证（当前实测 N6 BootROM 不动 0x34000000 区域，属于平台观察而非保证）。

## 7. D-30 异常登记（按用户裁定）

D-30 第 1 轮为 **INVALID（测试基础设施缺陷）**，不评价故障夹具；本报告与 `STM32N647_Automatic_Debug_Closure_Report_20260722_004451.md` 第 2.3/5 节保留其作为加载器缺陷发现证据。

## 8. 下一次硬件验证的最小方案（待授权）

1. `/mcp reconnect embedded-debugger` 后核验 25 工具与新 schema（含 launch_mode）；
2. **确定性干跑**：基线 ELF（`38ea1f13…`）start=true 默认模式 → 预期返回 reset_performed=true、cache_state_after_reset 双关、Plan==Readback MATCH、目标 Running、LED 恢复；
3. **负向验证**：preserve_state + start=true → 预期拒绝（零目标副作用）；
4. 以上通过后重跑 D-30 第 1 轮（fault-hardfault）：预期 UDF → HardFault，现场 PC 落在 `fault_trigger_hardfault`（0x340080F2），diagnose_fault 显示 UNDEFINSTR。

---

**MCP server 已停止**（旧进程已终止，新二进制待加载）。请手工执行：

```text
/mcp reconnect embedded-debugger
```

我未连接开发板、未做任何硬件验证，当前在此暂停。
