# -*- coding: utf-8 -*-
"""T-003 单测：OOD 窗口物化薄驱动（DGN-E-03，SEALED_PREPROCESS_ONLY）。

全部合成小航次（scipy savemat 构造 v7 结构），不触真实 OOD 语料。
覆盖（tasks T-003 Validation）：
- 双跑 canonical payload hash 一致（文件 sha256 另报）；
- per-condition 窗口数 > 0 + 计数与 FR-577 结构一致；
- schema 与 IID npz 平行（同名同形视图列；无 splits 列；有 ood_condition）；
- 负向：缺 realized 映射航次必须 FAIL；
- integrity JSON 只含 schema/计数/hash/元数据（禁科学分布字段的白名单断言）。
"""
import json
import os
import sys

import numpy as np
import pytest
from scipy.io import savemat

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import ood_materialize as om  # noqa: E402

RNG = np.random.default_rng(20260913)

H1_COLS = ["P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
           "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
           "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct"]
H2_ONLY = ["P_gen_actual_kW", "P_bat_actual_kW", "SOC", "V_dc_V", "state_DG1",
           "state_DG2", "V_bat_V", "I_bat_A", "P_bat_dis_avail_kW",
           "P_bat_chg_avail_kW"]
KF_COLS = ["v_plan_mps", "s_plan_m", "h_chart_m", "P_hotel_plan_kW",
           "u_fcst_mps", "wl_fcst_m"]

PROV = {"forecast_generator_id": "mission_v3_env_forecast",
        "forecast_generator_version": "v1",
        "forecast_generator_hash": "deadbeef",
        "issue_time_s": 0.0, "source_cutoff_time_s": 0.0,
        "forecast_provenance_class": "CAUSAL_GENERATOR"}


def _mk_voyage(path, voyage_id, split_group_id, dur_s=700.0):
    """合成单航次 v7 .mat（stream/kf/meta 三 struct，1 Hz native）。"""
    t = np.arange(0.0, dur_s, 1.0)
    n = t.size
    stream = {"time_s": t}
    for c in H1_COLS + H2_ONLY:
        stream[c] = RNG.normal(100.0, 10.0, size=n)
    stream["ctrl_mode"] = np.zeros(n)            # AUTO → manual cmd 置零语义
    stream["manual_power_cmd_pct"] = np.full(n, 50.0)
    stream["v_ref_mps"] = np.full(n, 4.0)
    # K/F snapshots：issue 时刻与窗口 anchor 对齐（300..dur-300 step 10）
    issues = np.arange(300.0, dur_s - 300.0 + 1e-9, 10.0)
    m = issues.size
    kf = {"issue_time_s": issues,
          "lead_s": np.tile(np.arange(10.0, 301.0, 10.0), (m, 1))}
    for c in KF_COLS:
        kf[c] = RNG.normal(5.0, 1.0, size=(m, 30))
    meta = {"voyage_id": voyage_id, "fixture_kind": "NONE",
            "split_group_id": split_group_id,
            "scenario_stratum": "STRAT_TEST", "forecast_provenance": PROV}
    savemat(path, {"stream": stream, "kf": kf, "meta": meta}, do_compression=True)


@pytest.fixture()
def ood_env(tmp_path):
    ood_dir = tmp_path / "ood"
    ood_dir.mkdir()
    _mk_voyage(str(ood_dir / "FLTB_OOD_A1_M1.mat"), "FLTB_OOD_A1_M1", "SG#G1")
    _mk_voyage(str(ood_dir / "FLTB_OOD_B1_M1.mat"), "FLTB_OOD_B1_M1", "SG#G2")
    _mk_voyage(str(ood_dir / "FLTB_OOD_B1_M2.mat"), "FLTB_OOD_B1_M2", "SG#G2")
    freeze = {"ood_evidence": {"voyage_evidence": [
        {"voyage_id": "FLTB_OOD_A1_M1", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "A1"},
        {"voyage_id": "FLTB_OOD_B1_M1", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "B1"},
        {"voyage_id": "FLTB_OOD_B1_M2", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "B1"},
    ]}}
    fr = tmp_path / "freeze_result.json"
    fr.write_text(json.dumps(freeze), encoding="utf-8")
    return {"ood_dir": str(ood_dir), "freeze": str(fr), "tmp": tmp_path}


def test_materialize_schema_counts_determinism(ood_env):
    npz1 = str(ood_env["tmp"] / "ood1.npz")
    npz2 = str(ood_env["tmp"] / "ood2.npz")
    s1 = om.materialize_ood(ood_env["ood_dir"], npz1, ood_env["freeze"])
    s2 = om.materialize_ood(ood_env["ood_dir"], npz2, ood_env["freeze"])

    assert s1["n_windows"] == s2["n_windows"] > 0
    assert s1["n_voyages"] == 3
    # per-condition 窗口数 > 0 + 航次/组计数
    pc = s1["per_condition"]
    assert pc["A1"]["voyages"] == 1 and pc["A1"]["split_groups"] == 1
    assert pc["B1"]["voyages"] == 2 and pc["B1"]["split_groups"] == 1
    assert pc["A1"]["windows"] > 0 and pc["B1"]["windows"] > 0

    # 双跑确定性：canonical payload hash 一致（文件 sha256 另报，可不同）
    h1 = om.scientific_payload_hash(npz1)
    h2 = om.scientific_payload_hash(npz2)
    assert h1 == h2
    f1 = om.sha256_file(npz1)
    assert f1 != "" and (f1 == om.sha256_file(npz2) or True)  # 另报不作门

    # schema 平行：视图列同名同形 + 无 splits + 有 ood_condition
    z = np.load(npz1)
    names = set(z.files)
    assert "splits" not in names
    assert {"Y", "E300", "voyage_id", "anchor_t0_s",
            "window_index_in_voyage", "ood_condition",
            "split_group_id"} <= names
    n = s1["n_windows"]
    assert z["Y"].shape == (n, 30) and z["E300"].shape == (n,)
    assert z["Y"].dtype == np.float32 and z["E300"].dtype == np.float32
    assert sum(1 for x in names if x.startswith("X_H1::")) == 11
    assert sum(1 for x in names if x.startswith("X_H2::")) == 10
    assert sum(1 for x in names if x.startswith("X_KF::")) == 6
    assert z["X_H1::v_ship_mps"].shape == (n, 300)
    assert z["X_KF::v_plan_mps"].shape == (n, 30)
    conds = z["ood_condition"]
    assert set(conds.tolist()) == {"A1", "B1"}
    # 无 K/F NaN（invalid=0 硬检查已在驱动内）
    assert np.isfinite(z["X_KF::v_plan_mps"]).all()


def test_negative_missing_mapping_fails(ood_env):
    # 缺映射航次：B1_M2 不在 realized 映射 → FAIL（DGN-E-03）
    freeze = {"ood_evidence": {"voyage_evidence": [
        {"voyage_id": "FLTB_OOD_A1_M1", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "A1"},
        {"voyage_id": "FLTB_OOD_B1_M1", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "B1"},
    ]}}
    fr = ood_env["tmp"] / "freeze_partial.json"
    fr.write_text(json.dumps(freeze), encoding="utf-8")
    with pytest.raises(ValueError, match="不一致"):
        om.materialize_ood(ood_env["ood_dir"],
                           str(ood_env["tmp"] / "x.npz"), str(fr))


def test_negative_unknown_condition_fails(ood_env):
    freeze = {"ood_evidence": {"voyage_evidence": [
        {"voyage_id": "FLTB_OOD_A1_M1", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "Z9"},
        {"voyage_id": "FLTB_OOD_B1_M1", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "B1"},
        {"voyage_id": "FLTB_OOD_B1_M2", "verdict": "OOD",
         "dataset_domain": "OOD", "ood_condition_id": "B1"},
    ]}}
    fr = ood_env["tmp"] / "freeze_badcond.json"
    fr.write_text(json.dumps(freeze), encoding="utf-8")
    with pytest.raises(ValueError, match="realized 条目非法"):
        om.load_realized_mapping(str(fr))


def test_integrity_whitelist_fields(ood_env):
    npz1 = str(ood_env["tmp"] / "ood1.npz")
    stats = om.materialize_ood(ood_env["ood_dir"], npz1, ood_env["freeze"])
    doc = om.build_integrity(stats, "cmd-test",
                             om.scientific_payload_hash(npz1),
                             freeze_result_path=ood_env["freeze"],
                             ood_dir=ood_env["ood_dir"], out_npz=npz1)
    blob = json.dumps(doc)
    # 白名单：schema/计数/hash/元数据在；科学分布字段禁现
    assert doc["counts"]["n_windows"] > 0
    assert doc["npz"]["scientific_payload_sha256"]
    assert doc["determinism"]["payload_hash_equal"] is True
    for banned in ("mean", "std", "quantile", "p01", "p99", "histogram",
                   "metrics", "mae", "rmse", "distribution"):
        assert f'"{banned}"' not in blob.lower(), f"integrity 含禁产字段 {banned}"
    # 自哈希复算一致
    body = {k: v for k, v in doc.items() if k != "artifact_sha256"}
    import hashlib
    assert hashlib.sha256(om.canonical_json(body)).hexdigest() == \
        doc["artifact_sha256"]
