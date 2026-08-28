#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_dataset_v2.py — Phase 2.5 数据集 v2 窗口化 + 泄漏断言 + 跨 split 近邻重复审计
================================================================================

Spec:    nn-predictive-ecms-phase25（T-303 / FR-302、FR-304、FR-316）
         t301_dataset_v2_perturbation.md（扰动机制 M0~M3）
输入:    results/ecms_phase25/fleet_profiles/voyage_001..400.mat
         （law=0 Baseline、零降额；字段 t_s / P_demand_kW / v_ship_mps /
           voyage_id / duration_s / law / rejected）
输出:
    datasets/fleet_windows_v2.npz    —— 窗口化 + 航次级 split + train 拟合 z-score
                                       后的 X/y（float32）+ 逐窗口 voyage_id
    datasets/fleet_norm_v2.json      —— 归一化参数（per-feature mean/std、y mean/std）
                                       + 公式说明（仅在 train 拟合，AI_MODELING §7）
    results/ecms_phase25/dataset_manifest_v2.json —— 窗口计数、split 名单（全 400
                                       航次）、归一化统计、泄漏检查结论、FR-316
                                       跨 split 近邻重复审计、输入 md5、生成时间

窗口定义（与 v1 build_dataset.py 逐行一致，Do Not Change）:
    步长 10 s；
    X(t) = [P_demand(t−299…t) 300 维（1 s 采样，时间升序）, v_ship(t), accel(t)]
           accel(t) = v_ship(t) − v_ship(t−1)（1 s 差分）
    y(t) = Σ_{s=t+1}^{t+300} P_demand(s) / 3600   [kWh]（未来 300 s 能量）
    丢弃历史不足 300 s 或未来不足 300 s 的端部窗口；窗口只在一个航次内构建，不跨航次。

Split（航次级，seed=20260829，N=400 比例 70/15/15 航次 = 280/60/60）:
    与 v1 同法：np.random.RandomState(seed).shuffle(1..400)，
    前 280 train / 中 60 val / 后 60 test；seed 与航次生成种子一致（20260829）。

归一化（AI_MODELING §7）:
    per-feature z-score（X 的 302 个特征各用各自 mean/std）、y z-score；
    统计量仅在 train split 拟合；val/test 只做 transform。

泄漏纪律（与 v1 一致，只增不减）:
    - 窗口不跨航次（按航次构建 + 断言）；
    - split 在航次级打乱，每个 voyage_id 恰好属于一个 split，train/val/test 互斥；
    - 归一化仅用 train；
    - S20/S01/S02/S07 与本数据集结构性无关（FR-204 沿革），manifest 中声明。

FR-316 跨 split 近邻重复审计（v2 新增）:
    - 对象：全部可用航次的 P_demand 剖面，线性重采样 256 点（resample_p）+ z 标准化
      （zscore），复用 python/audit_kshape_v2.py 的 sbd/resample_p/zscore（仅 import，
      不触发其 main()）；
    - 跨 split 最近邻：对每个航次，计算其到"其他 split"全部航次的最小 SBD；
    - 同 split 最近邻：对每个航次，计算其到"同 split 其他航次"的最小 SBD；
    - 判定规则（固定并写入 manifest）：跨 split 最近邻 SBD < 同 split 最近邻分布的
      p1 ⇒ 标记疑似模板泄漏；疑似对记录（voyage 对 id、split、SBD、处置）。
      处置默认 kept_declared（v2 速度/水流/阻力连续随机化使模板级完全相同不可能，
      但机制必须存在并留痕，不得静默忽略）。
    - 审计统计（跨 split NN p50/p90/min、同 split NN p1/p50、疑似对清单）写入
      manifest 的 near_duplicate_audit 节。

用法:
    cd model/simulink_modelD_(EMS_NN_AECMS) &&
      ../simulink/.venv_tf/Scripts/python.exe python/build_dataset_v2.py
"""

import datetime
import glob
import hashlib
import json
import os
import subprocess
import sys
import time

import numpy as np
from scipy.io import loadmat

# 同目录复用 audit_kshape_v2 的 sbd/resample_p/zscore（仅 import，不触发 main()）
_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)
from audit_kshape_v2 import zscore, resample_p, sbd  # noqa: E402

# ---------------------------------------------------------------- 配置
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROFILES_DIR = os.path.join(ROOT, "results", "ecms_phase25", "fleet_profiles")
RESULTS_DIR = os.path.join(ROOT, "results", "ecms_phase25")
DATASETS_DIR = os.path.join(ROOT, "datasets")

HIST = 300          # 历史窗口 [s]（P_demand 300 个 1 s 样本）
HORIZON = 300       # 预测时域 [s]
STEP = 10           # 窗口步长 [s]
N_FEATURES = HIST + 2        # 300 P + v_ship + accel
SPLIT_SEED = 20260829        # 航次级 split 打乱种子（与航次生成种子一致）
N_TRAIN, N_VAL, N_TEST = 280, 60, 60    # N=400 时 70/15/15%（与 v1 同比例）
N_VOYAGES = N_TRAIN + N_VAL + N_TEST    # = 400
SCRIPT_VERSION = "1.0.0"    # 数据集 v2 首版（T-303）
P_SAT_KW = 800.0            # P_demand 饱和上限（speed PI 饱和封顶，design §2）
Y_UPPER_KWH = P_SAT_KW * HORIZON / 3600.0   # ≈ 66.7 kWh
Y_LOWER_KWH = 100.0 * HORIZON / 3600.0      # ≈ 8.33 kWh（P_demand ≥ hotel 100 kW）
AUDIT_NPTS = 256            # FR-316 近邻审计重采样点数


def git_info():
    """只读 git 溯源信息（AI_MODELING §3），失败时记为 unknown。"""
    out = {"branch": "unknown", "commit_sha": "unknown"}
    for key, args in (("branch", ["rev-parse", "--abbrev-ref", "HEAD"]),
                      ("commit_sha", ["rev-parse", "HEAD"])):
        try:
            r = subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                               text=True, timeout=10)
            if r.returncode == 0:
                out[key] = r.stdout.strip()
        except Exception:
            pass
    return out


def md5_file(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_profiles():
    """读取并校验 N_VOYAGES 个航次剖面；跳过 rejected=true 并计数。

    返回 (profiles {voyage_id: dict}, inputs 文件清单, n_rejected)。
    数组访问一律 .ravel()（loadmat 对 1×N 行向量返回二维），标量用 ravel()[0] 或
    .item()（numpy 2.5 禁止对 ndim>0 数组直接 int()/bool()）。
    """
    profiles = {}
    inputs = []
    n_rejected = 0
    for vid in range(1, N_VOYAGES + 1):
        name = "voyage_%03d" % vid
        path = os.path.join(PROFILES_DIR, name + ".mat")
        assert os.path.isfile(path), "缺少输入剖面: %s" % path
        m = loadmat(path)
        t = np.asarray(m["t_s"], dtype=np.float64).ravel()
        P = np.asarray(m["P_demand_kW"], dtype=np.float64).ravel()
        v = np.asarray(m["v_ship_mps"], dtype=np.float64).ravel()
        fid = int(np.asarray(m["voyage_id"]).ravel()[0])
        dur = int(np.asarray(m["duration_s"]).ravel()[0])
        law = int(np.asarray(m["law"]).ravel()[0])
        rejected = int(np.asarray(m["rejected"]).ravel()[0])
        n_derate = int(np.asarray(m["n_derate_samples"]).ravel()[0])
        # ---- 字段与一致性校验（与 v1 同式）----
        assert fid == vid, "%s voyage_id=%d != %d" % (name, fid, vid)
        assert law == 0, "%s law=%d != 0（必须为 Baseline）" % (name, law)
        assert len(t) == dur + 1, "%s len(t)=%d != duration+1=%d" % (name, len(t), dur)
        assert t[0] == 0 and t[-1] == dur, "%s t 网格应为 0..%d" % (name, dur)
        assert np.all(np.diff(t) == 1), "%s t 网格须为 1 s 均匀" % name
        assert not (np.isnan(P).any() or np.isnan(v).any()), "%s 含 NaN" % name
        assert P.min() >= 100.0 and P.max() <= P_SAT_KW + 1e-6, \
            "%s P_demand 范围 %.2f..%.2f 超出 [100, 800]" % (name, P.min(), P.max())
        assert v.min() >= 0.0 and v.max() <= 6.0 + 1e-9, \
            "%s v_ship 范围 %.3f..%.3f 超出 [0, 6]" % (name, v.min(), v.max())
        rec = {
            "file": name + ".mat",
            "md5": md5_file(path),
            "size_bytes": os.path.getsize(path),
            "voyage_id": vid,
            "duration_s": dur,
            "law": law,
            "rejected": rejected,
            "n_derate_samples": n_derate,
        }
        inputs.append(rec)
        if rejected or n_derate > 0:
            n_rejected += 1
            continue    # 跳过 rejected/降额航次（本轮 0 个，机制保留留痕）
        profiles[vid] = {"t": t, "P": P, "v": v, "duration_s": dur}
    return profiles, inputs, n_rejected


def build_windows(prof):
    """单航次窗口化。返回 (X_raw (n,302), y_raw (n,), anchors (n,))。与 v1 逐行一致。"""
    P, v = prof["P"], prof["v"]
    N = len(P)
    # 有效锚点 t ∈ [299, N-301]（历史 t-299..t 完整 且 未来 t+1..t+300 完整）
    anchors = np.arange(HIST - 1, N - HORIZON, STEP)
    assert len(anchors) >= 1, "航次过短，无完整窗口"
    accel = np.empty_like(v)
    accel[0] = 0.0
    accel[1:] = np.diff(v)               # accel[i] = v[i] - v[i-1]
    idx_past = anchors[:, None] + np.arange(-(HIST - 1), 1)[None, :]   # (n,300)
    idx_fut = anchors[:, None] + np.arange(1, HORIZON + 1)[None, :]    # (n,300)
    X = np.empty((len(anchors), N_FEATURES), dtype=np.float64)
    X[:, :HIST] = P[idx_past]            # P(t-299..t)，时间升序
    X[:, HIST] = v[anchors]              # v_ship(t)
    X[:, HIST + 1] = accel[anchors]      # accel(t)
    y = P[idx_fut].sum(axis=1) / 3600.0  # Σ P(t+1..t+300)/3600 [kWh]
    assert not (np.isnan(X).any() or np.isnan(y).any())
    # 窗口不跨航次：idx 全部落在本航次 t 索引范围内
    assert idx_past.min() >= 0 and idx_fut.max() <= N - 1, "窗口越出航次边界"
    return X, y, anchors


def split_voyages():
    """航次级 split：seed 打乱 1..N_VOYAGES，前 N_TRAIN train / 中 N_VAL val / 后 N_TEST test。"""
    rng = np.random.RandomState(SPLIT_SEED)
    ids = np.arange(1, N_VOYAGES + 1)
    rng.shuffle(ids)
    tr = sorted(ids[:N_TRAIN].tolist())
    va = sorted(ids[N_TRAIN:N_TRAIN + N_VAL].tolist())
    te = sorted(ids[N_TRAIN + N_VAL:].tolist())
    assert len(tr) == N_TRAIN and len(va) == N_VAL and len(te) == N_TEST
    # 互斥 + 全覆盖断言
    s_tr, s_va, s_te = set(tr), set(va), set(te)
    assert s_tr & s_va == set() and s_tr & s_te == set() and s_va & s_te == set(), \
        "split 航次集合互斥断言失败"
    assert s_tr | s_va | s_te == set(range(1, N_VOYAGES + 1)), \
        "split 未覆盖全部 %d 个航次" % N_VOYAGES
    return {"train": tr, "val": va, "test": te}


def near_duplicate_audit(profiles, split):
    """FR-316 跨 split 近邻重复审计。

    对全部可用航次的 P_demand 剖面重采样 256 点 + z 标准化后：
      - 跨 split 最近邻 SBD（min over 其他 split）
      - 同 split 最近邻 SBD（min over 同 split 其他航次）
    判定：跨 split 最近邻 SBD < 同 split 最近邻分布的 p1 ⇒ 疑似模板泄漏。
    返回 (stats dict, flagged list)。
    """
    name_of_split = {"train": "train", "val": "val", "test": "test"}
    vid2split = {}
    for sp in ("train", "val", "test"):
        for vid in split[sp]:
            vid2split[vid] = sp
    # 重采样 + z 标准化（顺序固定 = vid 升序，可复现）
    ids = sorted(profiles.keys())
    profs = {}
    for vid in ids:
        p = profiles[vid]
        rp = zscore(resample_p(p["t"], p["P"], AUDIT_NPTS))
        profs[vid] = rp
    cross_nn = {}     # vid -> min SBD to other splits
    cross_arg = {}    # vid -> (other_vid, other_split)
    within_nn = {}    # vid -> min SBD within same split
    for vid in ids:
        sp = vid2split[vid]
        best_c, best_cv, best_csp = np.inf, -1, ""
        best_w, best_wv = np.inf, -1
        for oid in ids:
            if oid == vid:
                continue
            osp = vid2split[oid]
            d = sbd(profs[vid], profs[oid])
            if osp != sp:
                if d < best_c:
                    best_c, best_cv, best_csp = d, oid, osp
            else:
                if d < best_w:
                    best_w, best_wv = d, oid
        cross_nn[vid] = best_c
        cross_arg[vid] = (best_cv, best_csp)
        within_nn[vid] = best_w
    cross_arr = np.array([cross_nn[v] for v in ids])
    within_arr = np.array([within_nn[v] for v in ids])
    p1 = float(np.percentile(within_arr, 1))
    flagged = []
    for vid in ids:
        if cross_nn[vid] < p1:
            oid, osp = cross_arg[vid]
            flagged.append({
                "voyage": vid,
                "split": vid2split[vid],
                "nearest_other_voyage": oid,
                "nearest_other_split": osp,
                "sbd": round(float(cross_nn[vid]), 4),
                "threshold_p1": round(p1, 4),
                "disposition": "kept_declared",
                "disposition_reason": (
                    "v2 速度/水流/阻力连续随机化使模板级完全相同不可能；"
                    "近邻相似属同段型族合理相似，保留并声明"),
            })
    stats = {
        "method": (
            "P_demand 线性重采样 %d 点 + z 标准化（resample_p+zscore，复用 "
            "audit_kshape_v2），SBD（1-maxNCC）成对距离；"
            "跨 split 最近邻 = 该航次到其他 split 航次的最小 SBD；"
            "同 split 最近邻 = 该航次到同 split 其他航次的最小 SBD" % AUDIT_NPTS),
        "n_profiles": len(ids),
        "cross_split_nn": {
            "min": round(float(cross_arr.min()), 4),
            "p50": round(float(np.percentile(cross_arr, 50)), 4),
            "p90": round(float(np.percentile(cross_arr, 90)), 4),
        },
        "within_split_nn": {
            "p1": round(p1, 4),
            "p50": round(float(np.percentile(within_arr, 50)), 4),
        },
        "threshold_rule": (
            "跨 split 最近邻 SBD < 同 split 最近邻分布的 p1 ⇒ 疑似模板泄漏"),
        "n_flagged": len(flagged),
        "flagged": flagged,
        "disposition_default": "kept_declared",
        "conclusion": "PASS" if not flagged else "REVIEW",
    }
    return stats


def main():
    t0 = time.time()
    print("=" * 72)
    print("Phase 2.5 数据集 v2 窗口化 + 泄漏断言 + 近邻重复审计（T-303，FR-302/304/316）")
    print("=" * 72)

    profiles, inputs, n_rejected = load_profiles()
    print("[输入] 已加载并校验 %d 个可用航次剖面（跳过 rejected=%d）"
          % (len(profiles), n_rejected))
    assert len(profiles) == N_VOYAGES, \
        "可用剖面 %d != %d（存在 rejected/降额航次，需人工处置）" % (
            len(profiles), N_VOYAGES)

    # ---- 1. 窗口化（按航次）----
    windows = {}
    for vid, prof in profiles.items():
        X, y, anchors = build_windows(prof)
        windows[vid] = {"X": X, "y": y}
        expect_lo = (prof["duration_s"] - 2 * HIST) / STEP
        assert expect_lo <= len(anchors) <= expect_lo + 2.0, \
            "voyage %03d 窗口数 %d 超出启发式区间 [%.1f, %.1f]" % (
                vid, len(anchors), expect_lo, expect_lo + 2.0)
    n_per_voyage = {vid: len(w["y"]) for vid, w in windows.items()}
    total_windows = sum(n_per_voyage.values())
    print("[窗口] 总计 %d 个（%d 航次，步长 %d s）"
          % (total_windows, len(windows), STEP))

    # ---- 2. 航次级 split ----
    split = split_voyages()
    split_names = {"train": split["train"], "val": split["val"],
                   "test": split["test"]}
    print("[split] train=%d val=%d test=%d 航次（seed=%d）"
          % (len(split["train"]), len(split["val"]), len(split["test"]), SPLIT_SEED))

    # ---- 3. 组装 raw 数组 + 逐窗口 voyage_id ----
    raw = {}
    for name in ("train", "val", "test"):
        Xs, ys, ids = [], [], []
        for vid in split[name]:
            Xs.append(windows[vid]["X"])
            ys.append(windows[vid]["y"])
            ids.append(np.full(len(windows[vid]["y"]), vid, dtype=np.uint16))
        X = np.concatenate(Xs, axis=0)
        y = np.concatenate(ys, axis=0)
        vids = np.concatenate(ids, axis=0)
        assert len(X) == len(y) == len(vids)
        raw[name] = {"X": X, "y": y, "vids": vids}

    # ---- 4. 自检：物理范围 / y 分布（raw）----
    print("[自检] Ê_H 物理范围（%.3f ~ %.2f kWh）" % (Y_LOWER_KWH, Y_UPPER_KWH))
    y_all = np.concatenate([raw[n]["y"] for n in ("train", "val", "test")])
    assert y_all.min() >= 0.0 and y_all.max() <= Y_UPPER_KWH + 1e-6, \
        "y 超物理范围: min=%.3f max=%.3f" % (y_all.min(), y_all.max())
    assert y_all.min() >= Y_LOWER_KWH - 1e-6, \
        "y.min=%.3f 低于理论下界 %.3f" % (y_all.min(), Y_LOWER_KWH)
    print("    y: min=%.3f max=%.3f kWh（理论下界 %.3f，上界 %.3f）"
          % (y_all.min(), y_all.max(), Y_LOWER_KWH, Y_UPPER_KWH))

    y_stats = {}
    print("[自检] train/val/test 的 y（raw）分布摘要（同分布族对比）")
    for name in ("train", "val", "test"):
        yy = raw[name]["y"]
        st = {
            "n": int(len(yy)),
            "mean": float(yy.mean()),
            "std": float(yy.std()),
            "q25": float(np.percentile(yy, 25)),
            "q50": float(np.percentile(yy, 50)),
            "q75": float(np.percentile(yy, 75)),
            "min": float(yy.min()),
            "max": float(yy.max()),
        }
        y_stats[name] = st
        print("    %-5s n=%6d mean=%7.2f std=%6.2f q25=%6.2f q50=%6.2f q75=%6.2f"
              % (name, st["n"], st["mean"], st["std"], st["q25"], st["q50"], st["q75"]))
    # 相似性宽松断言（同分布族）：均值/中位数相互偏差 < 25%，std 比值 < 1.5
    means = [y_stats[n]["mean"] for n in ("train", "val", "test")]
    meds = [y_stats[n]["q50"] for n in ("train", "val", "test")]
    stds = [y_stats[n]["std"] for n in ("train", "val", "test")]
    assert max(means) / min(means) < 1.25, "y 均值差异过大: %s" % means
    assert max(meds) / min(meds) < 1.25, "y 中位数差异过大: %s" % meds
    assert max(stds) / min(stds) < 1.5, "y std 差异过大: %s" % stds
    print("[自检] y 分布同分布族断言通过")

    # ---- 5. 归一化（fit-on-train，AI_MODELING §7）----
    Xtr, ytr = raw["train"]["X"], raw["train"]["y"]
    x_mean = Xtr.mean(axis=0)
    x_std = Xtr.std(axis=0)
    y_mean = float(ytr.mean())
    y_std = float(ytr.std())
    assert x_std.min() > 1e-6, "存在零方差特征（std.min=%.3g），无法归一化" % x_std.min()
    assert y_std > 1e-6, "y 零方差"

    norm = {"X": {}, "y": {}}
    normed = {}
    for name in ("train", "val", "test"):
        Xn = (raw[name]["X"] - x_mean) / x_std
        yn = (raw[name]["y"] - y_mean) / y_std
        normed[name] = {"X": Xn.astype(np.float32),
                        "y": yn.astype(np.float32),
                        "vids": raw[name]["vids"]}
    print("[归一化] 参数仅在 train 拟合：x_mean/x_std（302 维）、y_mean=%.4f、y_std=%.4f"
          % (y_mean, y_std))

    # ---- 6. 自检：归一化后 train 的 X 均值≈0 / 标准差≈1（抽查 5 特征）----
    Xn_tr = normed["train"]["X"]
    chk_feats = [0, 75, 150, 225, 301]     # P 起/中/末、v、accel
    print("[自检] 归一化后 train X 抽查 5 特征（mean≈0, std≈1）")
    for fi in chk_feats:
        fm, fs = float(Xn_tr[:, fi].mean()), float(Xn_tr[:, fi].std())
        print("    feat %3d: mean=% .3e std=%.6f" % (fi, fm, fs))
        assert abs(fm) < 1e-6 and abs(fs - 1.0) < 1e-3, \
            "特征 %d 归一化自检失败" % fi
    allmax_abs_mean = float(np.abs(Xn_tr.mean(axis=0)).max())
    allmax_abs_std1 = float(np.abs(Xn_tr.std(axis=0) - 1.0).max())
    print("    全 302 特征：max|mean|=%.3e  max|std-1|=%.3e"
          % (allmax_abs_mean, allmax_abs_std1))
    # 存储为 float32，均值舍入噪声约 ~1e-5，故全局阈值取 1e-4（抽样 5 特征已用 1e-6 严格验证）
    assert allmax_abs_mean < 1e-4 and allmax_abs_std1 < 1e-3, \
        "train 全局归一化自检失败"

    # ---- 7. FR-316 跨 split 近邻重复审计 ----
    print("[FR-316] 跨 split 近邻重复审计（%d 点重采样 + SBD）..." % AUDIT_NPTS)
    nd_audit = near_duplicate_audit(profiles, split)
    print("    跨 split NN: min=%.4f p50=%.4f p90=%.4f | 同 split NN: p1=%.4f p50=%.4f"
          % (nd_audit["cross_split_nn"]["min"], nd_audit["cross_split_nn"]["p50"],
             nd_audit["cross_split_nn"]["p90"], nd_audit["within_split_nn"]["p1"],
             nd_audit["within_split_nn"]["p50"]))
    print("    疑似模板泄漏: %d 对 | 结论 %s"
          % (nd_audit["n_flagged"], nd_audit["conclusion"]))

    # ---- 8. 泄漏检查结论 ----
    leak = {
        "window_cross_voyage": False,     # 按航次构建，边界断言已过
        "voyage_in_multiple_splits": False,
        "every_voyage_in_exactly_one_split": True,
        "n_voyages_covered": N_VOYAGES,
        "n_rejected_skipped": n_rejected,
        "normalization_fit_train_only": True,
        "val_test_transform_only": True,
        "s20_s01_s02_s07_in_dataset": False,
        "future_info_in_features": False,   # 特征仅含 t 及更早信息
        "near_duplicate_cross_split": nd_audit["conclusion"],
        "structural_declaration": (
            "FR-302/FR-204 沿革：本数据集全部 %d 个航次（voyage_001..%03d）由 "
            "60_generate_voyages_v2.m + 61_generate_fleet_profiles_v2.m 生成"
            "（seed=20260829，results/ecms_phase25/fleet/voyages_seed20260829.json，"
            "law=0 Baseline 闭环仿真），扰动机制 M0~M3（t301 论证文档），与固定评估"
            "场景 S20/S01/S02/S07 无共享样本、无共享生成路径；S20/S01/S02/S07 数据"
            "未进入任何 split。段型速度/水流范围参照 S20 工况类型属设计知识复用，"
            "不构成数据泄漏。" % (N_VOYAGES, N_VOYAGES)),
        "conclusion": "PASS",
    }
    print("[泄漏检查] %s（FR-316 近邻审计 %s）"
          % (leak["conclusion"], nd_audit["conclusion"]))

    # ---- 9. 写出输出 ----
    os.makedirs(DATASETS_DIR, exist_ok=True)
    npz_path = os.path.join(DATASETS_DIR, "fleet_windows_v2.npz")
    np.savez_compressed(
        npz_path,
        X_train=normed["train"]["X"], y_train=normed["train"]["y"],
        X_val=normed["val"]["X"], y_val=normed["val"]["y"],
        X_test=normed["test"]["X"], y_test=normed["test"]["y"],
        voyage_ids_train=normed["train"]["vids"],
        voyage_ids_val=normed["val"]["vids"],
        voyage_ids_test=normed["test"]["vids"],
    )
    print("[输出] %s (%.1f MB)" % (npz_path, os.path.getsize(npz_path) / 1e6))

    norm_json = {
        "spec": "nn-predictive-ecms-phase25",
        "script": "python/build_dataset_v2.py",
        "script_version": SCRIPT_VERSION,
        "generated_utc": datetime.datetime.now(datetime.timezone.utc)
                        .isoformat(timespec="seconds"),
        "fit_on": "train split only（%d 航次，seed=%d）" % (N_TRAIN, SPLIT_SEED),
        "transform_formula": (
            "x_norm[f] = (x[f] - mean[f]) / std[f]；"
            "y_norm = (y - y_mean) / y_std；恢复: x = x_norm*std+mean, "
            "y = y_norm*y_std+y_mean。mean/std 仅由 train split 统计（AI_MODELING §7）。"
            "归一化常数为 float64 精度，本文件为原始精度（npz 内 float32 已量化）。"),
        "feature_layout": (
            "302 维 = [P_demand(t-299) .. P_demand(t)]（300 维，1 s 采样，时间升序）"
            "+ [v_ship(t)] + [accel(t)]，accel(t)=v_ship(t)-v_ship(t-1)"),
        "target_formula": (
            "y(t) = sum_{s=t+1..t+300} P_demand(s) / 3600 [kWh]"
            "（未来 300 s 能量积分，右端点 300 样本）"),
        "target_note": (
            "与 Phase 1 oracle E_future(t)=sum_{s=t..t+299} P_demand(s)/3600 差一个"
            "端点样本（|差|<=0.22 kWh），law=7 集成对齐口径时注意（Phase 2 沿革）。"),
        "X_mean": x_mean.tolist(),
        "X_std": x_std.tolist(),
        "y_mean": y_mean,
        "y_std": y_std,
        "n_train_windows": int(len(normed["train"]["y"])),
        "n_val_windows": int(len(normed["val"]["y"])),
        "n_test_windows": int(len(normed["test"]["y"])),
        "split_seed": SPLIT_SEED,
        "split_voyages": {
            "train": split["train"], "val": split["val"], "test": split["test"]},
    }
    norm_path = os.path.join(DATASETS_DIR, "fleet_norm_v2.json")
    with open(norm_path, "w", encoding="utf-8") as f:
        json.dump(norm_json, f, ensure_ascii=False, indent=2)
    print("[输出] %s" % norm_path)

    manifest = {
        "meta": {
            "task": "T-303",
            "spec": "nn-predictive-ecms-phase25",
            "requirement_ids": ["FR-302", "FR-304", "FR-316"],
            "script": "python/build_dataset_v2.py",
            "script_version": SCRIPT_VERSION,
            "git_branch": git_info()["branch"],
            "git_commit_sha": git_info()["commit_sha"],
            "generated_utc": datetime.datetime.now(datetime.timezone.utc)
                            .isoformat(timespec="seconds"),
            "elapsed_s": round(time.time() - t0, 2),
            "seed_split": SPLIT_SEED,
            "dataset_version": "v2",
            "dataset_generation_seed": 20260829,
        },
        "inputs": {
            "dir": "results/ecms_phase25/fleet_profiles",
            "n_files": len(inputs),
            "n_rejected_skipped": n_rejected,
            "provenance_note": (
                "全部 law=0 Baseline 闭环仿真；扰动 M0~M3（t301 文档）；n_derate=0、"
                "Vdc 健康（summary_seed20260829.json 已验收）；1 s 均匀网格；无 NaN"),
            "files": inputs,
        },
        "window": {
            "history_s": HIST,
            "horizon_s": HORIZON,
            "step_s": STEP,
            "feature_layout": (
                "[P_demand(t-299)..P_demand(t)] x300（1 s 采样，时间升序）"
                "+ v_ship(t) + accel(t)=v(t)-v(t-1) -> %d 维" % N_FEATURES),
            "target_formula": (
                "y(t) = sum_{s=t+1..t+300} P_demand(s)/3600 [kWh]"),
            "edge_rule": (
                "丢弃历史<300 s 或未来<300 s 的端部窗口；窗口在单航次内构建，"
                "不跨航次（断言已过）"),
            "total_windows": total_windows,
            "per_voyage_windows": n_per_voyage,
        },
        "split": {
            "method": (
                "航次级：np.random.RandomState(seed=%d).shuffle(1..%d)，"
                "前 %d train / 中 %d val / 后 %d test" % (
                    SPLIT_SEED, N_VOYAGES, N_TRAIN, N_VAL, N_TEST)),
            "ratio": "70/15/15（280/60/60，与 v1 同比例）",
            "ratio_history": [
                {"dataset_generation": "v1 N=200", "counts": "140/30/30",
                 "note": "design §3 原案比例 70/15/15%"},
                {"dataset_generation": "v2 N=400（T-303）", "counts": "280/60/60",
                 "note": "保持 70/15/15% 比例；seed=20260829（与航次生成种子一致，"
                         "沿 v1 惯例 SPLIT_SEED=生成种子）"},
            ],
            "counts": {"train": len(split["train"]), "val": len(split["val"]),
                       "test": len(split["test"])},
            "train_voyages": split["train"],
            "val_voyages": split["val"],
            "test_voyages": split["test"],
            "assertions": {
                "every_voyage_exactly_one_split": True,
                "splits_disjoint": True,
                "all_%d_covered" % N_VOYAGES: True,
            },
        },
        "window_counts": {
            "train": int(len(normed["train"]["y"])),
            "val": int(len(normed["val"]["y"])),
            "test": int(len(normed["test"]["y"])),
            "total": total_windows,
        },
        "raw_target_stats_kWh": y_stats,
        "distribution_note": (
            "y(Ê_H) 呈多峰分布（来源为物理结构：未来 300 s 内航段类型组合不同；"
            "v2 扰动 M1~M3 使 y 展宽约 ±25~35%）。精确分位数见 raw_target_stats_kWh；"
            "三 split 的同分布族断言（均值/中位数偏差<25%、std 比值<1.5）已在脚本内"
            "强制通过；z-score 归一化良定义（y_std>>0），不构成 escalate 条件。"),
        "normalization": {
            "fit_on": "train only（%d 航次，seed=%d）" % (N_TRAIN, SPLIT_SEED),
            "x_302_feat_max_abs_mean": allmax_abs_mean,
            "x_302_feat_max_abs_std_minus_1": allmax_abs_std1,
            "x_std_min": float(x_std.min()),
            "y_mean": y_mean,
            "y_std": y_std,
            "params_file": "datasets/fleet_norm_v2.json",
            "npz_contains": "z-score 后（train 拟合）的 X/y，float32",
        },
        "leakage_check": leak,
        "near_duplicate_audit": nd_audit,
        "outputs": {
            "npz": "datasets/fleet_windows_v2.npz",
            "norm_json": "datasets/fleet_norm_v2.json",
            "manifest": "results/ecms_phase25/dataset_manifest_v2.json",
        },
    }
    man_path = os.path.join(RESULTS_DIR, "dataset_manifest_v2.json")
    with open(man_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)
    print("[输出] %s" % man_path)

    # ---- 汇总 ----
    print("=" * 72)
    print("汇总：航次 %d（train %d / val %d / test %d）；窗口 %d"
          "（train %d / val %d / test %d）"
          % (N_VOYAGES, N_TRAIN, N_VAL, N_TEST,
             total_windows, len(normed["train"]["y"]),
             len(normed["val"]["y"]), len(normed["test"]["y"])))
    print("泄漏检查：%s | FR-316 近邻审计：%s（疑似 %d 对）"
          % (leak["conclusion"], nd_audit["conclusion"], nd_audit["n_flagged"]))
    print("耗时 %.1f s" % (time.time() - t0))
    print("=" * 72)
    return 0


if __name__ == "__main__":
    sys.exit(main())
