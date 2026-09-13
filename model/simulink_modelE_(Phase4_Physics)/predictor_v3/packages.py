# -*- coding: utf-8 -*-
"""packages.py — winner 包封装 + reload 探针（DGN-E-17；FR-590/611）。

- PRETERMINAL_WINNER_PACKAGE：收集 canonical seed0 `.keras` 工件（confirm 证据
  `s0_evidence.json` 的 `artifact` 字段 = seed0 canonical 交付工件）、model_config、
  input_contract、complexity_report（candidates/<arch>/ 下）、
  WINNER_SELECTED_PRETERMINAL.json、reload_probe.json（探针后补，存在才收），
  写包 manifest（self-hash 配方与仓库统一：canonical JSON 排除
  artifact_sha256 字段后 SHA256，见 registry.write_json_with_self_hash）。
- reload 探针协议（冻结）：保存后模型经 `keras.models.load_model`（custom_objects
  = hpo._custom_objects()）新鲜加载 → 推理 → 与保存前参考输出对比；核验项全部
  机器判定：reload 成功、shape 精确一致、输出有限、quantile 顺序 q10≤q50≤q90
  （容差 1e-5）、`max|Δ| ≤ 1e-5`（预注册容差；bitwise 更佳则如实记 bitwise=True）。
  verdict=FAIL 时返回 FAIL 文档、不抛异常（FAIL ⇒ T-444a 不得闭合）。

canonical 交付种子语义（tasks gate R5 镜像澄清）：canonical_delivery_seed_index=0
固定交付工件；科学 Winner 身份 = model_id + config_sha256；本模块只封装身份，
不重开任何科学判断。
"""
from __future__ import annotations

import hashlib
import json
import os
from datetime import datetime, timezone

import numpy as np

from predictor_v3 import registry as reg  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
RESULTS_DIR = os.path.join(MODELE_ROOT, "results", "predictor_v3")
CONFIRM_DIR = os.path.join(RESULTS_DIR, "confirm")
CANDIDATES_DIR = os.path.join(RESULTS_DIR, "candidates")
WINNER_DIR = os.path.join(RESULTS_DIR, "winner")

PACKAGE_NAME = "PRETERMINAL_WINNER_PACKAGE"
MANIFEST_SCHEMA = "predictor_v3_package_manifest_v1"
RELOAD_PROBE_SCHEMA = "predictor_v3_reload_probe_v1"
MANIFEST_FILENAME = "PRETERMINAL_WINNER_PACKAGE.json"
RELOAD_PROBE_FILENAME = "reload_probe.json"
PRETERMINAL_FILENAME = "WINNER_SELECTED_PRETERMINAL.json"

CANONICAL_SEED_INDEX = 0          # canonical 交付种子（tasks gate R5）
PROBE_WINDOWS = 64                # 探针窗口数（DGN-E-17：val 前 64 窗）
QUANTILE_ORDER_TOL = 1e-5         # q10≤q50≤q90 顺序核验容差（冻结）
DEFAULT_TOLERANCE = 1e-5          # max|Δ| 预注册容差（冻结）


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _sha256_array(arr: np.ndarray) -> str:
    """数组身份 hash：C 连续字节流 SHA256（float32/float64 均按内存字节）。"""
    a = np.ascontiguousarray(np.asarray(arr))
    return hashlib.sha256(a.tobytes()).hexdigest()


def _read_json(path: str) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


# ------------------------------------------------------------ 成员收集 / 包封装

def _member(abs_path: str, role: str) -> dict:
    return {"path": os.path.relpath(abs_path, MODELE_ROOT),
            "sha256": sha256_file(abs_path), "role": role}


def _canonical_artifact(winner_arch: str) -> str:
    """从 confirm seed0 证据读 canonical .keras 工件路径（相对 MODELE_ROOT）。

    证据文件存在但工件文件缺失 ⇒ raise（包完整性损坏必须暴露，禁静默跳过）。
    """
    ev_path = os.path.join(CONFIRM_DIR, winner_arch, "s0_evidence.json")
    if not os.path.isfile(ev_path):
        return ""
    artifact_rel = _read_json(ev_path)["artifact"]
    artifact_abs = os.path.join(MODELE_ROOT, artifact_rel)
    if not os.path.isfile(artifact_abs):
        raise FileNotFoundError(
            f"canonical seed0 工件缺失（证据 {artifact_rel} 指向不存在文件）——"
            f"PRETERMINAL_WINNER_PACKAGE 禁止在交付工件缺失时封装")
    return artifact_abs


def _winner_identity(winner_arch: str, has_preterminal: bool,
                     has_evidence: bool) -> dict:
    """winner 身份（model_id + config_sha256）：优先 WINNER_SELECTED_PRETERMINAL，
    回退 confirm seed0 证据；两者皆无 ⇒ 无法确定身份，raise。"""
    if has_preterminal:
        w = _read_json(os.path.join(WINNER_DIR,
                                    PRETERMINAL_FILENAME))["winner"]
        return {"model_id": w["model_id"],
                "config_sha256": w["config_sha256"],
                "canonical_delivery_seed_index": CANONICAL_SEED_INDEX}
    if has_evidence:
        ev = _read_json(os.path.join(CONFIRM_DIR, winner_arch,
                                     "s0_evidence.json"))
        return {"model_id": ev["model_id"],
                "config_sha256": ev["config_sha256"],
                "canonical_delivery_seed_index": CANONICAL_SEED_INDEX}
    raise ValueError(
        f"{winner_arch}: 无法确定 winner 身份（无 WINNER_SELECTED_PRETERMINAL "
        f"且无 confirm s0_evidence）——拒绝封装无身份包")


def build_preterminal_package(winner_arch: str,
                              out_dir: str | None = None) -> dict:
    """封装 PRETERMINAL_WINNER_PACKAGE 并写包 manifest（DGN-E-16(10)/DGN-E-17）。

    成员（存在的才收，按 DGN-E-17 冻结顺序）：
      canonical .keras（confirm s0 证据）、model_config.json、input_contract.json、
      complexity_report.json（candidates/<arch>/）、WINNER_SELECTED_PRETERMINAL.json、
      reload_probe.json（探针后补）。
    manifest 写 <out_dir|winner>/PRETERMINAL_WINNER_PACKAGE.json；
    self-hash 配方与仓库统一（canonical JSON 排除 artifact_sha256 后 SHA256）。
    返回写入的 manifest dict（含 artifact_sha256）。
    """
    cand_dir = os.path.join(CANDIDATES_DIR, winner_arch)
    preterminal_path = os.path.join(WINNER_DIR, PRETERMINAL_FILENAME)
    probe_path = os.path.join(WINNER_DIR, RELOAD_PROBE_FILENAME)

    artifact_abs = _canonical_artifact(winner_arch)
    has_preterminal = os.path.isfile(preterminal_path)
    has_evidence = os.path.isfile(os.path.join(CONFIRM_DIR, winner_arch,
                                               "s0_evidence.json"))

    members: list = []
    if artifact_abs:
        members.append(_member(artifact_abs, "canonical_keras_artifact"))
    for rel, role in (("model_config.json", "model_config"),
                      ("input_contract.json", "input_contract"),
                      ("complexity_report.json", "complexity_report")):
        p = os.path.join(cand_dir, rel)
        if os.path.isfile(p):
            members.append(_member(p, role))
    if has_preterminal:
        members.append(_member(preterminal_path, "winner_selected_preterminal"))
    if os.path.isfile(probe_path):
        members.append(_member(probe_path, "reload_probe"))

    manifest = {
        "schema": MANIFEST_SCHEMA,
        "package": PACKAGE_NAME,
        "winner": _winner_identity(winner_arch, has_preterminal, has_evidence),
        "members": members,
    }
    out_path = os.path.join(out_dir or WINNER_DIR, MANIFEST_FILENAME)
    manifest["artifact_sha256"] = reg.write_json_with_self_hash(
        manifest, out_path)
    return manifest


# ------------------------------------------------------------ reload 探针（冻结）

def run_reload_probe(model_path: str, probe_input: dict,
                     reference_output: np.ndarray,
                     tolerance: float = DEFAULT_TOLERANCE) -> dict:
    """reload 探针（FR-590/DGN-E-17 冻结协议）。

    保存后模型新鲜加载（custom_objects = hpo._custom_objects()）→ 推理 →
    与保存前参考输出对比。verdict=FAIL 时返回 FAIL 文档、不抛异常
    （load 失败文档附加 `error` 诊断字段；verdict 判定字段语义不变）。
    """
    reference_output = np.asarray(reference_output)
    ref_sha = _sha256_array(reference_output)
    per_input = {name: _sha256_array(v)
                 for name, v in sorted(probe_input.items()) if v is not None}
    probe_sha = hashlib.sha256(_canonical(per_input)).hexdigest()

    doc = {
        "schema": RELOAD_PROBE_SCHEMA,
        "model_path": os.path.relpath(model_path, MODELE_ROOT),
        "probe_input_sha256": probe_sha,
        "reference_output_sha256": ref_sha,
        "max_abs_delta": None,
        "bitwise": False,
        "shape_exact": False,
        "finite": False,
        "quantile_order_ok": False,
        "tolerance": float(tolerance),
        "verdict": "FAIL",
        "utc": datetime.now(timezone.utc).isoformat(),
        "git_commit": os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED"),
    }

    try:
        import tensorflow as tf
        from predictor_v3.hpo import _custom_objects
        model = tf.keras.models.load_model(model_path, compile=False,
                                           custom_objects=_custom_objects())
        inputs = {k: v for k, v in probe_input.items() if v is not None}
        new_output = np.asarray(model.predict(inputs, verbose=0))
    except Exception as exc:  # noqa: BLE001 — FAIL 必须落文档而非抛异常
        doc["error"] = f"{type(exc).__name__}: {exc}"
        return doc

    shape_exact = new_output.shape == reference_output.shape
    finite = bool(np.isfinite(new_output).all())
    quantile_order_ok = _quantile_order_ok(new_output)
    if shape_exact:
        max_abs_delta = float(np.max(np.abs(new_output - reference_output)))
        bitwise = bool(np.array_equal(new_output, reference_output))
    else:
        max_abs_delta = None
        bitwise = False

    doc["shape_exact"] = bool(shape_exact)
    doc["finite"] = finite
    doc["quantile_order_ok"] = bool(quantile_order_ok)
    doc["max_abs_delta"] = max_abs_delta
    doc["bitwise"] = bitwise
    delta_ok = max_abs_delta is not None and max_abs_delta <= doc["tolerance"]
    doc["verdict"] = "PASS" if (shape_exact and finite and quantile_order_ok
                                and delta_ok) else "FAIL"
    return doc


def _canonical(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def _quantile_order_ok(output: np.ndarray, tol: float = QUANTILE_ORDER_TOL) -> bool:
    """q10 ≤ q50 ≤ q90（末维 [q10,q50,q90]，容差 tol；shape 不合 ⇒ False）。"""
    if output.ndim < 1 or output.shape[-1] != 3:
        return False
    q10, q50, q90 = (output[..., 0], output[..., 1], output[..., 2])
    return bool(np.all(q10 <= q50 + tol) and np.all(q50 <= q90 + tol))


def make_probe_input(val_hist: np.ndarray,
                     val_kf: np.ndarray | None) -> dict:
    """探针输入 = val 前 64 窗（DGN-E-17；val 来源合法非 test/OOD）。

    身份纪律：调用方负责记录源文件 sha256；本函数只切片 + float32 化。
    窗数 < PROBE_WINDOWS ⇒ raise（禁静默少窗）。
    """
    val_hist = np.asarray(val_hist)
    if val_hist.shape[0] < PROBE_WINDOWS:
        raise ValueError(
            f"val 窗数 {val_hist.shape[0]} < 探针要求 {PROBE_WINDOWS}——禁静默少窗")
    out = {"hist_input": np.ascontiguousarray(
        val_hist[:PROBE_WINDOWS], dtype=np.float32)}
    if val_kf is not None:
        val_kf = np.asarray(val_kf)
        if val_kf.shape[0] < PROBE_WINDOWS:
            raise ValueError(
                f"kf 窗数 {val_kf.shape[0]} < 探针要求 {PROBE_WINDOWS}")
        out["kf_input"] = np.ascontiguousarray(
            val_kf[:PROBE_WINDOWS], dtype=np.float32)
    else:
        out["kf_input"] = None
    return out
