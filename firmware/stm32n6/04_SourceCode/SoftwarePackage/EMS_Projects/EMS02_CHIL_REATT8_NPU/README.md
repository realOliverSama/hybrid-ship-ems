# EMS01_CHIL_REATT8 — REAT-T int8 板上预测服务工程（phase4-g，CHIL 平台）

> **立项**：2026-09-22（用户指令：参考 Phase 3 部署模式，把 Phase 4-F 的
> REAT-T winner 量化部署至 STM32N647 并做 soft-HIL；以 `../EMS01_CHIL_ECMS_TCN64/`
> 副本为基础开发）。spec：`.specs/phase4-g-reatt-board-deployment/`（仓库根）。
>
> **角色**：Phase 4-G 交付工程——REAT-T（Predictor-v3 冻结 winner reatt-t018，
> 1,012,835 参数）**int8 全 CPU 编译形态**（STEDGEAI 4.0.1，CM55 Lite 内核）板上
> 预测服务 + Phase 4-F C2 臂的 soft-HIL 板后端（预测器在环；控制/plant 仍在
> PC Simulink——形态差异见 spec §边界说明）。
>
> **母本关系**：`../EMS01_CHIL_ECMS_TCN64/` 副本（Model-D 谱系 ECMS/TCN 遗产
> 保留，目录内其余条目说明见母本 README）；本工程增量 = REATT 服务链。
> **与母本的形态差异（两项，fw_gate 7/7 通过）**：
> ① Debug 镜像 = TCN64 基线形态 + CMD_REATT 明确桩（REAT-T 权重 1.43MB +
> 激活 333KB + 图代码 ≈2.09MB，超 Debug SRAM 2047K 窗口，实测含 TCN+应用
> 溢出 1.15MB——物理不可行，非配置问题）；
> ② Release 镜像（正式交付形态，NOR appli）= REATT 全量 + workclf，**排除 TCN
> 网络**（4.0.1 CPU 形态生成代码每节点 forward_lite_* 为全局符号，两 CPU 形态
> 网络不能同链——实测 multiple definition；TCN 槽位在 Release 为明确回退桩，
> legacy ECMS soft-HIL 正式载体仍为 TCN64 工程，不受本工程影响）。
>
> **部署链**（Host/tools/quantize_reatt.py 可再生，tflite sha256 `efde1aff…`）：
> `reatt_s0.keras`（FR836 冻结）→ 输出裁剪到 raw_proj (m,a,b)（TFLite SOFTPLUS
> 溢出对策，分位变换在固件）→ TFLite int8（标定 = 归一化 train 窗 1024 个，
> 部署口径，AI_MODELING §22.3）→ stedgeai 4.0.1 全 CPU 形态 `Model/reatt8_q/`。
> host int8 vs keras：q50 MAE 23.5 kW / corr 0.981 / 有限率 100%（L0 门 PASS）。

## 目标

把 Phase 4-F C2 臂的预测器（REAT-T q50）从 PC keras 迁到板端 int8，经
soft-HIL（板上预测器 + PC Model E law6/PMS/plant，逐锚点文件桥）验证
量化部署对预测精度与控制结果的端到端影响。

## 结构（在 TCN64 副本基础上的增量；其余 = 母本遗产原样）

```text
EMS01_CHIL_REATT8/
├── Model/reatt8_q/           # ★ REATT-T int8 部署包（reatt_raw_int8.tflite 只读
│                              #   + stedgeai 生成 reatt8.{c,h}/reatt8_data.{c,h}
│                              #   + generate-reatt8.sh + generate_401.log + NOTE.md
│                              #   + reatt_raw_fp32.tflite 参考）
├── Model/reatt8_q_npu/       # ★ NPU 线产物（NOTE.md = 假设台账）
│   ├── reattnpu.{c,h} + stai_reattnpu.{c,h}
│   │                          #   整图 NPU 形态生成物（EC blobs + NOR 权重）
│   ├── part_n1/              # ★ 拆分 N1（TCN）CPU 形态：reattn1c.{c,h} +
│   │                          #   reattn1c_data.{c,h}（f32 I/O，权重 584,392 B 入 ELF）
│   └── part_n2/              # ★ 拆分 N2（Attention/Dense）NPU 形态：reattn2n.{c,h} +
│                              #   stai_reattn2n.{c,h} + reattn2n_ecblobs.h +
│                              #   reattn2n_atonbuf.xSPI2.raw（877,009 B，NOR 权重源）
├── Binary/network-data-reattn2n.hex  # ★ N2 权重 hex（@0x70300000）
├── Appli/App/
│   ├── ems_reatt.c/.h        # ★ REATT 板上服务（形态分派：SPLIT 空 TU / NPU / CPU；
│   │                          #   Debug/REF/mlpq 配置为明确桩）
│   ├── ems_reatt_split.c     # ★ 拆分形态实现（N1 CPU + N2 NPU，h_past 零拷贝衔接）
│   ├── ems_npu_reattn1c.c    # ★ N1 CPU 形态包装 TU（契约断言 + #include 图）
│   ├── ems_npu_reattn2n.c    # ★ N2 NPU 形态包装 TU（混合精度契约断言）
│   ├── ems_reatt_params.c/.h # ★【生成物，禁手改】冻结归一化常数
│   │                          #   （Host/tools/gen_reatt_params.py，sha256 溯源）
│   ├── ems_npu_reatt8.c      # ★ 整图 CPU 形态包装 TU（仅 Release 编译）
│   ├── ems_stai_rt.c/.h      # ★ stai runtime 全局一次初始化共享助手
│   ├── ems_proto.c/.h        # ＋CMD_REATT(0x0A)/RSP_REATT(0x0B) + MAX_PAYLOAD
│   │                          #   提升至 13924B + enc_rsp_reatt
│   ├── ems_app.c             # ＋CMD_REATT 分发 + RX 环 256→1024
│   ├── ems_config.h          # ＋EMS_REATT_UART_BAUD（默认 921600）
│   └── ems_infer_npu.c       # ＋EMS_NO_TCNQ_NET 守卫（Release 下 TCN 槽位桩）
├── Appli/Core/Src/main.c     # ＋USER CODE 2 波特率运行时覆盖（不动 .ioc）
├── Appli/CMakeLists.txt      # ＋EMS_REATT_FORM=CPU|NPU|SPLIT 源/守卫选择
├── Host/tools/
│   ├── quantize_reatt.py     # ★ 量化部署链（keras→raw 变体→fp32/int8 tflite
│   │                          #   + L0 对拍 + 三口径一致报告；--check 幂等校验）
│   ├── gen_reatt_params.py   # ★ 归一化常数生成器（3 冻结 JSON → params）
│   ├── gen_reference_split.py# ★ 拆分链 host 参考（ORT N1_int8 + N2_int8 +
│   │                          #   NPU 输出边界量化口径；精度归因报告）
│   ├── reatt_board_client.py # ★ 板级客户端（--ping/--parity/--latency/--form
│   │                          #   whole|split，独立协议栈，不改 ems_inject）
│   ├── reatt_split_probe.py  # ★ 拆分形态单窗探针（含固件诊断行原始捕获）
│   ├── wc_test_client.py     # ★ workclf 判别实验客户端（CMD_WC_TEST 0x08）
│   └── npu/                  # ★ NPU 导出/量化/拆分脚本（见其 README）
├── Host/reference/reatt/     # ★ 对拍向量（64 窗原始域 + host fp32/int8 +
│                              #   reatt_ref_vectors_split.npz 拆分链参考）
├── Host/results/reatt_board/ # ★ 量化/板级证据（quant_report / split_ref_report /
│                             #   board_parity[_split]_report / wc_test_report）
└── （其余 = TCN64 副本遗产：ECMS 控制律/plantstep/闭环工具链/既有 reference
    与 results——条目级说明沿用母本 README，本 README 只登记增量）
```

I/O 契约（CMD_REATT，线上为**原始域** f32）：

```text
REQ  [seq u32][hist 300×11 f32（t 主序，通道序 = registry INPUT_H1）]
     [kf 30×6 f32（k 主序，通道序 = registry INPUT_K+INPUT_F）]   = 13924 B
RSP  [seq u32][q 30×3 f32 kW（[q10,q50,q90]）][infer_us u32]      = 368 B
板内 xnorm[t][ch] = (xraw−MEAN[ch][t])/STD[ch][t]（formal_norm_v3 逐 bin）
     knorm[k][ch] = (kraw−MEAN[ch][k])/SCALE[ch][k]（norm_ext_v1 逐 lead）
     q10=m−softplus(a)，q50=m，q90=m+softplus(b)（稳定 softplus，固件实现）
```

## 验证链（spec §验证链；先验阈值，FAIL 不放宽）

| 层 | 内容 | 状态 |
|---|---|---|
| L0 | host int8 vs keras（64 参考窗，整图形态） | **PASS**（q50 MAE 23.5 kW ≤25 / corr 0.981 ≥0.98 / 有限 100%；`Host/results/reatt_board/quant_report.json`） |
| L0′ | 拆分链 int8 vs keras（同 64 窗） | 报告性：q50 MAE 29.6 kW / corr 0.981（**超整图 int8 的 25 kW 门**，如实登记不放宽；归因见 §NPU 线） |
| L1 | 板 vs host int8（64 向量） | **FAIL**：整图 NPU 形态 MAE 347.5 kW（`board_parity_report.json`）；拆分形态 MAE 288.1 kW（`board_parity_split_report.json`）——两形态同为"输出与输入无关"退化 |
| L1b | workclf 简单图板上判别（200 向量） | ~~FAIL~~ → 生命周期修复后 **40/40 = 100%**（归一化口径）⇒ NPU 通路正常 |
| L1c | S0 判别（golden 输入 → 服务 reattn2n，服务生命周期） | **0/90 逐字节一致失败**（m 恒 −128 单值 vs golden 7 唯一值）⇒ **REATT 图自身在 NPU 上算错**，与输入无关 |
| L1d | 切子图（dl1..dl4/dlf1，含全 f32 仅 Tile 的 dl3） | **全部 stall 于 run_continue**（不报错、不 DONE）⇒ 切子图不可执行，分区验证策略不可实施 |
| 终局 | **REATT-on-NPU NO-GO（ST Edge AI 4.0.1）** | 证据链闭合（NOTE.md 终章 + `NPU_ISSUE_HANDOFF_PROMPT.md` 移交提示词）；交付形态 = 整图 CPU（`../EMS02_CHIL_REATT8_CPU/`） |
| L2 | 板 vs keras fp32 | 同 L1（报告性，退化） |
| L3 | soft-HIL 闭环 6 航次（C2-board vs 冻结 C2-host 配对） | 不执行（无可用 NPU 形态；CPU 形态为验收交付形态，见 `../EMS02_CHIL_REATT8_CPU/`） |
| 时延 | CMD_REATT infer_us + 往返 | 拆分形态：N1 CPU ≈944 ms + N2 NPU ≈175 ms（RS-T 诊断行）；infer_us 统计受 DWT 计数回绕污染，待修 |

## NPU 线（2026-09-23，拆分形态；用户裁决的"TCN 放 CPU、其余尽量 NPU"路线）

**动机**：整图 NPU 形态输出退化（m 通道恒 −128、跨输入不变）已定性于 ST Neural-ART
编译/执行语义层（见 `Model/reatt8_q_npu/NOTE.md` 假设台账）。用户裁决把"有问题的
那一部分（TCN）放 CPU，其余能放 NPU 的尽量放 NPU"——拆分后若 N2 正常，则根因锁定
在 TCN；若 N2 同样退化，则 NPU 路线定论关闭。

**形态选择（CMake）**：`-DEMS_REATT_FORM=CPU|NPU|SPLIT`（本工程默认 SPLIT）

| 形态 | 组成 | 产物 |
|---|---|---|
| CPU | 整图 CPU 编译形态（f32 边界，SYNC） | `Model/reatt8_q/`（CPU 交付工程默认；本工程可回退） |
| NPU | 整图 NPU 形态（EC blobs + NOR 权重 0x70300000） | `Model/reatt8_q_npu/reattnpu.*`（phase4-g 实验形态） |
| SPLIT | N1=TCN CPU 形态 + N2=Attention/Dense NPU 形态 | `Model/reatt8_q_npu/part_n1/` + `part_n2/` |

**拆分数据流**（h_past 共享缓冲零拷贝，无量化往返）：

```text
hist_raw ──冻结归一化(f32)──> N1 图(reattn1c，CPU 形态，SYNC)
                                 └─> h_past[96] f32（同一缓冲）
kf_raw ──归一化+int8 量化──> N2 图(reattn2n，NPU 形态)
                              ↑ h_past 直喂（IN_2 = 384B f32，生成物契约）
                              └─> raw (m,a,b) int8 [90B] → 反量化 → 分位变换
```

**拆分预算**：N1 = 168,864 参数（macc 56.7M / 权重 584,392 B 入 ELF / 激活 283,776 B）；
N2 = 843,971 参数（权重 atonbuf 877,009 B → NOR 池 0x70300000 占 83.6%，激活 1.08 MB
在 npuRAM3-5）。Release：ROM 1.64 MB / 5%，RAM 657 KB / 31%；appli 尾 0x702908AC 距
权重池 0x70300000 余 445 KB。

**板上结果（2026-09-23，Release + NOR 权重，逐字节校验一致）**：

- ✅ N1（CPU 形态，含 int8 图在 f32 边界的 Lite 执行）：h_past 非退化且随输入变化
  （RS-N1 行实测 min −1.04 / max 15.10）；与 host ORT 同图参考同量级（差异 ≤0.31，
  属 int8 执行引擎口径差，非结构性故障）。
- ❌ N2（NPU 形态）：输出是**与输入无关的常数**——整窗 30 个 lead 的三元组完全相同
  （RS-N2 行实测 int8 码 (−128,−111,−111)），跨 12 个不同输入窗 m 通道全为 −128
  （= 量化零点）；且**同一输入两次调用结果不同**（另一次实测 (−128,−28,+12)）⇒
  NPU 侧执行不可信，非量化口径问题。
- ❌ **判别实验**：workclf（本平台历史上可正常执行的简单图，权重在 NOR 0x70400000）
  在**本 appli** 中 CMD_WC_TEST 全 NACK ERR_INFER（40/40）⇒ 共享的 NPU 集成路径
  （NOR 权重通路 / 缓存契约 / EC 运行时）在本镜像内即不可用，不能用"图复杂度"解释
  N2 退化。
- 结论：**TCN 不是根因**（N2 无任何卷积、同样退化）；拆分路线未能救活 NPU；NPU 线
  按 2026-09-03 TCN64 定性与本次双重证据保持 **NO-GO**，剩余可查项 = 共享集成的
  NOR 完整性 / 生成物 rc 码（需 Dev boot 读回 + 带插桩重建）。

**精度归因（离线，与部署同口径；`Host/results/reatt_board/split_ref_report.json`）**：
全 fp32 拆分链 vs keras 仅 0.015 kW（**拆分本身无损**）；int8 链合计 29.6 kW
= TCN 段 20.5 kW + N2 段 16.1 kW（整图 int8 为 23.5 kW）。⇒ 若将来恢复 NPU，
精度提升的杠杆在 TCN 段（int8 敏感度最高），而非 N2。

**烧录流程修订（重要）**：外部 loader 在 **flash boot**（运行态固件已把 XSPI 配成
内存映射）下读通路失败（连 256 B 都 `Data read failed`）——必须 `-c port=SWD mode=UR`
（connect-under-reset）让 loader 复位后独占 XSPI；Dev boot 下普通连接即可。扇区几何
（loader 上报）：32 MB / 4 KB 扇区 / 页 256 B，扇区码 = 地址偏移 ÷ 4096；appli 区
[256 656]，N2 权重区 [768 982]。

## 构建 / 门禁入口

```text
prepare:  python3 Tools/atk_stm32n6_cmake_prepare.py EMS_Projects/EMS02_CHIL_REATT8_NPU
build:    Appli 预设 Debug（TCN64 基线+桩）/ Release（REATT 全量，交付形态）
形态:     -DEMS_REATT_FORM=CPU|NPU|SPLIT（默认 SPLIT）
gate:     python3 firmware/gate/fw_gate.py <本工程路径>   （2026-09-23 拆分形态 7/7 PASS）
量化再生:  .venv_tf/bin/python Host/tools/quantize_reatt.py [--check]
拆分参考:  .venv_tf/bin/python Host/tools/gen_reference_split.py     （ORT N1+N2 同口径链）
拆分探针:  .venv_tf/bin/python Host/tools/reatt_split_probe.py --port <串口>（含诊断行捕获）
对拍:      .venv_tf/bin/python Host/tools/reatt_board_client.py --port <串口> --form split --parity
workclf:  .venv_tf/bin/python Host/tools/wc_test_client.py --port <串口> --n 200
参数再生:  .venv_tf/bin/python Host/tools/gen_reatt_params.py [--check]
```


## soft-HIL 配套（Model E 侧，本工程之外）

`model/simulink_modelE_(Phase4_Physics)/`：`python/ppe/c2_server_board.py`
（板后端文件桥服务，`--mock` 联调模式已验证 2026-09-22）、
`scripts/+ppe/c2_bridge_board.m`、`scripts/+ppe/run_softhil_voyage.m`
（genB 正式执行路径的板后端副本，输出 `results/ppe_practical/soft_hil/`，
非正式标记 SOFT_HIL_NONFORMAL）。
