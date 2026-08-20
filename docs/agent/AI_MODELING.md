# AI_MODELING.md

本文件规定混合动力船舶负荷预测模型的数据、训练、验证、模型选择和部署规则。

当前主要涉及 MLP、LSTM 等轻量负荷预测模型。

Python 工作应使用项目规定的 TensorFlow 虚拟环境。

当前约定：

```text
model/simulink/python/.venv_tf
```

如果仓库实际环境位置与此不同，以仓库当前正式配置为准，不得自行创建重复环境。

---

## 1. AI 在系统中的角色

当前 AI 模块主要负责：

```text
预测未来负荷相关信息
```

而不是默认取代整个 EMS。

总体关系：

```text
Historical Data
      ↓
Prediction Model
      ↓
Prediction Output
      ↓
EMS
      ↓
Power Allocation
```

预测和控制必须保持概念分离。

是否改变为端到端 AI EMS 属于重大研究路线变化，必须由用户和主 Agent决定。

---

## 2. 预测目标

开始训练之前必须明确：

- Input Variables；
- Historical Window；
- Sampling Interval；
- Forecast Horizon；
- Prediction Target；
- Target Unit；
- Target Calculation；
- Prediction Update Period。

例如，如果目标是：

```text
使用过去 T_history 的负荷序列
预测未来 T_future 内的最大负荷
```

则必须明确：

```text
T_history
T_future
Sampling Interval
Peak Definition
Target Unit
```

不得只使用模糊表述：

```text
未来负荷预测
```

---

## 3. Dataset 来源

Dataset 必须尽可能可追溯。

应记录：

- 数据来源；
- Scenario；
- Model Version；
- 生成脚本；
- 参数；
- Sampling；
- Commit SHA，如条件允许。

Simulink 生成的数据必须明确为：

```text
simulation-generated
```

不得将仿真数据描述为真实船舶实测数据。

如果未来引入：

- 实船数据；
- 台架数据；
- HIL 数据；
- 公开 Dataset；

应明确区分来源。

---

## 4. Train / Validation / Test

必须严格区分：

```text
Training Set
Validation Set
Test Set
```

Test Set：

- 不参与模型训练；
- 不参与 Early Stopping 决策；
- 不参与 Hyperparameter Selection；
- 不参与模型架构选择；
- 不参与 Feature 选择。

禁止：

```text
查看 Test 结果
→ 根据 Test 调整模型
→ 再把同一个 Test 称为独立最终测试
```

如果已经发生这种情况，应明确说明该 Test 已经受到模型选择污染。

---

## 5. 时间序列数据泄漏

时间序列窗口非常容易产生 Leakage。

必须检查：

- Window Overlap；
- Temporal Leakage；
- Scenario Leakage；
- Normalization Leakage；
- Future Information Leakage。

如果相邻窗口高度重叠，随机 Window Split 可能导致 Train 和 Test 包含高度相似的时间段。

这种划分不应默认使用。

---

## 6. Scenario Split

如果研究目标需要验证模型跨场景泛化能力，应考虑：

```text
Scenario-Level Split
```

而不是单纯 Random Window Split。

具体划分方式由主 Agent根据研究目标决定。

Subagent 不得因为某种划分指标较低而自行更换 Split 方法。

---

## 7. Normalization

Normalization / Standardization 参数只能使用允许的数据范围拟合。

通常：

```text
Fit on Training Set
Apply to Validation/Test
```

不得使用完整 Dataset 的统计量造成 Test Information Leakage，除非研究设计明确要求并记录理由。

模型导出时必须同时保存或明确记录预处理参数。

---

## 8. Feature

Feature 应具有明确物理或控制意义。

候选可能包括：

- Historical Load；
- SOC；
- Speed；
- Control State；
- 其他部署时真实可获得的变量。

加入 Feature 前必须考虑：

- 部署时是否能够获得；
- 是否包含未来信息；
- 是否造成 Leakage；
- 是否增加 MCU 输入和预处理成本；
- 是否真正符合研究目标。

不能仅仅因为某 Feature 能显著提高 Test Score 就自动采用。

---

## 9. Model

当前可研究的模型包括：

- MLP；
- LSTM；
- 其他明确批准的轻量模型。

模型选择不能只看 Prediction Accuracy。

还应综合考虑：

```text
Prediction Quality
+
Model Size
+
RAM
+
Flash
+
Inference Latency
+
Implementation Complexity
+
STM32N6 Deployability
```

更复杂的模型只有在收益足以抵偿部署成本时才值得采用。

---

## 10. Hyperparameter Search

Subagent 可以在主 Agent定义的范围内执行：

- Hidden Size；
- Layer Count；
- Learning Rate；
- Batch Size；
- Epoch；
- Dropout；
- Sequence Length；
- 其他批准参数。

每次实验尽可能记录：

- Model Config；
- Seed；
- Dataset Version；
- Split；
- Metrics；
- Runtime；
- Model Size。

Subagent 不得把普通超参数搜索扩大成新的研究路线。

---

## 11. Reproducibility

训练和实验应尽可能固定：

- Random Seed；
- Dataset Version；
- Dataset Split；
- Config；
- Software Environment；
- Preprocessing。

实验结果应该能够通过正式脚本重新运行。

禁止依赖 Notebook 中未保存的临时状态作为正式结果来源。

---

## 12. Prediction Metrics

预测指标可以包括：

- MAE；
- RMSE；
- MAPE，在适用时；
- Peak Error；
- Underprediction Error；
- Overprediction Error；
- Large Peak Miss Rate。

具体核心指标由主 Agent决定。

预测指标不是最终 EMS 价值的唯一评价标准。

最终需要建立：

```text
Prediction Quality
        ↓
EMS Control Performance
```

之间的关系。

一个预测 RMSE 略差的模型，也可能因为错误分布更适合控制而获得更好的 EMS 效果。

最终模型由主 Agent综合选择。

---

## 13. Peak Prediction

如果预测目标为未来窗口内 Peak，需要特别关注：

- Peak MAE；
- Peak RMSE；
- Underprediction Frequency；
- Underprediction Magnitude；
- Large Peak Miss；
- Bias；
- 必要时的 Peak Timing。

在 EMS 场景中：

```text
低估未来峰值
```

与：

```text
高估未来峰值
```

可能产生不同控制后果。

因此不应只依赖对称误差指标。

---

## 14. 模型比较公平性

MLP、LSTM 或其他模型比较时应保持：

- 相同 Dataset；
- 相同 Split；
- 相同 Input；
- 相同 Target；
- 相同主要评价指标；
- 相同数据预处理原则。

不得为不同模型采用明显有利于其中一个模型的数据条件。

---

## 15. Export

导出：

```text
.keras
.tflite
```

时应记录：

- Source Model；
- Input Shape；
- Output Shape；
- Input Datatype；
- Output Datatype；
- Scaling；
- Quantization；
- Model Size；
- Preprocessing；
- Postprocessing。

如果执行 Quantization，必须比较：

```text
Float Model
vs
Quantized Model
```

的精度差异。

---

## 16. STM32N6 部署约束

部署前至少确认：

- Input Shape；
- Input Datatype；
- Output Shape；
- Output Datatype；
- Model Size；
- RAM Requirement；
- Flash Requirement；
- Inference Runtime；
- Preprocessing；
- Postprocessing；
- Quantization；
- Prediction Update Period。

Python 端和 MCU 端预处理必须保持一致。

尤其检查：

- Scaling；
- Normalization；
- Feature Order；
- Tensor Shape；
- Datatype；
- Window Order。

“模型能够在 MCU 上运行”不等于部署正确。

---

## 17. AI 与 EMS 联合验证

最终模型不仅需要进行独立预测评价，还需要放回 EMS 中测试。

应回答：

```text
这个预测模型是否真正改善 EMS？
```

而不是仅回答：

```text
这个预测模型 RMSE 是否更低？
```

至少考虑：

- Battery Peak；
- SOC；
- Generator Ramp；
- Speed；
- Overload；
- Power Balance；
- 其他既定 EMS 指标。

---

## 18. 禁止操作

Subagent 严禁：

- 使用 Test Set 调参；
- 使用 Test Set 选架构；
- 根据 Test Score 修改 Dataset Split；
- 删除低分实验；
- 隐藏失败实验；
- 偷换 Prediction Target；
- 修改 Metric 以制造更好结果；
- 使用未来信息；
- 未经批准更换最终模型路线；
- 对不同模型使用不公平实验条件；
- 将仿真数据描述为实测数据。

---

## 19. 实验记录

正式模型实验至少应能够回答：

```text
Dataset:
Dataset Version:
Split:
Input:
History Window:
Forecast Horizon:
Target:
Model:
Hyperparameters:
Seed:
Metrics:
Model Size:
Export:
Notes:
```

如果某项未知，应写：

```text
Unknown
```

而不是猜测。

---

## 20. Agent 分工

### 主 Agent

负责：

- 定义研究问题；
- 定义 Prediction Target；
- 定义 Dataset Split；
- 定义 Feature 原则；
- 选择模型路线；
- 定义核心 Metric；
- 判断实验意义；
- 决定最终模型；
- 判断是否适合 STM32N6；
- 解释 EMS 联合实验结果。

### Subagent

负责：

- 数据处理；
- Window 生成；
- Dataset 构建；
- 训练；
- Hyperparameter Sweep；
- Export；
- Quantization；
- Benchmark；
- 汇总结果。

原则：

```text
Subagent 产生实验结果
主 Agent 解释实验结果
```

---

## 21. 必须 Escalate 的情况

Subagent 遇到以下情况必须返回主 Agent：

- 必须改变 Prediction Target；
- 必须改变 Forecast Horizon；
- 必须改变 Dataset Split；
- 怀疑存在 Data Leakage；
- 必须改变 Feature 定义；
- 必须更换核心模型路线；
- Test 指标异常好或异常差且原因不清楚；
- Python 和 MCU Preprocessing 不一致；
- 模型无法在部署约束内运行；
- 不同实验结果不可公平比较；
- 为获得更好结果必须改变研究方法。