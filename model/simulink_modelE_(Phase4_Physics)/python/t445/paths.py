"""T-445 path helpers. All paths derived from this file; no absolute literals."""
from __future__ import annotations

import os

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))  # python/t445 -> python -> ModelE
REPO_ROOT = os.path.dirname(MODELE_ROOT)
CONFIG_DIR = os.path.join(MODELE_ROOT, "config")
RESULTS_DIR = os.path.join(MODELE_ROOT, "results", "t445_oracle_gate")
ARTIFACTS_DIR = os.path.join(MODELE_ROOT, "artifacts", "t445")
SCRIPTS_DIR = os.path.join(MODELE_ROOT, "scripts")
T445_PKG_DIR = os.path.join(SCRIPTS_DIR, "+t445")

CONFIG_M = os.path.join(CONFIG_DIR, "phase4f_t445_config.m")
CONFIG_JSON = os.path.join(CONFIG_DIR, "phase4f_t445_config.json")
FIXTURE_MANIFEST = os.path.join(RESULTS_DIR, "qualification_fixture_manifest.json")
ENV_MANIFEST = os.path.join(RESULTS_DIR, "env_manifest_t445.json")
E_GUARD = os.path.join(RESULTS_DIR, "e_stage_readonly_guard.json")
BASELINE_HASHES = os.path.join(RESULTS_DIR, "baseline_integrity_t001.json")
T001_EVIDENCE = os.path.join(RESULTS_DIR, "t001_unit_test_result.json")
INCIDENTS = os.path.join(RESULTS_DIR, "incidents.jsonl")
ACCESS_LEDGER = os.path.join(RESULTS_DIR, "access_ledger.jsonl")


def rel(path: str) -> str:
    return os.path.relpath(path, REPO_ROOT).replace("\\", "/")
