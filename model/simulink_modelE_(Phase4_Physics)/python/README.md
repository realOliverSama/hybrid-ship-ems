# python/ — Model E 工作区 Python 脚本

> **入库策略**: 入库（脚本）；训练权重/keras 不入库。`t445/` 为 F 包新链；
> 其余 Phase 2.5 训练脚本标 SUPERSEDED（Dataset v3 不继承其 schema）。

## 目录结构

| 条目 | 说明 |
|---|---|
| `t445/` | ★ **F 包 T-445 Trajectory Oracle Gate Python 包**（冻结 config 导出、语料/统计/Gate；详见该目录 README.md）。**预测模型：无** |
| `ppe/` | **Phase 4 practical 实验链**：C2 推理链（`c2_inference.py`，FR836 float keras canonical 域）、formal window / freeze / replay / 资格判定、softHIL 报告（`softhil_report.py`）、板端单次 REATT 查询（`reatt_query_once.py`，UART 单帧问一次取板载预测）。产物落 `results/ppe_practical/` |
| `architectures.py` `train_*.py` `build_dataset*.py` `export/` 等 | **SUPERSEDED FOR MODELE DATASET V3 / REPRODUCTION ONLY**（Phase 2.5 训练/导出链） |
