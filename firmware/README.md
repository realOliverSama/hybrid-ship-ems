# firmware/ — 固件工作区

> **入库策略**: 入库（生成产物如 `*.elf`、`flash/*.hex`、`build*/` 等按 `AGENTS.md` §13 排除）。

存放 STM32N6 固件相关内容：质量门禁脚本与 AlienTek 开发板资料/源码包。
涉及 `firmware/stm32n6/` 工作时必须先读取 `docs/agent/STM32N6_ARCHITECTURE.md` 及 vendor 子树内的 `AGENTS.md`。

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `gate/` | 目录 | 固件质量门禁：`fw_gate.py`（提交前必须执行，exit code = 0 才允许提交）与 `fw_gate_result.json`（机器可读结果） |
| `stm32n6/` | 目录 | AlienTek ATK-STM32N647 开发板资料与固件工作区：`01_入门教程`~`10_学习总结`（厂商教程与资料）、`03_原理图`、`07_硬件资料`、`08_STM32参考资料`、`04_SourceCode/SoftwarePackage/`（**vendor 原始包，原则上只读**；内含用户自有工作区 `EMS_Projects/`，见下）等 |
| `stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/` | 目录 | **用户自有 EMS 固件工程工作区**（2026-08-31 设立，Vendor 只读原则的显式例外）：Phase 3 起 EMS 工程每个一个独立子文件夹，组织类比 `Projects/`；规则见 `AGENTS.md` §11 与该目录内 README |

## Vendor 红线（摘自 `AGENTS.md` §11）

- `firmware/stm32n6/04_SourceCode/SoftwarePackage/` 原则上只读；**例外：`EMS_Projects/` 为用户自有工作区，其中文件可按工程需要修改**；
- 禁止 Mass Erase、猜测烧录地址、未授权修改 `.ioc` / Option Bytes / Boot / Security 配置；
- Build、Gate、Debug、Flash 是独立阶段，前一阶段完成不自动授权后一阶段。

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）。
vendor 子树内文件变动不经本 README 登记（vendor 包保持原样）。
