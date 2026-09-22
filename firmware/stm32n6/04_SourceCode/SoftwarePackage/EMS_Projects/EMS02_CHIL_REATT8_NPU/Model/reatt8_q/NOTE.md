# reatt8_q — REAT-T winner int8 全 CPU 编译形态（STEdgeAI 4.0.1，CM55 Lite 内核）

> phase4-g（spec `.specs/phase4-g-reatt-board-deployment`）。源模型 = Phase 4-E
> Predictor-v3 冻结 winner `reatt_s0.keras`（reatt-t018：Cp=96/Nf=4/d_dec=128/
> df=128，1,012,835 参数，FR-836 keras sha256
> `d8da6af4667875c45de7de90383d2b6b750420f5af62c7dcdcb1e5997897f894`）。

## 生成链（Host/tools/quantize_reatt.py + generate-reatt8.sh）

```text
reatt_s0.keras（冻结，只读）
  → 部署图裁剪：输出 = raw_proj (m,a,b) [30,3]（D1：TFLite SOFTPLUS 溢出，
    MonotonicQuantileTransform 移固件稳定实现）
  → TFLite int8 全整数量化（float32 I/O，标定 = 归一化 train 窗 1024 个，
    seed 20260922，windows_v3_formal.npz splits=='train'——部署口径标定，
    AI_MODELING §22.3 硬规则）
  → stedgeai 4.0.1 generate --no-inputs-allocation --no-outputs-allocation
    --target stm32n6 --name reatt8（无 --st-neural-art，全 CPU 编译形态）
```

## 目录内容

| 文件 | 说明 |
|---|---|
| `reatt_raw_int8.tflite` | int8 tflite（sha256 `efde1aff…23f066a`，只读；host 对拍/再生成锚点） |
| `reatt8.c` / `reatt8.h` | 生成代码（stai 绑定层 stai_reatt8_* + 图 + Lite 内核调用，自洽 TU） |
| `reatt8_data.c` / `reatt8_data.h` | 权重数组 `g_reatt8_weights_array`（const uint64_t[179070] = 1,432,556 B，STAI_ALIGNED(8)） |
| `reatt8_details.h` | 生成代码辅助头 |
| `generate_401.log` | stedgeai generate 全量报告（provenance） |
| `generate-reatt8.sh` | 再生成脚本（STEDGEAI_CLI 指向 4.0.1 CLI） |

## I/O 契约（stai，喂入口径 = 归一化域）

- IN_1 = 13,200 B（[1,300,11] f32，**归一化后** hist 窗，t-major × 11 通道，
  通道序 = registry INPUT_H1；归一化在固件完成，常数 =
  `ems_reatt_params.c`（formal_norm_v3 逐 bin mean/std））
- IN_2 = 720 B（[1,30,6] f32，**归一化后** KF（K+F 合序，registry INPUT_K+INPUT_F；
  常数 = predictor_norm_ext_v1 逐 lead mean/scale））
- OUT_1 = 360 B（[1,30,3] f32 = (m,a,b) **kW 域原始三分量**；固件后处理
  q10=m−softplus(a) / q50=m / q90=m+softplus(b)，稳定 softplus）

## 关键数字（analyze 171/171 PASS，2026-09-22 去风险实测）

- macc 192,909,319；权重 1,432,556 B（入 ELF）；激活 332,928 B（固件静态数组）
- RT 静态占用：text ~102K + rodata ~35K + data ~58K + bss ~2K（NetworkRuntime1201）
- host int8 vs keras fp32 对拍（256 窗 train/val 各半）：q50 MAE 19.88 kW /
  max 250.1 kW / corr 0.9850（L0 门 ≤25 kW 过；E 段 ONNX int8 为 59–62 kW）
- fp32 部署变体 vs keras：≤0.0013 kW（TFLite 化无损）

## 与 TCN 槽位（tcn_q_v2_cpu）的关键差异

1. 本包为**独立预测服务**（Phase 4-F C2 语义：300s 窗 11 通道 + 30 lead KF →
   30 lead × 3 分位 kW），不复用 `ems_infer_mlpq` 槽位契约（302 维→q3 kWh，
   Model D 谱系）——两契约语义不同，不互换；
2. 双输入张量语义不同（[300,11] 归一化窗 + [30,6] 归一化 KF，均为模型直喂域）；
3. 权重 1.43 MB 入 ELF：**Debug SRAM（2047K 窗口）装不下完整应用**，本工程板级
   验证走 Release（NOR appli，ROMxspi2 区域 31,743K）；
4. 分位变换在固件（D1），非模型内。
