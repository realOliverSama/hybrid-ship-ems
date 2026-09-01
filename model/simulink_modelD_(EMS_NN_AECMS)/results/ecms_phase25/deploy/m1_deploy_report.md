# M1 部署报告（T-507 / phase25 T-311 验收文档）

| 项 | 内容 |
|---|---|
| 工程 | EMS01_CHIL_ECMS（Model D 混合动力船舶 EMS，NPU 后端 v1，CHIL 里程碑 M1） |
| 任务出处 | `.specs/ems01-chil-ecms-v1/tasks.md` T-507 第⑦步；验收链条：`.specs/nn-predictive-ecms-phase25/tasks.md` T-311（八项验收内容） |
| 板卡 | AlienTek ATK-CNN647B（STM32N647：Cortex-M55 @600 MHz + Neural-ART NPU；外挂 MX25UM25645G OctoSPI NOR 256 Mbit = 32 MB） |
| 数据采集 | 2026-08-31 ~ 2026-09-01 板上会话（用户在场，阶段隔离授权） |
| 报告日期 | 2026-09-01（由 delegate 按主 Agent 委派数据表 + 数据源复核起草） |
| 路径约定 | `WS` = `model/simulink_modelD_(EMS_NN_AECMS)`；`FW` = `firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS`；`deploy` = `WS/results/ecms_phase25/deploy` |
| 状态 | T-311 八项验收数据齐备；FR-511 清扫后干净镜像复测结果留占位（§8.3 / §9.1，标记见文内），待主 Agent 板上会话回填 |

## 1. 摘要与 T-311 验收总表

phase25 spec T-311 要求：*int8 模型大小 / activation RAM / weight memory / Neural-ART HW coverage / SW·HYBRID fallback 算子清单 / 实测时延 / 量化精度差 / 板上输出 vs Python reference 一致性*。逐项结论（详细数据与出处见对应章节）：

| # | T-311 验收项 | 结论 | 关键数据 | 正文 |
|---|---|---|---|---|
| 1 | int8 模型大小 | 数据完备 | mlpq：tflite 文件 26.2 KB（fp32 tflite 280.2 KB）；analyze weights 21,537 B（较 float 模型 -75.0%）。workclf：tflite 21.9 KB（keras fp32 147.5 KB → int8）；analyze weights 8,129 B（-74.2%） | §3 |
| 2 | activation RAM | 数据完备 | mlpq 1,518 B（1.48 KiB）@npuRAM3；workclf 808 B @npuRAM3（analyze 原值） | §3 |
| 3 | weight memory | 数据完备 | mlpq 21,537 B @octoFlash 0x70200000-0x70205430；workclf 8,129 B @octoFlash 0x70400000-0x70401FD0；各居 1 MB mpool 池 | §3 |
| 4 | Neural-ART HW coverage | 数据完备 | mlpq：6 epochs = 4 EC(NPU HW) + 2 SW；workclf：22 epochs = 16 NPU/EC + 6 SW | §4 |
| 5 | SW/HYBRID fallback 算子清单 | 记录完备，design 预期内 | mlpq：QuantizeLinear、DequantizeLinear（+大 GEMM epoch 实测于 CM55 以 SW GEMM 执行）；workclf：QuantizeLinear、Pad×3、Softmax、DequantizeLinear（小 chunk，合计 <10 μs，非瓶颈） | §4 |
| 6 | 实测时延 | PASS（按 2026-09-01 修订预算） | mlpq 129 μs ≤ 1 ms；workclf 42.5 ms（42~46 ms）≤ 50 ms；合计 ≤ 60 ms（1 Hz 控制拍占用实测 4.2%） | §5 |
| 7 | 量化精度差 | 数据完备（记录性） | mlpq val RMSE fp32 9.9292 → int8 9.9659 kWh（+0.37%）；workclf val acc 84.39%→84.37%（-0.02 pp）、test acc +0.01 pp | §6 |
| 8 | 板上 vs Python 一致性 | PASS（FR-504 修订门禁，全项有余量） | mlpq ×200：p50=1 / p90=2 / p99=5 / max=6 LSB，均值口径 PASS，无偏置无发散，RMSE 比 0.0102；workclf argmax 一致 200/200 = 100%（门禁 ≥99%） | §7 |

**总评**：T-311 八项验收数据全部齐备且结论为 PASS／数据完备；FR-504 硬门禁全项通过有余量；FR-505 按 2026-09-01 修订预算通过（修订记录见 `.specs/ems01-chil-ecms-v1/design.md`）。唯一未闭环项为 FR-511 清扫后干净镜像复测（占位待回填，不阻塞上述八项）。

## 2. 部署对象与工具链

### 2.1 部署模型

| 模型 | 角色 | 结构 | 来源与只读副本 |
|---|---|---|---|
| `mlpq` | MLP-Q 分位数负荷预测器（q10/q50/q90） | 输入 f32(1×302) → 输出 f32(1×3)；params 21,571 | T-307 选定 run_id `mlp_h64_32_lr3e-4_q`；源 `WS/results/ecms_phase25/models/mlp_h64_32_lr3e-4_q/mlp_h64_32_lr3e-4_q_int8.tflite`，sha256 `56fc3488ff0b210c…cc0c32`；只读副本 `FW/Model/mlp_q/mlp_q.tflite`（2026-09-01 复核两副本 sha256 一致） |
| `workclf` | 工况识别器（Conv1D，7 类：depart/harbor/eco/rush/bridge/upstream/berth） | 输入 f32(1×2) + f32(1×30×1) → 输出 f32(1×7)；params 7,871 | 真 int8 重导出 2026-08-31（缺陷修复：旧 `workclf_v1_int8.tflite` 实为 float32 误标，改名 `workclf_v1_float.tflite` 留证；权威记录 = `WS/results/ecms_phase25/classifier/workclf_int8_report.json` 的 `defect_record`，工程侧存档 `FW/Model/workclf/NOTE.md`） |

### 2.2 工具链与运行库

- 编译工具链：ST Edge AI Core **v2.2.0-20266**（build `2adc00962`，对应 STM32CubeAI 10.2.0-RC1）——两份 `FW/Model/*/analyze_report.txt` 报告头部原值；Neural-ART 编译器 1.1.1-14。
- 运行库：**`NetworkRuntime1020_CM55_GCC.a`**（工程内 `FW/Middlewares/STEdgeAI/Lib/GCC/ARMCortexM55/`，即 ST Edge AI 2.2.0 安装自带中间件的最小自洽子集；`FW/Appli/CMakeLists.txt:269` 链接项与两份 analyze 报告 section 表一致）。
  - **勘误【待核，请主 Agent §14 确认】**：委派数据表记为 "vendor NetworkRuntime1000_CM55_GCC.a"。经源文件核实：vendor 包 `Middlewares/AI` 下的 NetworkRuntime1000 属 atonn 1.0.0 旧代中间件，与 stedgeai 2.2.0 生成代码（atonn 1.1.1-14）跨代不兼容；2026-08-31 "路径 A" 裁决（用户 + 主 Agent）明确**不得引用**（`FW/Appli/CMakeLists.txt:157` 注释原文）——实际部署并链接的是工程内 `NetworkRuntime1020_CM55_GCC.a`（`Appli/CMakeLists.txt:269`）。本表按源文件记载，提请主 Agent §14 确认勘误。
- 烧录工具面：STM32CubeProgrammer v2.23.0 + vendor 外部加载器 `MX25UM25645G_ATK-CNN647B.stldr`（sha256 `0bce846a…`，烧录前预检校验，见 `deploy/nor_evidence_t507/stage1_readonly_log.txt`）。

### 2.3 mpool 权重布局（NOR）

| 模型 | NOR 基址 | 池 | hex 产物 |
|---|---|---|---|
| mlpq | `0x70200000` | 1 MB（octoFlash 池 0x70200000-0x70300000，analyze 占用 2.05%） | `FW/Binary/network-data-mlpq.hex` |
| workclf | `0x70400000` | 1 MB（octoFlash 池 0x70400000-0x70500000，analyze 占用 0.78%） | `FW/Binary/network-data-workclf.hex` |

两模型权重于 2026-08-31 21:13 **同一烧录会话内一次完成**（T-507 stage 2，本工程唯一 Level C 事务，授权令牌 `PROGRAM_AUTHORIZED_E7576952_786A905D` 单次有效；FSBL 区前后零改变，见 §9）。

## 3. 资源占用（stedgeai analyze 报告原值）

数据源：`FW/Model/mlp_q/analyze_report.txt`、`FW/Model/workclf/analyze_report.txt`。

| 项 | mlpq | workclf |
|---|---|---|
| weights (ro)（analyze 口径） | 21,537 B（21.03 KiB），1 段；较 float 模型 -64,747 B（-75.0%） | 8,129 B（7.94 KiB），1 段；较 float 模型 -23,355 B（-74.2%） |
| weight 存放（octoFlash） | 0x70200000-0x70205430（1 MB 池占用 2.05%） | 0x70400000-0x70401FD0（1 MB 池占用 0.78%） |
| activations (rw) | 1,518 B（1.48 KiB）@npuRAM3（0x34200000-0x342005F0，域 448 KB 的 0.33%） | 808 B @npuRAM3（0x34200000-0x34200330，0.18%） |
| 网络代码+运行库（section 口径，TOTAL） | FLASH (ro) 43,238 B / RAM 1,531 B | FLASH (ro) 65,830 B / RAM 2,557 B |
| tflite 源文件（文件级） | 26.2 KB（fp32 tflite 280.2 KB，T-307 报告） | 21.9 KB（keras fp32 147.5 KB → int8 21.9 KB，T-312 报告） |
| xSPI2 权重 bin | 21.0 KB（`network_data.xSPI2.bin`） | 7.9 KB |
| 输入 → 输出 | f32(1×302) → f32(1×3) | f32(1×2) + f32(1×30×1) → f32(1×7) |

> **口径说明**：报告同时存在"文件级大小"与"analyze weights 口径"两组数字（tflite 容器含 runtime/量化元数据，大于裸权重），表内分列不混用。

## 4. HW coverage 与 SW fallback（T-311 第 4、5 项 / FR-502）

数据源：两份 `analyze_report.txt` 的 "Epochs details" 原表。

| 模型 | 总 epoch | NPU HW | SW fallback | SW 算子清单（analyze 原表） |
|---|---|---|---|---|
| mlpq | 6 | 4（EC） | 2 | QuantizeLinear（epoch 1）、DequantizeLinear（epoch 6） |
| workclf | 22 | 16（analyze 表 15 个标 EC，epoch 1 标 `??`，均非 SW） | 6 | QuantizeLinear、Pad×3、Softmax、DequantizeLinear |

- workclf 的 6 个 SW 回退全部落在小 chunk，**合计 <10 μs，非时延瓶颈**（§5 归因实证）；conv1d SW fallback 属 design 预期（design FR-504 条目明文），已列入 FR-502 fallback 清单。
- FR-505 归因补充（analyze 静态口径之外）：mlpq 的**大 GEMM epoch 实际由 CM55 以 SW GEMM 执行**（单 chunk 55,394 CYCCNT ≈ 92 μs，0.2 GMAC/s 量级）→ mlpq 属 **CPU 活跃型**（§5）。
- fallback 清单与 design 预期一致，无未记录的新增 fallback。

## 5. 实测时延与归因（FR-505，DWT CYCCNT）

### 5.1 预算（design 2026-09-01 修订，用户批准）

| 项 | 原预算（tasks T-507 ⑤ 拍定） | 修订预算（2026-09-01） | 修订依据 |
|---|---|---|---|
| mlpq | ≤10 ms | **≤1 ms** | 原值为无推导拍定值（与 FR-504 的 0.25 kWh 同模式） |
| workclf | ≤10 ms | **≤50 ms** | 归因实证：42 ms 中 99.8% 为 CPU WFE 等 NPU 的执行墙钟，NPU 域时钟 400 MHz 下原值不可达 |
| 合计 | ≤20 ms | **≤60 ms**（1 Hz 控制拍占用 ≤6%，实测 4.2%） | design FR-505 变更记录（2026-09-01，用户批准） |

### 5.2 实测值

| 模型 | 实测 | 门限 | 结论 | 数据源 |
|---|---|---|---|---|
| mlpq | **129 μs**（含归一化 + NPU 推理全链路） | ≤1 ms | PASS | 进展日志 2026-09-01（下午）续②；RSP 遥测 `us` 字段：8 帧指纹实验 128~130 μs，干净链路 ×200 归档 127~128 μs（均值 127.2，`deploy/m1_infer_test/x200_clean_seq.json`） |
| workclf | **42.5 ms**（×200 范围 42~46 ms，均值 42.1 ms） | ≤50 ms | PASS | `deploy/m1_infer_test/wc200_clean.json`（wc_us 41,984~46,080 μs）+ 归因会话墙钟 42.5 ms |

合计 ~42.6 ms ≤ 60 ms → 1 Hz 控制拍占用实测 **4.2%**（design 修订记录口径）。

### 5.3 归因（SWD 读 `ems_epoch_cycles` + `fr505_epoch_dump.txt`，进展日志 09-01 续②）

- **workclf（42.5 ms 墙钟）**：15 个有效 chunk CPU 活跃 CYCCNT 合计 46,532 ≈ **78 μs @600 MHz** → **99.8% 时间 CPU 在 WFE 睡眠等 NPU**（NPU 执行墙钟主导）；6 个 SW 回退 epoch 全在小 chunk（合计 <10 μs），SW 回退不是瓶颈。
- **mlpq（129 μs）**：6 chunk CPU 活跃 60,728 周期 ≈ 101 μs，其中大 GEMM 单 chunk 55,394 周期 = **92 μs**（SW GEMM 于 CM55 执行，0.2 GMAC/s 量级）→ mlpq 属 **CPU 活跃型**、workclf 属 **NPU 执行型**，两者 ~300 倍差异机理完全解释。
- NPU 域时钟 SWD 实证：CFGR1=0x33330000 → SYSSWS=11（sysd=ic11）；IC11CFGR=0x00020000 → IC11=PLL1/3 = **400 MHz**（vendor 991 例程为 PLL3/2=800 MHz）→ EMS01 sys_npu_ck 上限 400 MHz。频率提升（对齐 991 800 MHz 或 RM 上限 1 GHz，收益估计 42 ms → ~17 ms）已裁决 **Phase 3 不做**（design FR-505 变更记录）。

> 归因原始档 `deploy/m1_infer_test/fr505_epoch_dump.txt` 存在 UART 编码混叠（乱码 + 二进制残留），决定性数据来自 SWD 读取 `ems_epoch_cycles`（逐 chunk 数据见进展日志 09-01 续②），引用以日志记载为准。

## 6. 量化精度差（T-307 / T-312 口径）

| 模型 | 指标 | fp32 | int8 | 差 | 说明 |
|---|---|---|---|---|---|
| mlpq | val RMSE | 9.9292 kWh | 9.9659 kWh | **+0.37%** | 量化校准 = val 前 1000 样本（仅校准），test 未参与；模型结构/语义未变 |
| workclf | val accuracy | 84.39% | 84.37% | -0.02 pp | 权重固定仅重导出、未重训练（`--reuse` 语义） |
| workclf | test accuracy | 79.87% | 79.88% | +0.01 pp | 同上 |

出处：`WS/results/ecms_phase25/t307_int8_report.json`（MLP-Q 条目 float_val / int8_val，2026-08-29）、`WS/results/ecms_phase25/classifier/workclf_int8_report.json`（float_vs_int8）。

## 7. 板上 vs Python 一致性（FR-504，2026-09-01 修订门禁）

### 7.1 门禁（design 固定，2026-09-01 修订版）

原门禁 max |Δq50| ≤ 0.25 kWh 为无推导拍定值，小于 1 LSB（= 0.2536 kWh），字面等效要求 NPU 与解释器遂位一致，与 design R3（承认定点累加顺序差异）矛盾 → 2026-09-01 经用户批准修订为 LSB 分布形式。**1 LSB = 冻结 mlpq 输出张量 scale 0.015376422 × y_std 16.495129 = 0.2536 kWh**。

| 分项 | 门禁 |
|---|---|
| 均值（绝对偏差） | ≤1.5 LSB |
| p99 / max | ≤6 LSB / ≤8 LSB |
| 符号偏置 | 符号均值绝对值 ≤0.5 LSB |
| 发散 | 无（无 NaN/野值） |
| board-vs-python RMSE 比 | ≤1.1 |
| workclf | argmax 类别一致率 ≥99%（logits 差报告性记录） |

### 7.2 mlpq ×200 实测（`deploy/m1_infer_test/x200_clean_seq.json`；200/200 seq 配对，val split seed=20260831 向量）

- Δq50 LSB 分布：0 LSB×62、1×87、2×38、3×8、4×1、5×3、6×1（合计 200）。
- 分位数（绝对偏差）：p50 = 1、p90 = 2、p99 = 5、max = 6 LSB（6 LSB = 1.52 kWh）。
- 均值：mean(|Δ|) = **1.06 LSB**（门禁 ≤1.5）；符号均值 -0.110 LSB（无偏置门禁 ≤0.5）→ PASS。design 2026-09-01 变更记录所记 "mean 1.06 LSB" 即 mean(|Δ|) 口径，与上述分布自洽。
- 符号对称：负 75 / 正 63 / 零 62 → 无系统性偏置；相对偏差 p50 0.79% / max 10.76%；board-vs-python **RMSE 比 0.0102**（门禁 ≤1.1）。
- **全项 PASS 且有余量**（design 2026-09-01 变更记录同口径："mean 1.06 LSB、p99 5 LSB、max 6 LSB，RMSE 比 0.0102，符号对称无偏置，全项通过且有余量"）。
- 定性结论：典型 1 LSB、无偏置、无发散的定点累加噪声分布；NPU 定点 vs TFLite 解释器差异属定点实现固有属性（design R3），不构成部署障碍。

### 7.3 workclf 一致性（`deploy/m1_infer_test/wc200_clean.json`）

- argmax 类别一致 **200/200 = 100%**（门禁 ≥99%）→ PASS；wc_us 42~46 ms（§5）。
- 每条记录含 7 类 logits 原值（报告性归档，无门禁）。

### 7.4 污染数据警示

同目录 `deploy/m1_infer_test/infer_compare.csv` / `infer_compare.json` 为**早期污染数据**（主机 USB-CH340 链路级联节流所致：100/200 超时、max Δq50 达 ~44.9 kWh、硬门禁 false），**一律勿引用**；干净数据以 `x200_clean_seq.json` / `wc200_clean.json` / `fr505_epoch_dump.txt` 为准（进展日志 09-01 续②交接明文）。

## 8. FR-511 换模型流程（how-to + 本次实证）

### 8.1 标准流程（以 mlpq 为例，2026-09-01 晚实证走通）

1. **再生成部署包**：`cd FW/Model/mlp_q && STEDGEAI_CLI=<锁定的 stedgeai v2.2.0 CLI> ./generate-n6-model.sh` → 产物四件 `network.c` / `network.h` / `network_ecblobs.h` / `network_data.xSPI2.bin` + 权重 hex `FW/Binary/network-data-mlpq.hex`（脚本内 `WEIGHTS_BASE=0x70200000` 必须与 `my_mpools/stm32n6.mpool` xSPI2 池基址一致）。
2. **权重哈希比对（判定是否需重烧 NOR）**：sha256(新 `network_data.xSPI2.bin`) 对比 NOR 权重区回读哈希（或 `deploy/nor_evidence_t507/stage2_hashes.txt` 存档）。
3. **按 8.2 判定树处置**（是否触发 Level C 重烧）。
4. **重构建**：`python3 firmware/stm32n6/04_SourceCode/SoftwarePackage/Tools/atk_stm32n6_cmake_prepare.py --build Debug`（仓库相对路径；路径 A 接线块不被 prepare/CubeMX 覆盖）。
5. **SRAM 加载**：debug-stm32n647-sram 工作流 + 授权令牌（阶段隔离，AGENTS.md §11）。
6. **INFER_TEST ×200 复测**：FR-504 门禁重跑；**LSB 阈值按新模型输出张量 scale × y_std 重推导**（design 明文：LSB 绑定冻结模型输出张量，换模型必须重推导）。

### 8.2 判定树（权重哈希 → 烧录决策）

```text
重新 generate 完成
 ├─ 新 network_data.xSPI2.bin 的 sha256 == NOR 在烧权重回读哈希
 │     → 权重未变：跳过 NOR 烧录（不触发 Level C），仅重构建 + SRAM 加载 + INFER_TEST 复测
 └─ sha256 不一致 → 权重已变：走 Level C 重烧（T-507 stage2 同款：授权令牌 →
       sector 擦除〔禁 Mass Erase〕→ 写 hex → vendor verify → 独立回读哈希比对）
       → 再重构建 + 加载 + 复测
```

### 8.3 本次实证结果（2026-09-01 晚：同一 `mlp_q.tflite` 重新 generate）

- 产物四件 `network.c` / `network_ecblobs.h` / `network_data.xSPI2.bin` / `Binary/network-data-mlpq.hex` **sha256 逐字节一致**；唯一差异为 `network.h` 的 `@date` 时间戳（2026-09-01 19:34:34）→ **工具链完全确定性**。
- 重构建 ELF sha256 `a1326b51…` 与上一镜像**逐字节一致**（构建同样确定）。
- **NOR 权重区无需重烧**：NOR 回读哈希 `0cca9d72…`（stage2 独立回读存档）与再生成 bin 一致，即 2026-08-31 烧录镜像仍有效。
- **清扫后干净镜像 INFER_TEST ×200 复测（d3ea1da2 板上，2026-09-01 晚）**：
  - 镜像身份：清扫后重建 ELF sha256 `d3ea1da2ec0b1d3b…`（工作区 build 产物与板上 SRAM 加载源一致）；
  - 注入 200/200，错误零：`timeout=0 / bad_rsp=0 / resend=0 / stale_skip=0`；
  - FR-504 LSB 分布复测：`mean_abs=1.06`（门 ≤1.5）/ `p99=5.0`（≤6）/ `max=6.0`（≤8）/ `|signed_mean|=0.11`（≤0.5），**hard_gate_fr504 = True**；`rmse_ratio=0.0102`（≤1.1）；
  - 与下午 a1326b51 ×200 数据**逐项一致** → 插桩清扫零回归，板上推理等价性闭环。
  - 同晚 WC_TEST 复测：注入 200/200 零错误，`cls_match_rate=1.0`（200/200），`max_logit_abs_delta=5e-7`。
  - 数据：`m1_infer_test/infer_compare.csv[.json]`（prefix `clean200`）、`wc_compare.csv[.json]`（prefix `wc200_retest`）。

> 结论：FR-511 换模型流程已实际走通（再生成 → 哈希判定 → 重构建 → SRAM 加载 → 复测框架），同模型重跑零副作用。换**新模型**时，除上述步骤外还须按新输出 scale 重推 FR-504 LSB 门禁并更新 analyze 资源记录（design 明文要求）。

## 9. 板上会话记录与证据链

### 9.1 镜像 / 令牌沿革简表

| 日期 | SRAM 镜像（ELF sha256 前缀） | 事件 | 出处 |
|---|---|---|---|
| 08-31 22:01 | `fad198b0` | stage 3 首次 SRAM Debug 加载成功（令牌 `…FAD198B0_R2`；R1 因 delegate 悬挂未生效）；banner + LED0 心跳证实运行 | `deploy/nor_evidence_t507/stage3_sram_load_log.txt` |
| 09-01 下午 | `76435b9d` | bug#4（RX 中断环）修复后 NPU 全链路首通（us_infer 128~130 μs） | 进展日志 09-01（下午）续② |
| 09-01 | `fc43db07` | B1/B2（seq 回显 + CMD_WC_TEST / logits 出参）；换线后 ×200 干净数据采集（零超时零重试） | 进展日志 09-01 续② |
| 09-01 | `a1326b51` | B3 FR-505 插桩（RunEpochBlock CYCCNT 环形数组），归因数据来源 | 进展日志 09-01 续② |
| 09-01 晚 | `a1326b51`（FR-511 重构建，逐字节一致） | FR-511 换模型流程实证（§8.3） | 本报告 §8 |
| 09-01 晚 | `d3ea1da2` | FR-505 插桩清扫后重建（fw_gate 7/7）；INFER_TEST ×200 复测全绿（mean 1.06/p99 5/max 6 LSB，hard_gate_fr504=True）+ WC ×200（cls 200/200），与 a1326b51 下午数据逐项一致 → 清扫零回归闭环；当晚另修复主机侧 pyserial read(size) 攒字节伪延迟（bug#5，详见进展日志） | 本报告 §8.3；进展日志 09-01（晚） |

### 9.2 T-507 三阶段证据（Level A → Level C → SRAM）

- **stage 1（Level A，只读）**：NOR 0x70200000 / 0x70400000 区域现状只读存档（`stage1_readonly_log.txt`，无擦写）。
- **stage 2（Level C，唯一）**：CubeProgrammer v2.23.0 sector 擦除（**非 Mass Erase**，[512-517]/[1024-1025]）+ 双 hex 写入 + vendor verify + **独立回读哈希比对全 PASS**：mlpq `0cca9d72…` / workclf `7fcbc865…`，FSBL 0x70000000 前 `961abaf1…` 后零改变；授权令牌单次有效（`stage2_hashes.txt` VERDICT: PASS）。
- **stage 3（SRAM Debug）**：`load_elf_to_sram` 计划/回读 SHA-256 一致（`f338556d…`），Reset_Handler == ELF entry，AIRCR 复位先于写镜像；全程零 Flash / Option Bytes / OTP / RDP 操作（`stage3_sram_load_log.txt`）。

### 9.3 证据归档清单

| 位置 | 内容 |
|---|---|
| `deploy/nor_evidence_t507/` | `stage1_readonly_log.txt`（Level A 现状）；`stage2_program_log.txt` + `stage2_hashes.txt`（烧录 + 独立回读：mlpq `0cca9d72…` / workclf `7fcbc865…`，FSBL `961abaf1…` 前后不变）；`stage3_sram_load_log.txt` |
| `deploy/m1_infer_test/` | `x200_clean_seq.json` / `wc200_clean.json` / `fr505_epoch_dump.txt`（**注意：同目录 `infer_compare.csv` / `infer_compare.json` 为早期污染数据，勿引用**） |
| `FW/Model/mlp_q/`、`FW/Model/workclf/` | 各自 `analyze_report.txt`（真 int8 报告；workclf float 时期报告已被同名覆盖，关键数值存其 `NOTE.md` 与 defect_record）；mlpq `generate.log`（2026-09-01 19:34 再生成，FR-511 确定性复核载体） |
| `FW/Binary/` | `network-data-mlpq.hex` / `network-data-workclf.hex` |
| spec / 日志 | `.specs/ems01-chil-ecms-v1/design.md`（FR-504/505 2026-09-01 修订记录）；`项目开发进展.md` 2026-09-01 条目（根因链与裁决）；`.specs/nn-predictive-ecms-phase25/tasks.md` T-311 |

## 10. 遗留与后续

| # | 事项 | 处置 / 归属 |
|---|---|---|
| 1 | 清扫后干净镜像 INFER_TEST ×200 复测 | 占位待回填 §8.3 / §9.1（主 Agent 板上会话执行；板上当前运行 `a1326b51`（含 FR-505 插桩），插桩清扫重建进行中） |
| 2 | NPU 域时钟 400 MHz（991 参照 800 MHz） | 频率提升收益估计 42 ms → ~17 ms；已裁决 **Phase 3 不做**（design FR-505 变更记录） |
| 3 | T-508 M2 注入比对（`m2_port_report.md`） | 待做：REF/NPU 注入三层比对（层②硬门禁 ≥99% tick ≤1 网格步）+ FR-508 故障注入，需用户在场 |
| 4 | T-312 收口：复现检查 + README 登记（含本报告）+ fw_gate + T-507 勾选 + git 提交流程 | 主 Agent §14 验收后执行（AGENTS.md §14 / §18 / §20；本报告为验收输入，非提交动作） |
| 5 | 污染数据处置：`deploy/m1_infer_test/infer_compare.csv` / `.json` 勿引用 | 改名留证 / 移除由主 Agent 决定（本报告不改动任何证据文件） |
| 6 | 数据表勘误：NetworkRuntime1000 → NetworkRuntime1020（§2.2） | 待主 Agent §14 确认 |

---

*本报告由 delegate 按 T-507 规格起草（2026-09-01）。全部数据引自正文标注的数据源文件与委派数据表，无编造；占位符仅限清扫后复测相关（§8.3、§9.1），勘误标记仅 §2.2 工具链项。*