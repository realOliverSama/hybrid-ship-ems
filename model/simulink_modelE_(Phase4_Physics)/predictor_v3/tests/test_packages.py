# -*- coding: utf-8 -*-
"""T-018 前置单测：winner 包封装 + reload 探针（DGN-E-17；FR-590）。

覆盖：① mlpq 小合成保存→reload 探针 PASS；② 被篡改模型 → FAIL 且
max_abs_delta 超容差；③ 伪造最小成员集 → 包 manifest + self-hash 复算一致；
④ make_probe_input 形状/float32 语义。
"""
import hashlib
import json
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import packages as pk  # noqa: E402

RNG = np.random.default_rng(20260918)


def _canonical(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def _build_mlpq(s0: float = 0.2):
    """复用 predictor_v3.models 的 mlpq build（含共用单调 quantile 头）。"""
    import tensorflow as tf
    from predictor_v3.models import ARCH_BUILDERS
    tf.keras.utils.set_random_seed(0)
    return ARCH_BUILDERS["mlpq"]["build"]({"lr": 1e-3, "batch": 256}, s0)


def _probe_inputs(n: int = pk.PROBE_WINDOWS) -> dict:
    hist = RNG.normal(0, 1, size=(n, 300, 11)).astype(np.float32)
    return {"hist_input": hist, "kf_input": None}


# ---------------- ① 正常 reload：save → 新鲜加载 → PASS（bitwise 或 ≤1e-5）

def test_reload_probe_pass_on_unmodified_model(tmp_path):
    tf = pytest.importorskip("tensorflow")
    from predictor_v3.hpo import _custom_objects
    probe_input = _probe_inputs()
    model = _build_mlpq()
    reference = np.asarray(model.predict(
        {k: v for k, v in probe_input.items() if v is not None}, verbose=0))
    assert reference.shape == (pk.PROBE_WINDOWS, 30, 3)

    art = str(tmp_path / "mlpq_s0.keras")
    model.save(art)

    doc = pk.run_reload_probe(art, probe_input, reference)
    assert doc["schema"] == "predictor_v3_reload_probe_v1"
    assert doc["verdict"] == "PASS"
    assert doc["shape_exact"] and doc["finite"] and doc["quantile_order_ok"]
    assert doc["max_abs_delta"] <= doc["tolerance"]
    if doc["bitwise"]:
        assert doc["max_abs_delta"] == 0.0
    # 身份字段可复算：probe 输入/参考输出的 SHA256 canonical 拼接
    per = {"hist_input": pk._sha256_array(probe_input["hist_input"])}
    assert doc["probe_input_sha256"] == hashlib.sha256(_canonical(per)).hexdigest()
    assert doc["reference_output_sha256"] == pk._sha256_array(reference)
    assert doc["tolerance"] == 1e-5
    assert doc["git_commit"] == os.environ.get("EMS_PINNED_GIT_COMMIT", "UNPINNED")
    # 实际复载一次验证新鲜加载路径本身可用（与探针同 custom_objects）
    tf.keras.models.load_model(art, compile=False, custom_objects=_custom_objects())


# ---------------- ② 被篡改模型 → verdict FAIL 且 max_abs_delta 超容差

def test_reload_probe_fails_on_tampered_model(tmp_path):
    tf = pytest.importorskip("tensorflow")
    from predictor_v3.hpo import _custom_objects
    probe_input = _probe_inputs()
    model = _build_mlpq()
    reference = np.asarray(model.predict(
        {k: v for k, v in probe_input.items() if v is not None}, verbose=0))

    # 篡改：加载后改动 raw 投影 bias（最后一段可训练权重），再保存为新工件
    tampered_path = str(tmp_path / "mlpq_tampered.keras")
    model.save(tampered_path)
    m = tf.keras.models.load_model(tampered_path, compile=False,
                                   custom_objects=_custom_objects())
    weights = m.get_weights()
    weights[-1] = weights[-1] + 0.5          # raw Dense bias += 0.5
    m.set_weights(weights)
    m.save(tampered_path)

    doc = pk.run_reload_probe(tampered_path, probe_input, reference)
    assert doc["verdict"] == "FAIL"
    assert doc["max_abs_delta"] is not None
    assert doc["max_abs_delta"] > doc["tolerance"]
    assert not doc["bitwise"]


def test_reload_probe_fail_doc_on_unloadable_model(tmp_path, monkeypatch):
    """load 失败（文件损坏）→ FAIL 文档不抛异常（FR-590 fail-closed）。"""
    pytest.importorskip("tensorflow")
    bad = tmp_path / "broken.keras"
    bad.write_bytes(b"not-a-keras-file")
    probe_input = _probe_inputs()
    reference = np.zeros((pk.PROBE_WINDOWS, 30, 3), dtype=np.float32)
    doc = pk.run_reload_probe(str(bad), probe_input, reference)
    assert doc["verdict"] == "FAIL"
    assert doc["shape_exact"] is False and doc["bitwise"] is False
    assert "error" in doc


# ---------------- ③ 伪造最小成员集 → 包 manifest + self-hash 复算一致

def _fake_member_tree(root, arch="mlpq", with_probe=False):
    """在 fake MODELE_ROOT 下伪造最小成员文件集（不触碰真实 results/）。"""
    rel_art = f"experiments/predictor_v3/confirm/{arch}/{arch}_s0.keras"
    art = root / rel_art
    art.parent.mkdir(parents=True, exist_ok=True)
    art.write_bytes(b"fake-keras-bytes")

    cand = root / "results" / "predictor_v3" / "candidates" / arch
    cand.mkdir(parents=True, exist_ok=True)
    (cand / "model_config.json").write_text(json.dumps(
        {"schema": "predictor_v3_model_config_v1", "model_id": arch}))
    (cand / "input_contract.json").write_text(json.dumps(
        {"schema": "predictor_v3_input_contract_v1"}))
    (cand / "complexity_report.json").write_text(json.dumps(
        {"param_count": 200_000}))

    winner = root / "results" / "predictor_v3" / "winner"
    winner.mkdir(parents=True, exist_ok=True)
    (winner / "WINNER_SELECTED_PRETERMINAL.json").write_text(json.dumps(
        {"schema": "predictor_v3_winner_selected_preterminal_v1",
         "winner": {"model_id": arch, "config_sha256": "cfg-" + arch,
                    "canonical_delivery_seed_index": 0}}))
    if with_probe:
        (winner / "reload_probe.json").write_text(json.dumps(
            {"schema": "predictor_v3_reload_probe_v1", "verdict": "PASS"}))
    return root / rel_art


def test_build_preterminal_package_fake_members(tmp_path, monkeypatch):
    monkeypatch.setattr(pk, "MODELE_ROOT", str(tmp_path))
    monkeypatch.setattr(pk, "CONFIRM_DIR",
                        str(tmp_path / "results" / "predictor_v3" / "confirm"))
    monkeypatch.setattr(pk, "CANDIDATES_DIR",
                        str(tmp_path / "results" / "predictor_v3" / "candidates"))
    monkeypatch.setattr(pk, "WINNER_DIR",
                        str(tmp_path / "results" / "predictor_v3" / "winner"))

    # confirm s0 证据（canonical 工件引用，相对 MODELE_ROOT）
    ev_dir = tmp_path / "results" / "predictor_v3" / "confirm" / "mlpq"
    ev_dir.mkdir(parents=True)
    (ev_dir / "s0_evidence.json").write_text(json.dumps(
        {"artifact": "experiments/predictor_v3/confirm/mlpq/mlpq_s0.keras",
         "model_id": "mlpq", "config_sha256": "cfg-mlpq", "seed_index": 0}))
    _fake_member_tree(tmp_path, with_probe=False)

    out_dir = tmp_path / "results" / "predictor_v3" / "winner"
    doc = pk.build_preterminal_package("mlpq", out_dir=str(out_dir))

    assert doc["schema"] == "predictor_v3_package_manifest_v1"
    assert doc["package"] == "PRETERMINAL_WINNER_PACKAGE"
    assert doc["winner"] == {"model_id": "mlpq", "config_sha256": "cfg-mlpq",
                             "canonical_delivery_seed_index": 0}
    roles = [m["role"] for m in doc["members"]]
    assert roles == ["canonical_keras_artifact", "model_config",
                     "input_contract", "complexity_report",
                     "winner_selected_preterminal"]      # reload_probe 后补未写
    for m in doc["members"]:
        assert not os.path.isabs(m["path"]) and ".." not in m["path"].split(os.sep)
        real = os.path.join(str(tmp_path), m["path"])
        assert hashlib.sha256(open(real, "rb").read()).hexdigest() == m["sha256"]

    # self-hash 复算（canonical JSON 排除 artifact_sha256 后 SHA256）
    body = {k: v for k, v in doc.items() if k != "artifact_sha256"}
    assert hashlib.sha256(_canonical(body)).hexdigest() == doc["artifact_sha256"]

    # manifest 落盘且盘上内容与返回一致（含 self-hash）
    mf = out_dir / "PRETERMINAL_WINNER_PACKAGE.json"
    assert mf.is_file()
    on_disk = json.loads(mf.read_text(encoding="utf-8"))
    assert on_disk == doc

    # reload_probe.json 后补 → rebuild 后成员补收
    (out_dir / "reload_probe.json").write_text(json.dumps(
        {"schema": "predictor_v3_reload_probe_v1", "verdict": "PASS"}))
    doc2 = pk.build_preterminal_package("mlpq", out_dir=str(out_dir))
    assert "reload_probe" in [m["role"] for m in doc2["members"]]
    assert len(doc2["members"]) == len(doc["members"]) + 1


def test_build_preterminal_package_missing_artifact_raises(tmp_path, monkeypatch):
    """证据存在但 canonical 工件文件缺失 ⇒ raise（禁静默封装残缺包）。"""
    monkeypatch.setattr(pk, "MODELE_ROOT", str(tmp_path))
    monkeypatch.setattr(pk, "CONFIRM_DIR",
                        str(tmp_path / "results" / "predictor_v3" / "confirm"))
    monkeypatch.setattr(pk, "CANDIDATES_DIR",
                        str(tmp_path / "results" / "predictor_v3" / "candidates"))
    monkeypatch.setattr(pk, "WINNER_DIR",
                        str(tmp_path / "results" / "predictor_v3" / "winner"))
    ev_dir = tmp_path / "results" / "predictor_v3" / "confirm" / "mlpq"
    ev_dir.mkdir(parents=True)
    (ev_dir / "s0_evidence.json").write_text(json.dumps(
        {"artifact": "experiments/predictor_v3/confirm/mlpq/mlpq_s0.keras",
         "model_id": "mlpq", "config_sha256": "cfg-mlpq"}))
    with pytest.raises(FileNotFoundError):
        pk.build_preterminal_package("mlpq", out_dir=str(tmp_path))


# ---------------- ④ make_probe_input 形状 / float32 / 语义

def test_make_probe_input_shapes_and_dtype():
    hist = RNG.normal(0, 1, size=(70, 300, 11)).astype(np.float64)
    kf = RNG.normal(0, 1, size=(70, 30, 6)).astype(np.float64)
    p = pk.make_probe_input(hist, kf)
    assert p["hist_input"].shape == (pk.PROBE_WINDOWS, 300, 11)
    assert p["hist_input"].dtype == np.float32
    assert p["kf_input"].shape == (pk.PROBE_WINDOWS, 30, 6)
    assert p["kf_input"].dtype == np.float32
    # 前 64 窗切片语义（逐元素等于源数组前缀）
    assert np.array_equal(p["hist_input"], hist[:64].astype(np.float32))
    assert np.array_equal(p["kf_input"], kf[:64].astype(np.float32))
    # H1 面：kf=None → kf_input=None
    p2 = pk.make_probe_input(hist, None)
    assert p2["kf_input"] is None


def test_make_probe_input_rejects_insufficient_windows():
    hist = RNG.normal(0, 1, size=(10, 300, 11))
    with pytest.raises(ValueError):
        pk.make_probe_input(hist, None)
