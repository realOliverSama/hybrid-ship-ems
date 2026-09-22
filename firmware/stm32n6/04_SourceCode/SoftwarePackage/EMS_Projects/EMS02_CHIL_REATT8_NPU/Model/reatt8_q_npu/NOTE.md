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


## 板端退化排查全账（2026-09-23，每条均有板级/离线证据）

症状：**输出与输入解耦**——m 通道恒 0（int8 −128≡zp）、跨向量不变；a/b 近常数。
工具自打印与 Q.json 均证实固件 I/O 契约、量化参数、反量化口径**逐项正确**；
ONNX 在 ORT 上语义正确。以下假设逐一实验并**排除**：

| # | 假设 | 实验 | 结果 |
|---|---|---|---|
| 1 | 图形态（Pad 分解）致编出错误图 | v2 中间维 Pad 手术 vs **v3 Concat 零填充**（无 Pad） | 两者同样退化 → 排除 |
| 2 | 输入缓冲 cache 契约违反（用户缓冲必须 clean+invalidate） | 固件补 `SCB_CleanDCache`（官方 cache maintenance 文档契约） | 仍退化 → 排除（契约缺陷已修复保留） |
| 3 | Softmax SW 岛交接破坏 | `--expand-softmax`（5 SW → 0，纯 HW epochs 218→234） | 仍退化 → 排除 |
| 4 | 权重池地址非 1MB 对齐（ATON 取数对齐假设） | 池迁 **0x70300000**（1MB 对齐，回读逐字节一致） | 仍退化 → 排除 |
| 5 | 量化参数/输出口径错 | 工具 Q.json：输出 SIGNED int8 scale [3.620067] zp [−128]，与固件一致 | 排除 |
| 6 | I/O 绑定未生效 / 输入未送达 | get_info/set_inputs 链路 + 缓冲内容自检打印 | 排除 |
| 7 | 激活溢出到未初始化的 HyperRAM（Release 无 XSPI1 init） | 生成报告 hyperRAM 占用 = 0 B，全在 npuRAM3-6 | 排除 |
| 8 | 权重未正确烧录 | 各版回读哈希逐字节一致（含旧池/新池） | 排除 |
| 9 | 与 REATT 共存致 workclf NPU 失效 | 冷启动首命令 WC_TEST 即失败（无共存） | workclf 今日也异常（历史上正常，另需排查） |

**结论**：本工程侧可查项已全部排除；衰竭点位于 **ST Neural-ART 编译/执行语义层**，
与项目 2026-09-03 对 TCN 的 NO-GO 判定同类（当时 TCN NPU 形态输出部分偏差 34%/47.7%
exact；本次 REATT 全退化——程度更重，可能同根或另有工具链缺陷）。

**未试杠杆（均为工具链/外部资源级）**：① 新版 STEdgeAI（5.x/LiteRT 线）重试——本机
仅有 4.0.1；② 直接调用底层 `atonn` 编译（FAQ 确认可行，中间 ONNX+Q.json 在
`st_ai_output/`）做优化开关消融（如 `--Ono-clone-dma`、去 `--enable-virtual-mem-pools`、
降 `--optimization`）；③ `--mode target` 官方直连验证（需按其 USB-CDC 文档构建 ST 验证
固件）；④ **分体网络**：卷积段（TCN）留 CPU 形态、Dense/attention 段（workclf 类简单
图，历史 NPU 正常）独立编译上 NPU——最多可硬件化 ~70% MAC。

---

## 2026-09-23 — 分体网络实验（用户裁决路线；未试杠杆 ④ 的落地）

**动机**：把"有问题的那一部分（TCN）放 CPU，其余能上 NPU 的尽量上 NPU"——若 N2
（无卷积的 Dense/attention 段）正常，则根因锁定 TCN；若 N2 同样退化，则 NPU 线
定论关闭。

**实施**（`Host/tools/npu/npu_export_split.py` + `npu_quantize_split.py`，固件形态
`EMS_REATT_FORM=SPLIT`）：

| 段 | 参数 | 编译形态 | I/O 契约（生成物实测） | 权重/激活 |
|---|---|---|---|---|
| N1 = TCN 支路 | 168,864 | CPU（`part_n1/reattn1c.*`，编译自 `reatt_n1_int8.onnx`） | IN 3300×f32 / OUT 96×f32 | 权重 584,392 B 入 ELF；激活 283,776 B |
| N2 = Future+Fusion+Decoder | 843,971 | NPU（`part_n2/reattn2n.*`，编译自 `reatt_n2_int8.onnx`） | IN kf 180B int8 / h_past 384B **f32** / OUT 90B int8 | 权重 atonbuf 877,009 B → NOR 0x70300000（83.6%）；激活 1.08 MB npuRAM3-5 |

拆分保真（host，fp32 两段链 vs 整图 keras）= **0.015 kW MAE**（拆分本身无损）；
int8 链 29.6 kW = TCN 段 20.5 + N2 段 16.1（整图 int8 23.5 kW）。

**板上结果（Release，appli 逐字节校验、N2 权重 sha 一致、FSBL 前后未变）**：

1. **N1（CPU 形态）正常**：`RS-N1` 诊断行 h_past 非退化且随输入变化
   （hp[0..3]=2.1481,2.1481,0.0000,1.5343；min −1.0434 / max 15.0980）。
2. **N2（NPU 形态）退化**：整窗 30 lead 输出为**同一常数三元组**（int8 码
   (−128,−111,−111)），跨 12 个不同输入窗 m 通道恒 −128（量化零点）；同一输入
   两次调用结果不同（另一次 (−128,−28,+12)）⇒ 与输入无关 + 非确定。
3. **判别实验**：workclf（简单图、NOR 权重 0x70400000，本平台历史正常）在本 appli
   `CMD_WC_TEST` **全 NACK ERR_INFER**（40/40，`Host/results/reatt_board/wc_test_report.json`）
   ——与假设台账 #9 记录一致（历史上正常）。
4. 板上 L1：拆分形态 q50 MAE **288 kW**（整图 NPU 347 kW），同档退化。

**推论**：① TCN **不是**根因（N2 无卷积同样退化）；② N2 与 workclf 同时失败，指向
**共享集成路径**（NPU 侧 NOR 权重可达性 / NPU cache 属性 / EC 运行时）而非图复杂度，
但 N2 权重经回读校验与源文件逐字节一致、N1 同源 NOR 读路径（ELF .rodata 在 NOR）
在 CPU 侧正常 ⇒ 剩余可疑点集中在 **NPU 作为 master 的 NOR 通路与缓存属性**。

**下一步（需 Dev boot）**：① 读回 0x70400000 workclf 权重区与
`Binary/network-data-workclf.hex` 比对（台账遗留项）；② 带插桩重建：打印 workclf
失败步骤的 stai rc 码 + N2 原始 90B + CPU 侧 NOR 权重校验和；③ 若 workclf 修复
（权重区缺失）而 N2 仍退化，则 N2 的问题回到图/工具链层。
