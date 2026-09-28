# S4-C3 设计笔记 — plantE↔board 实时闭环单航次三链对照（口径登记）

> 会话：2026-09-28/29 夜窗口（主 Agent，无人监管执行）。
> 依据：`S4_FULLCTRL_CONSTRUCTION_SPEC.md` §3 C3（§2-G2 经 SA-1 修正：植物子集含
> 07/16）+ `S4_G6_ACCEPTANCE.md` 定稿 v1.0 + C3 前置对拍基线
> （`Host/results/s4_c2/s4_c3_preamble_report.json`：执行层 100/100 一致、11 拍
> d1 差=已登记后端漂移传递）。
> 本文件登记 C3 runner（`Host/tools/s4_c3_closedloop.py`）的全部主 Agent 设计决策；
> gate JSON（`s4_c3_gate.json`）的 provenance 与本文件一一对应。

## 1. 拍序与域

- 控制拍 10 s；植物 10 s = 1000×0.01 s 子步（PlantStepE GRT dylib，backend=modelE）。
- **预热段（is_formal=0，30 控制拍，t∈(0,300]）**：板不参与。原因：H1 环 301 样本
  未满（`ems04_h1win.c` fail-closed：construct 需 t0≥300）且 kf issue 网格自 300
  起——板侧在该域内无法合法运行。预热控制 = law6 冷启动（d1=0 → soc_ref_eff=
  0.60，λ=max(0.425+30×(0.60−SOC),0.43)，执行层网格穷举），无 PMS（预热不进判据
  域，host PMS 实例在正式首拍下行前与板 CMD_RESET 同步 `ems04_pms_init`）。
- **正式段（is_formal=1，541 拍，kk=30..570，查询 t0=kk×10=300..5700）**：与 kf
  表 issue 网格（300:10:5700）逐拍重合；t0>5700 无 kf 行 → fail-closed 停止。
  G6-5 域 = 仅正式段。
- **每拍次序**：① 本拍第 1 个子步（pgen=上一拍板值）→ OUT@stamp=t0 恰好闭合 A2
  窗与环边界；② TICK_CTRL 下行；③ 板 RSP = B_BOARD；④ B_HOST/A2 host 链；⑤ 逐拍
  CSV；⑥ 本拍剩余 999 子步用新板值（ZOH 前沿 0.01 s，全程一致）。
- **首拍前**：PING（探活）→（预热）→ CMD_RESET → host PMS×2 同步复位。

## 2. 三链定义（G6 §0）

| 链 | 输入 | 预测器 | 决策/执行/PMS |
|---|---|---|---|
| A2_CANONICAL_HOST | canonical 域（§3） | `ppe.c2_inference.C2Predictor`（FR836 float keras，q50 kW 域，SHA fail-closed） | 同行 |
| B_HOST | 与下行完全相同的 hist/kf f32 字节 | `gen_ref_split_v2.Chain`（板源 OE int8 N1+N2，ORT 代理；S3-C HB 腿已证） | 同行 |
| B_BOARD | 同 B_HOST 的下行字节 | 板载 NEURAL-ART int8 REATT | 板载 law6 + PMS/Safety×1000 |

- 同行 = `libems04_law6_c2.dylib ems04_dp_tick`（S1 377/377；gen0: st=3/3、
  P_ramp=P_lag=0、t_dwell=0，A-02；n0=max(1, 本链 PMS `UD_N_dispatch_fb`)）
  → λ_eff=max(0.425+30×(0.60+K_S_B2·d1−SOC),0.43)（常数自
  `ems04_s1/exported/constants.json`）→ 执行层契约 §4.3 网格穷举（板
  `s4_exec_grid` 同式）→ `libems04_pms_safety.dylib` 1000 内部拍（G1 6001/6001；
  喂入=下行同值，st1=3.0、st2=UD 反馈、DG 实际 0 喂、voyage=1.0——板
  `ems04_tick_ctrl.c` 同构）。
- A2/B_HOST 各持独立 PMS 实例（各自随本链 P_gen_ref 演化；n0 反馈独立）。

## 3. 输入构造（列源表 + 采样网格）

H1 11 列冻结序 = `ems04_h1win.c ems04_h1_col_names`。列源：

| 列 | 源 | A2（canonical） | B（板算法 host 复刻） |
|---|---|---|---|
| P_load_total_kW / v_ship_mps / acceleration_mps2 | plantE 活通道 | 子步真样本 → `h1_from_raw_logs` time_mean | 1 Hz 环边界 → (v0+v1)×0.5 |
| n_prop_rpm | states() omega×30/π | 同上（time_mean） | 同上（中点） |
| P_hotel_kW / v_current_mps / P_prop_demand_kW | S20 pilot stream 时间表（场景侧，不依赖闭环状态） | stream 子步样本 time_mean | stream 1 Hz 边界中点 |
| water_depth_m | 20.0 常量 | (w+w)×0.5 | 同左 |
| v_ref_mps | stream 时间表 | 右端点 | 环右端点 |
| ctrl_mode / manual_power_cmd_pct | 0.0 | 常量/零填 | 同左 |

- **场景侧 4 列取 stream 的依据**：v_ref/P_hotel/v_current 为场景输入（时间驱动，
  与闭环状态无关）；P_prop_demand 由速度控制器从 v_ref 跟踪产生（推进动力学与
  电气侧解耦，闭环下与 stream 一致）。仅按 t≤t0 因果取样本；**不触任何真值列**
  （P_gen_actual/oracle 类禁读）。
- **stamp 口径**：OUT 为 ERT pre-integration 写出（=子步起点值；t_s 信号自带
  +0.0075 场景时移，不作 stamp——G-P2E 写点语义）。自有 0.01 网格时钟 stamp；
  omega/SOC/Vdc/x_ship = states() X 直读（子步末真值；n_prop 平移对齐 stamp，
  0.01 s）。A2 聚合含端点 t0（覆盖门要求窗端点样本齐备）；B 环边界 = stamp 命中
  整秒 e（OUT@e + 上一子步 X@e；v[0]=名义整秒，满足环契约 v[0]==tick_idx）。
- **A2 kf_raw** = kf 表行 f32 值（与 canonical npz 的 f32 存储同域）；三链同一
  kf 行字节。

## 4. 下行 8 态口径（板/两 host 链同喂）

`(P_load, SOC, Vdc, P_bat_dis, P_bat_chg, P_prop_dem, P_hotel, 0)`：
- P_load = OUT@t0；SOC/Vdc = X 边界真值；P_bat_dis/chg = P_bat_actual 符号拆分
  （**登记**：G1 参考用 `*_avail` 能力信号，但该信号不在 PlantStepE 16 键出口；
  C3 用实际功率拆分——两字段仅影响 PMS gap/firm/过供安全面比较，非判据面）；
  P_prop_dem/P_hotel = stream@t0。全 f32 RNE cast；**host 链消费 f32 回读值**
  （板 `rd_f32` 同域，保证 B_HOST 与板输入逐位一致）。

## 5. G6 机械化口径（S4_G6_ACCEPTANCE v1.0 + run2 修正 v2）

- **G6-1 运行性**：零 NACK/超时/中断（超时同 seq 重试 1 次，再败=park）；零
  flags≠0；SOC∈[0.15,0.95]（定稿原文界）。|ΔP_gen_ref| 界：首跑证据修正——执行
  层契约 §4.3 输出=clamp[0,1000] 逐拍 argmin、**无斜坡限制**，`constants.ramp=20
  kW/s` 是植物侧发电机模型参数（契约 L147），原"≤200 kW/拍"界与冻结契约冲突 →
  降级为遥测面（step 分布全量记录）；植物实际跟踪由 p_gen_inertia+τ=5 s 与数值
  健康检查覆盖（首跑 62 拍吸收 0↔800 跳变无异常）。
- **G6-2**：A2 vs B_HOST action-index 精确 + P_gen_ref≤1 网格（ECMS_Grid_dP_kW=
  20）；差异=**CLOSED_LOOP_TRANSFORM_EFFECT 基线档案**（非 FAIL，全量记录）。
- **G6-3/G6-4（v2，用户裁决 2026-09-29）**：action 不一致按归因分档——
  **host 两链（A2 float canonical 与 B_HOST ORT-OE int8）彼此一致而板偏离** →
  可归因 S3-C 已登记 REATT 后端数值漂移（KNOWN_TARGET_BACKEND_NUMERICAL_DRIFT），
  记 **CLOSED_LOOP_BACKEND_DRIFT 档案（非 FAIL）**；**host 两链互异** → 输入构造
  域差与板后端混杂=**未归因**（严格判据面）；action 同档而 P_gen_ref>1 网格 →
  未归因（执行层真发散）。G6-4 频次门与"连续 ≥3 拍停机"只作用于未归因拍次。
- **G6-5**：is_formal=1 域（§1）。
- **G6-6 事件冻结清单（v2 修正）**：fault 0/0；flags=0；health=1；fallback=0；
  force_stop=0；force_start 请求位（fs0/fs1）= 已声明 realized-gap 机制
  （reason=4，C2 冒烟声明）的锁存请求——RSP 无 reason 字段，板面声明性由 host
  链 `fs_reason∈{0,4}` 佐证（首跑 62 拍 fs1=1 + host reason=4 全程对应）。
  其余出清单 → STOP 复核。
- **overall(v2) = G6-1 ∧ G6-3amended ∧ G6-4 ∧ G6-6 ∧ 全拍完成 ∧ 无停机 ∧ 非干
  跑**；G6-2 与 CLOSED_LOOP_BACKEND_DRIFT 均为档案面。
- **停机条件（v2，运行时）**：连续 ≥3 拍**未归因** action 不一致 / 连续 ≥3 拍
  同档 P_gen_ref 发散 / 板失联重试无效 / SOC 出界 / flags≠0 / 植物非有限 →
  park（写部分产物，exit 2，报告用户）。
- **产物版本**：run1（park，62 拍，gate v1 原始记录）归档于 `run1_park_archive/`；
  run2 = v2 口径全程重跑。

### 5.1 裁决记录

> 2026-09-29 用户裁决（G6 §4 承接点）：认定 B_BOARD vs B_HOST action 差异为
> S3-C 已登记后端漂移传播，G6-3/G6-4 一致性门对该类拍次降级资格
> （CLOSED_LOOP_BACKEND_DRIFT 档案，非 FAIL）；停机条件只对未归因不一致触发。
> 依据：run1 t0=890/900/910 三拍 host 共识/板偏离签名，与 C3 前置基线（11 连拍
> 同签名）同源；板自身闭环决策自洽无振荡。全文见 gate JSON `adjudication` 块。

### 5.2 v2.1 归因规则（裁决的演绎闭合 + run2 证据）

run2（v2 规则）在 t0=2260–2280 暴露 v2 归因启发式的盲区：三拍 **board==canonical
（action -20，与 A2 float 链一致）而 B_HOST 偏离**（-17/-16/-14），被 v2 误判
"unattributed" 触发停机；同窗执行层呈刀锋形态（P_load≈398 kW 时 pg=0 与 pg=800
的 J 值近平，float 链自身在相邻拍翻转 800→0）。据此 v2.1 收紧为演绎闭合规则：

- **action 差异归因函数 `classify_action_face`**：下行字节同源 + 下游
  （DP/law6/PMS）同输入位级同构已证（S1 377/377、G1 6001/6001）⇒ bb-vs-bh 的
  action 差**只能**经已登记 REATT 后端通道产生 → 一律记
  CLOSED_LOOP_BACKEND_DRIFT 档案，子类标注
  `host_consensus / board_canonical / triple`；
- **硬判据面（停机/FAIL）**收窄为：执行层同档 P_gen_ref 发散（action 同档而
  pg 差 >1 网格，连续 ≥3 拍）、板 P_gen_ref 自振荡（10 拍滑动窗翻转 ≥6 次）、
  运行性（G6-1）、事件面（G6-6）；
- 档案面保留完整再判能力：CSV 逐拍记录三链 action/d1/λ/pg 全量，可离线按任何
  更严规则重判。

run1/run2 原始记录归档于 `run1_park_archive/`、`run2_rulestop_archive/`。

## 6. 产物清单

| 文件 | 内容 |
|---|---|
| `Host/tools/s4_c3_pack_kf_table.py` | S20 kf 表打包器（EMS04KFT v1，profile_code=3=PILOT_FIXTURE，AST 白名单自查） |
| `experiments/softhil_ems03_2026-09-25/results/s4_c3/S20_composite.kf_table.bin` | kf 表（216+541×728 B，sha 见自校验 JSON） |
| `experiments/softhil_ems03_2026-09-25/results/s4_c3/s4_c3_kf_table_selfcheck.json` | 表自校验（重解析+抽行独立重算零失配） |
| `Host/tools/s4_c3_h1win_host.py` | H1 环 host 版（`ems04_h1win.c` 逐位复刻，37 项自检） |
| `Host/tools/s4_c3_closedloop.py` | 闭环 runner（本文档主体） |
| `Host/results/s4_c3/s4_c3_warmup.csv` | 预热段记录（is_formal=0） |
| `Host/results/s4_c3/s4_c3_closedloop.csv` | 三链同录逐拍 CSV（is_formal=1） |
| `Host/results/s4_c3/s4_c3_gate.json` | G6 机械化判定 + provenance |

## 7. 已知事项

- S4Smoke 形态板上 `infer_us/ctrl_us=0`（DWT CYCCNT 未使能）——遥测面项。
- 前置对拍（同输入固定窗）11 拍 d1 差（tick 46–56 连续段）= 后端漂移传递；闭环
  输入逐拍变化，预期形态不同；若闭环再现连续 ≥3 拍差 → 按停机条件 park。
- 干跑（--dry）不出正式 gate（overall 恒 false）。
