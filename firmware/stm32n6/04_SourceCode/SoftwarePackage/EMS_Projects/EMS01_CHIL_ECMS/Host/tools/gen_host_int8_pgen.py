#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_host_int8_pgen.py — T-508 层②参考工具：host-int8 全链路 P_gen 参考生成。

design §架构 层②对拍定义 = "board-NPU vs host-int8（同 C 控制律 + Python tflite
推理）"（spec ems01-chil-ecms-v1）。本工具生成 host-int8 侧 P_gen 参考：
  gcc 编译 同一 Appli/App/ems_ctrl.c（控制律单一来源，不复制控制逻辑到本工具）
  + ems_params.c + tests/test_driver_int8ref.c（模式 B CSV 的 NN 输出作 stub
  后端注入，刻意不编译 ems_infer_ref.c）→ 逐 tick 输出 host-int8 P_gen 参考 CSV。
产物供 ems_inject.py --ref-int8 <本产物> 的层② P_gen_kW 网格步对拍
（GRID_STEP_KW=20 design 固定；硬门禁 within_1grid_rate ≥ 0.99 在 ems_inject 侧判）。
编译选项沿用 test_l0_trace_diff.py：-std=c11 -O2 -Wall -Wextra -ffp-contract=off
（零警告硬检查；-ffp-contract=off 固定 IEEE 运算树）；二进制写系统临时目录，
不污染工程树。
用法：
  .venv_tf python Host/tools/gen_host_int8_pgen.py \
      --trace <fw_reference>/ref_G2_*.csv \
      --modeb Host/reference/reference_int8_trace_*.csv \
      --out Host/reference/reference_int8_pgen_*.csv [--cc gcc] [--ref-sim CSV]
  --ref-sim：与 Simulink 参考 P_gen_ref_kW 的网格步一致率（报告性，不作门禁；
  显著 <0.9 说明 stub 口径错，需 ESCALATE）。
退出码：0 = 成功；1 = 编译/运行/行数校验失败。
路径纪律：全部由 __file__ 相对推导，禁止硬编码绝对路径。
"""

import argparse
import csv
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))            # EMS01/Host/tools
PROJ = os.path.abspath(os.path.join(HERE, "..", ".."))       # EMS01_CHIL_ECMS
APP = os.path.join(PROJ, "Appli", "App")
TESTS_DIR = os.path.join(PROJ, "Host", "tests")

GRID_STEP_KW = 20.0  # design 固定：P_gen 网格步 20 kW（与 ems_inject.py 同源口径）


def compile_driver(cc, build_dir):
    """gcc 零警告编译（硬性检查，口径同 test_l0_trace_diff.py）。"""
    srcs = [os.path.join(APP, f) for f in ("ems_ctrl.c", "ems_params.c")]
    srcs.append(os.path.join(TESTS_DIR, "test_driver_int8ref.c"))
    exe = os.path.join(build_dir, "int8ref_driver")
    r = subprocess.run(
        [cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-ffp-contract=off",
         "-I", APP, "-o", exe] + srcs + ["-lm"],
        capture_output=True, text=True)
    if r.returncode != 0 or r.stderr.strip():
        raise RuntimeError("gcc 非零警告/失败:\n%s" % (r.stderr or r.stdout))
    return exe


def count_data_rows(path):
    """CSV 数据行数（不含表头；空行按 csv.reader 自然计）"""
    with open(path, newline="") as f:
        return max(0, sum(1 for _ in csv.reader(f)) - 1)


def grid_stats_vs_sim(out_csv, sim_csv):
    """out P_gen_kW vs Simulink P_gen_ref_kW 网格步统计（按 tick 键控，报告性）。"""
    ref = {}
    with open(sim_csv, newline="") as f:
        for r in csv.DictReader(f):
            ref[int(r["tick"])] = float(r["P_gen_ref_kW"])
    n = exact = within1 = 0
    maxdp = 0.0
    with open(out_csv, newline="") as f:
        for r in csv.DictReader(f):
            k = int(r["tick"])
            if k not in ref:
                continue
            n += 1
            dp = abs(float(r["P_gen_kW"]) - ref[k])
            maxdp = max(maxdp, dp)
            if dp < 1e-6:
                exact += 1
            if dp <= GRID_STEP_KW + 1e-9:
                within1 += 1
    return n, (exact / n if n else None), (within1 / n if n else None), maxdp


def main():
    ap = argparse.ArgumentParser(
        description="T-508 层②：host-int8 全链路 P_gen 参考生成"
                    "（模式 B NN 输出 stub 后端 + 同一 ems_ctrl.c）")
    ap.add_argument("--trace", required=True,
                    help="注入轨迹 CSV（ref_G2 同款 7 信号全列）")
    ap.add_argument("--modeb", required=True,
                    help="模式 B 输出 CSV（gen_reference_int8.py --trace 口径）")
    ap.add_argument("--out", required=True,
                    help="host-int8 P_gen 参考 CSV 输出路径")
    ap.add_argument("--cc", default="gcc", help="C 编译器（默认 gcc；clang 兼容）")
    ap.add_argument("--ref-sim", dest="ref_sim", metavar="CSV",
                    help="Simulink 参考 CSV（P_gen_ref_kW 网格步一致率，报告性）")
    a = ap.parse_args()

    try:
        with tempfile.TemporaryDirectory(prefix="ems01_int8ref_") as build:
            exe = compile_driver(a.cc, build)
            r = subprocess.run([exe, a.trace, a.modeb, a.out],
                               capture_output=True, text=True)
    except RuntimeError as e:
        print("ERROR: %s" % e)
        return 1
    if r.returncode != 0:
        print("ERROR: driver 退出码 %d\n%s%s" % (r.returncode, r.stdout, r.stderr))
        return 1
    print("[int8ref] compile OK: %s -std=c11 -O2 -Wall -Wextra -ffp-contract=off "
          "(ems_ctrl.c + ems_params.c + test_driver_int8ref.c)" % a.cc)
    if r.stdout.strip():
        print("[int8ref] driver: %s" % " | ".join(r.stdout.strip().splitlines()))
    n_trace = count_data_rows(a.trace)
    n_out = count_data_rows(a.out)
    if n_out != n_trace:
        print("ERROR: out 行数 %d != trace 行数 %d" % (n_out, n_trace))
        return 1
    print("[int8ref] rows: out=%d trace=%d OK -> %s" % (n_out, n_trace, a.out))
    if a.ref_sim:
        n, ex, w1, maxdp = grid_stats_vs_sim(a.out, a.ref_sim)
        print("[int8ref] vs Simulink P_gen_ref（报告性，不作门禁）: n=%d "
              "exact=%.4f%% within_1grid=%.4f%% max_dP=%.6g kW"
              % (n, 100 * ex, 100 * w1, maxdp))
    return 0


if __name__ == "__main__":
    sys.exit(main())