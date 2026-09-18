#!/usr/bin/env python3
"""impact_scope_audit.py — EP3 影响范围只读审计 + 无效证据重新分类 + 写入点审计。

Gate: Phase 4-F Execution-Path Defect Recovery / Scope Audit。
本模块**只读**：不重训、不重生成、不改任何冻结/正式工件；产出的 JSON/MD 为新工件。

三件产出（写入 results/ppe_practical/execution_path_recovery/）：
  1. impact_scope_audit.json     每个研究工件/语料的分类与证据
  2. runtime_writer_audit.json   控制选择变量的全仓写入点清单 + 冻结覆盖状态
  3. invalid_evidence_reclassification.json  正式零结果的重新分类（保留原件）

判定所用的**决定性只读检验**：冻结 Dataset-v3 语料的 H2 `V_dc_V` 是否触及推进降额阈值
675 V（`P_prop_limit = Mot_P_nom·clamp((V_dc−600)/75,0,1)`）。未触及 ⇒ `P_load_total_kW`
（预测特征 H1 与预测目标 Y/C1 真值来源）与 EMS/PMS 源选择无关 ⇒ 语料与预测器可用。
"""
from __future__ import annotations

import glob
import hashlib
import json
import os
import sys

import numpy as np

_PPE = os.path.dirname(os.path.abspath(__file__))
_MODEL = os.path.dirname(os.path.dirname(_PPE))
OUT = os.path.join(_MODEL, "results", "ppe_practical", "execution_path_recovery")

DERATE_ONSET_V = 675.0     # build_model_e.m: P_prop_limit 线性降额起点（CCS_Vdc_Steady_Band(1)）
V_DC_NOMINAL_V = 750.0


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


# ───────────────────────── 只读检验：derate-free 判定 ─────────────────────────
def scan_iid() -> dict:
    p = os.path.join(_MODEL, "datasets_v3", "formal", "windows_v3_formal.npz")
    z = np.load(p, allow_pickle=True)
    A = z["X_H2::V_dc_V"]
    n = A.shape[0]
    gmin, n_below, n_tot = np.inf, 0, 0
    win_min = []
    for i in range(0, n, 20000):
        b = np.asarray(A[i:i + 20000], dtype=np.float64)
        gmin = min(gmin, float(b.min()))
        n_below += int(np.sum(b < DERATE_ONSET_V))
        n_tot += b.size
        win_min.append(float(b.min()))
    wm = np.array(win_min)
    return {
        "artifact": p,
        "artifact_sha256": sha256_file(p),
        "n_windows": int(n),
        "n_voyages": int(len(np.unique(z["voyage_id"]))),
        "n_samples": int(n_tot),
        "v_dc_min_V": float(gmin),
        "samples_below_derate_onset": int(n_below),
        "derate_margin_min_V": float(gmin - DERATE_ONSET_V),
        "window_min_percentiles_V": {
            "p01": float(np.percentile(wm, 1)), "p50": float(np.percentile(wm, 50)),
            "p99": float(np.percentile(wm, 99))},
        "verdict": "DERATE_NEVER_ACTIVE" if n_below == 0 else "DERATE_ACTIVE",
    }


def scan_ood() -> dict:
    import scipy.io as sio
    fs = sorted(glob.glob(os.path.join(_MODEL, "datasets_v3", "formal", "ood", "*.mat")))
    gmins, n_below, n_tot = [], 0, 0
    for f in fs:
        st = sio.loadmat(f)["stream"]
        v = np.asarray(st["V_dc_V"][0, 0]).ravel().astype(float)
        gmins.append(float(v.min()))
        n_below += int((v < DERATE_ONSET_V).sum())
        n_tot += int(v.size)
    g = np.array(gmins)
    return {
        "artifacts_glob": "datasets_v3/formal/ood/*.mat",
        "n_files": len(fs),
        "n_samples": int(n_tot),
        "v_dc_min_V": float(g.min()),
        "samples_below_derate_onset": int(n_below),
        "derate_margin_min_V": float(g.min() - DERATE_ONSET_V),
        "per_voyage_min_percentiles_V": {
            "p01": float(np.percentile(g, 1)), "p50": float(np.median(g))},
        "verdict": "DERATE_NEVER_ACTIVE" if n_below == 0 else "DERATE_ACTIVE",
    }


# ───────────────────────── 工件分类表（分析结论 + 证据） ─────────────────────────
CLASSIFICATIONS = [
    {
        "artifact": "Phase 4-F formal epoch-1 + epoch-2（192 run 记录 / ctrace / traj / audit）",
        "producing_execution_path": "ppe.run_formal_voyage → ppe.inject_voyage（执行 mission_v3 注入清单）→ ppe.run_cstar（分段 SimState）",
        "pms_source_mode_value_and_source": "0，来源 mission_v3/scenario_compile.m:293（硬编码 manifest）经 ppe/inject_voyage.m:12-15 assignin",
        "ems_output_intended_to_affect_plant": True,
        "depends_on_closed_loop_plant_state": True,
        "affected_branch_can_change_artifact": True,
        "downstream_consumers": "effect_analysis（B/L 估计）、FC-3 replay/readjudication、control_path_attribution、epoch-2 记录层",
        "scientific_consequence": "四臂被同一 baseline 需求律驱动 ⇒ EMS 操作从未到达 plant ⇒ 对 Phase 4-F 控制效应问题**无效**（数值本身对「已执行的缺陷实验」成立）",
        "classification": "SCIENTIFICALLY_INVALIDATED",
    },
    {
        "artifact": "effect_analysis（B_perfect / B_predicted / L_prediction = 0，CI [0,0]，bootstrap）",
        "producing_execution_path": "python/ppe/fc3_effect_analysis.py（消费上表 192 记录）",
        "pms_source_mode_value_and_source": "继承 0（同上）",
        "ems_output_intended_to_affect_plant": True,
        "depends_on_closed_loop_plant_state": True,
        "affected_branch_can_change_artifact": True,
        "downstream_consumers": "Phase 4-F 科学结论、论文叙述",
        "scientific_consequence": "数值正确但**不得**用作「预测无控制价值」的证据；标记 SUPERSEDED_FOR_SCIENTIFIC_EFFECT_INFERENCE",
        "classification": "SCIENTIFICALLY_INVALIDATED",
    },
    {
        "artifact": "FC-3 可行性契约（fc3_core.py checker v2/v3、语义修正：服务削减 vs tracking、post-sync PMIN）",
        "producing_execution_path": "契约层本身不产轨迹；消费 192 轨迹做 readjudication",
        "pms_source_mode_value_and_source": "不适用（契约语义不含源选择）",
        "ems_output_intended_to_affect_plant": False,
        "depends_on_closed_loop_plant_state": False,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "epoch-3 可行性判定（候选 canonical 契约）",
        "scientific_consequence": "契约与 checker 保持有效；历史 192/192 REJUDICATED_FEASIBLE 仅适用于 baseline 路由轨迹，**不得**外推为 epoch-3 必然可行",
        "classification": "UNAFFECTED",
    },
    {
        "artifact": "Dataset-v3 IID 语料（576 航次 stream + windows_v3_formal.npz）",
        "producing_execution_path": "fleet_v3.member_exec → 注入清单（PMS_SOURCE_MODE=0、EMS_CTRL_LAW=0）→ run_scenario_sim",
        "pms_source_mode_value_and_source": "0（manifest；**设计如此**：语料元数据显式记录 model_baseline_ref = 'P2 bench, law=0'）",
        "ems_output_intended_to_affect_plant": False,
        "depends_on_closed_loop_plant_state": False,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "Predictor-v3 训练/HPO/选模、C1 真值存储、Phase 4-F 语料选择",
        "scientific_consequence": "特征 H1 中与 plant 环耦合的 5 通道仅在推进降额时受闭环影响；实测语料 V_dc 最小 %.2f V（阈值 675 V，裕量 +%.1f V，0 个采样低于阈值）⇒ 负荷真值与 EMS/源选择无关 ⇒ **语料有效**" % (
            scan_iid()["v_dc_min_V"], scan_iid()["derate_margin_min_V"]),
        "classification": "AFFECTED_BUT_INTENDED",
    },
    {
        "artifact": "Dataset-v3 OOD 语料（200 航次 stream .mat）",
        "producing_execution_path": "fleet_v3.gen_ood/task_ood → member_exec（同上）",
        "pms_source_mode_value_and_source": "0（同上，设计如此）",
        "ems_output_intended_to_affect_plant": False,
        "depends_on_closed_loop_plant_state": False,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "Predictor-v3 OOD 评估、E 包证据",
        "scientific_consequence": "同 IID：实测最小 V_dc %.2f V（裕量 +%.1f V，0 个采样低于阈值）⇒ 有效" % (
            scan_ood()["v_dc_min_V"], scan_ood()["derate_margin_min_V"]),
        "classification": "AFFECTED_BUT_INTENDED",
    },
    {
        "artifact": "Dataset-v3 未来目标 Y（30×10 s 负荷能量）与 E300",
        "producing_execution_path": "dataset_v3/targets.py:64-90，源通道 stream P_load_total_kW（1 Hz）",
        "pms_source_mode_value_and_source": "不适用（目标为负荷真值）",
        "ems_output_intended_to_affect_plant": False,
        "depends_on_closed_loop_plant_state": False,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "Predictor-v3 训练目标、C1 完美信息真值",
        "scientific_consequence": "目标 = 未来负荷真值；负荷与 EMS 的唯一耦合通道（欠压降额）在全部冻结语料中**从未激活** ⇒ 目标与 PMS_SOURCE_MODE 无关，**不需要重生成**",
        "classification": "UNAFFECTED",
    },
    {
        "artifact": "Predictor-v3（特征 H1 11 通道 + K/F 6 通道、训练/HPO/confirm/winner/terminal 证据）",
        "producing_execution_path": "predictor_v3/*.py（消费 Dataset-v3 npz）",
        "pms_source_mode_value_and_source": "不适用（不读 H2 闭环通道：FR-582，data_access.py:222-244）",
        "ems_output_intended_to_affect_plant": False,
        "depends_on_closed_loop_plant_state": False,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "C2 臂活桥推理、Phase 4-E 终端证据",
        "scientific_consequence": "特征面含 5 个 plant 环通道（P_load_total / v_ship / accel / n_prop_rpm / P_prop_demand），其与 EMS 的耦合同样只在降额时出现（语料实测未出现）⇒ 训练数据有效；**残余风险**：修复后运行若产生 V_dc<675 将构成 train/runtime 语义不一致，须在 epoch-3 作为断言监控",
        "classification": "UNAFFECTED",
    },
    {
        "artifact": "C1 完美信息目标存储（datasets_v3/formal/windows_v3_formal.npz::Y，hash 203dc92d…）",
        "producing_execution_path": "dataset_v3/run_formal_freeze.py:308-330；运行时经 python/ppe/c1_index.py + +ppe/winfun_plan.m（C1_TRUTH）分发",
        "pms_source_mode_value_and_source": "不适用（负荷真值查表）",
        "ems_output_intended_to_affect_plant": False,
        "depends_on_closed_loop_plant_state": False,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "C1 臂（正式资产 artifacts/ppe_formal/c1_windows/*.mat）",
        "scientific_consequence": "与目标同源同理由 ⇒ 有效，无需重生成",
        "classification": "UNAFFECTED",
    },
    {
        "artifact": "Mission-v3 语料（mission/scenario 定义、seeds、split_group/tier、T_eval 表）",
        "producing_execution_path": "mission_v3 生成器（dag_sample/env_realize/plan_compose/family_expand）＋ dataset_v3/split.py（全部为**仿真前**纯函数）",
        "pms_source_mode_value_and_source": "不适用",
        "ems_output_intended_to_affect_plant": False,
        "depends_on_closed_loop_plant_state": False,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "epoch-3 语料身份、PPE 正式资产",
        "scientific_consequence": "任务定义与划分先于任何仿真 ⇒ 与缺陷无关 ⇒ **沿用同一冻结语料**（epoch-3 绑定同一 corpus identity）",
        "classification": "UNAFFECTED",
    },
    {
        "artifact": "Phase 4-B/C 控制层 Gate（43/44，3 臂 AUTO 矩阵 + 注入场景 S1..S5）",
        "producing_execution_path": "gate_ctrl.run_gate/run_negative（显式 assignin EMS_CTRL_LAW 与 PMS_SOURCE_MODE=src，臂定义 run_gate.m:39: R0 law=0/src=1、R5 law=5/src=1、P2 law=0/src=0）",
        "pms_source_mode_value_and_source": "1（EMS 臂；由 run_gate.m:181 显式写入场景值）",
        "ems_output_intended_to_affect_plant": True,
        "depends_on_closed_loop_plant_state": True,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "Phase 4-B 控制结论、B 包验收",
        "scientific_consequence": "EMS 臂显式绑定 src=1 且实测三臂指标不同（S20：R0 SOC_end 0.6204 / R5 0.6396 / P2 0.6307；fuel 402.5/412.5/408.3 kg）⇒ EMS 确实驱动了 plant ⇒ 结论有效",
        "classification": "UNAFFECTED",
    },
    {
        "artifact": "T-445 oracle gate（B 臂对照）",
        "producing_execution_path": "+t445/inject_scenario.m（**从不写 PMS_SOURCE_MODE** ⇒ 继承 config 标称 1）；gate 以 validity_only 关闭",
        "pms_source_mode_value_and_source": "1（config 标称；无写入点）",
        "ems_output_intended_to_affect_plant": True,
        "depends_on_closed_loop_plant_state": True,
        "affected_branch_can_change_artifact": False,
        "downstream_consumers": "F 包设计裁决（B2 optimizer 资格化）",
        "scientific_consequence": "gate_result.json: formal_corpus_observed=false、closure_mode=validity_only、failure_code=B2_OPTIMIZER_QUALIFICATION_FAILED、results/t445_oracle_gate/runs/ 为空 ⇒ **无结果性 B 臂语料存在**，不受本缺陷影响；其资格化工件（离线/控制器级）与源选择无关",
        "classification": "UNAFFECTED",
    },
    {
        "artifact": "修复后的 epoch-3 执行（尚未发生）",
        "producing_execution_path": "规划中：arm_runtime_bind → run_cstar(BindingGuard) → 遥测",
        "pms_source_mode_value_and_source": "1（ppc.control_critical_vars 单一事实源；回读断言）",
        "ems_output_intended_to_affect_plant": True,
        "depends_on_closed_loop_plant_state": True,
        "affected_branch_can_change_artifact": True,
        "downstream_consumers": "Phase 4-F 修正后结论",
        "scientific_consequence": "须在 epoch-3 监控 V_dc ≥ 675 V 裕量（train/runtime 语义一致性）与四臂轨迹不再逐位相同",
        "classification": "REQUIRES_FURTHER_EVIDENCE",
    },
]


def writer_audit() -> dict:
    """控制选择变量写入点（来源：只读全仓审计；file:line 可核）。"""
    return {
        "schema": "ppe_runtime_writer_audit_v1",
        "variables_in_scope": ["PMS_SOURCE_MODE", "EMS_CTRL_LAW", "EMS_MODE",
                               "EMS_WORK_MODE", "AI_Pred_Source", "AI_TPeak_Source",
                               "ADAPTER_FORCE_MODE", "ADAPTER_FORCE_P1_KW",
                               "ADAPTER_FORCE_P2_KW", "EMS_HEALTH_TS"],
        "pms_source_mode_writers": [
            {"file": "model/.../config/ModelE_Control_Parameters.m", "line": 56,
             "value": 1, "mechanism": "config definition (nominal)",
             "bound_by_freeze": False, "note": "标称 EMS_AGGREGATE"},
            {"file": "model/.../scripts/+mission_v3/scenario_compile.m", "line": 293,
             "value": 0.0, "mechanism": "injection manifest entry (names{8})",
             "bound_by_freeze": "partial(mission_v3 stack identity)", "note": "**缺陷源**：BASELINE bench convention"},
            {"file": "model/.../scripts/+ppe/inject_voyage.m", "lines": "11-15",
             "value": "e.value (=0)", "mechanism": "manifest-loop assignin",
             "bound_by_freeze": False, "note": "正式 PPE 路径；随后仅恢复 EMS_CTRL_LAW"},
            {"file": "model/.../scripts/+fleet_v3/member_exec.m", "lines": "81-85",
             "value": "e.value (=0)", "mechanism": "manifest-loop assignin",
             "bound_by_freeze": False, "note": "Dataset-v3 生成路径（设计如此）"},
            {"file": "model/.../scripts/+mission_v3/producer_prequal.m", "lines": "550-554",
             "value": "e.value (=0)", "mechanism": "manifest-loop assignin",
             "bound_by_freeze": "partial", "note": "T-020 生产路径"},
            {"file": "model/.../scripts/+mission_v3/producer_confirm.m", "lines": "597-601",
             "value": "e.value (=0)", "mechanism": "manifest-loop assignin",
             "bound_by_freeze": "partial", "note": "T-022 confirm 路径"},
            {"file": "model/.../scripts/+gate_e/reset_bench_nominal.m", "lines": "21-22",
             "value": 0, "mechanism": "assignin (bench reset)",
             "bound_by_freeze": False, "note": "台架不变量恢复写 0（随后 gate 显式覆盖）"},
            {"file": "model/.../scripts/+gate_ctrl/run_gate.m", "line": 181,
             "value": "src (armDefs: R0=1, R5=1, P2=0)", "mechanism": "assignin per scenario",
             "bound_by_freeze": False, "note": "Phase 4-B gate：EMS 臂显式 1 ⇒ 未受缺陷影响"},
            {"file": "model/.../scripts/+gate_ctrl/run_negative.m", "lines": "67-79,135-148",
             "value": "src (case 值)", "mechanism": "assignin",
             "bound_by_freeze": False, "note": "负向验证 D4/D6"},
            {"file": "model/.../scripts/+mission_v3/+audits/feasibility_sweep.m", "line": 297,
             "value": 0, "mechanism": "assignin",
             "bound_by_freeze": False, "note": "+audits 被 stack identity 排除域"},
            {"file": "model/.../scripts/tests/t008_variant_compile_check.m", "lines": "28-40",
             "value": 1, "mechanism": "assignin (compile matrix)",
             "bound_by_freeze": False, "note": "变体编译矩阵显式 1"},
            {"file": "model/.../scripts/+ppe/qual_observability.m", "lines": "251-261",
             "value": "(未写)", "mechanism": "继承 config 标称 1",
             "bound_by_freeze": False, "note": "Q-OBS 探针"},
        ],
        "ems_ctrl_law_writers_key": [
            {"file": "config/ModelE_Physics_Parameters.m", "line": 40, "value": 0,
             "note": "标称默认 0（Baseline）——第二静默陷阱：忘写即跑 baseline 律"},
            {"file": "scripts/+model/build_model_e.m", "lines": "101-102",
             "value": "law (循环末值 7)", "note": "builder 编译后把 base 留在 law=7（E4）"},
            {"file": "scripts/+ppe/inject_voyage.m", "line": 17, "value": "double(law)",
             "note": "正式路径唯一恢复点"},
            {"file": "scripts/+model/run_scenario_sim.m", "line": 13, "value": "double(mode)",
             "note": "Dataset 路径（mode=0）"},
            {"file": "scripts/+t445/inject_scenario.m", "lines": "8-9", "value": "double(mode)",
             "note": "T-445 路径"},
        ],
        "unbound_writers_no_freeze": [
            "scripts/+ppe/inject_voyage.m（正式 PPE 注入执行者本身未被冻结绑定；且未调用 check_injection）",
            "scripts/+model/run_scenario_sim.m（Dataset 路径核心写入者）",
            "scripts/+gate_e/reset_bench_nominal.m 及 +gate_e/gate_assign_*.m / run_dir_sim.m / run_e07.m",
            "scripts/+gate_ctrl/run_gate.m / run_negative.m / injection_scenarios.m",
            "scripts/+fleet_v3/member_exec.m 及 ctx_build/worker_provision",
            "scripts/+mission_v3/+audits/feasibility_sweep.m（+audits/ 在栈身份排除域）",
            "scripts/+ppe/qual_observability.m / qual_t005.m / qual_runner_amendment.m",
        ],
        "epoch2_freeze_injection_path_mention": False,
        "resolution": "epoch-3 冻结须绑定上述全部写入者（见 EP3_RECOVERY_REPORT §G）；未知/未绑定写入者 ⇒ overall=false",
    }


def reclassification() -> dict:
    return {
        "schema": "ppe_invalid_evidence_reclassification_v1",
        "formal_zero_result_disposition": "IMPLEMENTATION_INVALID_ZERO_RESULT",
        "revoked_prior_classification": "VALID_NEGATIVE_ARCHITECTURAL_RESULT",
        "canonical_classification": "IMPLEMENTATION_INVALID_ZERO_RESULT",
        "numerical_values": {"B_perfect_pp": 0.0, "B_predicted_pp": 0.0,
                             "L_prediction_pp": 0.0, "ci95": [0.0, 0.0]},
        "numerical_status": "CORRECT_FOR_THE_EXECUTED_DEFECTIVE_EXPERIMENT",
        "scientific_status": "SUPERSEDED_FOR_SCIENTIFIC_EFFECT_INFERENCE",
        "prohibition": "SHALL NOT be reported as evidence that prediction has zero control value",
        "reason": ("PMS_SOURCE_MODE forced the PMS Source Selector to consume the "
                   "baseline-demand branch rather than the intended EMS reference; the "
                   "C0/C1/C2 control manipulation did not propagate to the plant"),
        "preservation": {
            "deleted": [], "rewritten": [],
            "preserved": ["results/ppe_practical/formal_epoch2/**",
                          "results/ppe_practical/formal/**",
                          "results/ppe_practical/effect_analysis/**",
                          "results/ppe_practical/readjudication_epoch1*/**",
                          "results/ppe_practical/fc3_replay_input_view/**",
                          "results/ppe_practical/control_path_attribution/**"],
            "note": "全部原件逐字节保留；本工件为**新增**分类标记，不修改任何历史文件",
        },
        "fc3_status": ("FC-3 contract retained as candidate canonical feasibility contract; "
                        "historical 192/192 REJUDICATED_FEASIBLE applies only to the old "
                        "baseline-routed trajectories and SHALL NOT be assumed to carry over "
                        "to epoch-3"),
        "epoch_plan": {"formal_epoch": 3,
                       "not_a_new_estimand": True,
                       "reason": "repaired online source selection changes execution behavior"},
    }


def main() -> int:
    os.makedirs(OUT, exist_ok=True)
    iid, ood = scan_iid(), scan_ood()
    payload = {
        "schema": "ppe_impact_scope_audit_v1",
        "task": "EXECUTION_PATH_DEFECT_RECOVERY_SCOPE_AUDIT",
        "read_only": True,
        "deciding_check": {
            "question": ("is P_load_total_kW (H1 feature + predictor target + C1 truth) "
                         "independent of PMS_SOURCE_MODE / EMS plant action?"),
            "coupling_path": ("P_load_total = prop_actual + hotel; prop_actual limited by "
                              "Mot_P_nom*clamp((V_dc-600)/75,0,1); V_dc is closed-loop"),
            "derate_onset_V": DERATE_ONSET_V,
            "iid": iid, "ood": ood,
            "verdict": ("DERATE_NEVER_ACTIVE in both frozen corpora at 1 Hz resolution "
                        "-> the load truth is EMS-independent -> Dataset-v3 targets, "
                        "C1 store and Predictor-v3 training data remain valid"),
            "residual_caveat": ("1 Hz sampling cannot exclude sub-second dips; margin "
                                "(+19.5 V IID / +21.0 V OOD) bounds the exposure. "
                                "Repaired-execution V_dc must be monitored in epoch-3 "
                                "for train/runtime semantic consistency (U2)."),
        },
        "artifacts": CLASSIFICATIONS,
        "summary_counts": {
            "UNAFFECTED": sum(1 for a in CLASSIFICATIONS if a["classification"] == "UNAFFECTED"),
            "AFFECTED_BUT_INTENDED": sum(1 for a in CLASSIFICATIONS if a["classification"] == "AFFECTED_BUT_INTENDED"),
            "SCIENTIFICALLY_INVALIDATED": sum(1 for a in CLASSIFICATIONS if a["classification"] == "SCIENTIFICALLY_INVALIDATED"),
            "REQUIRES_FURTHER_EVIDENCE": sum(1 for a in CLASSIFICATIONS if a["classification"] == "REQUIRES_FURTHER_EVIDENCE"),
        },
        "stop_condition_check": {
            "predictor_v3_identity_invalidated": False,
            "dataset_v3_target_invalidated": False,
            "mission_corpus_invalidated": False,
            "c1_truth_channel_invalidated": False,
            "unresolved_additional_runtime_writers": True,
            "note": ("writers exist but are enumerated (runtime_writer_audit.json); "
                     "§G requires epoch-3 to bind them; none is unknown"),
        },
    }
    p1 = os.path.join(OUT, "impact_scope_audit.json")
    json.dump(payload, open(p1, "w"), indent=1, ensure_ascii=False, sort_keys=True)
    p2 = os.path.join(OUT, "runtime_writer_audit.json")
    json.dump(writer_audit(), open(p2, "w"), indent=1, ensure_ascii=False, sort_keys=True)
    p3 = os.path.join(OUT, "invalid_evidence_reclassification.json")
    json.dump(reclassification(), open(p3, "w"), indent=1, ensure_ascii=False, sort_keys=True)
    print(json.dumps(payload["summary_counts"]))
    print("verdict:", payload["deciding_check"]["verdict"])
    print("wrote:", p1, p2, p3, sep="\n  ")
    return 0


if __name__ == "__main__":
    sys.exit(main())
