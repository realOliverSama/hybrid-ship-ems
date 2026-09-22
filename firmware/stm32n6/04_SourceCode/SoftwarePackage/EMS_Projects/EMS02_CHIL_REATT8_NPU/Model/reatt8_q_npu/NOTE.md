# reatt8_q_npu — REAT-T **NPU 形态**部署包（ST Edge AI Core 4.0.1，Neural-ART）

> phase4-g NPU 线（2026-09-22）。目标：REAT-T 在 STM32N6 **Neural-ART NPU** 上
> 推理（EC blob + NOR 权重），替代/并行现有全 CPU 形态（`../reatt8_q/`）。
> 状态：**analyze + generate 双双通过（离线闭环完成）**；固件集成与上板待做。

## 打通 NPU 的三个关键修复（全部为部署图变换，权重数值不变）

| # | 障碍（实测错误） | 修复 |
|---|---|---|
| 1 | `NOT IMPLEMENTED: Unsupported layer types: Erfc`（精确 GELU 的 erf 不在 NPU 算子集） | **tanh 近似 GELU**（`tf.keras.activations.gelu(approximate=True)`；E 段先例 corr 0.999，本版实测 corr 1.000000 / max|Δ| 0.15kW） |
| 2 | `INTERNAL ERROR: Unknown dimensions: W`（Keras MHA 的运行时 shape 计算链：Shape/Gather/ReduceProd + 动态 Reshape） | **静态 attention 重写**（`StaticMHA`：显式常量 Reshape/Permute/MatMul/Softmax，数学与 Keras MHA 严格等价，权重平移；ONNX 从 641→529 节点，Shape/Gather/ReduceProd 归零） |
| 3 | `Unknown dimensions: W`（TFLite 路 `ONNX shape inference fails`；二分定位到 **中间维 Pad**：ST 转换器只支持最内维 padding） | **中间维 Pad 手术**（`onnx_fix_midpad.py`：对 5 个中间维 Pad 包 Transpose→Pad(最内维)→Transpose，数值零改变） |

## 生成链（Host/tools/npu/ 脚本，全部可复现）

```text
reatt_s0.keras（冻结）
  → npu_export_static.py：静态 attention + tanh GELU 构建 + winner 权重平移（对拍 corr 1.000000）
  → tf2onnx 静态 ONNX（批=1 固定，opset 17）
  → onnx_fix_midpad.py（中间维 Pad 手术）
  → npu_quantize_int8.py（onnxruntime QDQ 静态量化；标定 = 部署口径归一化 train 窗 1024）
  → stedgeai 4.0.1 generate --st-neural-art（NPU 形态）→ 本目录产物
```

## 产物与关键数字（2026-09-22）

| 文件 | 说明 |
|---|---|
| `reatt_static_int8.onnx` | int8 QDQ ONNX（1.58MB，NPU 编译器输入；与 fp32 对拍 corr 0.978 / mean 16.9kW） |
| `reattnpu.c` / `reattnpu.h` / `reattnpu_ecblobs.h` | 生成代码（EC blob 化，stai API） |
| `stai_reattnpu.c/.h` | stai NPU 形态绑定层（ext 生命周期） |
| `reattnpu_atonbuf.xSPI2.raw` | **NOR 权重块（1.05MB）** → `../../Binary/network-data-reattnpu.hex` @ **0x70200000**（2MB 池，workclf 0x70400000 下界内） |
| `reattnpu_generate_report.txt` | 全量生成报告（provenance） |
| `my_mpools/stm32n6.mpool` | 池配置（**xSPI2 池 1MB→2MB**，承 REATT 权重；npuRAM 四池 448KB×4） |
| `user_neuralart.json` | NPU 形态选项（--enable-epoch-controller --cache-maintenance 等，与 tcn_q_v2 同构） |

**analyze（int8, target stm32n6npu）**：macc 77.3M；epochs 471 = **211 纯 HW + 51 混合
+ 209 纯 SW**（对比 fp32 形态 90/420——int8 让硬件映射翻倍）；FLASH 合计 2.03MB
（权重全进 2MB NOR 池）；RAM 1.30MB 激活。
**generate**：FLASH 1,945,812 B / RAM 1,336,011 B，9 文件（含 EC blobs）。

## 待办（上板闭环，下一步）

1. 固件 NPU 形态包装（沿 `../tcn_q_v2/` + `ems_npu_workclf_stai.c` 先例：stai ext
   生命周期 + NOR 权重 memory-mapped；CMake 选项与 CPU 形态二选一）；
2. NOR 权重烧录 `Binary/network-data-reattnpu.hex` → 0x70200000（Level C，专用加载器，
   仅该 2MB 区；覆盖旧 TCN NPU 权重，回滚=重烧 Binary 资产）；
3. 板上复验：L1/L2 对拍（同 64 向量）+ **推理时延对比**（预期 0.80s CPU → 数十 ms NPU 量级）。
