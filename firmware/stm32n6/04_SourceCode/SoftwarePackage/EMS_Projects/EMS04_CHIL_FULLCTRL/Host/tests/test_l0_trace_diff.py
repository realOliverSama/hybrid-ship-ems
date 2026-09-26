#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_l0_trace_diff.py — EMS01 CHIL ECMS L0 参考轨迹差分（T-504，spec ems01-chil-ecms-v1）。

职责（design §Testing L0 / tasks T-504 Validation，硬门禁 design 固定不得调整）：
  1. host gcc `-std=c11 -Wall -Wextra -ffp-contract=off` 零警告编译 T-503 交付的
     ems_ctrl.c + ems_infer_ref.c + ems_params.c + test_driver.c（-O2 与 -O0 双构型）；
     -ffp-contract=off 固定 IEEE 运算树，是逐位可比的编译前提（T-503 移交注意）。
  2. 逐 tick 回放 Simulink 参考轨迹（66_export_fw_reference.m 产物，law=7 G2）：
     对每个 ref_*.csv 以 `step t P_load v_ship SOC 1` 驱动 ems_ctrl_step
     （nn_ok=1，对应仿真中 ECMS_NN_WeightsOK=1），对比参考 P_gen_ref。
  3. 硬门禁：≥99% tick 同网格点（Grid_dP=20 kW，round(P/20) 相等）且
     max |ΔP| ≤ 20 kW（1 网格步）；另报告逐位（double bit-exact）一致率。
  4. 诊断（报告性，不设门禁）：e_hat（门控后）/K/K_w/cls_eff/cls_raw 与参考
     内部量的一致性；-O0 与 -O2 构型 P 的一致性。
  5. 报告归档：fw_reference/l0_diff_report.json + l0_diff_report.md +
     l0_deviation_<run>.csv（逐位不一致 tick 清单）。

运行：model/simulink/.venv_tf/bin/python Host/tests/test_l0_trace_diff.py
退出码：0 = 全部 run 门禁通过；1 = 任一失败（不得为 PASS 调整任何阈值）。
"""
import csv
import hashlib
import json
import math
import os
import subprocess
import sys
import tempfile

GRID_DP_KW = 20.0     # ECMS_Grid_dP_kW（design §Data Model，禁改）
GATE_GRID_RATE = 0.99  # ≥99% tick 同网格点（design 固定）
GATE_MAX_ABS_KW = 20.0  # max |Δ| ≤ 1 网格步（design 固定）


def find_repo_root(start_dir):
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("ERROR: 无法定位仓库根")
        d = parent


HERE = os.path.dirname(os.path.abspath(__file__))
REPO = find_repo_root(HERE)
P = os.path.join(REPO, "firmware", "stm32n6", "04_SourceCode", "SoftwarePackage",
                 "EMS_Projects", "EMS01_CHIL_ECMS")
APP = os.path.join(P, "Appli", "App")
REF_DIR = os.path.join(REPO, "model", "simulink_modelD_(EMS_NN_AECMS)",
                       "results", "ecms_phase25", "fw_reference")


def compile_driver(opt, build_dir, extra=()):
    """gcc 零警告编译（硬性检查）；二进制写系统临时目录，不污染工程树。"""
    srcs = [os.path.join(APP, f) for f in
            ("ems_ctrl.c", "ems_infer_ref.c", "ems_params.c")]
    srcs.append(os.path.join(HERE, "test_driver.c"))
    exe = os.path.join(build_dir, "l0_driver_%s_%s"
                       % (opt.lstrip("-"), "_".join(extra) or "std"))
    r = subprocess.run(
        ["gcc", "-std=c11", opt, "-Wall", "-Wextra", "-ffp-contract=off"]
        + list(extra) + ["-I", APP, "-o", exe] + srcs + ["-lm"],
        capture_output=True, text=True)
    if r.returncode != 0 or r.stderr.strip():
        raise AssertionError("gcc %s %s 非零警告/失败:\n%s"
                             % (opt, " ".join(extra), r.stderr or r.stdout))
    return exe


def run_trace(exe, rows):
    """整段轨迹一次性喂入驱动（reset + 逐 tick step），解析 STEP 输出。"""
    lines = ["reset"]
    for row in rows:
        lines.append("step %.17g %.17g %.17g %.17g 1"
                     % (row["t"], row["P_load_kW"], row["v_ship_mps"],
                        row["SOC"]))
    r = subprocess.run([exe], input="\n".join(lines) + "\n",
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise AssertionError("driver 退出码 %d: %s" % (r.returncode, r.stderr))
    out = []
    for ln in r.stdout.splitlines():
        if not ln.startswith("STEP "):
            continue
        kv = {}
        for tok in ln.split()[1:]:
            k, _, v = tok.partition("=")
            kv[k] = v
        out.append(kv)
    if len(out) != len(rows):
        raise AssertionError("STEP 行数 %d != tick 数 %d" % (len(out), len(rows)))
    return out


def check_baseline_manifest():
    """D-4 混态防护：参考数据 sha256 与基线清单逐一比对。

    清单 fw_reference/baseline_manifest.json 存在时，对其 files 中每个相对路径
    （相对 fw_reference/）重算 sha256 并比对；不一致 → AssertionError（防
    ref_*.csv 与基线清单分属新旧口径的混态比对）。清单不存在（参考数据尚未
    冻结基线）时跳过并打印提示。
    """
    mpath = os.path.join(REF_DIR, "baseline_manifest.json")
    if not os.path.isfile(mpath):
        print("[L0] baseline_manifest.json 不存在 → 跳过基线指纹校验"
              "（参考数据未冻结基线清单；混态防护未启用）")
        return
    with open(mpath, "r", encoding="utf-8") as f:
        manifest = json.load(f)
    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        raise AssertionError(
            "baseline_manifest.json 缺少非空 files 字典: %s" % mpath)
    bad = []
    for rel in sorted(files):
        fpath = os.path.normpath(os.path.join(REF_DIR, rel))
        if not os.path.isfile(fpath):
            bad.append("%s: 清单内文件缺失" % rel)
            continue
        with open(fpath, "rb") as f:
            got = hashlib.sha256(f.read()).hexdigest()
        want = files[rel]
        if isinstance(want, dict):   # 兼容 {"sha256": ...} 对象型条目
            want = want.get("sha256")
        if want != got:
            bad.append("%s: sha256 不一致（清单=%s 实际=%s）"
                       % (rel, str(want)[:16], got[:16]))
    if bad:
        raise AssertionError(
            "参考数据与基线清单不一致——混态防护，重新生成基线清单或排查:\n  "
            + "\n  ".join(bad))
    print("[L0] 基线清单指纹校验 OK（%d 个文件）" % len(files))


def diff_run(name, csv_path, exe, build_dir):
    with open(csv_path, newline="") as f:
        rd = csv.DictReader(f)
        rows = list(rd)
    for row in rows:
        for k in ("t", "P_load_kW", "SOC", "v_cmd_mps", "v_ship_mps",
                  "P_gen_ref_kW", "e_hat", "e_hat_raw", "K",
                  "q10_kwh", "q50_kwh", "q90_kwh", "s_q_kwh"):
            row[k] = float(row[k])
        for k in ("tick", "K_w", "cls_eff", "cls_raw"):
            row[k] = int(row[k])
        row["accel_mps2"] = float(row["accel_mps2"])
        row["x_ship_m"] = float(row["x_ship_m"])

    steps = run_trace(exe, rows)
    p_fw = [float(s["P"]) for s in steps]
    p_ref = [row["P_gen_ref_kW"] for row in rows]

    n = len(rows)
    dev_rows = []       # 逐位不一致 tick 清单
    n_exact = 0         # double bit-exact
    n_grid = 0          # 同网格点
    max_abs = 0.0
    n_held = n_notok = 0
    eh_max = 0.0        # e_hat（门控后）最大绝对差（诊断）
    n_k_mis = n_clsmis = 0
    d10s, d50s, d90s = [], [], []   # 分位数通道差（诊断：常量偏移检验）
    for i, s in enumerate(steps):
        held, ok = int(s["held"]), int(s["ok"])
        if held:
            n_held += 1
        if not ok:
            n_notok += 1
        a, b = p_fw[i], p_ref[i]
        d = abs(a - b)
        max_abs = max(max_abs, d)
        if a == b:
            n_exact += 1
        else:
            dev_rows.append({"tick": rows[i]["tick"], "t": rows[i]["t"],
                             "P_ref_kW": b, "P_fw_kW": a, "abs_d_kW": d})
        if int(round(a / GRID_DP_KW)) == int(round(b / GRID_DP_KW)):
            n_grid += 1
        # 诊断面（参考侧回退拍 ok=false 时内部量记 -1/NaN，逐位比对无意义，跳过）
        if rows[i]["K"] == rows[i]["K"]:  # ref K 非 NaN
            eh = abs(float(s["e_hat"]) - rows[i]["e_hat"])
            eh_max = max(eh_max, eh)
            if int(round(float(s["K"]))) != int(round(rows[i]["K"])):
                n_k_mis += 1
            if int(s["cls_eff"]) != rows[i]["cls_eff"]:
                n_clsmis += 1
        d10s.append(float(s["q10"]) - rows[i]["q10_kwh"])
        d50s.append(float(s["q50"]) - rows[i]["q50_kwh"])
        d90s.append(float(s["q90"]) - rows[i]["q90_kwh"])

    grid_rate = n_grid / n
    res = {
        "run": name, "csv": os.path.relpath(csv_path, REPO), "n_ticks": n,
        "n_exact_bit": n_exact, "exact_rate": n_exact / n,
        "n_grid_match": n_grid, "grid_rate": grid_rate,
        "max_abs_d_kW": max_abs,
        "n_held": n_held, "n_not_ok": n_notok,
        "diag_e_hat_max_abs_d": eh_max,
        "diag_K_mismatch": n_k_mis, "diag_cls_eff_mismatch": n_clsmis,
        "diag_q_diff_distinct": {"q10": len(set(round(v, 9) for v in d10s)),
                                 "q50": len(set(round(v, 9) for v in d50s)),
                                 "q90": len(set(round(v, 9) for v in d90s))},
        "diag_q_diff_median": {"q10": sorted(d10s)[n // 2],
                               "q50": sorted(d50s)[n // 2],
                               "q90": sorted(d90s)[n // 2]},
        "gate": grid_rate >= GATE_GRID_RATE and max_abs <= GATE_MAX_ABS_KW,
    }
    # 偏差 tick 清单归档（逐位不一致；空=全部逐位一致）
    dev_path = os.path.join(REF_DIR, "l0_deviation_%s.csv" % name)
    with open(dev_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["tick", "t", "P_ref_kW", "P_fw_kW",
                                          "abs_d_kW"])
        w.writeheader()
        w.writerows(dev_rows)
    res["deviation_csv"] = os.path.relpath(dev_path, REPO)
    res["n_dev_ticks"] = len(dev_rows)

    # -O0 交叉核验（同轨迹，报告性）
    exe0 = compile_driver("-O0", build_dir)
    steps0 = run_trace(exe0, rows)
    p_o0 = [float(s["P"]) for s in steps0]
    res["o0_vs_o2_P_mismatch"] = sum(1 for a, b in zip(p_o0, p_fw) if a != b)
    return res


def main():
    csvs = sorted(f for f in os.listdir(REF_DIR)
                  if f.startswith("ref_") and f.endswith(".csv"))
    if not csvs:
        raise SystemExit("ERROR: 未找到参考 CSV（先运行 66_export_fw_reference.m）")
    check_baseline_manifest()  # D-4 混态防护：ref CSV 与基线清单指纹比对
    build_dir = tempfile.mkdtemp(prefix="ems01_l0_trace_")
    exe = compile_driver("-O2", build_dir)
    results = []
    for f in csvs:
        name = f[len("ref_"):-len(".csv")]
        results.append(diff_run(name, os.path.join(REF_DIR, f), exe, build_dir))

    # ---- e_hat/q 分位数常量偏移根因检验（诊断，不设门禁） ----
    # 已知发现：EML/chart 侧 ECMS_NN_b3 为行向量 [1x3]，EML `y_norm = W3.'*h2 + b3`
    # 触发隐式扩张得 [3x3]，Ehat_q(1/2/3) 线性索引均取第 1 列 → 三个分位数头共用
    # 偏置 b3(1)；T-503 的 C 按设计意图逐头加 b3(j)。若差异满足
    #   dQ10≈0、dQ50=(b3(1)-b3(2))*YStd、dQ90=(b3(1)-b3(3))*YStd
    # 则 P 无关的 ê/分位数遥测差异完全由该 EML 行向量 b3 隐式扩张语义解释。
    finding = "deploy JSON 不可读，未做常量偏移检验"
    dj_path = os.path.join(REPO, "model", "simulink_modelD_(EMS_NN_AECMS)",
                           "results", "ecms_phase25", "deploy",
                           "selected_model_weights_v2.json")
    try:
        with open(dj_path) as f:
            dj = json.load(f)
        b3 = dj["weights"]["b3"]
        ystd = dj["norm"]["y_std"]
        pred50 = (b3[0] - b3[1]) * ystd
        pred90 = (b3[0] - b3[2]) * ystd
        ok_all = True
        for r in results:
            m50, m90 = r["diag_q_diff_median"]["q50"], r["diag_q_diff_median"]["q90"]
            # 存储方向为 fw - ref，故期望 -pred（ref - fw = +pred）
            ok_all &= abs(r["diag_q_diff_median"]["q10"]) < 1e-9
            ok_all &= abs(m50 + pred50) < 1e-9 and abs(m90 + pred90) < 1e-9 \
                and r["diag_q_diff_distinct"]["q50"] <= 64 \
                and r["diag_q_diff_distinct"]["q90"] <= 64
        if ok_all:
            finding = ("已确认：q50/q90/e_hat 遥测差异为常量偏移（ref−fw: q50=%.9f, "
                       "q90=%.9f kWh, q10=0，多 tick 仅浮点末位抖动），与 EML 行向量 "
                       "b3 隐式扩张预测 (b3(1)-b3(j))*YStd 逐项吻合——chart 实际语义为 "
                       "三分位数共用偏置 b3(1)（Ehat_q 线性索引取 [3x3] 第 1 列），而 "
                       "T-503 C 为逐头偏置（设计意图）。P_gen_ref 不受影响（bit-exact）。" % (pred50, pred90))
        else:
            finding = "常量偏移假设未完全吻合，需人工分析"
    except (OSError, KeyError, ValueError):
        pass

    all_pass = all(r["gate"] for r in results) \
        and all(r["n_held"] == 0 and r["n_not_ok"] == 0 for r in results)
    report = {
        "task": "T-504",
        "spec": "ems01-chil-ecms-v1",
        "gate_definition": ">=99% tick 同网格点(20 kW) 且 max|dP|<=20 kW "
                           "(design 固定，不得为 PASS 调整)",
        "gate_grid_rate": GATE_GRID_RATE,
        "gate_max_abs_kW": GATE_MAX_ABS_KW,
        "compile": "gcc -std=c11 -O2/-O0 -Wall -Wextra -ffp-contract=off "
                   "(ems_ctrl + ems_infer_ref + ems_params + test_driver)",
        "all_pass": all_pass,
        "diag_e_hat_offset_finding": finding,
        "runs": results,
    }
    jpath = os.path.join(REF_DIR, "l0_diff_report.json")
    with open(jpath, "w") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)

    md = ["# T-504 L0 参考轨迹差分报告（host gcc ems_ctrl vs Simulink law=7 G2）", "",
          "- 门禁（design 固定）：≥99% tick 同网格点（20 kW）且 max |Δ| ≤ 20 kW",
          "- 编译：`gcc -std=c11 -Wall -Wextra -ffp-contract=off`（O2/O0 双构型，零警告）", ""]
    for r in results:
        md += [
            "## %s" % r["run"], "",
            "| 指标 | 值 |", "|---|---|",
            "| tick 数 | %d |" % r["n_ticks"],
            "| 逐位一致（bit-exact） | %d（%.4f%%） |" % (r["n_exact_bit"], 100 * r["exact_rate"]),
            "| 同网格点 | %d（%.4f%%） |" % (r["n_grid_match"], 100 * r["grid_rate"]),
            "| max abs(dP) | %.6g kW |" % r["max_abs_d_kW"],
            "| held/not-ok tick | %d / %d |" % (r["n_held"], r["n_not_ok"]),
            "| 诊断 e_hat 最大abs差 | %.3g |" % r["diag_e_hat_max_abs_d"],
            "| 诊断 K/cls_eff 不一致拍 | %d / %d |" % (r["diag_K_mismatch"], r["diag_cls_eff_mismatch"]),
            "| -O0 vs -O2 P 不一致拍 | %d |" % r["o0_vs_o2_P_mismatch"],
            "| 偏差 tick 清单 | %s（%d 条） |" % (r["deviation_csv"], r["n_dev_ticks"]),
            "| **门禁** | **%s** |" % ("PASS" if r["gate"] else "FAIL"), ""]
    md.append("诊断发现（e_hat/q50/q90 常量偏移）: " + finding)
    md.append("")
    md.append("总体判定: **%s**" % ("PASS" if all_pass else "FAIL"))
    with open(os.path.join(REF_DIR, "l0_diff_report.md"), "w") as f:
        f.write("\n".join(md) + "\n")

    for r in results:
        print("[L0] %-32s ticks=%-5d exact=%.4f%% grid=%.4f%% max|dP|=%.6g kW "
              "held=%d notok=%d gate=%s"
              % (r["run"], r["n_ticks"], 100 * r["exact_rate"], 100 * r["grid_rate"],
                 r["max_abs_d_kW"], r["n_held"], r["n_not_ok"],
                 "PASS" if r["gate"] else "FAIL"))
    print("[L0] 总体判定: %s -> %s" % ("PASS" if all_pass else "FAIL", jpath))
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
