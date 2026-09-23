# EMS02_CHIL_REATT8_CPU — REAT-T int8 板上预测服务工程（phase4-g，CHIL 平台）

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
EMS02_CHIL_REATT8_CPU/
├── Model/reatt8_q/           # ★ REATT-T int8 部署包（reatt_raw_int8.tflite 只读
│                              #   + stedgeai 生成 reatt8.{c,h}/reatt8_data.{c,h}
│                              #   + generate-reatt8.sh + generate_401.log + NOTE.md
│                              #   + reatt_raw_fp32.tflite 参考）
├── Appli/App/
│   ├── ems_reatt.c/.h        # ★ REATT 板上服务（冻结归一化→int8 推理→稳定
│   │                          #   softplus 分位变换；Debug/REF/mlpq 配置为明确桩）
│   ├── ems_reatt_params.c/.h # ★【生成物，禁手改】冻结归一化常数
│   │                          #   （Host/tools/gen_reatt_params.py，sha256 溯源）
│   ├── ems_npu_reatt8.c      # ★ 生成物包装 TU（仅 Release 编译）
│   ├── ems_stai_rt.c/.h      # ★ stai runtime 全局一次初始化共享助手
│   ├── ems_proto.c/.h        # ＋CMD_REATT(0x0A)/RSP_REATT(0x0B) + MAX_PAYLOAD
│   │                          #   提升至 13924B + enc_rsp_reatt
│   ├── ems_app.c             # ＋CMD_REATT 分发 + RX 环 256→1024
│   ├── ems_config.h          # ＋EMS_REATT_UART_BAUD（默认 921600）
│   └── ems_infer_npu.c       # ＋EMS_NO_TCNQ_NET 守卫（Release 下 TCN 槽位桩）
├── Appli/Core/Src/main.c     # ＋USER CODE 2 波特率运行时覆盖（不动 .ioc）
├── Appli/CMakeLists.txt      # ＋reatt 源/守卫（Debug=TCN+桩；Release=REATT 全量）
├── Host/tools/
│   ├── quantize_reatt.py     # ★ 量化部署链（keras→raw 变体→fp32/int8 tflite
│   │                          #   + L0 对拍 + 三口径一致报告；--check 幂等校验）
│   ├── gen_reatt_params.py   # ★ 归一化常数生成器（3 冻结 JSON → params）
│   └── reatt_board_client.py # ★ 板级客户端（--ping/--parity/--latency，
│                              #   独立协议栈，不改 ems_inject）
├── Host/reference/reatt/     # ★ 对拍向量（64 窗原始域 + host fp32/int8 参考）
├── Host/results/reatt_board/ # ★ 量化/板级证据（quant_report.json 等，精选入库）
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
| L0 | host int8 vs keras（64 参考窗） | **PASS**（q50 MAE 23.5 kW ≤25 / corr 0.981 ≥0.98 / 有限 100%；`Host/results/reatt_board/quant_report.json`） |
| L1 | 板 vs host int8（64 向量） | 待板卡连接（门：q50 mean ≤0.5 kW，max ≤5 kW） |
| L2 | 板 vs keras fp32 | 待板（报告性） |
| L3 | soft-HIL 闭环 6 航次（C2-board vs 冻结 C2-host 配对） | 待板 |
| 时延 | CMD_REATT infer_us + 往返 | 待板 |

## 时延优化（phase4-g，2026-09-23）

- REATT 模型 TU（reatt8.c/reatt8_data.c，Lite 内核执行体 forward_lite_* 所在）与服务
  TU 在 Release 下定向 `-O3`（CMake `set_source_files_properties`）——依据：56.7M MACs
  的 ONNX-QDQ 图 967ms vs 192.9M MACs 的 TFLite 原生图 800ms，内核执行效率是主要
  自由度；板上复测待下轮验证。
- 编译源原则（实测教训）：CPU 形态必须用 **TFLite 原生 int8 图**（现状 ✓）——ONNX-QDQ
  图逐算子显式 requantize，Lite 内核执行效率显著更低。

## 构建 / 门禁入口

```text
prepare:  python3 Tools/atk_stm32n6_cmake_prepare.py EMS_Projects/EMS02_CHIL_REATT8_CPU
build:    Appli 预设 Debug（TCN64 基线+桩）/ Release（REATT 全量，交付形态）
gate:     python3 firmware/gate/fw_gate.py <本工程路径>   （2026-09-22 首验 7/7 PASS）
量化再生:  .venv_tf/bin/python Host/tools/quantize_reatt.py [--check]
参数再生:  .venv_tf/bin/python Host/tools/gen_reatt_params.py [--check]
```

## soft-HIL 配套（Model E 侧，本工程之外）

`model/simulink_modelE_(Phase4_Physics)/`：`python/ppe/c2_server_board.py`
（板后端文件桥服务，`--mock` 联调模式已验证 2026-09-22）、
`scripts/+ppe/c2_bridge_board.m`、`scripts/+ppe/run_softhil_voyage.m`
（genB 正式执行路径的板后端副本，输出 `results/ppe_practical/soft_hil/`，
非正式标记 SOFT_HIL_NONFORMAL）。
