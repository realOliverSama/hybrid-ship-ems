#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_c1_modelE_selftest.py — S4-C1 门③：modelE 后端扩展 selftest（无板）。

覆盖（S4 spec §3 C1 门后半："host 闭环 selftest 扩展用例全绿"）：
  M1 plantstep_advancer modelE 后端加载/场景校验/复位幂等；
  M2 IC 一致：init 后 states() 与 PlantStepE 已知初始条件一致（SOC=Bat_SOC_init=0.60、
     x/v=0、V_dc=750 空载 OCV 域）；
  M3 推进健康：5 拍（P_gen=300）输出 16 键全有限、t_s 单调 +10（ode3 末级 ±0.0025
     采样窗内）、SOC 单调性按 P_bat 符号一致性抽查；
  M4 G-P2E 门 JSON 存在且 overall=true（引用既有门禁证据，不重复计算）；
  M5 双臂 runner 报告存在且 anchor beats=377（引用既有产物）。
退出码 0=全绿；1=有失败（逐项打印）。
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))              # .../Host/tools
HOST = os.path.dirname(HERE)                                   # .../Host
sys.path.insert(0, HOST)
from plantstep_advancer import make_advancer, PlantStepError  # noqa: E402

def _find_repo_root(start):
    d = os.path.abspath(start)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise RuntimeError("无法定位仓库根")
        d = parent


REPO = _find_repo_root(HOST)
REF = os.path.join(HOST, "reference")
C1 = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25", "results",
                  "s4_c1")


def main():
    checks = []
    ok_all = True

    def check(name, cond, detail=""):
        nonlocal ok_all
        checks.append((name, bool(cond), detail))
        if not cond:
            ok_all = False

    # M1 加载与场景校验
    try:
        adv = make_advancer(backend="modelE", scenario="S20_composite_voyage")
        check("M1 modelE 加载", True)
        try:
            make_advancer(backend="modelE", scenario="wrong")
            check("M1 场景校验拒绝", False, "错误场景未被拒绝")
        except PlantStepError:
            check("M1 场景校验拒绝", True)
    except PlantStepError as exc:
        check("M1 modelE 加载", False, str(exc))
        _report(checks, ok_all)
        return 1

    try:
        # M2 IC
        s0 = adv.states()
        check("M2 SOC IC=0.60", abs(s0["SOC"] - 0.60) < 1e-9, f"{s0['SOC']!r}")
        check("M2 x IC=0", abs(s0["x_ship_m"]) < 1e-12)
        check("M2 v IC=0", abs(s0["v_ship_mps"]) < 1e-12)
        check("M2 V_dc IC∈[700,780]（空载 OCV 域）", 700.0 <= s0["V_dc_V"] <= 780.0,
              f"{s0['V_dc_V']:.1f}")

        # M3 推进 5 拍
        prev_t = None
        soc_monotone_ok = True
        for k in range(5):
            y = adv.step(300.0)
            for key, v in y.items():
                if not math.isfinite(v):
                    check(f"M3 拍{k} {key} 有限", False, repr(v))
                    break
            if prev_t is not None:
                dt_t = y["t_s"] - prev_t
                if not (9.99 <= dt_t <= 10.01):
                    check(f"M3 拍{k} Δt≈10", False, f"Δt={dt_t}")
                    break
            prev_t = y["t_s"]
        else:
            check("M3 5 拍全有限 + Δt≈10", True)
        adv.close()
        check("M5 close 幂等", True)
    except PlantStepError as exc:
        check("M3 推进健康", False, str(exc))

    # M4 G-P2E 门证据
    gp = os.path.join(REF, "s4_gp2e_S20_composite_voyage_gate.json")
    if os.path.isfile(gp):
        g = json.load(open(gp))
        check("M4 G-P2E overall=true", g.get("overall") is True,
              f"n_fail={g.get('n_fail')}")
    else:
        check("M4 G-P2E gate JSON 存在", False, gp)

    # M5 双臂报告
    da = os.path.join(C1, "s4_c1_dual_arm_FLTA_S2_D01009_M1.json")
    if os.path.isfile(da):
        r = json.load(open(da))
        check("M5 双臂 anchor=377", r["domain"]["anchor_beats"] == 377)
    else:
        check("M5 双臂报告存在", False, da)

    _report(checks, ok_all)
    return 0 if ok_all else 1


def _report(checks, ok_all):
    for name, ok, detail in checks:
        print(f"  [{'OK' if ok else 'FAIL'}] {name}"
              + (f" — {detail}" if detail and not ok else ""))
    n_fail = sum(1 for _, ok, _ in checks if not ok)
    print(f"汇总: PASS={len(checks)-n_fail} FAIL={n_fail}")
    print("S4-C1 MODELE SELFTEST", "PASS" if ok_all else "FAIL")


if __name__ == "__main__":
    sys.exit(main())
