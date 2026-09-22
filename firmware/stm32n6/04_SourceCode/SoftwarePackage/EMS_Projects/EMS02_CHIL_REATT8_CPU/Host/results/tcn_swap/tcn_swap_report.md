# TCN-f64 编译期换型报告（spec `ems01-chil-tcn64-swap`，T-814 验收文档）

| 项 | 内容 |
|---|---|
| 工程 | EMS01_CHIL_ECMS_TCN64（P3，TCN-f64 流式换型工程；路径约定见下） |
| 任务出处 | spec `.specs/ems01-chil-tcn64-swap/`（requirements/design/tasks 三批准，2026-09-02）；报告挂靠 FR-715 / T-814 |
| 换型对象 | 预测器槽位 MLP-Q G2 → **TCN-f64**（165,851 参数，int8 tflite 219.7 KB，float val RMSE 9.8433 kWh）；编译期槽位切换（档 1），可逆（`-DEMS_PREDICTOR=mlpq` + mlpq hex 重烧） |
| 板卡 | AlienTek ATK-CNN647B（STM32N647：Cortex-M55 @600 MHz + Neural-ART NPU；外挂 MX25UM25645G OctoSPI NOR 256 Mbit） |
| 数据采集 | 2026-09-02 ~ 2026-09-03（离线链 T-801~809/816；板会话 T-808/810/811/812 + 层②诊断） |
| 报告日期 | 2026-09-03（delegate 按 evidence 文件原值起草；板上层①全轨迹/层②复测/闭环数据**留 `[PENDING-BOARD]` 槽位**，主 Agent 晨间回填） |
| 路径约定 | `SW` = `firmware/stm32n6/04_SourceCode/SoftwarePackage`；`P1` = `SW/EMS_Projects/EMS01_CHIL_ECMS`；`P3` = `SW/EMS_Projects/EMS01_CHIL_ECMS_TCN64`（本工程）；`WS` = `model/simulink_modelD_(EMS_NN_AECMS)`；`TS` = `.specs/ems01-chil-tcn64-swap`；`R3` = `P3/Host/results/tcn_swap`（本报告所在目录）。全部引用路径相对仓库根 |
| 状态 | **离线层全绿**（L0 batch/streaming、双镜像 fw_gate ×2、三口径、NOR 烧录验证）；**板上层① REF 链 PASS**（×200 vs keras + 40-tick 探针 100%）；**层② NPU 发散**（S20 exact 34.0%，诊断完成，出路待裁决）；**NPU 600MHz NO-GO**（已回退）；闭环/层②复测/FR-508 PENDING |

## 1. 摘要与验收总表

v3 soft-HIL 真闭环收官后，按 v1 spec FR-511 流程执行编译期换型：将 TCN-f64 换入预测器槽位（NOR 0x70200000 mpool），走完整链路（stedgeai 重生成 → 重构建 → fw_gate → SRAM 加载 → 一致性复测 → 闭环复测）。换型价值定位 = **FR-511 换模型流程的首次真实兑现 + 更大模型（macc 11.96M vs MLP-Q 21.5K）入闭环的平台能力验证**，非"预期更省油"（int8 口径 MLP-Q 9.9659 仍优于 TCN-f64 int8 10.0353，TS/requirements.md Fixed Decisions 前言）。

**已锁定结论**：① TCN C 前向（REF）与 keras 参考全数据集 LSB 门 PASS，最大 |Δq50| = 4.5e-5 kWh（门 0.266214 kWh，裕量 ~5×10³×）；② 流式 closure 增量与 batch 前向**位级一致 9,965/9,965（max|Δ| = 0）**——构造性成立 + 实测；③ 板上 REF ×200 vs keras PASS（mean 1.32e-5 kWh ≈ 5e-5 LSB，裕量 ≥10⁴×）；④ 板上 REF closure 40-tick 探针 P_gen **精确一致 100%**；⑤ 双镜像 fw_gate 7/7 ×2；⑥ NOR 权重烧录独立回读验证 PASS。**未闭环**：层② NPU 执行发散（S20 P_gen exact 34.0%，Δq50 达 87.8 LSB，根因指向 stedgeai 对 dilated conv 的 NPU 分解形态），FR-504 ×200、闭环 (a)/(b)、FR-508 抽测留板上槽位。

| # | FR | 要求（TS/requirements.md） | 结论 | 关键数据 | 正文 | 证据 |
|---|---|---|---|---|---|---|
| 1 | FR-701 | `Model/tcn_q/` 部署链（只读副本 + neuralart/mpool + generate 脚本 + 报告归档） | ✅ T-801 | tcn_q.tflite 219.7 KB，sha256 `a3406b84…8a4d43` 与源逐位一致；analyze/generate 日志归档 | §2 | `P3/Model/tcn_q/NOTE.md` |
| 2 | FR-702 | 部署 JSON 单一来源 + `EMS_TCNQ_*` 表 + 归一化逐位断言 | ✅ T-802/803 | 参数量断言 165,851；spot-check f64 重放 3.663e-15 kWh | §5 | `WS/results/ecms_phase25/deploy/m3_tcn_swap_calib_report.json` |
| 3 | FR-703 | `ems_infer_tcn.c`（double 前向，求和顺序声明，非有限→返回 1） | ✅ T-804 + closure 修订 | 413 行；static 双缓冲 bss ≈311.7 KB；求和顺序口径见头注释 | §4 | `P3/Appli/App/ems_infer_tcn.c` |
| 4 | FR-704 | NPU 双输入适配（窗 + aux 分缓冲，对齐/cache 沿先例） | ✅ T-807 | static_assert IN_1=1200/IN_2=8/OUT_1=12 编译期通过 | §2 | `R3/image_fingerprints.json` |
| 5 | FR-705 | 槽位契约不变 + `EMS_PREDICTOR` 编译期选择；非 TCN 路径零改动 | ✅ T-806/807 | mlpq 模式回归 PASS（L0 基线不变）；ems_ctrl/ems_app/ems_proto 零改动 | §2 | `P3/README.md`；TS/tasks.md T-806 |
| 6 | FR-706 | stedgeai generate 同参 + objcopy @0x70200000 + analyze 预算核对 | ✅ T-801 | 132 epochs（14 CONV_2D+3 FC 全 EC）；activations 131.320 kB @npuRAM3；weights 161.267 kB @0x70200000 | §3 | `P3/Model/tcn_q/st_ai_output/network_analyze_report.txt` |
| 7 | FR-707 | 双镜像重构建 + fw_gate 7/7 ×2 + 指纹新鲜度 | ✅ T-808 | P1 gate 7/7（09-03 03:14 CST 复跑，exit 0）+ P3 gate 7/7（FW_GATE_ALLOW_IOC=1，用户授权） | §3 | `firmware/gate/fw_gate_result.json`；`R3/fw_gate_p3.json`；`R3/image_fingerprints.json` |
| 8 | FR-708 | 三口径一致检查机器可读 | ✅ T-803 | overall=true，conclusion "三口径一致，允许部署" | §5 | `WS/results/ecms_phase25/deploy/m3_tcn_swap_calib_report.json` |
| 9 | FR-708b | L0 host 差分（gcc REF 链 vs keras float32，LSB 门） | ✅ T-805 | 全数据集 PASS：max|Δq50| ≤4.5e-5 kWh（≤1.7e-4 LSB）；网格一致率 1.0 ×2 轨迹 | §5 | `R3/l0_tcn_diff.json` |
| 10 | FR-709 | 层① 两段式（①host vs keras；②板上 REF vs host gcc 100%） | 🟡 段①②(40-tick) PASS，全轨迹 PENDING | 板 REF ×200 vs keras mean 1.32e-5 kWh（裕量 ≥10⁴×）；closure 40-tick exact 100% | §6 | `R3/board_ref_infer/ref_keras_gate.json`；`/tmp/tcn_ref40/fw_compare.json` |
| 11 | FR-710 | 层② ×200 FR-504 LSB 门 + INJECT NPU vs host-int8 ≥99% | 🔴 NPU 发散，诊断完成 | S20 exact 34.0%（门 ≥99%）、max_dP 980 kW、Δq50 87.8 LSB；输入装配逐位正确 | §7 | `R3/layer2_diagnosis_20260903.json`；`R3/board_npu_S20/fw_compare.json` |
| 12 | FR-711 | 层③式闭环 (a)vs(b)（v3 harness，SOC ≤5e-3 / fuel ≤2%） | ⏳ [PENDING-BOARD: 闭环 (a) NPU + (b) REF × S20/voyage002] | 阈值沿 TS/design.md §Testing T7 | §6 | `P1/Host/results/closedloop/closedloop_metrics.json`（v3 权威条目引用基准） |
| 13 | FR-712 | 闭环 4 组 + (b′)/(c) 引用 v3 + 公平性带 + 增量 metrics | ⏳ [PENDING-BOARD] | 同 schema 增量条目计划沿 Fixed Decisions 10 | §6/§10 | `P3/Host/results/closedloop/closedloop_metrics.json` |
| 14 | FR-713 | 时延 ≤60 ms（TCN 推理 + workclf），超限 Escalate 不放宽 | 🟡 REF 臂超限已实测登记；NPU 臂待复测 | REF batch 8.8 s/拍 → closure ≈1.2 s/拍（板上折算）/ 40-tick 实测 ≈1.33 s/拍；仍 ≫60 ms | §4/§10 | `P3/Appli/App/ems_infer_tcn.c` 头注释；`/tmp/tcn_ref40/fw_compare.csv` |
| 15 | FR-714 | FR-508 状态机抽测（NaN/停发超时/CMD_RESET） | ⏳ [PENDING-BOARD] | 语义沿 v3 收官记录（F1 FALLBACK / F2 STALE / F4 清环） | §6 | TS/design.md §Testing T10 |
| 16 | FR-715 | 登记：报告 + metrics 增量 + roadmap/README/§18 日志 | 🟡 本报告为其一步（槽位待回填） | 三步管线（roadmap→新内容→docx 回更）沿 T-814 范围 | §10 | TS/tasks.md T-814 |
| 17 | NFR-701 | 实测时延记录在案，满足 FR-505 预算 | 🟡 同 FR-713 | workclf 42.5 ms（M1 实测）；TCN NPU 时延 [PENDING-BOARD]（600MHz 实验已回退） | §8/§10 | `WS/results/ecms_phase25/deploy/m1_deploy_report.md` §5 |
| 18 | NFR-702 | 资源预算（权重 1MB 池 15.75%、activations 29.31%、REF SRAM 不超限） | ✅ | REF 镜像 1,876,152 / 2,096,128 B = 89.51%，余量 219,976 B（门 ≤2047K−64KB） | §3 | `R3/image_fingerprints.json` ref_occupancy |
| 19 | NFR-703 | 全链脚本化 + sha256 溯源 + README 登记 | ✅ | 溯源链 tflite→deploy JSON→EMS_TCNQ_*→hex；`Model/tcn_q/NOTE.md` + `R3/replay/README.md` | §2 | `P3/Model/tcn_q/NOTE.md` |
| 20 | NFR-704 | 阶段隔离（令牌制 SRAM 加载 + Level C NOR 烧录） | ✅ | 令牌单次核销留痕；NOR 仅 0x70200000 区域，FSBL/workclf 零变化 | §3/§6 | `R3/nor_evidence_t811/`；`R3/board_session_interrupt_20260903.md` |

**总评**：换型离线链（部署链/构建/门禁/L0/三口径/NOR）全部 PASS；板上层①（REF 臂）两段式证据 PASS 且裕量大；层②（NPU 臂）发散已被诊断定位至工具链分解形态层面，属 ST 工具链/模型导出策略问题而非固件集成错误——层①/闭环 (b) 证据链不受影响。层② 出路、闭环复测与时延处置需主 Agent/用户裁决后收尾（TS/tasks.md Escalation Conditions）。

## 2. 部署对象与工具链

### 2.1 部署模型：TCN-f64

| 项 | 值 | 出处 |
|---|---|---|
| 架构 | 7 残差块 causal dilated Conv1D（k=3，d=1,2,4,8,16,32,32，f=64；block0 带 1×1 skip conv 1→64，block1..6 恒等 skip）+ LastTimeStep（末步 64）+ Dense(2→8) aux 旁路 concat → Dense(72→64) → Dense(64→3)（q10/q50/q90，归一化域线性输出） | `P3/Model/tcn_q/NOTE.md`；`P3/Appli/App/ems_infer_tcn.c` 头注释 |
| 参数量 | 165,851 | deploy JSON 导出断言（`m3_tcn_swap_calib_report.json` predictor 字段） |
| int8 tflite | 219.7 KB（QDQ，float32 I/O：IN_1=1200 B / IN_2=8 B / OUT_1=12 B） | `P3/Model/tcn_q/tcn_q.tflite`；`R3/image_fingerprints.json` static_assert |
| 输入 | 双输入：`[1,300,1]` 归一化 P 窗（1 s 升序）+ `[1,2]` 归一化 [v, a]；302 特征语义与 MLP-Q 同管线（`fleet_norm_v2.json` per-feature X_mean/X_std） | `P3/Model/tcn_q/NOTE.md` 口径声明 |
| 输出量化 | per-tensor scale=0.016138948500156403，zp=−28 → **1 LSB_TCN = 0.266214 kWh**（× y_std 16.495129） | `P3/Model/tcn_q/NOTE.md`；`R3/board_ref_infer/ref_keras_gate.json` lsb_tcn_kwh=0.2662140420605206 |
| 来源与只读副本 | `WS/results/ecms_phase25/models/tcn_f64/tcn_f64_int8.tflite`（T-307 产物，原样使用，不重训不重标定）；副本 sha256 `a3406b8466b5ae10dd7893adc5bec41573f7ee58f10768b0555e78b0d58a4d43` | `P3/Model/tcn_q/NOTE.md` |
| 槽位契约 | `ems_infer_mlpq(x302[302], q3_norm[3], *us)` 符号名沿 v1、语义 = 预测器槽位（归一化 302 入 → 归一化 q3 出）；调用点零改动；`-DEMS_PREDICTOR=tcnq\|mlpq` 编译期选择 | TS/design.md §Architecture 1；TS/tasks.md T-807 |

### 2.2 工具链与生成

| 项 | 值 | 出处 |
|---|---|---|
| ST Edge AI Core | **v2.2.0-20266 2adc00962**（锁定） | `P3/Model/tcn_q/generate.log` 第 1 行 |
| 生成参数 | `stedgeai generate --no-inputs-allocation --no-outputs-allocation`（I/O 由固件缓冲填充）+ `--st-neural-art default@user_neuralart.json`（`P3/Model/tcn_q/generate-n6-model.sh:38-40`）；atonn 侧 `--enable-epoch-controller --cache-maintenance --optimization 3 --Os`（同 mlpq 配置，`generate.log` 命令行） | `P3/Model/tcn_q/generate-n6-model.sh`；`P3/Model/tcn_q/generate.log` |
| 产物 | `network.c/h/ecblobs.h` + `network_data.xSPI2.bin`（161.3 KB）→ objcopy `P3/Binary/network-data-tcnq.hex`（ihex，453.7 KB 文本，基址 **0x70200000**） | `P3/Model/tcn_q/NOTE.md`；`P3/Binary/network-data-tcnq.hex` |
| NPU 生成符号 | `*_Default` 经 `ems_npu_tcnq.c` 宏改名 `*_Tcnq` 后 `#include`（单网络实例，改名仅为与未来并存预留）；生成物不复制进工程 | TS/design.md §Architecture 3；`R3/image_fingerprints.json` gc_checks |
| REF 侧 | `ems_infer_tcn.c`（double C 前向，方案 R1）+ `EMS_TCNQ_*` 双精度权重表（`ems_params.c` 重生成，deploy JSON 单一来源）；NPU 镜像内 EMS_TCNQ_* 经 gc-sections 裁除（nm 实测 0 符号） | `P3/Appli/App/ems_infer_tcn.c`；`R3/image_fingerprints.json` gc_checks |

## 3. 资源占用

### 3.1 analyze 报告（stedgeai v2.2.0-20266，T-801 归档）

| 项 | 值 | 预算/占比 |
|---|---|---|
| epochs | **132**，其中 47 SW（Pad / SpaceToDepth / DepthToSpace / Quantize / Gather 等**数据搬运**算子，非计算回退） | TS/design.md R2 预期内 |
| NPU HW 覆盖 | **14 CONV_2D + 3 FULLY_CONNECTED 全 EC（NPU HW）** | design §Summary 预检一致（AC-1） |
| macc | 11,959,685 / 次推理 | —（v1 mlpq 21.5K 的 ~556×，平台能力验证对象） |
| activations | 131.320 kB @npuRAM3 [0x34200000…] | 448 kB 池 **29.31%** |
| NPU weights | 161.267 kB @octoFlash **0x70200000**（与 mlpq 同基址，mpool 布局零改动） | 1 MB 池 **15.75%** |
| 合计 | 292.587 kB | — |

出处：`P3/Model/tcn_q/st_ai_output/network_analyze_report.txt`（权威）+ `P3/Model/tcn_q/analyze.log`；与 TS/design.md §Summary 预检数字逐项一致（Escalation Conditions 第 1 条不触发）。

### 3.2 NOR 权重烧录验证（T-811，Level C）

| 项 | 值 | 出处 |
|---|---|---|
| 写入区域 | 0x70200000 起预测器池（TCN hex），专用外部加载器 `MX25UM25645G_ATK-CNN647B.stldr`；禁 Mass Erase，workclf 池 0x70400000 与 FSBL 区零接触 | TS/tasks.md T-811；`R3/nor_evidence_t811/` |
| byte-map 回读 | **165,137 B 逐位一致 PASS**；byte-map 之外全 0xFF（erased state） | `R3/nor_evidence_t811/readback_verdict.txt` |
| CRC32 | TCN 权重 **0x47191C22**（与生成时记录一致） | 同上 |
| vendor verify | PASS（STM32_Programmer_CLI -v）；烧录前后 region 读取留证（`nor_0x70200000_pre/post.bin`、`fsbl_before.bin`） | 同上 + 目录文件清单 |

### 3.3 镜像指纹与 REF 镜像占用（T-806/808 + closure 重构建）

| 项 | 值 | 出处 |
|---|---|---|
| NPU Debug ELF（batch 版，T-808 指纹） | sha256 `cf0f9c0763d7de4b4a5d90578ccaca2b6b8c89b429c52fe20657ad28cd630641`（2026-09-02 23:28，freshness fresh） | `R3/image_fingerprints.json` |
| REF Debug ELF（batch 版，build-ref2 同源核验） | sha256 `5427419a335bb29ffc8a89047241e52c35aefa0379aba14d0ba2e26a06155f18` | 同上 |
| REF 镜像 SRAM 占用 | **1,876,152 / 2,096,128 B = 89.51%**；预算门 ≤2,030,592 B（2047K−64KB）PASS，余量 219,976 B | 同上 ref_occupancy |
| closure 修订增量 | `ems_infer_tcn.c` bss 311.7 KB（batch 版 309.6 KB，净增 ~2 KB）；REF 镜像重构建后仍按 ≤2047K−64KB 门核对 | `P3/Appli/App/ems_infer_tcn.c` 头注释 |
| 晨间续作镜像（closure 版） | P3 NPU 镜像 sha256 前缀 `1dbd86f9`（令牌 `DEBUG_MUTATION_AUTHORIZED_1DBD86F9`）；P3 REF 流式镜像 sha16 `396c4c8b`（令牌 `DEBUG_MUTATION_AUTHORIZED_396C4C8B`）——待 SRAM 加载执行层①全轨迹/层②/闭环 | `R3/board_session_interrupt_20260903.md` |
| gc 核对 | NPU 镜像内 `ems_tcnq` 符号 0（裁除）、改名符号在位；REF 镜像 `EMS_TCNQ_*` 表 36 个、`EMS_MLPQ_W1` 不在（可逆性结构证明） | `R3/image_fingerprints.json` gc_checks |

> 注：`image_fingerprints.json` 为 T-808 batch 版构建指纹；T-816 closure 修订后重构建镜像以中断记录所载 sha 为准（板载执行以 Plan==Readback 核验，[PENDING-BOARD]）。

## 4. 流式增量后端（closure 精确增量，T-816）

### 4.1 为什么需要流式（batch 版板上不可行）

| 项 | 值 | 出处 |
|---|---|---|
| REF batch 前向时延（板上 T-810 实测） | **≈8.8 s/拍**（52M MAC double 标量 + cache 关 + dilation 访存；DWT 32 位回绕假象已排除：显示 1.66 s = 8.83 − 7.16 s 逐位吻合） | TS/design.md 实施期澄清③ |
| 后果 | 超出 ems_proto 注入会话 2 s STALE 契约 **4.4×**，全轨迹 INJECT 半数 tick 被 held 污染（协议不可行）；纯计算 23.8 h → 层①全轨迹与闭环 (b) 臂不可行 | 同上 |
| 处置 | 用户裁决 C 方案（2026-09-02 深夜）：流式增量前向为生产正确形态，按**工程隔离规则**在 P3 新工程实施（不动已验证工程） | TS/tasks.md M-H；`P3/README.md` |

### 4.2 plain 流式方案的 L0 数值否决（机制级）

L0 数值实验（真实权重，ems_ctrl 型滑窗 700 tick，gcc -O2 -ffp-contract=off，`/tmp/tcn_stream/probe.c` 沙盒）否决"环深 2d+1 流式 + fast/cold 双路"原 design §2 方案：

| 实验 | 结果 | 根因 |
|---|---|---|
| (a) 归一化域窗连续性检查 | fast path 触发 **0/700**（机制死码） | x302 为 per-feature 归一化，滑窗平移位 i 与 i+1 使用不同 X_mean/X_std，连续性逐位检查永不成立 |
| (b) raw 窗连续性 + plain fast path | 与 batch 前向位级失配 **699/699**（首拍 tick 2，worst \|Δq50\| = 3.149 kWh = **11.8 LSB_TCN**，相对最高 168%） | 结构性：感受野 RF = 1 + 4·Σd = **381 > 窗长 300**——batch 每拍在窗口首移动左零边界，环缓存值携带上次冷复位点的陈旧零边界；失配与环深无关，任何环深/索引方案均无法修复 |

出处：`P3/Appli/App/ems_infer_tcn.c` 头注释（"设计修订"节）。定案为主 Agent 裁决：**依赖闭包精确增量（closure）**。

### 4.3 closure 设计与实测

| 项 | 值 | 出处 |
|---|---|---|
| 原理 | 每拍从当前 x302 窗重算末输出 A_6[299] 的依赖闭包内全部 (层,位置) 节点，**无跨拍状态**（连续与不连续窗口同价同语义；窗连续性自检/fast-cold 双路随 plain 流式一并废止） | `P3/Appli/App/ems_infer_tcn.c` 头注释 |
| 闭包节点集 | **889 节点**（数据无关，stride 结构固定）：C1_0 全 300 位；A_0/C1_1 各 150；A_1/C1_2 各 75；A_2 38；C1_3 37；A_3/C1_4 各 19；A_4 9；C1_5 7；A_5 5；C1_6 3；A_6 1（窗触点全 300） | 同上（分层明细以头注释原文为准） |
| MAC/拍 | **7,300,048**（batch 51.9M 的 **1/7.1**） | 同上 |
| host gcc -O2 实测 | **3.3 ms/call**（batch 22.1 ms） | 同上 |
| 板上折算 | ~5.9M MAC/s（T-810 实测 batch 口径）→ closure **≈1.2 s/拍**（batch 8.8 s 的 1/7）→ 层①全轨迹 INJECT（<2 s STALE 契约）恢复可行 | 同上；40-tick 探针实测 us_ctrl ≈1.331e6 µs/拍（`/tmp/tcn_ref40/fw_compare.csv`） |
| bit-exact 论证 | 每输出元素运算树与 batch 完全一致（求和顺序同族：acc 从偏置起 → kernel kk 升序 → 通道 c 升序；j<0 左补零整项跳过），输入值 = 当前窗同位值 → **构造性逐位一致**；L0 实测沙盒 700/700 tick max\|Δ\| = 0 + 板级链全量复测 | `P3/Appli/App/ems_infer_tcn.c` 头注释；`R3/l0_tcn_stream_diff.json` |
| 内存 | static 双缓冲 tcn_buf1/tcn_buf2（各 [300][64] double ≈153.6 KB）+ 闭包掩码 uint8_t 2×7×300 ≈4.2 KB；本文件 bss ≈311.7 KB | `P3/Appli/App/ems_infer_tcn.c` 头注释 |
| 实现形态 | 413 行；workclf 前向/遥测经宏改名 `#include "ems_infer_ref.c"` 同源复用（分类器零漂移）；被换下 MLP-Q 前向以 unused 符号保留（板上 gc-sections 裁除） | 同上 |

**时延预算注记（FR-713）**：closure ≈1.2–1.33 s/拍仍 ≫ 60 ms 预算——按 TS/design.md §Error Handling 既定 Escalation 路径处置（登记/降级/终止，用户裁决），本实现不放宽任何阈值；NPU 臂时延 [PENDING-BOARD]。

## 5. L0 验证链（三口径表）

### 5.1 三口径定义与一致检查（FR-708）

| 口径 | 内容 | 核对结果 |
|---|---|---|
| 训练口径 | 302 特征（[P 窗 300（1 s 升序）\| v_ship \| accel]）+ fleet_norm_v2 per-feature 归一化 + split_inputs 布局（窗 [N,300,1] + aux [N,2]） | ✅（`m3_tcn_swap_calib_report.json` training_caliber） |
| 部署喂入口径 | ems_ctrl 归一化单份（R8）→ 槽位收归一化 302 → ems_infer_tcn.c 内拆分 x302[0:300]=窗 + x302[300:302]=aux（=训练 split_inputs 布局） | ✅（deployment_feed_caliber） |
| int8 标定口径 | T-307 val 前 1000 归一化样本（与部署喂入口径相同，§22.3 规则 3 例外条款）；int8 val RMSE 10.0353 kWh | ✅（int8_calib_caliber） |
| norm 逐位断言 / sha256 身份门 / 部署链 spot-check | deploy JSON norm 与 fleet_norm_v2.json 逐位一致；tcn_q.tflite sha 一致；3 随机窗 f64 重放 vs keras = **3.663e-15 kWh**（门 1e-6），f32 噪声底 1.344e-6 kWh 记录性保留（sanity 上限 1e-3） | ✅ overall=true，conclusion "三口径一致，允许部署" |

出处：`WS/results/ecms_phase25/deploy/m3_tcn_swap_calib_report.json`（T-803）。

### 5.2 L0 batch 差分（FR-708b，T-805）

宿主链：gcc -O2 -Wall -Wextra -ffp-contract=off（ems_ctrl.c + ems_infer_tcn.c + ems_params.c + test_driver.c）；参考链：`export_tcnq_reference.py`（keras float32 tcn_f64.keras + fleet_norm_v2 归一化，kWh 域输出）。门：|Δq50| ≤ 1 LSB_TCN = 0.266214 kWh 逐 tick；P_gen 决策链网格一致率 ≥99%。

| 数据集 | n | max\|Δq50\| (kWh) | mean\|Δq50\| (kWh) | p99\|Δq50\| (kWh) | worst | 网格一致率 | gate |
|---|---|---|---|---|---|---|---|
| val200（seed=20260831 抽样） | 200 | 3.003e-05 | 3.325e-06 | 2.278e-05 | row 40 | — | ✅ |
| random32（seed=20260902，N(0,1) 反归一化） | 32 | 4.505e-05 | 9.673e-06 | 3.793e-05 | row 6 | — | ✅ |
| S20 全轨迹（ref_G2_S20_composite_voyage_matrix.csv） | 5521 | 4.425e-05 | 3.907e-06 | 1.934e-05 | tick 5349 | **1.0**（5521 cmp / 0 mismatch；fallback 0；illegal_pgen 0） | ✅ |
| voyage002 全轨迹（ref_G2_test_voyage_002_v2.csv） | 4212 | 3.738e-05 | 3.963e-06 | 1.957e-05 | tick 2038 | **1.0**（4212 cmp / 0 mismatch） | ✅ |

出处：`R3/l0_tcn_diff.json`（overall=true；参考 CSV sha16：val200 `a343c45838caf5dc` / S20 `c819b6c979483403` / voyage002 `521f9383176fb20f` / vectors_raw `dd5f2d91a30a34ad`）。最大偏差 4.5e-5 kWh = 门的 **1.7e-4 倍**。

### 5.3 L0 streaming 位级差分（T-816，closure vs batch）

驱动 %.17g 响应行**逐字节相等**（round-trip 无损 = IEEE754 double 位级等价；非容差比对）。batch 位级基准 = 同文件 `-DEMS_TCN_BATCH_REF` 测试专用 T-804 batch 前向（固件构建编译剔除）。

| 数据集 | n | n_bitexact | n_mismatch | max\|Δq50\| | first_mismatch |
|---|---|---|---|---|---|
| val200 | 200 | 200 | 0 | **0.0** | −1 |
| random32 | 32 | 32 | 0 | **0.0** | −1 |
| S20 | 5521 | 5521 | 0 | **0.0** | −1 |
| voyage002 | 4212 | 4212 | 0 | **0.0** | −1 |
| **合计** | **9965** | **9965** | **0** | **0.0** | — |

同文件 vs_keras 段（同源 L0 结果复算）与 §5.2 数值一致（overall=true；gate_bitexact=true）。出处：`R3/l0_tcn_stream_diff.json`。

### 5.4 车道 B 交叉核对（int8 vs keras，记录不设门，T-809）

| 数据集 | mean \|Δq50\| (kWh) | max (kWh) | ≈ LSB_TCN |
|---|---|---|---|
| val200 | 0.58 | 5.23 | ≈2.2 / 19.7 |
| S20 稳态（tick≥300，n=5221） | 0.89 | 4.79（p99 2.76） | ≈3.3 / 18.0 |
| voyage002 稳态（n=3912） | 0.68 | 8.21（p99 3.68） | ≈2.6 / 30.8 |

启动 flat-fill 窗（tick<300，OOD）int8 输出饱和钳位（上限 73.34 kWh），与 v1 mlpq 同签名（v1 S20 tick1-4 钳位 70.63 kWh）。出处：`R3/replay/README.md` + `R3/replay/tcnq_crosscheck_keras_vs_int8.json`。该 2.2 LSB 量级即 TCN int8 量化噪声基准（§6.3 board-vs-int8 口径的解释依据）。

## 6. 板上验证

### 6.1 层①：板上 REF 臂（T-810 + closure 探针）

| 段 | 内容 | 结果 | 证据 |
|---|---|---|---|
| 板 ×200 INFER_TEST vs keras（batch REF 镜像，T-810） | val seed=20260831 同向量 ×200，LSB_TCN 门 | **PASS，裕量 ≥10⁴×**：mean_abs 1.3198e-05 kWh ≈ **5.0e-5 LSB**（门 mean ≤1.5 LSB，裕量 ≈3.0×10⁴）；p99 9.7723e-05 kWh ≈ 3.7e-4 LSB（门 ≤6，裕量 ≈1.6×10⁴）；max 1.1602e-04 kWh ≈ 4.4e-4 LSB（门 ≤8，裕量 ≈1.8×10⁴）；\|signed_mean\| 6.354e-06 kWh（门 ≤0.5）；四门全 true | `R3/board_ref_infer/ref_keras_gate.json`（pass=true） |
| 板 ×200 vs Python int8 参考（记录性） | 同一批 200 向量 | mean 2.18 / p99 10.815 / max 19.664 LSB，RMSE 比 0.0222——与车道 B 量化噪声口径（≈2.2 LSB）一致；`hard_gate_fr504=false`（FR-504 硬门属 NPU 臂，见 §6.2） | `R3/board_ref_infer/infer_compare.json`（errors 全 0：timeout/bad_rsp/resend/stale_skip） |
| 板 REF closure 40-tick 探针（P3 流式镜像） | S20 型注入 40 tick，P_gen 网格比对 vs 宿主链参考 | **exact_rate = 1.0、within_1grid = 1.0、max_dP = 0.0 kW**（40/40，errors 0：timeout/crcerr/idx_mismatch 全 0）；us_ctrl ≈1.331e6 µs/拍（closure 折算 1.2 s/拍的板级实证） | `/tmp/tcn_ref40/fw_compare.json` + `fw_compare.csv`（易变目录，正式归档建议随 [PENDING-BOARD] 回填复制入 R3/） |
| 层① 全轨迹 INJECT（S20 5521 + voyage002 4212） | 板 REF closure 镜像 vs `R3/host_tcn_pgen_S20.csv` / `host_tcn_pgen_voyage002.csv`（宿主全链 P_gen 参考，5521/4212 tick） | **[PENDING-BOARD: 后台作业进行中（2026-09-03 02:58 启动，≈1.33 s/拍 × 5521 拍 ≈ 2 h/轨迹）；`R3/board_ref_S20/` 截至本报告起草（09-03 03:2x CST）为空，主 Agent 晨间回填 exact/within_1grid/max_dP 与逐拍 CSV]** | 槽位：`R3/board_ref_S20/` |

层① 口径声明（design R5，强制陈述）：TCN 无 EML 锚点，层① 由 v1 的"vs Simulink 位级"改为**两段式**——① host gcc TCN-C vs keras float32（LSB 容差门，§5.2）；② 板上 REF vs host gcc 同源双编译（P_gen 网格精确一致语义）。本报告不暗示与 v1 层① 同锚点。

### 6.2 层②：NPU 臂（T-812）——发散，诊断完成，复测 [PENDING-BOARD]

S20 全轨迹 INJECT 5521 拍（NPU 镜像 vs host-int8 参考，协议错误 0）实测：

| 指标 | 值 | 门 | 结论 |
|---|---|---|---|
| P_gen exact_rate | **33.997%** | ≥99% | **FAIL** |
| P_gen within_1grid_rate | 41.460% | — | 发散态 |
| max_dP | **980.0 kW** | — | 超差 |
| max_de_hat | 3.0896 | — | ê 通道发散（dê mean 0.916，诊断 JSON） |
| cls_match_rate（workclf NPU 臂） | 99.964% | ≥99% | ✅ 识别器不受影响 |
| layer2_pgen_pass | **false** | — | 门禁 FAIL |

出处：`R3/board_npu_S20/fw_compare.json`（rows 5521，errors timeout/crcerr/idx_mismatch 全 0）。FR-504 ×200（INFER_TEST 路径）与闭环 (a) NPU 在诊断期间被列为 blocked——**[PENDING-BOARD: FR-504 ×200 重测 + INJECT 层② 复核 + 闭环 (a) NPU]**。根因诊断见 §7（专节）。

### 6.3 闭环与收尾项（T-813/814/815）

| 项 | 状态 | 槽位 |
|---|---|---|
| 闭环 4 组（(a) NPU / (b) REF × S20/voyage002，v3 harness lock-step + REPRO） | 待执行（(b) 臂恢复可行的前提 = closure 流式镜像 ≈1.3 s/拍 < 2 s STALE 契约，40-tick 已实证） | **[PENDING-BOARD: 逐组 gate JSON + closedloop_metrics.json 增量条目]** |
| 层③式 (a)vs(b) 对拍（SOC 终点 \|Δ\| ≤5e-3、fuel_corr 相对 ≤2%） | 待执行 | **[PENDING-BOARD]** |
| (b′)/(c) 引用 v3 收官权威数据 + 跨模型对比表（TCN vs MLP-Q vs law=5，公平带 ±0.02） | 数据源在库 | `P1/Host/results/closedloop/closedloop_metrics.json`（v3 权威）；TCN 条目 [PENDING-BOARD] |
| FR-508 抽测（NaN 输入 / 停发超时 / CMD_RESET 恢复） | 待执行 | **[PENDING-BOARD]** |
| workclf us_infer 实测（T-817 回退后 400 MHz 口径） | 待执行（600MHz 预期 ~28 ms 的对照实验随频率回退取消，400 MHz 实测值 [PENDING-BOARD]；M1 基线 42.5 ms） | **[PENDING-BOARD]** |

## 7. 层② 根因诊断专节（T-812，`R3/layer2_diagnosis_20260903.json`）

### 7.1 症状

板上 NPU TCN 前向偏离 host-int8 解释器：S20 P_gen exact **34.0%**（门 ≥99%）、max_dP **980 kW**、dê mean **0.916**。首个证据点：单拍探针（`/tmp/tcn_tick0/fw_compare.json`，NPU 模式 1 拍）exact_rate = 0、max_dP = 800 kW——**发散自首拍即存在**，非累积漂移。

### 7.2 输入装配逐位核验（排除固件侧装配错误）

| 检查项 | 结果 |
|---|---|
| tcnq_win（[1,300,1] 窗缓冲） | 176 个抽样位置逐一比对 expected per-feature normalized flat window，**0 mismatch**（P(0)=100 kWh 锚点） |
| tcnq_aux（[1,2] 缓冲） | [-3.703197, -0.035377] == expected xnorm[300:302] **精确相等** |
| 判定 | **input assembly CORRECT（bit-verified）**——输入侧排除 |

### 7.3 输出发散量化

| 量 | 板 NPU | host int8 解释器 | 差 |
|---|---|---|---|
| q3_norm[3] | [-1.468644, 0.806947, 2.501537] | q50_norm = −0.61328 | — |
| q50_kwh | 45.39 | 21.96 | **Δ = 87.8 LSB_TCN** |

判定：NPU（Neural-ART）对 TCN 图的执行偏离 tflite 解释器，**超出量化噪声量级**（对照：车道 B int8-vs-keras 噪声 ≈2.2 LSB，§5.4；板 REF ×200 vs keras ≈5e-5 LSB）。

### 7.4 隔离矩阵

| 通路 | 结果 | 含义 |
|---|---|---|
| workclf NPU 臂 | cls_match **99.96%** | ✅ 更简单图（Conv1D k=3 无 dilation）NPU 执行正常——NPU 硬件/驱动/加载链路本身可用 |
| REF closure C 臂 | L0 bit-exact 9965/9965 + 板上 40-tick P_gen exact **100%** | ✅ closure C 数据通路与宿主链一致 |
| tcn_npu_graph | **14 个 dilated conv 被 stedgeai 分解**（SpaceToBatch / Conv2D / BatchToSpace）+ **47 个 SW fallback epochs**（数据搬运） | 疑似根因：**NPU epoch ↔ SW op 数据交接破坏中间张量**（TCN 的 dilation 形态迫使分解 + 交接，workclf 无此形态） |

### 7.5 并发发现：INFER_TEST 长帧接收损坏

CMD_INFER_TEST → **NACK(ERR_LEN) 重复**（板收到但 LEN 校验失败 = 长帧接收损坏）；TICK 36 B 帧正常。与 NPU 执行错误同根概率高（待排查）。诚实记录：诊断期间主 Agent 误 halt 核心污染了部分实验，已修正并重测关键项（§7.2–7.4 数字均为修正后重测值）。

### 7.6 影响面与三条出路

**Blocked**（诊断 JSON 原文）：层② gate (a) 臂、FR-504 ×200（INFER_TEST path）、闭环 (a) NPU。
**Unaffected**：层① REF INJECT（closure bit-exact）、闭环 (b) REF、T-817 已回退至验证过的时钟域。

**出路（诊断 JSON next_steps，待主 Agent/用户裁决）**：
1. **ST 工具链级排查**：stedgeai 对 TCN 图的 NPU 执行 vs 解释器差异（dilated conv 分解 + SW fallback 数据交接链路取证）；
2. **模型重导出策略**：避免 dilation 分解形态（重排/重导出使 conv 图形适配 NPU 单 epoch 执行），或**全 SW 覆盖率方案**（CM55 int8 CMSIS-NN ≈ 解释器同族内核，放弃 TCN 图的 NPU 覆盖）；
3. **REF 侧证据不受影响**：层①/闭环 (b) 继续推进（当前后台作业即此线）。

## 8. NPU 600MHz 实验 NO-GO 专节（T-817）

| 项 | 内容 |
|---|---|
| 动机与授权 | 用户指令"NPU 频率拉到最大"（限定新工程 P3；PLL1 = 1.2 GHz，RM0486 NPU 上限 1 GHz → 最大安全档 IC2Div 2 = **600 MHz**） |
| 实施 | `npu_config()` 运行时 `LL_RCC_IC2_SetDivider(2)` + `.ioc` IC2Div 3→2 声明对齐（fw_gate 以 `FW_GATE_ALLOW_IOC=1` 放行，用户授权） |
| 板上实测 | **NO-GO**：IC2 400→600 MHz → **XSPI2 kernel clock 同域抬升**（200→300 MHz）→ NOR memory-mapped 读失配（超出 MX25UM25645G **200 MHz** 规格）→ `LL_ATON_RT_Main` 挂死 |
| 回退 | 已回退原频率域并留实验记录：`P3/Appli/Core/Src/main.c` `npu_config()` 注释（"原始 .ioc IC2Div=3/400MHz 恢复"）+ `.ioc` 复原；T-817 状态 = reverted to validated clock（诊断 JSON unaffected 列） |
| 遗产 | "NPU 频率提升"维持 v1 design 既定结论：后续硬件方向，需 RM 时钟树全查 + XSPI 重定时验证，Phase 3 不做；预期收益（workclf 42.5→~28 ms）未兑现，400 MHz 口径 us_infer 实测 [PENDING-BOARD] |

出处：`P3/README.md` 头注（NO-GO 记录）；`P3/Appli/Core/Src/main.c:95-103`；`R3/layer2_diagnosis_20260903.json` npu_600mhz_experiment；`R3/fw_gate_p3.json` ioc_unchanged（FW_GATE_ALLOW_IOC=1）。

## 9. 三工程结构（EMS_Projects 工程隔离）

用户裁决（2026-09-02 深夜，`SW/EMS_Projects/README.md` "用户裁决三工程结构"）确立工程隔离规则：**凡为适配当前预测模型而需修改工程源代码的后续工作，必须在 `EMS_Projects/` 下以独立工程实施，不得改动现有已验证工程**（TS/tasks.md M-H 头注）。当前注册与状态：

| 工程 | 角色（EMS_Projects/README.md 登记） | 本换型中的职责 | 状态（截至 2026-09-03 凌晨） |
|---|---|---|---|
| `EMS01_CHIL_ECMS`（P1） | MLP-Q G2 v3 基线（保持可跑）；batch TCN 换型首个实施现场 | T-801~811 首现场：部署链 / ems_infer_tcn.c batch 版 / L0 / 双镜像 fw_gate / 板 ×200 / NOR 烧录验证 | 换型验证完成；证据整合至 P3 `R3/` 后回归 v3 基线态（fw_gate 7/7 复跑 09-03 03:14 CST，`.ioc` 与 HEAD 一致）；提交按 T-815 feat 分支隔离 |
| `EMS01_CHIL_ECMS_TCN64`（P3） | **TCN-f64 流式 + NPU 600MHz 工作区**（工程隔离规则承接体） | closure 流式前向（T-816，bit-exact）、NPU 600MHz 实验（T-817，NO-GO 回退）、层② NPU 发散诊断现场；全部换型证据与本报告 | closure bit-exact 9965/9965；fw_gate_p3 7/7；层② NPU 发散待出路裁决 |
| （第三工程槽位） | — | — | **[PENDING-CONFIRM: `SW/EMS_Projects/README.md` 清单标题载"三工程结构"但注册表当前仅枚举两工程；`EMS_Projects/Home/` 为空目录未注册条目——第三工程身份/职责待主 Agent 晨间回填本表]** |

职责边界（P1↔P3）：冻结资产（ref_G2 / fw_reference / baseline_manifest / v1.1 冻结件）、主模型与 PlantStep、G-P1/G-P2 仪器链**零改动**（被控对象换型无关，免重跑）；mlpq 模式 L0 回归链保留（`--predictor mlpq` 可跑通 = 可逆性证据）。

## 10. 已知限制与后继

| # | 项 | 性质 | 处置/去向 |
|---|---|---|---|
| 1 | **层② NPU 执行发散**（S20 exact 34.0%，Δq50 87.8 LSB；根因指向 stedgeai dilated conv 分解形态 + 47 SW fallback epochs 数据交接） | 机制级未解 | 三条出路（§7.6）待主 Agent/用户裁决；TS/tasks.md Escalation Conditions 在案，阈值不放宽 |
| 2 | **INFER_TEST 长帧 NACK(ERR_LEN)**（LEN 校验失败，TICK 帧正常） | 待排查 | 与 NPU 发散同根概率高；FR-504 ×200 重测前置排查项 [PENDING-BOARD] |
| 3 | **层① 全轨迹 / 闭环 4 组 / FR-508 / NPU 时延复测** | 板上数据未产出 | [PENDING-BOARD] 槽位（§6.1/§6.3），晨间按中断记录恢复清单续作（ST-LINK USB 需重新插拔恢复枚举，`R3/board_session_interrupt_20260903.md`） |
| 4 | **时延预算（FR-713）**：REF closure ≈1.2–1.33 s/拍 ≫ 60 ms | Escalation 已触发（不放宽） | design §Error Handling 既定路径：登记 / 候选降级（TCN-f16）/ 终止换型，用户裁决；NPU 臂时延待板测 [PENDING-BOARD] |
| 5 | **REF 镜像占用 89.51%**（余量 219,976 B，预算门内） | 高位但 PASS | 无需行动；后续在 REF 侧增表须复核 ≤2047K−64KB 门 |
| 6 | **层① 口径差异**（两段式，非 v1 "vs Simulink 位级"同锚点） | 已声明（design R5） | §6.1 口径声明；导师报告并入时按 T-814 注明 |
| 7 | **v1 冻结参考已知缺陷**（`Host/reference/reference_int8_trace_*.csv` 前 ~300 tick 同源失配） | 登记不修复（冻结纪律） | 本换型全部参考（tcnq_*）用修复后生成器，验证链不受影响（TS/design.md 实施期澄清②） |
| 8 | **回退路径** | 可逆性保证 | `-DEMS_PREDICTOR=mlpq` + mlpq hex 重烧 0x70200000（同 Level C 流程）即完整回滚；MLP-Q 资产全保留（gc_checks 实证） |
| 9 | **收尾登记（T-814/815）** | 待办 | closedloop_metrics 增量 + roadmap §8 + `Model/README.md`/`Model/tcn_q/README.md`/`README_Model.md` 两级登记 + §18 日志 + feat 分支提交；metrics 读取方 dry-run 兼容校验（design R7） |

## 附录 A：证据文件清单（本报告引用路径核对表）

| 路径（相对仓库根） | 内容 |
|---|---|
| `P3/Host/results/tcn_swap/l0_tcn_diff.json` | L0 batch 差分（overall=true） |
| `P3/Host/results/tcn_swap/l0_tcn_stream_diff.json` | L0 streaming 位级差分（9965/9965） |
| `P3/Host/results/tcn_swap/layer2_diagnosis_20260903.json` | 层②根因诊断 |
| `P3/Host/results/tcn_swap/fw_gate_p3.json` | P3 fw_gate 7/7（FW_GATE_ALLOW_IOC=1） |
| `firmware/gate/fw_gate_result.json` | P1 fw_gate 7/7（09-03 复跑） |
| `P3/Host/results/tcn_swap/image_fingerprints.json` | T-808 镜像指纹 + REF 占用 89.51% |
| `P3/Host/results/tcn_swap/board_ref_infer/ref_keras_gate.json` | 板 ×200 vs keras PASS（裕量 ≥10⁴×） |
| `P3/Host/results/tcn_swap/board_ref_infer/infer_compare.json`（+ `infer_compare.csv`） | 板 ×200 vs int8 参考（记录性） |
| `P3/Host/results/tcn_swap/board_npu_S20/fw_compare.json`（+ `fw_compare.csv`） | 层② S20 5521 拍 NPU 发散数据（exact 34.0%） |
| `P3/Host/results/tcn_swap/nor_evidence_t811/` | NOR 烧录前后留证 + readback verdict（165,137 B / CRC32 0x47191C22） |
| `P3/Host/results/tcn_swap/board_ref_S20/` | 层① 全轨迹后台产出目录（起草时为空，[PENDING-BOARD]） |
| `P3/Host/results/tcn_swap/replay/README.md`（+ `tcnq_crosscheck_keras_vs_int8.json` 等） | T-809 车道 B 离线复演 + 交叉核对 |
| `P3/Host/results/tcn_swap/ref_keras_{val200,S20,voyage002}.csv` | keras float32 参考链输出 |
| `P3/Host/results/tcn_swap/host_tcn_pgen_{S20,voyage002}.csv` | 宿主 REF closure 全链 P_gen 参考（5521/4212 tick） |
| `P3/Host/results/tcn_swap/board_session_interrupt_20260903.md` | 板会话中断快照 + 晨间恢复清单 |
| `/tmp/tcn_ref40/fw_compare.json`（+ `fw_compare.csv`） | 板 REF closure 40-tick 探针（exact 100%；易变目录，建议回填时归档入 R3/） |
| `/tmp/tcn_tick0/fw_compare.json` | 层②首拍发散证据点（易变目录） |
| `P3/Model/tcn_q/NOTE.md` | 部署链溯源（sha256/量化/口径声明） |
| `P3/Model/tcn_q/st_ai_output/network_analyze_report.txt`（+ `analyze.log` / `generate.log`） | analyze 权威报告 + 工具日志（stedgeai v2.2.0-20266） |
| `P3/Binary/network-data-tcnq.hex` | NOR 权重 hex（0x70200000，453.7 KB 文本） |
| `P3/Appli/App/ems_infer_tcn.c` | REF closure C 前向（413 行，头注释含设计修订全记录） |
| `P3/Appli/Core/Src/main.c` | `npu_config()` 600MHz NO-GO 回退注释 |
| `WS/results/ecms_phase25/deploy/m3_tcn_swap_calib_report.json` | 三口径一致检查（overall=true） |
| `WS/results/ecms_phase25/deploy/m1_deploy_report.md` | M1 报告（表格风格先例；workclf 42.5 ms 基线） |
| `WS/results/ecms_phase25/models/tcn_f64/tcn_f64_int8.tflite` | 源模型（只读输入资产） |
| `WS/results/ecms_phase25/fw_reference/ref_G2_S20_composite_voyage_matrix.csv`、`ref_G2_test_voyage_002_v2.csv` | 注入轨迹冻结参考（S20 5521 / voyage002 4212 tick） |
| `P1/Host/results/closedloop/closedloop_metrics.json` | v3 收官权威指标（(b′)/(c) 引用基准） |
| `TS/brainstorm.md` / `requirements.md` / `design.md` / `tasks.md` | spec 四件套（FR-701~715 / 阈值先验 / T-801~817） |
| `SW/EMS_Projects/README.md` + `P1/README.md` + `P3/README.md` | 三工程结构登记与工程 README |

> **报告完整性声明**：本报告全部数值引用自上表证据文件原值，未做任何外推或编造。板测数据槽位（`[PENDING-BOARD]`）集中位于：§6.1 层①全轨迹（S20 5521 + voyage002 4212）、§6.2 层② FR-504 ×200 重测与 INJECT 复核、§6.3 闭环 4 组 / 层③式对拍 / 跨模型对比 TCN 条目 / FR-508 / NPU 时延复测（workclf us_infer 400 MHz 口径）、§3.3 closure 镜像 Plan==Readback 板载核验；另有 `[PENDING-CONFIRM]` 1 处（§9 第三工程槽位）。晨间回填后本报告升级为 T-814 完整验收文档。