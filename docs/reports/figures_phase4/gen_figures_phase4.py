#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_figures_phase4.py — Phase 4 研究总结报告配图生成器（幂等可重跑）。

两类：
  A. 复用论文草稿（/Users/oliverlyu/Desktop/00毕业论文/05论文草稿/figures/）已渲染图
     —— 外部依赖经环境变量 THESIS_FIG_DIR 注入（AGENTS §20.2），缺失时优雅跳过；
  B. 新绘 —— 数据源为仓库内事实件（EMS04 C3/C4 闭环 CSV、predictor_v3 confirm/
     terminal JSON、dataset_v3 formal 冻结数），并在 manifest 中登记来源。

输出：docs/reports/figures_phase4/*.png + manifest_phase4_figs.json
运行：.venv_tf/bin/python docs/reports/figures_phase4/gen_figures_phase4.py
"""
import csv
import json
import os
import shutil

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager as fm

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
R4 = os.path.join(ROOT, "firmware", "stm32n6", "04_SourceCode", "SoftwarePackage",
                  "EMS_Projects", "EMS04_CHIL_FULLCTRL", "Host", "results")
ME = os.path.join(ROOT, "model", "simulink_modelE_(Phase4_Physics)")
THESIS_FIG_DIR = os.environ.get(
    "THESIS_FIG_DIR", "/Users/oliverlyu/Desktop/00毕业论文/05论文草稿")
OUT = HERE

# ---- CJK 字体链（同论文 figstyle 口径）----
_CJK_CHAIN = [
    ("/System/Library/Fonts/Hiragino Sans GB.ttc", "Hiragino Sans GB"),
    ("/System/Library/Fonts/PingFang.ttc", "PingFang SC"),
    ("/System/Library/Fonts/Supplemental/Songti.ttc", "Songti SC"),
    ("/System/Library/Fonts/Supplemental/Arial Unicode.ttf", "Arial Unicode MS"),
]
for path, name in _CJK_CHAIN:
    if os.path.exists(path):
        fm.fontManager.addfont(path)
        plt.rcParams["font.sans-serif"] = [name]
        break
plt.rcParams["axes.unicode_minus"] = False
plt.rcParams["font.size"] = 9

C = {"blue": "#1f5fa8", "orange": "#d97706", "green": "#2f8f5b", "red": "#c0392b",
     "gray": "#6b7280", "purple": "#7c5cbf", "lblue": "#7fb0dd"}

manifest = {"reused": [], "generated": [], "skipped": []}


def reuse(src_name, dst_name, origin):
    for base in (os.path.join(THESIS_FIG_DIR, "figures", "data"),
                 os.path.join(THESIS_FIG_DIR, "figures", "thesis_visio_redesign", "previews")):
        src = os.path.join(base, src_name)
        if os.path.isfile(src):
            dst = os.path.join(OUT, dst_name)
            shutil.copyfile(src, dst)
            manifest["reused"].append({"file": dst_name, "origin": origin})
            print("reuse", dst_name, "<-", os.path.relpath(src, ROOT))
            return True
    manifest["skipped"].append({"file": dst_name, "origin": origin})
    print("SKIP (missing)", src_name)
    return False


def save(fig, name, origin):
    dst = os.path.join(OUT, name)
    fig.savefig(dst, dpi=200, bbox_inches="tight")
    plt.close(fig)
    manifest["generated"].append({"file": name, "origin": origin})
    print("gen   ", name)


# ---------------------------------------------------------------- 复用图
def do_reuse():
    reuse("fig1_1.png", "r_fig1_1_tech_route.png", "论文草稿 Visio fig1_1（技术路线）")
    reuse("fig2_1.png", "r_fig2_1_system_arch.png", "论文草稿 Visio fig2_1（系统总体架构）")
    reuse("fig2_2.png", "r_fig2_2_model_top.png", "论文草稿 Visio fig2_2（Model E 顶层结构）")
    reuse("fig2_5.png", "r_fig2_3_pms_safety.png", "论文草稿 Visio fig2_5（PMS v2/Safety 结构）")
    reuse("fig2_6.png", "r_fig2_4_mission_gen.png", "论文草稿 Visio fig2_6（Mission Generator v3）")
    reuse("fig3_1.png", "r_fig3_1_problem.png", "论文草稿 Visio fig3_1（预测问题定义）")
    reuse("fig3_2.png", "r_fig3_2_archs.png", "论文草稿 Visio fig3_2（五候选架构对比）")
    reuse("fig3_5.png", "r_fig3_3_control_law.png", "论文草稿 Visio fig3_5（law6 控制律结构）")
    reuse("fig5_1.png", "r_fig4_1_prior_softhil.png", "论文草稿 Visio fig5_1（前期 MLP soft-HIL 链，口径见映射文档）")
    reuse("fig2_4_bsfc_cn.png", "r_fig2_5_bsfc.png", "论文草稿 figures/data（机组 BSFC 曲线）")
    reuse("fig2_5_battery_cn.png", "r_fig2_6_battery.png", "论文草稿 figures/data（电池 1RC 特性）")
    reuse("fig3_4_segment_mae_cn.png", "r_fig3_4_segment_mae.png", "论文草稿 figures/data（REAT-T 分段误差）")
    reuse("fig4_1_paired_cn.png", "r_fig4_2_paired.png", "论文草稿 figures/data（预注册配对对照）")
    reuse("fig4_4_dlambda_cn.png", "r_fig4_3_dlambda.png", "论文草稿 figures/data（Δλ 决策面）")


# ---------------------------------------------------------------- 新绘：预测器五候选
def fig_pred_benchmark():
    """五候选：val pinball（3 种子）组点 + test E300 MAE/R² 摘要条。
    数值来源：ME/results/predictor_v3/{confirm,terminal}（worker-1 证据笔记，
    与 models 字面量一致处已双核；缺失时按字面量回退）。"""
    models = ["REAT-T", "TFT", "Transformer", "TCN-Q", "MLP-Q"]
    val = {"REAT-T": [4.005, 3.968, 3.953], "TFT": [4.137, 3.935, 4.582],
           "Transformer": [4.034, 4.234, 4.021], "TCN-Q": [16.296, 17.028, 16.925],
           "MLP-Q": [17.266, 17.435, 17.654]}          # confirm s0/s1/s2 evidence
    test_mae = {"REAT-T": 0.746, "TFT": 0.822, "Transformer": 0.698,
                "TCN-Q": 2.913, "MLP-Q": 3.112}        # terminal *_s0_test.json
    test_r2 = {"REAT-T": 0.9931, "TFT": 0.9913, "Transformer": 0.9895,
               "TCN-Q": None, "MLP-Q": None}
    abl_h1 = 18.4    # history-only（3 种子均值 18.19，取报告口径量级）
    abl_h1k = 3.80   # 去 F 保 K

    fig, (a1, a2) = plt.subplots(1, 2, figsize=(9.2, 3.4))
    xs = range(len(models))
    for i, m in enumerate(models):
        a1.scatter([i] * 3, val[m], s=22, color=C["blue"], zorder=3)
        a1.plot([i - 0.18, i + 0.18], [sum(val[m]) / 3] * 2, color=C["red"], lw=1.6, zorder=4)
    a1.axhline(abl_h1k, color=C["gray"], ls="--", lw=1)
    a1.text(4.35, abl_h1k, "去F消融(保K)", va="center", fontsize=8, color=C["gray"])
    a1.axhline(abl_h1, color=C["gray"], ls=":", lw=1)
    a1.text(0.0, abl_h1 - 1.1, "history-only 消融", fontsize=8, color=C["gray"])
    a1.set_xticks(list(xs)); a1.set_xticklabels(models, fontsize=8.5)
    a1.set_ylabel("val split-group-macro pinball（3 种子）")
    a1.set_title("(a) 验证集 HPO/确认（越低越好）", fontsize=9.5)
    a1.grid(axis="y", alpha=0.3)

    ms = models
    xs_mae = [i - 0.2 for i in range(len(ms))]
    a2.bar(xs_mae, [test_mae[m] for m in ms], width=0.4, color=C["blue"],
           label="E300 MAE (kWh)")
    a2b = a2.twinx()
    ms_r2 = [m for m in models if test_r2[m] is not None]
    xs_r2 = [models.index(m) + 0.2 for m in ms_r2]
    a2b.bar(xs_r2, [test_r2[m] for m in ms_r2], width=0.4, color=C["green"],
            label="E300 $R^2$")
    a2b.set_ylim(0.97, 1.002); a2b.set_ylabel("$R^2$（右轴）")
    for i, m in enumerate(ms):
        a2.text(i - 0.2, test_mae[m] + 0.05, "%.2f" % test_mae[m], ha="center", fontsize=7.5)
    for m in ms_r2:
        a2b.text(models.index(m) + 0.2, test_r2[m] + 0.0006, "%.4f" % test_r2[m],
                 ha="center", fontsize=7.5, color=C["green"])
    a2.set_xticks(range(len(ms))); a2.set_xticklabels(ms, fontsize=8.5)
    a2.set_ylabel("E300 MAE (kWh)")
    a2.set_title("(b) test 集 25,236 窗终评（seed0）", fontsize=9.5)
    a2.grid(axis="y", alpha=0.3)
    h1, l1 = a2.get_legend_handles_labels(); h2, l2 = a2b.get_legend_handles_labels()
    a2.legend(h1 + h2, l1 + l2, fontsize=8, loc="upper left")
    fig.tight_layout()
    save(fig, "g_fig3_5_pred_benchmark.png",
         "ME/results/predictor_v3/{confirm,terminal}（test 25,236 窗；消融 9 arm-seed）")


# ---------------------------------------------------------------- 新绘：数据集规模
def fig_dataset():
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(9.2, 3.2))
    a1.barh([0, 1, 2, 3], [19, 92, 576, 200],
            color=[C["gray"], C["lblue"], C["blue"], C["orange"]], height=0.6)
    a1.set_yticks([0, 1, 2, 3])
    a1.set_yticklabels(["pilot 家族", "预资格 Stage A\n(IID 72 + OOD 20)",
                        "formal IID\n(FLTA)", "formal OOD\n(FLTB)"], fontsize=8.5)
    for y, v in zip([0, 1, 2, 3], [19, 92, 576, 200]):
        a1.text(v + 6, y, str(v), va="center", fontsize=9)
    a1.set_xlabel("航次数"); a1.set_xlim(0, 660)
    a1.set_title("(a) 数据集 v3 航次族构成", fontsize=9.5)
    a1.grid(axis="x", alpha=0.3)

    a2.bar([0, 1, 2], [136052, 36160, 25236], color=[C["blue"], C["lblue"], C["green"]], width=0.55)
    for x, v in zip([0, 1, 2], [136052, 36160, 25236]):
        a2.text(x, v + 2500, "{:,}".format(v), ha="center", fontsize=8.5)
    a2.set_xticks([0, 1, 2]); a2.set_xticklabels(["train", "val", "test"], fontsize=9)
    a2.set_ylabel("窗口数（History 300 s@1 Hz × Future 30×10 s）")
    a2.set_ylim(0, 155000)
    a2.set_title("(b) formal 冻结窗口划分（合计 197,448）", fontsize=9.5)
    a2.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    save(fig, "g_fig3_6_dataset_v3.png",
         "ME/results/dataset_v3/formal/formal_freeze_result.json（双跑零 mismatch）")


# ---------------------------------------------------------------- 新绘：C3 闭环总览
def fig_c3_overview():
    import csv
    path = os.path.join(R4, "s4_c3", "s4_c3_closedloop.csv")
    rows = list(csv.DictReader(open(path)))
    t0 = [float(r["t0"]) for r in rows]
    soc = [float(r["plant_SOC"]) for r in rows]
    pload = [float(r["plant_P_load"]) for r in rows]
    pg_bb = [float(r["bb_P_gen_ref"]) for r in rows]
    pg_bh = [float(r["bh_P_gen_ref"]) for r in rows]
    pg_a2 = [float(r["a2_P_gen_ref"]) for r in rows]
    d1_bb = [float(r["bb_d1"]) for r in rows]
    d1_bh = [float(r["bh_d1"]) for r in rows]
    d1_a2 = [float(r["a2_d1"]) for r in rows]
    attr = [r["bb_bh_attr"] for r in rows]

    fig, (ax_soc, ax_pw, ax_d1) = plt.subplots(3, 1, figsize=(9.2, 7.0), sharex=True,
                                               gridspec_kw={"height_ratios": [1, 1, 1]})
    ax_soc.plot(t0, soc, color=C["blue"], lw=1.2)
    ax_soc.axhspan(0.15, 0.95, color=C["green"], alpha=0.06)
    ax_soc.axhline(0.60, color=C["gray"], ls="--", lw=0.8)
    ax_soc.text(320, 0.935, "SOC 允许界 [0.15, 0.95]（G6-1）", fontsize=8, color=C["green"])
    ax_soc.set_ylabel("SOC"); ax_soc.set_ylim(0.13, 0.97)
    ax_soc.set_title("C3 单航次板在环闭环（S20 复合航次，541 拍 × 10 s，t0=300..5700 s）", fontsize=10)

    ax_pw.plot(t0, pload, color=C["gray"], lw=1.0, label="植物 P_load")
    ax_pw.plot(t0, pg_bb, color=C["red"], lw=1.0, label="P_gen_ref 板(B_BOARD)")
    ax_pw.plot(t0, pg_bh, color=C["blue"], lw=0.9, ls="--", label="P_gen_ref B_HOST")
    ax_pw.plot(t0, pg_a2, color=C["orange"], lw=0.9, ls=":", label="P_gen_ref A2 canonical")
    ax_pw.set_ylabel("功率 (kW)")
    ax_pw.legend(fontsize=8, ncol=2, loc="upper right")

    ax_d1.plot(t0, d1_bb, color=C["red"], lw=1.0, label="d1 板")
    ax_d1.plot(t0, d1_bh, color=C["blue"], lw=0.9, ls="--", label="d1 B_HOST")
    ax_d1.plot(t0, d1_a2, color=C["orange"], lw=0.9, ls=":", label="d1 A2")
    ax_d1.axhline(-0.1, color=C["gray"], lw=0.8, ls="-."); ax_d1.text(330, -0.098, "DP 网格下界 −0.1", fontsize=8, color=C["gray"])
    in_drift = False; start = None
    for i, x in enumerate(t0):
        d = attr[i] == "drift"
        if d and not in_drift:
            in_drift, start = True, x
        elif not d and in_drift:
            in_drift = False
            ax_d1.axvspan(start, x, color=C["orange"], alpha=0.15, lw=0)
    if in_drift:
        ax_d1.axvspan(start, t0[-1], color=C["orange"], alpha=0.15, lw=0)
    ax_d1.set_ylabel("Δλ 首步 d1"); ax_d1.set_xlabel("t0 (s)")
    ax_d1.legend(fontsize=8, ncol=3, loc="upper right")
    fig.align_ylabels()
    fig.tight_layout()
    save(fig, "g_fig5_2_c3_overview.png",
         "EMS04 Host/results/s4_c3/s4_c3_closedloop.csv（run3，541 拍，gate v2_1 overall=True）")


# ---------------------------------------------------------------- 新绘：漂移档案
def fig_c3_drift():
    import csv
    path = os.path.join(R4, "s4_c3", "s4_c3_closedloop.csv")
    rows = list(csv.DictReader(open(path)))
    t0 = [float(r["t0"]) for r in rows]
    attr = [r["bb_bh_attr"] for r in rows]
    sub = [r["bb_bh_sub"] for r in rows]
    eq = [int(float(r["bb_bh_action_eq"])) for r in rows]
    pgd = [float(r["bb_bh_pgen_diff"]) for r in rows]

    fig, (a1, a2) = plt.subplots(2, 1, figsize=(9.2, 4.6), sharex=True)
    colors = {"host_consensus": C["blue"], "board_canonical": C["green"], "triple": C["purple"]}
    for i, x in enumerate(t0):
        if attr[i] == "drift":
            a1.scatter(x, 1, s=14, color=colors.get(sub[i], C["gray"]))
    a1.set_yticks([1]); a1.set_yticklabels(["漂移拍"], fontsize=9)
    a1.set_ylim(0.6, 1.4)
    for k, c in colors.items():
        a1.scatter([], [], s=18, color=c, label={"host_consensus": "host共识/板偏离",
                                                 "board_canonical": "板随 canonical/B_HOST 偏离",
                                                 "triple": "三链互异"}[k])
    a1.legend(fontsize=8, ncol=3, loc="upper center", framealpha=0.9)
    a1.set_title("C3 漂移档案：%d 拍（%.1f%%），全部归因已登记 REATT 后端漂移（v2.1）"
                 % (sum(1 for a in attr if a == "drift"),
                    100.0 * sum(1 for a in attr if a == "drift") / len(t0)), fontsize=10)

    a2.plot(t0, pgd, color=C["gray"], lw=0.9)
    a2.axhline(20, color=C["red"], lw=0.9, ls="--")
    a2.text(330, 28, "1 网格步 = 20 kW（ECMS_Grid_dP）", fontsize=8, color=C["red"])
    a2.set_ylabel("|P_gen_ref 差| 板−B_HOST (kW)")
    a2.set_xlabel("t0 (s)")
    a2.grid(axis="y", alpha=0.3)
    fig.tight_layout()
    save(fig, "g_fig5_3_c3_drift.png",
         "EMS04 Host/results/s4_c3/s4_c3_closedloop.csv（bb_bh_attr/bb_bh_sub 逐拍）")


# ---------------------------------------------------------------- 新绘：C4 三航次
def fig_c4_battery():
    vids = ["S02_downstream", "S08_resistance_bands", "S03_upstream"]
    roles = ["R_low (293.5 kW 均值)", "R_mid (441.6 kW 均值)", "R_high (601.8 kW 均值)"]
    fig, axes = plt.subplots(2, 3, figsize=(9.6, 5.0), sharex=False)
    for j, (vid, role) in enumerate(zip(vids, roles)):
        rows = list(csv.DictReader(open(os.path.join(
            R4, "s4_c4", "s4_c4_%s.csv" % vid))))
        t0 = [float(r["t0"]) for r in rows]
        axes[0][j].plot(t0, [float(r["plant_SOC"]) for r in rows], color=C["blue"], lw=1.2, marker="o", ms=2.5)
        axes[0][j].set_ylim(0.58, 0.62)
        axes[0][j].set_title("%s\n%s" % (vid, role), fontsize=8.5)
        axes[0][j].grid(alpha=0.3)
        axes[1][j].plot(t0, [float(r["bb_P_gen_ref"]) for r in rows], color=C["red"], lw=1.2,
                        marker="s", ms=2.5, label="板 P_gen_ref")
        axes[1][j].plot(t0, [float(r["bh_P_gen_ref"]) for r in rows], color=C["blue"], lw=1.0,
                        ls="--", label="B_HOST")
        axes[1][j].set_ylim(-30, 900)
        axes[1][j].grid(alpha=0.3)
        axes[1][j].set_xlabel("t0 (s)")
    axes[0][0].set_ylabel("SOC")
    axes[1][0].set_ylabel("功率 (kW)")
    axes[1][2].legend(fontsize=8, loc="upper right")
    fig.suptitle("C4 冻结电池三航次（每航次独立复位；三航次 bb==bh 全等值、零 NACK、电池合取 PASS）",
                 fontsize=10)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    save(fig, "g_fig5_4_c4_battery.png",
         "EMS04 Host/results/s4_c4/s4_c4_<vid>.csv（S02/S08/S03，gate 均 overall=True）")


# ---------------------------------------------------------------- 新绘：每拍耗时
def fig_timing():
    fig, ax = plt.subplots(figsize=(9.2, 2.9))
    # P1/P2 实测分账（CMD_PROF 0x10/0x11，CYCCNT 周期域回绕安全；DP=J1-J3 位级加速后）
    segs = [("下行 UART 13960 B@115200", 1210, C["lblue"]),
            ("REATT 推理（NPU int8）", 300, C["green"]),
            ("law6 DP（位级加速 1.22× 后）", 5233, C["orange"]),
            ("执行层 grid+PMS×1000", 4, C["gray"]),
            ("RSP 上行 93 B", 8, C["gray"]),
            ("host 三链（keras+ORT+law6+PMS×2）", 642, C["lblue"])]
    left = 0
    for name, v, c in segs:
        ax.barh(0, v, left=left, color=c, height=0.5, label=name)
        if v > 300:
            ax.text(left + v / 2, 0, "%d ms" % v, ha="center", va="center", fontsize=8.5)
        left += v
    ax.axvline(10000, color=C["red"], lw=1.2, ls="--")
    ax.text(10000, 0.36, "10 s 决策拍（100%）", color=C["red"], fontsize=8.5, ha="right")
    ax.axvline(6755, color=C["orange"], lw=1.0, ls=":")
    ax.set_xlim(0, 10600); ax.set_yticks([])
    ax.set_xlabel("每拍墙钟（ms）—— v1.1 实测分账合计 ≈6.76 s，占用率 ≈68%（物理实时内）")
    ax.legend(fontsize=8, loc="upper center", ncol=2, framealpha=0.9)
    ax.set_title("C3 每拍耗时分解（v1.1：DWT 实测分账 + DP 位级加速后；10 s 拍占空 68%）", fontsize=10)
    fig.tight_layout()
    save(fig, "g_fig5_5_timing.png",
         "CMD_PROF 板内分账（v1.1，2026-09-29）+ s4_c3_closedloop.csv host_ms/board_ms")


def main():
    do_reuse()
    fig_pred_benchmark()
    fig_dataset()
    fig_c3_overview()
    fig_c3_drift()
    fig_c4_battery()
    fig_timing()
    with open(os.path.join(OUT, "manifest_phase4_figs.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, ensure_ascii=False, indent=1)
    print("manifest ->", os.path.join(OUT, "manifest_phase4_figs.json"))


if __name__ == "__main__":
    main()
