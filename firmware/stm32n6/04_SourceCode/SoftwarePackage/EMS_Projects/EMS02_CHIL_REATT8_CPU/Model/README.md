# Model/ — NPU 部署模型链（每模型一个子目录）

本目录承载 STM32N6 NPU 部署链的每模型工作区：源 tflite 只读副本 + stedgeai 配置 +
生成脚本 + 生成代码与报告。入库策略：生成代码与报告入库（工程资产）；`st_ai_ws/`
编译工作区本地暂存。

## 子目录

| 目录 | 模型 | 角色 | 预测模型（架构 + 简要角色） |
|---|---|---|---|
| `mlp_q/` | MLP-Q G2（v1 部署先例） | 负荷分位数预测器（v3 收官态：mlpq 槽位，NOR 0x70200000） | MLP 302-64-32-3（q10/q50/q90 分位数头，21,571 参数，int8 26.2KB） |
| `workclf/` | workclf v1.1 | 工况识别器（两模式共用，不参与本次换型） | causal Conv1D×3 + MaxPool + GAP + 旁路融合（7 类，7,871 参数，int8 21.9KB） |
| `tcn_q/` | TCN-f64（FR-511 编译期换型，2026-09-02 换入） | 负荷分位数预测器（替换 mlpq 槽位，NOR 0x70200000 同基址） | TCN：7 残差块 causal dilated Conv1D（k=3，d=1,2,4,8,16,32,32，f=64）+ LastTimeStep + Dense(2→8) 旁路 + Dense(72→64) + Dense(64→3)（q10/q50/q90，165,851 参数，int8 219.7KB） |
| `tcn_q_v2/` | TCN-f64 **v2 重导出**（T-816b，2026-09-03；原生 dilated Conv2D，绕开 SpaceToBatch 分解图） | 4.0.1 stai 迁移（层②，2026-09-03）NPU 形态生成物；**回退资产**（tcnq 槽位已切 CPU 形态） | 同 TCN-f64 架构（v2 导出图），I/O 契约不变（1200/8/12） |
| `tcn_q_v2_cpu/` | TCN-f64 v2 **全 CPU 编译形态**（4.0.1，无 --st-neural-art；用户指令最终路线 2026-09-03） | 当前 tcnq 槽位实际部署（权重 611KB 编入 ELF，TCN 不再依赖 NOR 权重 hex；激活区 194KB 用户缓冲绑定；Lite 内核 CM55 执行） | 同 TCN-f64 架构（同源 tflite，md5 dcd11116e320a955dca7f01ca2886d59 = ../tcn_q_v2/），I/O 契约不变（1200/8/12） |
| `reatt8_q/` | REAT-T winner int8 **全 CPU 编译形态**（phase4-g，2026-09-22；4.0.1，无 --st-neural-art，`--name reatt8`） | Phase 4-F C2 板上预测服务（CMD_REATT；权重 1,432,556B 编入 ELF，激活 332,928B 用户缓冲；仅 Release 链接，Debug 为桩——见工程 README） | REAT-T（reatt-t018）：past TCN 6 块 causal dilated Conv1D（Cp=96）+ attention pooling + future 4 块 preLN Transformer encoder（df=128, h=4）+ 融合 + 共享单调分位头（1,012,835 参数；输出 (m,a,b)，分位变换在固件）；I/O 13200/720/360（归一化域喂入） |

## 4.0.1 stai 迁移注记（层②，2026-09-03）

- tcnq 槽位（默认构建）：TCN = tcn_q_v2_cpu **全 CPU 编译形态**（4.0.1 生成，
  无 --st-neural-art，权重编入 ELF，同步 run，用户激活区绑定；与 NPU 形态同
  stai API/同 I/O 契约，运行库同 NetworkRuntime1201 内含 Lite CPU 内核）；
  workclf = 4.0.1 重生成 NPU 形态（`--name workclf`，源 tflite/量化口径不变）
  → stai ext 生命周期；运行时栈 `../Middlewares/STEdgeAI/v401`
  （NetworkRuntime1201）；详见工程根 README、`tcn_q_v2_cpu/NOTE.md` 与
  `workclf/NOTE.md`。
- mlpq 槽位（可逆性出口）：2.2 LL_ATON 栈逐字节保留（NetworkRuntime1020）。
- 各模型 4.0.1/2.2 产物配对关系见各子目录 NOTE 与 `Appli/App/ems_npu_*.c` 头注。

各子目录细节见其内部 `NOTE.md` / README。模型选择依据（选模依据）：
phase25 三级选模（FR-317）Winner = MLP-Q G2（2026-08-30 裁决）；TCN-f64 为
FR-511 编译期换型候选（2026-09-02 用户裁决，价值定位 = 流程实证 + 更大模型闭环
评估，见 spec `ems01-chil-tcn64-swap` brainstorm/requirements）。

## 两级文档分工（AGENTS.md §20.1）

本 README 为目录索引（概括）；模型工作区详细结构见仓库根 `model/` 目录的
`README_Model.md` 体系与各工程内 NOTE。