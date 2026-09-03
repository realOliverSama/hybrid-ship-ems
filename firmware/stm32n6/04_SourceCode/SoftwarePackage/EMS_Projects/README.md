# EMS_Projects — 用户自有 EMS 固件工程工作区

> 本文件夹是**用户自有工作区**，不属于 AlienTek vendor 原始资产。
> 由用户 2026-08-31 决定设立，构成对 Vendor Package 只读原则的显式例外
> （规则见仓库 `AGENTS.md` §11）。Vendor 其余内容（`Drivers/`、`Middlewares/`、
> `Projects/`、`FSBL/`、`STM32Cube_FW_N6_V1.4.0/` 等）仍保持只读。

## 用途与组织

Phase 3 起，全部 EMS 固件工程统一放在本文件夹下，**一个 EMS 工程一个独立子文件夹**：

```text
EMS_Projects/
├── EMS01_CHIL_ECMS/          # MLP-Q G2 v3 基线（已恢复纯净态，git = HEAD @1308ab76）
├── EMS01_CHIL_ECMS_TCN64/    # TCN-f64 换型工程（batch 证据 + 流式 closure + NPU 实验记录）
└── Home/                     # vendor 模板空目录，非 EMS 工程
```

工程组织类比 `../Projects/`（典型结构：`.ioc` / `CMakeLists.txt` / `CMakePresets.json` /
`Appli/` / `STM32CubeIDE/` / `Binary/` 等）。

## 工程清单（2026-09-03 晨，最终两工程结构）

| 工程 | 角色 | 说明 |
|---|---|---|
| `EMS01_CHIL_ECMS` | MLP-Q G2 v3 基线（已恢复纯净态：git = HEAD，fw_gate 7/7，重建双镜像 nm 无 TCN 符号） | v1/v3 先例工程；本会话 TCN 换型改动已全部迁出至 TCN64 |
| `EMS01_CHIL_ECMS_TCN64` | **TCN-f64 换型工作区**（batch 换型验证态证据 + 流式 closure 实现 + NPU 频率实验记录合一） | 用户工程隔离规则承接体；流式与 batch 为同一模型演进阶段，证据合并在 `Host/results/tcn_swap/`；spec `ems01-chil-tcn64-swap` |

> 注：早期曾计划"batch 与流式分列两工程"，实际实施中流式 closure 直接在 TCN64 内
> 演进（batch 证据即流式的基线），故最终为两工程结构。

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
