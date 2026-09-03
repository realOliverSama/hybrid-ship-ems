#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_fw_params.py — 生成 Appli/App/ems_params.c/.h（T-503，spec ems01-chil-ecms-v1）。

参数单一来源（design.md §Data Model，FR-503/NFR-506），从 5 个只读源生成：

  1. WS/results/ecms_phase25/deploy/selected_model_weights_v2.json
       MLP-Q fp32 权重（W1/b1/W2/b2/W3/b3）、workclf 权重（conv1~3/fc1~3）、
       gate 常数（S1/S2/M/upstream_id）
  2. WS/datasets/fleet_norm_v2.json           X_mean/X_std(302)、y_mean/y_std
  3. WS/results/ecms_phase1/kS_calibration.json
       λ₀/K_soc/K_S/Λ_floor/SOC_ref/Ē_H/E_scale
  4. WS/config/ecms_bsfc_override.mat         BSFC 501 点表（tag 校验，非 7 点默认表）
  5. WS/config/Model_Parameters.m             η_rect/η_dcdc/Bat_Eff/SOC 限/Idle_Fuel/
                                              Grid dP/Gen_P_max/Pgen_init/dt_ctrl（人工核对清单）

确定性 / 幂等：输出文件内容是 5 个源文件字节的纯函数（含各源 sha256 溯源头）。
生成时间不写入文件（T-503 验收要求重跑逐字节一致），仅打印到 stdout。

用法：
    python3 Host/tools/gen_fw_params.py            # 在仓库任意位置（自动定位仓库根）
    python3 Host/tools/gen_fw_params.py --check    # 只校验“源→现文件”一致性，不重写

环境：model/simulink/.venv_tf（numpy + scipy，不另建环境）。
"""
import argparse
import ast
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone

import numpy as np
from scipy.io import loadmat

# ---------------------------------------------------------------- 路径定位（§20.2：禁绝对路径）

def find_repo_root(start_dir):
    """自脚本位置向上找仓库根：同时含 AGENTS.md、model/、firmware/ 的目录。"""
    d = os.path.abspath(start_dir)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise SystemExit("ERROR: 无法定位仓库根（缺少 AGENTS.md + model/ + firmware/ 锚点）")
        d = parent


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = find_repo_root(SCRIPT_DIR)
WS = os.path.join(REPO_ROOT, "model", "simulink_modelD_(EMS_NN_AECMS)")
OUT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "Appli", "App"))

SOURCES = [
    ("selected_model_weights_v2.json", os.path.join(WS, "results", "ecms_phase25", "deploy", "selected_model_weights_v2.json")),
    ("fleet_norm_v2.json",             os.path.join(WS, "datasets", "fleet_norm_v2.json")),
    ("kS_calibration.json",            os.path.join(WS, "results", "ecms_phase1", "kS_calibration.json")),
    ("ecms_bsfc_override.mat",         os.path.join(WS, "config", "ecms_bsfc_override.mat")),
    ("Model_Parameters.m",             os.path.join(WS, "config", "Model_Parameters.m")),
    # 6) FR-511 编译期换型（spec ems01-chil-tcn64-swap）：TCN-f64 预测器权重单一来源
    ("selected_model_weights_tcnq.json", os.path.join(WS, "results", "ecms_phase25", "models", "selected_model_weights_tcnq.json")),
]

BSFC_TAG_EXPECTED = "ecms-bsfc-family-mid-d25=0.50-agg-v0.1.0"

# design §Data Model 人工核对清单（Model_Parameters.m 解析结果必须与之一致）
PLANT_EXPECT = {
    "ASSUMED_eta_rect": 0.97,
    "ASSUMED_eta_dcdc": 0.97,
    "ASSUMED_Bat_Eff_Discharge": 0.95,
    "ASSUMED_Bat_Eff_Charge": 0.95,
    "ECMS_SOC_Discharge_Max": 0.105,
    "ECMS_SOC_Charge_Min": 0.945,
    "ECMS_Idle_Fuel_kgph": 6.795,
    "ECMS_Grid_dP_kW": 20.0,
    "ECMS_dt_ctrl_s": 1.0,
    "Pgen_init": 0.0,
    "Gen_P_max": 1000.0,
}
# design §Data Model kS 标定常数（kS_calibration.json 键值必须与之一致）
KS_EXPECT = {
    "lambda_0": 0.425,
    "K_soc": 30.0,
    "chosen_K_S": 0.01,
    "lambda_floor": 0.43,
    "soc_target": 0.60,
}
EML_GATE_LITERAL = {"S1": 28.947700, "S2": 40.771000, "M": 3, "upstream": 6}  # EML:2-3

# ---------------------------------------------------------------- .m 简单赋值解析


def parse_m_assignments(text):
    """解析 Model_Parameters.m 中的 `NAME = <数值表达式>`（剥离 % 注释）。

    RHS 支持数字与 `NAME * NAME` 等四则表达式（如 Gen_P_max = A * B）。
    """
    vals = {}
    for raw in text.splitlines():
        line = raw.split("%", 1)[0]   # 本文件所需行值域内无 '%' 字面量
        line = line.split(";", 1)[0].strip()  # 只取首个语句（防多语句行）
        if not line:
            continue
        m = re.match(r"^([A-Za-z]\w*)\s*=\s*(.+?)\s*$", line)
        if not m:
            continue
        name, rhs = m.group(1), m.group(2)
        try:
            vals[name] = _eval_expr(rhs, vals)
        except (KeyError, ValueError, SyntaxError):
            continue  # 非数值表达式（向量/字符串/函数调用等）跳过
    return vals


def _eval_expr(expr, vals):
    node = ast.parse(expr, mode="eval").body
    return _eval_node(node, vals)


def _eval_node(node, vals):
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return float(node.value)
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.UAdd)):
        v = _eval_node(node.operand, vals)
        return -v if isinstance(node.op, ast.USub) else v
    if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Sub, ast.Mult, ast.Div)):
        a = _eval_node(node.left, vals)
        b = _eval_node(node.right, vals)
        if isinstance(node.op, ast.Add):
            return a + b
        if isinstance(node.op, ast.Sub):
            return a - b
        if isinstance(node.op, ast.Mult):
            return a * b
        return a / b
    if isinstance(node, ast.Name):
        return float(vals[node.id])  # 未知名/未定义名 → KeyError
    raise ValueError("unsupported expr")


# ---------------------------------------------------------------- 数值格式化（C double 字面量，最短往返）


def cnum(v):
    v = float(v)
    if not np.isfinite(v):
        raise ValueError("non-finite value in params: %r" % v)
    return repr(v)  # Python3 repr = 最短十进制往返表示，C99 合法字面量


def fmt_array(name, ctype, size, data, per_line=4):
    flat = np.asarray(data, dtype=np.float64).ravel()
    assert flat.size == size, "%s: size %d != %d" % (name, flat.size, size)
    if not np.all(np.isfinite(flat)):
        raise ValueError("non-finite entries in %s" % name)
    lines = []
    lines.append("const %s %s[%d] = {" % (ctype, name, size))
    for i in range(0, size, per_line):
        chunk = ", ".join(cnum(x) for x in flat[i:i + per_line])
        sep = "," if i + per_line < size else ""
        lines.append("    " + chunk + sep)
    lines.append("};")
    return "\n".join(lines)


# ---------------------------------------------------------------- 主流程


def load_all():
    data = {}
    # 1) 部署权重 JSON
    with open(SOURCES[0][1], "r", encoding="utf-8") as f:
        w = json.load(f)
    mlpq = w["weights"]
    clf = w["clf"]
    gate = w["gate"]
    dnorm = w["norm"]
    expect_shapes = [
        ("W1", mlpq, (302, 64)), ("b1", mlpq, (64,)),
        ("W2", mlpq, (64, 32)), ("b2", mlpq, (32,)),
        ("W3", mlpq, (32, 3)), ("b3", mlpq, (3,)),
        ("conv1_w", clf, (5, 1, 16)), ("conv1_b", clf, (16,)),
        ("conv2_w", clf, (3, 16, 32)), ("conv2_b", clf, (32,)),
        ("conv3_w", clf, (3, 32, 32)), ("conv3_b", clf, (32,)),
        ("fc1_w", clf, (2, 8)), ("fc1_b", clf, (8,)),
        ("fc2_w", clf, (40, 64)), ("fc2_b", clf, (64,)),
        ("fc3_w", clf, (64, 7)), ("fc3_b", clf, (7,)),
    ]
    for key, src, shape in expect_shapes:
        arr = np.array(src[key], dtype=np.float64)
        assert arr.shape == shape, "shape %s: %r != %r" % (key, arr.shape, shape)
        data[key] = arr
    # gate 常数：与 EML 字面量同源核对（design §Data Model）
    assert float(gate["S1_kwh"]) == EML_GATE_LITERAL["S1"], gate["S1_kwh"]
    assert float(gate["S2_kwh"]) == EML_GATE_LITERAL["S2"], gate["S2_kwh"]
    assert int(gate["hysteresis_windows"]) == EML_GATE_LITERAL["M"]
    assert int(gate["upstream_class_id"]) == EML_GATE_LITERAL["upstream"]
    data["gate"] = (float(gate["S1_kwh"]), float(gate["S2_kwh"]),
                    int(gate["hysteresis_windows"]), int(gate["upstream_class_id"]))

    # 2) 归一化（以 fleet_norm_v2.json 为准；与部署 JSON 内副本逐位一致才允许生成）
    with open(SOURCES[1][1], "r", encoding="utf-8") as f:
        n = json.load(f)
    x_mean = np.array(n["X_mean"], dtype=np.float64)
    x_std = np.array(n["X_std"], dtype=np.float64)
    assert x_mean.shape == (302,) and x_std.shape == (302,)
    assert np.array_equal(x_mean, np.array(dnorm["X_mean"], dtype=np.float64)), \
        "fleet_norm_v2 与 deploy JSON 的 X_mean 不一致"
    assert np.array_equal(x_std, np.array(dnorm["X_std"], dtype=np.float64)), \
        "fleet_norm_v2 与 deploy JSON 的 X_std 不一致"
    assert float(n["y_mean"]) == float(dnorm["y_mean"]) and float(n["y_std"]) == float(dnorm["y_std"])
    assert np.all(x_std > 0), "X_std 含非正值"
    data["x_mean"], data["x_std"] = x_mean, x_std
    data["y_mean"], data["y_std"] = float(n["y_mean"]), float(n["y_std"])

    # 3) kS 标定常数
    with open(SOURCES[2][1], "r", encoding="utf-8") as f:
        ks = json.load(f)
    got = {k: float(ks[k]) for k in KS_EXPECT}
    for k, v in KS_EXPECT.items():
        assert got[k] == v, "kS %s: %r != %r" % (k, got[k], v)
    ebar = float(ks["oracle"]["E_bar_H_kWh"])
    escale = float(ks["oracle"]["E_scale_kWh"])
    assert abs(ebar - 36.8933) < 1e-3 and abs(escale - 20.5069) < 1e-3, (ebar, escale)
    data["ks"] = (got["lambda_0"], got["K_soc"], got["chosen_K_S"],
                  got["lambda_floor"], got["soc_target"], ebar, escale)

    # 4) BSFC override 501 点表
    mat = loadmat(SOURCES[3][1])
    tag = str(mat["override_tag"][0])
    assert tag == BSFC_TAG_EXPECTED, "BSFC tag: %r" % tag
    bp = np.asarray(mat["BSFC_Breakpoints_kW"], dtype=np.float64).ravel()
    tbl = np.asarray(mat["BSFC_Table_gkWh"], dtype=np.float64).ravel()
    assert bp.size == 501 and tbl.size == 501, (bp.size, tbl.size)
    assert np.all(np.diff(bp) > 0), "BSFC bp 非严格递增"
    data["bsfc_bp"], data["bsfc_tbl"] = bp, tbl

    # 5) Model_Parameters.m 人工核对清单
    with open(SOURCES[4][1], "r", encoding="utf-8") as f:
        mvals = parse_m_assignments(f.read())
    plant = {}
    for k, v in PLANT_EXPECT.items():
        assert k in mvals, "Model_Parameters.m 缺少 %s" % k
        assert mvals[k] == v, "%s: %r != %r（design §Data Model 人工核对清单）" % (k, mvals[k], v)
        plant[k] = mvals[k]
    data["plant"] = plant

    # 6) TCN-f64 预测器权重（FR-511 换型，design §Data Model；flat 布局与表名一一对应）
    with open(SOURCES[5][1], "r", encoding="utf-8") as f:
        tq = json.load(f)
    tcnq = tq["weights"]
    tcn_shapes = [
        ("b0_conv1_w", (3, 1, 64)), ("b0_conv1_b", (64,)),
        ("b0_conv2_w", (3, 64, 64)), ("b0_conv2_b", (64,)),
        ("b0_skip_w", (1, 1, 64)), ("b0_skip_b", (64,)),
    ]
    for i in range(1, 7):
        tcn_shapes += [("b%d_c1_w" % i, (3, 64, 64)), ("b%d_c1_b" % i, (64,)),
                       ("b%d_c2_w" % i, (3, 64, 64)), ("b%d_c2_b" % i, (64,))]
    tcn_shapes += [("head8_w", (2, 8)), ("head8_b", (8,)),
                   ("head64_w", (72, 64)), ("head64_b", (64,)),
                   ("out_w", (64, 3)), ("out_b", (3,))]
    for key, shape in tcn_shapes:
        arr = np.array(tcnq[key], dtype=np.float64).reshape(shape)
        data["tcn_" + key] = arr
    n_tcn = sum(int(v.size) for k, v in data.items() if k.startswith("tcn_"))
    assert n_tcn == 165851, "TCN 参数量 %d != 165851" % n_tcn
    # TCN JSON 内 norm 副本与 fleet_norm_v2 逐位一致（同一训练口径，§22.3）
    assert np.array_equal(x_mean, np.array(tq["norm"]["X_mean"], dtype=np.float64)), \
        "fleet_norm_v2 与 tcnq JSON 的 X_mean 不一致"
    assert float(n["y_mean"]) == float(tq["norm"]["y_mean"]) and float(n["y_std"]) == float(tq["norm"]["y_std"])
    return data


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(1 << 16), b""):
            h.update(blk)
    return h.hexdigest()


def generate(data):
    (s1, s2, gm, up) = data["gate"]
    (lam0, k_soc, k_s, lam_floor, soc_ref, ebar, escale) = data["ks"]
    p = data["plant"]

    provenance = "\n".join(
        "    %-32s sha256=%s" % (name, sha256_of(path)) for name, path in SOURCES)

    header = """/* GENERATED FILE — DO NOT EDIT.
 * ems_params.h — EMS01 CHIL ECMS 固件参数（T-503，spec ems01-chil-ecms-v1）
 * 生成器: Host/tools/gen_fw_params.py（单一来源，design.md §Data Model；FR-503/NFR-506）
 * 幂等性: 本文件内容是下列源文件字节的纯函数，重跑生成器逐字节一致；
 *         生成时间不入文件（T-503 幂等验收），由生成器 stdout 记录。
 *
 * 溯源（sha256）:
{prov}
 *
 * 布局约定（与 EML eml_ecms_step_mode2.m / selected_model_weights_v2.json 同构）:
 *   Dense 权重 [in x out] 行主序（h = W.' * x + b 语义）;
 *   Conv 权重 [kernel x in_ch x out_ch]; 源值均为 float32 精度的 double。
 * 修改任何常数 = design 修订，须走主 Agent 决策；改源后重跑生成器并以 diff 审。
 */
#ifndef EMS_PARAMS_H
#define EMS_PARAMS_H

#define EMS_PARAMS_WIN_N    300u  /* P_load 滑窗长度（EML:31，300 s @1 Hz） */
#define EMS_PARAMS_FEAT_N   302u  /* = WIN_N + v_ship + accel（fleet_norm_v2 feature_layout） */
#define EMS_PARAMS_BSFC_N   501u  /* ecms_bsfc_override.mat 501 点表（非 7 点默认表） */

/* ---- MLP-Q 302-64-32-3（q10/q50/q90，归一化域输出） ---- */
extern const double EMS_MLPQ_W1[302 * 64];
extern const double EMS_MLPQ_B1[64];
extern const double EMS_MLPQ_W2[64 * 32];
extern const double EMS_MLPQ_B2[32];
extern const double EMS_MLPQ_W3[32 * 3];
extern const double EMS_MLPQ_B3[3];

/* ---- workclf v1 工况识别器（causal Conv1D x3 + MaxPool + GAP + 旁路融合） ---- */
extern const double EMS_CLF_CONV1_W[5 * 1 * 16];
extern const double EMS_CLF_CONV1_B[16];
extern const double EMS_CLF_CONV2_W[3 * 16 * 32];
extern const double EMS_CLF_CONV2_B[32];
extern const double EMS_CLF_CONV3_W[3 * 32 * 32];
extern const double EMS_CLF_CONV3_B[32];
extern const double EMS_CLF_FC1_W[2 * 8];
extern const double EMS_CLF_FC1_B[8];
extern const double EMS_CLF_FC2_W[40 * 64];
extern const double EMS_CLF_FC2_B[64];
extern const double EMS_CLF_FC3_W[64 * 7];
extern const double EMS_CLF_FC3_B[7];

/* ---- TCN-f64 预测器（FR-511 编译期换型，selected_model_weights_tcnq.json；
 *      spec ems01-chil-tcn64-swap。Conv flat = [(kk*CIN+c)*COUT+f]（keras (k,Cin,Cout)
 *      行主序）；Dense W = [in*out] 行主序；全 float32 精度的 double。 ---- */
extern const double EMS_TCNQ_B0_CONV1_W[3 * 1 * 64];
extern const double EMS_TCNQ_B0_CONV1_B[64];
extern const double EMS_TCNQ_B0_CONV2_W[3 * 64 * 64];
extern const double EMS_TCNQ_B0_CONV2_B[64];
extern const double EMS_TCNQ_B0_SKIP_W[1 * 1 * 64];
extern const double EMS_TCNQ_B0_SKIP_B[64];
extern const double EMS_TCNQ_B1_C1_W[3 * 64 * 64];
extern const double EMS_TCNQ_B1_C1_B[64];
extern const double EMS_TCNQ_B1_C2_W[3 * 64 * 64];
extern const double EMS_TCNQ_B1_C2_B[64];
extern const double EMS_TCNQ_B2_C1_W[3 * 64 * 64];
extern const double EMS_TCNQ_B2_C1_B[64];
extern const double EMS_TCNQ_B2_C2_W[3 * 64 * 64];
extern const double EMS_TCNQ_B2_C2_B[64];
extern const double EMS_TCNQ_B3_C1_W[3 * 64 * 64];
extern const double EMS_TCNQ_B3_C1_B[64];
extern const double EMS_TCNQ_B3_C2_W[3 * 64 * 64];
extern const double EMS_TCNQ_B3_C2_B[64];
extern const double EMS_TCNQ_B4_C1_W[3 * 64 * 64];
extern const double EMS_TCNQ_B4_C1_B[64];
extern const double EMS_TCNQ_B4_C2_W[3 * 64 * 64];
extern const double EMS_TCNQ_B4_C2_B[64];
extern const double EMS_TCNQ_B5_C1_W[3 * 64 * 64];
extern const double EMS_TCNQ_B5_C1_B[64];
extern const double EMS_TCNQ_B5_C2_W[3 * 64 * 64];
extern const double EMS_TCNQ_B5_C2_B[64];
extern const double EMS_TCNQ_B6_C1_W[3 * 64 * 64];
extern const double EMS_TCNQ_B6_C1_B[64];
extern const double EMS_TCNQ_B6_C2_W[3 * 64 * 64];
extern const double EMS_TCNQ_B6_C2_B[64];
extern const double EMS_TCNQ_HEAD8_W[2 * 8];
extern const double EMS_TCNQ_HEAD8_B[8];
extern const double EMS_TCNQ_HEAD64_W[72 * 64];
extern const double EMS_TCNQ_HEAD64_B[64];
extern const double EMS_TCNQ_OUT_W[64 * 3];
extern const double EMS_TCNQ_OUT_B[3];

/* ---- 归一化（fleet_norm_v2.json，train-split 统计） ---- */
extern const double EMS_NORM_X_MEAN[302];
extern const double EMS_NORM_X_STD[302];
extern const double EMS_NORM_Y_MEAN;
extern const double EMS_NORM_Y_STD;

/* ---- kS 标定常数（kS_calibration.json，T-106 v2） ---- */
typedef struct {{
    double lambda0;        /* λ₀ [kg/kWh] */
    double k_soc;          /* K_soc [kg/kWh/SOC] */
    double k_s;            /* K_S [-]（K_S≠0 时 λ 下限钳位生效，EML:173） */
    double lambda_floor;   /* Λ_floor [kg/kWh] */
    double soc_ref;        /* SOC_ref [-] */
    double ebar_h_kwh;     /* Ē_H [kWh] */
    double e_scale_kwh;    /* E_scale [kWh] */
}} ems_ks_t;
extern const ems_ks_t EMS_KS;

/* ---- 门控常数（selected_model_weights_v2.json:gate，与 EML:2-3 同源） ---- */
typedef struct {{
    double s1_kwh;         /* GATE_S1 = val P75 [kWh]（禁调参） */
    double s2_kwh;         /* GATE_S2 = val P95 [kWh]（禁调参） */
    int    m_ticks;        /* GATE_M：连续同类 tick 数才切换（EML:143，3 tick = 3 s） */
    int    upstream_class; /* 可辨识受限类 id（=6，EML:149） */
}} ems_gatecst_t;
extern const ems_gatecst_t EMS_GATE;

/* ---- plant/ECMS 常数（Model_Parameters.m 人工核对清单，design §Data Model） ---- */
typedef struct {{
    double eta_rect;         /* 发电机整流效率 [-] */
    double eta_dcdc;         /* 电池 DC/DC 效率 [-] */
    double bat_eff_dis;      /* 电池放电效率 [-] */
    double bat_eff_chg;      /* 电池充电效率 [-] */
    double soc_discharge_max;/* SOC ≤ 此值禁放电（硬约束，EML:183） */
    double soc_charge_min;   /* SOC ≥ 此值禁充电（硬约束，EML:190） */
    double idle_fuel_kgph;   /* 机组怠速燃油 [kg/h]（EML:194，/3600 → kg/s） */
    double grid_dp_kw;       /* ECMS 网格步长 [kW] */
    double gen_p_max_kw;     /* 发电机聚合上限 [kW] */
    double pgen_init_kw;     /* Pheld 初值 [kW]（EML:29） */
    double dt_ctrl_s;        /* 控制周期 [s]（EML:47 tick 门） */
}} ems_plant_t;
extern const ems_plant_t EMS_PLANT;

/* ---- BSFC override 表（g/kWh，bp 严格递增 kW） ---- */
extern const double EMS_BSFC_BP[501];
extern const double EMS_BSFC_TBL[501];

#endif /* EMS_PARAMS_H */
""".format(prov=provenance)

    body = """/* GENERATED FILE — DO NOT EDIT.
 * ems_params.c — 数值定义；来源与布局约定见 ems_params.h 头注释。
 */
#include "ems_params.h"

{mlpq}

{clf}

/* ---- TCN-f64 预测器（FR-511 编译期换型；布局见 ems_params.h 声明块注释） ---- */
{tcnq}

/* ---- 归一化 ---- */
{xmean}

{xstd}

const double EMS_NORM_Y_MEAN = {ymean};
const double EMS_NORM_Y_STD  = {ystd};

/* ---- kS 标定常数 ---- */
const ems_ks_t EMS_KS = {{
    {lam0}, /* lambda0 */
    {ksoc}, /* k_soc */
    {ks},   /* k_s */
    {lfl},  /* lambda_floor */
    {srf},  /* soc_ref */
    {ebar}, /* ebar_h_kwh */
    {escl}, /* e_scale_kwh */
}};

/* ---- 门控常数 ---- */
const ems_gatecst_t EMS_GATE = {{
    {s1},  /* s1_kwh */
    {s2},  /* s2_kwh */
    {gm},  /* m_ticks */
    {up},  /* upstream_class */
}};

/* ---- plant/ECMS 常数 ---- */
const ems_plant_t EMS_PLANT = {{
    {etar}, /* eta_rect */
    {etad}, /* eta_dcdc */
    {edis}, /* bat_eff_dis */
    {echg}, /* bat_eff_chg */
    {sdmx}, /* soc_discharge_max */
    {scmn}, /* soc_charge_min */
    {idlf}, /* idle_fuel_kgph */
    {gdp},  /* grid_dp_kw */
    {gpmx}, /* gen_p_max_kw */
    {pgi},  /* pgen_init_kw */
    {dtc},  /* dt_ctrl_s */
}};

/* ---- BSFC override 501 点表 ---- */
{bsbp}

{bstbl}
""".format(
        mlpq="\n\n".join([
            fmt_array("EMS_MLPQ_W1", "double", 302 * 64, data["W1"]),
            fmt_array("EMS_MLPQ_B1", "double", 64, data["b1"]),
            fmt_array("EMS_MLPQ_W2", "double", 64 * 32, data["W2"]),
            fmt_array("EMS_MLPQ_B2", "double", 32, data["b2"]),
            fmt_array("EMS_MLPQ_W3", "double", 32 * 3, data["W3"]),
            fmt_array("EMS_MLPQ_B3", "double", 3, data["b3"]),
        ]),
        clf="\n\n".join([
            fmt_array("EMS_CLF_CONV1_W", "double", 5 * 1 * 16, data["conv1_w"]),
            fmt_array("EMS_CLF_CONV1_B", "double", 16, data["conv1_b"]),
            fmt_array("EMS_CLF_CONV2_W", "double", 3 * 16 * 32, data["conv2_w"]),
            fmt_array("EMS_CLF_CONV2_B", "double", 32, data["conv2_b"]),
            fmt_array("EMS_CLF_CONV3_W", "double", 3 * 32 * 32, data["conv3_w"]),
            fmt_array("EMS_CLF_CONV3_B", "double", 32, data["conv3_b"]),
            fmt_array("EMS_CLF_FC1_W", "double", 2 * 8, data["fc1_w"]),
            fmt_array("EMS_CLF_FC1_B", "double", 8, data["fc1_b"]),
            fmt_array("EMS_CLF_FC2_W", "double", 40 * 64, data["fc2_w"]),
            fmt_array("EMS_CLF_FC2_B", "double", 64, data["fc2_b"]),
            fmt_array("EMS_CLF_FC3_W", "double", 64 * 7, data["fc3_w"]),
            fmt_array("EMS_CLF_FC3_B", "double", 7, data["fc3_b"]),
        ]),
        tcnq="\n\n".join(
            [fmt_array("EMS_TCNQ_B0_CONV1_W", "double", 3 * 1 * 64, data["tcn_b0_conv1_w"]),
             fmt_array("EMS_TCNQ_B0_CONV1_B", "double", 64, data["tcn_b0_conv1_b"]),
             fmt_array("EMS_TCNQ_B0_CONV2_W", "double", 3 * 64 * 64, data["tcn_b0_conv2_w"]),
             fmt_array("EMS_TCNQ_B0_CONV2_B", "double", 64, data["tcn_b0_conv2_b"]),
             fmt_array("EMS_TCNQ_B0_SKIP_W", "double", 1 * 1 * 64, data["tcn_b0_skip_w"]),
             fmt_array("EMS_TCNQ_B0_SKIP_B", "double", 64, data["tcn_b0_skip_b"])]
            + [x for i in range(1, 7) for x in (
                fmt_array("EMS_TCNQ_B%d_C1_W" % i, "double", 3 * 64 * 64, data["tcn_b%d_c1_w" % i]),
                fmt_array("EMS_TCNQ_B%d_C1_B" % i, "double", 64, data["tcn_b%d_c1_b" % i]),
                fmt_array("EMS_TCNQ_B%d_C2_W" % i, "double", 3 * 64 * 64, data["tcn_b%d_c2_w" % i]),
                fmt_array("EMS_TCNQ_B%d_C2_B" % i, "double", 64, data["tcn_b%d_c2_b" % i]))]
            + [fmt_array("EMS_TCNQ_HEAD8_W", "double", 2 * 8, data["tcn_head8_w"]),
               fmt_array("EMS_TCNQ_HEAD8_B", "double", 8, data["tcn_head8_b"]),
               fmt_array("EMS_TCNQ_HEAD64_W", "double", 72 * 64, data["tcn_head64_w"]),
               fmt_array("EMS_TCNQ_HEAD64_B", "double", 64, data["tcn_head64_b"]),
               fmt_array("EMS_TCNQ_OUT_W", "double", 64 * 3, data["tcn_out_w"]),
               fmt_array("EMS_TCNQ_OUT_B", "double", 3, data["tcn_out_b"])]),
        xmean=fmt_array("EMS_NORM_X_MEAN", "double", 302, data["x_mean"]),
        xstd=fmt_array("EMS_NORM_X_STD", "double", 302, data["x_std"]),
        ymean=cnum(data["y_mean"]), ystd=cnum(data["y_std"]),
        lam0=cnum(lam0), ksoc=cnum(k_soc), ks=cnum(k_s), lfl=cnum(lam_floor),
        srf=cnum(soc_ref), ebar=cnum(ebar), escl=cnum(escale),
        s1=cnum(s1), s2=cnum(s2), gm=gm, up=up,
        etar=cnum(p["ASSUMED_eta_rect"]), etad=cnum(p["ASSUMED_eta_dcdc"]),
        edis=cnum(p["ASSUMED_Bat_Eff_Discharge"]), echg=cnum(p["ASSUMED_Bat_Eff_Charge"]),
        sdmx=cnum(p["ECMS_SOC_Discharge_Max"]), scmn=cnum(p["ECMS_SOC_Charge_Min"]),
        idlf=cnum(p["ECMS_Idle_Fuel_kgph"]), gdp=cnum(p["ECMS_Grid_dP_kW"]),
        gpmx=cnum(p["Gen_P_max"]), pgi=cnum(p["Pgen_init"]), dtc=cnum(p["ECMS_dt_ctrl_s"]),
        bsbp=fmt_array("EMS_BSFC_BP", "double", 501, data["bsfc_bp"]),
        bstbl=fmt_array("EMS_BSFC_TBL", "double", 501, data["bsfc_tbl"]),
    )
    return header, body


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true",
                    help="只校验源与既有生成物一致（不重写）")
    args = ap.parse_args()

    data = load_all()
    header, body = generate(data)

    h_path = os.path.join(OUT_DIR, "ems_params.h")
    c_path = os.path.join(OUT_DIR, "ems_params.c")

    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    print("gen_fw_params: %s (UTC)" % now)
    for name, path in SOURCES:
        print("  source %-32s sha256=%s" % (name, sha256_of(path)))

    if args.check:
        ok = True
        for path, content in ((h_path, header), (c_path, body)):
            with open(path, "r", encoding="utf-8") as f:
                cur = f.read()
            if cur != content:
                ok = False
                print("  MISMATCH: %s 与重新生成结果不一致（源已变更？请重跑生成器并以 diff 审）" % path)
        print("check: %s" % ("OK — ems_params.c/.h 与 5 源一致（幂等）" if ok else "FAIL"))
        return 0 if ok else 1

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(h_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
    with open(c_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(body)
    print("written: %s (%d B)" % (h_path, len(header.encode("utf-8"))))
    print("written: %s (%d B)" % (c_path, len(body.encode("utf-8"))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
