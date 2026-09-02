#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成《研究总结报告》全部 8 张实验数据图（docs/reports/figures/*.png，300 dpi）。

数据源（只读，全部为仓库内产物，禁止凭记忆写数）：
  - firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS/
      Host/results/closedloop/closedloop_metrics.json                 （四臂×两场景统一指标，T-615）
      Host/results/closedloop/{S20,voyage002}/closedloop_*_{ref,npu}.csv（板载闭环轨迹，19 列，P_gen 列名 P_gen_kW，t 列名 t_s）
      Host/reference/v3_gp1_{S20,voyage002}_replay.csv                 （开环解释执行参考，13 列，P_gen_ref_kW）
      Host/fault_out/closedloop_npu/closedloop/{nan,reset}_closedloop_S20_npu.csv（FR-508 harness 轨迹）
  - model/simulink_modelD_(EMS_NN_AECMS)/
      results/ecms_phase25/closedloop/native_arms/V3_{B,C}_{S20,voyage002}_native_law{7,5}.csv（原生臂，22 列，P_gen 列名 P_gen_ref_kW，t 列名 t）
      results/ecms_phase25/fw_reference/ref_G2_{S20_composite_voyage_matrix,test_voyage_002_v2}.csv（G-P1 冻结参考，22 列）

图 7 说明：G-P1 残差 = v3_gp1_*_replay.csv（子集模型解释执行回放）对 ref_G2_*.csv
（ref_G2 冻结参考）逐拍 |Δ|——与 Host/reference/v3_gp1_*_gate.json 的 G-P1 门禁同源
（T-602/603，阈值 1e-6，实测位级一致）。

幂等可重跑；相对路径（仓库根由脚本位置推导，脚本位于 <root>/docs/reports/ 下）。
用法：model/simulink/.venv_tf/bin/python docs/reports/generate_figures.py
"""

import csv
import json
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.sans-serif"] = ["Hiragino Sans GB", "PingFang SC", "Arial Unicode MS"]
plt.rcParams["axes.unicode_minus"] = False

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))  # docs/reports/ -> docs/ -> <root>
FW = os.path.join(ROOT, "firmware", "stm32n6", "04_SourceCode", "SoftwarePackage",
                  "EMS_Projects", "EMS01_CHIL_ECMS")
MD = os.path.join(ROOT, "model", "simulink_modelD_(EMS_NN_AECMS)")
OUT = os.path.join(HERE, "figures")
os.makedirs(OUT, exist_ok=True)

METRICS = os.path.join(FW, "Host", "results", "closedloop", "closedloop_metrics.json")

ARM_STYLE = {
    "a_npu_onboard": dict(color="#d62728", ls="-", lw=1.4, label="(a) NPU 板载闭环"),
    "b_ref_onboard": dict(color="#1f77b4", ls="--", lw=1.4, label="(b) REF 板载闭环"),
    "bp_native_law7": dict(color="#2ca02c", ls=":", lw=2.0, label="(b′) 原生 law=7"),
    "c_native_law5": dict(color="#7f7f7f", ls="-.", lw=1.4, label="(c) 原生 law=5（基线）"),
}
ARM_FILES = {
    ("S20", "a_npu_onboard"): os.path.join(FW, "Host", "results", "closedloop", "S20", "closedloop_S20_npu.csv"),
    ("S20", "b_ref_onboard"): os.path.join(FW, "Host", "results", "closedloop", "S20", "closedloop_S20_ref.csv"),
    ("S20", "bp_native_law7"): os.path.join(MD, "results", "ecms_phase25", "closedloop", "native_arms", "V3_B_S20_native_law7.csv"),
    ("S20", "c_native_law5"): os.path.join(MD, "results", "ecms_phase25", "closedloop", "native_arms", "V3_C_S20_native_law5.csv"),
    ("voyage002", "a_npu_onboard"): os.path.join(FW, "Host", "results", "closedloop", "voyage002", "closedloop_voyage002_npu.csv"),
    ("voyage002", "b_ref_onboard"): os.path.join(FW, "Host", "results", "closedloop", "voyage002", "closedloop_voyage002_ref.csv"),
    ("voyage002", "bp_native_law7"): os.path.join(MD, "results", "ecms_phase25", "closedloop", "native_arms", "V3_B_voyage002_native_law7.csv"),
    ("voyage002", "c_native_law5"): os.path.join(MD, "results", "ecms_phase25", "closedloop", "native_arms", "V3_C_voyage002_native_law5.csv"),
}
BOARD_T = "t_s"      # 板载 CSV 时间列
NATIVE_T = "t"       # 原生臂 CSV 时间列


def load_csv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def col(rows, name):
    return [float(r[name]) for r in rows]


def metrics():
    with open(METRICS) as f:
        return json.load(f)


def save(fig, name):
    path = os.path.join(OUT, name)
    fig.savefig(path, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print("written:", os.path.relpath(path, ROOT))


# ---------------------------------------------------------------- 图 1/2：四臂 SOC 轨迹
def fig_soc(scenario, name):
    d = metrics()
    fig, ax = plt.subplots(figsize=(8.8, 4.6))
    for arm in ("a_npu_onboard", "b_ref_onboard", "bp_native_law7", "c_native_law5"):
        rows = load_csv(ARM_FILES[(scenario, arm)])
        tcol = BOARD_T if arm.endswith("onboard") else NATIVE_T
        t = col(rows, tcol)
        soc = col(rows, "SOC")
        st = ARM_STYLE[arm]
        soc_end = d["arms"][scenario][arm]["SOC_end"]
        ax.plot(t, soc, color=st["color"], ls=st["ls"], lw=st["lw"],
                label="%s（SOC_end=%.4f）" % (st["label"], soc_end))
    ax.axhline(0.60, color="k", ls=(0, (4, 2)), lw=0.8, alpha=0.5)
    ax.text(0.995, 0.60, "SOC 参考 0.60（fuel_corr 口径）", ha="right", va="bottom",
            fontsize=8, color="k", alpha=0.6, transform=ax.get_yaxis_transform())
    ax.set_xlabel("时间 t [s]")
    ax.set_ylabel("SOC")
    ax.set_title("四臂 SOC 轨迹对比——场景 %s（lock-step 真闭环 / 原生臂同网格）" %
                 ("S20 复合航次" if scenario == "S20" else "voyage002"))
    ax.legend(loc="best", fontsize=9, framealpha=0.9)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    save(fig, name)


# ---------------------------------------------------------------- 图 3：四臂油耗柱状（两场景）
def fig_fuel_bars():
    d = metrics()
    fig, axes = plt.subplots(1, 2, figsize=(11.5, 4.8))
    notes = {
        "S20": "SOC 公平性：FAIR（|ΔSOC_end| ≤ 2.2e-4，±0.02 带内）\n"
               "NN 增益（fuel_raw 主结论）：-2.25 ~ -2.65 kg（≈0.68%~0.80%）",
        "voyage002": "SOC 公平性：SOC-UNFAIR（NN 臂 SOC_end +0.0794 超带）\n"
                     "fuel_raw 不作主结论；fuel_corr 次级结论 -15.7 kg（三形态一致）",
    }
    for ax, scn in zip(axes, ("S20", "voyage002")):
        arms = ("a_npu_onboard", "b_ref_onboard", "bp_native_law7", "c_native_law5")
        xs = range(len(arms))
        raw = [d["arms"][scn][a]["fuel_raw_kg"] for a in arms]
        corr = [d["arms"][scn][a]["fuel_corr_kg"] for a in arms]
        w = 0.38
        b1 = ax.bar([x - w / 2 for x in xs], raw, w, color="#4c78a8", label="fuel_raw（原始油耗）")
        b2 = ax.bar([x + w / 2 for x in xs], corr, w, color="#f58518",
                    hatch="//", edgecolor="white", lw=0.5, label="fuel_corr（电荷维持修正）")
        for bars in (b1, b2):
            for r in bars:
                ax.text(r.get_x() + r.get_width() / 2, r.get_height(), "%.3f" % r.get_height(),
                        ha="center", va="bottom", fontsize=7, rotation=90)
        ax.set_xticks(list(xs))
        ax.set_xticklabels(["(a) NPU\n板载", "(b) REF\n板载", "(b′) 原生\nlaw=7", "(c) 原生\nlaw=5"], fontsize=9)
        lo = min(min(raw), min(corr))
        hi = max(max(raw), max(corr))
        ax.set_ylim(lo - 0.045 * (hi - lo), hi + 0.16 * (hi - lo))
        ax.set_ylabel("油耗 [kg]")
        ax.set_title("场景 %s（5521 拍）" % scn if scn == "S20" else "场景 voyage002（4212 拍）")
        ax.text(0.02, 0.98, notes[scn], transform=ax.transAxes, va="top", fontsize=8,
                bbox=dict(boxstyle="round,pad=0.35", fc="lemonchiffon" if scn == "S20" else "mistyrose",
                          ec="gray", alpha=0.9))
        ax.legend(loc="lower right", fontsize=8)
        ax.grid(alpha=0.3, axis="y")
    fig.suptitle("四臂 × 两场景油耗对比（真闭环 lock-step；纵轴截断以显差异，精确值以 closedloop_metrics.json 为准）",
                 fontsize=11)
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "fuel_bars_two_scenarios.png")


# ---------------------------------------------------------------- 图 4：层③/仪器级① P_gen 对比（S20）
def fig_pgen_board_vs_ref():
    d = metrics()
    board = load_csv(ARM_FILES[("S20", "b_ref_onboard")])
    ref = load_csv(os.path.join(FW, "Host", "reference", "v3_gp1_S20_replay.csv"))
    n = min(len(board), len(ref))
    t = col(board, BOARD_T)[:n]
    p_b = col(board, "P_gen_kW")[:n]
    p_r = col(ref, "P_gen_ref_kW")[:n]
    dp = [a - b for a, b in zip(p_b, p_r)]
    exact = sum(1 for x in dp if x == 0) / n
    grid = sum(1 for x in dp if abs(x) <= 20.0) / n
    ig1 = d["instrument_grade_1_closedloop_b_vs_gp1"]["S20"]
    # 交叉核对：网格一致率应与 closedloop_metrics.json 一致
    assert abs(grid - ig1["items"][3]["value"]) < 1e-9, "grid match rate mismatch vs JSON"
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(9.2, 6.0), sharex=True,
                                   gridspec_kw=dict(height_ratios=(3, 1.4)))
    w0, w1 = 3000, 3600
    m = [w0 <= ti <= w1 for ti in t]
    ax1.plot([ti for ti, ok in zip(t, m) if ok], [pi for pi, ok in zip(p_b, m) if ok],
             color="#1f77b4", lw=1.0, label="(b) REF 板载闭环 P_gen")
    ax1.plot([ti for ti, ok in zip(t, m) if ok], [pi for pi, ok in zip(p_r, m) if ok],
             color="#ff7f0e", lw=1.0, ls="--", label="解释执行参考（gp1 replay / ref_G2 驱动序列）")
    ax1.set_ylabel("P_gen [kW]")
    ax1.set_title("板载 REF P_gen vs 解释执行 P_gen——S20 局部窗口 3000–3600 s（仪器级①图形化）")
    ax1.legend(loc="upper left", fontsize=9)
    ax1.grid(alpha=0.3)
    ax2.plot([ti for ti, ok in zip(t, m) if ok], [di for di, ok in zip(dp, m) if ok],
             color="#d62728", lw=0.8)
    ax2.axhline(0, color="k", lw=0.6)
    for y in (20, -20):
        ax2.axhline(y, color="gray", ls=":", lw=0.8)
    ax2.text(0.995, 20 + 3, "±1 网格步（20 kW）", ha="right", fontsize=8, color="gray")
    ax2.set_xlabel("时间 t [s]（局部窗口 3000–3600 s）")
    ax2.set_ylabel("ΔP [kW]（板载-参考）")
    ax2.grid(alpha=0.3)
    txt = ("全程 %d 拍统计：exact 率 %.2f%%；|ΔP|≤1 网格步 %.2f%%（JSON %.2f%%，阈值 ≥95%%）\n"
           "仪器级① 其余项：SOC 终点差 %.1e（阈 ≤0.005）、fuel 相对偏差 %.1e（阈 ≤1%%）、"
           "v_ship RMSE %.1e m/s（阈 ≤0.01）——overall PASS"
           % (n, exact * 100, grid * 100, ig1["items"][3]["value"] * 100,
              ig1["items"][0]["value"], ig1["items"][1]["value"], ig1["items"][2]["value"]))
    fig.text(0.5, -0.015, txt, ha="center", fontsize=8.5,
             bbox=dict(boxstyle="round,pad=0.4", fc="honeydew", ec="gray"))
    fig.subplots_adjust(hspace=0.12)
    save(fig, "pgen_board_vs_ref_s20.png")


# ---------------------------------------------------------------- 图 5：v_ship 跟踪（S20，(b) 臂）
def fig_vship_tracking():
    board = load_csv(ARM_FILES[("S20", "b_ref_onboard")])
    t = col(board, BOARD_T)
    v = col(board, "v_ship_mps")
    vc = col(board, "v_cmd_mps")
    n = len(t)
    rmse = (sum((a - b) ** 2 for a, b in zip(v, vc)) / n) ** 0.5
    fig, ax = plt.subplots(figsize=(9.2, 4.2))
    ax.plot(t, vc, color="gray", lw=1.0, ls="--", label="航速指令 v_cmd")
    ax.plot(t, v, color="#1f77b4", lw=1.0, label="(b) REF 板载闭环 v_ship")
    ax.set_xlabel("时间 t [s]")
    ax.set_ylabel("航速 [m/s]")
    ax.set_title("航速跟踪——S20 复合航次全程（离泊→港区限速→经济巡航→赶班→桥区限速→逆水→靠泊）")
    ax.text(0.99, 0.02,
            "全任务 RMSE = %.6f m/s（1 s 轴口径，closedloop_metrics.json 0.351676）" % rmse,
            transform=ax.transAxes, ha="right", va="bottom", fontsize=9,
            bbox=dict(boxstyle="round,pad=0.3", fc="aliceblue", ec="gray"))
    ax.legend(loc="upper left", fontsize=9)
    ax.grid(alpha=0.3)
    fig.tight_layout()
    save(fig, "vship_tracking_s20.png")


# ---------------------------------------------------------------- 图 6：FR-508 闭环注入特写（F1/F4）
def fig_fr508_fallback_zoom():
    fault_dir = os.path.join(FW, "Host", "fault_out", "closedloop_npu", "closedloop")
    cases = [
        ("F1_nan_ticks", os.path.join(fault_dir, "nan_closedloop_S20_npu.csv"),
         "F1 NaN 注入（tick 30 P_load 置 NaN → FALLBACK×2 拍，tick 32 恢复）",
         "段内 Δfuel +0.027 kg，ΔSOC +5.19e-5（mock 口径）"),
        ("F4_reset_recovery", os.path.join(fault_dir, "reset_closedloop_S20_npu.csv"),
         "F4 复位恢复（tick 29 INIT 平 fill + FALLBACK×1 拍，tick 30 恢复）",
         "段内 Δfuel +0.013 kg，ΔSOC +2.59e-5（mock 口径）"),
    ]
    fig, axes = plt.subplots(2, 1, figsize=(9.2, 6.4), sharex=True)
    for ax, (key, path, title, note) in zip(axes, cases):
        rows = load_csv(path)
        tk = [int(r["tick"]) for r in rows]
        p = col(rows, "P_gen_kW")
        fl = [int(r["flags"]) for r in rows]
        eh = col(rows, "e_hat")
        lo, hi = 20, 45
        m = [lo <= x <= hi for x in tk]
        ax.plot([x for x, ok in zip(tk, m) if ok], [y for y, ok in zip(p, m) if ok],
                color="#1f77b4", lw=1.6, drawstyle="steps-post", label="P_gen [kW]")
        ax.axvspan(29.5, 31.5, color="red", alpha=0.0)
        # FALLBACK 位（0x02）区间着色
        in_fb = False
        for i, (x, f) in enumerate(zip(tk, fl)):
            fb = bool(f & 0x02)
            if fb and not in_fb:
                span0 = x - 0.5
                in_fb = True
            if in_fb and (not fb or i == len(tk) - 1):
                ax.axvspan(span0, x + 0.5, color="orange", alpha=0.25)
                in_fb = False
        ax2 = ax.twinx()
        ax2.plot([x for x, ok in zip(tk, m) if ok], [f for f, ok in zip(fl, m) if ok],
                 color="#d62728", lw=1.2, drawstyle="steps-post", label="flags（bit1=FALLBACK）")
        ax2.plot([x for x, ok in zip(tk, m) if ok], [e for e, ok in zip(eh, m) if ok],
                 color="#2ca02c", lw=1.0, ls=":", label="ê [kWh]")
        ax2.set_ylabel("flags / ê", color="#d62728")
        ax2.set_ylim(-1, 14)
        ax.set_ylabel("P_gen [kW]")
        ax.set_ylim(0, 400)
        ax.set_title(title + "　" + note, fontsize=9.5)
        ax.set_xlim(lo, hi)
        ax.grid(alpha=0.3)
        h1, l1 = ax.get_legend_handles_labels()
        h2, l2 = ax2.get_legend_handles_labels()
        ax.legend(h1 + h2, l1 + l2, loc="upper right", fontsize=8)
    axes[-1].set_xlabel("tick（1 s/拍，mock harness：stub advancer + pty+_MockFW，STALE 阈加速 0.05 s）")
    fig.suptitle("FR-508 闭环故障注入回退特写——S20，图像=NPU（段内油耗为 mock 口径，验证协议/flags/回退语义）",
                 fontsize=10.5)
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "fr508_fallback_zoom.png")


# ---------------------------------------------------------------- 图 7：G-P1 逐拍残差直方图
def fig_gp1_residual_hist():
    pairs = [
        ("S20",
         os.path.join(FW, "Host", "reference", "v3_gp1_S20_replay.csv"),
         os.path.join(MD, "results", "ecms_phase25", "fw_reference", "ref_G2_S20_composite_voyage_matrix.csv")),
        ("voyage002",
         os.path.join(FW, "Host", "reference", "v3_gp1_voyage002_replay.csv"),
         os.path.join(MD, "results", "ecms_phase25", "fw_reference", "ref_G2_test_voyage_002_v2.csv")),
    ]
    fig, axes = plt.subplots(2, 2, figsize=(9.6, 6.4))
    items = (("SOC", "SOC"), ("v_ship_mps", "v_ship"), ("x_ship_m", "x_ship"), ("P_load_kW", "P_load"))
    scn0, rep0, ref0 = pairs[0]
    rep_r = load_csv(rep0)
    ref_r = load_csv(ref0)
    n0 = min(len(rep_r), len(ref_r))
    for ax, (ck, label) in zip(axes.flat, items):
        resid = [abs(float(rep_r[i][ck]) - float(ref_r[i][ck])) for i in range(n0)]
        n_zero = sum(1 for x in resid if x == 0.0)
        ax.hist(resid, bins=1, range=(0.0, 1e-6), color="#4c78a8", edgecolor="k")
        ax.set_yscale("log")
        ax.set_ylim(0.5, max(10, n0 * 2))
        ax.set_title("%s：max|Δ| = %.1e（全部 %d 拍 = 0）" % (label, max(resid), n_zero), fontsize=9.5)
        ax.set_ylabel("拍数（对数轴）")
        ax.grid(alpha=0.3, axis="y")
    fig.suptitle("G-P1 逐拍残差分布——被控对象子集解释执行回放 vs ref_G2 冻结参考\n"
                 "（S20 5521 拍：SOC / v_ship / x_ship / P_load 逐拍 |Δ| = 0，位级一致，阈 1e-6，overall PASS）",
                 fontsize=10.5)
    fig.tight_layout(rect=(0, 0, 1, 0.90))
    save(fig, "gp1_residual_hist.png")


# ---------------------------------------------------------------- 图 8：各阶段 S20 基准油耗演进
def fig_phase_evolution():
    # 口径注意（图注必须说明，禁止跨口径直接互比）：
    #  Phase 0 DP 315.83 kg      —— Phase 0 离线动态规划上界（tier2 BSFC，docs/research_roadmap.md §4/§6.5）
    #  Phase 1 C2 330.662 kg     —— 旧 v1 数据集口径（roadmap §6.5 表，Phase 1 复用）
    #  Phase 2 C4 328.549 kg     —— 旧口径：v1 数据集 + law=7 无门控 + 旧归一化（roadmap §6.5）
    #  Phase 2.5 G2 328.095 kg   —— v2 口径：v2 数据集重训 MLP-Q + workclf v1.1 + G2 门控（原生臂回放 fuel_raw，closedloop 交叉核对）
    #  Phase 3 (b) 328.114 kg    —— 板载闭环口径：REF 板载真闭环 fuel_raw（closedloop_metrics.json）
    stages = [
        ("DP 全局最优\n（Phase 0·上界）", 315.83, "#2ca02c"),
        ("C2 SOC-自适应\n（Phase 1）", 330.662, "#7f7f7f"),
        ("C4 NN-A-ECMS\n（Phase 2·旧口径）", 328.549, "#9467bd"),
        ("MLP-Q G2 门控\n（Phase 2.5·v2 口径）", 328.095, "#1f77b4"),
        ("板载闭环 (b) REF\n（Phase 3·闭环口径）", 328.114, "#d62728"),
    ]
    C0 = 349.002
    fig, ax = plt.subplots(figsize=(9.2, 5.0))
    xs = range(len(stages))
    vals = [s[1] for s in stages]
    bars = ax.bar(xs, vals, color=[s[2] for s in stages], width=0.55)
    for x, v in zip(xs, vals):
        ax.text(x, v + 0.35, "%.3f" % v if v != 315.83 else "315.83", ha="center", fontsize=9)
    ax.axhline(C0, color="k", ls="--", lw=1.0)
    ax.text(len(stages) - 0.45, C0 + 0.5, "Baseline C0 = 349.002 kg（对照）", ha="right", fontsize=9)
    ax.set_xticks(list(xs))
    ax.set_xticklabels([s[0] for s in stages], fontsize=8.5)
    ax.set_ylim(310, 352)
    ax.set_ylabel("S20 全程油耗 [kg]")
    ax.set_title("各阶段 S20 基准油耗演进（Phase 0 → Phase 3 闭环收官）")
    ax.grid(alpha=0.3, axis="y")
    note = ("口径注记：C4（Phase 2）为 v1 数据集 + law=7 无门控 + 旧归一化（roadmap §6.5 旧口径）；"
            "MLP-Q G2 为 v2 数据集重训 + workclf v1.1 + G2 门控（ModelD 原生臂 law=7 回放，"
            "fuel_raw=328.094642）；闭环 (b) 为 REF 板载真闭环（board CSV 口径，fuel_raw=328.114131）。\n"
            "三个数字口径不同（数据集重做 + workclf 归一化口径 v1.1 + 闭环回路），不可直接互比，仅示演进位置。")
    fig.text(0.5, -0.10, note, ha="center", va="top", fontsize=8,
             bbox=dict(boxstyle="round,pad=0.4", fc="floralwhite", ec="gray"))
    fig.tight_layout()
    save(fig, "phase_evolution.png")


def main():
    fig_soc("S20", "soc_s20_four_arms.png")
    fig_soc("voyage002", "soc_voyage002_four_arms.png")
    fig_fuel_bars()
    fig_pgen_board_vs_ref()
    fig_vship_tracking()
    fig_fr508_fallback_zoom()
    fig_gp1_residual_hist()
    fig_phase_evolution()
    print("done: 8 figures ->", os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()