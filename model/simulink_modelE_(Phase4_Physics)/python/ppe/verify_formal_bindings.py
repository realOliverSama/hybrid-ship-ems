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
    args = ap.parse_args(argv)

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

    print(json.dumps(report, indent=1, sort_keys=True))
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
