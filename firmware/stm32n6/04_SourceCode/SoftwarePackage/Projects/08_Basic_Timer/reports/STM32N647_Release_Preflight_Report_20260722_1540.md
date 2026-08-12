# STM32N647 第三层验收：Release 只读预检报告

- **日期**：2026-07-22（15:16–15:40 JST）
- **范围**：P-00 ~ P-40 只读预检；**全程未连接/复位/擦写/启动开发板**
- **终态**：**`APP_FSBL_CONTRACT_CHANGED`**（入口定位规则不匹配，停止；不生成烧录计划、不展示授权请求）

---

## P-00：第二层继承 — PASS（含自动中断活性证据）

继承清单文件：`reports/STM32N647_L2_Final_Inheritance_20260722.md`（本报告姊妹文件，由原始工具输出封存，含每条证据定位）。
- 两层结论 `Safe Debug Regression: PASS` / `Automatic Debug Closure: PASS`；
- MCP 二进制 SHA-256 `b06ecc3ef2f9f227dcbc99cd90c18337ba4f5c629d06ea1cc86fc929ccf58b9d`（26 tools，90 tests）——主机工具哈希，不作烧录授权前缀；
- 五个 ELF SHA-256 与 LOAD 范围、D-30×3（stacked PC=0x340080F2、UNDEFINSTR、FORCED）、D-40×3（50→200）、D-50×3（PC=0x340080FA、marker 0x57A11ED1→0）、每次 Plan==Readback MATCH、启动前 mask 全零+CONTROL 保持+C_MASKINTS=0；
- **自动中断活性**：从锁定基线 ELF 动态解析的 SysTick_Handler=0x34007e1a / TIM6_IRQHandler=0x34007e28 硬件断点命中记录（基线与恢复后各一轮，均已清除断点）。
- `L2_INHERITANCE = PASS`（无需 appendix）。

## P-10：Release 构建链 — PASS（可复现）

| 项 | 值 |
|---|---|
| 生成源 | `Tools/atk_stm32n6_cmake_prepare.py` v3.3.0；tasks.json 无 `_v3_3.py` 引用 |
| dry-run | "already up to date"（无源码/配置改动计划） |
| Release linker | `Appli/STM32N647XX_ROMxspi2_RAMxspi1.ld`：ROM ORIGIN=**0x70100400**（含 `ASSERT(ORIGIN(ROM)==0x70100400)`） |
| 工具链 | GNU Tools for STM32 14.3.rel1（14.3.1）、cmake 3.28.1、ninja 1.11.1 |
| 隔离构建 | `Appli/build-l3a` 与 `Appli/build-l3b`（预先不存在的新目录），configure/build 退出码均 0 |

**两次构建 manifest（逐字节一致）**：

| role | path | size | SHA-256 | 关键值 |
|---|---|---|---|---|
| ELF（两轮同哈希） | `Appli/build-l3a|b/08_Basic_Timer_Appli.elf` | 25064 | `56ce8ef0c098df69bc220bea2924ef52ca43c899a7ae195c72cde9d5687e4763` | entry=0x70101699（Thumb）；`g_pfnVectors`=**0x70100400**；LOAD：0x70100000..0x701017e0（含 ELF 头）+ 0x34000000（LMA 0x701017e0，12B）+ 2 个 BSS 段 |
| MAP | `…/08_Basic_Timer_Appli.map` | 324772 | — | — |
| HEX（两轮各自 objcopy，逐字节一致） | 由隔离 ELF 生成 | — | `467d5ad4adaa4c53…`（前 16 位，全量见日志） | **数据范围 0x70100400..0x701017EC，5072 字节** |
| BIN（两轮一致） | 由隔离 ELF 生成 | 5100 | `288966c5975a6590…`（前 16 位） | 绑定唯一基址 0x70100400（linker+HEX 证明） |

工程固定产物目录 `Appli/build/flash/` 亦含同名 HEX/BIN（`atk_release_flash_files` 生成，与隔离产物同源）。**HEX 最低地址由实际解析得出 = 0x70100400** ✓（与 EXPECTED_RELEASE_LINK_BASE 一致）。

## P-20：启动资产审计 — 发现契约矛盾

| asset | path / 证据 | SHA-256 | status |
|---|---|---|---|
| Release ELF/HEX/BIN | 见 P-10 manifest | 见上 | candidate |
| on-board FSBL | 不可离线读取 | — | `PRESERVE` / `PENDING_READ_ONLY_VERIFY` |
| FSBL 参考镜像 | `FSBL/MX25UM25645G_W958D8NBYA5I_Example/Binary/fsbl.hex` | `0ecad6988e4d2795344975eefe53869d987ae40474e742429dad8cc4427d4b99` | 只读参考；范围 **0x70000000..0x70007020**（<32KB，slot=外部 Flash 0 偏移 ✓） |
| FSBL 参考源码 | `FSBL/MX25UM25645G_W958D8NBYA5I_Example/FSBL/Core` + `Middlewares/ST/STM32_ExtMem_Manager/boot/stm32_boot_xip.c` | — | 只读参考 |
| recovery Appli（前版） | `Projects/08_Basic_Timer/Binary/appli.hex` | `2699eb7a7223f26b7b9041c6e5888eb01e1125f066a1dacce2ce229967bb9fb3` | 范围 0x70100400..0x701018A0；`NOT AUTHORIZED`（用户未要求自动恢复，不阻塞） |
| 外部 loader | `External_Loader/MX25UM25645G_ATK-CNN647B/Binary/MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr` | `0bce846a1f25e4e967d572efb5a3f97f8a839817b53bf60d837aadfee740dcb5` | candidate（41156B） |
| SigningTool | 未在 Appli 构建链使用（HEX/BIN 直接由 objcopy 生成） | — | not required |

**外部 NOR**：MX25UM25645G（Macronix，256Mbit=32MB），XSPI2，AXI 映射基址 `XSPI2_BASE=0x70000000`（stm32n647xx.h:2876）。

### ❗ 契约矛盾（终态根因）

本地参考 FSBL 的入口定位（`stm32_boot_xip.c: JumpToApplication` + 板级 `stm32_extmem_conf.h`）：

```text
Application_vector = XSPI2_BASE(0x70000000) + EXTMEM_XIP_IMAGE_OFFSET(0x100000) + EXTMEM_HEADER_OFFSET(0)
                   = 0x70100000        ← FSBL 在此读 MSP 与 Reset 并 VTOR=0x70100000
（fsbl.hex 字节中实测含 0x00100000 常量 ×2，证明参考镜像确实用 0x100000 偏移）
```

而 Appli 三方证据（linker ASSERT、ELF `g_pfnVectors`、HEX 最低地址）一致为 **向量表=0x70100400**。

**0x70100000–0x70100400（1KB）归属无法离线判定**：Appli HEX/BIN 不覆盖（自 0x70100400 起）、fsbl.hex 不覆盖（止于 0x70007020）、工程内无产生该"着陆向量/header"的资产。若板上 FSBL 即参考配置，则其读取 0x70100000 处的向量内容来源不明；若板上 FSBL 实际偏移为 0x100400，则与本地参考源不一致 —— 两种情况都构成"入口定位规则不匹配/不可证明"，且板上 FSBL 内容本回合禁止读取（`PENDING_READ_ONLY_VERIFY`）。

按 P-20 第 6 条与 P-40 第 14 条：输出 `APP_FSBL_CONTRACT_CHANGED` 并停止；不生成 FSBL 写入计划，也不生成 Appli 烧录计划。

## P-30：编程后端审计

| 后端 | 裁决 | 证据 |
|---|---|---|
| probe-rs / MCP | `MCP_EXTERNAL_NOR_PROGRAMMING = UNSUPPORTED` | probe-rs 0.30 `STM32N6_Series.yaml` 虽有 `XSPI2NOR` NVM 区域（0x70000000–0x74000000），但**整个 yaml 无任何 flash_algorithms 条目**；无针对本板 XSPI2/MX25UM25645G 的写入算法 |
| STM32CubeProgrammer | `CUBEPROGRAMMER_EXTERNAL_LOADER` 机械可行 | v2.23.0（`--version` 实测）；`--help` 实测支持 `-el/--extload <file>`（自定义 .stldr）、`-e [start end]`（指定扇区范围擦除）、`-w <addr>`（定址写入）、`-v`（verify）、`-rst`/`-run`/`-g`（复位/运行分离控制） |

## P-40：离线负向门与计划

由于 P-20 契约矛盾，不生成烧录计划。离线负向检查中已识别并记录的硬性事实：HEX 最低地址 0x70100400 ≠ FSBL 跳转目标 0x70100000；0x400 间隙无归属资产；recovery Appli 已锁定哈希但未授权自动恢复；probe-rs 路线不可用；CubeProgrammer 路线机械可行但被契约问题阻断。

## 终态

```text
APP_FSBL_CONTRACT_CHANGED
```

**不展示授权请求。** 解除阻塞的最小事项（二选一或组合）：
1. 用户提供板上 FSBL 的真实入口定位证据（例如：原生冷启动 Release 成功的历史、板级文档、或授权下一阶段经 external loader **只读**回读 0x70000000 起的 FSBL 区与 0x70100000–0x70100400 间隙内容以实证跳转偏移与着陆向量）；
2. 若证明板上 FSBL 确跳 0x70100000 且间隙无合法着陆向量，则需独立的 FSBL/启动头维护流程（另行授权，本文件范围外）。

> 备注：本报告未执行任何硬件命令；未修改工程/MCP 源码、链接脚本、`.stldr` 或既有报告；仅新增本报告、`STM32N647_L2_Final_Inheritance_20260722.md`、构建目录 `Appli/build-l3a|b` 及 `reports/stageL3_20260722_141841/` 日志。
