# EMS_Projects — 用户自有 EMS 固件工程工作区

> 本文件夹是**用户自有工作区**，不属于 AlienTek vendor 原始资产。
> 由用户 2026-08-31 决定设立，构成对 Vendor Package 只读原则的显式例外
> （规则见仓库 `AGENTS.md` §11）。Vendor 其余内容（`Drivers/`、`Middlewares/`、
> `Projects/`、`FSBL/`、`STM32Cube_FW_N6_V1.4.0/` 等）仍保持只读。

## 用途与组织

Phase 3 起，全部 EMS 固件工程统一放在本文件夹下，**一个 EMS 工程一个独立子文件夹**：

```text
EMS_Projects/
├── <EMS 工程 1>/        # 独立 CubeMX/CMake 工程
├── <EMS 工程 2>/
└── ...
```

工程组织类比 `../Projects/`（典型结构：`.ioc` / `CMakeLists.txt` / `CMakePresets.json` /
`Appli/` / `STM32CubeIDE/` / `Binary/` 等）。

## 工程约定

- 工程自包含；命名建议 `EMS<序号>_<主题>`（如 `EMS01_CHIL_Predictor`）；
- 遵守 `../AGENTS.md`（vendor 子树规则）：
  - 不得把 `../../Tools/atk_stm32n6_cmake_prepare.py` 或 `../../Drivers/BSP` 复制进工程；
  - 保留 SoftwarePackage 整体目录结构；
  - `.ioc` 修改需用户明确授权；CubeMX 生成代码只改 USER CODE 区域；
- 工程入库策略遵循仓库 `AGENTS.md` §13：`Binary/*.hex`（vendor 源资产）可入库、
  `flash/*.hex` 生成产物不入库、`build*/` 不入库、`*.elf` / `*.map` 不入库；
- Build / Gate / Debug / Flash 阶段划分与硬件安全红线见仓库 `AGENTS.md` §11
  与 `docs/agent/STM32N6_ARCHITECTURE.md`。
