#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_gp2e_gate.py — S4-C1 门①：PlantStepE codegen C 推进器 vs 解释执行参考逐拍对照。

口径（承 Model D run_gp2_gate.py G-P2；S4 spec §3 C1 门）：
  - codegen 侧：Host/plantstep_advancer.py backend='modelE' 加载
    Host/plantstep_e/<scenario>/libplantstepE_<scenario>.dylib（23E GRT 生成 +
    build_plantstep_e.sh 编译），开环恒 P_gen 回放，逐拍 step(300.0)，
    每拍 = 1000×0.01 s 子步（10 s 控制拍）。
  - 解释执行参考侧：Host/reference/s4_gp2e_<scenario>_simref.csv
    （主 Agent MATLAB 以同一 PlantStepE 模型解释执行、同一恒 P_gen 开环输入
    生成的 0.01 s dense 轨迹，在 t=k×10 拍边界采样导出；列定义见文件头）。
  - 门禁项（阈值 1e-6，不放宽）：X 直读 4 状态逐拍对照（tick 1..N-1）：
      x_ship_m / v_ship_mps / SOC / V_dc_V
    （Model D G-P2 用 4 状态 + 5 内部态；Model E 等价核心 = 位置/速度/SOC/母线
      电压；fuel/omega/drive/ki 为报告项——Model E fuel 由 3 个积分器合成，
      拍边界口径先在报告面观察）。
  - 报告项（不作门禁）：P_load/v_cmd/accel/P_bat/P_prop/state_DG1/DG2/
    P_DG1/P_DG2/t_s 采样时序特征。

运行（.venv_tf）:
    python3 tools/run_gp2e_gate.py --scenario S20_composite_voyage
    产物: Host/reference/s4_gp2e_<scenario>_gate.json + _compare.csv
    退出码: 0 = PASS；1 = FAIL（先写 JSON）；2 = 结构性错误。
"""
import argparse
import csv
import ctypes
import hashlib
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)
sys.path.insert(0, HOST)

from plantstep_advancer import make_advancer, PlantStepError  # noqa: E402

P_GEN_CONST = 300.0          # 开环恒 P_gen（与参考运行同口径）
TICK_S = 10.0                # Model E 控制拍
THRESH = 1e-6


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scenario', default='S20_composite_voyage')
    ap.add_argument('--ticks', type=int, default=0, help='0 = 全部（simref 决定）')
    args = ap.parse_args()

    ref_csv = os.path.join(
        HOST, 'reference', 's4_gp2e_%s_simref.csv' % args.scenario)
    if not os.path.isfile(ref_csv):
        print('GATE:GP2E:NO_REF — 缺参考 CSV: %s' % ref_csv, file=sys.stderr)
        return 2

    ref_rows = []
    with open(ref_csv) as fh:
        rd = csv.DictReader(fh)
        for row in rd:
            ref_rows.append(row)
    n_ref = len(ref_rows)

    try:
        adv = make_advancer(backend='modelE', scenario=args.scenario)
    except PlantStepError as exc:
        print('GATE:GP2E:ADVANCER — %s' % exc, file=sys.stderr)
        return 2

    n_ticks = (n_ref - 1) if args.ticks == 0 else min(args.ticks, n_ref - 1)
    # adv tick k (1..n_ticks-1) 对照 ref row k（拍边界 t=k×10；X 直读口径）
    gate_keys = ['x_ship_m', 'v_ship_mps', 'SOC', 'V_dc_V']
    report_keys = ['t_s', 'P_load_total_kW', 'v_cmd_mps', 'acceleration_mps2',
                   'P_bat_kW', 'P_prop_kW', 'state_DG1', 'state_DG2',
                   'P_DG1_kW', 'P_DG2_kW', 'fuel_total_kg']
    compare_rows = []
    n_pass = 0
    n_fail = 0
    worst = {k: 0.0 for k in gate_keys}
    worst_tick = {k: -1 for k in gate_keys}
    # 口径: ref row0 = t=0 初始状态（= adv init 后 X IC，可先断言一致性）；
    # adv.step 第 k 拍结束状态 ↔ ref_rows[k]（t=k×10）。
    # k=0 行先做 IC 断言（X 4 状态应与参考初始一致，阈值同 1e-6）。
    row0 = ref_rows[0]
    try:
        s0 = adv.states()
    except PlantStepError as exc:
        print('GATE:GP2E:ABNORMAL init — %s' % exc, file=sys.stderr)
        return 2
    ic_d = {'x_ship_m': abs(s0['x_ship_m'] - float(row0['x_ship_m'])),
            'v_ship_mps': abs(s0['v_ship_mps'] - float(row0['v_ship_mps'])),
            'SOC': abs(s0['SOC'] - float(row0['SOC'])),
            'V_dc_V': abs(s0['V_dc_V'] - float(row0['V_dc_V']))}
    if max(ic_d.values()) > THRESH:
        print('GATE:GP2E:IC_MISMATCH — %s' % ic_d, file=sys.stderr)
        return 1
    for k in range(1, n_ticks + 1):
        try:
            d = adv.step(P_GEN_CONST)
            xd = adv.states()   # X 直读（拍边界真值；G-P2 门禁读点，Model D 同法）
        except PlantStepError as exc:
            print('GATE:GP2E:ABNORMAL tick %d — %s' % (k, exc), file=sys.stderr)
            adv.close()
            return 2
        ref = ref_rows[k]
        diffs = {}
        ok = True
        for key in gate_keys:
            dv = abs(float(xd[key]) - float(ref[key]))
            diffs[key] = dv
            if dv > worst[key]:
                worst[key] = dv
                worst_tick[key] = k
            if dv > THRESH:
                ok = False
        n_pass += 1 if ok else 0
        n_fail += 0 if ok else 1
        compare_rows.append({
            'tick': k, 'ok': ok,
            **{key: xd[key] for key in gate_keys},
            **{('ref_' + key): float(ref[key]) for key in gate_keys},
            **{('d_' + key): diffs[key] for key in gate_keys},
            **{key: d[key] for key in report_keys},
        })
    adv.close()

    overall = n_fail == 0
    out_gate = os.path.join(
        HOST, 'reference', 's4_gp2e_%s_gate.json' % args.scenario)
    out_cmp = os.path.join(
        HOST, 'reference', 's4_gp2e_%s_compare.csv' % args.scenario)
    dylib = os.path.join(
        HOST, 'plantstep_e', args.scenario,
        'libplantstepE_%s.dylib' % args.scenario)
    gate = {
        'schema': 's4_gp2e_gate_v1',
        'scenario': args.scenario,
        'backend': 'modelE (GRT dylib)',
        'p_gen_const_kw': P_GEN_CONST,
        'tick_s': TICK_S,
        'threshold': THRESH,
        'gate_keys': gate_keys,
        'n_ticks': n_ticks, 'ic_diff': ic_d,
        'n_pass': n_pass, 'n_fail': n_fail,
        'worst_abs_diff': {k: worst[k] for k in gate_keys},
        'worst_tick': worst_tick,
        'dylib_sha256': sha256_file(dylib) if os.path.isfile(dylib) else None,
        'ref_sha256': sha256_file(ref_csv),
        'overall': overall,
        'note': 'X 直读 4 状态门禁（Model D G-P2 同法 1e-6 不放宽）；'
                'Model E fuel 三积分器合成、t_s 采样时序为报告项。',
    }
    with open(out_gate, 'w') as fh:
        json.dump(gate, fh, indent=1)
    with open(out_cmp, 'w', newline='') as fh:
        w = csv.DictWriter(fh, fieldnames=list(compare_rows[0].keys()))
        w.writeheader()
        for r in compare_rows:
            w.writerow(r)
    print('G-P2E %s: %d/%d ticks bit-within-%g; worst=%s'
          % (args.scenario, n_pass, n_ticks, THRESH,
             {k: '%.3g@%d' % (worst[k], worst_tick[k]) for k in gate_keys}))
    print('GATE:', 'PASS' if overall else 'FAIL', '->', out_gate)
    return 0 if overall else 1


if __name__ == '__main__':
    sys.exit(main())
