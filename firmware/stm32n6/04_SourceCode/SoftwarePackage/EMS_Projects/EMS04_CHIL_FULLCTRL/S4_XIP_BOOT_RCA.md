# S4 XIP/flash boot 不启动 RCA 与修复方案（P4）

> 2026-09-29 主 Agent · 依据：十一续/十续 §18 日志、NOR 读回档案（/tmp/nor_backup/）、
> EMS04 构建系统源码、FSBL 例程源码。**本文档为纯 host 证据 RCA + 修复提案；
 * 烧写/断电属用户批准点，本阶段不执行。**

## 1. 现象（冻结记录）

- 十续：烧 `build/flash/EMS01_CHIL_ECMS_Appli.hex` @0x70100400（hex 生成 17:23:48）
  → flash boot **能跑**（PING 活），TICK_CTRL 13960B 大帧静默（旧 MAX_PAYLOAD 守卫）。
- 十一续：重烧修正版（含 proto.h MAX_PAYLOAD 13960 修正）→ 拨 flash boot **黑屏**，
  PC 停 boot ROM 0x1800350E；期间叠加 NOR 死态（loader 会话叠加，断电已恢复）。

## 2. 根因链（两条独立缺陷叠加）

### D1 — build/flash hex 被任意 Release 形态覆盖（烧串错版风险，现已实锤）

`Appli/CMakeLists.txt:209` 的 `atk_release_flash_files` 是 **ALL target**：
任何 Release 配置的构建目录（含 SRAM 布局的 S4Smoke/RamRel/BdDiagUart）构建时都会
把 `build/flash/EMS01_CHIL_ECMS_Appli.hex` 覆盖为**该形态的镜像**。

- 2026-09-29 实测：当前 build/flash hex 首记录地址 = **0x34000400（SRAM 布局）**
  ——是 S4Smoke 构建的副产物。若误烧此文件到 NOR，flash boot 必砖。
- 十续首烧的 hex（17:23:48）实为 fw_gate 默认 flags 的 Release XIP 构建
  （无 S4_FULLCTRL/PRED_PARITY），故能 boot（PING 活），但无 TICK_CTRL 服务。

### D2 — XIP 自杀：PRED_PARITY 的 NOR/XSPI2 重初始化在 XIP 执行态下引爆

- `CMakeLists.txt` 顺序缺陷：`EMS04_S4_FULLCTRL=1` 对 `EMS04_PRED_PARITY=1` 的
  FORCE 发生在**链接脚本选择块之后**（line 337 vs line 93）→ S4Full 构建实际得到
  **XIP 布局**（ROMxspi2_RAMxspi1）而非预期的 SRAM PREDPARITY 布局。
- 该 XIP 镜像的 main() USER CODE 2 在 `EMS04_PRED_PARITY=1` 下进入
  `MX_XSPI2_Init() + NORFlash_Init + EnableMemoryMappedMode` 分支（main.c:269-288）
  ——**在 CPU 正从 XSPI2 NOR 取指（XIP）时重初始化 XSPI2**，memory-mapped 取指窗口
  崩塌 → 启动自杀（黑屏）。代码注释原文已预见"Release（ROMxspi2）由 boot 链完成
  XSPI2 memory-mapped，不入此分支"，但守卫写成 `defined(EMS04_PRED_PARITY)` 时
  PRED_PARITY 与 XIP 的组合未被排除（历史上 PRED_PARITY 恒为 SRAM 直载）。

### 证据链

| 证据 | 内容 | 指向 |
|---|---|---|
| /tmp/nor_backup/post_verify.bin（首烧读回 17:27） | 向量表 MSP=0x34200000 Reset=**0x70107071**（XIP 布局） | 首烧=gate 默认 Release XIP → 能 boot ✓ |
| /tmp/nor_backup/post_verify2.bin（修正版读回 20:04） | Reset=**0x70107991**（XIP 布局）；与 S4Full ELF(18:40) 扁平化比对 65532/65536 匹配、全程仅 44B 差 | 修正版=S4Full 族 XIP 镜像（S4_FULLCTRL=1 → PRED_PARITY=1）→ 含自杀分支 |
| 当前 build/flash hex | 首地址 0x34000400（SRAM 布局） | D1 实锤 |
| EMS02 main.c 同款 NOR init | SRAM 形态下安全（NOR 未用于取指） | 自杀仅在 XIP 形态发生 |
| **NOR 现场活读（2026-09-29，probe-rs AHB 后台读，无 extloader 会话）** | 0x70100400：MSP=0x34200000 Reset=**0x70107991** —— 与 post_verify2.bin 逐字节一致（修正版 S4Full XIP 镜像仍在 NOR 原位）；0x70000000：`STM2` boot 头完好 | 现场与读回档案闭合；FSBL 区完好 |

「旧能跑新不能跑」完全解释：旧=默认 flags（无 PRED_PARITY 分支）；新=S4Full XIP
（含自杀分支）。与擦除历史/FSBL 校验无关（②中 NOR 死态为独立的 loader 会话叠加
问题，断电已恢复；boot ROM 0x1800350E 停留发生于该死态窗口）。

## 3. FSBL 接口事实（vendor FSBL 例程源码）

- `stm32_extmem_conf.h`：EXTMEM_MEMORY_BOOTXIP=EXTMEMORY_1，**EXTMEM_XIP_IMAGE_OFFSET
  = 0x100000** → FSBL 跳转 XIP 镜像于 0x70100000+0x400=**0x70100400**（固定）。
- FSBL 流程：XSPI1(HyperRAM MM) + XSPI2 + EXTMEM_MANAGER(BOOTXIP) → BOOT_Application。
  XIP 交接后 NOR 经 XSPI2 memory-mapped 窗（0x70000000 基址）可读——N1@0x70B00000 /
  N2@0x70A00000 权重池在 XIP boot 态下对 appli 可读（窗口覆盖全 NOR；烧写验证时实证）。
- 结论：appli 留在 **0x70100400** 则 FSBL 零改动；挪 0x70C00000（NOR_PLAN §2 草案
  原案）需改 FSBL offset 并重烧 FSBL（风险更高，且 0x70100400 槽位十续起已实际
  承载 EMS04 镜像并有备份纪律）。

## 4. 修复方案（候选；烧写执行 = 用户批准点）

| # | 修复 | 内容 |
|---|---|---|
| F1 | main.c XIP 守卫 | NOR/XSPI2 重初始化分支增加 XIP 执行排除：新增 `EMS04_XIP_BOOT` 编译定义（CMake 按链接脚本形态注入），XIP 形态跳过 MX_XSPI2_Init/NORFlash 重初始化（boot 链已 memory-mapped；权重池直读） |
| F2 | CMake 顺序修正 | S4_FULLCTRL→PRED_PARITY 的 FORCE 前置到链接脚本选择块**之前**；新增显式产品形态 `EMS04_S4_XIP=1`（Release + ROMxspi2_RAMxspi1 + FULLCTRL + XIP_BOOT），与 SRAM 直载形态（S4Smoke=LRUN_PREDPARITY）并列清晰 |
| F3 | flash hex 守卫 | `atk_release_flash_files` 仅在 ROM（XIP）链接形态下产出 build/flash hex；SRAM 形态不覆盖（消除 D1 误烧通道） |
| F4 | 镜像地址 | 维持 0x70100400（FSBL 零改动；槽位已有备份纪律）；NOR_PLAN §2 的 0x70C00000 原案标记为"若需双镜像并存再启用（需 FSBL offset 变更）"——**此偏离需用户在批准烧写时一并确认** |

## 5. 验证链（批准后执行，顺序固定）

```text
① fw_gate 全过（含新 S4XIP 形态构建）
② NOR 备份：0x70100400 区 4MB 读回存档（sha256 记录）
③ 烧写 S4XIP hex @0x70100400（仅该段；--verify）
④ 后验读回逐字节比对（段间隙语义差异登记）
⑤ 断电 → 拨 flash boot → 上电
⑥ PING + s4_c2_smoke 100 拍零 NACK（=产品形态等价证据）
⑦ N1/N2 权重池读取实证（REATT 推理正常 = 0x70A/0x70B 窗口可读）
⑥ 更新报告 6.5 偏差③ + §18 记账
```

红线：0x70A/0x70B/0x703 禁碰；烧写前备份；OB/OTP 零触碰。

## 6. 状态

- [x] RCA（host 证据闭环：D1+D2）
- [x] 修复方案 F1-F4 成文
- [ ] **用户批准烧写 + 镜像地址确认（0x70100400 vs 0x70C00000）→ park**
- [ ] 批准后按 §5 执行
