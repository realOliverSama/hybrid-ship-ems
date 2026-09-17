# Feasibility Contract Amendment — Design Gate 裁决记录

**VERDICT: APPROVED WITH REQUIRED CORRECTIONS A1–A7**（人工/ChatGPT Amendment Design Gate）

- 裁决时间：Phase 4-F PPE 会话，T-011A 交付之后
- 裁决对象：`feasibility_contract_amendment.md` + `feasibility_contract_requirements.md` + `feasibility_contract_design.md`（初版 DRAFT）
- epoch 1 保持不可变（裁决原文确认）

## 必须修正（已全部落实）

| # | 修正要求 | 落实位置 | 状态 |
|---|---|---|---|
| A1 | epoch-1 terminal_soc 违约定性修正：**LEGACY_TERMINAL_ENERGY_NEUTRALITY_CONTRACT_VIOLATION**（非 TRUE_PHYSICAL_INFEASIBILITY——四判据全 PASS，船舶安全完成任务） | design.md D-13；amendment.md §1 F3 行、§4.2；requirements FR-C3.1 | ✅ |
| A2 | [0.25,0.90] 仅作为 **NORMAL_OPERATIONAL_SOC**（电池模型独立导出）冻结；三层区分 HARD_SAFETY [0.10,0.95] / NORMAL_OPERATIONAL [0.25,0.90] / TERMINAL_RESERVE（单独定义）；SOC 落带 ≠ 储备充分 | amendment §4.1b；requirements FR-C3.2；design D-5b | ✅ |
| A3 | P_reserve 禁止 R-a/R-b/R-c 启发式 → 由设计基准事件集 E_design 从冻结参数+语料统计推导（EV-1..EV-4） | amendment §4.3；design D-6（派生 P_dis=402/E=3.35/P_chg=168） | ✅ |
| A4 | 终端可采性 = SOC 带 AND 放电 SOP 储备 AND 充电 SOP 储备 AND 可用能量储备；自动满足者记录为派生结果、保留检查 | amendment §4.3；requirements FR-C3.3；design D-6 | ✅ |
| A5 | P_unserved 服务完备性核实（仅覆盖 V_dc 欠压降额路径）+ 强制对偶诊断 `P_service_gap = max(P_prop_demand − P_prop_actual, 0)`；分歧 → FAIL CLOSED | amendment §3.1b；design D-3 | ✅ |
| A6 | 1 Hz 轨迹批准（~25 通道/~180 MB）+ 快量硬谓词**高频审计工件**（battery SOP V/I 1e-6 容差、ramp 逐 tick → 0.01s 原生评估就地计数/极值/时长/时间戳） | design D-7b；requirements FR-C8 | ✅ |
| A7 | FEC 保留 + 稳健性**预注册**（k_eq 0.5457 冻结 + 0.8×/1.2× 敏感性 + break-even；可选次要 SOC/效率感知诊断恒为次要；禁止看结局后选择） | design D-12；requirements FR-C9 | ✅ |

## GENSET CONTRACT 附注（裁决原文）

方向批准（实际 P_DG1/P_DG2 + 实际状态 + 禁止均分回退 + 缺信号 FAIL CLOSED +
below_min/min_clamp 降级为诊断——待源码证明其非物理违规）；
状态映射 0/1/2/3/4 保留；Pmin/Pmax/ramp 按**实际冻结状态机语义**适用；
**禁止将"所有过渡态恒零功率"普遍化超出源码证明范围**——源码核实：code_genset_unit_e
第 2 行显式 `P_actual_kW = 0（非 ON 态恒 0）` ✓ 源码已证明；
资格化必须覆盖每一个过渡状态（Q8 扩展为全状态×全约束矩阵）。

## epoch-2 科学边界（裁决原文重申）

不改 Predictor-v3 身份、C* 科学语义、C0/C1/C2 forecast 定义、P0 角色、
不因 epoch-1 结局改 48 航次语料或 bootstrap 种子/方法、不为使可行性通过而改任务定义。

## 资格化增补（裁决原文 Q21–Q40 语义，映射见 design D-9b）

实际分机组信号/状态查询、缺信号 fail-closed、合法 OFF/单机/双机/过渡 fixture、
真实 Pmax/ramp 违规正 fixture、P_unserved 零缺口 fixture、真实缺供正 fixture、
P_unserved 与 demand/served 一致性、残差偏置不串扰、硬/正常 SOC 区分、
终端放电/充电 SOP 储备 PASS/FAIL、可用能量储备 PASS/FAIL、
正常 SOC 但功率/能量储备不足 FAIL、1 Hz schema/完整性/哈希、高频审计（按需）、
原子写/resume、FEC 重构、k_eq 敏感性/break-even（合成数据）、无密封数据/无结果性调参。
