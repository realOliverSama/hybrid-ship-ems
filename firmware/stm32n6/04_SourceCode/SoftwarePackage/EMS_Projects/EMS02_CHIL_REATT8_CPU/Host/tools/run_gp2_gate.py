#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""run_gp2_gate.py — T-605 G-P2 门禁：codegen C 推进器 vs 解释执行子集模型逐拍对照
（spec: ems01-chil-v3-closedloop，FR-605/AC-602；design §4.4a ERT 输出时序修订 + §5 G-P2）。

机制与 provenance
    - codegen 侧：Host/plantstep_advancer.py ctypes 后端加载
      Host/plantstep/<scenario>/libplantstep_<scenario>.dylib
      （scripts/23_codegen_plantstep.m 生成 C 代码 + tools/build_plantstep.sh 编译，
      provenance 见 plantstep/<scenario>/build_manifest.json），
      逐拍 step(P_gen[tick]) = set_pgen + 100×0.01 s 子步（ZOH，P(t)=ref 行 floor(t)，
      与 22 号脚本回放网格同口径）推进。
    - 解释执行参考侧（sim_ref）：Host/reference/v3_gp1_<scenario>_replay.csv —— 由
      model/simulink_modelD_(EMS_NN_AECMS)/scripts/22_run_gp1_gate.m 以【同一回放输入】
      （ref_G2 的 P_gen_ref_kW 列构造 0.01 s ZOH 网格）驱动【同一
      HybridShip_ModelD_PlantStep 模型】（MATLAB 解释执行）生成的逐拍轨迹。
      provenance 链：ref_G2（v1.1 冻结基线）→ 22 号脚本 → gp1 replay CSV。
    - 门禁项（design §4.4a 修订，阈值 1e-6 不放宽）：拍边界 X 直读
      （wrapper states()，9 个连续状态）逐拍对照（tick 1..N-1）：
        4 状态    x_ship_m / v_ship_mps / SOC / fuel_total_kg
                  （fuel CSTATE 为 g 口径，X 直读值 ×1e-3 = kg）
        5 内部态  omega_prop_radps / drive_lag_s / V_dc_V / p_gen_inertia_kW /
                  ki_integrator
      sim 侧来源：4 状态 ← gp1 replay CSV 同 tick 列（位级一致预期）；
      5 内部态 ← v3_gp2_<scenario>_simref.csv（主 Agent 以 MATLAB logsout 0.01 s
      dense 数据在 tick 时刻采样导出，列定义见下）。tick0（k=0 拍）由本脚本按
      场景数据构造（P_load=hotel、SOC=Bat_SOC_init、v_cmd=VOYAGE_V_VALUE_MPS[0]、
      v_ship=accel=x_ship=fuel=P_bat=P_prop=0、t=0、voyage_stop=0），与 gp1 replay
      row0 断言一致（ctypes 后端另以 X IC 直读互证）。
    - 报告项（不作门禁，design §4.4a）：t_s / P_load_kW / v_cmd_mps / accel_mps2 /
      P_bat_kW / P_prop_kW / voyage_stop —— ERT 根 Outport 写出于子步末级阶段时刻
      t=(k+1)−0.0025（ode3 第 3 级），wrapper Y 即该时刻读数；对照 sim 同刻线性插值
      （插值仅用于报告残差特征，残差为仪器时序特征而非 codegen-vs-解释执行差异）。

sim-ref CSV 列定义（Host/reference/v3_gp2_<scenario>_simref.csv，主 Agent MATLAB 导出契约）
    表头 16 列逐字符一致，%.17g，行 = tick 1..N-1（sim t=1..N-1 拍边界采样，N = 回放行数）：
        tick,t,x_ship_m,v_ship_mps,SOC,fuel_total_kg,omega_prop_radps,drive_lag_s,
        V_dc_V,p_gen_inertia_kW,ki_integrator,P_load_kW,v_cmd_mps,accel_mps2,
        P_bat_kW,P_prop_kW
    - x_ship_m/v_ship_mps/SOC：X CSTATE 值（= logsout 同名信号，design §4.4a 已核证同值）；
    - fuel_total_kg：kg 口径（fuel CSTATE 为 g → ×1e-3，或直接取 logsout kg 信号）；
    - omega_prop_radps/drive_lag_s/V_dc_V/p_gen_inertia_kW/ki_integrator：与 shim
      PS_X（x_ship/v_ship/omega_prop/DriveLag/SOC/V_dc/inertia/Ki_int/fuel_int 顺序）
      对应的 logsout 连续状态信号在 t=k 的采样；
    - P_load_kW/v_cmd_mps/accel_mps2/P_bat_kW/P_prop_kW：t=k 输出信号，
      仅用于与 gp1 replay 的一致性交叉核对（报告项，不入门禁）。

运行（Python: model/simulink/.venv_tf，纯 stdlib；由主 Agent 执行）
    python3 tools/run_gp2_gate.py --scenario S20
    产物: <out-dir>/v3_gp2_<scenario>_gate.json（机器可读门禁，gate_result 风格）
          <out-dir>/v3_gp2_<scenario>_compare.csv（逐拍对照，%.17g）
    前置: v3_gp2_<scenario>_simref.csv 已按上述列定义由主 Agent MATLAB 导出。
    退出码: 0 = overall PASS；1 = 门禁 FAIL（先写 JSON 再退出）；
            2 = 结构性错误（stderr 带 GATE:GP2:* 标识，不写产物）。
    --backend stub 仅为对照逻辑自测干跑（stub 无内部动力学、物理粗糙），
    不得用于任何门禁结论（JSON 记录 backend 以资识别）。
"""
import argparse
import csv
import hashlib
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))            # .../EMS01_CHIL_ECMS/Host/tools
HOST_DIR = os.path.dirname(HERE)                             # .../EMS01_CHIL_ECMS/Host
EMS01_DIR = os.path.dirname(HOST_DIR)                        # .../EMS01_CHIL_ECMS

THRESHOLD = 1e-6                                             # design §5 G-P2（不放宽）
STAGE_DT = 0.0025                                            # ERT 阶段时刻偏移 (k+1)−0.0025
FUEL_SCALE = 1e-3                                            # fuel CSTATE g → kg
SCENARIOS = ("S20", "voyage002")
REF_G2_NAMES = {
    "S20": "ref_G2_S20_composite_voyage_matrix.csv",
    "voyage002": "ref_G2_test_voyage_002_v2.csv",
}
FW_REF_REL = os.path.join("model", "simulink_modelD_(EMS_NN_AECMS)",
                          "results", "ecms_phase25", "fw_reference")

# 门禁项 9 状态（= wrapper states() X 直读键序的子集；前 4 ← gp1 replay，后 5 ← simref）
GATE_STATES = (
    "x_ship_m", "v_ship_mps", "SOC", "fuel_total_kg",
    "omega_prop_radps", "drive_lag_s", "V_dc_V", "p_gen_inertia_kW",
    "ki_integrator",
)
STATE_STATES = GATE_STATES[:4]          # sim 侧 ← gp1 replay CSV
INTERNAL_STATES = GATE_STATES[4:]       # sim 侧 ← v3_gp2 simref CSV

# v3_gp1_<scenario>_replay.csv 表头（22 号脚本产物契约，逐字符）
SIM_REF_COLUMNS = (
    "tick", "t", "P_load_kW", "SOC", "v_cmd_mps", "v_ship_mps", "accel_mps2",
    "x_ship_m", "fuel_total_kg", "P_bat_kW", "P_prop_kW", "voyage_stop",
    "P_gen_ref_kW",
)
# ref_G2 回放输入 CSV 表头（21 列冻结基线契约，22 号脚本同款校验）
REPLAY_IN_COLUMNS = (
    "tick", "t", "P_load_kW", "SOC", "v_cmd_mps", "v_ship_mps", "accel_mps2",
    "x_ship_m", "P_gen_ref_kW", "e_hat", "e_hat_raw", "K", "K_q", "K_w",
    "cls_eff", "cls_raw", "q10_kwh", "q50_kwh", "q90_kwh", "s_q_kwh",
    "P_replay_kW",
)
# v3_gp2_<scenario>_simref.csv 表头（本脚本定义 → 主 Agent MATLAB 导出契约，逐字符）
SIMREF_COLUMNS = (
    "tick", "t", "x_ship_m", "v_ship_mps", "SOC", "fuel_total_kg",
    "omega_prop_radps", "drive_lag_s", "V_dc_V", "p_gen_inertia_kW",
    "ki_integrator", "P_load_kW", "v_cmd_mps", "accel_mps2", "P_bat_kW",
    "P_prop_kW",
)

# 报告项（design §4.4a；键 = JSON/统计口径 → wrapper Y 键映射）
REPORT_KEYS = (
    "t_s", "P_load_kW", "v_cmd_mps", "accel_mps2", "P_bat_kW", "P_prop_kW",
    "voyage_stop",
)
REPORT_Y_KEYS = {
    "t_s": "t_s", "P_load_kW": "P_load_total_kW", "v_cmd_mps": "v_cmd_mps",
    "accel_mps2": "acceleration_mps2", "P_bat_kW": "P_bat_kW",
    "P_prop_kW": "P_prop_kW", "voyage_stop": "voyage_stop",
}
# 报告项中经 sim 同刻线性插值对照的信号（t_s 单独按阶段时刻语义对照）
REPORT_INTERP_KEYS = ("P_load_kW", "v_cmd_mps", "accel_mps2", "P_bat_kW",
                      "P_prop_kW", "voyage_stop")

# 门禁项名 → JSON metric 名（gate_result 风格）
STATE_METRIC = {
    "x_ship_m": "x_ship_max_abs",
    "v_ship_mps": "v_ship_max_abs",
    "SOC": "SOC_max_abs",
    "fuel_total_kg": "fuel_max_abs",
    "omega_prop_radps": "omega_prop_max_abs",
    "drive_lag_s": "drive_lag_max_abs",
    "V_dc_V": "V_dc_max_abs",
    "p_gen_inertia_kW": "p_gen_inertia_max_abs",
    "ki_integrator": "ki_integrator_max_abs",
}


class GateStructuralError(Exception):
    """结构性错误（文件缺失/列不符/维度不符等）→ exit 2，stderr GATE:GP2:*。"""

    def __init__(self, code, msg):
        super().__init__("%s: %s" % (code, msg))
        self.code = code
        self.msg = msg


def find_repo_root(start_dir):
    """向上找同时含 AGENTS.md + model/ + firmware/ 的目录（仓库根）。
    惯例来源: Host/tests/test_l0_trace_diff.py find_repo_root。"""
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise GateStructuralError(
                "REPO_ROOT", "无法定位仓库根（AGENTS.md + model/ + firmware/）")
        d = parent


def fmt17(v):
    return "%.17g" % float(v)


def load_csv_cols(path, expected_header, tag):
    """读数值 CSV → 列主序 dict；表头/列数逐字符校验（结构性错误 exit 2）。"""
    if not os.path.isfile(path):
        raise GateStructuralError("FILE_MISSING_%s" % tag, "文件不存在: %s" % path)
    with open(path, "r", encoding="utf-8", newline="") as fh:
        reader = csv.reader(fh)
        try:
            header = next(reader)
        except StopIteration:
            raise GateStructuralError("CSV_EMPTY_%s" % tag, "CSV 无表头: %s" % path)
        if tuple(header) != tuple(expected_header):
            raise GateStructuralError(
                "COLUMN_MISMATCH_%s" % tag,
                "%s 表头与预期不符；预期: %s；实测: %s"
                % (os.path.basename(path), ",".join(expected_header),
                   ",".join(header)))
        rows = []
        for ln, row in enumerate(reader, start=2):
            if len(row) != len(expected_header):
                raise GateStructuralError(
                    "COLUMN_MISMATCH_%s" % tag,
                    "%s 第 %d 行列数 %d != %d"
                    % (os.path.basename(path), ln, len(row), len(expected_header)))
            try:
                rows.append([float(x) for x in row])
            except ValueError as exc:
                raise GateStructuralError(
                    "CSV_PARSE_%s" % tag,
                    "%s 第 %d 行数值解析失败: %s" % (os.path.basename(path), ln, exc))
    if not rows:
        raise GateStructuralError("ROWS_TOO_FEW_%s" % tag,
                                  "%s 无数据行" % os.path.basename(path))
    return {name: [r[i] for r in rows] for i, name in enumerate(header)}


def check_tick_sequence(ticks, expect_start, tag):
    """tick 列必须从 expect_start 起逐 1 连续（结构性错误 exit 2）。"""
    for i, tv in enumerate(ticks):
        if tv != float(expect_start + i):
            raise GateStructuralError(
                "TICK_SEQUENCE_%s" % tag,
                "tick 第 %d 个数据行为 %r，预期 %d（须连续无缺）"
                % (i + 1, tv, expect_start + i))


def nonfinite_counts(cols, names):
    """统计指定列中 NaN/Inf 个数（数值健康口径）。"""
    out = {}
    for name in names:
        cnt = sum(0 if math.isfinite(v) else 1 for v in cols[name])
        if cnt:
            out[name] = cnt
    return out


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# MATLAB Transfer Function 规范形换算（T-605 执行期实测确认）:
#   num=[1], den=[τ 1] 的 TF 块，其状态 x 与输出 y 满足 y = x/τ（simref 的
#   logsout 信号为块【输出】y，而 wrapper X 直读为【状态】x）。换算系数:
#     drive_lag_s:      τ = ASSUMED_Drive_TimeConstant = 0.5  s（y = 2x）
#     p_gen_inertia_kW: τ = ASSUMED_Gen_TimeConstant    = 5.0  kW（y = x/5）
#   来源: config/Model_Parameters.m（单一来源）；Integrator 块（omega/ki/SOC/V_dc）
#   无此问题（y = x）。
TF_STATE_SCALE = {"drive_lag_s": 0.5, "p_gen_inertia_kW": 5.0}


def read_gate_states(adv, backend):
    """拍边界 9 状态直读（门禁口径）。
    ctypes: wrapper states() X 直读；fuel CSTATE 为 g 口径 → ×1e-3 = kg。
    stub（selftest 干跑专用）: 无 states() API，由 11 键 Y 快照合成；
    内部 5 状态 stub 无对应动力学 → 记 0（干跑仅验证对照机制）。"""
    if backend == "ctypes":
        st = adv.states()
        st["fuel_total_kg"] = st["fuel_total_kg"] * FUEL_SCALE
        for k, tau in TF_STATE_SCALE.items():
            st[k] = st[k] / tau   # 状态 x → 块输出口径 y = x/τ（与 simref 同口径）
        return st
    snap = adv._snapshot()                   # stub 后端 selftest 专用路径
    return {
        "x_ship_m": snap["x_ship_m"], "v_ship_mps": snap["v_ship_mps"],
        "SOC": snap["SOC"], "fuel_total_kg": snap["fuel_total_kg"],
        "omega_prop_radps": 0.0, "drive_lag_s": 0.0, "V_dc_V": 0.0,
        "p_gen_inertia_kW": 0.0, "ki_integrator": 0.0,
    }


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="G-P2 门禁: codegen C 推进器 vs 解释执行子集模型逐拍对照（T-605）")
    ap.add_argument("--scenario", required=True, choices=SCENARIOS,
                    help="回放场景（S20 | voyage002）")
    ap.add_argument("--backend", choices=("ctypes", "stub"), default="ctypes",
                    help="推进器后端（默认 ctypes=codegen C；stub 仅 selftest 干跑）")
    ap.add_argument("--out-dir", default=None,
                    help="产物目录（缺省 Host/reference）")
    ap.add_argument("--sim-ref", default=None,
                    help="解释执行参考 gp1 replay CSV（缺省 Host/reference/v3_gp1_<scenario>_replay.csv）")
    ap.add_argument("--simref-csv", default=None,
                    help="MATLAB 导出的内部状态参考 CSV（缺省 Host/reference/v3_gp2_<scenario>_simref.csv）")
    ap.add_argument("--replay-csv", default=None,
                    help="回放输入 P_gen 源 ref_G2 CSV（缺省 fw_reference 冻结基线文件）")
    args = ap.parse_args(argv)
    scenario = args.scenario
    backend = args.backend

    # ---- 路径解析（全部相对推导，AGENTS §20.2） ----
    repo_root = find_repo_root(HERE)
    out_dir = args.out_dir if args.out_dir else os.path.join(HOST_DIR, "reference")
    sim_ref_path = args.sim_ref if args.sim_ref else os.path.join(
        HOST_DIR, "reference", "v3_gp1_%s_replay.csv" % scenario)
    simref_path = args.simref_csv if args.simref_csv else os.path.join(
        HOST_DIR, "reference", "v3_gp2_%s_simref.csv" % scenario)
    replay_path = args.replay_csv if args.replay_csv else os.path.join(
        repo_root, FW_REF_REL, REF_G2_NAMES[scenario])
    manifest_path = os.path.join(HOST_DIR, "plantstep", scenario,
                                 "build_manifest.json")
    dylib_path = os.path.join(HOST_DIR, "plantstep", scenario,
                              "libplantstep_%s.dylib" % scenario)
    gp1_gate_path = os.path.join(HOST_DIR, "reference",
                                 "v3_gp1_%s_gate.json" % scenario)

    # ---- 结构性前置检查 ----
    if backend == "ctypes" and not os.path.isfile(dylib_path):
        raise GateStructuralError(
            "DYLIB_MISSING",
            "dylib 不存在: %s（先执行 scripts/23_codegen_plantstep.m 与 "
            "tools/build_plantstep.sh %s）" % (dylib_path, scenario))
    if not os.path.isfile(manifest_path):
        raise GateStructuralError(
            "BUILD_MANIFEST_MISSING",
            "build_manifest.json 不存在: %s（codegen 构建指纹，T-604 产物）"
            % manifest_path)
    with open(manifest_path, "r", encoding="utf-8") as fh:
        manifest = json.load(fh)
    if manifest.get("scenario") != scenario:
        raise GateStructuralError(
            "BUILD_MANIFEST_SCENARIO",
            "manifest scenario=%r 与 --scenario=%r 不符"
            % (manifest.get("scenario"), scenario))
    if manifest.get("overall") is not True:
        raise GateStructuralError(
            "BUILD_MANIFEST_NOT_GREEN",
            "build_manifest.json overall != true（codegen 构建门未通过）")
    matlab_version = manifest.get("matlab_version")
    if not matlab_version:
        raise GateStructuralError(
            "MATLAB_VERSION_MISSING", "build_manifest.json 无 matlab_version")

    # ---- 回放输入（ref_G2 的 P_gen_ref_kW 列，tick 0..N-1） ----
    rp_cols = load_csv_cols(replay_path, REPLAY_IN_COLUMNS, "REPLAY_IN")
    n_total = len(rp_cols["tick"])                     # N（含 tick0）
    if n_total < 2:
        raise GateStructuralError(
            "ROWS_TOO_FEW_REPLAY_IN", "回放输入需 >=2 行（tick 0..N-1），实测 %d" % n_total)
    check_tick_sequence(rp_cols["tick"], 0, "REPLAY_IN")
    p_gen = rp_cols["P_gen_ref_kW"]
    for i, p in enumerate(p_gen):
        if not math.isfinite(p):
            raise GateStructuralError(
                "REPLAY_IN_NONFINITE", "P_gen_ref_kW 第 %d 行非有限" % i)
    stop_time_s = float(manifest["scenario_variables"]["scn_key"]["stop_time_s"])
    if abs(stop_time_s - (n_total - 1)) > 1e-6:
        raise GateStructuralError(
            "STOP_TIME_MISMATCH",
            "manifest stop_time_s=%r 与回放行数-1=%d 不一致" % (stop_time_s, n_total - 1))

    # ---- 解释执行参考（gp1 replay CSV；13 列契约） ----
    sr_cols = load_csv_cols(sim_ref_path, SIM_REF_COLUMNS, "SIM_REF")
    if len(sr_cols["tick"]) != n_total:
        raise GateStructuralError(
            "ROW_MISMATCH_SIM_REF",
            "gp1 replay 行数 %d != 回放输入行数 %d"
            % (len(sr_cols["tick"]), n_total))
    check_tick_sequence(sr_cols["tick"], 0, "SIM_REF")
    for i, (tv, tk) in enumerate(zip(sr_cols["t"], sr_cols["tick"])):
        if abs(tv - tk) > 1e-9:
            raise GateStructuralError(
                "TICK_SEQUENCE_SIM_REF", "gp1 replay 第 %d 行 t=%r 与 tick=%r 不符" % (i + 2, tv, tk))
    # provenance 互证: 两文件的 P_gen 序列须位级一致（同一回放输入）
    for i, (pa_, pb_) in enumerate(zip(p_gen, sr_cols["P_gen_ref_kW"])):
        if pa_ != pb_:
            raise GateStructuralError(
                "REPLAY_INPUT_MISMATCH",
                "回放输入 P_gen 与 gp1 replay P_gen_ref_kW 在 tick %d 不一致"
                "（%r != %r）——sim_ref 并非由该回放输入生成" % (i, pb_, pa_))

    # ---- sim-ref CSV（MATLAB 导出的内部状态参考；16 列契约） ----
    mr_cols = load_csv_cols(simref_path, SIMREF_COLUMNS, "SIMREF")
    n_steps = n_total - 1                              # 逐拍 step 数（tick 1..N-1）
    if len(mr_cols["tick"]) != n_steps:
        raise GateStructuralError(
            "ROW_MISMATCH_SIMREF",
            "simref 行数 %d != 预期 %d（tick 1..N-1，N=%d）——须以同一回放输入"
            "重跑 22 号脚本口径导出" % (len(mr_cols["tick"]), n_steps, n_total))
    check_tick_sequence(mr_cols["tick"], 1, "SIMREF")
    for i, (tv, tk) in enumerate(zip(mr_cols["t"], mr_cols["tick"])):
        if abs(tv - tk) > 1e-6:
            raise GateStructuralError(
                "TICK_SEQUENCE_SIMREF",
                "simref 第 %d 行 t=%r 与 tick=%r 偏差 >1e-6（须在 tick 时刻采样）"
                % (i + 2, tv, tk))

    # ---- gp1 gate JSON（sim_ref 的 MATLAB 版本交叉核对，信息级） ----
    sim_ref_matlab_version = None
    if os.path.isfile(gp1_gate_path):
        try:
            with open(gp1_gate_path, "r", encoding="utf-8") as fh:
                sim_ref_matlab_version = json.load(fh).get("matlab_version")
        except (OSError, ValueError):
            sim_ref_matlab_version = None
        if (sim_ref_matlab_version is not None
                and sim_ref_matlab_version != matlab_version):
            print("WARN: gp1 gate MATLAB 版本(%s)与 build_manifest(%s)不一致"
                  % (sim_ref_matlab_version, matlab_version), file=sys.stderr)

    # ---- 数值健康前置扫描（消费列全量；非有限 → 门禁 FAIL，exit 1） ----
    simref_gate_names = tuple(n for n in SIMREF_COLUMNS if n not in ("tick", "t"))
    health_bad = {}
    health_bad.update(nonfinite_counts(sr_cols, SIM_REF_COLUMNS))
    _mr_bad = nonfinite_counts(mr_cols, simref_gate_names)
    for k_, v_ in _mr_bad.items():
        health_bad["simref:" + k_] = v_

    # ---- wrapper（codegen C）装载 ----
    sys.path.insert(0, HOST_DIR)
    import plantstep_advancer as pa
    try:
        adv = pa.make_advancer(backend=backend, scenario=scenario)
    except pa.PlantStepError as exc:
        raise GateStructuralError("ADVANCER_INIT", str(exc))

    nan_cnt = {name: 0 for name in GATE_STATES}
    max_abs = {name: 0.0 for name in GATE_STATES}
    tick_of_max = {name: None for name in GATE_STATES}
    rep_d = {name: [] for name in REPORT_KEYS}          # wrapper Y − sim 同刻插值
    rep_nan = {name: 0 for name in REPORT_KEYS}
    t_s_dev_tick = []                                   # Y.t_s − (k+1)
    csv_rows = []
    vstop_mismatch = 0                                  # Y.voyage_stop vs sim tick k+1（ZOH）
    runtime_error = None

    try:
        # ---- tick0（k=0 拍）：wrapper 未推进（Y 清零内存）→ 按场景数据构造 ----
        sv = manifest["scenario_variables"]
        hotel = float(sv["scn_key"]["hotel_kW"])
        soc0 = float(sv["inject"].get("Bat_SOC_init",
                                      sv["scn_key"]["initial_SOC"]))
        v_cmd0 = float(sv["inject"]["VOYAGE_V_VALUE_MPS"][0])
        tick0_constructed = {
            "t": 0.0, "P_load_kW": hotel, "SOC": soc0, "v_cmd_mps": v_cmd0,
            "v_ship_mps": 0.0, "accel_mps2": 0.0, "x_ship_m": 0.0,
            "fuel_total_kg": 0.0, "P_bat_kW": 0.0, "P_prop_kW": 0.0,
            "voyage_stop": 0.0,
        }
        # ctypes: X IC 直读并与构造 SOC 互证（IC 与场景数据同源）
        tick0_ic = None
        if backend == "ctypes":
            ic = adv.states()
            ic["fuel_total_kg"] = ic["fuel_total_kg"] * FUEL_SCALE
            tick0_ic = ic
            if abs(ic["SOC"] - soc0) > THRESHOLD:
                raise GateStructuralError(
                    "TICK0_IC",
                    "X IC SOC=%r 与场景初始 SOC=%r 不一致（>1e-6）" % (ic["SOC"], soc0))
        t0_states = {name: tick0_constructed[name]
                     for name in ("x_ship_m", "v_ship_mps", "SOC", "fuel_total_kg")}
        if tick0_ic is not None:
            for name in t0_states:
                t0_states[name] = tick0_ic[name]
        tick0_row0 = {name: sr_cols[name][0]
                      for name in SIM_REF_COLUMNS if name != "P_gen_ref_kW"}
        tick0_field_d = {name: abs(t0_states[name] - tick0_row0[name])
                         for name in t0_states}
        tick0_field_d.update(
            {name: abs(tick0_constructed.get(name, 0.0) - tick0_row0[name])
             for name in tick0_row0 if name not in t0_states})
        # 结构性: t / P_load / v_cmd / voyage_stop 构造值须与 row0 一致
        for name in ("t", "P_load_kW", "v_cmd_mps", "voyage_stop"):
            if tick0_field_d[name] > THRESHOLD:
                raise GateStructuralError(
                    "TICK0_MISMATCH",
                    "tick0 构造 %s=%r 与 gp1 replay row0=%r 偏差 %r > 1e-6"
                    % (name, tick0_constructed.get(name), tick0_row0[name],
                       tick0_field_d[name]))
        tick0_state_max_abs = max(tick0_field_d[name] for name in
                                  ("x_ship_m", "v_ship_mps", "SOC", "fuel_total_kg"))

        # ---- 逐拍推进 + 对照（tick 1..N-1；第 k 拍消费 P_gen[k]，区间 [k,k+1) ZOH） ----
        for k in range(n_steps):
            y = adv.step(p_gen[k])
            st = read_gate_states(adv, backend)
            tick = k + 1
            t_stage = (k + 1) - STAGE_DT
            frac = t_stage - k
            row = [float(tick), p_gen[k], t_stage]
            for name in GATE_STATES:
                wv = st[name]
                # sim 侧: gp1 replay tick k ↔ 列索引 k；simref tick j ↔ 行索引 j-1
                sv_ = sr_cols[name][tick] if name in STATE_STATES \
                    else mr_cols[name][tick - 1]
                dv = wv - sv_
                if not (math.isfinite(wv) and math.isfinite(sv_) and math.isfinite(dv)):
                    nan_cnt[name] += 1
                else:
                    ad = abs(dv)
                    if ad > max_abs[name]:
                        max_abs[name] = ad
                        tick_of_max[name] = tick
                row.extend((wv, sv_, dv))
            # 报告项: wrapper Y（阶段时刻 t=(k+1)−0.0025）vs sim 同刻线性插值
            for name in REPORT_INTERP_KEYS:
                yv = float(y[REPORT_Y_KEYS[name]])
                s0 = sr_cols[name][k]
                s1 = sr_cols[name][k + 1]
                si = s0 + frac * (s1 - s0)
                dv = yv - si
                if not (math.isfinite(yv) and math.isfinite(si) and math.isfinite(dv)):
                    rep_nan[name] += 1
                rep_d[name].append(dv)
                if name == "voyage_stop":
                    if int(round(yv)) != int(round(sr_cols[name][tick])):
                        vstop_mismatch += 1
            yv_t = float(y["t_s"])
            si_t = sr_cols["t"][k] + frac * (sr_cols["t"][k + 1] - sr_cols["t"][k])
            dv_t = yv_t - t_stage
            if not (math.isfinite(yv_t) and math.isfinite(dv_t)):
                rep_nan["t_s"] += 1
            rep_d["t_s"].append(dv_t)
            t_s_dev_tick.append(yv_t - (k + 1))
            row.extend((yv_t, si_t, dv_t))
            row.extend(rep_d[name][-1] for name in REPORT_INTERP_KEYS)
            csv_rows.append(row)
    except pa.PlantStepError as exc:
        runtime_error = str(exc)
        print("FAIL(health): wrapper 运行期异常: %s" % exc, file=sys.stderr)
    finally:
        try:
            adv.close()
        except Exception:
            pass

    # ---- simref 与 gp1 replay 一致性交叉核对（报告级，预期 0） ----
    simref_consistency = {}
    for name in ("x_ship_m", "v_ship_mps", "SOC", "fuel_total_kg", "P_load_kW",
                 "v_cmd_mps", "accel_mps2", "P_bat_kW", "P_prop_kW"):
        m = 0.0
        for i in range(n_steps):
            # simref tick i+1 ↔ 行索引 i；gp1 replay tick i+1 ↔ 列索引 i+1
            d = abs(mr_cols[name][i] - sr_cols[name][i + 1])
            if math.isfinite(d) and d > m:
                m = d
        simref_consistency[name] = m

    # ---- 统计汇总 ----
    rows_cmp = len(csv_rows)
    total_nan = sum(nan_cnt.values()) + sum(rep_nan.values()) + \
        sum(len(v) for v in health_bad.values())
    numerical_health = (total_nan == 0) and (runtime_error is None) and \
        (rows_cmp == n_steps)

    def _stat(seq):
        fin = [abs(v) for v in seq if math.isfinite(v)]
        if not fin:
            return 0.0, 0.0
        return max(fin), sum(fin) / len(fin)

    derived_report = {}
    for name in REPORT_KEYS:
        mx, mn = _stat(rep_d[name])
        derived_report[name] = {
            "max_abs_d": mx, "mean_abs_d": mn, "n": len(rep_d[name]),
            "nan_count": rep_nan[name],
        }
    mx, mn = _stat(t_s_dev_tick)
    derived_report["t_s_dev_vs_tick_end"] = {
        "max_abs": mx, "mean_abs": mn, "n": len(t_s_dev_tick),
        "note": "Y.t_s − (k+1)，ERT 阶段时刻写出的系统偏差（预期 ≈ −0.0025）",
    }
    derived_report["voyage_stop_stage_vs_next_tick_mismatch_count"] = vstop_mismatch
    derived_report["note"] = (
        "报告项不作门禁（design §4.4a）：wrapper Y 为 ERT 阶段时刻 t=(k+1)−0.0025 "
        "读数，sim 侧为 gp1 replay tick 序列线性插值，残差为仪器时序特征")

    # ---- 门禁项（gate_result 风格） ----
    items = []
    metrics = {}
    state_comparison = {}
    for name in GATE_STATES:
        metric = STATE_METRIC[name]
        ok = (max_abs[name] <= THRESHOLD) and (nan_cnt[name] == 0)
        items.append({"metric": metric, "value": max_abs[name],
                      "threshold": THRESHOLD, "pass": ok})
        metrics[metric] = max_abs[name]
        state_comparison[name] = {
            "max_abs": max_abs[name], "tick_of_max": tick_of_max[name],
            "nan_count": nan_cnt[name],
            "sim_source": "gp1_replay" if name in STATE_STATES else "simref_csv",
        }
    items.append({"metric": "tick0_state_max_abs", "value": tick0_state_max_abs,
                  "threshold": THRESHOLD,
                  "pass": tick0_state_max_abs <= THRESHOLD})
    metrics["tick0_state_max_abs"] = tick0_state_max_abs
    items.append({"metric": "numerical_health", "value": bool(numerical_health),
                  "threshold": True, "pass": bool(numerical_health)})
    metrics["numerical_health"] = bool(numerical_health)
    metrics["rows_cmp"] = rows_cmp
    metrics["rows_expected"] = n_steps
    overall = all(it["pass"] for it in items)

    # ---- 产物写出 ----
    os.makedirs(out_dir, exist_ok=True)
    compare_path = os.path.join(out_dir, "v3_gp2_%s_compare.csv" % scenario)
    compare_header = (
        ["tick", "p_gen_kW", "t_stage"]
        + ["%s_%s" % (n_, sfx) for n_ in GATE_STATES for sfx in ("wrap", "sim", "d")]
        + ["t_s_wrap", "t_s_sim_interp", "t_s_d"]
        + ["%s_d" % n_ for n_ in REPORT_INTERP_KEYS]
    )
    with open(compare_path, "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(compare_header)
        for row in csv_rows:
            w.writerow([fmt17(v) for v in row])

    def rel(p):
        try:
            return os.path.relpath(p, repo_root)
        except ValueError:
            return os.path.abspath(p)

    gate = {
        "task": "T-605",
        "spec": "ems01-chil-v3-closedloop (FR-605/AC-602)",
        "generated": time.strftime("%Y-%m-%d %H:%M:%S"),
        "matlab_version": matlab_version,
        "sim_ref_matlab_version": sim_ref_matlab_version,
        "scenario": scenario,
        "backend": backend,
        "plantstep_dylib_sha256": (sha256_file(dylib_path)
                                   if os.path.isfile(dylib_path) else None),
        "build_manifest": {
            "path": rel(manifest_path),
            "run_id": manifest.get("run_id"),
            "model_sha256": manifest.get("model_sha256"),
            "parameters_sha256": manifest.get("parameters_sha256"),
            "matlab_version_string": manifest.get("matlab_version_string"),
        },
        "ref_csv": rel(replay_path),
        "sim_ref_csv": rel(sim_ref_path),
        "sim_ref_provenance": (
            "解释执行参考 = scripts/22_run_gp1_gate.m 以同一回放输入（ref_csv 的 "
            "P_gen_ref_kW 列 0.01s ZOH 网格）驱动同一 HybridShip_ModelD_PlantStep "
            "模型（MATLAB 解释执行）生成的逐拍轨迹（provenance: ref_G2 冻结基线 "
            "→ 22 号脚本 → gp1 replay CSV）"),
        "simref_csv": rel(simref_path),
        "simref_columns": list(SIMREF_COLUMNS),
        "simref_provenance": (
            "内部状态参考 = 主 Agent 以 MATLAB logsout 0.01s dense 数据在 tick 时刻"
            "采样导出（与 gp1 replay 同一解释执行运行；列定义见本脚本头注释）"),
        "thresholds": {"gate_max_abs": THRESHOLD, "numerical_health": True},
        "stage_time_semantics": (
            "门禁项=拍边界 X 直读（wrapper states()）；报告项=wrapper Y（ERT 阶段"
            "时刻 t=(k+1)−0.0025 写出，design §4.4a）vs sim 同刻线性插值"),
        "fuel_caliber": "codegen X CSTATE g ×1e-3 = kg；sim 侧 gp1 replay/simref 均为 kg",
        "p_gen_ticks_consumed": n_steps,
        "tick0": {
            "constructed": tick0_constructed,
            "ref_row0": tick0_row0,
            "field_abs_d": tick0_field_d,
            "wrapper_states_ic": tick0_ic,
            "state_max_abs": tick0_state_max_abs,
        },
        "items": items,
        "metrics": metrics,
        "state_comparison": state_comparison,
        "derived_report": derived_report,
        "simref_consistency": simref_consistency,
        "runtime_error": runtime_error,
        "overall": bool(overall),
    }
    gate_path = os.path.join(out_dir, "v3_gp2_%s_gate.json" % scenario)
    with open(gate_path, "w", encoding="utf-8") as fh:
        json.dump(gate, fh, ensure_ascii=False, indent=1)
        fh.write("\n")

    print("[G-P2] scenario=%s backend=%s rows_cmp=%d/%d overall=%s"
          % (scenario, backend, rows_cmp, n_steps, gate["overall"]))
    print("[G-P2] gate: %s" % gate_path)
    print("[G-P2] compare: %s" % compare_path)
    if not overall:
        failed = [it["metric"] for it in items if not it["pass"]]
        print("FAIL: G-P2 门禁未通过: %s" % ", ".join(failed), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except GateStructuralError as exc:
        print("GATE:GP2:%s: %s" % (exc.code, exc.msg), file=sys.stderr)
        sys.exit(2)