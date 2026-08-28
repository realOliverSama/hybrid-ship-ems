# model/ — Simulink 建模工作区

> **入库策略**: 入库（`slprj/`、`*.slxc`、`*.slx.bak`、`.venv*/`、codegen 等生成产物不入库）。

存放船舶混合动力 EMS 的 Simulink 模型与配套 AI 训练资产。每个模型目录均为**脚本驱动**的可复现工作区
（禁止 GUI 手工修改形成不可复现状态，见 `AGENTS.md` §9），内部结构统一：

```
<模型目录>/
├── README_Model.md      模型说明（定位、目录结构、使用流程）
├── 项目结构.txt         结构速览
├── config/              ★ 参数与工况唯一来源（Model_*_Parameters.m / Scenarios.m / Gate 阈值）
├── scripts/             ★ 01~07 编号流水线脚本（真正的"源代码"）+ +model/ 工具函数包
├── model/               .slx 模型快照（可由脚本完全重建，当构建产物对待）
├── python/              AI 侧资产（train_*.py 训练脚本、export/ 导出 .keras/.tflite）
├── scenarios/           工况数据（预留/生成）
├── datasets/            航次数据 raw/ 与 train/validation/test 划分
├── results/             烟雾测试与工况结果（T0*/、ai_compare/、gate_result.json）
├── reports/             审计与对比报告（*.md、*.csv、图）
├── legacy_reference/    历史模型只读副本
└── .venv_tf/            Python 训练环境（不入库）
```

## 内容清单

| 条目 | 类型 | 说明 |
|---|---|---|
| `simulink/` | 目录 | **主力模型工作区**——MVP 最小闭环验证模型 + ModelA/ModelB（静止起步→巡航→固定航程；MLP 负荷预测 AI 闭环验证；07 步流水线 + 08/09 ModelB 扩展脚本） |
| `simulink_modelC/` | 目录 | Model C——定时前馈 EMS 研究副本（`HybridShip_ModelC_DCBus.slx`） |
| `simulink_modelC_(oracle_ms)/` | 目录 | Model C 的 oracle 多步（multi-step）变体研究副本（`HybridShip_ModelCms_DCBus.slx`） |
| `simulink_modelD/` | 目录 | Model D——定时前馈 EMS 研究副本（`HybridShip_ModelD_DCBus.slx`，含 `experiments/` 子目录）；Phase 2 后冻结（纯 Phase 2 状态） |
| `simulink_modelD_(EMS_NN_AECMS)/` | 目录 | Model D 本体 + 神经网络预测型 A-ECMS（Phase 2.5 起，TCN 主推多分位数预测 + NPU 导向；自 `simulink_modelD` 完整复制独立演进，spec：`.specs/nn-predictive-ecms-phase25`） |
| `scripts/` | 目录 | 预留（当前为空） |
| `parameters/` | 目录 | 预留（当前为空） |
| `*.slxc` | 文件 | 各模型 Simulink 编译缓存，自动生成，不入库 |

各模型目录的详细内容见其内部 `README_Model.md`。

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）；
新增模型目录时须同时建立其 `README_Model.md`。修改 `.slx` 后必须做结构差异检查（见 `AGENTS.md` §9）。
