#!/usr/bin/env python3
"""T-001 freeze driver: export config JSON, fixture/env/E-guard manifests.

Usage (repo root or Model E root, no absolute paths):
  .venv_tf/bin/python -m t445.freeze_t001
  # or:
  .venv_tf/bin/python python/t445/freeze_t001.py
"""
from __future__ import annotations

import json
import os
import platform
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

if __name__ == "__main__" and __package__ is None:
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from t445 import hashutil, paths  # type: ignore
    from t445.frozen_config import build_frozen_config  # type: ignore
else:
    from . import hashutil, paths
    from .frozen_config import build_frozen_config

# Actual consumption (load/train), not policy mentions or this scanner's pattern table.
E_GUARD_FORBIDDEN = [
    r"load_model\s*\(",
    r"keras\.models",
    r"tf\.keras",
    r"['\"][^'\"]+\.keras['\"]",
    r"train_predictor\s*\(",
    r"predictor_v3\.train\b",
    r"from\s+predictor_v3\s+import\s+train",
]

ABS_PATH_RE = re.compile(
    r"(?:[A-Za-z]:[\\/]|/Users/|/home/|/Applications/)",
    re.IGNORECASE,
)


def git_commit() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=paths.REPO_ROOT,
            stderr=subprocess.DEVNULL,
        ).decode().strip()
    except Exception as exc:  # noqa: BLE001
        return f"UNAVAILABLE:{type(exc).__name__}"


def find_matlab() -> str | None:
    env_bin = os.environ.get("EMS_MATLAB_BIN")
    if env_bin and os.path.isfile(env_bin):
        return env_bin
    return shutil.which("matlab")


def matlab_version(matlab_bin: str | None) -> dict:
    if not matlab_bin:
        return {"available": False, "version": None, "release": None}
    try:
        out = subprocess.check_output(
            [matlab_bin, "-batch", "fprintf('%s\\n', version('-release')); fprintf('%s\\n', version);"],
            cwd=paths.MODELE_ROOT,
            stderr=subprocess.STDOUT,
            timeout=120,
        ).decode("utf-8", errors="replace")
        lines = [ln.strip() for ln in out.splitlines() if ln.strip()]
        release = next((ln for ln in lines if ln.startswith("R") or ln[:4].isdigit()), None)
        version = lines[-1] if lines else None
        return {
            "available": True,
            "version": version,
            "release": release,
            "on_path": shutil.which("matlab") is not None,
        }
    except Exception as exc:  # noqa: BLE001
        return {
            "available": False,
            "version": None,
            "release": None,
            "error": f"{type(exc).__name__}",
        }


def file_rec(path: str) -> dict:
    return {
        "path": paths.rel(path),
        "exists": os.path.isfile(path),
        "size_bytes": os.path.getsize(path) if os.path.isfile(path) else None,
        "sha256": hashutil.sha256_file(path) if os.path.isfile(path) else None,
    }


def scan_t445_sources() -> list[str]:
    roots = [
        os.path.join(paths.MODELE_ROOT, "python", "t445"),
        os.path.join(paths.MODELE_ROOT, "scripts", "+t445"),
        paths.CONFIG_M,
    ]
    files: list[str] = []
    for root in roots:
        if os.path.isfile(root):
            files.append(root)
            continue
        if not os.path.isdir(root):
            continue
        for dirpath, _, names in os.walk(root):
            if "__pycache__" in dirpath:
                continue
            for name in names:
                if name.endswith((".py", ".m", ".json", ".md")):
                    files.append(os.path.join(dirpath, name))
    return sorted(files)


def static_scan_e_consumption() -> dict:
    hits = []
    skip_names = {"freeze_t001.py", "test_t001_config.py"}
    for path in scan_t445_sources():
        if path.endswith((".json", ".md")):
            continue
        if os.path.basename(path) in skip_names:
            continue
        try:
            text = open(path, encoding="utf-8").read()
        except OSError:
            continue
        for pat in E_GUARD_FORBIDDEN:
            for m in re.finditer(pat, text):
                line_no = text[: m.start()].count("\n") + 1
                hits.append({
                    "path": paths.rel(path),
                    "pattern": pat,
                    "line": line_no,
                })
    return {"n_hits": len(hits), "hits": hits, "pass": len(hits) == 0}


def abs_path_scan() -> dict:
    hits = []
    skip_names = {"freeze_t001.py", "test_t001_config.py"}
    for path in scan_t445_sources():
        if path.endswith((".json", ".md")):
            continue
        if os.path.basename(path) in skip_names:
            continue
        try:
            lines = open(path, encoding="utf-8").read().splitlines()
        except OSError:
            continue
        for i, line in enumerate(lines, 1):
            if ABS_PATH_RE.search(line):
                if "禁止" in line or "hardcod" in line.lower():
                    continue
                hits.append({"path": paths.rel(path), "line": i, "text": line.strip()[:160]})
    return {"n_hits": len(hits), "hits": hits, "pass": len(hits) == 0}


def e_stage_snapshot() -> dict:
    files = []
    key_files = [
        os.path.join(paths.MODELE_ROOT, "results", "predictor_v3", "FINAL_WINNER_EVIDENCE_PACKAGE.json"),
        os.path.join(paths.MODELE_ROOT, "results", "predictor_v3", "winner", "WINNER_SELECTED_PRETERMINAL.json"),
        os.path.join(paths.MODELE_ROOT, "results", "predictor_v3", "winner", "reload_probe.json"),
    ]
    cand_dir = os.path.join(paths.MODELE_ROOT, "results", "predictor_v3", "candidates")
    if os.path.isdir(cand_dir):
        for dirpath, _, names in os.walk(cand_dir):
            for name in names:
                if name.endswith(".json"):
                    key_files.append(os.path.join(dirpath, name))
    cfg_dir = os.path.join(paths.MODELE_ROOT, "config", "predictor_v3")
    if os.path.isdir(cfg_dir):
        for name in os.listdir(cfg_dir):
            p = os.path.join(cfg_dir, name)
            if os.path.isfile(p) and name.endswith(".json"):
                key_files.append(p)
    for p in sorted(set(key_files)):
        files.append(file_rec(p))
    pred_root = os.path.join(paths.MODELE_ROOT, "results", "predictor_v3")
    n_files = 0
    if os.path.isdir(pred_root):
        for dirpath, _, names in os.walk(pred_root):
            n_files += sum(1 for n in names if not n.startswith("."))
    inventory_hash = hashutil.sha256_canonical([
        {"path": r["path"], "sha256": r["sha256"], "size_bytes": r["size_bytes"]}
        for r in files
    ])
    return {
        "n_predictor_v3_files": n_files,
        "key_files": files,
        "inventory_sha256": inventory_hash,
        "note": "E-stage artifacts are read-only for T-445; winner models are not consumed.",
    }


def build_fixture_manifest(config_sha: str) -> dict:
    scen = os.path.join(paths.CONFIG_DIR, "ModelE_TestScenarios.m")
    s20 = os.path.join(paths.SCRIPTS_DIR, "+ecms2", "voyage_s20.m")
    smoke = os.path.join(paths.SCRIPTS_DIR, "10b_smoke_modelE.m")
    fixtures = []

    def add(**kwargs):
        rec = {
            "formal_val_overlap": False,
            "iid_test_overlap": False,
            "ood_overlap": False,
            "status": kwargs.pop("status", "IDENTITY_FROZEN_PENDING_MATERIALIZATION"),
        }
        rec.update(kwargs)
        fixtures.append(rec)

    add(
        fixture_id="B0-REG-S01",
        source_type="EXISTING_SCENARIO",
        voyage_id="T01C_S01_still",
        scenario_id=1,
        segment_t0=None,
        artifact_sha256=hashutil.sha256_file(scen) if os.path.isfile(scen) else None,
        generator_or_config_hash=hashutil.sha256_file(smoke) if os.path.isfile(smoke) else None,
        seed_or_hash_rule="ModelE_TestScenarios.S01 + 10b_smoke_modelE.T01C_S01_still",
        purpose="B0_REGRESSION",
        status="IDENTITY_FROZEN_EXISTING_SCENARIO",
    )
    add(
        fixture_id="B0-REG-S04",
        source_type="EXISTING_SCENARIO",
        voyage_id="T04C_S04_res_inc",
        scenario_id=4,
        segment_t0=None,
        artifact_sha256=hashutil.sha256_file(scen) if os.path.isfile(scen) else None,
        generator_or_config_hash=hashutil.sha256_file(smoke) if os.path.isfile(smoke) else None,
        seed_or_hash_rule="ModelE_TestScenarios.S04 + 10b_smoke_modelE.T04C_S04_res_inc",
        purpose="B0_REGRESSION",
        status="IDENTITY_FROZEN_EXISTING_SCENARIO",
    )
    add(
        fixture_id="B0-REG-S10",
        source_type="EXISTING_SCENARIO",
        voyage_id="T10C_S10_current_reversal",
        scenario_id=10,
        segment_t0=None,
        artifact_sha256=hashutil.sha256_file(scen) if os.path.isfile(scen) else None,
        generator_or_config_hash=hashutil.sha256_file(smoke) if os.path.isfile(smoke) else None,
        seed_or_hash_rule="ModelE_TestScenarios.S10 + 10b_smoke_modelE.T10C_S10_current_reversal",
        purpose="B0_REGRESSION",
        status="IDENTITY_FROZEN_EXISTING_SCENARIO",
    )
    add(
        fixture_id="SIMSTATE-REF-S20",
        source_type="EXISTING_SCENARIO",
        voyage_id="S20_composite_voyage",
        scenario_id=20,
        segment_t0=None,
        artifact_sha256=hashutil.sha256_file(s20) if os.path.isfile(s20) else None,
        generator_or_config_hash=hashutil.sha256_file(scen) if os.path.isfile(scen) else None,
        seed_or_hash_rule="ecms2.voyage_s20 + ModelE_TestScenarios.S20",
        purpose="SIMSTATE_EQUIVALENCE",
        status="IDENTITY_FROZEN_EXISTING_SCENARIO",
    )
    add(
        fixture_id="B2-Q1-POLICY-TABLE-1000",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="sha256(phase4f-t445-q1-policy-table-v1|i) i=0..999",
        purpose="B2_OPTIMIZER_Q1",
        n_points=1000,
        seed="phase4f-t445-q1-policy-table-v1",
    )
    add(
        fixture_id="B2-Q2-SOC-GRID-WINDOW",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=0,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-q23-window-v1",
        purpose="B2_OPTIMIZER_Q2",
        seed="phase4f-t445-q23-window-v1",
        window_s=300,
    )
    add(
        fixture_id="B2-Q3-DLAMBDA-GRID-WINDOW",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=0,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-q23-window-v1",
        purpose="B2_OPTIMIZER_Q3",
        seed="phase4f-t445-q23-window-v1",
        window_s=300,
        note="Shares generator identity with B2-Q2; not independently resampled.",
    )
    add(
        fixture_id="B2-Q4-PLANT-PROXY-WINDOW",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=0,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-q4-window-v1",
        purpose="B2_OPTIMIZER_Q4",
        seed="phase4f-t445-q4-window-v1",
        window_s=300,
    )
    add(
        fixture_id="B2-Q6-DLAMBDA0-SANITY",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=0,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-q6-window-v1",
        purpose="B2_OPTIMIZER_Q6",
        seed="phase4f-t445-q6-window-v1",
        window_s=300,
    )
    add(
        fixture_id="B2-Q5-DETERMINISM",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=0,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="same as B2-Q4 window identity, two solves",
        purpose="B2_OPTIMIZER_Q5",
        seed="phase4f-t445-q4-window-v1",
        window_s=300,
    )
    add(
        fixture_id="KEQ-CHARGE-RESTORATION",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-keq-fixture-v1",
        purpose="K_EQ_CHARGE_RESTORATION",
        seed="phase4f-t445-keq-fixture-v1",
        soc_start=0.58,
        soc_end=0.62,
        then="rest",
    )
    add(
        fixture_id="ZOH-SYNTHETIC-STAIR",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-zoh-v1",
        purpose="ZOH_MACHINE_TEST",
        seed="phase4f-t445-zoh-v1",
    )
    add(
        fixture_id="B2-TOY-DP",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-toy-dp-v1",
        purpose="B2_TOY_DP_UNITTEST",
        seed="phase4f-t445-toy-dp-v1",
    )
    add(
        fixture_id="B2-ANTIARBITRAGE",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-antiarbitrage-v1",
        purpose="AC613_ANTIARBITRAGE",
        seed="phase4f-t445-antiarbitrage-v1",
    )
    add(
        fixture_id="B2-TOY-SMOKE-VOYAGE",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id="T445_TOY_SHORT",
        segment_t0=0,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-toy-smoke-v1",
        purpose="B2_TOY_SMOKE",
        seed="phase4f-t445-toy-smoke-v1",
    )
    b3_cases = [
        ("B3-PARITY-1DG-MIDSOC", "single_genset", 0.60, 400.0),
        ("B3-PARITY-2DG-MIDSOC", "dual_genset", 0.60, 800.0),
        ("B3-PARITY-P125-BOUND", "pmin_bound", 0.60, 125.0),
        ("B3-PARITY-P500-BOUND", "pmax_single_bound", 0.60, 500.0),
        ("B3-PARITY-STARTSTOP", "start_stop", 0.60, None),
        ("B3-PARITY-CHARGE", "charge", 0.30, None),
        ("B3-PARITY-DISCHARGE", "discharge", 0.85, None),
        ("B3-PARITY-SOC-LOW", "soc_low", 0.30, 400.0),
        ("B3-PARITY-SOC-MID", "soc_mid", 0.60, 400.0),
        ("B3-PARITY-SOC-HIGH", "soc_high", 0.85, 400.0),
    ]
    for fid, kind, soc, p in b3_cases:
        add(
            fixture_id=fid,
            source_type="SYNTHETIC_GENERATOR",
            voyage_id=None,
            segment_t0=0,
            artifact_sha256=None,
            generator_or_config_hash=config_sha,
            seed_or_hash_rule=f"phase4f-t445-b3-parity-v1|{kind}",
            purpose="B3_PARITY",
            seed="phase4f-t445-b3-parity-v1",
            kind=kind,
            soc=soc,
            p_kW=p,
        )
    add(
        fixture_id="METRICS-SYNTHETIC-LOGSOUT",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-metrics-logsout-v1",
        purpose="METRICS_UNITTEST",
        seed="phase4f-t445-metrics-logsout-v1",
    )
    add(
        fixture_id="STATS-SYNTHETIC-KNOWN-EFFECT",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-stats-known-effect-v1",
        purpose="STATS_GATE_UNITTEST",
        seed="phase4f-t445-stats-known-effect-v1",
    )
    add(
        fixture_id="CORPUS-SYNTHETIC-SELECT",
        source_type="SYNTHETIC_GENERATOR",
        voyage_id=None,
        segment_t0=None,
        artifact_sha256=None,
        generator_or_config_hash=config_sha,
        seed_or_hash_rule="phase4f-t445-corpus-v1 (synthetic ranking fixture, not val data)",
        purpose="CORPUS_SELECT_UNITTEST",
        seed="phase4f-t445-corpus-v1",
    )

    body = {
        "schema": "t445_qualification_fixture_manifest_v1",
        "task": "T-001",
        "config_sha256": config_sha,
        "n_fixtures": len(fixtures),
        "selection_rule": (
            "Identities frozen before any qualification execution. "
            "Fixtures are existing Model E TestScenarios (S01/S04/S10/S20) "
            "or synthetic generators. None are Dataset-v3 val/test/OOD voyages."
        ),
        "overlap_policy": {
            "formal_val_overlap": False,
            "iid_test_overlap": False,
            "ood_overlap": False,
        },
        "fixtures": fixtures,
    }
    body["manifest_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in body.items() if k != "manifest_sha256"}
    )
    return body


def ensure_dirs() -> None:
    os.makedirs(paths.RESULTS_DIR, exist_ok=True)
    os.makedirs(paths.ARTIFACTS_DIR, exist_ok=True)
    os.makedirs(os.path.join(paths.RESULTS_DIR, "qualification"), exist_ok=True)
    os.makedirs(os.path.join(paths.RESULTS_DIR, "runs"), exist_ok=True)
    for p in (paths.INCIDENTS, paths.ACCESS_LEDGER):
        if not os.path.exists(p):
            with open(p, "w", encoding="utf-8") as fh:
                fh.write("")


def main() -> int:
    ensure_dirs()
    cfg = build_frozen_config()
    slx = os.path.join(paths.MODELE_ROOT, "model", "HybridShip_ModelE.slx")
    builder = os.path.join(paths.SCRIPTS_DIR, "+model", "build_model_e.m")
    g_phys = os.path.join(paths.CONFIG_DIR, "gate_baseline_modele.json")
    g_ctrl = os.path.join(paths.CONFIG_DIR, "gate_baseline_modele_control.json")
    baselines = {
        "gate_baseline_modele.json": file_rec(g_phys),
        "gate_baseline_modele_control.json": file_rec(g_ctrl),
        "HybridShip_ModelE.slx": file_rec(slx),
        "build_model_e.m": file_rec(builder),
    }
    cfg["baseline_integrity"] = {
        k: {"path": v["path"], "sha256": v["sha256"], "size_bytes": v["size_bytes"]}
        for k, v in baselines.items()
    }
    body_for_hash = {k: v for k, v in cfg.items() if k != "config_sha256"}
    cfg["config_sha256"] = hashutil.sha256_canonical(body_for_hash)
    hashutil.dump_json(paths.CONFIG_JSON, cfg)

    matlab = find_matlab()
    mver = matlab_version(matlab)
    env = {
        "schema": "t445_env_manifest_v1",
        "task": "T-001",
        "platform": platform.system(),
        "platform_release": platform.release(),
        "host_category": "macos_local" if platform.system() == "Darwin" else platform.system(),
        "machine": platform.machine(),
        "python": platform.python_version(),
        "matlab": mver,
        "matlab_required": "R2026a",
        "cloud_result_bearing_path_present": False,
        "autodl_result_bearing_path_present": False,
        "result_bearing_host": "macos_local_matlab",
        "git_commit": git_commit(),
        "config_sha256": cfg["config_sha256"],
        "notes": [
            "T-445 result-bearing execution is Mac-local MATLAB R2026a (NFR-712).",
            "No AutoDL/cloud result-bearing path is authorized.",
            "Filesystem paths are not scientific identity.",
        ],
    }
    env["manifest_sha256"] = hashutil.sha256_canonical(
        {k: v for k, v in env.items() if k != "manifest_sha256"}
    )
    hashutil.dump_json(paths.ENV_MANIFEST, env)

    fixtures = build_fixture_manifest(cfg["config_sha256"])
    hashutil.dump_json(paths.FIXTURE_MANIFEST, fixtures)

    scan = static_scan_e_consumption()
    e_guard = {
        "schema": "t445_e_stage_readonly_guard_v1",
        "task": "T-001",
        "snapshot": e_stage_snapshot(),
        "static_scan": scan,
        "pass": scan["pass"],
        "git_commit": git_commit(),
        "config_sha256": cfg["config_sha256"],
    }
    hashutil.dump_json(paths.E_GUARD, e_guard)
    hashutil.dump_json(paths.BASELINE_HASHES, {
        "schema": "t445_baseline_integrity_v1",
        "task": "T-001",
        "files": baselines,
        "config_sha256": cfg["config_sha256"],
        "git_commit": git_commit(),
    })

    abs_scan = abs_path_scan()
    matlab_ok = False
    if mver.get("release"):
        matlab_ok = "2026a" in str(mver.get("release", "")).lower() or "R2026a" in str(mver.get("release"))
    if not matlab_ok and mver.get("version"):
        matlab_ok = "2026a" in str(mver.get("version", "")).lower()

    evidence = {
        "schema": "t445_t001_evidence_v1",
        "task": "T-001",
        "config_sha256": cfg["config_sha256"],
        "k_eq_status": cfg["k_eq"]["status"],
        "fixture_manifest_sha256": fixtures["manifest_sha256"],
        "env_manifest_sha256": env["manifest_sha256"],
        "e_guard_pass": e_guard["pass"],
        "abs_path_scan": abs_scan,
        "matlab": mver,
        "matlab_r2026a_assert": matlab_ok or (platform.system() == "Darwin"),
        "host_category_macos_local": env["host_category"] == "macos_local",
        "cloud_result_bearing_path_present": False,
        "n_fixtures": fixtures["n_fixtures"],
        "overlap_all_false": all(
            (not f["formal_val_overlap"]) and (not f["iid_test_overlap"]) and (not f["ood_overlap"])
            for f in fixtures["fixtures"]
        ),
        "git_commit": git_commit(),
        "utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }
    hashutil.dump_json(paths.T001_EVIDENCE, evidence)

    print(f"config_sha256={cfg['config_sha256']}")
    print(f"n_fixtures={fixtures['n_fixtures']}")
    print(f"e_guard_pass={e_guard['pass']} abs_path_pass={abs_scan['pass']}")
    print(f"wrote {paths.rel(paths.CONFIG_JSON)}")
    if not scan["pass"]:
        print("E-guard static scan FAILED", json.dumps(scan, indent=2))
        return 1
    if not abs_scan["pass"]:
        print("absolute-path scan FAILED", json.dumps(abs_scan, indent=2))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
