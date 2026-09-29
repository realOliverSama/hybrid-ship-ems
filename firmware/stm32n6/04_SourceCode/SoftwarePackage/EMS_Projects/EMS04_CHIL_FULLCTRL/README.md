# EMS04_CHIL_FULLCTRL — 全控制器上板 S2 载体工程（phase4-g，EMS03 副本）

> **立项**：2026-09-25（EMS04-S2 阶段 1：工程创建 + S1 law6 冻结决策律集成 +
> RamRel 构建实测；纯 host 操作，本阶段不碰板/串口/调试器）。
> **定位**：law6（Model E C2 臂控制律）**全控制器板上化**的 S2 载体——S1 已在
> host 完成决策律 C 移植的位级对拍（377/377 anchor 拍 Δλ 位级一致，证据见
> `experiments/softhil_ems03_2026-09-25/ems04_s1/`），本工程把冻结常数表、
> 决策律与拍回放服务装进固件，供 S2 板级对拍（SRAM 直载 + RTT 逐拍）使用。
> **母本**：`../EMS03_CHIL_REATT8_NPU2/`（2026-09-25 整树复制；仅排除
> `Appli/build*/`、各级 `st_ai_ws/`、`st_ai_output/`、未跟踪 stedgeai 工作区
> `ws/`、`__pycache__`——均经 git 确认为未跟踪生成产物且构建无引用）。
> EMS03 的全部条目说明沿用母本 README；本 README 只登记 EMS04 增量。
> **不改项**：EMS03 的 CMD_REATT UART 服务原样保留（G3 soft-HIL 用），与本工程
> 新增的 RTT 拍回放服务互不干扰；`.ioc`/Boot/Security/vendor 其余内容零改动。

## 本工程增量（相对 EMS03）

### Appli/App/ 新增（全部为 EMS04 自有文件）

| 文件 | 内容 |
|---|---|
| `ems04_s1_constants.h` | 【生成物，禁手改】S1 冻结标量常数（`export_constants_table.m` → `gen_constants_header.py` 链导出的 `ems04_s1_constants.h` 原样拷入） |
| `ems04_law6_c2.c/.h` | law6 C2 臂每拍决策律（`t445.b2_window_dp` 位级 C 移植，S1 host 对拍 377/377 PASS）。EMS04 增量：`ems04_pg_at()` palette 访问器 + `ems04_tables_board()` 绑定入口；保留 S1 的 `PG_F32` 开关设计，板侧经 CMake 恒定义 `EMS04_PG_PALETTE=1` 走 palette 路径，文件加载路径板侧编译排除 |
| `ems04_tables_gen.c/.h` | 【生成物，禁手改】冻结常数表 C 源（`Host/tools/gen_ems04_tables.py` 生成）：`bsfc_lut[376]`/`soc_grid[171]`/`dlam_grid[41]` 保 **f64**（S1.5 实证三表 f32 化有位损，合计 4.7 KiB）；`policy_table_Pg`（3020×66×3 f64）palette 化为 `ems04_pg_palette[51]` f64 + `ems04_pg_index[597960]` u8（**584.3 KiB**，原 f64 表 4.56 MiB） |
| `ems04_rtt_min.c/.h` | SEGGER-RTT **线格式**最小实现（vendor 树无 RTT 中间件；仅实现标准控制块 + 通道 0 上下行环，SKIP 模式）。仅服务 S2 对拍，非生产链路 |
| `ems04_beat_svc.c/.h` | 拍回放服务（见下方帧格式）；板**拍间无状态**——SOC/N/gen0 状态推进不在板上实现，由 host 逐拍喂入（状态重构同 S1 口径） |
| `ems04_sha256.c/.h`、`ems04_fp_proto.c/.h`、`ems04_h1win.c/.h`、`ems04_kftable.c/.h`、`ems04_fp_svc.c/.h` | 【C-fw，2026-09-26】EMS04-S2 特征面板（FPC-1.0 / `PROTOCOL_FP.md`）B 阶段模块，自 `experiments/softhil_ems03_2026-09-25/ems04_s2/b/` **逐字节拷贝**（10/10 文件 sha256 源=目标；溯源哈希见 `…/ems04_s2/c/C_FW_REPORT.json`）。仅在 `EMS04_FP_PARITY=1` 时编入 |
| `ems04_fp_board.c/.h` | 【C-fw，2026-09-26】FP 服务 ↔ 既有 ems_proto UART 层胶水：`ems04_fp_board_handle(type,payload,len,rsp,rsp_len)` 把一条完整命令载荷重装成 `PROTOCOL_FP §0` 帧（CRC 用既有 `ems_proto_crc16/_chain` 现算）喂 `ems04_fp_svc_feed`，产出的响应线帧（ACK/NACK/RSP_FP_QUERY/RSP_FP_DUMP，上限 13939 B）交回调用方直发 UART；静态 svc 实例 486,088 B BSS + 诊断计数。仅在 `EMS04_FP_PARITY=1` 时编入 |

### Host/ 新增

| 路径 | 内容 |
|---|---|
| `tools/gen_ems04_tables.py` | 常数表生成器（`.venv_tf` python）：从 S1 `exported/` 的 f64 冻结表生成 `ems04_tables_gen.c/.h`；Pg 唯一值 >256 时拒绝生成（palette 无损前提破坏告警） |
| `tools/check_ems04_tables.py` | **位级自校验**：独立重解析生成的 C 数组（十六进制浮点字面量精确回读）与 S1 f64 冻结表逐元素位型比对。结果：**376/376 + 171/171 + 41/41 + 597960/597960 全部位级一致**（机器可读：`results/ems04_tables/tables_check_report.json`） |
| `results/ems04_tables/` | `tables_gen_manifest.json`（生成清单）、`tables_check_report.json`（自校验报告） |
| `tools/fp_replay_client.py` | 【C-fw，2026-09-26】EMS04-S2 特征面板串口回放客户端（`.venv_tf` python + pyserial）：`--list-ports` / `ping`（CMD_PING 0x06）/ `replay --scripts <b/vectors_b2/script_B2_*.bin…> --capture <out.bin> --baud 115200 [--batch 512] [--progress 50]` / `--dry-run`（不连板自检）。B2V1 脚本束解析后**帧字节原样透传**，按脚本自带 EXPECT 做响应级流控（板侧 RX 环 1024 B vs DUMP 单帧 13939 B），`in_waiting` 排水 + 超时/零进展保护 + 异常时报告已发送字节数 + summary JSON。**2026-09-27 增 `--tail-drain SEC`（默认 0=行为不变）**：分块诊断传输（TYPE 0x27）下一个 DUMP 产生 29 条响应帧、超出脚本 EXPECT 计数，按计数停读会截断 capture（实测丢尾部 4 块 + 2 个 NACK）；给定该项后脚本跑完继续排空至线路静默 |

### S3-A2 / S3-C 资格与诊断工具（2026-09-27 新线）

| 路径 | 内容 |
|---|---|
| `Appli/App/ems04_fp_dumpchunk.c/.h` | 【诊断形态，`-DEMS04_DUMP_CHUNK=1`，默认 0 零体积】RSP_FP_DUMP(0x26) 13939 B 单帧在 115200 长流丢帧头（用户裁决 2026-09-26 §14"Repair only the diagnostic transport"），改为 TYPE 0x27 分块直发（块头 18 B：seq/idx/total/data_len/last/rsv，数据 ≤494 B/帧，13932 B → 29 帧）；DUMP 查询（0x25）与载荷字节不变，主机收齐 total_chunks 自行重组 |
| `Appli/App/ems04_s3c_q27_shadow.c/.h` | 【qualification-only，`-DEMS04_Q27_SHADOW=1`】N1 生成码 `LL_ATON_End_EpochBlock_2` 内 Q27（int8[1,300,11]=3300 B @0x34357080）clean 之后插入一次影子 memcpy + CRC32 头（SEQ 自增、fail-closed valid 位），使 Q27 在 arena 复用前留证；插桩经 Host/tools/patch_reattn1n_q27shadow.py（patch/--restore 两向），非干预性见 `s3c_provenance.json` |
| `Appli/App/ems04_s3c_bddiag.c/.h` | 【qualification-only，`-DEMS04_BDDIAG_UART=1`，默认 0 零体积】S3-C BD 腿批量诊断回传：CMD_BDDIAG(0x0C) → RSP_BDDIAG(0x0D, 666 B = seq + Q27 影子 CRC/valid + h_past 96×f32 + n2out 90×i8 + n2kf 180×i8)；只读边界缓冲（`ems_reatt_split.c` 尾部同开关 accessor）与 Q27 影子头，不改推理数学/既有命令；主机客户端 `Host/tools/s3c_bddiag_client.py`（纯 UART 批量，无需 halt）；构建记录 `experiments/…/tmp_rescue_20260927/T8_BDDIAG_BUILD_PROVENANCE.md` |
| `tools/s3c_reatt_replay.py` | CMD_REATT 回放客户端（S2 真值包 canonical hist/kf + sha256 校验）→ q30×3 raw f32 + infer_us capture |
| `tools/s3c_bddiag_client.py` | BD 腿批量诊断客户端（BdDiagUart 形态）：逐锚 CMD_REATT→CMD_BDDIAG，落盘 `.672.bin`（n2out/n2kf/hpast 重排布局）+ `.meta.json`（seq/q27_crc32/valid/q30×3/infer_us）+ `BDLEG_MANIFEST.json` |
| `tools/s3c_verify.py` | S3-C 边界逐位核验器（A/B/C/D 边界） |
| `tools/s3c_pilot_batch.py` | 六锚批式驱动（复用 reatt_replay 的 Board/锚加载）+ `--shadow-dump` 离线校验 Q27 影子缓冲镜像（magic/hdr_len/len/valid/CRC fail-closed） |
| `tools/s3a2_board_test.py` | S3-A2 分块 DUMP 板测驱动：VOYAGE_INIT + 29×KF_CHUNK + 301×TICK2 + QUERY + DUMP，收 0x27 流重组 13932 B；断言块连续性/last_flag/长度/零坏帧/零 NACK（`--dry-run`/`--selftest` host 自测） |
| `tools/s3a2_dump_reasm.py` | 0x27 capture 重组工具（独立实现，按 seq 分组，fail-closed） |
| `tools/s3a2_chunk_to_frame.py` | **传输适配器**：0x27 分块流原位替换为 0x26 单帧（载荷 = 板侧重组字节），使按 0x26 语义编写的 A1-A8 断言机可直接消费分块形态；fail-closed（连续性/last_flag/CRC/长度/单 seq）。往返自测：0x26 载荷切块再适配 ≡ 原 capture |
| `tools/s3a2_check.py` | S3-A2 时序语义断言机 A1-A8（capture 离线核验器）。**2026-09-27 勘误 3**：负路径期望值对齐冻结固件 gate 序（QUERY(299)→0x43 范围门、QUERY(300.5)→0x40 锁存；原稿 0x45/0x43 与冻结固件/冻结脚本/板级先例三方矛盾），`simulate_capture` 同步实现锁存语义 |
| `tools/s3c_provenance.json` | S3-C 工具链/冻结制品 SHA 清单（experiments 侧产物以 SHA 绑定；含板级证据 ELF 身份） |

### S4 线新增（2026-09-28，C1 纯 host 段）

| 路径 | 内容 |
|---|---|
| `Host/plantstep_e/<scenario>/` | 23E 生成码 + `build_manifest.json` + `libplantstepE_<scenario>.dylib`（GRT，MATLAB 安装树头文件经 `EMS_MATLAB_ROOT` 注入编译；`MAT_FILE=0` 绕缺失的 `builtin_typeid_types.h`）。场景 `S20_composite_voyage`（dylib sha256 `df81b23f…`） |
| `tools/plantstep_e_shim.c` | PlantStepE GRT 接口 shim（16 输出/10 状态直读 API，Model D shim 同形扩口） |
| `tools/build_plantstep_e.sh` | shim + 生成码 → dylib 编译脚本（`EMS_MATLAB_ROOT` 必填） |
| `tools/run_gp2e_gate.py` | **G-P2E 门**（S4-C1 门①②）：codegen dylib vs 解释执行参考逐拍对照；X 直读 4 状态（x/v/SOC/V_dc）1e-6 门禁；参考 = `reference/s4_gp2e_<scenario>_simref.csv`（MATLAB 解释执行、根 Inport 外部驱动 300 kW 恒值、拍相位=dylib tick m ↔ MATLAB index m*1000+1，一处一拍采样对齐依 Model D G-P2 §4.4a 同法核实）。**结果：全量 719/719 PASS，worst=3.64e-12（机器精度）**；产物 `reference/s4_gp2e_*_gate.json` + `_compare.csv` |
| `tools/s4_c1_modelE_selftest.py` | S4-C1 门③：modelE 后端扩展 selftest（M1 加载/校验、M2 IC、M3 推进健康、M4 G-P2E 证据引用、M5 双臂报告引用）。**10/10 PASS** |
| `reference/s4_gp2e_S20_composite_voyage_simref.csv` | 解释执行参考逐拍基准（720 拍 × 20 列，X 直读 + Y 报告面） |

### S4-C2/C3 线新增（2026-09-28/29，板级冒烟与闭环段；含前序补登）

| 路径 | 内容 |
|---|---|
| `tools/run_g1_pms_gate.py` | 【补登】S4-G1 门：PMS/Safety C 移植 vs Simulink 参考逐拍对拍（6001/6001 位级）；State 布局权威副本 |
| `tools/s4_c2_smoke.py` | 【补登】S4-C2 门：TICK_CTRL 板级冒烟客户端（合成状态 + S2 真值包 hist/kf，100 拍零 NACK） |
| `tools/s4_c3_preamble.py` | 【补登】C3 前置：B_HOST 同输入对拍（板 vs host 同链 100 拍；执行层 100/100 一致，11 拍 d1 差=后端漂移传递已登记） |
| `tools/s4_c3_pack_kf_table.py` | C3-①：S20 kf 表打包器（EMS04KFT v1，profile_code=3=PILOT_FIXTURE；白名单取材 `datasets_v3/pilot/S20_composite.mat` 的 `kf.*`，禁触 stream 真值列；AST 静态自查） |
| `tools/s4_c3_h1win_host.py` | C3-②：H1 特征窗 host 版（`ems04_h1win.c` 语义逐位复刻：301 环/11 列冻结序/f64 构造域/f32 RNE 出口/fail-closed；37 项自检） |
| `tools/s4_c3_closedloop.py` | C3-③：闭环 runner——plantE↔board 实时闭环单航次 + 三链同录（A2_CANONICAL_HOST=C2Predictor canonical 域 / B_HOST=Chain 板源 OE 代理 / B_BOARD=板 RSP）→ G6 判据机械化 → `results/s4_c3/s4_c3_gate.json`；口径登记 `results/s4_c3/s4_c3_design_notes.md` |
| `tools/s4_c4_battery.py` | C4：多航次电池闭环 runner（冻结电池 S02_downstream/S08_resistance_bands/S03_upstream，一次板会话顺序跑、每航次独立复位与 gate，电池级合取 → `results/s4_c4/s4_c4_gate.json`；复用 C3 组件与 v2.1 归因口径） |
| `plantstep_advancer.py`（改） | SCENARIOS_E 注册表扩 S02_downstream/S08_resistance_bands/S03_upstream（C4 电池 dylib） |
| `results/s4_c2/` | C2 冒烟与 C3 前置产物（`s4_c2_gate.json`/`s4_c2_smoke_report.json`/`s4_c3_preamble_report.json`/`libems04_pms_safety.dylib`） |
| `results/s4_c3/` | C3 产物（`s4_c3_warmup.csv` 预热记录 / `s4_c3_closedloop.csv` 三链逐拍 / `s4_c3_gate.json` G6 判定 / `s4_c3_design_notes.md` 口径登记 / `run1_park_archive/`+`run2_rulestop_archive/` 原始记录归档） |
| `results/s4_c4/` | C4 产物（每航次 CSV+warmup+gate / 电池级 `s4_c4_gate.json`）；另含 P3 冻结件 `S4_P3_FORMAL_ARM_FREEZE.json`（formal 臂电池覆盖 4 航次，主 Agent 定稿）与 worker 草案/Phase A 报告 |

### S4-P3 B1 段新增（2026-09-29，formal 臂流驱动植物变体 host 装配；冻结件 `results/s4_c4/S4_P3_FORMAL_ARM_FREEZE.json`，规格 `experiments/softhil_ems03_2026-09-25/S4_P3_STREAM_PLANT_SPEC.md`）

| 路径 | 内容 |
|---|---|
| `tools/s4_p3_pack_stream.py` | P3-B1①：formal 航次 stream bin 打包器（EMS04STM v1，规格 §3 冻结格式：20 B 头 + f64×4 行 [t_s, P_prop_realized=P_load_total−P_hotel, P_hotel_kW, v_ship_mps]；源列白名单 4 列 + AST 禁读闭环响应列自查；自校验 = bin sha256 / n_rows==duration_s+1 / 首末行 float.hex 指纹 / 派生恒等式独立第二路径逐位复算 → `plantstep_e_stream/<vid>/stream_check.json`）。冻结 4 航次 4/4 PASS |
| `plantstep_e_stream/<vid>/` | P3-B1①产物：每航次 `<vid>.stream.f64.bin` + `stream_check.json`（S2_D01006_M1/M2 = 4680 行 sha `8c854f4a…`；S6_D05003_M1/M2 = 3791 行 sha `fbb64959…`；同族两臂共享回放流——臂差异只走 kf 表，不进植物） |
| `tools/s4_c3_pack_kf_table.py`（改） | P3-B1②：参数化 `--profile-code`（1=GOOD_V1/2=DEGRADED_V1/3=PILOT_FIXTURE；默认 3 行为逐字节不变——S20 selfcheck 回归表 sha `8a1f597e…` 与改前一致）；formal 4 航次出表（M1→1/M2→2）落 `experiments/softhil_ems03_2026-09-25/results/s4_c3/` |
| `tools/s4_c3_closedloop.py`（改） | P3-B1②：`load_kf_table` profile 断言放宽为登记域 {1,2,3}（S20 默认路径 code=3 行为不变，5 表装载回归通过） |
| `plantstep_advancer.py`（改） | P3-B1③：backend='streamE'（ModelEBackend 注册表/库目录/文件名/提示提为类属性——modelE 默认行为零改动；SCENARIOS_E 追加 4 冻结航次名、既有 4 项不动；dylib 模式 `plantstep_e_stream/<vid>/libplantstepE_stream.dylib`；新增 `streamE_dylib_path()` 就绪检查助手）。closedloop_selftest 回归 PASS=10 SKIP=0 FAIL=0 与改前一致 |
| `tools/s4_c4_battery.py`（改） | P3-B1④：`--plant-backend streamE` + `--data-root`（装配 P3 冻结件 schema `s4_p3_formal_arm_freeze_v1`；stream 数据源 = formal .mat；产物目录改 `results/s4_p3/`；kf 表/stream bin/dylib 三级启动就绪检查，dylib 缺席 → `s4_p3_gate.json` PLANTSTEP_DYLIB_PENDING 门 rc=3 待 B2）。默认 pilot/modelE 路径零改动，selftest 20/20 保持 |
| `results/s4_p3/` | P3-B1④产物：`s4_p3_gate.json`（PLANTSTEP_DYLIB_PENDING 门，overall 恒 false，B2 以正式 4/4 合取门覆盖） |

### S4-P3 B2 段新增（2026-09-29，流驱动植物变体 MATLAB 手术 + codegen + 回放保真门）

| 路径 | 内容 |
|---|---|
| `Host/tools/plantstep_e_stream_shim.c` | P3-B2④：流驱动 shim（API 与 plantstep_e_shim.c 同形；init 经 dladdr 同目录读 `<vid>.stream.f64.bin`（EMS04STM v1），ZOH 每 100 substeps 换行喂 4 根 Inport；voyage_stop=行尽∨模型终止；`get_states` 按名映射归一到 advancer 10 键契约序——本变体生成码 X 字段序与 PlantStepE 不同，实测 2026-09-29） |
| `Host/tools/build_plantstep_e_stream.sh` | P3-B2④：shim + 23S 生成码 → `plantstep_e_stream/<vid>/libplantstepE_stream.dylib`（`<vid>|all`；EMS_MATLAB_ROOT 注入、MAT_FILE=0 绕行；`-install_name` 固定使 4 航次 dylib **逐字节一致** sha `e987d0cd…`；前置检查 manifest+stream bin） |
| `Host/tools/run_gp2e_stream_gate.py` | P3-B2⑤：**G-P2E 回放保真门**——子步级全程核验 P_load_total_out ≡ P_prop_realized+P_hotel（1e-9 主门）+ v_ship 直通 + x_ship ForwardEuler 积分一致性（1e-6 m）+ SOC/Vdc 健康 + 终止语义 → `results/s4_p3/gp2e_stream_gate.json`。**结果：4/4 PASS，G1/G2 逐位 0 mismatch（max diff=0），x_ship 相位 B 参考逐位一致** |
| `Host/plantstep_e_stream/codegen/` | P3-B2③：23S GRT 生成码（单份，4 航次共用）+ `build_manifest.json`（三模型哈希零改动门禁 PASS） |
| `Host/plantstep_e_stream/<vid>/libplantstepE_stream.dylib` | P3-B2④产物：每航次一份（同一二进制 ×4） |
| `Host/results/s4_p3/gp2e_stream_gate.json` | P3-B2⑤门产物（4/4 合取 overall=true） |
| `Host/results/s4_p3/s4_c4_gate.json` 等 | P3-B2⑥ host 干跑产物（`--plant-backend streamE --dry` 4 航次；dry 语义 overall 恒 false） |

### P1/P2/P4 段新增（2026-09-29，Phase 4 收官续）

| 路径 | 内容 |
|---|---|
| `Appli/App/ems04_tick_ctrl.c/.h`（改） | P1：TICK_CTRL handler 入口 DWT 自愈重武装（DEMCR.TRCENA/CYCCNTENA——boot 窗口 TRCENA 被清的冻结实测）+ RSP 补 reserved f32 占线（infer_us@84/ctrl_us@88/flags@92 与 host 三工具既有解析逐字节对齐，93 B 契约不变）；P2：DP 段 CYCCNT 夹 + CMD_PROF(0x10)/RSP_PROF(0x11) 24 B 段级分账回放（reatt/dp/grid/pms，附加命令，不动 TICK_CTRL 契约） |
| `Appli/App/ems_reatt_split.c`（改） | P1：infer_us 改 CYCCNT 周期域差值再换算（u32 模算术吸收 7.16 s 回绕；755 战役"偶发 u32 溢出/0"伪值根因修复） |
| `Appli/App/ems04_law6_c2.c`（改） | P2：DP 位级加速 J1（FLOOR 前缀合并，单调性剪枝）+J2（机组对对称崩塌，按位比较）+J3（内层查表前置）；验证=377/377 replay 逐拍位级 + 板级 100 拍逐拍同一性 + C4 3/3 零回归；板上 DP 6.39→5.23 s |
| `Appli/App/ems_app.c`（改） | CMD_PROF 分发接线（EMS04_S4_FULLCTRL 守卫内） |
| `S4_XIP_BOOT_RCA.md` | **P4：XIP/flash boot 不启动 RCA（D1 build/flash hex 被任意 Release 形态覆盖 + D2 PRED_PARITY 的 XSPI2 重初始化在 XIP 执行态自杀）+ F1-F4 修复方案——烧写/断电=用户批准点（park）** |
| `S4_PRED_PHYSICS_RCA.md` | **偏差④ RCA（2026-09-29）：预测-实况物理脱节根因定案——训练侧健康（锚 q50 MAE 4.5 kW），断层 = int8 PTQ 两管线系统性失真（锚级 −31%/−46%）× NPU 编译执行段二次失真（再 −29%）× S20 fixture kf 垃圾输入（s_plan 绝对坐标+flat、wl=水深）；「三方一致」前提证伪；影响面重审清单 + 修复方案（fp32 CPU 形态）+ 判据盲区治理建议** |
| `results/s4_c2/s4_p2_dp_accel_memo.md` | P2 设计备忘：分账实测表、J1-J3 精确性论证、全部门禁证据、**小帧协议/1 s 拍可行性评估（结论：1 s 拍不可行，10 s 拍占空 67% 满足实时预算）** |
| `results/s4_c2/s4_c3_preamble_report_preP2_baseline.json` | pre-P2 对拍基线封存（重跑逐拍同一性对照锚） |
| `results/s4_c4_preP2_baseline/` | C4 冻结态封存（P2 重跑零回归对照锚） |
| `Host/tools/s4_c3_closedloop.py`（改）+ `Host/tools/s4_c4_battery.py`（改） | **口径 v2**（2026-09-29 用户裁决：执行层 1 s 门控直接补齐，契约 §4.3）：`--exec-period`（默认 10=冻结 10 s 口径逐字节不变，基线回归锚不动；=1 时正式拍剩余 999 子步按整秒边界拆 99+9×100 段，每段前 `EXEC_REFRESH` CMD 0x12（16 B=[seq][SOC][v_ship][P_load] f32）→ RSP 0x13（12 B=[seq][P_gen_ref][lam_eff] f32，板侧 handler=`Appli/App/ems04_tick_ctrl.c` `ems04_exec_refresh`，λ 输入=板自持上拍 TICK_CTRL d1）三链同步刷新：bb_1s=板返回值、bh_1s/a2_1s=host `law.exec_layer/lam_eff` 用本拍 TICK_CTRL d1（段内 e_hat≡d1 恒定）、输入全 f32 线域；观测 CSV `s4_c3_exec1s.csv` / `s4_c4_<vid>_exec1s.csv`（t/soc/p_load/v_ship/bb_pg/lam_bb/bh_pg/a2_pg/diff_bb_bh/diff_a2_bh）；gate 增 `exec1s_observation` 观测面（exec1s_n/exec1s_diff_n/exec1s_max_diff，stop_face=false——观测轮不进 G6 停机面，diff 判据由主 Agent 看首轮板跑数据后定；板失联仍走 G6-1 运行性面）；dry：不发帧、无 bb_1s，bh_1s/a2_1s 照算，植物 pgen 刷新源=bh_1s；预热段 =1 时也逐 1 s 冷启动刷新（is_formal=0 不入判据域）。`BoardClient.exec_refresh` 在 C3 定义、C4 import 复用不复制。回归：C4 selftest 20/20、closedloop_selftest PASS=10 FAIL=0、两 runner dry 冒烟（默认 10 与 =1）PASS |

> C3 的 kf 表与自校验 JSON 落 `experiments/softhil_ems03_2026-09-25/results/s4_c3/`
> （任务书指定路径；该目录属 experiments 结果区）。

## 拍回放服务线格式（RTT 通道 0，字节序 LE）

下行（host → 板）`EMS04_BEAT_FRAME_LEN = 312` B：

```text
[magic u32 = 0x34534D45 ("EMS4" LE)][seq u32]
[SOC0 f64][N f64]
[gen0 6xf64: st1, st2, P_ramp1, P_ramp2, P_lag1, P_lag2]   # t_dwell 板上恒 [0,0]（A-02）
[Pbar 30xf64]                                              # bus 侧 kW，10 s leads，K 恒 30
```

上行（板 → host）16 B：

```text
[seq u32][d1 f64][flags u32]
flags: bit0 dp_ok | bit1 infeasible_fallback | bit2 oob | bit3 tables_fault | 其余 0
```

说明：任务文本中 gen0 记为单 f64，但 S1 口径（位级对拍判据）要求 gen0 六元组
（st/P_ramp/P_lag），单标量无法承载，故按 S1 口径实现为 6×f64（帧内槽位相同、
原位展开），待主 Agent 核准。`d1` 为该拍唯一决策输出（`dlambda(1)`）。

## 构建（RamRel 形态；与 EMS03 README 同流程）

```text
prepare:  python3 Tools/atk_stm32n6_cmake_prepare.py EMS_Projects/EMS04_CHIL_FULLCTRL
          （AUTO 块会被重写为默认形态——既定防呆：本工程 CMakeLists USER 区已显式
           钉死 LADDER 链接脚本，prepare 后无需手工恢复；重跑前确认 USER 区仍在）
build:    Appli Release 构建 + S2-PARITY 精简形态 → build/RamRel：
          cmake -S Appli -B Appli/build/RamRel -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_TOOLCHAIN_FILE=<工程根>/gcc-arm-none-eabi.cmake \
                -DEMS04_S2_PARITY=1 -DEMS_N1_LADDER=0 -DEMS_N1_SEG_ONLY=0
          cmake --build Appli/build/RamRel
          （链接脚本 LRUN_RAMxspi1 由 EMS04_S2_PARITY 钉死，向量 0x34000400；
           SRAM 白名单 [0x34000000,0x34200000)；配方演变史见下方阶段 1b 小节）
build(FP): FP 服务形态 → build/FpParity（C-fw，2026-09-26，仅 host，未上板）：
          cmake -S Appli -B Appli/build/FpParity -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_TOOLCHAIN_FILE=<工程根>/gcc-arm-none-eabi.cmake \
                -DEMS04_FP_PARITY=1 -DEMS_N1_LADDER=0 -DEMS_N1_SEG_ONLY=0
          cmake --build Appli/build/FpParity
          （EMS04_FP_PARITY=1 复用 S2-PARITY 精简载荷集并钉死 LRUN_RAMxspi1；
           实测 RAM 1,379,528 B / 2,096,128 B（65.81%），余量 699.8 KiB）
tables:   .venv_tf/bin/python Host/tools/gen_ems04_tables.py   （再生【生成物】）
check:    .venv_tf/bin/python Host/tools/check_ems04_tables.py （位级自校验，FAIL 禁止构建）
gate:     python3 firmware/gate/fw_gate.py EMS_Projects/EMS04_CHIL_FULLCTRL
```

S2 板级流程（后续阶段）：`debug-stm32n647-sram`（`load_elf_to_sram` 白名单内 +
`rtt_attach` 扫描 RTT 控制块 + 逐拍喂帧 + 上行帧对拍）。对拍判据与 S1 相同：
0.005 格位级一致，不放宽。

## S2 阶段 1 构建实测（2026-09-25，纯 host）

| 形态 | 结果 |
|---|---|
| prepare（dry-run→实际无需写入） | 全部 `already up to date`（母本 AUTO 块随树复制，USER 区 LADDER 钉死保持） |
| fw_gate（Debug + Release-XIP 形态） | **7/7 PASS，exit 0**（`firmware/gate/fw_gate_result.json`；debug_layout 在 [0x34000400,0x34200000) 内、release HEX @0x70100400、.ioc 未动） |
| **RamRel（Release+LADDER=1+SEG_ONLY=1，LRUN_RAMxspi1）** | **链接 FAIL：RAM 区溢出 583,128 B**（region used 2,679,256 B / 2,096,128 B = 127.82%） |

溢出明细（对照 EMS03 RamRel 基线 text+data+bss = 2,059,636 B，白名单余量仅 36.5 KiB）：

```text
本工程新增（RamRel 实测对象尺寸）：
  ems04_tables_gen.c.obj  .rodata 603,204 B
    （pg_index u8 597,960 + pg_palette f64 408 + soc 1,368 + dlam 328 + bsfc 3,008）
  ems04_law6_c2.c.obj     text      3,556 B
  ems04_rtt_min.c.obj     text+data+bss 3,478 B（RTT 环 3,072 B）
  ems04_beat_svc.c.obj    text+bss      848 B（帧缓冲 312 B）
  合计 ≈ 611 KB  →  链接器 RAM 用量 2,679,256 B，超出 [0x34000000,0x34200000) 583,128 B
```

处置：按 S2 阶段 1 硬性判据**停在报告，不自行改内存布局**。候选方案（需主
Agent/用户裁决）：a) 白名单扩窗（S1.5 §7.3 的 SRAM3-6 窗 0x34200000 起空闲
1.75 MB，扩至 0x34380000 即可容纳，需链接脚本 + 调试器白名单双处授权）；
b) Pg 表走 `.EXTRAM`（0x90000000 HyperRAM，链接脚本已有 `.EXTRAM` 段，代价是
确定性/带宽）；c) RamRel 精简 REATT/ladder 载荷（S2 对拍是否需要该载荷属设计
决策）。在裁决前，fw_gate 覆盖的 Debug/Release-XIP 两形态可正常构建。

## S2 阶段 1b：S2-PARITY 精简形态（2026-09-25，主 Agent 方案 c 落地）

**S2-PARITY 定义**（RamRel 精简构建形态，仅随显式 flag 传入的构建生效）：

| 项 | 值 |
|---|---|
| flag 组合 | `-DCMAKE_BUILD_TYPE=Release -DEMS04_S2_PARITY=1 -DEMS_N1_LADDER=0 -DEMS_N1_SEG_ONLY=0`（CMake USER 区新增 cache 开关 `EMS04_S2_PARITY`，非 Release 配置直接 FATAL_ERROR 防误用） |
| 链接 | `STM32N647XX_LRUN_RAMxspi1.ld`（PARITY=1 时钉死，优先级高于 LADDER/XIP 分支）；向量 0x34000400 |
| 保留 | law6（`ems04_law6_c2.c`，palette 路径）+ 冻结表（`ems04_tables_gen.c`，584.3 KiB）+ RTT（`ems04_rtt_min.c`）+ 拍回放（`ems04_beat_svc.c`）+ 既有服务框架（ems_app/proto/ctrl/params/lcd/NPU 运行时栈/workclf） |
| 排除 | REATT 段链全家（`ems_reatt_split.c` + reattn1n/reattn2l 图 + selftest + `ems_diag_ladder.c` + dl1/dl2/dl3/dl4f/dlf1）与 N1 ladder 段图（LADDER=0 不编）；`EMS_REATT_SERVICE` 不再定义 → `ems_reatt.c` 落明确桩（CMD_REATT → NACK ERR_INFER，本形态不做 REATT 推理） |
| 实测（2026-09-25） | **链接 PASS：RAM used 872,360 B / 2,096,128 B（41.62%），余量 1,223,768 B**；PT_LOAD 单段 [0x34000000, 0x340D53A8) 全在白名单；`ems04_pg_index/pg_palette`、beat/rtt 符号在；`ems_reatt_query` 为桩态定义；`nm -u` 零 REATT/NPU 网络未定义符号 |

**两形态不回退验证**：fw_gate 7/7 PASS，exit 0（Debug 0x34000400 / Release-XIP
HEX 0x70100400 / .ioc 未动）；`build/Release` 树抽查 `EMS_REATT_SERVICE=1 +
EMS_REATT_SPLIT=1`、REATT split/N2/diag 符号 125 个在 → **Release-XIP 完整形态
保持**（S2-PARITY 仅影响显式传入的 RamRel 构建）。palette 自校验复跑
4 项位级全 PASS（597960/597960）。

**CMake USER 区改动留档**（本工程 untracked，无 git diff；改动全部位于
Appli/CMakeLists.txt 的 USER 区块，prepare 重写 AUTO 块不影响）：

1. 链接脚本段（`EMS_N1_LINK_XIP` 之后）新增 `EMS04_S2_PARITY` cache 开关
   （默认 `0`，含 `if(NOT DEFINED)` 普通变量兜底，防既有 build 树 genex 展开
   为空导致 Release 树被静默精简）；PARITY=1 时钉死 `LRUN_RAMxspi1` 并对
   非 Release 配置 FATAL_ERROR。
2. AI 接线 USER 块内 26 处 REATT 载荷 genex（FORM=NPU/SPLIT/CPU 源集 + selftest
   + diag ladder + dl 图 + `EMS_REATT_SERVICE/NPU/SPLIT` 编译定义）条件由
   `$<CONFIG:Release>` 扩为
   `$<AND:$<CONFIG:Release>,$<STREQUAL:${EMS04_S2_PARITY},0>[,FORM 条件]>`。
   其余 Release 条件（`EMS_NO_TCNQ_NET` 等）与 Debug 行为零改动。

阶段 1 历史配方（Release + LADDER=1 + SEG_ONLY=1，RAM-Release 段链形态）已由
本形态替代；该配方在 EMS03 仍为部署形态，EMS04 不再使用。

## S2 C-fw：FP-PARITY 形态（2026-09-26，纯 host，未上板）

**定位**：EMS04-S2 特征面板（FPC-1.0 / `PROTOCOL_FP.md`）的板级载体形态。
上层工作区：`experiments/softhil_ems03_2026-09-25/ems04_s2/c/`（报告 `C_FW_REPORT.{md,json}`）。

| 项 | 值 |
|---|---|
| flag 组合 | `-DCMAKE_BUILD_TYPE=Release -DEMS04_FP_PARITY=1 -DEMS_N1_LADDER=0 -DEMS_N1_SEG_ONLY=0`（新 cache 开关，默认 `0`；非 Release 配置 `FATAL_ERROR`） |
| 精简集 | `EMS04_FP_PARITY=1` 时把 `EMS04_S2_PARITY` 普通变量置 1 ⇒ 沿用既有精简载荷集（REATT/ladder 大载荷排除）与 LRUN_RAMxspi1 钉死，既有 26 处 genex 判定零改动 |
| 链接 | `STM32N647XX_LRUN_RAMxspi1.ld`，向量 0x34000400；PT_LOAD 单段 `[0x34000000,0x341510C8)` 全在白名单 |
| 内存 | RAM **1,379,528 B / 2,096,128 B（65.81%）**，余量 **716,600 B（699.8 KiB）**；`s_svc` 静态实例 486,088 B（RX 载荷 64 KB + kf 表 372,736 B + H1 环 19,264 B + 锁存/响应缓冲） |
| 新增源文件 | `ems04_sha256/fp_proto/h1win/kftable/fp_svc`（自 `b/` 逐字节拷入）+ `ems04_fp_board`（UART 胶水） |
| 编译定义 | `EMS04_FP_PARITY=1`（`ems_app.c` 据此在 `ems_dispatch()` USER 区挂 0x20/0x21/0x22/0x23/0x25 路由与 `ems04_fp_board_init()`） |
| 默认 0 时 | FP 源不编译、定义不存在、`ems_app.c` FP 段不编（实测三树 FP 目标文件 0 个） |

**UART 线格式**（复用既有 ems_proto 帧容器，`[AA 55][TYPE][LEN LE16][PAYLOAD][CRC16 LE]`，
CRC16-CCITT-FALSE 作用域 TYPE..PAYLOAD）：0x20 VOYAGE_INIT / 0x21 KF_CHUNK / 0x22 TICK2 /
0x23 FP_QUERY → 0x24 RSP_FP_QUERY / 0x25 FP_DUMP → 0x26 RSP_FP_DUMP / 0x7F ACK / 0x7E NACK。
入向 CRC 校验复用既有 ems_proto 解析面；RSP_FP_DUMP 载荷 13932 B 超既有
`EMS_PROTO_MAX_PAYLOAD`（13924）⇒ 响应帧由 FP 编码器产出完整线帧后直发 UART
（详见 `c/C_FW_REPORT.md` §2.1）。

**验证（实测）**：fw_gate **7/7 PASS exit 0**（Debug 0x34000400 / Release 0x70100400 / `.ioc` 未动）；
S2-PARITY 形态回归 RAM 872,744 B（与改动前同值）；胶水 host 回放台（`c/tools/glue_hostcheck.c`）
对 B2 全量回放产出 10,657,586 B 响应流，与 `b/out_b2/trace_B2_O2.bin` **逐字节相同**（first_diff=none）；
`c/tools/verify_capture.py --selfcheck` 755/755 锚点、0 NACK、exit 0；PTY 全链演练
（`c/tools/fake_board_rehearsal.py`）客户端+核验器双 PASS。

**Host 回放入口**（下一次板级会话，需显式授权）：`Host/tools/fp_replay_client.py`
（回放客户端，`--dry-run` 不连板自检）+ `…/ems04_s2/c/tools/verify_capture.py`（离线核验）。

## 结构（EMS03 遗产部分）

其余条目（Model/reatt8_q_npu REATT 链、Appli 既有 ems_* 应用、Host 既有工具、
Binary hex 资产等）说明见母本 `../EMS03_CHIL_REATT8_NPU2/README.md`。
