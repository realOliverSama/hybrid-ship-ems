# EMS01_CHIL_ECMS — Phase 3 预测型 A-ECMS 固件工程（CHIL 平台）

> **立项**：2026-08-31（用户决定）。Phase 3 EMS 固件工程，规划依据
> `docs/research_roadmap.md` §8；工作区规则见仓库 `AGENTS.md` §11 与 `../README.md`。
> **Status**: 工程骨架（尚未生成 CubeMX/CMake 工程，生成走 `prepare-stm32n647-project` 工作流）。

## 目标

把 Phase 2.5 出口形态（MLP-Q + 工况识别器 + G2 门控 + A-ECMS）移植到 STM32N647，
建成 CHIL 平台的控制器侧：NPU 跑 tflite 推理（模型热切换）、MCU 跑 ECMS 控制律，
与 PC 端 Simulink plant 步进同步闭环——支撑全模型对比大表与后续 trajectory-aware EMS 演进。

## 范围（对应 roadmap §8.3 实施路径）

1. AI 推理框架：ST Edge AI / X-CUBE-AI 集成，int8 tflite 部署（MLP-Q 26KB / TCN-f64 220KB / N-BEATS ~1.5MB）
2. 模型热切换机制：换权重文件即换预测模型（外部 NOR 256MB 存放模型库）
3. ECMS 控制律移植（CPU）：等效因子 + 网格穷举，1s 控制周期
4. G2 门控逻辑：工况识别器 int8（~50KB）+ 逆水段关断回退
5. CHIL 通信：与 Simulink plant 的接口（形式待 HIL 资料确认）+ 步进同步

## 约束（Do Not Change）

- ECMS 控制语义、ê 输入输出定义、门控逻辑与 Phase 2.5 出口形态一致；
- 不得为 MCU 本地方便重新定义 AI 输入输出语义 / 系统时间尺度（`docs/agent/STM32N6_ARCHITECTURE.md` §1）；
- 遵守 `../AGENTS.md`：不复制 prepare 脚本与 BSP 进工程、`.ioc` 修改需用户授权、
  生成代码只改 USER CODE 区域；
- Build / Gate / Debug / Flash 独立阶段，硬件操作红线见仓库 `AGENTS.md` §11。

## 结构（随工程生成补全）

```text
EMS01_CHIL_ECMS/
├── README.md            # 本文件
└── （CubeMX/CMake 工程生成后：.ioc / CMakeLists.txt / Appli/ / STM32CubeIDE/ / Binary/ ...）
```

## 验收锚点

- CHIL 复现 MLP-Q G2 基准（capture ≈ 0.589，S20 节油 ≈ 1.02%）→ 平台可信；
- 之后逐个换权重跑全模型矩阵（指标同口径：capture / FR-318 / fuel）。
