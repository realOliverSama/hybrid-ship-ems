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

| 条目 | 类型 | 说明 | 使用的预测模型 |
|---|---|---|---|
| `simulink/` | 目录 | **主力模型工作区**——MVP 最小闭环验证模型 + ModelA/ModelB（静止起步→巡航→固定航程；MLP 负荷预测 AI 闭环验证；07 步流水线 + 08/09 ModelB 扩展脚本） | MLP 负荷预测器（22s 窗口，int8 13.7KB） |
| `simulink_modelC/` | 目录 | Model C——定时前馈 EMS 研究副本（`HybridShip_ModelC_DCBus.slx`） | 无（规则 EMS 对照） |
| `simulink_modelC_(oracle_ms)/` | 目录 | Model C 的 oracle 多步（multi-step）变体研究副本（`HybridShip_ModelCms_DCBus.slx`） | Oracle 多步真值（离线上界参照） |
| `simulink_modelD/` | 目录 | Model D——定时前馈 EMS 研究副本（`HybridShip_ModelD_DCBus.slx`，含 `experiments/` 子目录）；Phase 2 后冻结（纯 Phase 2 状态） | 无（规则 EMS 对照） |
| `simulink_modelD_(EMS_NN_AECMS)/` | 目录 | Model D 本体 + 神经网络预测型 A-ECMS（Phase 2.5 起，自 `simulink_modelD` 完整复制独立演进，spec：`.specs/nn-predictive-ecms-phase25`） | **MLP-Q 分位数预测器（302→64→32→3，q10/q50/q90，int8 26KB，Winner）** + 工况识别器（Conv1D，G2 门控，~50KB）；另训练过 TCN/N-BEATS/集成（闭环对比候选） |
| `simulink_modelE_(Phase4_Physics)/` | 目录 | **Model E——Phase 4 船舶物理升级工作区**（Model D 复制分叉，spec `.specs/phase4-a2-model-e-construction`，feat/phase4-a2-model-e 分支）：500 t 口径船体阻力重构（R_deep 锚定 + 浅水 Δr 相对增量）、w(H/T) 信号化、双发电机组+启停状态机、per-genset BSFC PCHIP、电池 LFP 1RC ECM+SOP 四约束、T-412 整体物理 Gate；**Phase 4-E predictor_v3 预测器训练工作区**（spec `.specs/phase4-e-predictor-v3/`，T-001 起：`predictor_v3/` 包 + `config/predictor_v3/` + `results/predictor_v3/` 证据） | 七 EMS 变体继承不变（law=0/1/2/4/5/6/7；law=7 NN-AECMS 权重经 ecms_nn_weights.mat 外部注入）；Plant 侧无预测模型（纯物理测试台）；predictor_v3 = E 包独立 Python 训练/评估链（NN 负荷预测器五候选施工中——截至 T-001 无已训练模型工件） |
| `scripts/` | 目录 | 预留（当前为空） | — |
| `parameters/` | 目录 | 预留（当前为空） | — |
| `*.slxc` | 文件 | 各模型 Simulink 编译缓存，自动生成，不入库 | — |

> 注：`README.md` 为目录索引（概括，含各文件夹使用的预测模型）；每个模型文件夹内部 `README_Model.md` 为详细说明（Simulink 结构/EMS 逻辑/预测模型清单，见 `AGENTS.md` §20.1）。

各模型目录的详细内容见其内部 `README_Model.md`。

## 维护规则

新增/移动/重命名/删除本目录内任何文件或文件夹，必须同步更新本 README（见 `AGENTS.md` §20）；
新增模型目录时须同时建立其 `README_Model.md`。修改 `.slx` 后必须做结构差异检查（见 `AGENTS.md` §9）。
