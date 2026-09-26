# T-505 交接证据（EMS01 CHIL ECMS — 路径 A 中间件 + CMake AI 集成 + NPU 后端 + 构建）

任务状态：**全部验证项 PASS**（详见下文）。本文件为机器可读验证证据汇总，供主 Agent 轻量验收。
生成时间：2026-08-31（T-505 worker）；构建工具：Cube bundle gnu-tools-for-stm32 14.3.1+st.2，
prepare 脚本 atk_stm32n6_cmake_prepare.py v3.4.0，`--build all`（Debug + Release）。

---

## 0. 路径 A 裁决执行摘要

- vendor `SoftwarePackage/Middlewares/AI`（atonn 1.0.0 / NetworkRuntime1000）**未引用、未修改**。
- 工程内新增 `Middlewares/STEdgeAI/`（6.4 MB，STEdgeAI 2.2.0 安装目录 `Middlewares/ST/AI/`
  的逐位一致副本，`diff -rq` / `cmp` 校验通过），Appli CMake 全部以
  `${CMAKE_CURRENT_LIST_DIR}/../Middlewares/STEdgeAI` 相对路径引用。
- 生成代码版本自检：`Model/*/network.c` 顶部 `LL_ATON_VERSION_MAJOR/MINOR/MICRO/DEV == 1.1.1-14`
  （STAI-2.2 / atonn-v1.1.1-14-ge619e8606），与 2.2 中间件同代。

## 1. 构建（Validation ①）

```
atk_stm32n6_cmake_prepare.py --build all EMS01_CHIL_ECMS  →  exit 0
Verified Debug  ELF address: 0x34000400   （RAM region 120320 B / 2047 KB）
Verified Release ELF address: 0x70100400  （ROM 63780 B, RAM 8584 B / 2 MB）
Verified Release HEX address: 0x70100400  （Appli/build/flash/EMS01_CHIL_ECMS_Appli.hex）
```

- Debug/Release **0 error**。
- 警告（3 处，均非本任务代码）：
  - `ld: warning: ... has a LOAD segment with RWX permissions` ×2（Debug/Release 链接器
    既有基线行为，T-501 起即存在，fw_gate 仅记录不设限）；
  - `Middlewares/STEdgeAI/Npu/ll_aton/ecloader.c:662: format '%u' expects ... ECFileEntry
    {aka long unsigned int}`（ST 2.2 上游源码自带的 -Wformat 警告，1.0 也有的同类问题，未改动源码）。
- 关键编译定义（生成代码命令行实拍）：`-DEMS_FW_MODE=EMS_FW_MODE_NPU
  -DHAL_CACHEAXI_MODULE_ENABLED -DLL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL
  -DLL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6 -DLL_ATON_RT_MODE=LL_ATON_RT_ASYNC
  -DLL_ATON_SW_FALLBACK=1 -DBSP_UART_RX_DISABLE`；链接 `-u _printf_float`（%.6f 遥测）。
- **REF 后端回归（非 gate 必需，附加验证）**：scratch 配置（独立 build 目录，工具链
  14.2.rel1，与 gate 路径 14.3.1 bundle 不同）`-D EMS_FW_MODE=REF` 配置+编译+链接
  **0 error**，仅编 ems_infer_ref.c（无 NPU 源）；证明公共块改动（-u _printf_float、
  源/含路径变量重组）不影响 REF 分支。scratch 目录已清理，presets 产物未变动。

## 2. ALLOC 段契约（Validation ②，readelf -S；权重为 NOR 绝对寻址不占段）

工具：arm-none-eabi-readelf 14.3.1；ALLOC（含 NOBITS）段全部位段落：

```
Debug（契约 [0x34000400, 0x34200000)）：
  .isr_vector      0x34000400 +0x34c   PASS（向量表=0x34000400 精确命中）
  .text            0x34000750 +0x164cc PASS
  .rodata          0x34016c20 +0x4ab4  PASS
  .ARM             0x3401b6d4 +0x8     PASS
  .preinit_array   0x3401b6dc +0x0     PASS
  .init_array      0x3401b6dc +0x4     PASS
  .fini_array      0x3401b6e0 +0x4     PASS
  .gnu.sgstubs     0x3401b700 +0x20    PASS
  .data            0x3401b720 +0x204   PASS
  .tdata/.tbss     0x3401b924 +0x0     PASS
  .noncacheable    0x3401b924 +0x4     PASS
  .bss             0x3401b940 +0x16bc  PASS
  ._user_heap_stack 0x3401cffc +0xa04 PASS
Release（契约 [0x70100400, 0x72000000) ∪ [0x34000000, 0x34200000)）：
  .isr_vector      0x70100400 +0x34c   PASS
  .text            0x70100750 +0xabb4  PASS
  .rodata          0x7010b308 +0x47d4  PASS
  .ARM             0x7010fadc +0x8     PASS
  .preinit_array   0x7010fae4 +0x0     PASS
  .init_array      0x7010fae4 +0x4     PASS
  .fini_array      0x7010fae8 +0x4     PASS
  .gnu.sgstubs     0x7010fb00 +0x20    PASS
  .data            0x34000000 +0x204   PASS
  .tdata/.tbss     0x34000204 +0x0     PASS
  .noncacheable    0x34000204 +0x4     PASS
  .bss             0x34000220 +0x1568  PASS
  ._user_heap_stack 0x34001788 +0xa00  PASS
OVERALL: PASS（Debug 14 段 / Release 14 段，无越界段）
```

模型权重不进 ELF：`Model/*/network.c` 将权重烘为
`ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(0x70200000UL/0x70400000UL + offset)` 绝对寻址
（NetworkRuntime 常量池 xSPI2 1 MB@0x70200000 mlpq / @0x70400000 workclf，
`my_mpools/stm32n6.mpool` 同口径），故 ELF 中无大权重段。

## 3. 运行时符据（Validation ④，nm/readelf）

- `NPU0_IRQHandler`（= 宏 2.2 展开：`ATON_STD_IRQHandler` → `CDNN0_IRQHandler` →
  `NPU0_IRQHandler`）在 ELF 中**恰 1 个强定义** `340124a4 T NPU0_IRQHandler`
  （来源 `Middlewares/STEdgeAI/Npu/ll_aton/ll_aton_runtime.c`；T-505 修掉了 1.0 代
  it.c 桥接函数，见 §5）。
- 源编译的 2.2 LL_ATON 运行时全部解析（无 undefined）：
  `LL_ATON_RT_Main @3400f850/Release 70106850`、
  `LL_ATON_Set_User_Input_Buffer_Mlpq @3400f770`、
  `LL_ATON_EC_Network_Init_Mlpq @3400f850`、
  `mcu_cache_clean_range @34012c5c`、`npu_cache_init @34012ce0`。
- `nm -u`（Debug）：**空**——无未解析符号；`powf @34013aa0 T` 由尾随 `-lm` 解出
  （1020 库内 core_convert.o 引用，CMake 隐式 -lm 顺序在归档前，已在 CMake 块显式补尾随 -lm）。
- `NetworkRuntime1020_CM55_GCC.a`（工程内副本，cmp 一致）定义 stai_* 族
  （stai_get_format_name/stai_init_format/…）；生成代码不引用 stai 符号，归档整体
  作为 design 要求的链接期安全网被链接（SW 回退算子经 gc-sections 后无成员进 ELF，
  与官方 hello_world 行为一致）。ELF 无 "NetworkRuntime1000" 残留符号或引用。
- 1000 残留检查：`nm -u` 无 LL_ATON 未解析符号；CMakeLists 已无任何
  `Middlewares/AI`/`NetworkRuntime1000` 引用（全文搜索 0 命中）。

## 4. fw_gate（Validation ③）

```
FW_GATE_ALLOW_IOC=1 python3 firmware/gate/fw_gate.py <此工程>  →  exit 0，全部 7 项 PASS
  debug_build    PASS  exit=0; Verified Debug ELF address: 0x34000400
  release_build  PASS  exit=0; Verified Release HEX address: 0x70100400
  debug_layout   PASS  11 个 ALLOC 段在 [0x34000400,0x34200000)（gate 计数不含空段）
  release_layout PASS  11 个 ALLOC 段在两契约区间内
  ioc_unchanged  PASS  FW_GATE_ALLOW_IOC=1（用户已授权 .ioc 修改）
  no_forbidden_copy PASS 无 prepare 脚本 / BSP 副本 —— $P/Middlewares/STEdgeAI 实测不触发
  warnings       PASS  仅记录
结果 JSON：firmware/gate/fw_gate_result.json（overall: true / 2026-08-31）
```

## 5. 前棒草稿修正清单（对照 2.2 头文件/官方示例逐项审计）

| # | 文件:位置 | 问题 | 修正 |
|---|---|---|---|
| 1 | `Appli/Core/Src/stm32n6xx_it.c` USER CODE 1 | 前棒桥接函数 `NPU0_IRQHandler`→`extern ATON_STD_IRQHandler` 基于 **1.0 代语义**（ATON_STD_IRQHandler 为独立符号）。2.2 中它是宏（ll_aton_platform.h:477）展开为 `CDNN0_IRQHandler`，新 HAL 下即 `NPU0_IRQHandler`，ll_aton_runtime.c 自带强符号 → 多重定义 + 自调用 | **删除桥接**，留注释说明 2.2 代际绑定机制（USER CODE 区内，不动生成区） |
| 2 | `Appli/Core/Src/main.c` USER CODE 0 | 前棒 `npu_config()` 只开 NPU 时钟（991 同款），缺 2.2 `NPU_Config`（misc_toolbox.c:249-258）对 **CACHEAXI 时钟+复位释放** 的要求；本项目 .ioc 无 RAMCFG/CACHEAXI 外设配置 | `npu_config()` 增补 `__HAL_RCC_CACHEAXI_CLK_ENABLE/FORCE_RELEASE_RESET`（npu_cache_init 前置条件，2.2 同段） |
| 3 | `Appli/Core/Src/main.c` USER CODE 0/2 | **NPU 激活 RAM 供电缺失**：991 依赖 `MX_RAMCFG_Init`(main.c:317)+`HAL_RAMCFG_EnableAXISRAM(SRAM3-6)`(main.c:360-363)；本项目 .ioc 无 RAMCFG 外设，2.2 的实现为 `system_init_post`（misc_toolbox.c:330-345，CMSIS 直写 MEMENR + SRAMSD 位清零） | 新增 `npu_ram_init()` = 2.2 `system_init_post` NPU 必需子集（MEMENR: AXISRAM3-6EN+CACHEAXIRAMEN；RAMCFG_SRAM3..6_AXI->CR &= ~RAMCFG_CR_SRAMSD；SRAM2 非 NPU 池不动；MSCR 缓存激活位不加——基线工程 CPU cache 已在用，两参考一致不冲突），在 NPU 分支最先调用（main.c:209） |
| 4 | `Appli/Core/Src/main.c` 注释 | `Model/*/my_mpools` 注释中 `*/` 提前终止块注释导致编译错 | 注释改写（"Model 各子目录 my_mpools/…"），行为零变化 |
| 5 | `Appli/CMakeLists.txt` AI 块 | 引用 vendor `Middlewares/AI`（1.0.0/NetworkRuntime1000）路径；源集含 1.0 时代 `ll_aton_n64.c`（N64 平台）/`ll_aton_profiler.c`（官方 N6 清单不含）；链接 1000 库 | 全块改为 `$P/Middlewares/STEdgeAI`（路径 A）：官方 12 源（NPU_Validation/armgcc/Makefile 清单）+ npu_cache.c/mcu_cache.c + Inc/Npu/ll_aton/Npu/Devices include + 链接 `NetworkRuntime1020_CM55_GCC.a` + 尾随 `-lm`（归档内 powf 需在归档后再次链接 libm）+ 显式 `-DLL_ATON_RT_MODE=LL_ATON_RT_ASYNC`（2.2 示例同显式）+ 共通 `-u _printf_float` |
| 6 | （审计通过，未改） | `ems_infer_npu.c` 四步序列 = 2.2 官方 bare-metal 等价（991 app.c:155-158 同序）：`Set_User_Input_Buffer(_Mlpq/_Workclf)` → `SCB_InvalidateDCache_by_Addr(out)` → `Set_User_Output_Buffer` → `LL_ATON_RT_Main(&NN_Instance_x)`；`LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Mlpq/Workclf)` 声明与 ems_npu_*.c 的 10 个改名符号一一对应（ll_aton_rt_user_api.h:100 / ll_aton_NN_interface.h:331 NN_PROTOS 恰 10 符号）；32B 对齐缓冲 vs 生成代码 `buffer % 32` 校验一致；workclf num 0=8B 旁路/1=120B Pdec 与生成 network.c `Set_*_buffer(num)` 校验（num=0→size≥8，num=1→size≥120）一致 | 保留 |
| 7 | （审计通过，未改） | 生成 network.c 内嵌 `LL_ATON_Cache_MCU_Clean/Invalidate_Range`（--cache-maintenance）经 `ll_aton_caches_interface.h` inline 包装 `mcu_cache.c` —— mcu_cache.c 已编入（CMake）；`NPU_CACHE_IRQHandler`（npu_cache.c）与本地 CMSIS 向量名 `CACHEAXI_IRQHandler` 不同名，中断未使能、无符号冲突（ll_aton 全源码 0 处引用该 IRQ），向量槽保留 weak Default_Handler | 保留 |

## 6. 初始化序列对照表（Validation ⑤）

参照 A = STEdgeAI 2.2 官方 N6 示例 `NPU_Validation`（Core/Src/main.c + misc_toolbox.c）；
参照 B = vendor 991（Projects/99_Applications/991_AI_People_Detection/Appli/Core/Src/main.c）；
EMS01 = 本工程 `Appli/Core/Src/main.c`。

| 步骤 | 参照 A（2.2 N6 示例） | 参照 B（991 main.c） | EMS01 落点 |
|---|---|---|---|
| 时钟配置 | `system_clock_config`（main.c:92-95 区） | `system_clock_config()` main.c:696（DEBUG） | `sys_clock_config_debug()` main.c:155（BSP SYS，DEBUG）；时钟树零改动 |
| XSPI1 HyperRAM | BSP_XSPI_RAM_Init + MM（main.c:105-106） | MX_XSPI1_Init:703 + HyperRAM_Init/MM:717-721 | MX_XSPI1_Init():162（生成版）+ HyperRAM_Init/EnableMemoryMappedMode（SysInit USER CODE） |
| **NPU 激活 RAM 供电** | `system_init_post` misc_toolbox.c:330-343：MEMENR AXISRAM3-6+CACHEAXIRAM ≪SRAM2≫、RAMCFG_SRAM2..6 SD 清零、MSCR ICACTIVE/DCACTIVE | `MX_RAMCFG_Init`:317（HAL_RAMCFG_Init SRAM3-6）+ `HAL_RAMCFG_EnableAXISRAM` :360-363 | `npu_ram_init()` main.c:110-120（调用 :209）= A 的 NPU 子集（略 SRAM2/MSCR：SRAM2 非 NPU 池；CPU cache 基线已在用；.ioc 无 RAMCFG 外设，991 的 HAL 路径不可用） |
| XSPI2 + NOR mem-mapped | main.c:102-119（BSP NOR init + EnableMemoryMappedMode） | MX_XSPI2_Init:704 + NORFlash_Init:713 + HyperRAM/NOR MM :718-724（DEBUG，先于 NPU 配置） | MX_XSPI2_Init():192（CubeMX 生成版，.ioc 已授权增补 NPU RIF 后重生成）+ NORFlash_Init:193 + EnableMemoryMappedMode:198（DEBUG 分支，SystemIsolation 之后、npu_config 之前 = B 实序） |
| RIF（NPU 从端 RISUP + 主端 RIMC） | RISAF_Config()/RIF 生成配置 | `SystemIsolation_Config()` main.c:374-… | `SystemIsolation_Config()` main.c:176（生成区 NPU RISUP = RIF_RISC_PERIPH_INDEX_NPU SEC+PRIV）+ USER CODE RIF_Init 1（main.c:356 附近）：RIMC_MasterConfig{CID1, SEC\|PRIV} ×NPU（991 main.c:392 同款，前棒已写，本次复核未动） |
| NPU/CACHEAXI 时钟+复位 | `NPU_Config` misc_toolbox.c:249-258：NPU clk/FR/RR + **CACHEAXI clk/FR/RR** | `npu_config()` :849-854（仅 NPU；CACHEAXI 由其 BSP/FSBL 既有配置代做） | `npu_config()` main.c:92-105 = B 同款 NPU 三行 + A 同款 CACHEAXI 三行（:98-100，新增） |
| NPU cache | `npu_cache_init` :263 + `npu_cache_enable` :266 | `npu_cache_config()` :863-868（同款） | `npu_cache_config()` main.c:122-126（调用 :211，原样保留） |
| DWT CYCCNT | （2.2 示例用 ARM_PMU，不适用本项目 DWT 约定） | （991 无 DWT） | main.c:183-184：DEMCR TRCENA + DWT_CTRL_CYCCNTENA（NFR-505 EMS01 新增，代码使能） |
| 顺序约束 | A：RAM 供电（system_init_post）→ NPU_Config → XSPI/NOR MM | B：XSPI2+NOR MM → SystemIsolation → npu_config → npu_cache_config | 供电（209）→ 时钟复位（210）→ cache（211）；XSPI2/NOR（192-198）先于 NPU 配置 = B 实序，A 的供电位次一致（供电先于一切 NPU 访问） |

注：991 的 `iac_config`/`low_power_clock_config` 为 991 应用附属（IAC 报警、低功耗时钟），
非 NPU 必需，按前棒裁决不移植；ThreadX 侧 `tx_application_define` 不移植。

## 7. §20.2 绝对路径扫描（Validation ⑥）

`/Users/|/home/|[A-Za-z]:[\\/]` 扫描 $P 全部文本（排除 build/、st_ai_ws/、二进制/库文件、URL/env）：

- **本任务产物 0 命中**：`Appli/CMakeLists.txt`（全部 `${CMAKE_CURRENT_LIST_DIR}` 相对路径、
  EMS 工程内 `../Middlewares/STEdgeAI`）、`main.c`、`stm32n6xx_it.c`、`App/*.c/h`、
  `Middlewares/STEdgeAI/**`、`Middlewares/STEdgeAI/README.md` 均无绝对路径。
  （唯一"命中"为 `Npu/ll_aton/ll_aton_lib.c:2397` 注释字符串 `"after:\n"` 中 `r:\`
  恰好匹配 Windows 盘符正则的误报，非路径。）
- **既有产物命中（非本任务引入，Do-Not-Change 冻结）**：
  - `Model/*/network.c:25-31` 等：STEdgeAI 生成头注释（BUILD_DIR/--load-mpool-file 等
    工具自证路径，T-502 冻结产物，禁止重新 generate）；
  - `Model/*/analyze.log、generate.log、*_report.txt、st_ai_output/*.json`：T-502 工具日志证据；
  - `STM32CubeIDE/Appli/.cproject`：CubeMX 生成的 `${workspace_loc:...}` CMake 变量语法（非绝对路径）。

## 8. 中间件复制清单（Validation 目标 ①）

| 目标（$P/Middlewares/STEdgeAI/） | 来源（STEdgeAI 2.2 安装，只读复制源） | 校验 |
|---|---|---|
| `Inc/`（93 文件，864 KB） | `AI/Inc/` 全集（ai_*/stai_*/lite_*/layers_*） | `diff -rq` 空 |
| `Lib/GCC/ARMCortexM55/NetworkRuntime1020_CM55_GCC.a`（1.2 MB） | 同路径原件 | `cmp` 一致 |
| `Npu/ll_aton/`（22 文件，1.0 MB；编入工程的 12 源见 §9） | `AI/Npu/ll_aton/` 整目录 | `diff -rq` 空 |
| `Npu/Devices/STM32N6XX/`（ATON.h 3.3MB + npu_cache.c/h + mcu_cache.c/h，3.4 MB） | `AI/Npu/Devices/STM32N6XX/` | `diff -rq` 空 |
| `LICENSE.txt`（4.7 KB） | `AI/LICENSE.txt` | 已复制 |
| `README.md`（本任务撰写：来源、裁决、最小源集、接线说明） | — | 新增 |
| 合计 | — | 6.4 MB / 122 文件 |

## 9. CMake AI 接线块终态（Validation 目标 ②）

`Appli/CMakeLists.txt` 内 `>>> EMS01 AI 接线（T-505，路径 A 修订）<<<` 标记块（ATK AUTO 三块未触碰、CRLF 布局为 LF 的原文件格式保持）：

- 后端开关：`EMS_FW_MODE = REF|NPU`（cache 变量，非法值 FATAL_ERROR）。
- 共通：`App/` include + ems_app/ems_ctrl/ems_params 源；`BSP_UART_RX_DISABLE`；
  `-ffp-contract=off`；`-u _printf_float`（新增，%.6f 遥测需要）。
- NPU 分支：
  - defines：`EMS_FW_MODE=EMS_FW_MODE_NPU`、`LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6`、
    `LL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL`、`LL_ATON_RT_MODE=LL_ATON_RT_ASYNC`（2.2 示例同显式）、
    `LL_ATON_SW_FALLBACK=1`（官方示例显式、默认 0）、`HAL_CACHEAXI_MODULE_ENABLED`
    （本地 hal_conf.h 未启用模块，经 -D 启用，不动 hal_conf.h/.ioc）；
  - includes：`$P/Middlewares/STEdgeAI/{Inc, Npu/ll_aton, Npu/Devices/STM32N6XX}`（新增 Npu/ll_aton，
    官方 Makefile `-I$(ATONN_RT_PATH)` 同款，ll_aton 头链必需）+ 主包 `STM32Cube_FW_N6_V1.4.0/…/HAL Inc`
    （仅补 cacheaxi.h，append 于 mx-generated 之后，本地 HAL 头优先）+ `Drivers/BSP/NORFlash` + `App/`；
  - sources：App 的 ems_infer_npu.c/ems_npu_mlpq.c/ems_npu_workclf.c + NORFlash BSP 4 文件 +
    ll_aton 官方 12 源（ecloader/ll_aton/ll_aton_cipher/ll_aton_dbgtrc/ll_aton_debug/ll_aton_lib/
    ll_aton_lib_sw_operators/ll_aton_rt_main/ll_aton_runtime/ll_aton_util/ll_sw_float/ll_sw_integer，
    不含 ll_aton_n64.c/ll_aton_profiler.c）+ `Npu/Devices/STM32N6XX/{mcu_cache,npu_cache}.c` +
    主包 `stm32n6xx_hal_cacheaxi.c`；
  - link：`Middlewares/STEdgeAI/Lib/GCC/ARMCortexM55/NetworkRuntime1020_CM55_GCC.a` + 尾随 `-lm`。
- REF 分支：不变（T-503 参考后端）。

## 10. 遗留风险与建议（交接 T-506/T-507）

1. `powf` 由 `core_convert.o`（1020 库）触发、经尾随 `-lm` 解析——SW 回退算子若在运行时被
   实际调用（权重/算子异常路径），其数值精度路径含 libm；正常 int8 NPU 路径不触达。
2. `LL_ATON_SW_FALLBACK=1` 按官方示例显式开启；板级 NPU 失败时的软件回退路径依赖 NOR
   常量池与 RAMCFG/SRAM 供电齐全（npu_ram_init 已覆盖）。
3. it.c 的 `NPU0_IRQHandler` 桥接已删（2.2 由 ll_aton_runtime.c 强符号直接服务向量表）；
   若未来回退 1.0 代中间件需恢复桥接（现路径 A 下为错误）。
4. DWT CYCCNT 在 M55 上为 DWT 周期计数器（core_cm55.h 存在）；板上实测 infer_us 数值有效性
   归 T-507。ST 2.2 示例用 ARM_PMU——若 T-507 实测计数为 0，可将 us 源换 PMU（改动仅
   ems_infer_npu.c 单函数）。
5. ecloader.c 的 -Wformat 警告为上游 2.2 源码自带（ECFileEntry=long unsigned vs %u），
   未改动上游；fw_gate 记录 0/未达阈值。
6. Release RAM 使用仅 8.6 KB / ROM 63.8 KB：权重与 ems_params 常量按契约落 ROM/NOR，
   NPU 激活缓冲在 AXISRAM3-6（运行期，不在 ELF 段）——与 my_mpools 布局一致。