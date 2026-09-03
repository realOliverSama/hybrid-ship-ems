#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""K-shape 等效覆盖度审计（Phase 2.5 T-302，FR-303）

依据：.specs/nn-predictive-ecms-phase25/t301_dataset_v2_perturbation.md
纯 numpy+scipy 实现（.venv_tf 无 sklearn/tslearn，禁止引入新依赖）：
  - SBD（shape-based distance，1 - max NCC），FFT 互相关实现（零填充 2n 覆盖全部平移）
  - PAM k-medoids（自实现）+ 手写轮廓系数，k 扫描选优
  - 在 v2 上拟合聚类，v1 航次按最近 medoid 分配 -> v1/v2 覆盖对比
  - 跨集最近邻分析（每个 v2 航次的最近 v1 航次 SBD 分布 + 反向，FR-316 前置预览）
  - 无 v1 成员的 v2 簇 = 覆盖缺口

用法：
  python audit_kshape_v2.py --v1-dir results/ecms_phase2/fleet_profiles \
      --v2-dir results/ecms_phase25/fleet_profiles --out results/ecms_phase25/audit
输出：kshape_audit_v2.json + kshape_audit_v2.md（中文报告）。
"""
import argparse
import glob
import io
import json
import os
import sys

import numpy as np
import scipy.io

# GBK 控制台防乱码（项目环境约束）
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')


# ---------------------------------------------------------------------------
# 距离与聚类
# ---------------------------------------------------------------------------
def zscore(x):
    """z 标准化；零方差序列返回全零。"""
    x = np.asarray(x, dtype=float)
    sd = x.std()
    if sd < 1e-12:
        return np.zeros_like(x)
    return (x - x.mean()) / sd


def resample_p(ts, p, npts):
    """P_demand_kW 均匀时间网格重采样到 npts 点（时间轴归一化，形状对齐用）。"""
    ts = np.asarray(ts, dtype=float).ravel()
    p = np.asarray(p, dtype=float).ravel()
    if ts.size < 2 or p.size < 2:
        return np.zeros(npts)
    t0, t1 = ts[0], ts[-1]
    if t1 - t0 < 1e-9:
        return np.zeros(npts)
    tg = np.linspace(t0, t1, npts)
    return np.interp(tg, ts, p)


def sbd(x, y):
    """Shape-based distance（Paparrizos & Gravano 2015）：1 - max NCC。
    零填充 2n 的 FFT 互相关覆盖全部平移量 s∈[-(n-1), n-1]。
    x/y 应为等长 z 标准化剖面。"""
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    n = x.size
    denom = np.linalg.norm(x) * np.linalg.norm(y)
    if denom < 1e-12:
        return 1.0
    xf = np.fft.fft(x, 2 * n)
    yf = np.fft.fft(y, 2 * n)
    ccs = np.fft.ifft(xf * np.conj(yf)).real
    ncc = ccs / denom
    return float(1.0 - ncc.max())


def sbd_matrix(profiles):
    """成对 SBD 距离矩阵（对称，对角 0）。"""
    n = len(profiles)
    D = np.zeros((n, n))
    for i in range(n):
        for j in range(i + 1, n):
            d = sbd(profiles[i], profiles[j])
            D[i, j] = d
            D[j, i] = d
    return D


def kmedoids_pam(D, k, max_iter=200, seed=42):
    """PAM k-medoids：固定随机初始化（确定性）；迭代 分配->更新 直至收敛。"""
    n = D.shape[0]
    rng = np.random.default_rng(seed)
    medoids = rng.choice(n, k, replace=False)
    for _ in range(max_iter):
        labels = np.argmin(D[:, medoids], axis=1)
        newm = medoids.copy()
        for c in range(k):
            members = np.where(labels == c)[0]
            if members.size == 0:
                continue
            dsub = D[np.ix_(members, members)]
            newm[c] = members[int(np.argmin(dsub.sum(axis=1)))]
        if np.array_equal(newm, medoids):
            break
        medoids = newm
    labels = np.argmin(D[:, medoids], axis=1)
    return medoids, labels


def silhouette_score(D, labels):
    """手写轮廓系数（mean over samples）。a=同簇均值距离（自排除），b=最近异簇均值距离。"""
    n = D.shape[0]
    k = int(labels.max()) + 1
    s = np.zeros(n)
    for i in range(n):
        li = int(labels[i])
        same = np.where(labels == li)[0]
        if same.size <= 1:
            s[i] = 0.0
            continue
        a = D[i, same].sum() / (same.size - 1)  # D[i,i]=0 -> 自排除
        b = None
        for c in range(k):
            if c == li:
                continue
            oth = np.where(labels == c)[0]
            if oth.size == 0:
                continue
            bc = D[i, oth].mean()
            if b is None or bc < b:
                b = bc
        if b is None:
            s[i] = 0.0
            continue
        s[i] = (b - a) / max(a, b)
    return float(s.mean())


# ---------------------------------------------------------------------------
# 数据装载
# ---------------------------------------------------------------------------
def load_profiles(vdir, npts):
    """读取 voyage_*.mat，跳过 rejected=true 与无效剖面。
    返回 (profiles, meta)：profiles 为 z 标准化重采样剖面列表；
    meta 含 file/voyage_id/name/p_peak/p_mean（原始 P_demand 峰值/均值）。"""
    files = sorted(glob.glob(os.path.join(vdir, 'voyage_*.mat')))
    profiles = []
    meta = []
    for fp in files:
        try:
            m = scipy.io.loadmat(fp, squeeze_me=False)
        except Exception:
            continue
        rej_a = np.asarray(m.get('rejected', np.array(True))).ravel()
        rej = bool(rej_a[0]) if rej_a.size else True
        if rej:
            continue
        t = np.atleast_1d(m.get('t_s', np.array([])))
        p = np.atleast_1d(m.get('P_demand_kW', np.array([])))
        if t.size < 10 or p.size < 10:
            continue
        vid_a = np.asarray(m.get('voyage_id', np.array(-1))).ravel()
        vid = int(vid_a[0]) if vid_a.size else -1
        name = m.get('name', None)
        if name is None:
            nm = '?'
        else:
            name = np.asarray(name)
            if name.ndim == 2 and name.size and name.dtype.kind in 'US':
                nm = ''.join(str(ch) for ch in name[0])
            else:
                nm = str(np.atleast_1d(name)[0])
        rp = zscore(resample_p(t, p, npts))
        profiles.append(rp)
        meta.append({'file': os.path.basename(fp),
                     'voyage_id': int(vid),
                     'name': nm,
                     'p_peak': float(np.max(p)),
                     'p_mean': float(np.mean(p))})
    return profiles, meta


# ---------------------------------------------------------------------------
# 主流程
# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description='K-shape 等效覆盖度审计（v1 vs v2）')
    ap.add_argument('--v1-dir', default='results/ecms_phase2/fleet_profiles',
                    help='v1 舰队剖面目录')
    ap.add_argument('--v2-dir', default='results/ecms_phase25/fleet_profiles',
                    help='v2 舰队剖面目录')
    ap.add_argument('--k-min', type=int, default=4)
    ap.add_argument('--k-max', type=int, default=8)
    ap.add_argument('--npts', type=int, default=256)
    ap.add_argument('--out', default='results/ecms_phase25/audit')
    args = ap.parse_args()

    print('[audit] 读取 v1: %s' % args.v1_dir)
    v1_prof, v1_meta = load_profiles(args.v1_dir, args.npts)
    print('[audit] 读取 v2: %s' % args.v2_dir)
    v2_prof, v2_meta = load_profiles(args.v2_dir, args.npts)
    print('[audit] v1=%d 个剖面, v2=%d 个剖面' % (len(v1_prof), len(v2_prof)))

    if len(v2_prof) < args.k_min:
        print('[audit] v2 剖面数不足 k-min，终止')
        sys.exit(2)

    kmax = min(args.k_max, len(v2_prof))

    # ---- k 扫描（在 v2 上拟合，手写轮廓系数选优）----
    D = sbd_matrix(v2_prof)
    best = None  # (k, score, medoids, labels)
    scans = []
    for k in range(args.k_min, kmax + 1):
        medoids, labels = kmedoids_pam(D, k)
        sc = silhouette_score(D, labels)
        scans.append({'k': k, 'silhouette': sc})
        print('[audit] k=%d 轮廓系数=%.4f' % (k, sc))
        if best is None or sc > best[1]:
            best = (k, sc, medoids, labels)

    k, sc, medoids, labels = best
    result_silhouette_scan = scans
    print('[audit] 最优 k=%d（轮廓系数 %.4f）' % (k, sc))

    # ---- v1 按最近 medoid 分配（覆盖对比）----
    v2_med_prof = [v2_prof[i] for i in medoids]
    v1_labels = np.array([int(np.argmin([sbd(p, mp) for mp in v2_med_prof])) for p in v1_prof])

    # ---- 簇统计（v2 侧：簇内/簇间距离来自 D）----
    n2 = len(v2_prof)
    clusters = []
    for c in range(k):
        members2 = np.where(labels == c)[0]
        members1 = np.where(v1_labels == c)[0]
        dsub = D[np.ix_(members2, members2)]
        intra = float(dsub.sum() / (len(members2) * (len(members2) - 1))) if len(members2) > 1 else 0.0
        med_d = float(np.mean([sbd(v2_prof[i], v2_med_prof[c]) for i in members2])) if len(members2) else 0.0
        p2_peak = float(np.mean([v2_meta[i]['p_peak'] for i in members2])) if len(members2) else 0.0
        p2_mean = float(np.mean([v2_meta[i]['p_mean'] for i in members2])) if len(members2) else 0.0
        clusters.append({
            'cluster': c,
            'n_v2': int(len(members2)),
            'n_v1': int(len(members1)),
            'intra_sbd_mean': intra,
            'dist_to_medoid_mean': med_d,
            'v2_p_peak_mean_kW': p2_peak,
            'v2_p_mean_mean_kW': p2_mean,
            'gap': bool(len(members1) == 0),
        })

    inter_med = []
    for a in range(k):
        row = []
        for b in range(k):
            row.append(sbd(v2_med_prof[a], v2_med_prof[b]))
        inter_med.append(row)

    # ---- 跨集最近邻（FR-316 前置预览）----
    v2_to_v1_nn = [float(min(sbd(p, q) for q in v1_prof)) for p in v2_prof] if v1_prof else []
    v1_to_v2_nn = [float(min(sbd(p, q) for q in v2_prof)) for p in v1_prof] if v2_prof else []

    def nn_stats(vals):
        if not vals:
            return {'min': None, 'mean': None, 'median': None, 'p90': None, 'max': None}
        a = np.array(vals)
        return {'min': float(a.min()), 'mean': float(a.mean()),
                'median': float(np.median(a)), 'p90': float(np.percentile(a, 90)),
                'max': float(a.max())}

    result = {
        'meta': {
            'v1_dir': args.v1_dir, 'v2_dir': args.v2_dir,
            'n_v1': len(v1_prof), 'n_v2': len(v2_prof),
            'npts': args.npts, 'k_min': args.k_min, 'k_max': kmax,
            'method': 'SBD + PAM k-medoids + hand-written silhouette (numpy/scipy only)',
        },
        'best_k': k,
        'best_silhouette': sc,
        'silhouette_scan': result_silhouette_scan,
        'clusters': clusters,
        'inter_medoid_sbd': inter_med,
        'cross_nn_v2_to_v1': nn_stats(v2_to_v1_nn),
        'cross_nn_v1_to_v2': nn_stats(v1_to_v2_nn),
        'gap_clusters': [c['cluster'] for c in clusters if c['gap']],
    }

    # ---- 轮廓扫描记录（已在上方扫描时填充）----

    # ---- 写出 ----
    os.makedirs(args.out, exist_ok=True)
    json_path = os.path.join(args.out, 'kshape_audit_v2.json')
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)

    md_path = os.path.join(args.out, 'kshape_audit_v2.md')
    with open(md_path, 'w', encoding='utf-8') as f:
        w = f.write
        w('# K-shape 等效覆盖度审计报告（v1 vs v2）\n\n')
        w('- 日期：%s\n' % '（运行时刻见文件 mtime）')
        w('- 方法：SBD（1−max NCC，FFT 互相关）距离 + PAM k-medoids + 手写轮廓系数\n')
        w('- 输入：v1=%d 剖面（%s），v2=%d 剖面（%s），重采样 %d 点后 z 标准化\n' % (
            len(v1_prof), args.v1_dir, len(v2_prof), args.v2_dir, args.npts))
        w('- 最优 k=%d，轮廓系数=%.4f\n' % (k, sc))
        w('\n## 轮廓系数扫描\n\n')
        w('| k | 轮廓系数 |\n|---|---|\n')
        for s_ in result_silhouette_scan:
            w('| %d | %.4f |\n' % (s_['k'], s_['silhouette']))
        w('\n## 簇统计（v2 拟合，v1 按最近 medoid 分配）\n\n')
        w('| 簇 | v2 成员 | v1 成员 | 簇内 SBD 均值 | 距 medoid 均值 | v2 P 峰值均值(kW) | v2 P 均值(kW) | 覆盖缺口 |\n')
        w('|---|---|---|---|---|---|---|---|\n')
        for c_ in clusters:
            w('| %d | %d | %d | %.4f | %.4f | %.1f | %.1f | %s |\n' % (
                c_['cluster'], c_['n_v2'], c_['n_v1'], c_['intra_sbd_mean'],
                c_['dist_to_medoid_mean'], c_['v2_p_peak_mean_kW'], c_['v2_p_mean_mean_kW'],
                '是' if c_['gap'] else ''))
        w('\n## 簇间 medoid SBD 矩阵\n\n')
        w('| medoid | ' + ' | '.join('m%d' % i for i in range(k)) + ' |\n')
        w('|---|' + '---|' * k + '\n')
        for a in range(k):
            w('| m%d | ' % a + ' | '.join('%.4f' % inter_med[a][b] for b in range(k)) + ' |\n')
        w('\n## 跨集最近邻（FR-316 近邻审计前置预览）\n\n')
        w('- v2→最近 v1：min=%.4f mean=%.4f median=%.4f p90=%.4f max=%.4f\n' % (
            result['cross_nn_v2_to_v1']['min'], result['cross_nn_v2_to_v1']['mean'],
            result['cross_nn_v2_to_v1']['median'], result['cross_nn_v2_to_v1']['p90'],
            result['cross_nn_v2_to_v1']['max']))
        w('- v1→最近 v2：min=%.4f mean=%.4f median=%.4f p90=%.4f max=%.4f\n' % (
            result['cross_nn_v1_to_v2']['min'], result['cross_nn_v1_to_v2']['mean'],
            result['cross_nn_v1_to_v2']['median'], result['cross_nn_v1_to_v2']['p90'],
            result['cross_nn_v1_to_v2']['max']))
        w('\n## 覆盖缺口\n\n')
        if result['gap_clusters']:
            w('以下 v2 簇无 v1 成员（v2 新增工况覆盖）：%s\n' % result['gap_clusters'])
        else:
            w('无（v1 覆盖了全部 v2 簇）。\n')

    print('[audit] 完成：%s / %s' % (json_path, md_path))


if __name__ == '__main__':
    main()
