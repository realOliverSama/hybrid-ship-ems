# STEdgeAI 2.2.0 中间件自洽子集（路径 A，T-505）

本目录是 ST STEdgeAI **2.2.0** 安装自带 AI 中间件的工程内副本（ EMS01 自有资产，
可入库；引用一律相对路径，禁止指向用户目录绝对路径）。

## 来源与裁决

- 复制源：STEdgeAI 2.2.0 安装目录 `<STEdgeAI 2.2>/Middlewares/ST/AI/`（atonn 1.1.1，
  NetworkRuntime1020；生成代码自报 `atonn-v1.1.1-14-ge619e8606`）。
- 裁决依据：vendor 包内 `SoftwarePackage/Middlewares/AI`（atonn 1.0.0 /
  NetworkRuntime1000）与 stedgeai 2.2.0 CLI 生成的 `Model/*/network.c`
  （版本自检 `LL_ATON_VERSION_* == 1.1.1-14`）跨代不兼容。用户 + 主 Agent 裁决
  "路径 A"（2026-08-31）：EMS01 整体采用 2.2.0 自带中间件栈，本目录即其最小子集；
  vendor `Middlewares/AI` 保持只读、不引用。
- 最小源集 = STEdgeAI 2.2 官方 N6 参考工程
  `Projects/STM32N6570-DK/Applications/NPU_Validation/armgcc/Makefile` 的编译清单。

## 目录结构

| 路径 | 内容 |
|---|---|
| `Inc/` | stai / ai_platform / lite 算子头文件全集（93 个，SW 回退编译需要） |
| `Lib/GCC/ARMCortexM55/NetworkRuntime1020_CM55_GCC.a` | 官方预编译运行时库（ai_platform/stai SW 回退算子） |
| `Npu/ll_aton/` | LL_ATON 裸机运行时源码与头文件（编入工程的是官方 12 个 .c，见 `Appli/CMakeLists.txt` T-505 块） |
| `Npu/Devices/STM32N6XX/` | N6 器件层：`ATON.h`（寄存器映射）、`npu_cache.c/h`、`mcu_cache.c/h` |
| `LICENSE.txt` | ST 原始许可文件（保持随库分发） |

## 编译接线

见 `../../Appli/CMakeLists.txt` 的 `>>> EMS01 AI 接线（T-505）<<<` 标记块：
defines `LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6`、`LL_ATON_OSAL=LL_ATON_OSAL_BARE_METAL`、
`LL_ATON_RT_MODE=LL_ATON_RT_ASYNC`（默认）、`LL_ATON_SW_FALLBACK=1`（官方示例显式定义）；
链接 `Lib/GCC/ARMCortexM55/NetworkRuntime1020_CM55_GCC.a`。

本目录为库源快照（只被构建引用），不要在本目录内做工程修改；上游升级时应整体
重新复制并回归 `Model/*/network.c` 的版本自检宏。