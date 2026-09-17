"""Post-freeze binding verification for the formal epoch (T-010 recovery).

Recomputes every binding recorded in the T-009 freeze package against the
current repository state and emits a machine-readable verdict. Read-only:
this tool never mutates any artifact.

Run: python -m ppe.verify_formal_bindings
"""
from __future__ import annotations

import json
import os
import re
import sys

_PPE = os.path.dirname(os.path.abspath(__file__))
_PY = os.path.dirname(_PPE)
_MODELE = os.path.dirname(_PY)
for _p in (_PY, _MODELE):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from t445 import hashutil  # noqa: E402

from ppe import paths  # noqa: E402

FREEZE_JSON = os.path.join(paths.FREEZE_DIR, "ppe_pre_execution_freeze.json")
MANIFEST_JSON = os.path.join(paths.FREEZE_DIR, "formal_execution_manifest.json")
ASSETS_JSON = os.path.join(paths.FREEZE_DIR, "formal_assets.json")
CSTAR_FILES_M = os.path.join(paths.PPE_PKG_DIR, "cstar_impl_files.m")

FORMAL_OUT_DIR = os.path.join(paths.RESULTS_DIR, "formal")
CKPT_ROOT = os.path.join(paths.MODELE_ROOT, "artifacts", "ppe_formal_ckpt")


def cstar_file_list() -> list[str]:
    src = open(CSTAR_FILES_M, encoding="utf-8").read()
    return re.findall(r"'([^']+)'", src)


def cstar_impl_sha(override: dict[str, bytes] | None = None) -> str:
    """Composite SHA over the C* implementation set (same algorithm as
    ppe.cstar_impl_sha.m / freeze.cstar_impl_sha_py). `override` maps a
    relative path to raw bytes to substitute (recovery reconstructions)."""
    import hashlib

    h = hashlib.sha256()
    for rel in cstar_file_list():
        if override and rel in override:
            fs = hashlib.sha256(override[rel]).hexdigest()
        else:
            fs = hashutil.sha256_file(os.path.join(paths.MODELE_ROOT, rel))
        h.update(f"{rel}:{fs}\n".encode())
    return h.hexdigest()


def main(argv: list[str] | None = None) -> int:
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--amendment", default="",
                    help="post_freeze_runner_amendment.json; validates the "
                         "amended runner bindings instead of the frozen "
                         "run_cstar.m bytes")
    ap.add_argument("--freeze2", default="",
                    help="epoch2_pre_execution_freeze_v2.json; additionally "
                         "verifies the regenerated epoch-2 freeze "
                         "(recovery authorization §12)")
    args = ap.parse_args(argv)
    if args.freeze2 and not args.amendment:
        # §12: the re-run preflight includes the epoch-1 amended verification;
        # freeze2 mode implies it (the epoch-2 freeze re-binds the same
        # amended runner bytes).
        args.amendment = os.path.join(paths.FREEZE_DIR,
                                      "post_freeze_runner_amendment.json")

    freeze = json.load(open(FREEZE_JSON, encoding="utf-8"))
    b = freeze["bindings"]
    report: dict = {"schema": "ppe_binding_verification_v1", "checks": [],
                    "pass": True}

    def check(name: str, ok: bool, detail: str = "") -> None:
        report["checks"].append({"name": name, "pass": bool(ok),
                                 "detail": detail})
        if not ok:
            report["pass"] = False

    # --- post-freeze runner amendment (pre-execution) chaining ---
    amend = None
    if args.amendment:
        amend = json.load(open(args.amendment, encoding="utf-8"))
        a_sha = hashutil.sha256_canonical(
            {k: v for k, v in amend.items()
             if k not in ("amendment_sha256_self",)})
        check("amendment_self_hash",
              a_sha == amend["amendment_sha256_self"], a_sha[:16])
        check("amendment_binds_this_freeze",
              amend["freeze_sha256"] == freeze["freeze_sha256"],
              amend["freeze_sha256"][:16])
        check("amendment_pre_execution_confirmed",
              amend["pre_execution_confirmed"] is True)

    # --- file bindings ---
    file_bindings: dict[str, str] = {}
    file_bindings.update(b.get("assembler_file_sha256", {}))
    file_bindings.update(b.get("runner_checkpoint_file_sha256", {}))
    file_bindings.update(b.get("stats_bootstrap_file_sha256", {}))
    overrides = {}
    if amend:
        overrides = dict(amend.get("amended_file_overrides") or {})
        overrides.setdefault("scripts/+ppe/run_cstar.m",
                             amend["run_cstar_file_sha256"]["amended"])
    drift = {}
    for rel, want in sorted(file_bindings.items()):
        if amend and rel in overrides:
            want = overrides[rel]
        got = hashutil.sha256_file(os.path.join(paths.MODELE_ROOT, rel))
        if got != want:
            drift[rel] = {"frozen": want, "current": got}
    check("file_bindings_drift", not drift,
          json.dumps(drift, sort_keys=True) if drift else
          f"{len(file_bindings)} files identical to freeze"
          + (" (run_cstar.m vs amendment)" if amend else ""))

    # --- single-artifact bindings ---
    singles = {
        "modele_slx_sha256": paths.MODELE_SLX,
        "teval_table_sha256": None,          # embedded field below
        "study_config_sha256": None,         # embedded field below
        "causality_ledger_sha256": paths.CAUSALITY_LEDGER_JSON,
        "corpus_manifest_file_sha256": paths.CORPUS_MANIFEST,
        "hard_feasibility_file_sha256": paths.HF_CONFIG_JSON,
        "formal_execution_manifest_sha256": MANIFEST_JSON,
    }
    for name in ("KERAS_S0", "WINNER_JSON", "SELECTED_CONFIG_JSON",
                 "S0_EVIDENCE_JSON", "FORMAL_NORM_V3", "PREDICTOR_NORM_EXT",
                 "EVAL_CONFIG", "REAT_T_ARCH"):
        p = getattr(paths, name)
        sha = hashutil.sha256_file(p)
        key = {"KERAS_S0": "keras"}.get(name, None)
        if name == "KERAS_S0":
            check("predictor_keras_sha256",
                  sha == b["predictor"]["keras"]["sha256"], sha[:16])
        elif name == "WINNER_JSON":
            check("predictor_winner_json",
                  sha == b["predictor"]["winner_json"]["sha256"], sha[:16])
        elif name == "SELECTED_CONFIG_JSON":
            check("predictor_selected_config",
                  sha == b["predictor"]["selected_config"]["sha256"], sha[:16])
        elif name == "S0_EVIDENCE_JSON":
            check("predictor_s0_evidence",
                  sha == b["predictor"]["s0_evidence"]["sha256"], sha[:16])
        elif name == "FORMAL_NORM_V3":
            check("predictor_formal_norm_v3",
                  sha == b["predictor"]["formal_norm_v3"]["sha256"], sha[:16])
        elif name == "PREDICTOR_NORM_EXT":
            check("predictor_norm_ext_v1",
                  sha == b["predictor"]["predictor_norm_ext_v1"]["sha256"],
                  sha[:16])
        elif name == "EVAL_CONFIG":
            check("predictor_eval_config",
                  sha == b["predictor"]["eval_config"]["sha256"], sha[:16])
        elif name == "REAT_T_ARCH":
            check("predictor_reat_t_arch",
                  sha == b["predictor"]["reat_t_arch"]["sha256"], sha[:16])

    check("modele_slx_sha256",
          hashutil.sha256_file(paths.MODELE_SLX) == b["modele_slx_sha256"],
          b["modele_slx_sha256"][:16])
    caus = hashutil.sha256_file(paths.CAUSALITY_LEDGER_JSON)
    check("causality_ledger_sha256", caus == b["causality_ledger_sha256"],
          caus[:16])
    corp = hashutil.sha256_file(paths.CORPUS_MANIFEST)
    check("corpus_manifest_file_sha256", corp == b["corpus_manifest_file_sha256"],
          corp[:16])
    hf = hashutil.sha256_file(paths.HF_CONFIG_JSON)
    check("hard_feasibility_file_sha256", hf == b["hard_feasibility_file_sha256"],
          hf[:16])

    # embedded-field bindings
    study = json.load(open(paths.STUDY_CONFIG_JSON, encoding="utf-8"))
    check("study_config_sha256",
          study["config_sha256"] == b["study_config_sha256"],
          study["config_sha256"][:16])
    check("hard_feasibility_object_sha256",
          study["artifacts"]["hard_feasibility"]["object_sha256"]
          == b["hard_feasibility_object_sha256"])
    check("corpus_identity_sha256",
          study["artifacts"]["corpus_manifest"]["corpus_identity_sha256"]
          == b["corpus_identity_sha256"])
    teval = json.load(open(paths.TEVAL_TABLE_JSON, encoding="utf-8"))
    check("teval_table_sha256",
          teval["teval_table_sha256"] == b["teval_table_sha256"],
          teval["teval_table_sha256"][:16])

    # c1 store / coverage index (freeze-dir artifact fields; git-tracked)
    cov = json.load(open(os.path.join(paths.FREEZE_DIR, "c1_coverage.json"),
                         encoding="utf-8"))
    check("c1_target_store_sha256",
          cov["store_sha256"] == b["c1_target_store_sha256"],
          cov["store_sha256"][:16])
    c1idx = json.load(open(os.path.join(paths.FREEZE_DIR, "c1_target_index.json"),
                           encoding="utf-8"))
    check("c1_coverage_index_sha256",
          c1idx["index_sha256"] == b["c1_coverage_index_sha256"],
          c1idx["index_sha256"][:16])

    # manifest self-hash + freeze-binding
    man = json.load(open(MANIFEST_JSON, encoding="utf-8"))
    man_sha = hashutil.sha256_canonical(
        {k: v for k, v in man.items() if k != "manifest_sha256"})
    check("manifest_self_hash", man_sha == man["manifest_sha256"], man_sha[:16])
    check("manifest_freeze_binding",
          man_sha == b["formal_execution_manifest_sha256"], man_sha[:16])
    check("manifest_n_runs", man["n_runs"] == 192, str(man["n_runs"]))

    # freeze self-hash
    frozen_sha = hashutil.sha256_canonical(
        {k: v for k, v in freeze.items() if k != "freeze_sha256"})
    check("freeze_self_hash", frozen_sha == freeze["freeze_sha256"],
          frozen_sha[:16])

    # composite C* identity (current working tree)
    cur_composite = cstar_impl_sha()
    report["cstar_impl_sha256_current"] = cur_composite
    report["cstar_impl_sha256_frozen"] = b["cstar_implementation_sha256"]
    if amend:
        check("cstar_impl_composite_matches_amendment",
              cur_composite == amend["new_cstar_impl_sha256"],
              "current=" + cur_composite[:16])
        check("amendment_frozen_composite_reconstruction",
              amend["cstar_impl_sha256"]["reconstructed_from_frozen_bytes"]
              == b["cstar_implementation_sha256"])
    else:
        check("cstar_impl_composite_matches_freeze",
              cur_composite == b["cstar_implementation_sha256"],
              "current=" + cur_composite[:16])

    # formal assets export vs files on disk
    assets = json.load(open(ASSETS_JSON, encoding="utf-8"))
    assets_self = hashutil.sha256_canonical(
        {k: v for k, v in assets.items() if k != "assets_sha256"})
    check("formal_assets_self_hash",
          assets_self == assets["assets_sha256"], assets_self[:16])
    bad = []
    for vid, rec in assets["per_voyage"].items():
        pf = os.path.join(paths.MODELE_ROOT, "artifacts", "ppe_formal",
                          "plans", vid + ".json")
        cf = os.path.join(paths.MODELE_ROOT, "artifacts", "ppe_formal",
                          "c1_windows", vid + ".mat")
        # plan_sha256 binds the plan OBJECT; the exported file embeds it
        pdoc = json.load(open(pf, encoding="utf-8"))
        if pdoc.get("plan_sha256") != rec["plan_sha256"]:
            bad.append("plan:" + vid)
        if hashutil.sha256_file(cf) != rec["c1_windows_sha256"]:
            bad.append("c1win:" + vid)
    check("formal_assets_48_voyages", assets["n_voyages"] == 48,
          str(assets["n_voyages"]))
    check("formal_assets_files_match", not bad, ",".join(bad[:6]))

    # epoch-not-started preconditions (pre-execution recovery verification;
    # the amendment records them as historical statements)
    if not amend:
        check("no_formal_outputs", not os.path.isdir(FORMAL_OUT_DIR),
              FORMAL_OUT_DIR)
        check("no_checkpoints", not os.path.isdir(CKPT_ROOT), CKPT_ROOT)
    else:
        pre = amend.get("preconditions_verified", {})
        check("amendment_preconditions_epoch_not_started",
              pre.get("formal_outputs_existing") == 0
              and pre.get("formal_ledger_entries_at_amendment") == 0
              and pre.get("checkpoint_dirs_existing") == 0,
              json.dumps({k: pre.get(k) for k in
                          ("formal_outputs_existing",
                           "formal_ledger_entries_at_amendment",
                           "checkpoint_dirs_existing")}, sort_keys=True))
        check("amendment_scientific_bindings_unchanged",
              pre.get("all_scientific_bindings_unchanged") is True)

    # ledger state
    from ppe import ledger
    lc = ledger.ledger_counts()
    report["ledger"] = lc
    check("ledger_no_sealed_contact",
          lc["split_contact"]["test"] == 0 and lc["split_contact"]["ood"] == 0,
          json.dumps(lc["split_contact"]))

    if args.freeze2:
        verify_epoch2(report, check, args.freeze2)

    print(json.dumps(report, indent=1, sort_keys=True))
    return 0 if report["pass"] else 1


def verify_epoch2(report: dict, check, fz2_path: str) -> None:
    """Regenerated epoch-2 freeze verification (recovery authorization §12)."""
    import hashlib

    fz2_path = os.path.abspath(fz2_path)
    freeze_dir = os.path.dirname(fz2_path)
    root = paths.MODELE_ROOT
    rpp = os.path.join(root, "results", "ppe_practical")

    def sha_file(p: str) -> str:
        h = hashlib.sha256()
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        return h.hexdigest()

    with open(fz2_path, encoding="utf-8") as f:
        d = json.load(f)
    check("e2_schema", d.get("schema") == "ppe_epoch2_pre_execution_freeze_v3",
          str(d.get("schema")))
    check("e2_formal_epoch", d.get("formal_epoch") == 2)
    self_ok = hashutil.sha256_canonical(
        {k: v for k, v in d.items() if k != "sha256_self"}) == \
        d.get("sha256_self")
    check("e2_self_hash", self_ok)
    check("e2_result_bearing_runs_zero",
          d.get("epoch2_result_bearing_runs") == 0)
    cr = d.get("canonical_reserves", {})
    check("e2_reserves_canonical",
          abs(cr.get("P_reserve_dis_kW", -1) - 543.8) < 1e-9
          and abs(cr.get("E_reserve_dis_kWh", -1) - 6.839) < 1e-9
          and abs(cr.get("P_reserve_chg_kW", -1) - 819.8) < 1e-9
          and abs(cr.get("E_reserve_chg_kWh", -1) - 5.625) < 1e-9)
    check("e2_output_namespace",
          d.get("output_namespace") == "results/ppe_practical/formal_epoch2")
    check("e2_record_schema",
          d.get("record_schema") == "ppe_formal_arm_voyage_v2")
    check("e2_classification_authority",
          d.get("classification_authority") == "ppe.feasibility_contract_v2")

    file_sha = sha_file(fz2_path)
    report["epoch2_freeze_root_sha256"] = file_sha

    # gate
    gate_p = os.path.join(freeze_dir, "EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json")
    try:
        gate = json.load(open(gate_p, encoding="utf-8"))
        check("e2_gate_overall", gate.get("overall") is True)
        check("e2_gate_binds_freeze_bytes",
              gate.get("freeze_file_sha256") == file_sha)
    except FileNotFoundError:
        check("e2_gate_present", False, gate_p)

    # supersession marker
    marker_p = os.path.join(freeze_dir, "epoch2_freeze_supersession.json")
    try:
        mk = json.load(open(marker_p, encoding="utf-8"))
        check("e2_marker_disposition",
              mk.get("disposition") == "SUPERSEDED_BEFORE_FORMAL_EXECUTION")
        check("e2_marker_chain",
              mk.get("superseded_freeze_root") == d.get(
                  "supersedes_freeze_root_sha256")
              and mk.get("replacement_freeze_root") == file_sha)
        old_p = os.path.join(root, mk.get("superseded_artifact", ""))
        check("e2_defective_freeze_preserved", os.path.isfile(old_p))
    except FileNotFoundError:
        check("e2_marker_present", False, marker_p)

    # incident artifact
    inc_p = os.path.join(root, d.get("incident_artifact", ""))
    try:
        inc = json.load(open(inc_p, encoding="utf-8"))
        check("e2_incident_self_hash",
              inc.get("sha256_self") == hashutil.sha256_canonical(
                  {k: v for k, v in inc.items() if k != "sha256_self"}))
        check("e2_incident_sha_binding",
              sha_file(inc_p) == d["bindings"].get(
                  "incident_artifact_sha256"))
    except (FileNotFoundError, KeyError):
        check("e2_incident_present", False, inc_p)

    # file-level bindings
    bd = d["bindings"]
    file_rels = {
        "modele_slx_sha256": os.path.join(root, "model",
                                          "HybridShip_ModelE.slx"),
        "run_cstar_amended_sha256": os.path.join(root, "scripts", "+ppe",
                                                 "run_cstar.m"),
        "checker_v2_sha256": os.path.join(root, "scripts", "+ppe",
                                          "feasibility_contract_v2.m"),
        "traj_writer_sha256": os.path.join(root, "scripts", "+ppe",
                                           "write_traj_v1.m"),
        "audit_writer_sha256": os.path.join(root, "scripts", "+ppe",
                                            "write_audit_v1.m"),
        "runner_manifest_sha256": os.path.join(root, "scripts", "+ppe",
                                               "run_formal_manifest.m"),
        "runner_pack_sha256": os.path.join(root, "scripts", "+ppe",
                                           "run_formal_voyage_pack.m"),
        "runner_voyage_sha256": os.path.join(root, "scripts", "+ppe",
                                             "run_formal_voyage.m"),
        "terminal_pipeline_sha256": os.path.join(root, "scripts", "+ppe",
                                                 "formal_terminal_v2.m"),
        "pack_skip_predicate_sha256": os.path.join(root, "scripts", "+ppe",
                                                   "formal_pack_skip_v2.m"),
        "freeze_loader_sha256": os.path.join(root, "scripts", "+ppe",
                                             "load_epoch2_freeze_bindings.m"),
        "output_router_sha256": os.path.join(root, "scripts", "+ppe",
                                             "formal_outdir.m"),
        "checkpoint_mgr_sha256": os.path.join(root, "scripts", "+ppe",
                                              "checkpoint_mgr.m"),
        "acceptance_v2_sha256": os.path.join(root, "python", "ppe",
                                             "acceptance_v2.py"),
        "freeze_generator_sha256": os.path.join(root, "python", "ppe",
                                                "freeze_epoch2.py"),
        "verify_bindings_sha256": os.path.abspath(__file__),
        "teval_table_file_sha256": os.path.join(root, "config",
                                                "phase4f_ppe_teval_table.json"),
        "hard_feasibility_config_file_sha256": paths.HF_CONFIG_JSON,
        "t445_config_file_sha256": os.path.join(root, "config",
                                                "phase4f_t445_config.json"),
        "study_config_file_sha256": os.path.join(root, "config",
                                                 "phase4f_ppe_study_config.json"),
        "formal_execution_manifest_file_sha256": os.path.join(
            freeze_dir, "formal_execution_manifest.json"),
        "formal_assets_file_sha256": os.path.join(freeze_dir,
                                                  "formal_assets.json"),
        "c1_coverage_file_sha256": os.path.join(freeze_dir,
                                                "c1_coverage.json"),
        "c1_target_index_file_sha256": os.path.join(freeze_dir,
                                                    "c1_target_index.json"),
        "access_ledger_file_sha256": os.path.join(rpp, "access_ledger.jsonl"),
        "incident_artifact_sha256": inc_p,
    }
    specs_base = os.path.join(os.path.dirname(os.path.dirname(root)), ".specs",
                              "phase4-f-practical-predictive-ems")
    file_rels["reserve_derivation_TA0_sha256"] = os.path.join(
        specs_base, "reserve_derivation_TA0.json")
    file_rels["amendment_design_doc_sha256"] = os.path.join(
        specs_base, "feasibility_contract_design.md")
    file_rels["amendment_doc_sha256"] = os.path.join(
        specs_base, "feasibility_contract_amendment.md")
    qdir = os.path.join(rpp, "qualification")
    file_rels["amendment_v2_qual_artifact_sha256"] = os.path.join(
        qdir, "amendment_v2_qual.json")
    file_rels["e2e_qual_artifact_sha256"] = os.path.join(
        qdir, "e2e_runner_integration_qual.json")
    file_rels["fec_prereg_artifact_sha256"] = os.path.join(
        qdir, "fec_robustness_prereg.json")
    file_rels["coherence_gate_artifact_sha256"] = os.path.join(
        qdir, "feasibility_coherence_gate.json")

    drift = {}
    for key, p in sorted(file_rels.items()):
        want = bd.get(key)
        if want is None:
            drift[key] = "missing-binding"
            continue
        if not os.path.isfile(p):
            drift[key] = "file-missing"
            continue
        got = sha_file(p)
        if got != want:
            drift[key] = {"bound": want[:12], "current": got[:12]}
    check("e2_file_bindings", not drift, json.dumps(drift, sort_keys=True))

    # composite C* identity
    check("e2_cstar_composite", cstar_impl_sha() == bd.get(
        "cstar_composite_sha256"))

    # predictor assets
    pr = (bd.get("predictor") or {})
    pred_bad = []
    for key, p in (("keras", paths.KERAS_S0),
                   ("selected_config", paths.SELECTED_CONFIG_JSON),
                   ("formal_norm_v3", paths.FORMAL_NORM_V3),
                   ("predictor_norm_ext_v1", paths.PREDICTOR_NORM_EXT)):
        w = (pr.get(key) or {}).get("sha256")
        if w and hashutil.sha256_file(p) != w:
            pred_bad.append(key)
    check("e2_predictor_assets", not pred_bad, ",".join(pred_bad))

    # epoch-2 namespace still absent (pre-execution)
    check("e2_namespace_absent",
          not os.path.isdir(os.path.join(rpp, "formal_epoch2")))

    # epoch-2 FORMAL ledger entries zero
    n = 0
    ledp = os.path.join(rpp, "access_ledger.jsonl")
    if os.path.isfile(ledp):
        for line in open(ledp, encoding="utf-8"):
            line = line.strip()
            if not line:
                continue
            try:
                e = json.loads(line)
            except Exception:  # noqa: BLE001
                continue
            if e.get("purpose") == "FORMAL" and e.get("formal_epoch") == 2:
                n += 1
    check("e2_formal_ledger_zero", n == 0, str(n))

    # epoch-1 evidence intact
    ep1dir = os.path.join(rpp, "formal")
    n_term = n_par = 0
    if os.path.isdir(ep1dir):
        for vid in sorted(os.listdir(ep1dir)):
            vd = os.path.join(ep1dir, vid)
            if not os.path.isdir(vd) or vid.startswith(("_", "quarantine")):
                continue
            for fn in sorted(os.listdir(vd)):
                if fn.endswith(".json") and fn[:-5] in ("P0", "C0", "C1", "C2"):
                    try:
                        dd = json.load(open(os.path.join(vd, fn),
                                            encoding="utf-8"))
                        if str(dd.get("terminal_status", "")).startswith(
                                "COMPLETE"):
                            n_term += 1
                    except Exception:  # noqa: BLE001
                        pass
                if fn == "warmup_parity.json":
                    try:
                        if json.load(open(os.path.join(vd, fn),
                                          encoding="utf-8")) == "PASS":
                            n_par += 1
                    except Exception:  # noqa: BLE001
                        pass
    check("e2_epoch1_terminals_192", n_term == 192, str(n_term))
    check("e2_epoch1_warmup_parity_48", n_par == 48, str(n_par))


if __name__ == "__main__":
    sys.exit(main())
