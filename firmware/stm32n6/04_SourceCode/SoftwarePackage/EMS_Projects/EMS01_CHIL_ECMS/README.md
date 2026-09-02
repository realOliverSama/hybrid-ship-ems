# EMS01_CHIL_ECMS — Phase 3 预测型 A-ECMS 固件工程（CHIL 平台）

> **立项**：2026-08-31（用户决定）。Phase 3 EMS 固件工程，规划依据
> `docs/research_roadmap.md` §8；工作区规则见仓库 `AGENTS.md` §11 与 `../README.md`。
> **Status**: T-505（路径 A 中间件栈 + CMake AI 集成 + NPU 后端 + Debug/Release 构建
> + fw_gate PASS）已落地；此前 T-501（工程骨架/构建）→ T-502（STEdgeAI 两模型部署包）→
> T-503（App 骨架与控制律）→ T-504（REF 后端）。spec `ems01-chil-ecms-v1`，
> 剩余 T-506~T-509（Host 注入/联调/硬件验证）。

## 目标

把 Phase 2.5 出口形态（MLP-Q + 工况识别器 + G2 门控 + A-ECMS）移植到 STM32N647，
建成 CHIL 平台的控制器侧：NPU 跑 tflite 推理（模型热切换）、MCU 跑 ECMS 控制律，
与 PC 端 Simulink plant 步进同步闭环——支撑全模型对比大表与后续 trajectory-aware EMS 演进。

## 范围（对应 roadmap §8.3 实施路径）

1. AI 推理框架：ST Edge AI / X-CUBE-AI 集成，int8 tflite 部署（MLP-Q 26KB / TCN-f64 220KB / N-BEATS ~1.5MB）
2. 模型热切换机制：换权重文件即换预测模型（外部 NOR 32MB＝256Mbit，MX25UM25645G，存放模型库）
3. ECMS 控制律移植（CPU）：等效因子 + 网格穷举，1s 控制周期
4. G2 门控逻辑：工况识别器 int8（~50KB）+ 逆水段关断回退
5. CHIL 通信：与 Simulink plant 的接口（形式待 HIL 资料确认）+ 步进同步

## 约束（Do Not Change）

- ECMS 控制语义、ê 输入输出定义、门控逻辑与 Phase 2.5 出口形态一致；
- 不得为 MCU 本地方便重新定义 AI 输入输出语义 / 系统时间尺度（`docs/agent/STM32N6_ARCHITECTURE.md` §1）；
- 遵守 `../AGENTS.md`：不复制 prepare 脚本与 BSP 进工程、`.ioc` 修改需用户授权、
  生成代码只改 USER CODE 区域；
- Build / Gate / Debug / Flash 独立阶段，硬件操作红线见仓库 `AGENTS.md` §11。

## 结构（T-501 骨架起步，随任务补全）

```text
EMS01_CHIL_ECMS/
├── README.md                  # 本文件
├── EMS01_CHIL_ECMS.ioc        # CubeMX 工程（源自 05_Serial；T-501 增补 Appli 上下文 XSPI2
│                              #  = MACRONIX 256Mbit 八线 Port2，读外部 NOR 权重的前提）
├── CMakeLists.txt / CMakePresets.json / mx-generated.cmake   # CubeMX/CMake 外层
├── gcc-arm-none-eabi.cmake / starm-clang.cmake               # 工具链文件
├── Appli/                     # 应用工程（CubeMX 生成 + ATK prepare 自动块）
│   ├── Core/                  # main.c（USART1 + XSPI1 HyperRAM 初始化）等生成代码
│   ├── CMakeLists.txt         # 工程名 EMS01_CHIL_ECMS_Appli；ATK BSP/LINKER/ARTIFACTS 自动块
│   ├── CMakeLists.txt.atk-backup 等三个 .atk-backup         # prepare 首次修改前的原样备份
│   └── STM32N647XX_*.ld       # 链接脚本；prepare 选定 LRUN_RAMxspi1(Debug)/ROMxspi2_RAMxspi1(Release)
├── Drivers/                   # CMSIS + STM32N6xx HAL（CubeMX 工程内 ST 标准资产）
├── STM32CubeIDE/              # CubeIDE 工程描述（.project/.cproject + 链接脚本副本）
├── Secure_nsclib/             # secure_nsc.h（TZ 支持）
├── Binary/                    # （后续任务：appli.hex / network-data-*.hex 部署产物）
├── Middlewares/STEdgeAI/      # 【工程自有，T-505 路径 A】STEdgeAI 2.2.0 自带中间件子集
│                              #   （Inc 93 头 + Npu/ll_aton + Npu/Devices/STM32N6XX +
│                              #   Lib 1020 CM55 .a；逐位一致副本，6.4MB；vendor
│                              #   Middlewares/AI(1.0.0) 与生成代码跨代不兼容，不引用）
├── Model/                     # （T-502：STEdgeAI 两模型部署包 mlp_q/ + workclf/）
├── Appli/App/                 # 自有代码（T-503 已就位，T-504/505 续增）：
│   ├── ems_config.h           # 编译开关：EMS_FW_MODE∈{REF,NPU}、EMS_GATE_MODE（默认2）
│   ├── ems_params.c/.h        # 【生成物，禁手改】权重/归一化/常数/BSFC 表
│   │                          #   （Host/tools/gen_fw_params.py 生成，sha256 溯源、幂等）
│   ├── ems_infer.h            # 推理后端接口（REF/NPU 同签名）
│   ├── ems_infer_ref.c        # REF 后端：CPU double 前向（MLP-Q + workclf）
│   ├── ems_infer_npu.c        # NPU 后端（T-505，2.2 bare-metal 四步序列 + DWT 计时）
│   ├── ems_npu_mlpq.c/.h      # mlp_q 模型接口实例（LL_ATON_DECLARE_NAMED_NN_INSTANCE…）
│   ├── ems_npu_workclf.c/.h   # workclf 模型接口（num0=8B 旁路 / num1=120B Pdec）
│   ├── ems_proto.c/.h         # T-506：UART 帧协议模块（design §UART 帧表全实现：
│   │                          #   编解码 + CRC16-CCITT-FALSE + 字节流状态机 + STALE
│   │                          #   会话 + 事件环；HAL 无关可 host 环回测试）
│   ├── ems_app.c/.h           # T-505/506：EMS 应用主任务（T-506 起帧解析/分发由
│   │                          #   ems_proto 接管；INFER_TEST 链路行为不变）
│   └── ems_ctrl.c/.h          # 控制律核心（EML eml_ecms_step_mode2.m 逐条移植，
│                              #   double、平台无关；映射表 Host/tests/eml_c_mapping.md）
│                              # Appli/Core USER CODE：main.c 991/2.2 初始化序列移植
│                              #   （npu_ram_init/npu_config/npu_cache_config）+ DWT
├── Appli/HANDOFF_T505_evidence.md  # T-505 机器可读验证证据（构建/readelf/nm/fw_gate/
│                                   #   初始化序列对照表/路径扫描）
└── Host/                      # 主机侧工具（PC，python3；T-506 就位）：
    ├── ems_inject.py          # T-506：串口注入/对拍 CLI（TICK 注入 + INFER_TEST 批量
    │                          #   + WC_TEST 批量（B2 --wc-test）+ --selftest pty 离线链路；
    │                          #   infer/wc 载荷带 seq 自动计数、RSP 按 seq 配对陈旧帧跳过、
    │                          #   --retry 重发；产物 fw/infer/wc_compare.csv/json）
    ├── ems_fault_inject.py    # T-508：FR-508 故障注入 CLI（NaN 输入/停发超时/（含 T-609 闭环注入模式 --closed-loop/--selftest-closedloop）
    │                          #   垃圾帧/CMD_RESET 恢复四场景；协议复用 ems_inject
    │                          #   + test_proto_loopback 冻结源；逐场景 CSV/JSON +
    │                          #   CMD_DUMP 事件环文本；--selftest pty mock 自测）
    ├── plantstep_advancer.py  # T-604：被控对象推进器统一封装（P3 主案 ctypes 后端
    │                          #   加载 libplantstep_<scenario>.dylib + 纯 Python stub
    │                          #   后端 selftest 用，design §6.5；make_advancer 工厂，
    │                          #   init/step(100 子步 0.01s)/state/fuel/voyage_stop/close；
    │                          #   返回 11 键 Outport 信号 dict，非有限数值抛异常）
    ├── ems_closedloop.py      # T-607：soft-HIL 闭环 lock-step harness（CMD_TICK/RSP_TICK
    │                          #   协议栈复用 ems_inject：SerialSession/帧解析/CRC/事件环
    │                          #   不改协议；--port 板模式（PING 预同步）/缺省 replay 无板；
    │                          #   每拍 CMD_TICK→RSP_TICK→advancer.step(100 子步)→记录；
    │                          #   逐拍 CSV + meta（镜像/plantstep 指纹/拍耗时分布/终止原因）
    ├── tools/closedloop_selftest.py  # T-608：闭环 harness selftest 套件（pty mock 板 10 用例 A-I，--json 机器可读）
    │                          #   复用 ems_inject 协议栈 + plantstep_advancer 推进器；
    │                          #   board（--port，PING 预同步+CMD_RESET）/replay 无板
    │                          #   双模式；整跑重做（.aborted 落已收集行）；meta 含拍耗时
    │                          #   分布/事件清单；--selftest replay+stub 30 拍自检）
    ├── tools/run_gp2_gate.py         # T-605：G-P2 codegen 等价门禁 CLI（wrapper ctypes 推进 vs
    │                          #   解释执行子集模型逐拍对照；门禁项 = 拍边界 X 直读
    │                          #   9 状态 ≤1e-6（design §4.4a），派生量阶段时刻插值为报告项；
    │                          #   产物 v3_gp2_<scenario>_{gate.json,compare.csv,simref.csv}）
    ├── tools/closedloop_metrics.py   # T-615：四臂×两场景统一指标（纯 stdlib、只读输入、幂等）：
    │                          #   fuel_raw/fuel_corr/SOC_end/v_rmse/ê 占比/公平性带判定
    │                          #   + 仪器级①② + 层③ + FR-508 代价 + 22 项交叉核对 →
    │                          #   Host/results/closedloop/closedloop_metrics.json + summary.md）
    ├── tools/gen_fw_params.py # 参数生成器（5 只读源 → Appli/App/ems_params.*，单一来源）
    ├── tools/gen_reference_int8.py  # T-506：Python int8 参考（val200 抽样 seed=20260831
    │                          #   + 轨迹全程 int8；复现 T-307/T-312 冻结指标硬门；
    │                          #   B2：另写 reference_int8_val200_logits.csv）
    ├── tools/gen_host_int8_pgen.py  # T-508：host-int8 全链路 P_gen 参考生成（模式 B
    │                          #   NN 输出作 stub 后端 + gcc 编译同一 ems_ctrl.c →
    │                          #   逐 tick reference_int8_pgen_*.csv，层②对拍参考）
    ├── tools/plantstep_shim.c # T-604：ERT 生成接口 C shim（init/set_pgen/step/
    │                          #   get_outputs×11/get_fuel/get_voyage_stop；符号引用
    │                          #   集中顶部宏区，以生成代码实际符号为准微调）
    ├── tools/build_plantstep.sh     # T-604：gcc -O2 -fPIC -shared 把 <scenario>/
    │                          #   HybridShip_ModelD_PlantStep_ert_rtw/*.c + shim 编译
    │                          #   为 libplantstep_<scenario>.dylib（manifest 由 23 号
    │                          #   MATLAB 脚本写，本脚本只编译并打印 dylib sha256）
    ├── reference/             # T-506 生成：val200 向量（raw/norm）+ int8 参考 CSV
    │                          #   + logits 参考（B2，WC_TEST 对拍）
    │                          #   + P_gen 参考（T-508 reference_int8_pgen_*.csv）
    ├── plantstep/              # T-604：codegen 被控对象推进器产物（按场景烘焙，重跑覆盖）：
    │                          #   <scenario>/HybridShip_ModelD_PlantStep_ert_rtw/（ERT C 码）
    │                          #   + <scenario>/build_manifest.json（指纹：模型/参数
    │                          #   sha256、MATLAB 版本、场景变量指纹、逐文件 sha256、
    │                          #   ert/grt 标志）+ <scenario>/libplantstep_<scenario>.dylib
    │                          #   （tools/build_plantstep.sh 编译，不入 manifest）
    └── tests/                 # L0 单测（T-503）：test_ctrl_diff.py（22 用例全过）+
                               #   test_driver.c（白盒驱动）+ eml_c_mapping.md（映射表）；
                               #   T-506：test_proto_loopback.py + test_proto_driver.c
                               #   （ems_proto 环回 14 用例：CRC 正反/截断/垃圾流/STALE/
                               #   seq 回显布局/WC_TEST 回环）；
                               #   T-508：test_driver_int8ref.c（层② P_gen 参考驱动：
                               #   模式 B stub 后端 + 同一 ems_ctrl.c，由
                               #   tools/gen_host_int8_pgen.py 编译驱动）
```

构建 / 门禁入口（同 Vendor 规约）：

```text
prepare:  python3 Tools/atk_stm32n6_cmake_prepare.py EMS_Projects/EMS01_CHIL_ECMS
build:    ... --build Debug | Release
gate:     FW_GATE_ALLOW_IOC=1 python3 firmware/gate/fw_gate.py \
          firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS
```

## 验收锚点

- CHIL 复现 MLP-Q G2 基准（capture ≈ 0.589，S20 节油 ≈ 1.02%）→ 平台可信；
- 之后逐个换权重跑全模型矩阵（指标同口径：capture / FR-318 / fuel）。
