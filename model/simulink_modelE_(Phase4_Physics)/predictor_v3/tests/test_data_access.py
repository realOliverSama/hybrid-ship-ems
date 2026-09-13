# -*- coding: utf-8 -*-
"""T-002 单测：sealed 数据访问层与访问门（DGN-E-02）。

覆盖：
- 负向三形态必须 raise SealedAccessError（tasks T-002 Validation）：
  1) 未登记 campaign 访问 test/OOD；2) train API 越权 test；3) OOD 缺 ood=True；
- 六项核验逐项构造性正/反用例（DGN-E-02 ①~⑥）；
- campaign 生命周期五态 + INTERRUPTED fail-closed；
- per-run 访问日志分片字段齐全；
- DatasetView 懒加载切片/元数据正确（raw，无 transform）。

全部用合成小 npz + 临时目录，不触真实数据值（sealed 纪律）。
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

from predictor_v3 import data_access as da  # noqa: E402

H1_CH = [f"h1c{i}" for i in range(11)]
KF_CH = [f"kfc{i}" for i in range(6)]
H2_CH = [f"h2c{i}" for i in range(3)]

RNG = np.random.default_rng(20260913)


def _mk_iid_npz(path, n_per_split=7):
    """合成 IID npz：splits 列 + H1 11ch + H2 3ch + KF 6ch + Y/E300 + 元数据。"""
    n = n_per_split * 3
    splits = np.array(["train"] * n_per_split + ["val"] * n_per_split +
                      ["test"] * n_per_split)
    z = {
        "Y": RNG.normal(size=(n, 30)).astype(np.float32),
        "E300": RNG.normal(size=n).astype(np.float32),
        "splits": splits.astype("<U5"),
        "voyage_id": np.array([f"V{i:03d}" for i in range(n)]).astype("<U17"),
        "anchor_t0_s": np.arange(n, dtype=np.float64) * 10.0,
        "window_index_in_voyage": np.arange(n, dtype=np.int32),
    }
    for c in H1_CH:
        z[f"X_H1::{c}"] = RNG.normal(size=(n, 300)).astype(np.float32)
    for c in H2_CH:
        z[f"X_H2::{c}"] = RNG.normal(size=(n, 300)).astype(np.float32)
    for c in KF_CH:
        z[f"X_KF::{c}"] = RNG.normal(size=(n, 30)).astype(np.float32)
    np.savez_compressed(path, **z)
    return path


def _mk_ood_npz(path, n=9):
    """合成 OOD npz：无 splits 列，带 ood_condition（与 T-003 schema 平行）。"""
    z = {
        "Y": RNG.normal(size=(n, 30)).astype(np.float32),
        "E300": RNG.normal(size=n).astype(np.float32),
        "voyage_id": np.array([f"FLTB_{i:03d}" for i in range(n)]).astype("<U17"),
        "anchor_t0_s": np.arange(n, dtype=np.float64) * 10.0,
        "window_index_in_voyage": np.arange(n, dtype=np.int32),
        "ood_condition": np.array(["A1", "A1", "A1", "B1", "B1", "B1",
                                   "C1", "C1", "C1"]).astype("<U3"),
    }
    for c in H1_CH:
        z[f"X_H1::{c}"] = RNG.normal(size=(n, 300)).astype(np.float32)
    for c in H2_CH:
        z[f"X_H2::{c}"] = RNG.normal(size=(n, 300)).astype(np.float32)
    for c in KF_CH:
        z[f"X_KF::{c}"] = RNG.normal(size=(n, 30)).astype(np.float32)
    np.savez_compressed(path, **z)
    return path


def _mk_winner_package(root, identity="ident-abcd1234"):
    """合成 PRETERMINAL winner 包 manifest（只含核验所需字段）。"""
    wdir = os.path.join(root, "results", "predictor_v3", "winner")
    os.makedirs(wdir, exist_ok=True)
    manifest = {
        "schema": "predictor_v3_preterminal_manifest_test",
        "winner_identity_sha256": identity,
        "winner_selected_utc": "2026-09-13T00:00:00+00:00",
    }
    p = os.path.join(wdir, "PRETERMINAL_WINNER_PACKAGE.json")
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh)
    return p, identity


def _mk_eval_config(root):
    ec_dir = os.path.join(root, "config", "predictor_v3")
    os.makedirs(ec_dir, exist_ok=True)
    p = os.path.join(ec_dir, "eval_config.json")
    with open(p, "w", encoding="utf-8") as fh:
        json.dump({"schema": "predictor_v3_eval_config_test"}, fh)
    return p


def _mk_campaign(camp_dir, campaign_id, winner_manifest_path, identity,
                 eval_config_path, *, root, status="UNSEAL_AUTHORIZED",
                 authorized_utc="2026-09-13T12:00:00+00:00"):
    campaign = {
        "schema": "predictor_v3_terminal_campaign_test",
        "campaign_id": campaign_id,
        "status": status,
        # 正式语义：manifest 路径相对 MODELE_ROOT（DGN-E-16 仓库相对）；
        # 测试中 root 即模拟的 MODELE_ROOT。
        "preterminal_winner_manifest_path":
            os.path.relpath(winner_manifest_path, root),
        "preterminal_winner_artifact_sha256":
            da.file_sha256(winner_manifest_path),
        "winner_identity_sha256": identity,
        "authorized_utc": authorized_utc,
        "code_git_commit": da.current_git_commit(),
        "eval_config_sha256": da.file_sha256(eval_config_path),
    }
    campaign["artifact_sha256"] = da.self_hash(campaign)
    os.makedirs(camp_dir, exist_ok=True)
    p = os.path.join(camp_dir, f"{campaign_id}.json")
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(campaign, fh)
    return p


@pytest.fixture()
def env(tmp_path):
    """合成全套终评环境：IID/OOD npz + winner 包 + eval config + 合法 campaign。"""
    root = tmp_path
    iid = _mk_iid_npz(os.path.join(root, "iid.npz"))
    ood = _mk_ood_npz(os.path.join(root, "ood.npz"))
    wmp, identity = _mk_winner_package(root)
    ec = _mk_eval_config(root)
    camp_dir = os.path.join(root, "campaigns")
    cp = _mk_campaign(camp_dir, "camp-ok", wmp, identity, ec, root=root)
    acc = os.path.join(root, "access")
    return {"root": root, "iid": iid, "ood": ood, "wmp": wmp,
            "identity": identity, "ec": ec, "camp_dir": camp_dir,
            "camp_ok": "camp-ok", "acc": acc}


# ------------------------------------------------------------ 负向三形态

def test_negative_train_api_rejects_test(env):
    with pytest.raises(da.SealedAccessError):
        da.load_split("test", iid_npz_path=env["iid"], access_root=env["acc"],
                      log=False)


def test_negative_unregistered_campaign(env):
    with pytest.raises(da.SealedAccessError):
        da.load_terminal("test", "no-such-campaign", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"])
    with pytest.raises(da.SealedAccessError):
        da.load_terminal("ood", "no-such-campaign", ood=True,
                         ood_npz_path=env["ood"], campaign_dir=env["camp_dir"],
                         access_root=env["acc"])


def test_negative_ood_requires_explicit_flag(env):
    with pytest.raises(da.SealedAccessError):
        da.load_terminal("ood", env["camp_ok"], ood=False,
                         ood_npz_path=env["ood"], campaign_dir=env["camp_dir"],
                         access_root=env["acc"], modele_root=env["root"],
                         eval_config_path=env["ec"])


# ------------------------------------------------------------ load_split 正向

def test_load_split_train_val_ok(env):
    tr = da.load_split("train", iid_npz_path=env["iid"],
                       access_root=env["acc"])
    va = da.load_split("val", iid_npz_path=env["iid"], access_root=env["acc"])
    assert tr.n_windows == 7 and va.n_windows == 7
    assert tr.h1_channels == sorted(H1_CH) and len(tr.kf_channels) == 6
    w = tr.window(0)
    assert w["hist_raw"].shape == (300, 11) and w["kf_raw"].shape == (30, 6)
    assert w["y_raw"].shape == (30,)
    assert w["voyage_id"].startswith("V")
    assert tr.raw_arrays()["hist_raw"].shape == (7, 300, 11)
    assert tr.raw_arrays()["kf_raw"].shape == (7, 30, 6)


# ------------------------------------------------------------ 六项核验逐项

def _reload_campaign(env, campaign_id, mutate):
    """重写 campaign JSON（mutate(camp) 原地修改）并重算自哈希。"""
    p = os.path.join(env["camp_dir"], f"{campaign_id}.json")
    with open(p, encoding="utf-8") as fh:
        camp = json.load(fh)
    mutate(camp)
    camp["artifact_sha256"] = da.self_hash(camp)
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(camp, fh)
    return p


def test_check1_self_hash_mismatch(env):
    p = os.path.join(env["camp_dir"], "camp-ok.json")
    with open(p, encoding="utf-8") as fh:
        camp = json.load(fh)
    camp["status"] = "RUNNING"  # 篡改内容但保留旧自哈希 → ① FAIL
    with open(p, "w", encoding="utf-8") as fh:
        json.dump(camp, fh)
    with pytest.raises(da.SealedAccessError, match="核验①"):
        da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"])


def test_check2_winner_manifest_missing_or_hash(env):
    _reload_campaign(env, "camp-ok",
                     lambda c: c.update(preterminal_winner_manifest_path="nope.json"))
    with pytest.raises(da.SealedAccessError, match="核验②"):
        da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"])
    _reload_campaign(env, "camp-ok",
                     lambda c: c.update(preterminal_winner_artifact_sha256="0" * 64))
    with pytest.raises(da.SealedAccessError, match="核验②"):
        da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"])


def test_check3_winner_identity_mismatch(env):
    # 模拟 winner 包 manifest 被重写为新身份且 campaign 工件 hash 同步更新
    #（②过），但 campaign 冻结的 Winner 身份字段仍是旧值 → ③ FAIL。
    wmp, _ = _mk_winner_package(env["root"], identity="ident-CHANGED")

    def mutate(c):
        c["preterminal_winner_artifact_sha256"] = da.file_sha256(wmp)

    _reload_campaign(env, "camp-ok", mutate)
    with pytest.raises(da.SealedAccessError, match="核验③"):
        da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"],
                         modele_root=env["root"], eval_config_path=env["ec"])


def test_check4_timestamp_order(env):
    _reload_campaign(env, "camp-ok",
                     lambda c: c.update(authorized_utc="2026-09-12T00:00:00+00:00"))
    with pytest.raises(da.SealedAccessError, match="核验④"):
        da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"],
                         modele_root=env["root"], eval_config_path=env["ec"])


def test_check5_code_and_eval_config(env):
    _reload_campaign(env, "camp-ok",
                     lambda c: c.update(code_git_commit="deadbeef"))
    with pytest.raises(da.SealedAccessError, match="核验⑤"):
        da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"],
                         modele_root=env["root"], eval_config_path=env["ec"])
    _reload_campaign(env, "camp-ok",
                     lambda c: c.update(code_git_commit=da.current_git_commit(),
                                        eval_config_sha256="1" * 64))
    with pytest.raises(da.SealedAccessError, match="核验⑤"):
        da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                         campaign_dir=env["camp_dir"], access_root=env["acc"],
                         modele_root=env["root"], eval_config_path=env["ec"])


def test_check6_status_fail_closed(env):
    for bad in ("DRAFT", "REGISTERED_LOCKED", "INTERRUPTED", "SOMETHING_ELSE"):
        _reload_campaign(env, "camp-ok", lambda c, s=bad: c.update(status=s))
        with pytest.raises(da.SealedAccessError, match="核验⑥"):
            da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                             campaign_dir=env["camp_dir"],
                             access_root=env["acc"], modele_root=env["root"],
                             eval_config_path=env["ec"])


# ------------------------------------------------------------ 正向 + 日志

def test_terminal_test_ok_and_logged(env):
    view = da.load_terminal("test", "camp-ok", iid_npz_path=env["iid"],
                            campaign_dir=env["camp_dir"],
                            access_root=env["acc"], modele_root=env["root"],
                            eval_config_path=env["ec"])
    assert view.n_windows == 7
    # 访问日志分片在 dev 目录之外（campaign 目录），字段齐全
    camp_acc = os.path.join(env["acc"], "camp-ok")
    files = os.listdir(camp_acc)
    assert len(files) == 1 and files[0].endswith(".jsonl")
    entry = json.loads(open(os.path.join(camp_acc, files[0])).readline())
    for field in ("timestamp_utc", "split", "campaign_id", "run_id",
                  "caller", "git_commit"):
        assert field in entry
    assert entry["split"] == "test" and entry["campaign_id"] == "camp-ok"
    assert entry["note"] == "terminal_six_checks_passed"


def test_terminal_ood_ok(env):
    view = da.load_terminal("ood", "camp-ok", ood=True,
                            ood_npz_path=env["ood"],
                            campaign_dir=env["camp_dir"],
                            access_root=env["acc"], modele_root=env["root"],
                            eval_config_path=env["ec"])
    assert view.n_windows == 9
    assert view.ood_conditions.shape == (9,)
    w = view.window(0)
    assert w["ood_condition"] == "A1"


def test_lifecycle_single_direction():
    assert da.campaign_lifecycle_next("DRAFT") == "REGISTERED_LOCKED"
    assert da.campaign_lifecycle_next("REGISTERED_LOCKED") == "UNSEAL_AUTHORIZED"
    assert da.campaign_lifecycle_next("UNSEAL_AUTHORIZED") == "RUNNING"
    assert da.campaign_lifecycle_next("RUNNING") == "COMPLETE"
    with pytest.raises(ValueError):
        da.campaign_lifecycle_next("COMPLETE")
    with pytest.raises(ValueError):
        da.campaign_lifecycle_next("INTERRUPTED")


def test_dev_access_log_written(env):
    da.load_split("train", iid_npz_path=env["iid"], access_root=env["acc"])
    dev_dir = os.path.join(env["acc"], "dev")
    files = os.listdir(dev_dir)
    assert len(files) == 1
    entry = json.loads(open(os.path.join(dev_dir, files[0])).readline())
    assert entry["split"] == "train" and entry["campaign_id"] == "dev"
