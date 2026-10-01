# S4_PRED_PHYSICS_RCA — 偏差④「REATT 预测-实况物理脱节」根因分析

> 日期：2026-09-29 · 分支 feat/phase4-g · 分析方式：纯 host 决定性对照（P1）+ 源码考古到行（P2）
> 实验脚本与原始输出：`experiments/deviation4_rca/`（p1_anchor_decisive / p1b_layer_compare /
> p1c_n2_isolation / p1d_lineage / p1e_c3_stats / p1f_c3_oos / p1g_tflite_cpu / p1h_kf_inspect /
> p1i_systematic，输出 JSON 同目录；不入库）
> 结论先行：**训练侧完全健康；物理脱节由部署链两段级联失真 + C3 闭环 kf fixture 垃圾输入造成；
> 「BD/ST/HB 三方一致」的前提本身不成立（三链三个数值世界，从未在 kW 域一致）。**

---

## 1. 一句话根因

REAT-T 是**量化脆弱架构**（attention + LayerNorm + GELU），两条 int8 PTQ 管线
（TFLite `Optimize.DEFAULT` / onnxruntime QDQ）都造成输入依赖的系统性失真
（锚级 ±20~45%，低负载锚最重），NPU 编译/执行段再叠加一层同量级失真；
而 S1~S4 全部判据只验证「实现一致性」（链内互拍），从未把「输出 vs 物理真值」
纳入门禁——失真在每道门都是「已测出、未门禁、被放行」。

## 2. 决定性证据链（全部新测，机器可复算）

锚 = FLTA_S1_D00000_M2 t0=300（S3-C 首锚），物理真值：Y(30×10s bin) mean = **168.4 kW**，
瞬时 P_load(300)=170.5 kW，未来 300 s 均值 168.4 kW。

| 链 | 输入 | q50 mean30 (kW) | vs 真值 |
|---|---|---|---|
| keras fp32（C2Predictor 冻结管线） | canonical hist+kf | **163.96** | −2.7% ✓ |
| 同上，H1 环构造 hist（板侧语义复刻，自冻结 1 Hz 流） | 逐位 == canonical | 163.96 | ✓ |
| TFLite fp32（`reatt_raw_fp32.tflite`） | 同 | 163.96 | 无损 ✓ |
| 部署图 fp32 ONNX（`reatt_concat_fp32.onnx`：静态 attention+tanh GELU） | 同 | 163.97 | 无损 ✓ |
| **TFLite int8**（`reatt_raw_int8.tflite`，CPU 形态源） | 同 | **115.72** | **−31%** ✗ |
| **ORT QDQ int8**（`reatt_concat_int8.onnx`，NPU 形态源） | 同 | **91.23** | **−46%** ✗ |
| HB（板源 OE N1+N2 拆分图，ORT 代理） | 同 | 94.31 | −44% ✗ |
| **BD（板，NPU 执行）** | 同（755 战役回放字节） | **67.0**（q50[0]=64.96） | **−60%** ✗ |

三锚复核（Y vs keras q50 mean30）：M2@300 168.4/164.0；M2@1000 485.6/485.4；
M1@300 139.9/140.3 —— 训练侧模型 + 冻结特征管线物理保真成立（C1 假设否掉：
target = 10 s bin 能量等价均值 kW，绝对域，无语义断层）。

12 锚系统性强对照（keras / tfl_int8 / ort_int8）：int8 失真为**输入依赖**，
非恒定比例——tflite 比值 0.71~1.17、ort 0.56~1.11；高负载锚（Y≈460-486）两链
基本正常（0.89~1.0），低负载锚（Y≈93-168）失真最重。⇒ 典型 PTQ 激活范围失配
特征，非偏置常数。

N2 段隔离（keras h_past 直喂板源 N2）：q50 137.6（−26 kW）；N1+N2 全链 94.3
（再 −43 kW）⇒ **N1/N2 两段各自贡献失真**，非单点 bug。

### 2.1 「三方一致」证伪

755 战役 EVAL_755_REPORT 自登记：ST vs HB h_past E_rel 中位 **0.979**、
n2_out_code 最大码差中位 **36**（≈130 kW）、max 110；BD vs HB（全 755）同量级。
门禁只查 BD==ST 逐位（h_past）、无饱和、漂移平稳性、确定性——**绝对物理保真
从未入判**。BD≡ST 仅证板忠实执行了 ST 编译产物；ST 编译产物 ≠ OE 图 ≠ keras。

### 2.2 C3 闭环 A2 反常（434→117，corr=−0.34）归因

A2 = keras fp32 + canonical 管线（本已证健康）。闭环中植物跟踪计划良好
（corr(v_plant,v_ref)=0.995，P_load≡demand），hist 侧无失真。
**根因 = S20_composite fixture 的 kf 块与训练域严重不符**：

| kf 通道 | 训练域（npz 全局） | S20 fixture | 判定 |
|---|---|---|---|
| s_plan_m | [0, 2100]，**issue 相对**累积距离，跨 lead 递增（25/250/750） | [462, 23242]，**绝对**沿程位置且跨 lead 常值 | 双重错误（参考系+内容） |
| h_chart_m | [4.17, 8.82] | 恒 20.0 | 越界 2.3× |
| wl_fcst_m | [−1.69, 2.71] | 恒 20.0（=water_depth） | 语义错列 |
| u_fcst_mps | [−1.49, 1.34] | 恒 0 | 可容忍 |

归一化后 s_plan z 值 ≈ +30σ，模型被迫深度外推 ⇒ 434→117 反物理轨迹。
BH（466）= kf 垃圾 × int8 失真的复合；板（bb）再叠 NPU 段。
**形式臂（S4-P3，profile 1/2）用 formal 航次真 kf，不受 R3 影响，但仍受断层①②影响。**

## 3. 根因到行

### R1 int8 PTQ 结构性破坏（断层①，最大贡献者）

- `EMS04_CHIL_FULLCTRL/Host/tools/quantize_reatt.py`：`conv8.optimizations =
  [tf.lite.Optimize.DEFAULT]` + representative_dataset（约 175-181 行）——
  裸默认全整数量化，无 QAT、无敏感层保留、无逐通道激活；标定 1024 train 窗
  口径正确但对该架构不足。
- `EMS04_CHIL_FULLCTRL/Host/tools/npu/npu_quantize_int8.py:70-72`：
  `quantize_static(..., activation_type=QInt8, weight_type=QInt8, per_channel=True)`
  —— 激活 per-tensor QInt8 MinMax，LayerNorm/softmax/GELU 输出分布长尾被压垮。
- 当时已见信号被均值门禁放行：EMS02 L0 登记「host int8 vs keras q50 MAE 19.88 kW /
  **max 250.1 kW** / corr 0.9850」（≤25 过）；E 段 ONNX int8 自评 59-62 kW MAE 照登。

### R2 NPU 编译/执行段二次失真（断层②）

- 现象：ST 编译（OE→aton）改变图语义——ST（ST Edge AI 自仿）vs HB（ORT 跑同一
  OE 图）h_past E_rel≈0.98；板上 BD≡ST 逐位（板忠实）。
- 定性继承：TCN64 工程 2026-09-03 已判同类「Neural-ART int8 执行语义与解释器
  结构性分歧」NO-GO（`EMS01_CHIL_ECMS_TCN64/Host/results/tcn_swap/
  layer2_diagnosis_20260903.json`）；本 NOTE（`Model/reatt8_q_npu/NOTE.md`
  「板上结果与结论」节）亦登记「m 通道恒 −128、输出与输入解耦」。
- 结论：此段是工具链层阻塞，非本仓库可修；规避 = 不走 NPU。

### R3 S20 PILOT fixture kf 生成错误（闭环输入污染）

- 生成器：`model/simulink_modelE_(Phase4_Physics)/scripts/+dataset_v3/export_pilot.m`。
  - `wl_fcst_m` 填 `h_chart`（=water_depth 20）：`export_pilot.m:137`
    `kf.wl_fcst_m(ia, :) = repmat(h_chart, 1, nL);` —— 语义错列（浪高≠水深）。
  - `s_plan_m` 绝对坐标语义：`export_pilot.m:160` `kf.s_plan_m(ia, k) = x_plan;`
    （x_plan 起点 = `ds.x_ship_m(t0)` 绝对位置），与 formal「issue 相对累积」
    语义不符（对照 `+mission_v3/kf_snapshots.m:49` 注释与 formal mat 实测
    s_plan[0]=25=v×10）。
- 产物陈旧：S20_composite.mat 记录 producer_script_sha256=`625fc8f0…`，
  现行 export_pilot.m sha256=`91786450…`——不一致；且实测 s_plan 跨 lead 常值，
  与现行代码的逐 lead 前向积分行为亦不符 ⇒ 现存 mat 是旧版生成器的产物，
  且现行代码仍存在参考系语义错误。

## 4. 影响面重审清单

| 结论/档案 | 状态 | 重审 |
|---|---|---|
| E300 R²=0.9931（离线 test，fp32） | **有效**——但口径=frozen winner 的 fp32 离线指标；部署形态从未继承该指标 | 论文/报告中引用时必须标注「fp32 离线口径，部署链不保真（本 RCA）」 |
| S1~S2 位级对拍（377/377 等） | 有效（实现一致性） | 不变 |
| S3-C 755 战役 PASS | 「链路确定性/无饱和/漂移平稳」有效；**「预测服务可用」解读作废**——物理保真未证且实测不保真 | 结论降级登记 |
| C3 v2 541/541 G6=True | bb-vs-bh 实现一致性有效；一切物理解读（含 bb_d1 恒 −0.1 的「DP 下界/后端漂移传递」归因）**改判**为 q50 系统性失真 + kf 垃圾输入的必然下游 | 归因改判 |
| C4 v2 / S4-P3（formal 臂） | kf 健康（真 formal kf）；但 int8+NPU 断层①②仍在 ⇒ 物理结论无效 | 同上 |
| EMS02 CPU 形态「已验收」 | L0（19.88/250 max）为均值口径实现一致性；**不代表物理保真**（本 RCA 锚测 −31%） | 验收口径重审 |
| law6 / DP / PMS / 契约冻结件 | 实现一致性证据不受影响 | 不变；但物理有效性恢复前闭环行为无物理意义 |
| 「BD/ST/HB 三方一致 ⇒ 断层在共同上游」（偏差④登记文） | **前提证伪**：三链三个数值世界（164/94/67），从未一致；正确表述 = 断层级联在部署链 | 登记修正 |

## 5. 修复选项（P3 候选，主 Agent 裁决）

- **方案 A（推荐）：fp32 全 CPU 形态**——`reatt_raw_fp32.tflite` 已证 = keras
  （163.958，逐位级一致）；经 stedgeai 4.0.1 全 CPU 编译（无 --st-neural-art，
  reatt8_q 同流程），CMake `EMS_REATT_FORM=CPU` 切源。风险点：权重 ~4 MB 入 ELF
  （NOR appli 31.7 MB 域内可行；SRAM 直载窗 3.75 MB 装不下 ⇒ 板级验证须走 NOR
  release 烧录）；fp32 激活 RAM 与单帧时延需 analyze/板测（int8 CPU 形态 800 ms，
  fp32 估 2-4 s，10 s 节拍内可接受则成立）。
- 方案 B：QAT / 混合精度 int8 重训或再量化——物理保真上限接近 fp32，但工作量
  以周计且不过门风险高；不符合当前交付窗口。
- 方案 C：NPU 段排障——ST 工具链层（历史 NO-GO），不可行。
- **附带必修（与方案无关）**：S20 fixture 重建（修 export_pilot.m 的 wl_fcst 错列
  与 s_plan 参考系，重生成 mat + kf 表），否则闭环三链继续吃垃圾 kf；formal 臂
  不需要。

## 6. 判据盲区治理建议（防再发）

1. 一切部署形态（CPU/NPU/SPLIT）的验收门必须含「**输出 vs 物理真值**」硬判据：
   canonical 锚集 q50 vs Y 的 MAE/相对误差（建议沿用离线口径阈值量级，如
   MAE ≤ 12 kW 且 max ≤ 25 kW，数值待裁决），不得以链内互拍替代。
2. 闭环 fixture 的 kf/hist 输入须过「训练域包络校验」（逐通道 min/max + 语义抽查）
   方可用于 formal 结论。
3. corr/均值类指标不得单独作为保真门禁（系统偏置下仍可 >0.98）。

---

*证据复算入口：`experiments/deviation4_rca/p1*.py`（.venv_tf，纯 host，只读冻结件）。*

---

## 附录 A（2026-09-29 深夜增补）：修复线实测 + 第四根因（_ring_v 字段序）

### A.1 CPUF32 修复线板级实证（P3e 锚回放门）

`EMS_REATT_FORM=CPUF32`（Model/reatt8_f32，fp32 全 CPU）XIP 镜像
（sha256 `233982da79ab…`，三重校验烧录）flash boot 实跑，6 pilot 锚
（S2 canonical 字节经 CMD_REATT 回放）：

| 锚 | 真值 Y | keras | **板（修复前 SPLIT）** | **板（CPUF32）** | CPUF32 时延 |
|---|---|---|---|---|---|
| M2@300 | 168.4 | 164.0 | 67.0 | **163.96（diff 0.0）** | 1118 ms |
| M2@2180 | 363.5 | 372.6 | 284.3 | **372.59（diff 0.0）** | 1127 ms |
| M2@4070 | 92.8 | 97.7 | 54.7 | **97.69（diff 0.0）** | 1116 ms |
| M1@300 | 139.9 | 140.3 | 51.6 | **140.27（diff 0.0）** | 1118 ms |
| M1@2180 | 324.2 | 318.9 | 247.9 | **318.91（diff 0.0）** | 1125 ms |
| M1@4060 | 92.7 | 91.3 | 41.9 | **91.27（diff 0.0）** | 1116 ms |

⇒ 板 == keras（显示精度内逐位），单帧 ~1.12 s ≪ 10 s 控制拍（+DP 5.23 s 合计
~6.4 s = 64% 占空，实时预算内）。**预测链物理保真在板恢复。**

### A.2 第四根因 R4：闭环 runner `_ring_v` 字段序错位（TICK2 v[1]/v[2]/v[3]）

CPUF32 上板后 C3 重跑首段暴露：BH（host 代理）≈20 kW 而 A2（keras）≈207 kW。
定位：runner 环推样函数 `_ring_v`（`Host/tools/s4_c3_closedloop.py:659`）按
`(t, SOC, v_ref, P_load, …)` 组帧，而 TICK2 契约（固件 `ems04_h1win.c`
TICK2_TO_RING 注释 = host 复刻 `s4_c3_h1win_host.py` 头注）为
`(t, P_load, SOC, v_ref, …)`——**v[1]/v[2]/v[3] 三字段旋转错位**，导致：

- 环 P_load 通道 ← SOC ≈ 0.6（应为 100~600 kW）；
- 环 v_ref 通道 ← P_load ≈ 数百（应为 1.5~5.7 m/s）；
- 板与 BH 同吃环 ⇒ 同错（v2 的 "bb≈bh 一致性" 照样成立——一致性≠正确性的又一实例）。

该错位自 C3 runner 诞生起存在（git HEAD 逐版本同序），v1/v2 闭环结论均被其
污染（与 kf 垃圾、int8/NPU 失真三层叠加；v2 的 BH median 466 = 错误环 × 垃圾
kf × int8 的复合）。**A2 链不吃环（子步流直构），故 A2 数值始终由真输入决定。**
LCD「预测线画到绘图区底部以外」即此 bug 的可视化（预测 ~20 kW vs 实况
数百 → 贴底出框），非显示层缺陷。

修复：`_ring_v` 改回契约序；C3/C4 runner 新增 fail-closed sanity（正式首拍
环 P_load 列均值 vs 预热植物均值，[0.5×, 2×] 带外拒绝进正式段）。
验证：修复后 dry 跑 BH ≡ A2（167.2==167.2 逐拍），sanity 门 128.6 vs 117.5 过。

### A.3 pilot 航次族 kf 审计（R3 影响面补全）

S02/S03/S08（C4 电池）同病：s_plan 绝对坐标（1670→3311，部分越训练域）、
wl_fcst=水深 20、h_chart=20；S20 另加 s_plan 跨 lead 常值（陈旧生成器产物，
mat 记录 producer hash ≠ 现行脚本 sha256）。**C4 v2 三航次结论同样作废。**
formal 臂（FLTA_*，mission_v3 生成）健康。S20 v2 修复件：kf 全语义重算
（v_plan=v_ref 右端点、s_plan issue 相对累积、u=persistence、wl=0、
h_chart=20 保持物理+OOD 登记），MAE 200.8→77.3 kW；残差经消融归因为
water_depth=20 OOD（depth 8 输入下 MAE 0.7~23，仅最高速段 93）——
深度域 OOD 属模型泛化边界，场景级修复（深度改到训练域内）需重建
S20 场景+植物 dylib，列为后续项。

### A.4 判据盲区清单（最终版）

1. 部署形态验收缺「输出 vs 物理真值」门（S1~S4 全程）；
2. A2 vs B_HOST 差异被归档为"变换效应基线"而未逐通道核对（环错位由此逃过）；
3. 均值/相关性类指标掩盖锚级尾部失真（EMS02 L0：mean 19.88 放行 max 250）；
4. 闭环 runner 缺输入侧 sanity（本轮已补：环 P_load 量级 fail-closed）。

### A.5 遗留①（板 DP 恒 −0.100 vs host 跳档）取证定案（2026-09-30，RTT 注入）

**板 DP 层完全洗清**：dev boot + SRAM 取证固件上经 RTT beat 通道注入式直调
`ems04_dp_tick`（跳过推理），**5/5 输入（910/900/920/SOC=0.32/0.33）与 host
预言机位级一致**；910 全量 TickResult 逐字段吻合（dlambda_all 30/30 位级相等、
无 fallback/oob）。叠加：dp_tick 机器码 S4Smoke↔S4XIPF32 逐条一致、四表位级
同源、v1/v2（SPLIT 全路径）多档正常——**分歧根因不在 DP 源码/表/编译产物**，
被隔离在 **CPUF32 flash 构建 TICK_CTRL 的运行时上下文**（DP 前紧邻 1.12 s
stai CPU-Lite 推理；机制候选：推理后浮点状态遗留（FPSCR 舍入/FZ）、XIP NOR
数据面、栈布局隐性踩踏——定死需 flash boot 窗口跑 CMD_DPDBG + FPSCR 取证
补丁）。完整证据链与建议：`experiments/deviation4_rca/p5g_dpdbg_report.md`。
**机制①同日证否**（p5i 实验）：UART RX 环内存注入 CMD_REATT 触发板上真实
推理（rs_ran_before/lcd_q50 确证执行）后，RTT beat 直调 dp_tick —— 两轮 boot
× 4 次 beat（推理后/持续性/复位后/确证闭环）d1 全部位级一致 = −0.055，无一
翻转；S4Smoke 无 XIP，故 CPUF32 flash 分歧主嫌疑转向 **XIP 数据面/构建布局**，
"DP 前 FPSCR 复位"修复预案不适用（详见
`experiments/deviation4_rca/p5i_mechn1_result.md`；局限：本实验推理为 NPU
形态，与 CPU-Lite 不同构，但其 f32 前后处理同为 FP 密集，通用 FPSCR 遗留假设
被显著削弱）。
附带发现：本 SRAM 构建 UART 全波特率静默（RX 环全失配垃圾、TX 未被触发；
方向=dev boot USART1 内核时钟失配或 CH340@921600——D6 教训复现，未深挖）；
RTT 首应答曾现单比特异常（÷4，不复发，观察项）。

### A.6 UART 死区根因定案 + flash boot 判决备料（2026-09-30 深夜，通宵无监管执行）

**UART 死区根因定案（p5k，机制①证否后的附带取证，SWD only）**：dev boot 下
USART1"双侧死区"的 p5g 方向假设（内核时钟失配）**证否**——寄存器实证时钟链完全
正确（CCIPR13.USART1SEL=1(CKP)、CCIPR7.PERSEL=0(HSI)、HSI/PLL1 READY、BRR=0x45、
PRESC=0，boot banner 实际发完）。真根因 = **CH340@921600 ↔ BRR=0x45（927,536 baud，
+0.645%）链路容差失配**：14 帧 PING 全部"帧头 AA/55 与 CRC 尾部坏、长游程字节
06/00/00 恒干净、全程零 FE/NE/ORE"（小幅失配采样相位漂移的教科书指纹）；SWD 现场
改 BRR=0x22C（115200，−0.08%）后 **3/3 PING 收到位级完美 ACK**（双向链路当场复活，
BRR 已恢复 0x45）。p5i 的 TX 楔死（TXFNF=0）未在干净重载后复现，列未决次要观察项。
⇒ 判决手册主通路改为 SWD RX 环注入（不依赖 UART）；UART 可用性经可选 BRR 补丁
恢复。详见 `experiments/deviation4_rca/p5k_uart_deadzone_rca.md`。

**遗留① flash boot 判决备料完成（p5j）**：S4XIPF32 构建（含 CMD_DPDBG(0x14)/
RSP_DPDBG(0x15)，ELF sha256 `2468a405…`，fw_gate 全 PASS·ALLOW_IOC=1）经
`s4_xip_flash.sh` 烧录 NOR @0x70100400：备份 0x70100000+4MB（sha `9bc7a5d8…`）→
厂商 --verify 通过 → 独立读回 4,942,696 B 实质差 0 B → FSBL 0x70000000..64KB
前后哈希一致（`9b24a8a5…`，零 FSBL 写）、N2/N1 红线区未触碰、单次烧写尝试；
板留 dev boot。备份与读回件归档
`experiments/deviation4_rca/nor_backup_20261001_p5j/`。明早人工拨 flash boot 后按
`experiments/deviation4_rca/p5j_verdict_manual.md` 15 分钟判决：d1=−0.100 → 栈
canary/FPSCR 取证（方案已写、实施待裁决）；d1=−0.055 → XIP 数据面收口（四表
SWD 直读 vs .xip.bin 逐字节比对）；判决不依赖 UART（SWD 环注入兜底已写入手册）。

### A.7 遗留①判决执行（2026-10-01 凌晨，flash boot UART，p5j 手册落地）

**判决：−0.100 在 flash 交付形态不复现——遗留①收窄至 TICK_CTRL 全路径特有因素。**
用户三裁决（.ioc=R1′ 方案 / 波特率回落 115200 / 取证按规划）落地后，S4XIPF32
115200 版（hex sha `edcbfe94…`，二烧三重校验 PASS）flash boot 实跑，判决全走
UART（**flash boot 下 SWD 被生产 boot 安全关断——probe-rs/CubeProgrammer 双证，
p5j 手册 SWD 路径不可用，手册预判失误如实登记**）：

1. CMD_DPDBG 直调（fresh gen state、跳过推理）：d1 =
   **−0.05500000000000001（0xBFAC28F5C28F5C2A）= host 位级一致**（rc=0、oob=0、
   fallback_n=0，RSP 800B CRC OK，5.37 s，dlambda_all 前 6 项同 p5g 基线）
   ⇒ **②XIP 数据面/执行环境洗清**（表正确性另有烧录读回 0 实质差双证）。
2. CMD_REATT 真实 CPU-Lite fp32 推理（RSP CRC OK，板载 q50 首步 207.48 = host
   同值）→ 再 CMD_DPDBG：**d1 仍逐位不变** ⇒ **①fp32 推理遗留浮点状态在真
   形态洗清**（p5i NPU 形态实验的交付形态补齐）。
3. FPSCR/栈 canary 取证补丁未实施（分支 A 未触发；SWD 关断亦使 flash 态取证
   不可行；判别实验已覆盖取证意图）。

⇒ v3 闭环 −0.100 剩余候选仅：**跨拍持续 gen state（g 由历史拍演化，DPDBG 用
fresh g）** 与 **tick_ctrl 调用上下文/缓冲**。下一步（无需 SWD，flash boot
UART 即可）：从 v3 CSV 重建分歧拍 ring 输入（按 R4 修复后口径）→ CMD_TICK
全路径复现测试 → 二分定位至单因。完整数据：`experiments/deviation4_rca/
p5m_verdict_result.md`（本地）。工程附带：921600 退役、115200 全链可用
（.ioc/main.c/ems_config.h 同步修正，本次提交）。

### A.8 遗留①终局定案：TICK_CTRL 线格式错位（off-by-4B，2026-10-01）

**遗留① CLOSED——根因是集成 bug，不是数值分歧。** 判别链（UART，flash boot，
p5n 全记录）：①TICK_CTRL#1/#2（runner 8×f32 打包，N0 钳 1 与 PMS 反馈两态）
均 −0.100 → 复现且排除 N0；②SOC f32 舍入 delta=0 排除；③板 q50 vs host q50
max diff 1.068e-4 kW，但 host 探针×板 q50 = −0.055、**DPDBG×板 q50 = −0.055**
→ q50 微差与板表均不动结果；④源码逐行核数：`ems04_tick_ctrl_handle` 消费
seq+tick+**7**×f32（36B）即 memcpy hist/kf，而 `S4_CMD_HEAD`/runner/契约均为
**8×f32（40B）**——板消费的 hist/kf 整体错位一个 f32（pad 0.0 作首特征、kf
尾元素丢失）；⑤**handler 口径对齐帧（7f+尾 pad）→ e_hat = −0.055 翻回**
（lam 0.4484→0.4934 同步翻转）。⇒ 板上 DP/预测数学自始位级精确（①FPSCR/
推理遗留、②XIP 数据面、持续 gen state、N0 假设全部独立证伪），v3/C3 的
"板 d1 恒 −0.100"是板对**错位特征**的忠实计算；同因解释 R4 为何能漏网——
门禁体系只做 board-vs-board / host-vs-host 对比，从未在未错位输入上做过
board-vs-host 的 DP 层对拍（DPDBG 通道为此而建并当场命中）。修复（handler
补齐第 8 浮点一行 / 或 runner 改 7f）属裁决项：将改变板端闭环数值口径
（错位→真实），建议独立于 ThreadX 线单独裁决实施并重录基线；历史档案不重写。
完整判别链：`experiments/deviation4_rca/p5n_tickctrl_misalign.md`（本地）。

### A.9 post-RTOS（ThreadX 四任务）DPDBG 位级对比（2026-10-01 续，p5r）

RTOS bring-up 三根因修复后（s_thr_attr[4] / _sbrk 堆基址 / Prot osDelay，
详见 PLAN §7.5），S4Smoke SRAM RTOS 形态（ELF 73cccc4b）重跑 A.5 同款对拍：
- **DPDBG×p5a['910']（SWD 环注入，载荷 [seq][SOC][N0][q50×30]，gen 态
  st=3,3 与 beat A-02 同构）**：d1 = **0xBFAC28F5C28F5C2A（−0.055）**，
  rc=0、oob=0、fallback_n=0——与 A.5 pre-RTOS RTT 基线、A.7/A.8 flash
  DPDBG 基线**三方逐位一致**；J_window（12.948…）/soc_end（≈0.60400）/
  fuel（≈14.874）/dlambda_all[0]=−0.055、[1]=−0.065 全字段与 A.5 断点版
  dump 吻合。
- RTT beat 通道（312B 帧同输入）同构复验：d1 = 0xBFAC28F5C28F5C2A，
  flags=DP_OK——**任务化零数值影响（§5.3 位级红线 post-RTOS PASS）**。
- 手法登记：UART RX 环 SWD 注入前需 DCIMVAC(0xE000EF60) 按行失效
  D-cache（UART 环无 RTT 的 cache_ci 自愈纪律）。
完整证据：`experiments/deviation4_rca/p5r_rtos_bringup_closure.md`（本地）。
