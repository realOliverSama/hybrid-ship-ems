#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成《研究总结报告》建模章节的 Model D 顶层架构示意图
（docs/reports/figures/model/modeld_arch_schematic.png，300 dpi）。

内容为 HybridShip_ModelD_DCBus.slx 的 14 个子系统功率流/信号流关系，
模块语义以 scripts/+model/build_model_c.m 与 config/Model_Parameters.m 为准
（脚本驱动模型的唯一事实源；本图不含任何实验数据）。

幂等可重跑；相对路径（仓库根由脚本位置推导）。
用法：model/simulink/.venv_tf/bin/python docs/reports/generate_model_schematic.py
"""

import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

plt.rcParams["font.sans-serif"] = ["Hiragino Sans GB", "PingFang SC", "Arial Unicode MS"]
plt.rcParams["axes.unicode_minus"] = False

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "figures", "model")
os.makedirs(OUT, exist_ok=True)

C_POWER = "#1f5fa8"   # 功率/能量流
C_SIGNAL = "#666666"  # 控制/信号流
FC_POWER = "#dceaf7"
FC_CTRL = "#fdeeda"
FC_EMS = "#fcd9c8"
FC_BAT = "#dff0d8"
FC_MON = "#f2f2f2"


def box(ax, x, y, w, h, title, lines, fc, title_fs=9.5, line_fs=8.0):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.35",
                                fc=fc, ec="#444444", lw=1.1, zorder=2))
    ax.text(x + w / 2, y + h - 2.2, title, ha="center", va="center",
            fontsize=title_fs, fontweight="bold", zorder=3)
    if lines:
        ax.text(x + 1.2, y + h - 4.6, "\n".join(lines), ha="left", va="top",
                fontsize=line_fs, zorder=3, linespacing=1.45)


def poly(ax, pts, color, lw, ls, label=None, label_xy=None, label_fs=8.0):
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    ax.plot(xs[:-1], ys[:-1], color=color, lw=lw, ls=ls, zorder=1,
            solid_capstyle="round")
    ax.annotate("", xy=pts[-1], xytext=pts[-2],
                arrowprops=dict(arrowstyle="-|>", color=color, lw=lw,
                                linestyle=ls, shrinkA=0, shrinkB=0,
                                mutation_scale=13), zorder=1)
    if label:
        ax.text(label_xy[0], label_xy[1], label, fontsize=label_fs, color=color,
                ha="center", va="center", zorder=3,
                bbox=dict(fc="white", ec="none", alpha=0.85, pad=0.6))


def main():
    fig = plt.figure(figsize=(14.0, 7.4), dpi=300)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(-3, 141)
    ax.set_ylim(0, 80)
    ax.axis("off")

    # ---------------- 子系统框 ----------------
    box(ax, 4, 62, 26, 13, "01 工况管理", [
        "v_cmd(x/t 剖面)＋加减速爬坡",
        "水流 v_current、阻力倍率",
        "P_hotel、场景号（输入 x_ship）"], FC_CTRL)
    box(ax, 36, 62, 20, 13, "02 船速 PI", [
        "e = v_cmd - v_ship",
        "P_prop_demand（限幅抗饱和）"], FC_CTRL)
    box(ax, 64, 60, 32, 15, "05 EMS 控制器", [
        "Variant：law = 0/1/2/4/5/6/7",
        "ECMS：argmin [m_f(P_g)＋λ·P_bat]",
        "单步网格穷举 ≤200 点，1 s 控制拍",
        "输入 P_load/SOC/v/x/a/t → P_gen_ref"], FC_EMS)

    box(ax, 6, 40, 24, 13, "06 发电机组（均值）", [
        "2×500 kW 甲醇机组",
        "限幅 → 爬坡 40 kW/s → 一阶 5 s"], FC_POWER)
    box(ax, 42, 40, 18, 13, "13 直流母排", [
        "750 V，C = 40 F",
        "C·V·dV/dt = ΣP"], FC_POWER)
    box(ax, 70, 38, 34, 17, "03 推进与船体", [
        "逆变＋电机 2×350 kW（滞后 0.5 s）",
        "齿轮箱 6.08 / 0.96 → 轴系 J=1600",
        "螺旋桨（w=0.15，系泊点锚定 K_F/K_Q）",
        "船体 R=K_R·v^2，M = 1000 t"], FC_POWER)

    box(ax, 6, 22, 24, 10, "10 燃油消耗", [
        "BSFC(P_gen) 查表（双真实锚点）",
        "fuel = ∫ P·BSFC / 3.6e6 dt"], FC_MON)
    box(ax, 36, 16, 32, 13, "14 电池＋双向 DC/DC", [
        "1071.36 kWh，下垂并网（grid-forming）",
        "P = K_droop·(V_nom-V)，K = 10 kW/V",
        "SOC 积分＋充放电降额（0.25/0.90）"], FC_BAT)
    box(ax, 110, 40, 26, 10, "04 负荷测量", [
        "P_load = max(P_prop, 0)＋P_hotel"], FC_MON)
    box(ax, 110, 56, 26, 9, "08 欠压降额", [
        "V<675 V 线性降额（至 600 V）",
        "→ P_prop_limit"], FC_MON)

    box(ax, 6, 4, 26, 8, "09 母线残差监视", ["ΣP 残差（电气层一致性校验）"], FC_MON,
        title_fs=9, line_fs=7.5)
    box(ax, 38, 4, 26, 8, "15 母线电压监视", ["CCS 稳态/瞬态带报警（不干预）"], FC_MON,
        title_fs=9, line_fs=7.5)
    box(ax, 70, 4, 26, 8, "11 航程停止", ["x ≥ 航程 → Stop Simulation"], FC_MON,
        title_fs=9, line_fs=7.5)
    box(ax, 102, 4, 34, 8, "12 数据记录", ["logsout 信号日志 → 数据集/结果导出"], FC_MON,
        title_fs=9, line_fs=7.5)

    # ---------------- 功率流（实线） ----------------
    poly(ax, [(30, 46.5), (42, 46.5)], C_POWER, 2.4, "-",
         "P_gen·η_rect", (36, 48.3))
    poly(ax, [(60, 46.5), (70, 46.5)], C_POWER, 2.4, "-", "母排功率", (65, 48.3))
    poly(ax, [(50, 29), (50, 40)], C_POWER, 2.4, "-", "P_bat_bus（下垂双向）", (58, 34.5))
    poly(ax, [(49, 40), (49, 29)], C_POWER, 1.0, "-")
    poly(ax, [(18, 40), (18, 32)], C_POWER, 1.6, "-", "P_gen", (22.5, 36))

    # ---------------- 信号流（虚线） ----------------
    poly(ax, [(30, 68.5), (36, 68.5)], C_SIGNAL, 1.3, "--", "v_cmd", (33, 70.2))
    # 01 → 03：v_current、阻力倍率（左侧绕行）
    poly(ax, [(4, 66), (1, 66), (1, 35), (72, 35), (72, 38)], C_SIGNAL, 1.3, "--",
         "v_current、阻力倍率", (26, 33.2))
    # 02 → 03：P_prop_demand
    poly(ax, [(46, 62), (46, 58), (82, 58), (82, 55)], C_SIGNAL, 1.3, "--",
         "P_prop_demand", (63, 56.6))
    # 05 → 06：P_gen_ref
    poly(ax, [(68, 60), (68, 57), (18, 57), (18, 53)], C_SIGNAL, 1.3, "--",
         "P_gen_ref", (40, 58.4))
    # 14 → 05：SOC
    poly(ax, [(64, 29), (64, 57), (76, 57), (76, 60)], C_SIGNAL, 1.3, "--",
         "SOC", (60.5, 43))
    # 03 → 04 → 05：P_load
    poly(ax, [(104, 43), (110, 43)], C_SIGNAL, 1.3, "--", "P_prop", (107, 44.6))
    poly(ax, [(123, 50), (123, 66), (96, 66)], C_SIGNAL, 1.3, "--",
         "P_load_total", (110, 67.4))
    # 13 → 08：V_dc；08 → 03：P_prop_limit
    poly(ax, [(51, 53), (51, 58.5), (110, 58.5)], C_SIGNAL, 1.3, "--",
         "V_dc", (86, 59.9))
    poly(ax, [(117, 56), (117, 52), (104, 52)], C_SIGNAL, 1.3, "--",
         "P_prop_limit", (118.5, 53.6))
    # 13 → 09/15：V_dc、ΣP（左侧绕行）
    poly(ax, [(44, 40), (31, 40), (31, 13), (19, 13), (19, 12)], C_SIGNAL, 1.3, "--",
         "V_dc、ΣP", (30.5, 22))
    poly(ax, [(31, 13), (51, 13), (51, 12)], C_SIGNAL, 1.3, "--")
    # 03 → 11：x_ship
    poly(ax, [(87, 38), (87, 12)], C_SIGNAL, 1.3, "--", "x_ship", (91, 25))

    # ---------------- 图例 ----------------
    ax.plot([100, 107], [76.5, 76.5], color=C_POWER, lw=2.4)
    ax.text(108, 76.5, "功率/能量流", fontsize=8.5, va="center")
    ax.plot([100, 107], [73.5, 73.5], color=C_SIGNAL, lw=1.3, ls="--")
    ax.text(108, 73.5, "控制/信号流", fontsize=8.5, va="center")
    ax.text(100, 70.6, "反馈：x_ship→01、v_ship→02、\nSOC→05、V_dc→08/09/15（图中标出）",
            fontsize=7.5, va="top", color="#444444", linespacing=1.4)

    out = os.path.join(OUT, "modeld_arch_schematic.png")
    fig.savefig(out, dpi=300)
    plt.close(fig)
    print("written:", os.path.relpath(out, HERE))


if __name__ == "__main__":
    main()
