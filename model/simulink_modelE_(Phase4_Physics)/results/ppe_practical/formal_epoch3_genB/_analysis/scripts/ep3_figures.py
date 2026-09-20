#!/usr/bin/env python3
"""EPOCH-3 genB thesis figures (descriptive, from committed analysis artifacts)."""
import json
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = "/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)"
RPP = os.path.join(ROOT, "results", "ppe_practical")
ANA = os.path.join(RPP, "formal_epoch3_genB", "_analysis")
FIG = os.path.join(ANA, "figures")
os.makedirs(FIG, exist_ok=True)
TIERS = ["LOW", "MID", "HIGH"]
TCOL = {"LOW": "#4C72B0", "MID": "#55A868", "HIGH": "#C44E52"}
ARM_C = {"P0": "#222222", "C0": "#8172B2", "C1": "#CCB974", "C2": "#64B5CD"}

cx = json.load(open(ANA + "/effect_analysis_context_epoch3.json"))
bs = json.load(open(ANA + "/effect_analysis_bootstrap_epoch3.json"))
sens = json.load(open(ANA + "/effect_analysis_fec_sensitivity_epoch3.json"))
saf = json.load(open(ANA + "/safety_descriptive_epoch3.json"))

# ---------- Fig 1: paired J_FEC deltas vs P0, per voyage, by tier ----------
rows = cx["P0_context"]["per_voyage"]
fig, axes = plt.subplots(1, 3, figsize=(11, 3.6), sharey=True)
for ax, t in zip(axes, TIERS):
    sub = sorted([r for r in rows if r["tier"] == t],
                 key=lambda r: r["DJ_C2_vs_P0_kg"])
    xs = np.arange(len(sub))
    for i, a in enumerate(("C0", "C1", "C2")):
        ax.scatter(xs + (i - 1) * 0.22,
                   [r["DJ_%s_vs_P0_kg" % a] for r in sub],
                   s=14, alpha=0.75, label=a, color=ARM_C[a])
    ax.axhline(0, color="k", lw=0.8)
    ax.set_title("%s (n=16)" % t)
    ax.set_xticks([])
axes[0].set_ylabel("paired $\\Delta$J_FEC vs P0  [kg]")
axes[1].set_xlabel("voyages (sorted by $\\Delta$J of C2)")
axes[0].legend(frameon=False, loc="lower right")
fig.suptitle("Per-voyage fuel-equivalent saving of predictive arms vs rule "
             "baseline P0  (negative = saving)", y=1.02, fontsize=11)
fig.tight_layout()
fig.savefig(FIG + "/fig1_paired_savings_vs_P0.png", dpi=200,
            bbox_inches="tight")
plt.close(fig)

# ---------- Fig 2: mean J_FEC per arm x tier + savings pct ----------
tj = cx["tier_arm_J_FEC"]
fig, ax = plt.subplots(figsize=(7.5, 3.8))
w = 0.2
for i, a in enumerate(("P0", "C0", "C1", "C2")):
    ax.bar(np.arange(3) + (i - 1.5) * w,
           [tj[t][a]["mean"] for t in TIERS], w, label=a, color=ARM_C[a],
           alpha=0.9)
for j, t in enumerate(TIERS):
    d = cx["P0_context"]["tier_descriptives"]["DPCT_C2_vs_P0"][t]
    ax.text(j, max(tj[t][a]["mean"] for a in tj[t]) + 3,
            "C2$-$P0: %.1f%%" % d["mean"], ha="center", fontsize=9)
ax.set_xticks(np.arange(3)); ax.set_xticklabels(TIERS)
ax.set_ylabel("mean J_FEC [kg / voyage]"); ax.legend(frameon=False, ncol=4)
ax.set_title("Tier-stratified mean fuel-equivalent consumption by arm")
fig.tight_layout(); fig.savefig(FIG + "/fig2_tier_arm_jfec.png", dpi=200)
plt.close(fig)

# ---------- Fig 3: k_eq sensitivity (linear in k) ----------
K_CANON = 0.5457
ks = np.linspace(0.8 * K_CANON, 1.2 * K_CANON, 50)
metrics = ["B_perfect", "B_predicted", "L_prediction"]
fig, axes = plt.subplots(1, 2, figsize=(10, 3.8))
for m in metrics:
    y0 = sens["sensitivity"]["canonical"]["theta"][m]["hat"]
    y08 = sens["sensitivity"]["0.8x"]["theta"][m]["hat"]
    slope = (y0 - y08) / (K_CANON - 0.8 * K_CANON)
    axes[0].plot(ks / K_CANON, y0 + slope * (ks - K_CANON), label=m)
axes[0].axhline(0, color="k", lw=0.8); axes[0].axvline(1.0, color="gray",
                                                       ls="--", lw=0.8)
axes[0].set_xlabel("$k_{eq}$  [$\\times$ canonical]")
axes[0].set_ylabel("study estimate [pp of $J_{C0}$]")
axes[0].set_title("Pre-registered estimands vs $k_{eq}$")
axes[0].legend(frameon=False, fontsize=8)
m = "DJ_C2_vs_P0_kg"
y0 = sens["sensitivity"]["canonical"]["theta"][m]["hat"]
y08 = sens["sensitivity"]["0.8x"]["theta"][m]["hat"]
slope = (y0 - y08) / (K_CANON - 0.8 * K_CANON)
axes[1].plot(ks / K_CANON, y0 + slope * (ks - K_CANON),
             color=ARM_C["C2"], label="C2 $-$ P0 [kg]")
be = sens["break_even_and_band_flip_audit"]["C2_vs_P0"]["study_k_break_even"]
axes[1].axvline(be / K_CANON, color="crimson", ls=":", lw=1.2)
axes[1].text(be / K_CANON + 0.01, y0 * 0.55,
             "break-even\n%.2fx canonical" % (be / K_CANON), fontsize=8,
             color="crimson")
axes[1].axhline(0, color="k", lw=0.8); axes[1].axvline(1.0, color="gray",
                                                       ls="--", lw=0.8)
axes[1].set_xlabel("$k_{eq}$  [$\\times$ canonical]")
axes[1].set_ylabel("study estimate [kg / voyage]")
axes[1].set_title("C2 vs P0 saving vs $k_{eq}$ (sign robust)")
axes[1].legend(frameon=False, fontsize=8)
fig.tight_layout(); fig.savefig(FIG + "/fig3_keq_sensitivity.png", dpi=200)
plt.close(fig)

# ---------- Fig 4: delta_lambda distribution per arm ----------
import glob
dl = {a: [] for a in ("C0", "C1", "C2")}
for ns in ("formal_epoch3_genB", "formal_epoch3"):
    for p in glob.glob(os.path.join(RPP, ns, "*", "*.ctrace.v1.json")):
        try:
            c = json.load(open(p))
        except Exception:
            continue
        if not c.get("applies"):
            continue
        arm = os.path.basename(p).split(".")[0]
        if arm in dl:
            dl[arm].extend(c["delta_lambda"])
fig, axes = plt.subplots(1, 3, figsize=(10.5, 3.2), sharey=True)
for ax, a in zip(axes, ("C0", "C1", "C2")):
    ax.hist(dl[a], bins=np.arange(-0.105, 0.1051, 0.005),
            color=ARM_C[a], edgecolor="white", lw=0.3)
    ax.set_title("%s  (n=%d decisions)" % (a, len(dl[a])))
    ax.set_xlabel("$\\Delta\\lambda$")
    lo_frac = float(np.mean(np.asarray(dl[a]) <= -0.10 + 1e-9))
    ax.text(-0.10, ax.get_ylim()[1] * 0.82, "%.0f%%\nat bound" % (100 * lo_frac),
            fontsize=8, ha="left")
axes[0].set_ylabel("decision count")
fig.suptitle("Equivalence-factor correction usage: saturation at the frozen "
             "lower bound $-0.10$", y=1.03, fontsize=11)
fig.tight_layout(); fig.savefig(FIG + "/fig4_delta_lambda_dist.png", dpi=200,
                                bbox_inches="tight")
plt.close(fig)

# ---------- Fig 5: safety descriptive (V_dc std / min V by arm) ----------
runs = saf["per_run"]
fig, axes = plt.subplots(1, 2, figsize=(10, 3.8))
for ax, m, ylab in ((axes[0], "vdc_std_V", "per-run std of $V_{dc}$ [V]"),
                    (axes[1], "vdc_min_V", "per-run min $V_{dc}$ [V]")):
    data = [[r[m] for r in runs if r["arm"] == a]
            for a in ("P0", "C0", "C1", "C2")]
    bp = ax.boxplot(data, tick_labels=("P0", "C0", "C1", "C2"), widths=0.55)
    for i, a in enumerate(("P0", "C0", "C1", "C2")):
        for med in bp["medians"]:
            pass
    ax.set_ylabel(ylab)
axes[1].axhline(675, color="crimson", ls="--", lw=1.2)
axes[1].text(0.55, 677, "propulsion derate threshold 675 V",
             color="crimson", fontsize=8)
axes[0].set_title("DC-bus voltage dispersion (C* arms slightly higher)")
axes[1].set_title("DC-bus voltage minimum (all arms above derate line)")
fig.tight_layout(); fig.savefig(FIG + "/fig5_safety_vdc.png", dpi=200)
plt.close(fig)

print("FIGURES_DONE:", sorted(os.listdir(FIG)))
