"""PPE path helpers. All paths derived from this file; no absolute literals."""
from __future__ import annotations

import os

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))  # python/ppe -> python -> ModelE
REPO_ROOT = os.path.dirname(MODELE_ROOT)              # model/ (T-445 rel() base)
PROJECT_ROOT = os.path.dirname(REPO_ROOT)             # git repo root
CONFIG_DIR = os.path.join(MODELE_ROOT, "config")
RESULTS_DIR = os.path.join(MODELE_ROOT, "results", "ppe_practical")
QUAL_DIR = os.path.join(RESULTS_DIR, "qualification")
FREEZE_DIR = os.path.join(RESULTS_DIR, "freeze")
SCRIPTS_DIR = os.path.join(MODELE_ROOT, "scripts")
PPE_PKG_DIR = os.path.join(SCRIPTS_DIR, "+ppe")
T445_PKG_DIR = os.path.join(SCRIPTS_DIR, "+t445")
DATASETS_V3 = os.path.join(MODELE_ROOT, "datasets_v3")
FORMAL_IID_DIR = os.path.join(DATASETS_V3, "formal", "iid")
WINDOWS_NPZ = os.path.join(DATASETS_V3, "formal", "windows_v3_formal.npz")

STUDY_CONFIG_JSON = os.path.join(CONFIG_DIR, "phase4f_ppe_study_config.json")
TEVAL_TABLE_JSON = os.path.join(CONFIG_DIR, "phase4f_ppe_teval_table.json")
HF_CONFIG_JSON = os.path.join(CONFIG_DIR, "phase4f_ppe_hard_feasibility.json")
CAUSALITY_LEDGER_JSON = os.path.join(CONFIG_DIR, "phase4f_ppe_causality_ledger.json")
T445_CONFIG_JSON = os.path.join(CONFIG_DIR, "phase4f_t445_config.json")
CORPUS_MANIFEST = os.path.join(
    MODELE_ROOT, "results", "t445_oracle_gate", "corpus_manifest.json")
ACCESS_LEDGER = os.path.join(RESULTS_DIR, "access_ledger.jsonl")

SPEC_DIR = os.path.join(PROJECT_ROOT, ".specs", "phase4-f-practical-predictive-ems")

# FR-836 canonical C2 predictor identities (Requirements, frozen)
KERAS_S0 = os.path.join(
    MODELE_ROOT, "experiments", "predictor_v3", "confirm", "reatt", "reatt_s0.keras")
WINNER_JSON = os.path.join(
    MODELE_ROOT, "results", "predictor_v3", "winner",
    "WINNER_SELECTED_PRETERMINAL.json")
SELECTED_CONFIG_JSON = os.path.join(
    MODELE_ROOT, "results", "predictor_v3", "candidates", "reatt",
    "selected_config.json")
S0_EVIDENCE_JSON = os.path.join(
    MODELE_ROOT, "results", "predictor_v3", "confirm", "reatt", "s0_evidence.json")
FORMAL_NORM_V3 = os.path.join(
    MODELE_ROOT, "results", "dataset_v3", "formal", "formal_norm_v3.json")
PREDICTOR_NORM_EXT = os.path.join(
    CONFIG_DIR, "predictor_v3", "predictor_norm_ext_v1.json")
EVAL_CONFIG = os.path.join(CONFIG_DIR, "predictor_v3", "eval_config.json")
REAT_T_ARCH = os.path.join(CONFIG_DIR, "predictor_v3", "reat_t_arch.json")
MODELE_SLX = os.path.join(MODELE_ROOT, "model", "HybridShip_ModelE.slx")


def rel(path: str) -> str:
    return os.path.relpath(path, REPO_ROOT).replace("\\", "/")
