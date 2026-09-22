# phase4-g-reatt-board-deployment — REAT-T 量化部署 + soft-HIL（预测器在环）

Spec: `phase4-g-reatt-board-deployment`
Created: 2026-09-22（用户指令：参考 Phase 3 部署模式，将 Phase 4-F 的 REAT-T
winner 量化部署至 STM32N647 板并进行 soft-HIL，得到测试结果；从 Phase 4 开
phase4-g 分支，完成后并入；以 `EMS01_CHIL_ECMS_TCN64` 副本为基础开发；
烧录调试自主执行不需逐次授权）

## 目标（Goals）

1. **G1 量化部署链**：冻结 REAT-T winner（`reatt_s0.keras`，FR-836 身份
   `d8da6af4…`）→ 部署图裁剪（输出 = raw_proj (m,a,b)，单调分位变换移固件）→
   TFLite int8（部署口径标定 = 归一化 train 窗）→ ST Edge AI 4.0.1 全 CPU
   编译形态（无 `--st-neural-art`，同 TCN64 先例）→ 固件 `EMS01_CHIL_REATT8`。
2. **G2 板上服务**：新 UART 命令 `CMD_REATT`（raw hist 300×11 f32 + kf 30×6 f32
   → q10/q50/q90 30×3 f32 + infer_us），固件内完成冻结归一化（formal_norm_v3
   逐 bin + norm_ext 逐 lead）→ int8 推理 → 稳定 softplus 分位变换。
3. **G3 soft-HIL（预测器在环）**：Model E soft-HIL = PC 侧 Simulink 全模型
   （law6 + PMS + plant，冻结 run_cstar 机械）+ 板上 int8 预测器替换 C2 臂的
   keras 推理后端（新增 `ppe.c2_server_board` 文件桥后端，不改冻结件），
   在选定 formal 航次上跑 C2-board 臂，与冻结 C2-host 结果逐航次配对比较。
4. **G4 证据链**：三口径一致报告（训练口径 == 部署喂入口径 == int8 标定口径）、
   host fp32/int8 对拍、板 vs host-int8/vs keras-fp32 对拍、时延实测、
   闭环配对指标（fuel/SOC/q50 偏差）；全部机器可读 JSON + 报告。

## 非目标（Non-Goals）

- 不重训/不 QAT/不重标定 REAT-T（权重逐字节 = FR836 冻结件）；
- 不改 Model E `.slx`、冻结 campaign 机械（`+ppe/run_formal_voyage.m`、
  `c2_bridge.m`、`c2_inference.py` 原件零改动；soft-HIL 走新增副本
  `run_softhil_voyage.m` + `c2_bridge_board.m` + `ppe/c2_server_board.py`）；
- 不把 law6/PMS 控制律移植上板（板上只承载预测器；控制仍在 PC Simulink——
  与 Phase 3 全控制器上板形态的差异有见 design §边界说明）；
- 不写入任何 formal 命名空间；soft-HIL 证据落 `results/ppe_practical/soft_hil/`；
- 不动 workclf/门控/FR-508 语义；不动既有 NOR 权重区（0x70200000/0x70400000）。

## 关键设计决策（Main Agent，2026-09-22）

| # | 决策 | 依据 |
|---|---|---|
| D1 | 部署图输出裁剪到 `raw_proj` (m,a,b)，分位变换在固件 | TFLite SOFTPLUS 数值溢出（q10=−inf/q90=+inf 实测）；q50=m 不受影响；固件稳定 softplus `max(x,0)+log1p(exp(−\|x\|))` |
| D2 | int8 = TFLite 全整数量化（float32 I/O，沿 TCN tflite 形态），标定集 = **归一化 train 窗**（1024 窗，seed 20260922） | AI_MODELING §22.3：标定必须取部署口径样本；实测 q50 MAE 19.9 kW / corr 0.985（E 段 ONNX int8 为 59–62 kW，本路线更优）；cal4096 反而劣化（25.8 kW）不采用 |
| D3 | stedgeai 4.0.1 **全 CPU 编译形态**（NPU 形态不尝试） | 云端 NPU 编译器卡 Erfc/动态 shape/双输入（st_cloud_notes 三次尝试失败）；CPU 形态 analyze 171/171 PASS、macc 192.9M、权重 1.43MB 入 ELF、激活 325KB 用户缓冲 |
| D4 | 板级验证主路径 = Release NOR appli 烧录（CubeProgrammer appli-only） | Debug SRAM 窗口 2047K 容纳不下 1.98M REATT+应用；embedded-debugger MCP 本会话不可用；烧录已获用户自主授权 |
| D5 | 固件归一化（raw→norm 在板上，冻结常数生成入固件） | T-508 口径契约正面样板（MLP-Q 同模式）；三口径 = 板上喂入域与训练/标定域严格一致 |
| D6 | USART1 运行时波特率覆盖 115200→921600（USER CODE，不碰 .ioc） | 请求帧 13,920B：115200 下 1.24s/帧，350 锚点/航次 ×6 航次不可接受；.ioc 修改需用户授权，运行时覆盖规避且可回退 |
| D7 | C2-board 后端 = 文件桥协议不变，仅换推理引擎（板串口） | c2_bridge 超时 300s 足够容纳串口+推理（~0.5s/锚点@921600）；MATLAB 侧零改动（keras_sha256 溯源字段写源权重身份 + backend 扩展字段） |
| D8 | soft-HIL 航次选择：genB 冻结语料按 tier 分层确定性选 6 条（每 tier 2 条，voyage_id 字典序） | 配对基线 = 冻结 C2-host 逐航次结果（同 voyages）；非正式标注 `SOFT_HIL_NONFORMAL` |

## 边界说明（与 Phase 3 soft-HIL 的形态差异）

Phase 3（EMS01/TCN64）：全控制器（预测器 + ECMS 律）上板，PC 跑 Model D
plantstep。Phase 4-G：**预测器上板**（REAT-T int8 全链：归一化+推理+后处理），
law6/PMS/plant 留在 PC Simulink。理由：Model E 控制侧为 Simulink 内建
（PMS v2 Stateflow + Safety 层），全控制器移植属 Phase 3 规模新工程；
预测器正是"AI 上板"的研究对象，控制律数值等价性已由冻结 campaign 闭合。
该差异在报告与论文表述中须明示。

## 验证链（L0→L3）

| 层 | 内容 | 载体 | 门（先验设定，FAIL 不放宽） |
|---|---|---|---|
| L0 | host TFLite int8 vs keras fp32（raw 变体） | `Host/tools/quantize_reatt.py` 产物 | q50 MAE ≤ 25 kW；corr ≥ 0.98；q* 全有限 |
| L1 | 板 vs host TFLite int8 | `tools/reatt_board_client.py`（64 向量，train/val 抽样） | q50 mean \|Δ\| ≤ 0.5 kW，max ≤ 5 kW；q* 有限率 100% |
| L2 | 板 vs keras fp32 | 同上 | 报告性（预期 ≈ L0 + L1 复合） |
| L3 | 闭环 C2-board vs 冻结 C2-host（6 航次配对） | `run_softhil_voyage.m` + 分析脚本 | 数值健康（无 NaN/发散）；fuel_corr/SOC_end/q50-RMSE 配对表 + 描述统计；q50 逐锚点 RMSE（板 vs host fp32 重放） |
| 时延 | CMD_REATT 全链 + 纯推理 infer_us | RSP 遥测 | 报告性（预算参考：anchor 间隔 10s） |

## 目录与产物

```text
firmware/.../EMS_Projects/EMS01_CHIL_REATT8/     # TCN64 副本 + REATT 服务
  ├─ Model/reatt8_q/            # tflite + stedgeai 生成包 + NOTE + 脚本 + 日志
  ├─ Appli/App/ems_reatt*.c/.h  # 板上 REATT 服务（归一化+推理+变换）
  ├─ Appli/App/ems_reatt_params.*  # 【生成物】冻结归一化常数（sha256 溯源）
  ├─ Host/tools/quantize_reatt.py | gen_reatt_params.py | reatt_board_client.py
  ├─ Host/reference/reatt/      # 对拍向量 + host fp32/int8 输出
  └─ Host/results/reatt_board/  # 板级对拍与时延证据（精选入库）
model/simulink_modelE_(Phase4_Physics)/
  ├─ python/ppe/c2_server_board.py     # C2 板后端文件桥服务（新增）
  ├─ scripts/+ppe/c2_bridge_board.m    # 板后端桥 MATLAB 侧（新增）
  ├─ scripts/+ppe/run_softhil_voyage.m # run_formal_voyage 板后端副本（新增）
  └─ results/ppe_practical/soft_hil/   # soft-HIL 证据（新命名空间）
```

## 任务清单

- [x] T-001 去风险实验（keras→tflite→stedgeai，本 spec §关键数字）
- [x] T-002 分支 + spec
- [x] T-003 工程副本 EMS01_CHIL_REATT8（commit 1504f990；fw_gate 7/7；Debug=TCN64
      基线+桩 / Release=REATT 全量的形态差异登记于工程 README 与 §D4）
- [x] T-004 量化管线正式化 + 三口径报告 + 参考向量（L0 PASS：q50 MAE 23.5 kW /
      corr 0.981 / 有限 100%；再生 tflite 逐字节一致 efde1aff…）
- [x] T-005 固件接入（reatt8_q 包 / ems_reatt / proto CMD_REATT / baud 921600 /
      CMake 配置分形；协议环回 14/14 回归 PASS）
- [x] T-006 双构建 + fw_gate（7/7 PASS，2026-09-22）
- [ ] T-007 Release 烧录 + 板级 L1/L2/时延 —— **阻塞：ST-LINK 未连接**（持续复测）
- [ ] T-008 Model E 板后端 + soft-HIL 6 航次（代码完成 + mock 联调/mock 冒烟通过；
      板上 6 航次待板卡）
- [ ] T-009 结果分析 + 报告（softhil_report.py 就绪，待板上数据）
- [x] T-010a 文档登记 + 检查点提交（1504f990；最终并入 Phase4 待板上阶段完成后）

## 板上阶段执行手册（板卡连接后依序执行；用户已授权自主烧录）

```text
0. 连接检查
   STM32_Programmer_CLI -l          # 须列出 ST-LINK
   ls /dev/cu.usb*                  # 串口设备（EMS_REATT_PORT）

1. Release 烧录（appli-only，保 FSBL；CubeProgrammer + ATK 外部加载器）
   CLI=/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI
   ELDR=<SoftwarePackage>/Projects/... 不适用——用 vendor 外部加载器 MX25UM25645G_ATK-CNN647B.stldr（见 release-stm32n647-appli skill 与 TCN64 先例）
   流程：erase 仅 appli 扇区（0x70100400 起覆盖 hex 字节图的最小扇区集，FSBL 区零接触）
        → program Appli/build/flash/EMS01_CHIL_ECMS_Appli.hex 一次
        → verify + 独立 readback 哈希 → 断开（不 run/reset/go）
   （workclf 权重 @0x70400000 与 0x70200000 既有内容不动；REATT 权重在 appli ELF 内）

2. 板级 L1/L2/时延（固件工程内）
   .venv_tf/bin/python Host/tools/reatt_board_client.py --port $EMS_REATT_PORT --ping
   .venv_tf/bin/python Host/tools/reatt_board_client.py --port $EMS_REATT_PORT --parity
   （L1 门：q50 mean ≤0.5 kW / max ≤5 kW / 有限 100%；产物 Host/results/reatt_board/）

3. soft-HIL 6 航次（Model E；选航次 = 每 tier 字典序前 2：
   HIGH: FLTA_S2_D01009_M1/M2；MID: FLTA_S2_D01004_M1/M2；LOW: FLTA_S6_D05004_M1/M2）
   cd model/simulink_modelE_(Phase4_Physics)
   matlab -batch "addpath('config','scripts'); ppe.run_softhil_voyage('<vid>','C2','C2BridgeToken','g1')"
   （EMS_REATT_PORT 环境变量给板串口；逐航次跑）

4. 配对分析
   .venv_tf/bin/python -m ppe.softhil_report
   （产物 results/ppe_practical/soft_hil/_analysis/）
```

回退：重烧 TCN64 工程 Release appli 即恢复母本板态（本工程未动任何 NOR 权重区）。
