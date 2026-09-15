# Phase 4-F T-445 Trajectory Oracle Gate Brainstorm

Spec: `phase4-f-trajectory-oracle-gate`  
Status: brainstorm-complete  
Created: 2026-09-15

## User Intent

T-445 Trajectory Oracle Gate（Phase 4-F 首道 Gate，roadmap §7）。科学问题：在 Model E 仿真台、无 NN，量化完美 30-bin 轨迹信息相对完美 E_300 scalar 信息在冻结 ECMS 族内的开发期 oracle headroom，产出 GO/NO-GO/INCONCLUSIVE 机器裁决；上限不存在则停止预测式 EMS 方向。

Q1 CLOSED（用户裁决 (a)）：正式臂集 B0=law=5 无预测 SOC-Adaptive ECMS / B1=law=6 完美 E_300 scalar oracle（正式 scalar 基准，规范表述"冻结 scalar ECMS 接口/控制架构下的 scalar-oracle 上界"，非无条件理论上界）/ B2=30-bin 轨迹 oracle（冻结 ECMS 族内）/ B3=离线全航程优化参照（DP 适配+parity 资格后审）；law=7/MLP-Q 至多 LEGACY_PRACTICAL_DIAGNOSTIC 不入 GO/NO-GO。Oracle parity 不变量：同真值源/同 t0/同 300 s 视界/同刷新节奏/同航程末端处理；E300_true(t0)=(10/3600)·Σ P_future_true,k(t0) 由 B2 同一轨迹派生。Headroom 号约定：H_traj=J_B1−J_B2、H_traj_pct、H_arch=J_B1−J_B3，正=可利用余量。

Q2 CLOSED（Route B′）：B2=trajectory-oracle optimized ECMS reference（暂不称严格数学上界）。作用边界：仅经 Δλ_traj 监督变量，λ_B2(t)=λ0+K_soc·(SOC_ref−SOC(t))+Δλ_traj*(t)，执行链 ECMS argmin→PMS→Safety→Plant 冻结，禁直接指挥 P_gen/P_bat/启停/调度轨迹（否则塌缩向 B3/MPC）。滚动视界纪律：每 t0 用自身闭环状态+合法 300 s 窗优化整段容许修正序列、只执行首段、推进冻结 Plant、下一 t0 重解；禁超窗信息的全航程预计算回放；离线慢算允许（oracle 参照非实时控制器）。B2 必须带冻结终端能量/charge-sustaining 机制（终端 SOC 约束带/罚/价值近似，design 选定）；+ecms/dp_* 仅复用机械件（燃油模型/SOC 转移/可行性/终端 parity/网格插值），直接功率 DP 归 B3。H_traj 释义=相对冻结 scalar-oracle ECMS 基准的轨迹 oracle 可利用余量，非纯因果信息维度效应。可选 HEURISTIC_TRAJECTORY_DIAGNOSTIC 不入 Gate。

Q3 CLOSED（含修订）：J_fuel:=J_FEC=fuel_raw+(SOC0−SOC_end)·1071.36·k_eq^E；fuel_raw 含启机燃油不改动；k_eq^E 全局冻结由 Model E 边界重算（k_eq^E=(BSFC_ref^E/1000)/η_path^E；BSFC_ref^E 禁默认五节点算术均值，可选连续区间加权/能量加权工况分布/canonical 工作区定义；效率边界审计 + NO_DOUBLE_COUNTING_OF_ENERGY_LOSS 检查，禁机械沿用 Model D 电池效率因子）。FEC 不替代终端能量可比性：预注册 TERMINAL_SOC_ADMISSIBILITY 先于 FEC 排名，越限=INFEASIBLE_OR_NONCOMPARABLE。raw 六联输出（fuel_raw/SOC0/SOC_end/ΔSOC/电池修正/J_FEC）。k_eq 稳健性：canonical/0.8×/1.2×/H 符号稳定性/break-even k_eq 距离/ΔSOC≈0 时报 K_EQ_INSENSITIVE_BY_SOC_PARITY。强制可行性/服务 parity 审计（航次完成、时长/到达一致、速度跟踪 RMS/max、欠供负荷、SOC 硬界、电池 SOP/变换器/V/I 硬限、机组 Pmin/Pmax/ramp 硬限、终端 SOC 容许、母线能量闭合）。干预审计 S1–S5+PMS override 计数/时长。运行负担代理（启停计数、TV_Pgen、ramp 激活、ON-hours、E_bat_throughput、EFC_proxy）仅报告不入 J_fuel，禁复合磨损目标。B3 记账 parity：同 raw/终端 SOC/k_eq^E/FEC/可行性/负担报告后才可用 H_arch。

Q4 CLOSED（含修订）：语料=Dataset-v3 validation split（test 保持封印留 T-452；T-445 结果=DEVELOPMENT_STAGE_ORACLE_HEADROOM_EVIDENCE 非最终泛化证据；禁用 OOD 终评证据调 T-445 设计）。初始语料冻结 36 航次=9 完整 split_group（独立推断单元=split_group，有效样本 9 簇，报告 SMALL_N_GATE_CAUTION）。波动分层：臂独立负荷波动评分（仅用冻结负荷/mission 真值，禁控制器输出），27 个 val split_group 聚合排名分 LOW/MID/HIGH 各 9 组；种子 phase4f-t445-corpus-v1，组内 SHA256 稳定排序，初始=每层前 3 组，预留=每层后 1 组（3 组 12 航次，禁结果导向替换）。统计：h_v=100·(J_B1(v)−J_B2(v))/J_B1(v) 逐航次配对，h_g=组内均值，主统计 median_g(h_g)，另报 macro mean/IQR/min/max/正组比例/分层汇总。分层配对 split_group cluster bootstrap B=10000 冻结种子，95% percentile CI（初始每层重抽 3 组、扩样后 4 组；禁窗/航次独立重抽、禁破坏 family closure、禁 B1/B2 独立重抽）。阈值 X=2.0%（最小实用 oracle 余量）/Y=0%（方向性证据底）。GO_TRAJECTORY_VALUE iff median≥2% 且 CI 下界>0 且全部硬检通过（不声称在线学习控制器可达 2%）；NO_GO_TRAJECTORY_VALUE iff CI 上界<2%（B2 为乐观 oracle，强 futility 信号）；其余=INCONCLUSIVE→唯一一次扩样（+12 航次 48=12 组，同统计同代码全量重算，禁合并两个 CI；仍不定=FINAL_INCONCLUSIVE/DECISION_REQUIRED 停）。持续不可行=负面证据：机械/基建失败可同身份重跑+事故记录；持续控制器/物理不可行禁成对删除，默认期望 0 持续 B1/B2 不可行，否则 GATE_VALIDITY_FAIL/AUDIT_REQUIRED。B3 dominance 审计：J_FEC,B3≤J_FEC,B2+ε_num（ε_num design 冻结），倒挂=B3_DOMINANCE_AUDIT_FAIL；B3 未资格化报 H_arch=UNQUALIFIED_CONTEXT。Context 链：H_scalar=J_B0−J_B1 / H_traj / H_arch。

环境：Mac 本机 MATLAB R2026a + Model E 仿真台 + .venv_tf，无 AutoDL。红线：E5 oracle 非实时可实现声明；Model E 脚本驱动/机器可读 Gate；dataset v3 零字节改动；Git 纪律。Out of scope：NN 入环（T-450+）、在线 MPC 施工（T-447）、OOD 鲁棒矩阵（T-453）、实时 profiling（T-454）、E-additional 复训。Design-stage obligations：刷新节奏数值（10 s 优选）与末端窗口机制、Δλ 容许集合/界、TERMINAL_SOC_ADMISSIBILITY 容差带、BSFC_ref^E/η_path^E 定义、波动评分定义、ε_num、B2 窗内求解器构造、B3 parity 资格化。

编号分配：FR-720+ / NFR-710+ / AC-610+ / DGN-F-01+（现状最大 FR-715/NFR-704/AC-607）。

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.
