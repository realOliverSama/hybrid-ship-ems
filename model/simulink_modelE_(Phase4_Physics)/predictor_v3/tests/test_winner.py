# -*- coding: utf-8 -*-
"""T-017 单测：DGN-E-13 winner 算法（构造已知排序/等价/平局/UNDEFINED 用例）。"""
import json
import os
import sys

import numpy as np
import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import winner as wn  # noqa: E402
from predictor_v3.inference_stats import paired_bootstrap_rel_diff  # noqa: E402

RNG = np.random.default_rng(20260917)


def _cands(spec):
    """spec: {arch: (pinball 组值, mae 组值, ce80 组值)}——27 组等长。"""
    return {a: {"pinball": np.asarray(p, float), "mae": np.asarray(m, float),
                "ce80": np.asarray(c, float)} for a, (p, m, c) in spec.items()}


def test_stage1_clear_winner():
    """A 显著优于 B/C → S1 单元素，winner=A（stage=S1）。"""
    base = np.full(27, 100.0)
    cands = _cands({
        "A": (base, base, base * 0.01),
        "B": (base * 1.30, base * 1.3, base * 0.013),
        "C": (base * 1.60, base * 1.6, base * 0.016),
    })
    r = wn.select_winner(cands, params={"A": 1, "B": 2, "C": 3}, b=2000,
                         rng=np.random.default_rng(1))
    assert r["winner"] == "A" and r["stage"] == "S1_pinball"


def test_stage2_mae_tiebreak():
    """pinball 等价（全带内）→ 进 MAE 层：B 的 MAE 显著小 → winner=B。"""
    base = np.full(27, 100.0)
    cands = _cands({
        "A": (base, np.full(27, 50.0), base * 0.01),
        "B": (base * 1.001, np.full(27, 45.0), base * 0.01),
        "C": (base * 1.002, np.full(27, 52.0), base * 0.01),
    })
    r = wn.select_winner(cands, params={"A": 1, "B": 2, "C": 3}, b=2000,
                         rng=np.random.default_rng(2))
    assert r["winner"] == "B" and r["stage"] == "S2_mae"


def test_stage4_params_then_lex():
    """全指标等价 → 参数量最小者胜；参数量并列 → model_id 字典序（行政平局）。"""
    base = np.full(27, 100.0)
    cands = _cands({
        "bbb": (base, base * 0.5, base * 0.005),
        "aaa": (base, base * 0.5, base * 0.005),
        "ccc": (base, base * 0.5, base * 0.005),
    })
    r = wn.select_winner(cands, params={"aaa": 10, "bbb": 10, "ccc": 20},
                         b=2000, rng=np.random.default_rng(3))
    assert r["winner"] == "aaa" and r["stage"] == "S4_params_lex"
    # 参数量并列 aaa/bbb → 字典序 aaa < bbb ✓


def test_ce80_signed_diff_semantics():
    """R6：|CE80| 层用有符号差——CE80 一正一负且 |点估计| 相近时应判等价
    （先取 abs 再算 CI 会错判）。构造 A=+0.005、B=−0.005（|·|相等），
    两者 CE80 层互为等价 → 进入参数层。"""
    base = np.full(27, 100.0)
    cands = _cands({
        "A": (base, base * 0.5, np.full(27, 0.005)),
        "B": (base, base * 0.5, np.full(27, -0.005)),
        "C": (base, base * 0.5, np.full(27, 0.0)),
    })
    r = wn.select_winner(cands, params={"A": 5, "B": 5, "C": 5},
                         b=2000, rng=np.random.default_rng(4))
    # 三者 CE80 差 ≤0.01 全等价 → S4；参数并列 → 字典序 A
    assert r["winner"] == "A"


def test_undefined_fail_safe_not_equivalent():
    """分母非正 → UNDEFINED 保守不判等价（不进 S1；不影响 best 当选）。"""
    base = np.array([100.0] * 14 + [-100.0] * 13)
    cands = _cands({
        "A": (np.full(27, 100.0), np.full(27, 50.0), np.full(27, 0.01)),
        "B": (np.abs(base) * 1.001, np.full(27, 50.0), np.full(27, 0.01)),
    })
    # A 的 pinball 组值恒正（参照健康）；B 健康正数仅差 0.1% → 等价进 S1→S2→S3→
    # S4 参数量最小者 A 当选
    r = wn.select_winner(cands, params={"A": 3, "B": 9}, b=2000,
                         rng=np.random.default_rng(5))
    assert r["winner"] == "A"
    # 真 UNDEFINED：best = argmin 组均值 = B（负值拉低均值，mean(B)<0）；
    # A vs 参照 B 的点估计分母非正 ⇒ point=None ⇒ status=UNDEFINED（确定性，
    # 不依赖 rng）→ A 保守不判等价、不进 S1
    base2 = np.array([100.0] * 8 + [-100.0] * 19)
    cands2 = _cands({
        "A": (np.full(27, 100.0), np.full(27, 50.0), np.full(27, 0.01)),
        "B": (base2 * 1.001, np.full(27, 50.0), np.full(27, 0.01)),
    })
    chk = paired_bootstrap_rel_diff(cands2["A"]["pinball"],
                                    cands2["B"]["pinball"], b=2000,
                                    rng=np.random.default_rng(8))
    assert chk["status"] == "UNDEFINED"  # 分母非正 fail-safe
    r2 = wn.select_winner(cands2, params={"A": 3, "B": 9}, b=2000,
                          rng=np.random.default_rng(6))
    # A 不进 S1 → S1=[best=B] 单元素：best 依语义当选（UNDEFINED 不影响 best）
    assert r2["winner"] == "B" and r2["stage"] == "S1_pinball"


def test_preterminal_written_and_hashed(tmp_path, monkeypatch):
    """步骤 9：文件落盘 + 自哈希 + 身份字段（canonical_delivery_seed_index=0）。"""
    monkeypatch.setattr(wn, "WINNER_DIR", str(tmp_path))
    monkeypatch.setattr(wn, "CONFIRM_DIR", str(tmp_path / "confirm"))
    monkeypatch.setattr(wn, "CANDIDATES_DIR", str(tmp_path / "candidates"))
    # 伪造 confirm summary 供 val_evidence hash
    for c in wn.ARCH_ORDER:
        d = tmp_path / "confirm" / c
        d.mkdir(parents=True)
        (d / "summary.json").write_text("{}")
    base = np.full(27, 100.0)
    cands = _cands({"A": (base, base, base * 0.01),
                    "B": (base * 1.3, base * 1.3, base * 0.013)})
    r = wn.select_winner(cands, params={"A": 1, "B": 2}, b=1000,
                         rng=np.random.default_rng(7))
    sel = tmp_path / "candidates" / "A" / "selected_config.json"
    sel.parent.mkdir(parents=True)
    sel.write_text(json.dumps({"model_id": "mlpq", "config_sha256": "cfgA"}))
    doc = wn.write_preterminal(
        r, cands, {"A": 1, "B": 2},
        artifact_dir_map={"A": "experiments/x", "B": "experiments/y"},
        env_manifest_sha256="0" * 64)
    import hashlib
    path = tmp_path / "WINNER_SELECTED_PRETERMINAL.json"
    assert os.path.isfile(path)
    # 自哈希针对落盘文件验证（DGN-E-16：artifact_sha256 写入文件，不含在返回 doc 中）
    on_disk = json.loads(path.read_text(encoding="utf-8"))
    body = {k: v for k, v in on_disk.items() if k != "artifact_sha256"}
    assert hashlib.sha256(json.dumps(
        body, sort_keys=True, separators=(",", ":"),
        ensure_ascii=False).encode()).hexdigest() == on_disk["artifact_sha256"]
    assert doc["winner"]["canonical_delivery_seed_index"] == 0
    assert doc["winner_identity"] == {"model_id": "mlpq", "config_sha256": "cfgA"}
