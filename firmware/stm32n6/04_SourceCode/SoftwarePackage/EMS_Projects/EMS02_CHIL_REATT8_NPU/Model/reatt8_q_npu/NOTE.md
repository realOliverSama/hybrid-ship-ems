# reatt8_q_npu — REAT-T **NPU 形态**部署包（ST Edge AI Core 4.0.1，Neural-ART）

## 2026-09-23 safe 重编（未上板）

`user_neuralart_safe.json` + `my_mpools/service_n2_safe.mpool`：去掉
`--enable-epoch-controller`、`--enable-virtual-mem-pools`、`--Ocache-opt`、
`--Omax-ca-pipe`、`--expand-softmax`；npuRAM 各 bank 447KB（末尾留 1KB）；
xSPI2 `CACHEABLE_OFF`。产物在 `npu_safe/`（`dl3/`、`n2/`、`n2_nofloat/`）。

- `dl3s`：0 个 EC blob，只剩 1 个 SW `Tile(float)`。原先卡住的 16 字节 stub
  （`…003d`，`<no executed epochs>`）没有了。
- `reattn2s` 与去掉 `--native-float` 的 `reattn2f` 调度相同：120 SW + 17 hybrid
  + 168 HW，**0 个 EC blob**（旧 `reattn2n` 是 56 个）。9 个 `Sqrt(float)` 仍在。
  权重 `reattn2s_atonbuf.xSPI2.raw` 913,057 B @ 0x70A00000，与已烧的 877,009 B
  不是同一块，未替换固件、未烧录。本机这次没有 ST-LINK。

## 2026-09-24 float-LN 部署变体（L4 = `dl4f`）

`npu_bypass_ln_qdq.py` 拆掉 9 个 LayerNorm 的激活 Q/DQ（方差/sqrt 留浮点）→
`npu_safe/reattn2n_lnfloat.onnx`；`npu_export_ladder_l4f.py` 以它为源按 dl4 同边界
重切 L4 → `ladder/dl4f/`（net 名 `dl4f`），并把阶梯固件的 l4 级从 dl4 换成 dl4f。

配套目录一览（本包新增）：

| 目录/文件 | 用途 |
|---|---|
| `ladder/` | N2 逐级 bisection 阶梯（`dl1`/`dl2`/`dl2c`/`dl3`/`dl4`/`dl4f`/`dl4c`/`dlf1`/`dlfut`/`dlfutc`）；每级含 stedgeai 生成码 + ORT golden + 编译报告 |
| `ladder/_full_with_cuts.onnx` | 带全部切点标记的 N2 整图（切子图的公共源） |
| `my_mpools/` | 各编译形态的 memory pool 配置（`service_n2*`、`ladder_dl*`） |
| `part_n2/` | N2 子图源（`reattn2n_src.onnx`）+ 其 int8 编译件 |
| `selftest/` | REATT 自检小图（`reatt_selftest_int8.onnx`），上板首通冒烟用 |
| `npu_safe/` | safe 配置重编产物（`dl1s`/`dl3`/`n2`/`n2_nofloat`/`n2_lnfloat`），CMakeLists 的 SPLIT+Release 形态直接引用其 `.c/.h` |
| `user_neuralart*.json` | stedgeai 编译配置（`_safe` 去 epoch controller、`_nofloat`、`_dl1`） |

> 入库约定：`ws/`、`st_ai_ws/`、`st_ai_output/` 为 stedgeai 生成器临时工作区，
> 可由上述脚本重跑复现，不入库（当前由本地 `.git/info/exclude` 排除；仓库
> `.gitignore` 未改动，是否升格为仓库级规则待定）。


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

### 附：N2 输出退化结构指纹（2026-09-23 板上实测）

| 观测量 | 实测 |
|---|---|
| 窗内结构 | 30 lead 的三元组**完全相同**（m/a 通道各只有 1 个码值；b 通道仅 ±1 LSB 抖动，7~8 个值） |
| 常数值 | m = a = −128（= 量化零点 → 0 kW），b ≈ −128；host 参考同窗 = (−109,−103,−99) ≈ 100 kW |
| 跨调用漂移 | a: −28 → −111 → −128（单调下沉）；b: +12 → −111 → +14/15 |
| 重复性 | 同一输入 ×6：三元组在 ±1 LSB 间变化（b 通道）——**非完全确定** |
| host 反演 | 权重置 0 / −1 / +127 / −128 后 ORT 重放：输出塌为常数行，但**均不匹配**板上三元组 ⇒ 单纯"权重读成某常数"不足以解释 |

⇒ 判据结论：N2 的 NPU 执行**既不反映输入、也不反映权重语义**；结合 workclf 同 appli 失败，
指向 NPU 侧数据通路（NOR 权重可达性 / NPU cache 属性 / EC 运行时）而非图结构或量化。

---

## 2026-09-23（夜备）— N2 逐级 bisection 阶梯（晨间判别实验，一次烧录全部跑完）

**前提（已实证）**：NPU 通路正常（自检图输出=输入逐字节；workclf 归一化口径 40/40）；
N2 图输出退化为与输入无关的常数三元组。appli 读 NOR 三区校验和与 host 一致（RS-NOR）。

**阶梯设计**（`Host/tools/npu/npu_export_ladder.py`；全部独立 `--name`，同固件共存）：

| 级 | 网络 | 切点（源图 float 张量） | 考察对象 | NOR 池 |
|---|---|---|---|---|
| L1 | dl1 | kf_input → lead_pos | kf 输入通路（Transpose/DQ/Reshape/Dense/位置加） | 0x70410000 |
| L2 | dl2 | LN1 输出 → o_proj 输出 | 单块 attention（Q/K/V、QKᵀ、scale、Softmax、AV、o_proj） | 0x70440000 |
| L3 | dl3 | h_past_input → h_tile | **头号嫌疑**：Unsqueeze/Reshape/Q/DQ/Slice/Tile（30-lead 全同指纹） | （无权重） |
| L4 | dl4 | {fut_seq, h_tile} → raw | fusion+LN+Dense+decoder 头部 | 0x70600000 |
| F1 | dlf1 | 整图重编译 + **--Ono-clone-dma** | 编译选项消融（clone_dma 为 4.0.0 新默认） | 0x70800000 |

golden：部署口径输入（kf 归一化 + reatt_n1_int8 的 h_past）过整图一次取全部切点张量；
每级再用 ORT 跑该级子图得 golden 输出；固件运行时读边界 scale/zp 自适配，
float 域比对（S8 输出容差 2×步长；f32 输出 5%×(1+max|exp|)），单行判决 RS-Lx PASS/DIFF。

**地址规划（全 32MB NOR，占位检查零重叠）**：
appli 0x70100400..0x7036E814（阶梯使其从 1.65MB 涨到 2.55MB）→ **服务 N2 权重池被迫
迁 0x70300000 → 0x70A00000**（`my_mpools/service_n2.mpool`；reattn2n 已重编，hex 更新）；
selftest 0x703E0000；workclf 0x70400000（未动）；dl1/dl2/dl4/dlfut/dlf1 见上表
（dlfut = fut 支路整段 kf→fut3，0x70700000，为"Tile 坏→fut(NPU)+Tile(固件复制)+fusion(dl4)"
修复预案预备）；dl2c/dl4c/dlfutc = 各段 **CPU 形态兜底编译**（已生成未接线，
权重入 ELF，坏段→CPU 时即刻组装）。

**晨间流程**：Dev boot → `Host/tools/flash_ladder.sh`（一键 7 项全带 -v）→ 切 Flash boot
冷启动 → `python3 Host/tools/ladder_morning.py --port /dev/cu.usbserial-1120` → 判决表自动输出。

**判决树**：F1 PASS ⇒ clone_dma 即根因（服务 N2 重编译加选项即修）；首个 DIFF 级 =
首个坏段 ⇒ 该段退 CPU 形态（dl2c/dl4c/dlfutc 已备），其余留 NPU；L1-L4 全 PASS 而
F1 DIFF ⇒ 图结构无错，第二轮编译选项消融。

---

## 2026-09-23（终）— 阶梯判决闭合：S0（golden 输入 → 服务 reattn2n）0/90 逐字节一致 ⇒ REATT 图本身在 NPU 上算错；切子图（dl3 全 f32、仅 Tile）stall 于 run_continue ⇒ 分区验证路线被堵死

**晨间第二轮（官方绑定流修复后）全部结果**：

| 实验 | 结果 | 含义 |
|---|---|---|
| RS-SELF | 输出=输入 逐字节 ✓ | NPU 执行链路正常 |
| workclf | 40/40 ✓ | 同上（简单 Dense 图） |
| **RS-S0**（golden 输入 → 服务 reattn2n，服务生命周期） | **0/90 逐字节一致**（m 通道恒 −128 单值 vs golden 7 个唯一值） | **REATT 图自身在 NPU 上算错**，与 N1 的 h_past 无关（输入毒化假设排除） |
| L3（h_past→Tile，全 f32、无权重、无 MatMul） | **stall 于 run_continue**（@run 后无输出，挂死） | **切子图根本不执行** ⇒"已验证分区"策略不可实现 |
| L1/L2/L4/F1 | 同样 stall（被 L3 挡住未到，前轮亦 stall） | 同上 |

**三级最终定性**：
1. NPU 正常（自检图/workclf）；
2. REATT 整图 NPU（reattn2n）：可启动（175 ms）、可 DONE，但**计算结果错误**（golden 输入下 0/90）；
3. REATT 切子图：**无法执行**（run_continue 内部停滞）。

⇒ **REATT-on-NPU 于 ST Edge AI 4.0.1 = NO-GO（证据链闭合）**。与简单图（workclf/selftest）
可执行形成对照：失败特定于 REATT 级图的规模/结构（多 epoch/attention/Tile 组合），
而非 NPU 集成或量化口径。分区计算（把已验证正确的段放 NPU）在"切子图不执行"面前
不可实施。

**可递交 ST 支持的取证包**：① S0 golden 输入 + 0/90 输出（部署口径）；② 切子图
（dl3，全 f32 仅 Tile）run_continue 停滞复现步骤；③ workclf 同板 100% 对照；
④ 整图与切图的 analyze/generate 报告与 OE ONNX。

**交付结论**：最快可用形态 = **整图 CPU**（EMS02_CHIL_REATT8_CPU，0.8 s/窗，已验收）。
后续可选：① N1 CPU 形态改从 TFLite 原生 int8 图编译（当前 ONNX-QDQ 图 967 ms/56.7M MACs
显著慢于整图 800 ms/192.9M，存在数倍优化空间）；② 新版 STEdgeAI 重试本阶梯全套工具；
③ ST 支持取证包递交。
