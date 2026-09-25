# Model/tcn_q/ — TCN-f64 预测器部署链（FR-511 编译期换型，spec ems01-chil-tcn64-swap）

## 来源与溯源

| 项 | 值 |
|---|---|
| 源模型 | `WS/results/ecms_phase25/models/tcn_f64/tcn_f64_int8.tflite`（phase25 T-307 产物，原样使用，不重训不重标定） |
| sha256 | `a3406b8466b5ae10dd7893adc5bec41573f7ee58f10768b0555e78b0d58a4d43`（本目录 `tcn_q.tflite` 与源逐位一致） |
| 架构 | TCN-f64：7 残差块 causal dilated Conv1D（k=3，d=1,2,4,8,16,32,32，f=64；block0 带 1×1 skip conv 1→64）+ LastTimeStep + Dense(2→8) 旁路 concat → Dense(72→64) → Dense(64→3)（q10/q50/q90，归一化域） |
| 输入 | 双输入：`[1,300,1]` 归一化 P 窗 + `[1,2]` 归一化 [v,a]（QDQ，float32 I/O：IN_1=1200B / IN_2=8B / OUT_1=12B） |
| 输出量化 | per-tensor scale=0.016138948500156403 zp=−28 → **1 LSB_TCN = 0.266214 kWh**（× y_std 16.495129） |
| 预测器角色 | 替换 MLP-Q G2 槽位（NOR 0x70200000）；MLP-Q 资产保留可逆（`-DEMS_PREDICTOR=mlpq` + mlpq hex 重烧） |

## 目录内容

| 文件 | 说明 |
|---|---|
| `tcn_q.tflite` | 源 tflite 只读副本（sha256 见上） |
| `user_neuralart.json` | Neural-ART 配置（与 `Model/mlp_q/` 同款拷贝） |
| `my_mpools/stm32n6.mpool` | mpool 布局（与 `Model/mlp_q/` 同款拷贝，双 1MB octoFlash 池 + npuRAM3-6） |
| `generate-n6-model.sh` | stedgeai generate → objcopy（`network-data-tcnq.hex` @0x70200000）；用法见脚本头注释 |
| `analyze.log` / `generate.log` | 工具运行日志（归档） |
| `st_ai_output/` | analyze/generate 报告与中间产物（`network_analyze_report.txt` 权威、`network.c/h/ecblobs.h` 生成代码、`network_atonbuf.xSPI2.raw` 权重裸镜像） |
| `st_ai_ws/` | neural-art 编译工作区（本地暂存；产物以 `st_ai_output/` 为准） |
| `../../Binary/network-data-tcnq.hex` | NOR 权重 hex（ihex，基址 0x70200000，453.7KB 文本） |

## analyze 资源核对（2026-09-02，stedgeai v2.2.0-20266，T-801）

| 项 | 值 | 预算 |
|---|---|---|
| epochs | 132（47 SW：Pad/SpaceToDepth/DepthToSpace/Quantize/Gather 数据搬运；**14 CONV_2D + 3 FULLY_CONNECTED 全 EC= NPU HW**） | design R2 |
| activations | 131.320 kB @npuRAM3 | 448 KB 池 29.31% |
| weights | 161.267 kB @octoFlash 0x70200000 | 1 MB 池 15.75%（mlpq 同基址） |
| macc | 11,959,685 / 次推理 | — |

## 口径声明（AI_MODELING §22.3 规则 1）

- 输入口径：302 特征语义 = `[P_demand(t−299..t) ×300（1 s 升序）| v_ship | accel]`，
  **归一化域**（`datasets/fleet_norm_v2.json` X_mean/X_std per-feature；y 反归一化
  q = q_norm × y_std + y_mean）——与 MLP-Q 同一特征管线；固件侧 ems_ctrl 归一化后
  拆分双输入（窗口 300 + aux [v,a]）。
- int8 标定集：T-307 val 前 1000 归一化样本（与部署喂入口径相同，§22.3 规则 3
  "口径相同"例外条款适用）；三方一致检查见
  `WS/results/ecms_phase25/deploy/m3_tcn_swap_calib_report.json`（T-803 产物）。
- 求和顺序（REF C 前向位级基准）：见 `Appli/App/ems_infer_tcn.c` 头注释（T-804）。