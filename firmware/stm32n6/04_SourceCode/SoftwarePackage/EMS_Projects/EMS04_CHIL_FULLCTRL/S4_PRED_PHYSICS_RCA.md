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
