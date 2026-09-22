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
| `reatt_concat_fp32.onnx` | **v3 部署图**（静态 attention + tanh GELU + Concat 零填充；对拍 winner corr 1.000000） |
| `reatt_concat_int8.onnx` | int8 QDQ ONNX（1.59MB，NPU 编译器输入；与 fp32 对拍 corr 0.978 / mean 16.9kW）。v1/v2 图变体（tanh 独立版 / midpad 手术版）已迭代废弃，历史见 git |
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


## 板上结果与结论（2026-09-23 凌晨，板级实证）

### 已达成（工具链闭环 + 固件 + 启动）

- NPU 形态固件**能启动、能推理**：PING 20/20；CMD_REATT 单帧 ~300ms（对比 CPU 形态
  800ms）；混合精度 I/O 契约经工具自打印证实与固件实现逐项一致
  （f32 hist / int8 kf Q(0.022380276,−19) / int8 out Q(3.620067120,−128)）；
- 权重池 0x702F0000（1088KB，appli 尾 0x7022951F 与 workclf 0x70400000 双向无重叠，
  回读逐字节一致）；appli-only 烧录 + FSBL 前后哈希不变。

### 失败（板端数值退化，两种图形态同样复现）

- **输出退化**：m 通道恒 −128（0.00 kW）、跨不同输入输出不变——网络输出与输入
  解耦。两个图形态（v2 中间维 Pad 手术 / **v3 Concat 零填充**）**同样复现**；
- ONNX 语义经 ORT 验证正确（q50≈753 kW 等），**问题不在 ONNX/量化标定**；
- **继承的历史结论（关键）**：TCN64 工程 2026-09-03 已对同类问题定性为
  **Neural-ART int8 执行语义与解释器结构性分歧（与图形态无关）**——原生 dilation
  与 SpaceToBatch 分解两形态均复现，判 **NO-GO**，随后改走全 CPU 形态
  （证据：`../EMS01_CHIL_ECMS_TCN64/Host/results/tcn_swap/layer2_diagnosis_20260903.json`；
  当时 workclf 类简单图在 NPU 上正常 cls_match 99.96%）。
  本次 REATT（含 209 个 SW epoch 的混合图）复现同一类退化。

### 建议（部署决策）

1. **交付形态维持全 CPU 编译形态**（EMS02_CHIL_REATT8_CPU，已验收：64/64 有限、
  L2≈L0、mock 闭环 Δfuel −0.06%）——NPU 路由当前被工具链/NPU 语义层阻塞；
2. NPU 线后续选项（需独立立项/外部支持）：
   a. ST 工具链级支持（Neural-ART int8 累加/重整化语义取证）；
   b. 升级 SDK（5.x / LiteRT 系）重试；
   c. **分体网络实验**：卷积重段（TCN 分支）保留 CPU、Dense/attention 段
      （workclf 类简单图，历史 NPU 正常）走 NPU——需切图与中间张量接口设计。
3. 遗留硬件待办：ST-LINK 对 NOR 的**读取通路**在本次会话末段失效（写/校验此前
   正常；0x70000000/0x702F0000/0x70400000 全区域读失败，需另行排查 ST-LINK/加载器/
   板卡状态）；workclf NOR 权重区完整性核查因此未完成。
