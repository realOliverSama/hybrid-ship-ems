# EMS01 CHIL v3 — soft-HIL 真闭环实验 Design

Spec: `ems01-chil-v3-closedloop`
Status: design-approved（2026-09-08 用户批准）
Created: 2026-09-08
Requirements: `./requirements.md`（requirements-approved，2026-09-08）
Brainstorm: `./brainstorm.md`
Branch: `models-research`

> 术语：统一"被控对象（plant）"。被控对象模型（plant model）= 船舶物理系统（船体/螺旋桨/
> 直流母线/电池/发动机）；被控对象推进器（plant advancer）= 主机每控制拍单步推进被控对象的
> 仪器；被控对象子集模型（plant-only variant）= 从 ModelD 提取的、去掉 EMS 控制器的变体模型。

---

## 0. 问题重述与设计目标

v1 开环注入验证了"算得对"（层① 100% exact / 层② 受控 / FR-508 状态机），但 SOC 从未被
板子输出影响。v3 构造真闭环：**主机被控对象现场推进 + 板载 P_gen 回馈功率平衡**，回答
"控得好"。设计目标按依赖序：

```text
G-A1 被控对象仪器合格：子集提取保真（G-P1）+ codegen 等价（G-P2），机器可读；
G-A2 闭环回路正确：lock-step 因果链正确，闭环 (b) 臂复现开环参考行为（仪器级①②）；
G-A3 实验矩阵完整：4 臂 × 2 场景机器可读数据 + 两层指标；
G-A4 单一来源与可复现：参数/方程/工况单一来源，轨迹可复现，产物指纹可复核；
G-A5 不触碰冻结基线：v1.1 ref_G2/baseline_manifest 冻结资产零改动。
```

## 1. 设计总览

```text
                     ┌──────────────────────────────── Host（Python）────────────┐
                     │                                                            │
   ┌──────────┐ CMD_TICK(k) 7×f32 │  ┌──────────────────────────┐                │
   │ STM32N647│◄──────────────────┼──┤ closedloop harness       │                │
   │ EMS 固件 │  RSP_TICK(k) P_gen┼──┤ (ems_closedloop.py)      │                │
   │ (NPU/REF)│───────────────────┘  └───────┬─────────▲────────┘                │
   └──────────┘                              │P_gen(k) │7 状态(k)                │
                     │                       ▼         │                          │
                     │               ┌──────────────────────────┐                 │
                     │               │ 被控对象推进器 (ctypes)   │                 │
                     │               │ = codegen C（P3 主案）    │                 │
                     │               │ ode3 @0.01s ×100 子步/拍  │                 │
                     │               └──────────┬───────────────┘                 │
                     └──────────────────────────┼─────────────────────────────────┘
                                              │
                    单一来源：HybridShip_ModelD_PlantStep.slx（脚本构建）
                    ← +model/build_model_c.m 同源函数 + Model_Parameters.m
```

闭环因果链（每拍）：被控对象状态(k) → CMD_TICK(k) → 板上 EMS（推理+门控+ECMS）→
P_gen_ref(k) → 被控对象功率平衡推进 100 子步 → 状态(k+1)。固件零改动，协议帧格式不变。

## 2. Requirements 映射（Fixed Decisions → design 落点）

| Requirements Fixed Decision | design 落点 |
|---|---|
| 1 层② (a) 接受并记录 | §8.4 基线说明登记；层② 报告性输出（§8.3） |
| 2 推进器 P3 主案 / 回退 P1 | §3 子集模型 + §4 codegen 链 + §4.4 回退设计 |
| 3 lock-step / 固件零改动 | §6 harness |
| 4 4 臂矩阵 | §7 执行设计 |
| 5 两层指标 + SOC 公平性 | §8 指标设计 |
| 6 主模型不动 | §3.1 提取方法（新变体，不改 HybridShip_ModelD_DCBus.slx） |

## 3. 被控对象子集模型设计（FR-601，G-A1）

### 3.1 提取方法（单一来源由构造保证）

- 新脚本 `model/simulink_modelD_(EMS_NN_AECMS)/scripts/20_build_plantstep_model.m`
  （ModelD 仓库内，编号顺延既有脚本族）；
- 调用 `+model/build_model_c.m` **同一批构建函数**：`buildScenarioManager / 
  buildSpeedController / buildPropulsionPlant / buildLoadMeasurement / buildGensetPlant /
  buildBatteryConverter / buildUndervoltDerate / buildBusResidualMonitor /
  buildFuelConsumption / buildVoyageStop / buildDCBus / buildBusVoltageMonitor`；
  **跳过** `buildEMSControllerC`（05）与 Data_Logging（记录由 harness 承担）；
- 顶层以 **Inport `P_gen_ref_kW`** 替代 05 输出；Outport 暴露闭环接口所需信号（§3.3）；
- 求解器与主模型一致：**Fixed-step ode3 @ 0.01 s**（OQ-3 定案：不做 1s 离散化重构——
  保持与参考链数值同源，codegen 后每控制拍由 wrapper 推进 100 子步，P_gen 以 ZOH 保持）；
- 参数单一来源：`config/Model_Parameters.m`（同一份）；场景注入同 §3.5；
- **主模型 `HybridShip_ModelD_DCBus.slx` 零改动**；新变体独立命名独立文件。

### 3.2 子系统清单（进/出子集）

| 子系统 | 进子集 | 说明 |
|---|---|---|
| 01_Scenario_Manager | ✓ | v_cmd/v_current/resistance_multiplier/P_hotel/scenario_id（x_ship+t 驱动，距离/时间表双模式） |
| 02_Speed_Controller | ✓ | v_cmd 跟踪 → 推进需求 |
| 03_Propulsion_Plant | ✓ | 螺旋桨/船体动力学，v_ship/x_ship/accel 积分 |
| 04_Load_Measurement | ✓ | P_load_total 合成 |
| 05_EMS_Controller | ✗ | **由 Inport P_gen_ref_kW 替代**（7 入 1 出边界与 CMD_TICK 对应） |
| 06_Genset_Plant | ✓ | 发电机组（P_gen 消耗方 + 油耗来源） |
| 07_Battery_Converter | ✓ | 电池功率/电流 → SOC 积分 |
| 08_Undervolt_Derate | ✓ | 母线电压降额保护（临界工况行为来源之一） |
| 09_Bus_Residual_Monitor | ✓ | 功率平衡残差监测 |
| 10_Fuel_Consumption | ✓ | fuel_total_kg 积分 |
| 11_Voyage_Stop | ✓ | 任务终止条件（harness 同时以固定拍数兜底） |
| 12_Data_Logging_Doc | ✗ | 记录职责移交 harness |
| DCBus / Bus_Voltage_Monitor | ✓ | 母线电气层与电压监测 |

### 3.3 闭环接口信号

```text
Inport:  P_gen_ref_kW（f64 内部；harness 侧 f32→f64 提升）
Outport: t_s, P_load_total_kW, SOC, v_cmd_mps, v_ship_mps, acceleration_mps2,
         x_ship_m, fuel_total_kg, P_bat_kW, P_prop_kW, voyage_stop(bool)
         （前 7 项 = CMD_TICK 载荷 [t, P_load, SOC, v_cmd, v_ship, accel, x_ship]；
         fuel/P_bat/P_prop/voyage_stop 为闭环记录与终止用）
```

边界一致性声明：CMD_TICK 7 状态量 ≡ 05_EMS_Controller 的 7 输入（v1 design 已定型），
本设计的 Outport 前 7 项与其逐一对应——**接口零新增**。

### 3.4 初始条件

模型 IC 直承 `Model_Parameters.m`（`Bat_SOC_init=0.60` 等）；codegen 产物由 wrapper 在
每次运行前调用 `initialize()` 复位到模型 IC（与开环参考同起点：SOC=0.60，S20 静止起步）。

### 3.5 场景注入（OQ-5 前半：数据如何进 codegen）

- 场景外生量 = `VOYAGE_*` 基变量（v_cmd/水流时间表；S20 默认值 + voyage_002 由
  60/61 生成链注入）+ `SCN.*` 参数；
- codegen 采用**按场景烘焙**：构建脚本设置对应 `VOYAGE_*` 后生成，数值内联为常量；
  两个场景各一个生成目录（`Host/plantstep/S20/`、`Host/plantstep/voyage002/`），
  各带构建指纹（生成时间、MATLAB 版本、模型/参数文件哈希）；
- 不采用可调参数方案（减少 wrapper 复杂度与调参面）；换场景 = 重新烘焙（分钟级，脚本化）。

### 3.6 G-P1 门禁的对照数据（不触碰冻结资产）

- **轨迹对照**：`fw_reference/ref_G2_*.csv`（v1.1，冻结）提供 SOC/v_ship/x_ship/P_load
  逐拍参考与 P_gen 回放序列；
- **油耗对照**：ref_G2 CSV 无逐拍油耗列；采用 `export_summary.json` 的 `fuel_raw_kg`
  总量对照（S20=328.094641944243 / voyage_002=264.216856499727，REPRO d_fuel=0 已证）
  作为积分型终值校验；
- 如需逐拍油耗诊断，另行一次性诊断运行（新导出含 fuel 列，产物放 v3 专用目录，
  **不回写** ref_G2 与 baseline_manifest——冻结基线零改动）。

## 4. codegen 链设计（FR-602，P3 主案）

### 4.1 生成与构建

- Simulink Coder（R2026a 已确认安装）：`HybridShip_ModelD_PlantStep.slx` → ERT C 代码，
  单步接口（`model_step`），fixed-step ode3 @0.01s；
- 生成配置脚本化（`20b_codegen_plantstep.m` 或并入 20）：ERT 默认设置，禁动态内存，
  外部模式关，`Verbatim` 不引入；产物目录 `EMS01_CHIL_ECMS/Host/plantstep/<scenario>/`；
- Python wrapper：`Host/plantstep_advancer.py`——ctypes 加载编译后的步进库
  （macOS `.dylib`/`.so`，gcc 编译脚本 `Host/tools/build_plantstep.sh`），
  API：`init(scenario)`（=model initialize，复位 IC）/ `step(P_gen_kW)`（=100×子步，
  返回 k+1 全部 Outport 信号）/ `state()` / `fuel()` / `voyage_stop()`；
- 构建指纹：模型哈希 + 参数文件哈希 + MATLAB 版本 + 生成产物 sha256，写入
  `plantstep/<scenario>/build_manifest.json`（闭环运行 meta 必须引用）。

### 4.2 求解器形态论证（OQ-3 定案）

保留 ode3@0.01s 整体生成。理由：①与参考链数值同源（G-P1/G-P2 才可能紧）；
②CPU 代价可忽略（简单方程 ×100 子步 ≪ 1ms 量级）；③1s 离散化重构会引入新的
数值来源且破坏"被控对象=模型的忠实实时化"论文叙事。**否决 1s 重构。**

### 4.3 回退设计（FR-603，P1）

- 触发条件（任一）：codegen 许可/工具链不可用；子集模型结构无法生成（MF 块不兼容 ERT
  且不可修）；G-P2 反复 FAIL 且根因属结构性；
- 回退实现：`Host/plantstep_advancer_py.py`——从 `build_model_c.m` 的 MATLAB Function
  代码模板（CODE_* 常量）与 `Model_Parameters.m` 转录方程，RK3（ode3 同 Bogacki-Shampink
  3 阶）@0.01s；接口与 P3 wrapper 完全一致（harness 不感知后端）；
- 回退门禁：同一 G-P1 回放对照（阈值放至 §5.2 P1 档），并在 design 修订与实验报告中
  记录受阻证据链；
- **禁止**：静默换道；为通过 G-P1 放宽物理方程。

### 4.4a ERT 输出写出时序（T-604 执行期发现，2026-09-02 design 修订）

GRT/ERT 生成代码的根 Outport 写出发生于**子步末级阶段时刻**（ode3 第 3 级，
t = k+1−0.0025）：数值验证（S20 前段，SOC/v_ship/x_ship/P_load/P_bat 对 sim dense
轨迹的插值分数恒为 0.75）。因此：
- **wrapper `step()` 后的 Y（11 项 Outport）= t=(k+1)−0.0025 时刻输出**（Clock 亦读
  该时刻）；闭环 harness 的 CMD_TICK(k) 状态按此语义（2.5 ms 早于拍边界，
  占控制拍 0.25%，控制影响可忽略）——NFR-602 方法学材料；
- **拍边界位级真值经 `plantstep_get_states()` 直读 9 个连续状态**（X 结构：
  x_ship/v_ship/omega_prop/DriveLag/SOC/V_dc/inertia/Ki_int/fuel_int；fuel CSTATE
  为 g 口径，×1e-3 = fuel_total_kg）——shim `plantstep_get_states`（T-604 执行期增补）；
- **G-P2 比较基准修订**（§5 G-P2）：状态变量（x_ship/v_ship/SOC/fuel + 内部状态
  omega_prop/DriveLag/V_dc/inertia/Ki_int）在拍边界经 X 直读逐拍对照（阈值 1e-6
  维持，预期位级一致）；派生量（t_s/P_load/v_cmd/accel/P_bat/P_prop/voyage_stop）
  按阶段时刻语义对照 sim 同刻插值，**残差作为仪器时序特征报告、不作门禁项**
  （机制级依据：ERT 写出时刻结构性位于阶段时刻，非 codegen-vs-解释执行差异）；
  本修订为仪器实现语义澄清，阈值数值不放宽。

### 4.4 数值差异预期（§22/66 教训的落实）

- 被控对象动力学平滑（积分/限幅/查表），无 λ-argmin 型双稳结构 → 编译 vs 解释 FP 微差
  不会被放大；G-P2 预期近数值一致；
- 闭环反馈回路中 f32 序列化（CMD_TICK 载荷）是主要差异源：层① 已证 f32 状态喂入下
  REF P_gen vs Simulink 100% exact（开环），闭环中被控对象 1e-13 级漂移经 f32 取整
  可能偶发 λ 网格边界翻转 → 1 网格步 P 扰动 → 反馈回路小幅轨迹漂移。仪器级阈值按此
  物理机制设定（§5），FAIL 时按机制排查，不放宽。

## 5. 仪器门禁设计（FR-604/605，OQ-1）

度量口径统一：终点偏差（end-state）、逐拍最大绝对偏差（max|Δ|）、轨迹 RMSE、
网格一致率（P_gen 属同一离散网格步的比例）。全部机器可读（gate JSON，结构沿袭
`gate_result.json` 风格：`overall` + 逐项 `pass/metric/value/threshold`）。

### G-P1 子集保真（子集模型 vs ref_G2 回放）

- 方法：以 ref_G2 的 `P_gen_ref_kW` 逐拍序列 + 同场景 VOYAGE_*/SCN 注入驱动子集模型
  （解释执行），比较 SOC/v_ship/x_ship/P_load 逐拍 + fuel_total 终值 vs
  `export_summary.json`；
- 阈值（先验设定，FAIL 即结构性缺陷，排查不放宽）：

| 指标 | 阈值 |
|---|---|
| SOC 逐拍 max\|Δ\| | ≤ 1e-6 |
| v_ship / x_ship 逐拍 max\|Δ\| | ≤ 1e-6 m/s / m |
| P_load 逐拍 max\|Δ\| | ≤ 1e-6 kW |
| fuel_total 相对偏差（vs export_summary fuel_raw） | ≤ 1e-6 |
| 数值健康 | 无 NaN/Inf/发散 |

### G-P2 codegen 等价（codegen C vs 解释执行子集）

- 方法：同一回放输入，codegen C（wrapper）vs `sim(HybridShip_ModelD_PlantStep)`；
- 阈值：同 G-P1 表（同模型编译 vs 解释，66 教训允许 FP 微差但不承诺位级；
  被控对象无 argmin/迟滞放大机制，1e-6 预期充分）。

### 仪器级① 闭环 (b) vs 开环参考（接线正确性）

- 方法：闭环 (b) 臂轨迹 vs ref_G2（同控制律同场景）；
- 差异源 = 被控对象数值微差经 f32 + λ 网格边界的偶发翻转（§4.4），非结构性偏差；
- 阈值：

| 指标 | 阈值 |
|---|---|
| SOC_end \|Δ\| | ≤ 0.005（0.5% SOC） |
| fuel_raw 相对偏差 | ≤ 1% |
| v_ship RMSE（闭环 vs ref） | ≤ 0.01 m/s |
| P_gen 网格一致率（逐拍 |ΔP| ≤ 1 网格步占比） | ≥ 95% |
| 数值健康/事件 | 无 NaN/发散；无意外 ERROR 事件 |

### 仪器级② 闭环 (b) vs 原生 (b′)（被控对象仪器闭环复验）

- 方法：同控制律（REF C ≡ EML）、两被控对象运行时（codegen C vs 解释执行 Simulink）；
- 阈值：同仪器级①表。

## 6. 闭环 harness 设计（FR-606/607/608，OQ-4）

### 6.1 组件与复用

```text
Host/
├── ems_closedloop.py        # 闭环主循环 CLI（复用 ems_inject 协议栈：SerialSession/
│                            #   帧解析/CRC/事件环/非阻塞收向，import 复用不改协议）
├── plantstep_advancer.py    # ctypes 推进器封装（§4.1）
├── plantstep/<scenario>/    # codegen 产物 + build_manifest.json
└── tools/build_plantstep.sh # 编译步进库
```

### 6.2 lock-step 主循环（每拍）

```text
1. states = advancer.state()                    # k 拍 7 状态（f32 量化后入帧）
2. send CMD_TICK(k, 7×f32) → wait RSP_TICK(k)   # 现有协议/超时/CRC/事件语义不变
3. parse P_gen_ref_kW(=f32→f64), ê, K, cls_eff, flags, infer_us/ctrl_us
4. advancer.step(P_gen) → states(k+1), fuel(k), P_bat(k)
5. 记录行 + 事件环 flush；voyage_stop 或 tick==N-1 → 终止
```

- 节拍：lock-step（无墙钟约束）；单拍耗时（RTT+推进+协议）实测记录（NFR-602）；
- 终止：固定拍数（S20=5521 / voyage_002=4212）为主，`voyage_stop` 兜底断言一致；
- **OQ-4 定案：整跑重做**——mid-run 失败（串口/帧错误/超时）→ 本次运行作废（轨迹不采信，
  事件环归档保留），从头重跑；不做断点续跑。理由：闭环全程分钟级，续跑的状态复现与
  证据完整性成本 > 重跑成本。

### 6.3 记录格式（FR-607）

- 逐拍 CSV：`tick,t,P_load_kW,SOC,v_cmd_mps,v_ship_mps,accel_mps2,x_ship_m,
  P_gen_kW,e_hat,K,cls_eff,flags,infer_us,ctrl_us,fuel_total_kg,P_bat_kW,P_prop_kW`
  （列集 = ref_G2 对应列 + 板侧遥测 + 被控对象扩展列）；
- meta JSON：镜像 sha256（REF/NPU）、plantstep build_manifest 引用、场景、IC、
  拍耗时分布（mean/p50/p95/max）、终止原因；
- 事件环：沿用 v1 事件语义（FR-508 场景事件、协议错误事件）+ 闭环新增
  `VOYAGE_STOP` / `PLANT_STEP_ABNORMAL`（数值健康）。

### 6.4 FR-508 回退闭环语义（OQ：安全降级定义）

- 板载回退（ê=0 ≡ law=5）在闭环中 = **安全降级继续控制**：P_gen 由回退律给出，被控对象
  正常推进，回路不解体、不停机；
- 回退段以事件环界定（FALLBACK 进入/退出），闭环报告统计：回退段时长、段内油耗与
  SOC 变化、退出后恢复行为；
- 故障注入闭环化：复用 `ems_fault_inject.py` 骨架，四场景（F1 NaN / F2 STALE 停发 /
  F3 垃圾帧 / F4 RESET）在**运行中的闭环**上注入，验收 = design 状态机行为 +
  回退段被控对象轨迹连续合理（P 无跳变驱动的被控对象状态突变）。

### 6.5 selftest（AC-603）

- pty mock 板（复用 ems_fault_inject mock 模式）+ 推进器回放模式（P_gen 序列来自
  ref_G2 → 离线驱动推进器）；
- selftest 覆盖：帧配对、超时语义、事件环、记录列完整性、推进器接口、终止条件；
- **G-P1/G-P2 可无板执行**（回放模式），板会话前必须全绿。

## 7. 实验矩阵执行设计（FR-609/614，OQ-5）

### 7.1 四臂入口

| 臂 | 入口 | 说明 |
|---|---|---|
| (a) | `ems_closedloop.py --image npu --scenario S20/voyage002` | NPU 镜像 + codegen 被控对象 |
| (b) | `ems_closedloop.py --image ref --scenario ...` | REF 镜像 + **同一** codegen 被控对象 |
| (b′) | ModelD 仓库 `scripts/21_run_v3_native_arms.m --law 7` | 原生闭环仿真（复用 64 运行机制，G2 缓存可复用），导出 1s 全列含逐拍 fuel |
| (c) | `scripts/21_run_v3_native_arms.m --law 5` | 原生 law=5（64 机制支持 law 矩阵） |

- (b′)(c) 新脚本 `21_run_v3_native_arms.m`：复用 64 的运行/缓存/断言机制与 66 的
  导出格式，扩展逐拍 fuel_total 列；产物入 v3 目录（不回写 v1.1 冻结文件）；
- 各臂输出统一指标 JSON（§8）+ 轨迹 CSV。

### 7.2 板会话顺序与授权

```text
准备：NPU/REF 双镜像构建（fw_gate 7/7 + 新鲜产物指纹核对——M2 §11.5 缓存陷阱）
  ↓ SRAM 加载 REF（令牌）→ 闭环 (b) S20 → 闭环 (b) voyage_002      [仪器校准证据优先]
  ↓ SRAM 加载 NPU（令牌）→ 闭环 (a) S20 → 闭环 (a) voyage_002
  ↓ （同场可穿插 FR-508 闭环故障注入，四场景）
```

- 授权：用户在场或远程授权 + 令牌（沿袭 v1 SRAM 加载令牌机制）；双镜像会话时间预算
  预先声明；禁 Mass Erase/外部 NOR 擦写；
- 每次运行 meta 记录镜像哈希 + plantstep 指纹，确保数据可追溯可复跑。

## 8. 指标与统计设计（FR-610/611/612，OQ-2）

### 8.1 指标定义式

```text
fuel_raw      = fuel_total_kg(N-1)                       # 任务原始油耗
SOC_end       = SOC(N-1)
fuel_corr     = fuel_raw + (0.60 − SOC_end) × BatCap × K_EQ   # 64/66 同式同常数
                BatCap = 1071.36 kWh, K_EQ = 0.4774 kg/kWh    # 单一来源，禁新造公式
v_track_rmse  = sqrt(mean((v_ship − v_cmd)²))              # 全任务
ê 生效占比     = mean(K>0 ∧ flags 无回退)                   # 预测实际进入控制占比
临界工况       = SOC 触及 ASSUMED_SOC_DisDerate_Start 时段数/时长；
                Undervolt 降额事件数；FR-508 回退事件数/时长/段内 Δfuel/ΔSOC
```

### 8.2 SOC 公平性（OQ-2 定案）

- 主对比（臂 X vs Y）有效条件：`|SOC_end(X) − SOC_end(Y)| ≤ 0.02`（±2% SOC 容差带）；
- 超带时主结论标注 `SOC-UNFAIR`，以 `fuel_corr` 差值作为次级结论并显式标注
  （fuel_corr 即电荷维持修正口径，与参考链同式——不引入新折算公式）；
- 报告每臂同时给 fuel_raw 与 fuel_corr + SOC_end，禁止只报单一数字。

### 8.3 层② / 层③ 处置（FR-612）

- v3 基线说明（报告 §0 或独立小节）登记：层② S20 98.877% 边际 FAIL，用户裁决 (a)
  接受并记录（2026-09-08），层② 为参考指标不作 v3 门禁；
- 闭环 (a) 臂运行时同步采集层③ 式对照数据（board vs ref 同拍 P_gen 分布）——报告性
  输出，不作门禁。

### 8.4 报告结构（FR-611/613）

`results/ecms_phase25/closedloop/v3_closedloop_report.md`：
① 执行摘要（四臂两场景对比表：fuel_raw/fuel_corr/SOC_end/v_rmse/临界工况）；
② 仪器链（G-P1/G-P2/仪器级①②结果 + 阈值表）；
③ 闭环方法学（lock-step、单拍耗时分布、被控对象实时化叙事——论文素材）；
④ 临界工况与回退语义（FR-508 闭环代价）；
⑤ 层② (a) 裁决登记 + 层③ 报告性输出；
⑥ 图表（SOC 轨迹四臂对比 / 油耗柱状 / v_ship 跟踪 / 回退段特写）；
⑦ 复现指引（命令序列 + 指纹）。

## 9. 闭环模型清单（预测模型四形态）

| 臂 | MLP-Q（负荷预测器，int8 26.2KB） | workclf_v1（工况识别器，int8 ~21.9KB） | 预测生效机制 |
|---|---|---|---|
| (a) 板载 NPU | NPU int8 推理 | NPU int8 推理 | ê=q50 → SOC_ref 调制；workclf 逆水段→关断（G2 门控） |
| (b) 板载 REF | CPU 参考后端（同权重 deploy JSON） | 同左 | 同上（层① 已证 ≡ Simulink EML） |
| (b′) 原生 law=7 | EML 内嵌 double（v1.1 xnorm 口径） | 同左 | 同上 |
| (c) 原生 law=5 | 无 | 无 | ê≡0（SOC 自适应 ECMS 基线） |

- v3 **不引入新预测模型、不重训、不换口径**（三口径契约 v1.1 已闭合，AI_MODELING §22.3）；
- 对比变量 = 同一模型对（MLP-Q + workclf_v1）的运行形态；NN 闭环增益 = (a)/(b) vs (c)，
  NN 移植闭环代价 = (a) vs (b)；
- phase25 其他候选（TCN 族等）不进入 v3（开环层已比选，2026-08-30 用户裁决 Winner=
  MLP-Q G2）；热切换属 v2（本 spec 后继工作为编译期换型，见 §11）。

## 10. 产物与文档（FR-613，NFR-605/606）

- 产物目录：
  - `EMS01_CHIL_ECMS/Host/plantstep/<scenario>/`（codegen 产物 + manifest）；
  - `EMS01_CHIL_ECMS/Host/reference/`（G-P1/G-P2 回放与 gate JSON——命名
    `v3_gp1_*.json/csv`，不触碰 v1.1 冻结文件）；
  - `EMS01_CHIL_ECMS/Host/results/closedloop/`（闭环轨迹 CSV + meta + gate JSON）；
  - `model/.../results/ecms_phase25/closedloop/`（原生臂 (b′)(c) 轨迹 + 汇总 +
    `v3_closedloop_report.md`）；
- 脚本：ModelD 仓库 `scripts/20_build_plantstep_model.m`、`scripts/21_run_v3_native_arms.m`；
  EMS 工程 `Host/ems_closedloop.py`、`Host/plantstep_advancer.py`、
  `Host/tools/build_plantstep.sh`；
- 文档同步：新增文件登记所在目录 README（两级文档：Model 目录 README/README_Model.md
  增补被控对象子集模型条目；Host/README 增补闭环工具）；全部脚本相对路径（§20.2）；
- Session 按 §18 记录进展日志。

## 11. 后继工作（本次 Session 用户裁决，2026-09-08）

v3 完成后**立即进入编译期换型**（档 1，非运行期热切换）：按 v1 FR-511 流程
（stedgeai 重生成 → 重构建 → SRAM 加载 → 一致性复测）换入 phase25 同签名候选模型
（具体候选届时定，TCN 族为主选项）。硬规则沿袭 AI_MODELING §22.3：口径声明随交付、
三口径一致检查、int8 标定集取部署口径样本、部署后层③式对拍。该项以 v1 spec 的
FR-511 验收条目为载体（补充立项说明），不并入本 spec 任务清单。

## 12. 风险汇总

| 风险 | 缓解 |
|---|---|
| 子集提取隐藏耦合（被控对象 ↔ EMS 内部交叉引用） | **已侦察排除**：wireTopLevel 复核确认 05 边界恰为 7 入 1 出、仅 1 条输出线（P_gen→06），无其他交叉引用（2026-09-08）；残余风险由 G-P1 结构性 FAIL 兜底，escalate 不硬拆 |
| ERT 对 MATLAB Function 块不兼容 | §4.3 回退 P1（显式记录）；或 MF 块代码微调（不动物理方程） |
| VOYAGE_* 烘焙遗漏导致场景漂移 | build_manifest 记录场景变量哈希；G-P1 同场景回放兜底 |
| λ 网格边界翻转放大（闭环反馈） | §4.4 机制分析 + 仪器级阈值按机制设定；FAIL 排查不放宽 |
| 板会话时间预算 | 双镜像同场规划 + 令牌预申请（§7.2） |
| fw_gate 缓存构建陷阱 | 源码变更后核对产物指纹（M2 §11.5 教训，NFR-601） |

## Testing Strategy

验证采用五层递进，每层机器可读，前一层全绿才进入后一层：

| 层 | 验证内容 | 手段与载体 | 验收挂靠 |
|---|---|---|---|
| T1 | 被控对象子集保真 | G-P1 回放门禁（无板，ref_G2 P_gen 序列驱动，阈值 §5.2） | AC-601 |
| T2 | codegen 等价 | G-P2 门禁（codegen C vs 解释执行，同回放输入） | AC-602 |
| T3 | harness 机制正确 | selftest：pty mock 板 + 推进器回放模式，帧/超时/事件/记录/终止全覆盖 | AC-603 |
| T4 | 闭环回路正确 + 实验矩阵 | 仪器级①②门禁（闭环 (b) vs ref_G2 / vs 原生 (b′)）+ 4 臂 × 2 场景 gate JSON | AC-604 |
| T5 | 原生臂可信度 | (b′)(c) 复用 64 运行/缓存机制，REPRO 断言（d_fuel/d_SOC 超差即 fail） | AC-604/605 |

- 全部 gate 结果为机器可读 JSON（`overall` + 逐项 pass/threshold），FAIL 阻断对应阶段
  产物采信与提交（AGENTS.md §12 语义沿袭）；
- 阈值先验设定（§5），FAIL → 机制级根因排查，不放宽；数值健康（NaN/Inf/发散）
  为所有层前置条件；
- 回归保护：v1.1 冻结基线零改动原则下，G-P1/G-P2 回放本身即被控对象回归
  （重构后重跑应复现 PASS）；主模型回归仍由既有 03_run_smoke_tests.m 把关（不在 v3 范围重跑，
  但任何触及 build_model_c.m 函数族的修改必须先重跑 03 确认无回归）。

## Requirements Traceability

| FR/NFR/AC | design 落点 |
|---|---|
| FR-601 | §3.1–3.5 |
| FR-602 | §4.1–4.2 |
| FR-603 | §4.3 |
| FR-604/605 | §5（G-P1/G-P2） |
| FR-606/607 | §6.1–6.3 |
| FR-608 | §6.4 |
| FR-609/614 | §7 |
| FR-610 | §5（仪器级①②） |
| FR-611 | §8 |
| FR-612 | §8.3 |
| FR-613 | §10 |
| NFR-601 | §4.1 指纹 + §6.3 meta + §12 |
| NFR-602 | §6.2 |
| NFR-603 | §7.2 |
| NFR-604 | §3.1/§3.5/§8.1（单一来源） |
| NFR-605/606 | §10 |
| AC-601 | §3 + §5 G-P1 |
| AC-602 | §4 + §5 G-P2 |
| AC-603 | §6.5 |
| AC-604 | §5 仪器级 + §6.4 + §7 |
| AC-605 | §8.4 + §10 |
| AC-606 | §8.3 |
| AC-607 | §7.2 |