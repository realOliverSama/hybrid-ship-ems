# reatt8_f32 — REAT-T winner **fp32** 全 CPU 编译形态（偏差④修复，2026-09-29）

> 设立依据：`../../S4_PRED_PHYSICS_RCA.md`——int8 两条管线（TFLite/ORT QDQ）对
> REAT-T（attention+LayerNorm+GELU）造成输入依赖的系统性失真（锚级 −31%/−46%），
> NPU 段再叠二次失真；fp32 部署图经 host 对拍 == keras fp32 逐位级一致
> （锚 FLTA_S1_D00000_M2@300 q50 mean30 = 163.958 kW，物理真值 168.4 kW）。

## 生成链

```text
reatt_s0.keras（冻结 winner，只读）
  → Host/tools/quantize_reatt.py（同一脚本，fp32 分支无量化）
  → ../reatt8_q/reatt_raw_fp32.tflite（本目录只读副本，sha256 见下）
  → stedgeai 4.0.1 generate --no-inputs-allocation --no-outputs-allocation
      --target stm32n6 --name reatt8（无 --st-neural-art，全 CPU fp32）
  → 本目录产物
```

**`--name reatt8` 与 `../reatt8_q/`（int8）同符号命名空间**（stai_reatt8_*）：
两目录互斥编入，由 `EMS_REATT_FORM` 选择（`CPU`=int8 历史形态，保留回归用；
`CPUF32`=本目录，物理保真交付形态）。

## 关键数字（analyze/generate 171/171 PASS，2026-09-29）

- macc 189,482,352；权重 **4,052,912 B** 编入 ELF（Release NOR .rodata）；
  激活 **476,204 B**（固件静态数组，`STAI_REATT8_ACTIVATIONS_SIZE_BYTES`）
- I/O 契约与 int8 形态逐项相同：IN_1=13,200 B（归一化 hist [1,300,11] f32）、
  IN_2=720 B（归一化 kf [1,30,6] f32）、OUT_1=360 B（raw (m,a,b) [1,30,3] f32；
  分位变换在固件 `ems_reatt.c`，两形态同一实现）
- RT 静态占用：text 59,414 + rodata 4,055,714 + data 47,352 + bss 376,876
  （详见 `reatt8_analyze_report.txt`）

## 目录内容

| 文件 | 说明 |
|---|---|
| `reatt_raw_fp32.tflite` | fp32 tflite（只读；源 `../reatt8_q/reatt_raw_fp32.tflite` 逐字节副本） |
| `reatt8.c/.h` `reatt8_data.c/.h` `reatt8_details.h` | 生成代码（自洽 TU，只读禁改） |
| `generate-reatt8f32.sh` | 再生成脚本（STEDGEAI_CLI 指向 4.0.1 CLI） |
| `generate_401.log` / `reatt8_analyze_report.txt` / `reatt8_generate_report.txt` | provenance |

## 与 reatt8_q（int8）的关系

- reatt8_q 保留为历史形态（`EMS_REATT_FORM=CPU`），其 int8 失真结论见
  `../../S4_PRED_PHYSICS_RCA.md` §3-R1——不得再用于物理结论。
- 本形态是 S4 产品形态的预测器源（`EMS_REATT_FORM=CPUF32`）。
