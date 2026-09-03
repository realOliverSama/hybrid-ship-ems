# Chain A 集成规格与勘误登记 — T-004 船体浅水 + T-005 螺旋桨 w(H/T)

交付人：Phase 4-A2 链 A 执行工人。接收人：主 Agent（builder 集成与最终验收）。
依据：链 A 任务书 + `.specs/phase4-a2-model-e-construction/design.md` §4.1/§4.2（OQ-4、D-2/D-3/D-8）+
参数唯一事实源 `config/ModelE_Physics_Parameters.m`（worktree commit 95038407）。
本文件描述 builder 集成规格与任务书锚点勘误登记；数值推导与 provenance 见参数文件注释与证据链文档。

---

## 1. Ship_Hull_Dynamics_E（替代 legacy Ship_Hull_Dynamics / `CODE_HULL`）

- 块源码工厂：`scripts/+model/code_hull_e.m` → `model.code_hull_e()`
- 块函数签名（输入端口顺序，1~6 为信号、7~17 为 Parameter）：

| 端口 | 名称 | 来源 |
|---|---|---|
| 1 | `V_ship` | v_ship 积分器输出 |
| 2 | `F_prop_eff` | Propeller_Hydrodynamics 输出 1 |
| 3 | `V_current` | 子系统输入 v_current_mps |
| 4 | `F_env` | 常量 `F_env_const`（不变） |
| 5 | `resistance_multiplier` | 子系统输入（默认 1.0，通用不确定度旋钮，浅滩语义退役，D-7） |
| 6 | `water_depth_m` | T-003 Scenario_Manager 水深输出（Plant 第 5 输入口已存在） |
| 7~17 | 见下方参数清单 | ModelE_Physics_Parameters.m |

- **setMFCode 参数清单**（按签名顺序，全部存在于参数事实源）：

```matlab
setMFCode(fn, model.code_hull_e(), {'Hull_K_R','Ship_Mass','Hull_Lpp_m','Hull_T_m', ...
    'nu_water','Hull_k_form','Shallow_c1','Shallow_c2','Shallow_c3', ...
    'Shallow_HT_deep_limit','Shallow_HT_min_valid'});
```

- 注意：Δk = 0.644·(H/T)^(−1.72) 的 **0.644 与 −1.72 为块内字面量**（块内注释来源
  Millward[4]/Zhang[1]，任务书钦定写法）；config 中的 `Shallow_deltak_coef` /
  `Shallow_deltak_exp` **不作为块参数传入**（builder 勿加）。
- 输出端口（顺序）：
  1. `dVdt` → v_ship 积分器 + acceleration_mps2（legacy 用法不变）
  2. `R_total_N` → 既有 R_hull_N 输出改接此处（= R_deep + R_shallow，FR-413）
  3. `R_shallow_N` → **新增输出**（logging）
  4. `HT_ratio` → **新增输出**；同时是 Wake_vs_HT 的唯一输入（H/T 单一事实源，全模型只允许此处计算）
  5. `domain_warning` → **新增输出**（logging；H/T<1.2 clamp 报警，NFR-411）
- 语义要点：`HT_ratio` 输出为**原始比值** water_depth_m/Hull_T_m（不做 clamp）；
  H/T<1.2 时 Δr 内部按 clamp=1.2 计算并置 domain_warning=1，下游 Wake_vs_HT 自带同款 clamp。
- 已知离散化行为（裁决 C3 登记）：Δr 在 H/T=3.0 处取公式值 0.1234（严格 `>` 判深水），
  H/T→3.0⁺ 跳变为 0；边界跳变属已知行为，归档证据链文档（对 G2 单调性判据无实质影响：
  FR-415 六档最高档实际 H/T=2.9982，严格落在公式域内）。

## 2. Wake_vs_HT（新模块）

- 块源码工厂：`scripts/+model/code_wake_ht_e.m` → `model.code_wake_ht_e()`
- 输入：1 个信号输入 `HT_ratio` ← **Ship_Hull_Dynamics_E 输出 4**（单一事实源，禁止两处各自计算）；
- 输出：`w` → **Propeller_Hydrodynamics 第 4 输入口**（w_signal）。
- **setMFCode 参数清单**（按签名顺序）：

```matlab
setMFCode(fn, model.code_wake_ht_e(), {'Wake_w_deep','Wake_HT_nodes','Wake_w_Zhang', ...
    'Wake_HT_ref','Wake_w_ref'});
```

- 语义：w = w_deep + Δw_Zhang(H/T)；linear 插值；查询域 clamp [1.2, 2.7]；
  H/T > 2.7 显式分支 Δw=0（深水渐近）；H/T < 1.2 clamp 到首节点（与船体模块一致）。
- **transferred literature surrogate 声明**：Δw 取自 Zhang 2023 双桨表（标定族 C_B=0.86 ≠
  本船 0.60），只借相对变化量；证据链与论文**不得表述为本船验证伴流场**；
  深水锚 w_deep=0.10 由 Harvald/Molland 回归独立确定（D-3）。
- OQ-4 提示：w(H/T) **不要求单调**（Zhang 原数据非单调）；单调性判据只作用于
  R_total(H/T)/P_required(H/T)（§5.3 G2）。

## 3. Propeller_Hydrodynamics 改造（T-005 核心变更）

- 块源码工厂：`scripts/+model/code_propeller_e.m` → `model.code_propeller_e()`（替换 `CODE_PROPELLER`）。
- 签名变化：第 4 参数 `wake_fraction`（Parameter）→ **`w_signal`（输入信号）**；
  **setMFCode 参数清单变为**：

```matlab
setMFCode(fn, model.code_propeller_e(), {'K_Fw','K_Fv','K_Qw','K_Qv'});
```

  （`wake_fraction` 从参数清单删除；参数文件中 `wake_fraction` 别名行已标
  DEPRECATED→signal，builder 不得再作常量引用。）
- 输入端口顺序：1 `n_prop_rpm`（radps2rpm 输出）、2 `V_ship_mps`（v_ship）、
  3 `V_current_mps`、4 `w_signal` ← Wake_vs_HT 输出。
- 其余公式与 legacy `PropellerHydroSimple` 完全同构
  （V_A=(1−w)·V_rel；F=K_Fw·ω|ω|−K_Fv·V_A|V_A|；Q 同理；P_hydro=Q·ω/1000）。
- 四 K 为 w_deep=0.10 重标定值（D-8，参数文件已落盘）：K_Fw=128.107、K_Fv=1099.022、
  K_Qw=37.410、K_Qv=160.470；设计点实测复原 F≈57005.5 N、Q≈20808.7 N·m（±0.5% 锚内）。
- 螺旋桨 D、K_T/K_Q、P/D 保持 **INTENTIONALLY UNRESOLVED**（FR-417），不得填入借用值。

## 4. 03_Propulsion_Plant 新增输出端口建议（logging）

| 新增输出端口 | 来源 | 用途 |
|---|---|---|
| `R_shallow_N` | Ship_Hull_Dynamics_E 输出 3 | 浅水附加阻力记录（FR-413 证据） |
| `HT_ratio` | Ship_Hull_Dynamics_E 输出 4 | H/T 记录（G2 单调性判据输入） |
| `domain_warning` | Ship_Hull_Dynamics_E 输出 5 | 域外报警记录（NFR-411） |

- 既有 `R_hull_N` 输出改接船体块输出 2（R_total_N）；`acceleration_mps2` 仍接输出 1（dVdt）；
  `water_depth_m` 子系统输入口已由 T-003 就位，直接进船体块第 6 入口。
- 建议同步登记 Logging selector / To Workspace 变量名，供 T-412 Gate 与 AC-409 检查使用。

## 5. 效率链审计（FR-416；主 Agent 转入证据链文档）

Elec_Eff = 0.92 = η_inv(0.98) × η_motor(0.96) × η_shaft(0.98) ≈ 0.9219 → 冻结 0.92；
**不含齿轮箱**：Gear_Eff = 0.96 独立存在于转矩链（D-1 裁决保持 0.96；顾问 0.97 备查不入链）。

推进能量路径逐段审计（每因子恰好出现一次，无重复计损）：

| 能量路径段 | 实现（buildPropulsionPlant 现状） | 效率因子 |
|---|---|---|
| 电功率 → 机械功率 | Gain `ElecEff`（× Elec_Eff） | η_inv、η_motor、η_shaft 三因子合计出现一次 |
| 机械功率 → 电机转矩 | T_req = P_mech·1000 / max(\|ω_motor\|, ω_min) | 无（纯单位换算） |
| 电机转矩 → 桨转矩 | Gain `Gearbox`（× Gear_Ratio·Gear_Eff） | Gear_Eff 唯一出现点（转矩链） |
| 轴系 | Shaft_B = 0 线性阻尼 | 无稳态效率因子（动态项非损耗效率） |
| 设计点标定 | η_prop = 0.60 只用于 K 系数反演 | 运行时不出现在功率路径 |

结论：η_inv、η_motor、η_shaft 经 Elec_Eff 在电功率→机械功率 Gain 处各出现一次；
齿轮箱 Gear_Eff 在转矩链 Gain（Gear_Ratio·Gear_Eff）处出现一次；轴系无额外效率因子——
推进能量路径上每个效率因子恰好出现一次、无重复计损，满足 FR-416 效率链审计要求。

## 6. 任务书锚点勘误登记（主 Agent 裁决 2026-09-04，供证据链归档）

链 A 工人开工探针（MATLAB R2026a，以实际块源码字符串运行）发现任务书 3 处自相矛盾，
升级主 Agent 后裁决如下；单元测试按裁决后锚点实施：

| # | 任务书原文 | 裁决 | 依据 |
|---|---|---|---|
| C1 | Δr(H/T=1.2) ≈ 0.5005 ±2% | **勘误为 ≈0.5646 ±2%**（实测 0.564551） | 钦定公式与已批准 design §4.1 逐字一致；H/T=2.0 锚点交叉验证公式读法正确；主 Agent 复核确认原值为手算滑差（Δk(1.2)=0.47062 此前误用） |
| C1' | Δr(H/T=2.0) ≈ 0.2375 ±2% | **精化为 ≈0.2364 ±2%**（实测 0.236560；原值亦在容差内，保留有效） | 公式真值精化 |
| C3 | "H/T=3.0 → R_shallow=0" | **改为 Δr(3.0)≈0.123378 ±2%（域内计算）；深水断言移至 H/T=3.5 → R_shallow=0**（代码保持严格 `>` 不变） | 三处 spec 字面一致（design §4.1 "1.2≤H/T≤3→下式"、FR-415、任务书公式段）；3.0 处 0.123→0 边界跳变登记为已知离散化行为（见 §1） |
| C2 | "w=0.15 → V_A 增大 → F 减小" | **按公式实际方向断言**：w 0.10→0.15 ⇒ V_A 5.0926→4.8096（减小）、F 57005→60084（增大）；断言 F(w=0.15) > F(w=0.10) 即"w 信号生效" | 钦定公式（legacy 同构）只有一种可实现行为；任务书方向句为文字错误（主 Agent 确认），无需重设计 |

---

## 附：验证命令（从工作区根执行）

```bash
/Applications/MATLAB_R2026a.app/bin/matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_hull_shallow.m')"
/Applications/MATLAB_R2026a.app/bin/matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_propeller_e.m')"
```

两条脚本均为纯 MATLAB（无 Simulink 依赖），路径由 mfilename 推导 + pwd 兜底（NFR-406），
临时块源码写入 tempdir 并在结束时清理。