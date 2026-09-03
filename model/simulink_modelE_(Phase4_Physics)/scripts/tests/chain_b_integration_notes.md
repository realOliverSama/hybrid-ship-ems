# Chain B 集成规格（T-006 双发电机组/状态机 + T-007 per-genset 燃油 → Model E builder）

交付人：Phase 4-A2 链 B 执行工人。本文是给主 Agent 的 builder 集成规格。
三个 block 源码由 `+model` 包函数以字符串返回（经 `setMFCode` 写入 MATLAB Function 块）：

| 源码函数 | block 内函数名 | 角色 |
|---|---|---|
| `model.code_genset_unit_e()` | `GensetUnitE` | 单机组启停状态机 + 功率链（DG1/DG2 各一块实例，同一代码） |
| `model.code_commitment_adapter_e()` | `LegacyCommitmentAdapter` | 占位组合适配器（显式 PLACEHOLDER，FR-421） |
| `model.code_fuel_e()` | `FuelModelE` | per-genset 燃油（FR-420，含本地函数 `unit_fuel_rate`） |

## 1. `06_Genset_System`（建议替换 legacy `06_Genset_Average_Plant`）

### 1.1 端口建议

输入（In1..In5）：

| 端口 | 接线 | 说明 |
|---|---|---|
| `DG1_cmd_start` | ← adapter `DG1_enable` | enable 即启动命令（0/1） |
| `DG1_P_ref_kW` | ← adapter `P_DG1_ref_kW` | 单机功率参考 |
| `DG2_cmd_start` | ← adapter `DG2_enable` | 同上 |
| `DG2_P_ref_kW` | ← adapter `P_DG2_ref_kW` | 同上 |
| `DG_cmd_stop` | 默认 Constant `[0;0]`（2 维向量 [DG1;DG2]） | cmd_stop 可合并为单端口组；T-412 E06 aborted-start 直接注入用 |

cmd_stop 接线二选一（集成决策，均不改被控对象语义，由主 Agent 定）：
- **A（默认）**：`DG_cmd_stop` 独立端口 + 默认 Constant 0——状态机测试台/E06 直接注入；
- **B**：接线侧令 `cmd_stop = 1 − enable`（adapter 撤 enable 即请求停机；STARTING/SYNC 期间
  仍不可中断，进入 ON 后一拍内执行）。

输出（Out1..Out9）：

| 端口 | 来源 | 说明 |
|---|---|---|
| `P_DG1_kW` | GensetUnitE#1 `P_actual_kW` | DG1 实发 |
| `state_DG1` | GensetUnitE#1 `state` | 0=OFF 1=STARTING 2=SYNCHRONIZING 3=ON 4=COOLDOWN |
| `below_min_DG1` | GensetUnitE#1 `below_min_flag` | 仅 ON 态有意义（ref<P_min 登记） |
| `start_count_DG1` | GensetUnitE#1 `start_count` | 完成启动次数 |
| `P_DG2_kW` / `state_DG2` / `below_min_DG2` / `start_count_DG2` | GensetUnitE#2 同上 | |
| `P_gen_actual_kW` | Sum(P1,P2) | 送 13_DC_Bus / 09_Bus_Residual_Monitor |

### 1.2 内部结构

- 两个 `GensetUnitE` MATLAB Function 块（**同一代码字符串两个块实例**，Simulink 中
  persistent 状态按块实例天然隔离，无需改代码）；
- `setMFCode(fn, model.code_genset_unit_e(), {...})` 参数名单（全部标量 Parameter，
  均已存在于 base workspace，来自 config/ModelE_Physics_Parameters.m §5/§9）：
  `{'DG_P_rated_kW','DG_P_min_kW','DG_ramp_kW_per_s','DG_tau_s','N_ticks_starting','N_ticks_sync','N_ticks_cooldown','Pgen_init'}`；
- 输入端口：`cmd_start`、`cmd_stop`、`P_ref_kW`（3 入）；输出 4 出（见上表）；
- `P_gen_actual_kW` = P1+P2（Sum 块）。

### 1.2.1 状态机输出约定（重要）

- `state` 输出为**本拍转移后的状态**（组合输出，无传输延迟）；
- 非 ON 态 `P_actual_kW` 恒 0；ON 态按功率链输出；
- ON→COOLDOWN 的停机拍（cmd_stop 或 pending_stop 命中）`P_actual=0`——断路器分闸阶跃，
  功率缺口由电池下垂/母线自然响应（与启动缺口同一设计意图）；
- `below_min_flag` 仅 ON 态置位，其余状态恒 0。

### 1.3 采样时间说明

- MATLAB Function 块继承模型固定步 **0.01 s**；每步调用一次 = 1 tick；
- 转移判据全部为整数 tick 计数（N_ticks_starting=2500 / N_ticks_sync=500 /
  N_ticks_cooldown=3000），无浮点时间比较（OQ-6，确定性、codegen-friendly）；
- **dt=0.01 硬编码**于爬坡（±ramp×0.01）与惯性更新（×0.01/τ）：若模型固定步长变更，
  该 dt 与 N_ticks_* 必须一并重推，否则时序物理值漂移。

## 2. `Legacy_Commitment_Adapter` 接线

- EMS 顶层聚合输出 `P_gen_ref_kW` → adapter In1；
- `force_mode` / `force_P1_kW` / `force_P2_kW` ← Constant（默认 0 → auto 模式；
  scenario 脚本化强制组合测试时改 Constant 值）；
- adapter 输出 4 线 → `06_Genset_System` 对应输入；
- 块内已带**永久 PLACEHOLDER 注释**（FR-421 原文）；建议块名 `Legacy_Commitment_Adapter`，
  并在模型内加显式标注（如块注释/背景文本 "PLACEHOLDER: 不得用于经济性结论"）；
- 语义锚点：auto 分支的 500 为字面**单机额定**（冻结事实源 DG_P_rated_kW=500，config §5）；
  上游 EMS 已 clamp `P_gen_ref_kW ≤ Gen_P_max=1000`，故 auto 分支 DG2_ref≤500 恒成立；
- force 模式 enable 由对应 force 功率 >0 派生（=0 = 该机组不投入）；
- adapter 不虚构 STARTING/SYNC 期间功率——只输出 enable/参考值，启动缺口由电池/母线响应。

## 3. `10_Fuel_Consumption` 改造

- `FuelModelE` 块输入：`P1_kW`、`state1`、`P2_kW`、`state2`（来自 06_Genset_System 输出）；
- 输出三个瞬时率 [kg/s]：`fuel_rate_1_kg_s`、`fuel_rate_2_kg_s`、`fuel_rate_total_kg_s`；
- 后接**三个 Integrator**（InitialCondition=0，无输出限幅）：
  rate_1→`fuel_1_kg`、rate_2→`fuel_2_kg`、total→`fuel_total_kg`（kg；rate 已是 kg/s）；
- `setMFCode(fn, model.code_fuel_e(), {'DG_BSFC_P_kW','DG_BSFC_gkWh','DG_C_start_kg','DG_t_starting_s','DG_t_sync_s'})`；
  两个向量参数建议声明尺寸 5（Stateflow.Data `Props.Array.Size='5'`），其余标量；
- legacy 10 号子系统的 `ASSUMED_BSFC_Breakpoints/Table` + 聚合 `P_gen_actual_kW` 单机链路
  **整体替换**为 per-genset 口径（FR-420）；`ASSUMED_BSFC_*` 参数定义本身仍被 EMS 内部
  ECMS 引用，不删除。

## 4. known-limitation 草稿（供 gate/report 登记）

1. **COOLDOWN 怠速油耗未计**：state=4 时 fuel=0（无数据支撑，registered surrogate）；
2. **ON 态 P<P_min 爬坡过渡段**：BSFC 按 clamp 到 125 节点值查表（不外推、非零；
   过渡段怠速油耗未计，与 COOLDOWN 同类局限）；`below_min_flag` 登记过渡；
3. **启动燃油均摊**：STARTING+SYNC 30 s 内 0.02 kg/s 均匀计量（30 s 积分恰为 C_start=0.6 kg），
   只算一次（方案 A event-level surrogate），不与 ON 态 BSFC 双算；
4. **停机阶跃**：ON→COOLDOWN 停机拍 P_actual 直接归零（断路器分闸），缺口由电池下垂/母线响应；
5. **无 min up/down**（D-4）：COOLDOWN 物理驻留 3000 tick（期间 cmd_start 忽略）是机组
   OFF 前的唯一间隔约束；unit commitment 属 B 包 PMS v2（T-415/T-416）；
6. **adapter 为 PLACEHOLDER（FR-421）**：仅允许 plumbing、变体兼容、组件测试、T-412 Gate
   使用，不得用于任何正式 EMS 经济性/节油结论。

## 5. 链 B 验证状态（纯 MATLAB，无 Simulink）

- `scripts/tests/test_genset_statemachine.m`：tick 精确转移（±1 tick）、启动期 P=0、
  aborted-start 语义、clamp/below_min、爬坡斜率 20±5%、COOLDOWN 忽略启动、start_count、
  DG2 启动缺口不虚构、adapter auto/force 规则 —— ALL PASS；
- `scripts/tests/test_fuel_model.m`：BSFC 节点值精确、PCHIP 与 interp1 参考逐位一致、
  启动燃油 0.6±0.01 kg、域守卫（P=100→BSFC(125)，不外推）、经济性可区分（≈20.8%）—— ALL PASS；
- Simulink 集成后的 build/compile/smoke 由主 Agent builder 流程负责（本链只交付
  block 源码字符串与纯 MATLAB 单测）。