# -*- coding: utf-8 -*-
"""build_segment_labels.py — T-308：从航次定义 JSON 展开生成器段型真值标签

标签定义：窗口 t（历史末端时刻）所属段型（7 类，1=depart ... 7=berth）。
验证：逐航次窗口计数与 fleet_windows_v2.npz 的 voyage_ids 计数一致。
输出：results/ecms_phase25/segment_labels/seg_labels_v2.npz
"""
import io, sys, os, json
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import scipy.io as sio

BASE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(BASE, "..")
SEG_ORDER = ["depart", "harbor", "eco", "rush", "bridge", "upstream", "berth"]
SEG_ID = {t: i + 1 for i, t in enumerate(SEG_ORDER)}

def voyage_labels(v):
    """展开逐秒段 id（0..D-1，0 基秒索引），并返回边界列表。"""
    D = int(v["duration_s"])
    seg_ids = np.zeros(D, dtype=np.int8)
    bounds = []
    t0 = 0
    for s in v["seg"]:
        dur = int(s["dur_s"])
        t1 = t0 + dur
        seg_ids[t0:t1] = SEG_ID[s["type"]]
        bounds.append((t0, t1, SEG_ID[s["type"]]))
        t0 = t1
    assert t0 == D, f"{v['name']}: 段总时长 {t0} != duration_s {D}"
    return seg_ids, bounds

def window_times(N):
    """与 build_dataset_v2.build_windows 逐行一致：anchors = np.arange(HIST-1, N-HORIZON, STEP)。"""
    return np.arange(299, N - 300, 10)

def main():
    j = json.load(open(os.path.join(ROOT, "results", "ecms_phase25", "fleet",
                                    "voyages_seed20260829.json"), encoding="utf-8"))
    vmap = {int(v["id"]): v for v in j["voyages"]}
    # N 以 .mat 实际采样数为准（Simulink 闭区间 0..duration_s → duration_s+1 采样）
    prof_dir = os.path.join(ROOT, "results", "ecms_phase25", "fleet_profiles")
    N_of = {}
    for vid, v in vmap.items():
        mp = os.path.join(prof_dir, "voyage_%03d.mat" % vid)
        mm = sio.loadmat(mp)
        N_of[vid] = int(len(mm["t_s"]))
    d = np.load(os.path.join(ROOT, "datasets", "fleet_windows_v2.npz"))
    out = {}
    total = 0
    for split in ("train", "val", "test"):
        vids = d[f"voyage_ids_{split}"]
        uniq, cnts = np.unique(vids, return_counts=True)
        labels = np.zeros(len(vids), dtype=np.int8)
        # 每航次窗口按 t 升序排列（与 build_dataset_v2 构建顺序一致），逐个核对计数
        pos = {int(v): 0 for v in uniq}
        for i, vid in enumerate(vids):
            vid = int(vid)
            v = vmap[vid]
            v = dict(v); v["duration_s"] = N_of[vid] - 1  # seg 展开按闭区间秒数
            seg_ids, _ = voyage_labels(v)
            ts = window_times(N_of[vid])
            k = pos[vid]
            assert k < len(ts), f"{v['name']}: 窗口数超界"
            t = ts[k]
            labels[i] = seg_ids[t]
            pos[vid] = k + 1
        # 逐航次计数核对
        for v_ in uniq:
            vid = int(v_)
            v = vmap[vid]
            n_expect = len(window_times(N_of[vid]))
            n_actual = int(np.sum(vids == v_))
            assert n_expect == n_actual, f"{v['name']}: 窗口数 {n_actual} != 期望 {n_expect}"
        out[f"seg_labels_{split}"] = labels
        out[f"voyage_ids_{split}"] = vids
        total += len(labels)
        u, c = np.unique(labels, return_counts=True)
        dist = {SEG_ORDER[u[i] - 1]: int(c[i]) for i in range(len(u))}
        print(f"{split}: {len(labels)} 窗口 | 分布 {dist}")
    assert total == 155087, f"总窗口数 {total} != 155087"
    out["seg_names"] = np.array(SEG_ORDER)
    outdir = os.path.join(ROOT, "results", "ecms_phase25", "segment_labels")
    os.makedirs(outdir, exist_ok=True)
    np.savez_compressed(os.path.join(outdir, "seg_labels_v2.npz"), **out)
    print("SAVED ->", outdir)

if __name__ == "__main__":
    main()
