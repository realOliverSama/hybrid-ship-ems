# Phase 4-B 三臂对比报告（R0 / R5 / P2）

> spec：`.specs/phase4-b-pms-safety`（FR-452/453/454）｜Gate：`scripts/43_run_modelE_control_gate.m`
> 数据源：`model/simulink_modelE_(Phase4_Physics)/results/gate_control_result.json`（overall=true，2026-09-04）
> **判据口径（FR-452 修正）**：R0/R5 的燃油优劣**不参与 B 包 PASS/FAIL**（仅行为上下文）；
> P2 独承 behavioral-quality Gate（C1~C7）。本报告为 T-014 用户评审材料。

## 1. 三臂定义（臂间唯一差异 = aggregate generation demand source；Safety 层对全部控制臂恒在）

| 臂 | 配置 | 角色 |
|---|---|---|
| R0 | EMS_CTRL_LAW=0（旧 Baseline law）+ PMS_SOURCE_MODE=EMS_AGGREGATE | historical context（旧 law=0 需求律过共享链） |
| R5 | EMS_CTRL_LAW=5（SOC-Adaptive ECMS）+ EMS_AGGREGATE | advanced integration（先进 EMS 过共享链） |
| P2 | PMS_SOURCE_MODE=BASELINE_POLICY（law 值无关） | **唯一 conventional economic baseline**（论文经济性对照只允许用 P2——FR-449 防线） |

## 2. S01 静水巡航（900 s 级航次）三臂对照

| 指标 | R0 | R5 | P2 |
|---|---|---|---|
| fuel_total_kg | 55.05 | 58.24 | **56.95** |
| DG2 online time [s] | 804 | 804 | 804 |
| DG2 starts | 1 | 1 | 1 |
| SOC_end | 0.588 | 0.601 | 0.591 |
| unserved 能量 [kWh]（诊断） | 29.6 | 56.2 | 7.6 |

**共同行为**：三臂均在加速段负荷瞬态越 425 kW（P_commit LPF 穿越 t≈37 s）触发一次
NORMAL_HIGH_LOAD 启动（reason=1，t_start_hold 饱和 3000 tick），滞回带 [350,425] 内驻留不停——
设计行为（双机低负荷 dwell 指标如实记录，非缺陷）。

## 3. 全矩阵关键指标（fuel_total_kg / SOC_end）

| 工况 | R0 | R5 | P2 | 说明 |
|---|---|---|---|---|
| S20 复合航次 | 402.46 | 412.45 | **408.26** | P2/S20 SOC_end=0.631（C3 行为带 ±0.05 ✓） |
| S01 静水 | 55.05 | 58.24 | **56.95** | |
| S02 顺水 | 36.73 | 39.98 | **38.55** | 双机 dwell 288 s（三臂同） |
| S03 逆水 | 72.30 | 74.90 | **73.18** | |
| S31/34/36 浅水 | 34.7~39.8 | 38.4~42.6 | **36.0~40.5** | 浅水阻力单调性由 40 号 G2 承担 |
| M1/M2/M3（仅 P2） | — | — | 24.7 / 21.2 / 52.1 | M3 全链 1DG→2DG→1DG（ON@214.6 / COOLDOWN@615.3） |

- **R0/R5 燃油不参与 PASS/FAIL**（FR-452）；表列仅供行为上下文。
- R5 的 unserved 诊断能量显著（56~593 kWh）：law=5 ECMS 聚合请求超出机组 500 kW 上限的部分
  经 dispatch clamp 后以 `P_gen_unserved` 暴露——**指令错配纯诊断量**（D-B14），由共享链如实
  上报，正是"先进 EMS 需求必须过共享 Commitment/Dispatch"架构的价值证明。
- P2 的 unserved（5~11 kWh）为加速瞬态 + 滞回带驻留期间的短时缺口。

## 4. Safety 注入场景结果（S1~S5 各一，C6+C7 全过）

| 场景 | 注入 | 触发证据 | audit |
|---|---|---|---|
| S1 DG fault | t=400 s 非指令停机（GEN_CMD_STOP_TS） | fault_latch_DG2 锁存 + FORCE_STOP 语义 | 3→1，reason=3（t=400.01） |
| S2 realized deficit | **deliverability bench（T-014 amendment 1）**：MANUAL 延迟阶跃 + SOC=0.15 | force_start 事件 + **audit reason=4**（t=45.35） | 端到端 07→16→UD→Commitment，injection_start_reason PASS |
| S3 min gen surplus | bench force 125/125 + hotel 10 + SOC 0.90 | 充电爬入死区（SOC→0.9389）→ FORCE_STOP | （force 模式 chart 旁路，D-B16） |
| S4 prospective deficit | SOC=0.15 → D_firm>0 2 s | force_start 事件 | start+confirm，reason=5 |
| S5 EMS health | EMS_HEALTH_TS t=100 拉低 | fallback 锁存 + selector 切 BASELINE | 无 commitment 迁移（设计语义） |

S2/S4 reason 归属（T-014 amendment 1 闭合）：normal path S4 preempts S2（50 kW margin
不对称，S4 场景实测 reason=5）；专用 deliverability-deficit bench 证实 S2 reason=4 端到端
（S4 看 N_ON·500+SOP 额定口径、S2 看 ΣP_actual+SOP 实际口径 + demand LPF 滞后）。S2 = S4 的
deliverability/fault/ramp 后备防线。

## 5. 判据与负向验证

- 43 号 Gate 判据 C1~C7 共 133 项全部 PASS（确定性/规则合法性/SOC 行为带/Vdc 分层/
  能量闭合/S1~S5 断言/审计完整性 + episode 一对一 + FORCED 零出现）。
- 44 号负向验证 6/6 缺陷正确检出（FORCED 拒收 / timer 不足 / 应触发未触发 / 确定性破坏 /
  episode 破坏），`negative_control` 附入 gate_control_result.json。
- T-412 against candidate（物理 Gate）：overall=true（G1~G6 全过；G6 修复后为真实逐位比较，D-B19）。

## 6. 结论（供 T-014 评审）

1. P2 臂作为唯一 conventional economic baseline 的行为质量通过 43 号 Gate（SOC 行为带、
   transition 合法性、审计完整性、注入断言、确定性）。
2. 共享 Commitment/Dispatch/Safety 链对 R0/R5/P2 恒在（先进 EMS 不可绕过的架构保证），
   三臂间唯一差异为 demand source，实测臂间行为差异集中在 demand 波形与 unserved 诊断。
3. 双机低负荷 dwell（S02 288 s / S20 120 s / M3 120 s）为滞回带设计行为，已按 D-B4 预期
   出现在报告中（不加滤波、不视为缺陷）。