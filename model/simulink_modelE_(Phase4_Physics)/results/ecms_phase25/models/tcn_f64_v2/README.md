# tcn_f64_v2 — TCN-f64 原生 dilated Conv2D 形态重导出（T-812 跟进，2026-09-03）

## 用途

`tcn_f64` 的部署重导出：绕开 TFLite 转换器对 dilated Conv1D/Conv2D 的
SpaceToBatchND/Conv2D/BatchToSpaceND 图形分解（该分解图在 STM32N6 Neural-ART 上
执行发散，板实证 88 LSB，见 `Host/results/tcn_swap/layer2_diagnosis_20260903.json`）。

函数与 `tcn_f64` 严格等价（float 域逐窗 Δq50 = 0.0 kWh，35 窗验证），
量化口径与 T-307 完全一致（302 特征 / fleet_norm_v2 / val 前 1000 样本逐样本校准）。

## 文件

| 文件 | 内容 |
|---|---|
| `tcn_f64_v2.keras` | 函数等价重建模型：ZeroPadding2D + 自定义层 `NativeDilatedConv2D`（层内 raw `tf.nn.conv2d`，原生 dilation）+ Reshape 回序列复用 `LastTimeStep` + 原 Dense 头；权重逐层移植自 `tcn_f64.keras` |
| `tcn_f64_v2_int8.tflite` | 全整型 QDQ int8（float32 I/O），图形态：15×CONV_2D（原生 dilation）+ PAD，**无** SPACE_TO_BATCH_ND / BATCH_TO_SPACE_ND |
| `export_v2_report.json` | 机器可读验证报告：G1 算子形态 / G2 函数等价 / G3 解释器 LSB 三门 + 权重移植清单 + 量化校准声明 |

## 输入输出约定（与旧 tcn_f64_int8.tflite 的差异）

- `P_window_1x300x1`: **float32 [1,1,300,1]**（旧为 [1,300,1]，新增 H=1 轴，数据布局不变）
- `V_scale_2`: float32 [1,2]（不变）
- 输出 q10/q50/q90: float32 [1,3]（归一化域；输出 int8 网格 scale=0.0161389504, zp=-28，与旧模型 0.0161389485/-28 一致）

## 加载方法

```python
import sys; sys.path.insert(0, ".../model/simulink_modelD_(EMS_NN_AECMS)/python")
from export_tcn_v2_conv2d import CUSTOM_OBJECTS
m = tf.keras.models.load_model("tcn_f64_v2.keras", safe_mode=False, compile=False,
                               custom_objects=CUSTOM_OBJECTS)
```

## 门状态（2026-09-03 导出运行，含主 Agent 门限修订裁决）

- G1 算子形态：PASS（0 STB/BTS；CONV_2D=15/15；dilation 组合逐一匹配）
- G2 函数等价：PASS（max|Δq50| = 0.0 kWh ≤ 1e-4）
- G3 解释器：PASS（按 2026-09-03 主 Agent 裁决修订判据，方案 A）
  - (i) q50 头（ê 驱动头）int8-new vs int8-old：lsb_max = 4 LSB（35 窗）/10 LSB（512 窗）≤ 16，PASS；
  - (ii) 支配性（512 窗诊断集）：q10 new-vs-float mean 2.16 vs incumbent 6.01 LSB、
    q90 mean 16.2 vs 19.9 且 max 82.6 vs 112.8 LSB —— v2 严格占优；
    q50 int8-vs-float RMSE 0.936 vs 0.864 kWh（同量级，T-307 先例口径内），PASS；
  - q90 头 ~80 LSB 级尾噪在两实现中同在（分位头 int8 固有量化噪声，非 v2 引入退化）；
    层②门禁与 ê 只依赖 q50 头（干净）。原字面 16 LSB 门会同样否决在板 incumbent
    （tcn_f64_int8.tflite），主 Agent 判定为规格缺陷并修订判据（批准记录在案，
    详见 export_v2_report.json `gate_revision` 节）；不放宽控制相关校准。

## 板级衔接提示（供主 Agent 接手 stedgeai/NPU 验证）

- 新 tflite 算子为原生 dilated CONV_2D（含 PAD×14 + CONV_2D×15），若 stedgeai 对该形态
  编译失败或 NPU 复测仍发散，按预设转全 SW 方案裁决（与本次 G3 无关）；
- 输入张量 [1,300,1] → [1,1,300,1]（NHWC，数据布局不变），Host 侧装配与 Appliance 协议
  需按 4D 输入适配（本任务未触碰 P3 Host/ 侧）。

## 上层目录 README 登记

本目录由委派任务白名单创建（仅 `python/export_tcn_v2_conv2d.py` + 本目录），
`results/ecms_phase25/` 与 `models/` 上层 README 的条目登记按白名单约束**未改动**，
由主 Agent 验收时补登记（§20 纪律，随下次提交完成）。