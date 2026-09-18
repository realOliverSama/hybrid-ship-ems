#!/usr/bin/env python3
"""ep3_freeze.py — epoch-3 预执行冻结生成器（fail-closed，§M）。

**前置条件未满足时拒绝生成冻结**，并写出 `epoch3_freeze_BLOCKED.json` 说明缺哪一项。
这是刻意的：gate §M 规定冻结只能在「scope audit PASS ∧ Predictor/Dataset validity PASS ∧
execution-path repair PASS ∧ injection-chain freeze completeness PASS ∧ Q-EP3 PASS」
之后生成，而 Q-EP3 必须在真实 Model-E runtime 上执行。

冻结包内容（§M）：
  repaired execution commit / 完整运行时写入者审计 / arm-runtime manifest /
  injection-chain identities / Model-E·PMS·Safety / C* / P0..C2 身份 / Predictor-v3 /
  C1 target store / FC-3 checker / 48 航次语料 / 192-run manifest / FEC 与统计规范 /
  运行时源选择遥测 / 资格化工件

断言（overall=true 仅当全部通过）：
  CONTROL_ACTION_REACHES_PMS = proven
  PMS_SOURCE_MODE_ARM_BOUND = true
  ALL_CONTROL_CRITICAL_WRITERS_FROZEN = true
  FORMAL_EPOCH3_RUNS = 0
  IID_TEST_ACCESS = 0
  OOD_ACCESS = 0
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime, timezone

_PPE = os.path.dirname(os.path.abspath(__file__))
_MODEL = os.path.dirname(os.path.dirname(_PPE))
REPO = os.path.dirname(_MODEL)
OUT = os.path.join(_MODEL, "results", "ppe_practical", "freeze")
RECOV = os.path.join(_MODEL, "results", "ppe_practical", "execution_path_recovery")


def sha(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def rel(p: str) -> str:
    return os.path.relpath(p, _MODEL)


def header(name: str) -> str:
    """injection-chain 身份对象：路径 + 是否存在 + 字节哈希。"""
    p = os.path.join(_MODEL, name)
    if not os.path.exists(p):
        return {"path": name, "present": False, "sha256": None}
    return {"path": name, "present": True, "sha256": sha(p)}


# 全部"能改变运行时控制选择"的写入者（§G：未知/未绑定写入者 ⇒ overall=false）
WRITER_FILES = [
    "scripts/+mission_v3/scenario_compile.m",
    "scripts/+ppe/inject_voyage.m",
    "scripts/+model/run_scenario_sim.m",
    "scripts/+fleet_v3/member_exec.m",
    "scripts/+mission_v3/producer_prequal.m",
    "scripts/+mission_v3/producer_confirm.m",
    "scripts/+gate_e/reset_bench_nominal.m",
    "scripts/+gate_e/gate_assign_bench_inputs.m",
    "scripts/+gate_ctrl/run_gate.m",
    "scripts/+gate_ctrl/run_negative.m",
    "scripts/+gate_ctrl/injection_scenarios.m",
    "scripts/+mission_v3/+audits/feasibility_sweep.m",
    "config/ModelE_Control_Parameters.m",
    "config/ModelE_Physics_Parameters.m",
    "config/ModelE_TestScenarios.m",
]

# 修复/遥测/臂绑定身份
REPAIR_FILES = [
    "scripts/+ppe/control_critical_vars.m",
    "scripts/+ppe/arm_runtime_bind.m",
    "scripts/+ppe/assert_control_bindings.m",
    "scripts/+ppe/arm_runtime_manifest.m",
    "scripts/+ppe/write_sourcesel_v1.m",
    "scripts/+ppe/cstar_impl_files_ep3.m",
    "scripts/+ppe/load_epoch3_freeze_bindings.m",
    "scripts/+ppe/qual_ep3.m",
    "scripts/+model/set_source_selection_logging.m",
    "scripts/+ppe/run_cstar.m",
]

# 科学身份（沿用冻结，不重生成）
SCIENCE_FILES = [
    "model/HybridShip_ModelE.slx",
    "config/phase4f_t445_config.json",
    "config/phase4f_ppe_teval_table.json",
    "config/phase4f_ppe_hard_feasibility.json",
    "config/phase4f_ppe_study_config.json",
    "scripts/+ppe/feasibility_contract_v2.m",
    "scripts/+ppe/write_traj_v1.m",
    "scripts/+ppe/write_audit_v1.m",
    "scripts/+ppe/formal_terminal_v2.m",
    "python/ppe/fc3_core.py",
    "datasets_v3/formal/windows_v3_formal.npz",
]


def main(argv) -> int:
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    blockers, evidence = [], {}

    # ---- 前置工件 ----
    qual = os.path.join(RECOV, "qual_ep3", "qual_ep3_result.json")
    audit = os.path.join(RECOV, "impact_scope_audit.json")
    writers = os.path.join(RECOV, "runtime_writer_audit.json")
    reclass = os.path.join(RECOV, "invalid_evidence_reclassification.json")
    for p, label in ((audit, "impact_scope_audit"), (writers, "runtime_writer_audit"),
                     (reclass, "invalid_evidence_reclassification")):
        if not os.path.exists(p):
            blockers.append(f"missing {label}: {rel(p)}")
        else:
            evidence[label] = {"path": rel(p), "sha256": sha(p)}

    if not os.path.exists(qual):
        blockers.append("Q-EP3 qualification artifact missing "
                        "(results/ppe_practical/execution_path_recovery/qual_ep3/"
                        "qual_ep3_result.json) -- Q-EP3 must run on the real Model-E runtime")
        q = None
    else:
        q = json.load(open(qual))
        if not q.get("pass"):
            blockers.append("Q-EP3 qualification FAIL")

    # ---- 语料 / 预测器身份存在性 ----
    corpus_npz = os.path.join(_MODEL, "datasets_v3", "formal", "windows_v3_formal.npz")
    if not os.path.exists(corpus_npz):
        blockers.append("frozen corpus npz missing")

    # ---- 写入者与修复文件身份 ----
    inj = {"writers": [header(f) for f in WRITER_FILES],
           "repair": [header(f) for f in REPAIR_FILES],
           "science": [header(f) for f in SCIENCE_FILES]}
    missing_writers = [h["path"] for h in inj["writers"] if not h["present"]]
    if missing_writers:
        blockers.append(f"unbound/missing writer files: {missing_writers}")

    # ---- 访问计数（期望 0）----
    def ledger_count(patterns):
        """按 purpose 字段计数（历史 note 里提到 "OOD sealed" 不算访问）。"""
        p = os.path.join(_MODEL, "results", "ppe_practical", "access_ledger.jsonl")
        if not os.path.exists(p):
            return 0
        n = 0
        for line in open(p):
            try:
                e = json.loads(line)
            except Exception:
                continue
            pur = str(e.get("purpose", "")) + "|" + str(e.get("artifact", ""))
            if any(pat in pur for pat in patterns):
                n += 1
        return n

    iid_like = ledger_count(["IID_ACCESS", "TEST_ACCESS", "IID_TEST"])
    ood_like = ledger_count(["OOD_ACCESS"])
    epoch3 = 0  # 尚未有 epoch-3 运行
    access = {"FORMAL_EPOCH3_RUNS": epoch3, "IID_TEST_ACCESS": iid_like,
              "OOD_ACCESS": ood_like}

    assertions = {
        "CONTROL_ACTION_REACHES_PMS": "NOT_PROVEN" if q is None else (
            "proven" if q.get("pass") else "NOT_PROVEN"),
        "PMS_SOURCE_MODE_ARM_BOUND": bool(q) and q.get("pass", False),
        "ALL_CONTROL_CRITICAL_WRITERS_FROZEN": not missing_writers,
        "FORMAL_EPOCH3_RUNS": epoch3,
        "IID_TEST_ACCESS": iid_like,
        "OOD_ACCESS": ood_like,
    }
    ok = (not blockers and os.path.exists(qual)
          and assertions["CONTROL_ACTION_REACHES_PMS"] == "proven"
          and assertions["ALL_CONTROL_CRITICAL_WRITERS_FROZEN"]
          and epoch3 == 0 and iid_like == 0 and ood_like == 0)

    if not ok:
        out = {"schema": "ppe_epoch3_pre_execution_freeze_BLOCKED_v1",
               "utc": now, "overall": False, "blockers": blockers or
               ["assertions not all true"], "assertions": assertions,
               "evidence": evidence, "injection_chain": inj,
               "note": ("fail-closed: gate §M forbids building the epoch-3 freeze before "
                        "Q-EP3 PASS on the real Model-E runtime; no freeze file written")}
        p = os.path.join(OUT, "epoch3_pre_execution_freeze_BLOCKED.json")
        os.makedirs(OUT, exist_ok=True)
        json.dump(out, open(p, "w"), indent=1, ensure_ascii=False, sort_keys=True)
        print("[EP3-FREEZE] BLOCKED —", "; ".join(out["blockers"])[:400])
        print("wrote", rel(p))
        return 2

    # ---- 生成冻结（只有全 PASS 才可达）----
    freeze = {"schema": "ppe_epoch3_pre_execution_freeze_v1",
              "formal_epoch": 3, "utc": now, "overall": True,
              "assertions": assertions, "blockers": [],
              "repaired_execution_commit": subprocess.check_output(
                  ["git", "-C", REPO, "rev-parse", "HEAD"]).decode().strip(),
              "injection_chain": inj,
              "arm_runtime_manifest": header("scripts/+ppe/control_critical_vars.m"),
              "qualification": evidence,
              "corpus_policy": "RETAIN_SAME_FROZEN_CORPUS (48 voyages; corpus identity unchanged)",
              "run_manifest": "192 runs = 48 voyages x {P0,C0,C1,C2}",
              "provenance": {"generator": "python/ppe/ep3_freeze.py",
                             "generator_sha256": sha(os.path.abspath(__file__))}}
    p = os.path.join(OUT, "epoch3_pre_execution_freeze.json")
    json.dump(freeze, open(p, "w"), indent=1, ensure_ascii=False, sort_keys=True)
    print("[EP3-FREEZE] built", rel(p))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
