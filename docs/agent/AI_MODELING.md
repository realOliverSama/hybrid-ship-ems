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

> ⚠ **口径契约教训**：仅比较 float/int8 精度不够——int8 标定集口径必须与部署喂入口径一致，
> 否则 MCU 上会系统性坍缩且 float/int8 对比发现不了。见 **§22（T-508 口径契约事故，必读）**。

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

> ⚠ T-508 实证：以上清单逐项检查仍会漏掉 **int8 标定集口径 vs 部署喂入口径** 的一致性
> （workclf int8 因此在板上坍缩为单类）。闭合检查见 **§22（必读）**。

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

---

## 22. T-508 口径契约事故与后续路线（2026-09-07，后续模型工作必读）

### 22.1 事故经过（一句话版）

**workclf int8 在板上坍缩为单类（class 6 出现 0 次），根因不是训练、不是 NPU，而是“三个口径”不一致**：

```text
训练管线口径        部署喂入口径         int8 标定集口径
（归一化特征，       （raw 物理量，        （T-312 val 归一化样本）
 T-312）             EML:82-105）
      ✓____________________✗____________________✓
           部署喂入 ≠ 训练口径：P 通道 100–800 kW 全部饱和至 int8 上限
           → P 通道恒定 → 分类仅由 [v,accel] 驱动 → argmax 恒 class 4
```

- 量化证据：int8 输入量化节点 P_dec30 scale=0.0125731/zp=−22，可表示范围 ≈[−1.333,+1.873]
  （归一化单位），raw P 全部饱和；float keras × raw 与 EML double 参考逐拍 100% 一致 →
  **float 模型 raw 口径本身正常，坍缩纯由 int8 标定口径错配引起**。
- 为什么三道既有验证都没拦住：T-312 按训练口径测（达标）→ M1 测板载 vs host-int8
  一致性（同口径同错，一致性检查发现不了）→ 只有 T-508 层③闭环（vs Simulink double 参考）
  才暴露。**教训：一致性对拍永远发现不了“两个系统一起错”。**
- 证据：`model/simulink_modelD_(EMS_NN_AECMS)/results/ecms_phase25/deploy/`
  下 `m2_port_report.md` §6 + `m2_workclf_calib_report.md`（判别实验 E1–E5 全量化）。

### 22.2 事故暴露的第二个独立问题（设计层）

**先纠正一个容易混淆的点：本项目只有一个分类器模型（workclf_v1，T-312 训练）**。
所谓“EML 参考里的分类器”不是另一个模型——是**同一份 workclf_v1 权重**内嵌在 Simulink EML
中、以 **raw 物理量喂入**（EML:82-105，double 精度）运行：E1 实验证明 float keras × raw 与
Simulink 参考逐拍 100% 一致（双轨迹），层① REF 固件（同一权重 deploy JSON）P_gen 也与
Simulink 100% 一致。因此问题准确表述是：**同一模型在三种喂入口径下有三种行为**：

| 运行形态 | 喂入口径 | 行为 |
|---|---|---|
| float / int8（E3/E4，T-312 同款） | 归一化（训练口径） | 按训练工作：对设计真值 acc 0.598–0.760，S20 各类分布接近设计 |
| double（EML 参考 + 固件 REF 后端） | **raw**（EML:82-105） | 不执行有效段识别：S20 输出 upstream 89%（设计 10.9%）；voyage_002 设计无 upstream 而输出 88%、对真值 acc=0.0000 |
| int8（部署 NPU，标定集归一化） | **raw** | 量化饱和坍缩：恒 class 4，class 6 出现 0 次 |

即：**把按归一化口径训练的模型接到 raw 喂入链路上（无论 double 还是 int8），得到的都不是
训练时的那个函数**。double 下它恰好与 Simulink 参考逐拍一致——所以层①“移植保真”满分，
但这保真保的是“raw 口径下的退化行为”。归一化口径下模型对设计真值 acc 0.598–0.760，
S20 各类分布接近设计（eco 25.0%/rush 30.6%/harbor 34.3%/upstream 10.4% vs 设计
23.9%/31.5%/12.0%/10.9%），但 bridge/berth/depart 几乎不识别、voyage_002 无 harbor 却预测
37%——它是“训练口径下正常工作的分类器”，不是理想分类器（评估双轨制见 §22.3 规则 5）。
含义：**参考模型中预测门控 ê=0 的时段占绝对主导（S20 89%），"NN 预测"仅在少数时段实际
进入控制**——修正喂入口径/分类器（让 upstream 判定贴合设计真值）恰恰是让预测发挥价值的
前提，不是“训练好也没意义”。

### 22.3 后续模型工作的硬规则（新增，违反 = Escalate）

1. **口径契约显式化**：任何模型交付物（.keras/.tflite）必须随附“输入口径声明”——特征定义、
   归一化常数（来源 JSON）、原始量纲 or 归一化域、窗口顺序。训练脚本导出时自动生成，
   不允许口头约定。
2. **三口径一致检查**（部署验收新增硬门，FR-511 换模型流程必须包含）：
   `训练口径 == 部署喂入口径 == int8 标定集口径`，三方逐项核对；不一致禁止部署。
3. **int8 标定集必须取自部署口径样本**：representative dataset 用固件实际会喂入的数据分布，
   禁止直接复用训练管线样本（除非口径相同）。
4. **部署后首个闭环验证必须含层③式对拍**（vs double/Simulink 参考）：层②一致性对拍
  （board vs host-int8）发现不了共同口径错误。
5. **分类器评估双轨制**：除自身 val 指标（T-312 口径）外，必须报告对设计段真值的
   一致性（acc + 段边界对齐），两个指标分开陈述、不得混用。

### 22.4 workclf 修复路线选项（决策记录用，尚未裁决）

| 路线 | 内容 | 代价/含义 |
|---|---|---|
| A | raw 口径重标定 int8 | 忠实复现 EML 行为；但复现的是本身不做段识别的退化函数（§22.2） |
| B | raw 口径重训分类器 + 重标定 | 偏离 EML 参考（design 修订 + FR-511）；研究贡献方向 |
| C | 固件改喂归一化特征 | 对齐 T-312 训练口径；偏离 EML:82-105 语义（design 修订） |
| D | v1 维持现状，记录为部署限制 | 已在 M2 报告 §8 执行 |

> **决策与实施记录（2026-09-08）**：用户裁决 D-1~D-7（草案 §5）：批准路线 C 实施
> （变体同步、基线连锁重建、v1 冻结+指纹门、T-310 证据版本化；B 延后另立项）。
> 实施与板上复验已完成：层① 100% exact 保持；层③ 45.5%→98.46% / 95.25%→98.39%；
> 板载分类器对设计真值 acc 75.80%（原 10.87%）、upstream 9.7%≈设计 10.9%；FR-508 回归 PASS。
> 遗留：层② S20 98.877% 边际 FAIL（诚实记录，见 M2 报告 §11.4）。
> 数据：`m2_port_report.md` §11；镜像 NPU 9e2fab81 / REF f31714ea。

MLP-Q 无此问题（固件先归一化再喂入，与训练/标定口径一致）——**它就是三口径一致的正面样板**。
- 为获得更好结果必须改变研究方法。