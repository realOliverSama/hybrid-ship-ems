#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_ctrl_diff.py — EMS01 CHIL ECMS L0 主机单元用例（T-503，spec ems01-chil-ecms-v1）。

职责（design §Testing L0 / tasks T-503 Validation）：
  1. host gcc `-Wall -Wextra` 编译 ems_ctrl.c + ems_infer_ref.c + ems_params.c +
     test_driver.c，零警告为硬性检查（附加 `-ffp-contract=off` 固定 IEEE 运算树，
     使 C 与 Python 镜像逐位可比；固件侧 CMake 建议同选项，见任务报告）。
  2. 以纯 Python 逐条镜像 EML eml_ecms_step_mode2.m（`ecms_nn`）为语义基准，
     对驱动二进制（白盒缝合面 + 全拍 step）做逐位（double bit-exact）比对。
  3. 覆盖 scout 11 条裸机复现风险点 + 充电禁止分支 + G0~G3 门控 K 计算
     （用例名见 CASES 与各 test_* docstring）。
  4. gen_fw_params.py 幂等验证（重跑逐字节一致 + 头部 sha256 与 5 源实算一致）。
  5. AGENTS.md §20.2 绝对路径扫描（本任务新增文件无命中）。

--predictor tcnq（T-805，spec ems01-chil-tcn64-swap；closure 后继修订）：改编
  ems_infer_tcn.c（TCN-f64 REF 前向，依赖闭包精确增量实现，无跨拍状态；原
  plain 流式 fast/cold 双路经 L0 数值实验否决后主 Agent 裁决 closure 方案，
  见该文件头注释），内部宏改名 #include ems_infer_ref.c 复用 workclf（不得
  再链 ems_infer_ref.c——重复符号），执行 L0 host 差分：val200 向量
  （seed=20260831）+ 32 随机窗（seed=20260902）+ S20/voyage002 全轨迹，参考
  = keras float32（export_tcnq_reference.py），比对统一 kWh 域；门禁
  |Δq50| ≤ 1 LSB_TCN = 0.266214 kWh 逐 tick + P_gen 网格一致率 ≥99% + 链数值
  健康（合法网格点、无 FALLBACK 误触发）；产物 Host/results/tcn_swap/
  l0_tcn_diff.json。另增 test_tcnq_stream_diff（closure 后继）：closure 主链
  vs 同文件 -DEMS_TCN_BATCH_REF 测试专用 batch 前向位级断言（max|Δ|=0，
  val200/random32 向量级 + 全轨迹 step 全字段级）+ vs-keras LSB 分布汇总，
  产物 l0_tcn_stream_diff.json。其余结构用例两模式共享（决策链与预测器无关
  部分）。默认（无参数）= mlpq，v1 既有行为不变。

运行：model/simulink/.venv_tf/bin/python Host/tests/test_ctrl_diff.py [--predictor tcnq]
"""
import argparse
import csv
import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

# ---------------------------------------------------------------- 路径定位（§20.2：禁绝对路径）


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
# 工程自指（closure 后继任务，spec ems01-chil-tcn64-swap）：本文件位于
# EMS01_CHIL_ECMS_TCN64 工程内，App/工具/结果一律取本工程——工程副本初版 P
# 常量误指 EMS01_CHIL_ECMS，会使 P3 测试链编译 P2 文件并覆写 P2 results
# （违反工程隔离与 §8 车道规则）；§20.2 相对路径纪律：由 __file__ 推导。
P = os.path.dirname(os.path.dirname(HERE))
APP = os.path.join(P, "Appli", "App")
TOOLS = os.path.join(P, "Host", "tools")

sys.dont_write_bytecode = True  # 不在工程树内留 __pycache__
sys.path.insert(0, TOOLS)
import gen_fw_params  # noqa: E402  复用同一加载/校验代码，保证镜像常数与生成物同源

VENV_PY = os.path.join(REPO, "model", "simulink", ".venv_tf", "bin", "python")
GEN_PY = os.path.join(TOOLS, "gen_fw_params.py")

NEW_FILES = [
    "Appli/App/ems_config.h", "Appli/App/ems_params.h", "Appli/App/ems_params.c",
    "Appli/App/ems_infer.h", "Appli/App/ems_infer_ref.c", "Appli/App/ems_infer_tcn.c",
    "Appli/App/ems_ctrl.h", "Appli/App/ems_ctrl.c",
    "Host/tools/gen_fw_params.py", "Host/tests/test_driver.c",
    "Host/tests/test_ctrl_diff.py",
    "Host/results/tcn_swap/l0_tcn_diff.json",
    "Host/results/tcn_swap/l0_tcn_stream_diff.json",
]

# spec ems01-chil-tcn64-swap 新增（WS 侧），相对 WS 根扫描
NEW_FILES_WS = [
    "python/export_tcn_deploy.py",
    "python/export_tcnq_reference.py",
    "results/ecms_phase25/models/selected_model_weights_tcnq.json",
    "results/ecms_phase25/deploy/m3_tcn_swap_calib_report.json",
]

WIN = 300
FEAT = 302
DT = 1.0
GATE_S1 = 28.9477
GATE_S2 = 40.771
GATE_M = 3
UPSTREAM = 6

# ---- T-805（spec ems01-chil-tcn64-swap）L0 差分常数（design §Testing T2，先验设定） ----
LSB_TCN_KWH = 0.266214     # 1 LSB_TCN = out_scale(0.0161389485) × y_std(16.495129)
N_RANDOM32 = 32
SEED_RANDOM32 = 20260902
GRID_CONSIST_MIN = 0.99
TCN_DIL = (1, 2, 4, 8, 16, 32, 32)
TCN_Q_TOL_KWH = 1e-8       # C double vs numpy float64 求和序噪声界（观测 ~1e-10），
                           # 远低于 LSB_TCN 门；结构错误为 kWh 量级，可清晰区分


def fmax(a, b):
    """C fmax 语义：NaN 退让（fmax(NaN,x)=x），与 MATLAB max(NaN,x)=x 一致。"""
    return a if a > b else b


# ---------------------------------------------------------------- EML 镜像（语义基准逐条转写）


class Mirror:
    """eml_ecms_step_mode2.m `ecms_nn` 的纯 Python 转写（运算树与 C 移植一致）。

    常数经 gen_fw_params.load_all() 注入（与 ems_params.c 同一来源）。
    每处注明 EML 行号；这是 L0 比对的“期望值生成器”。
    """

    def __init__(self, data, gate_mode=2, predictor="mlpq"):
        self.d = data
        self.mode = gate_mode
        p = data["plant"]
        self.eta_rect = p["ASSUMED_eta_rect"]
        self.eta_dcdc = p["ASSUMED_eta_dcdc"]
        self.eff_dis = p["ASSUMED_Bat_Eff_Discharge"]
        self.eff_chg = p["ASSUMED_Bat_Eff_Charge"]
        self.soc_dmax = p["ECMS_SOC_Discharge_Max"]
        self.soc_cmin = p["ECMS_SOC_Charge_Min"]
        self.idle = p["ECMS_Idle_Fuel_kgph"]
        self.dp = p["ECMS_Grid_dP_kW"]
        self.pmax = p["Gen_P_max"]
        self.pgen_init = p["Pgen_init"]
        self.dt = p["ECMS_dt_ctrl_s"]
        (self.lam0, self.k_soc, self.k_s, self.lam_floor,
         self.soc_ref, self.ebar, self.escale) = data["ks"]
        self.bsfc_bp = [float(v) for v in data["bsfc_bp"]]
        self.bsfc_tbl = [float(v) for v in data["bsfc_tbl"]]
        self.nb = len(self.bsfc_bp)
        self.w1 = [[float(v) for v in row] for row in data["W1"]]
        self.b1 = [float(v) for v in data["b1"]]
        self.w2 = [[float(v) for v in row] for row in data["W2"]]
        self.b2 = [float(v) for v in data["b2"]]
        self.w3 = [[float(v) for v in row] for row in data["W3"]]
        self.b3 = [float(v) for v in data["b3"]]
        self.xm = [float(v) for v in data["x_mean"]]
        self.xs = [float(v) for v in data["x_std"]]
        self.ym = float(data["y_mean"])
        self.ys = float(data["y_std"])
        c1w, c2w, c3w = data["conv1_w"], data["conv2_w"], data["conv3_w"]
        self.c1w = [[[float(c1w[k][c][f]) for f in range(16)] for c in range(1)] for k in range(5)]
        self.c1b = [float(v) for v in data["conv1_b"]]
        self.c2w = [[[float(c2w[k][c][f]) for f in range(32)] for c in range(16)] for k in range(3)]
        self.c2b = [float(v) for v in data["conv2_b"]]
        self.c3w = [[[float(c3w[k][c][f]) for f in range(32)] for c in range(32)] for k in range(3)]
        self.c3b = [float(v) for v in data["conv3_b"]]
        self.f1w = [[float(v) for v in row] for row in data["fc1_w"]]
        self.f1b = [float(v) for v in data["fc1_b"]]
        self.f2w = [[float(v) for v in row] for row in data["fc2_w"]]
        self.f2b = [float(v) for v in data["fc2_b"]]
        self.f3w = [[float(v) for v in row] for row in data["fc3_w"]]
        self.f3b = [float(v) for v in data["fc3_b"]]
        self.initialized = False
        # 预测器槽位（FR-511 编译期换型）：默认 mlpq 逐位镜像不变；tcnq = numpy
        # float64 镜像 ems_infer_tcn.c（同源权重 data["tcn_*"]，求和序不同 →
        # 双精度噪声 ~1e-10 kWh，远低于 LSB_TCN 门）。
        self.predictor = predictor
        self.predict = self.tcnq if predictor == "tcnq" else self.mlpq

    # ---- 密集层单元（EML:65-67 语义：升序累加，后加偏置，可选 ReLU） ----
    def dense_unit(self, w, stride, x, b, j, relu):
        acc = w[0][j] * x[0]
        for i in range(1, len(x)):
            acc += w[i][j] * x[i]
        acc += b[j]
        return fmax(acc, 0.0) if relu else acc

    def mlpq(self, x):
        h1 = [self.dense_unit(self.w1, 64, x, self.b1, j, True) for j in range(64)]
        h2 = [self.dense_unit(self.w2, 32, h1, self.b2, j, True) for j in range(32)]
        return [self.dense_unit(self.w3, 3, h2, self.b3, j, False) for j in range(3)]

    # ---- TCN-f64 前向（ems_infer_tcn.c 的 numpy float64 镜像；布局 = design §Data Model 2） ----

    @staticmethod
    def _tcn_conv(x, w, b, dil):
        """causal dilated Conv1D(k=3)+ReLU：out[t] = b + Σ_{kk 升序} x[t-(2-kk)*d] @ w[kk]
        （W 布局 [kk][cin][cout] = C 的 [(kk*CIN+c)*COUT+f] flat；通道点积交给
        BLAS，double 噪声 ~1e-15 相对，与 C 标量序差异远低于门禁）。"""
        import numpy as np
        tn = x.shape[0]
        out = np.tile(b, (tn, 1))
        for kk in range(3):
            off = (2 - kk) * dil
            if tn > off:
                out[off:] += x[:tn - off] @ w[kk]
        return np.maximum(out, 0.0)

    def tcnq(self, x):
        """TCN-f64 前向：x302（归一化）→ q3_norm。7 残差块（block0 线性 skip
        1×1 conv 1→64）→ LastTimeStep + Dense(2→8) 旁路 concat → Dense(72→64)
        → Dense(64→3)。块语义与 ems_infer_tcn.c/keras 同构：
        out = relu(x_in + relu(conv2(relu(conv1(x_in)))))。"""
        import numpy as np
        d = self.d
        cur = np.asarray(x[:300], dtype=np.float64).reshape(WIN, 1)
        aux = np.asarray(x[300:302], dtype=np.float64)
        for i in range(7):
            di = TCN_DIL[i]
            w1 = d["tcn_b0_conv1_w"] if i == 0 else d["tcn_b%d_c1_w" % i]
            b1 = d["tcn_b0_conv1_b"] if i == 0 else d["tcn_b%d_c1_b" % i]
            c1 = self._tcn_conv(cur, w1, b1, di)
            w2 = d["tcn_b0_conv2_w"] if i == 0 else d["tcn_b%d_c2_w" % i]
            b2 = d["tcn_b0_conv2_b"] if i == 0 else d["tcn_b%d_c2_b" % i]
            c2 = self._tcn_conv(c1, w2, b2, di)
            if i == 0:
                skip = d["tcn_b0_skip_b"] + cur[:, 0:1] * d["tcn_b0_skip_w"][0, 0, :][None, :]
                cur = np.maximum(skip + c2, 0.0)
            else:
                cur = np.maximum(cur + c2, 0.0)
        s8 = np.maximum(aux @ d["tcn_head8_w"] + d["tcn_head8_b"], 0.0)
        z = np.concatenate([cur[-1], s8])
        h = np.maximum(z @ d["tcn_head64_w"] + d["tcn_head64_b"], 0.0)
        return h @ d["tcn_out_w"] + d["tcn_out_b"]

    def workclf(self, pdec, v, a):
        # EML:83-95 conv1（causal 核 5，acc 从偏置起）
        c1 = [[0.0] * 16 for _ in range(30)]
        for f in range(16):
            for t in range(30):
                acc = self.c1b[f]
                for kk in range(5):
                    j = t + kk - 4
                    if j >= 0:
                        acc += self.c1w[kk][0][f] * pdec[j]
                c1[t][f] = fmax(acc, 0.0)
        # EML:96-99 pool1（30→15）
        p1 = [[fmax(c1[2 * t][f], c1[2 * t + 1][f]) for f in range(16)] for t in range(15)]
        # EML:100-112 conv2（causal 核 3，16→32；通道点积后再加 acc）
        c2 = [[0.0] * 32 for _ in range(15)]
        for f in range(32):
            for t in range(15):
                acc = self.c2b[f]
                for kk in range(3):
                    j = t + kk - 2
                    if j >= 0:
                        s = 0.0
                        for c in range(16):
                            s += p1[j][c] * self.c2w[kk][c][f]
                        acc += s
                c2[t][f] = fmax(acc, 0.0)
        # EML:113-116 pool2（15→7，丢尾：第 15 行弃用）
        p2 = [[fmax(c2[2 * t][f], c2[2 * t + 1][f]) for f in range(32)] for t in range(7)]
        # EML:117-129 conv3（causal 核 3，32→32）
        c3 = [[0.0] * 32 for _ in range(7)]
        for f in range(32):
            for t in range(7):
                acc = self.c3b[f]
                for kk in range(3):
                    j = t + kk - 2
                    if j >= 0:
                        s = 0.0
                        for c in range(32):
                            s += p2[j][c] * self.c3w[kk][c][f]
                        acc += s
                c3[t][f] = fmax(acc, 0.0)
        # EML:130 GAP（时间维均值）
        gfeat = []
        for c in range(32):
            s = c3[0][c]
            for t in range(1, 7):
                s += c3[t][c]
            gfeat.append(s / 7.0)
        # EML:131 旁路 s8
        s8 = []
        for j in range(8):
            acc = self.f1w[0][j] * v
            acc += self.f1w[1][j] * a
            s8.append(fmax(acc + self.f1b[j], 0.0))
        # EML:132 hcl
        x40 = gfeat + s8
        hcl = [self.dense_unit(self.f2w, 64, x40, self.f2b, j, True) for j in range(64)]
        # EML:133 ocl
        ocl = [self.dense_unit(self.f3w, 7, hcl, self.f3b, j, False) for j in range(7)]
        # EML:134 argmax 平局取首
        best = 0
        for j in range(1, 7):
            if ocl[j] > ocl[best]:
                best = j
        return best + 1

    # ---- EML:196-206 BSFC ----
    def bsfc_at(self, bp, tbl, nb, Pg):
        bsfc = tbl[0]
        if Pg >= bp[nb - 1]:
            bsfc = tbl[nb - 1]
        elif Pg > bp[0]:
            ii = 0
            while ii < nb - 2 and bp[ii + 1] < Pg:
                ii += 1
            w = (Pg - bp[ii]) / (bp[ii + 1] - bp[ii])
            bsfc = tbl[ii] + w * (tbl[ii + 1] - tbl[ii])
        return bsfc

    # ---- EML:176-222 网格穷举 ----
    def grid_search(self, c, P_load, SOC, lam):
        n = int(math.floor(c["pmax"] / c["dp"])) + 1
        Pbest = 0.0
        Jbest = float("inf")
        nb = c["nb"]
        for i in range(n):
            Pg = float(i) * c["dp"]
            Pbat = P_load - c["eta_rect"] * Pg
            if SOC <= c["soc_dmax"]:
                if Pbat > 1e-9:
                    continue
                Pbest = Pg
                break
            if SOC >= c["soc_cmin"] and Pbat < 0.0:
                continue
            if Pg < 1e-9:
                mf = c["idle"] / 3600.0
            else:
                bsfc = self.bsfc_at(c["bp"], c["tbl"], nb, Pg)
                mf = Pg * bsfc / 3.6e6
            if Pbat > 0.0:
                mbe = Pbat / (c["eta_dcdc"] * c["eff_dis"]) / 3600.0
            else:
                mbe = Pbat * (c["eta_dcdc"] * c["eff_chg"]) / 3600.0
            J = mf + lam * mbe
            if J < Jbest:
                Jbest = J
                Pbest = Pg
        if SOC <= c["soc_dmax"] and Pbest < 1e-9 and P_load > c["eta_rect"] * c["pmax"]:
            Pbest = c["pmax"]
        return Pbest

    @staticmethod
    def hysteresis(g, cls):
        if g["cls_eff"] < 0:
            g["cls_eff"], g["run_cls"], g["run_cnt"] = cls, cls, 0
        elif cls == g["cls_eff"]:
            g["run_cls"], g["run_cnt"] = cls, 0
        elif cls == g["run_cls"]:
            g["run_cnt"] += 1
            if g["run_cnt"] >= GATE_M:
                g["cls_eff"], g["run_cnt"] = cls, 0
        else:
            g["run_cls"], g["run_cnt"] = cls, 1

    @staticmethod
    def gate_kw(cls_eff):
        return 0 if cls_eff == UPSTREAM else 1

    @staticmethod
    def gate_kq(s_q):
        if s_q <= GATE_S1:
            return 1.0
        if s_q >= GATE_S2:
            return 0.0
        return (GATE_S2 - s_q) / (GATE_S2 - GATE_S1)

    @staticmethod
    def gate_combine(mode, k_q, k_w):
        if mode == 0:
            return 1.0
        if mode == 1:
            return k_q
        if mode == 2:
            return float(k_w)
        return k_q * float(k_w)

    def real_cst(self):
        return {
            "eta_rect": self.eta_rect, "eta_dcdc": self.eta_dcdc,
            "eff_dis": self.eff_dis, "eff_chg": self.eff_chg,
            "soc_dmax": self.soc_dmax, "soc_cmin": self.soc_cmin,
            "idle": self.idle, "dp": self.dp, "pmax": self.pmax,
            "bp": self.bsfc_bp, "tbl": self.bsfc_tbl, "nb": self.nb,
        }

    # ---- EML:26-224 单拍（含 C 观测面默认值的复刻） ----
    def step(self, t, P_load, v_ship, SOC, nn_ok):
        if not self.initialized:
            self.lastT = -1e9
            self.Pheld = self.pgen_init
            self.k = 0
            self.buf = [P_load] * WIN
            self.vprev = v_ship
            self.g = {"cls_eff": -1, "run_cls": -1, "run_cnt": 0}
            self.initialized = True
        if t < self.lastT:
            self.lastT = t
            self.k = 0
            self.buf = [P_load] * WIN
            self.vprev = v_ship
            self.g = {"cls_eff": -1, "run_cls": -1, "run_cnt": 0}
        P = self.Pheld
        if t - self.lastT < self.dt - 1e-9:
            return {"held": 1, "ok": 1, "P": P}
        self.lastT = t
        if math.isfinite(P_load):
            self.buf[self.k] = P_load
        self.k = (self.k + 1) % WIN
        accel = v_ship - self.vprev
        self.vprev = v_ship
        ok = (nn_ok != 0) and math.isfinite(P_load) and math.isfinite(v_ship) \
            and math.isfinite(accel)
        xP = self.buf[self.k:] + self.buf[:self.k]
        x = xP + [v_ship, accel]
        for xi in x:
            if not math.isfinite(xi):
                ok = False
                break
        out = {"held": 0, "ok": 1, "P": 0.0, "e_hat": 0.0, "e_hat_raw": 0.0,
               "K": 1.0, "K_q": 1.0, "K_w": 1, "cls_eff": self.g["cls_eff"],
               "cls": 0, "cls_inv": 0, "q10": 0.0, "q50": 0.0, "q90": 0.0,
               "sq": 0.0}
        e_hat = 0.0
        if ok:
            xnorm = [(x[i] - self.xm[i]) / self.xs[i] for i in range(FEAT)]
            qn = self.predict(xnorm)
            q10 = qn[0] * self.ys + self.ym
            q50 = qn[1] * self.ys + self.ym
            q90 = qn[2] * self.ys + self.ym
            e_hat_raw = (q50 - self.ebar) / self.escale
            s_q = q90 - q10
            k_q = self.gate_kq(s_q)
            # 2026-09 口径修订：workclf 输入 = 同一 xnorm 的切片（xnorm[10*j]、
            # xnorm[300]/[301]，与固件 ems_ctrl.c 一致；T-312 训练口径
            # X[:, :300:10]+X[:,300:]；修订前 raw 口径致 int8 饱和坍缩）。
            pdec = [xnorm[10 * j] for j in range(30)]
            cls = self.workclf(pdec, xnorm[300], xnorm[301])
            if 1 <= cls <= 7:
                self.hysteresis(self.g, cls)
                cls_inv = 0
            else:
                cls_inv = 1
            k_w = 1 if cls_inv else self.gate_kw(self.g["cls_eff"])
            k = self.gate_combine(self.mode, k_q, k_w)
            e_hat = k * e_hat_raw
            out.update({"e_hat_raw": e_hat_raw, "q10": q10, "q50": q50,
                        "q90": q90, "sq": s_q, "K_q": k_q, "K_w": k_w,
                        "K": k, "cls_eff": self.g["cls_eff"], "cls": cls,
                        "cls_inv": cls_inv})
        else:
            e_hat = 0.0
            out["K"] = 1.0
            out["cls_eff"] = self.g["cls_eff"]
        out["ok"] = 1 if ok else 0
        out["e_hat"] = e_hat
        soc_ref_eff = self.soc_ref + self.k_s * e_hat
        lam = self.lam0 + self.k_soc * (soc_ref_eff - SOC)
        if self.k_s != 0.0:
            lam = fmax(lam, self.lam_floor)
        Pbest = self.grid_search(self.real_cst(), P_load, SOC, lam)
        P = Pbest
        self.Pheld = Pbest
        out["P"] = P
        return out


# ---------------------------------------------------------------- 驱动器封装


class Driver:
    def __init__(self, exe):
        self.p = subprocess.Popen([exe], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, text=True)

    def cmd(self, line):
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()
        resp = self.p.stdout.readline().rstrip("\n")
        if not resp:
            raise RuntimeError("driver 无响应: %r" % line)
        return parse_kv(resp)

    def close(self):
        self.p.stdin.close()
        self.p.wait(timeout=10)


def _to_num(v):
    try:
        return int(v)
    except ValueError:
        try:
            return float(v)
        except ValueError:
            return v


def parse_kv(line):
    key, _, rest = line.partition(" ")
    out = {"_key": key}
    for tok in rest.split():
        k, sep, v = tok.partition("=")
        if sep:
            out[k] = _to_num(v)
        else:
            out[key] = _to_num(k)  # 单裸值行（GRID/BSFC/KQ/KW/KCOMB/ARGMAX/RESET）
    return out


def eq(a, b):
    if isinstance(a, float) and isinstance(b, float):
        if math.isnan(a) and math.isnan(b):
            return True
    return a == b


# ---------------------------------------------------------------- 用例集

CASES = []


def case(fn):
    CASES.append(fn)
    return fn


def case_for(predictor):
    """仅指定预测器模式运行的用例（无标记用例两模式都跑）。"""

    def deco(fn):
        fn._predictor = predictor
        CASES.append(fn)
        return fn

    return deco


@case
def test_zero_warning_compile(ctx):
    """host gcc -Wall -Wextra 零警告编译（硬性检查，预测器由 ctx["predictor"] 选编）。二进制写系统临时目录，不污染工程树。"""
    infer_c = "ems_infer_tcn.c" if ctx["predictor"] == "tcnq" else "ems_infer_ref.c"
    srcs = [os.path.join(APP, f) for f in ("ems_ctrl.c", infer_c, "ems_params.c")]
    srcs.append(os.path.join(HERE, "test_driver.c"))
    build = tempfile.mkdtemp(prefix="ems01_l0_")
    ctx["build_dir"] = build
    for opt in ("-O2", "-O0"):
        exe = os.path.join(build, "test_driver_O2" if opt == "-O2" else "test_driver_O0")
        r = subprocess.run(
            ["gcc", "-std=c11", opt, "-Wall", "-Wextra", "-ffp-contract=off",
             "-I", APP, "-o", exe] + srcs + ["-lm"],
            capture_output=True, text=True)
        if r.returncode != 0 or r.stderr.strip():
            raise AssertionError(
                "gcc %s 非零警告/失败:\n%s" % (opt, r.stderr or r.stdout))
        if opt == "-O2":
            ctx["exe"] = exe
    # G0 端到端变体（-DEMS_GATE_MODE=0）：验证编译期门控缝合面
    exe0 = os.path.join(build, "test_driver_g0")
    r = subprocess.run(
        ["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-ffp-contract=off",
         "-DEMS_GATE_MODE=0", "-I", APP, "-o", exe0] + srcs + ["-lm"],
        capture_output=True, text=True)
    if r.returncode != 0 or r.stderr.strip():
        raise AssertionError("gcc G0 变体非零警告/失败:\n%s" % r.stderr)
    ctx["exe_g0"] = exe0
    # batch 参考变体（closure 后继任务，tcnq 模式）：-DEMS_TCN_BATCH_REF 使
    # ems_infer_tcn.c 的 ems_infer_mlpq 切换为 T-804 batch 前向（位级基准）；
    # 零警告同样硬性（批量路径与 closure 同文件同标准）。mlpq 模式不编译
    # （ems_infer_ref.c 无该宏，v1 链不变）。
    if ctx["predictor"] == "tcnq":
        exeb = os.path.join(build, "test_driver_batchref")
        r = subprocess.run(
            ["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-ffp-contract=off",
             "-DEMS_TCN_BATCH_REF", "-I", APP, "-o", exeb] + srcs + ["-lm"],
            capture_output=True, text=True)
        if r.returncode != 0 or r.stderr.strip():
            raise AssertionError("gcc batch-ref 变体非零警告/失败:\n%s" % r.stderr)
        ctx["exe_batch"] = exeb


@case
def test_flat_fill_init(ctx):
    """R1 平 fill 初始化：首调 buf 整窗填充当前 P_load，禁止零填充（EML:27-35）。"""
    d = Driver(ctx["exe"])
    try:
        d.cmd("reset")
        r = d.cmd("step 1.0 123.4 5.0 0.6 1")
        assert r["held"] == 0 and r["ok"] == 1
        st = d.cmd("state")
        assert st["k"] == 1, st
        for k in ("buf0", "buf1", "bufmid", "buf299"):
            assert st[k] == 123.4, (k, st[k])
        want_sum = 0.0
        for _ in range(WIN):
            want_sum += 123.4  # 与 C 侧顺序累加一致
        assert st["bufsum"] == want_sum
        assert st["lastT"] == 1.0 and st["vprev"] == 5.0
    finally:
        d.close()


@case
def test_tick_gate_hold(ctx):
    """R4 tick 门：t-lastT < dt-1e-9 早退返回保持值，状态完全不动（EML:46-49）。"""
    d = Driver(ctx["exe"])
    try:
        d.cmd("reset")
        r1 = d.cmd("step 100 500 5 0.6 1")
        assert r1["held"] == 0
        st1 = d.cmd("state")
        for dt_ in (0.5, 0.9, 1e-10):
            rh = d.cmd("step %.17g 999 9 0.1 1" % (100.0 + dt_))
            assert rh["held"] == 1, rh
            assert rh["P"] == r1["P"], (rh["P"], r1["P"])
            st = d.cmd("state")
            assert st["lastT"] == 100.0 and st["k"] == st1["k"]
            assert st["vprev"] == st1["vprev"] and st["bufsum"] == st1["bufsum"]
        r2 = d.cmd("step 101 500 5 0.6 1")
        assert r2["held"] == 0
    finally:
        d.close()


@case
def test_new_run_reset(ctx):
    """R2 t<lastT 全复位：平 fill+指针+速度基线+门控状态；复位拍返回 Pheld（EML:36-43）。"""
    d = Driver(ctx["exe"])
    try:
        d.cmd("reset")
        for i in range(3):
            d.cmd("step %d 500 5 0.6 1" % (100 + i))
        st_pre = d.cmd("state")
        assert st_pre["lastT"] == 102.0
        rh = d.cmd("step 50 77 5.5 0.6 1")  # 时间倒退 → 复位
        assert rh["held"] == 1 and rh["P"] == st_pre["Pheld"], rh
        st = d.cmd("state")
        assert st["lastT"] == 50.0 and st["k"] == 0
        assert st["vprev"] == 5.5
        assert st["cls_eff"] == -1 and st["run_cls"] == -1 and st["run_cnt"] == 0
        for k in ("buf0", "buf1", "bufmid", "buf299"):
            assert st[k] == 77.0, (k, st[k])
        r = d.cmd("step 51 77 5.5 0.6 1")
        assert r["held"] == 0
    finally:
        d.close()


@case
def test_nan_skip(ctx):
    """R3 NaN 跳写指针前进；vprev NaN 链与恢复；!ok 拍 ê=0 且决策照常（EML:51-57/166-169）。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"], predictor=ctx["predictor"])
    try:
        d.cmd("reset")
        d.cmd("step 1 100 5 0.6 1")
        st = d.cmd("state")
        assert st["k"] == 1 and st["buf1"] == 100.0
        r = d.cmd("step 2 nan 5 0.6 1")
        assert r["ok"] == 0 and r["held"] == 0 and r["e_hat"] == 0.0
        assert r["P"] == m.step(2.0, float("nan"), 5.0, 0.6, 1)["P"]
        st = d.cmd("state")
        assert st["k"] == 2, st          # 指针无条件前进（EML:54）
        assert st["buf1"] == 100.0       # NaN 拍不写窗（EML:51）
        # v NaN：vprev 更新为 NaN（EML:55-56 在 ok 之外），下一拍 accel=NaN → ok=0
        d.cmd("step 3 100 5 0.6 1")
        r = d.cmd("step 4 100 nan 0.6 1")
        assert r["ok"] == 0
        st = d.cmd("state")
        assert math.isnan(st["vprev"])
        r = d.cmd("step 5 100 5 0.6 1")
        assert r["ok"] == 0  # accel = 5 - NaN
        r = d.cmd("step 6 100 5 0.6 1")
        assert r["ok"] == 1  # accel = 0 → 恢复
        st = d.cmd("state")
        assert st["vprev"] == 5.0
    finally:
        d.close()


@case
def test_nn_ok_fallback(ctx):
    """nn_ok=0 → ê=0 回退（≡law=5）但 ECMS 决策照常（EML:57/166-169）。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"], predictor=ctx["predictor"])
    try:
        d.cmd("reset")
        r = d.cmd("step 1 400 6 0.6 0")
        assert r["held"] == 0 and r["ok"] == 0 and r["e_hat"] == 0.0
        want = m.step(1.0, 400.0, 6.0, 0.6, 0)
        assert r["P"] == want["P"]
        assert r["P"] != 0.0  # 决策照常（非保持）
    finally:
        d.close()


@case
def test_argmax_tie(ctx):
    """R5 argmax 平局取首（MATLAB max 语义，EML:134）。"""
    d = Driver(ctx["exe"])
    try:
        for vec, want in [([3, 5, 5, 2], 1), ([5, 5, 3], 0),
                          ([1, 2, 3], 2), ([0, 0, 0], 0), ([7], 0)]:
            r = d.cmd("argmax " + " ".join(repr(float(v)) for v in vec))
            assert r["ARGMAX"] == want, (vec, r)
    finally:
        d.close()


@case
def test_grid_argmin_tie(ctx):
    """R5 argmin 平局取首：J<Jbest 严格小于（EML:215），合成表使 Pg=20/40 精确同 J。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        cst = "1 1 1 1 0.0 0.5 10 20 1000 4 100 0.05 0.0"
        tbl = "10 20 30 40 1000 200 1000 100"
        r = d.cmd("grid " + cst + " " + tbl)
        want = m.grid_search({
            "eta_rect": 1.0, "eta_dcdc": 1.0, "eff_dis": 1.0, "eff_chg": 1.0,
            "soc_dmax": 0.0, "soc_cmin": 0.5, "idle": 10.0, "dp": 20.0,
            "pmax": 1000.0, "bp": [10.0, 20.0, 30.0, 40.0],
            "tbl": [1000.0, 200.0, 1000.0, 100.0], "nb": 4,
        }, 100.0, 0.05, 0.0)
        assert r["GRID"] == want == 20.0, (r, want)  # 平局取首：若 <= 则误取 40
    finally:
        d.close()


@case
def test_discharge_break(ctx):
    """R6 SOC≤Discharge_Max：首个可行 Pg break（非 J 最优）+ Pbat 恰为 0 边界（EML:183-189）。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        r = d.cmd("gridreal 50 0.105 0.425")
        assert r["GRID"] == 60.0, r  # Pbat(40)=11.2>1e-9 续；Pbat(60) 首个可行 → break
        # 对照：SOC 略高于限值时走 J 最优（不同结果 → 证明 break 语义未被统一）
        r_nobrk = d.cmd("gridreal 50 0.106 0.425")
        assert r_nobrk["GRID"] == m.grid_search(m.real_cst(), 50.0, 0.106, 0.425)
        assert r_nobrk["GRID"] != 60.0
        # Pbat 恰为 0：> 1e-9 为假 → 可行 → break
        pload = 0.97 * 40.0
        r = d.cmd("gridreal %.17g 0.105 0.425" % pload)
        assert r["GRID"] == 40.0, r
        # 镜像一致性（真实 501 点表）
        for args in ((50.0, 0.105, 0.425), (50.0, 0.106, 0.425),
                     (pload, 0.105, 0.425), (1500.0, 0.05, 0.425)):
            r = d.cmd("gridreal %.17g %.17g %.17g" % args)
            assert r["GRID"] == m.grid_search(m.real_cst(), *args), args
    finally:
        d.close()


@case
def test_rescue(ctx):
    """R7 末尾救急：低 SOC 且 P_load>η_rect·P_max → P_best=P_max（EML:220-222）。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        r = d.cmd("gridreal 1500 0.05 0.425")
        assert r["GRID"] == 1000.0, r  # 全部 Pg 不可行 → 救急（无救急则停留 0）
        # 高 SOC：无救急，正常 J 最优（镜像一致即可）
        r = d.cmd("gridreal 1500 0.5 0.425")
        assert r["GRID"] == m.grid_search(m.real_cst(), 1500.0, 0.5, 0.425), r
    finally:
        d.close()


@case
def test_charge_ban(ctx):
    """充电禁止分支：SOC≥Charge_Min 且 Pbat<0 → continue；Pbat==0 允许（EML:190-191）。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        # P_load=10：Pg≥20 时 Pbat<0 全部禁充 → 仅 Pg=0 可行
        r = d.cmd("gridreal 10 0.945 0.425")
        assert r["GRID"] == 0.0 == m.grid_search(m.real_cst(), 10.0, 0.945, 0.425), r
        # Pbat 恰为 0：Pbat<0 为假 → 允许参与竞争（J 更优则入选）
        pload = 0.97 * 20.0
        r = d.cmd("gridreal %.17g 0.945 0.425" % pload)
        want = m.grid_search(m.real_cst(), pload, 0.945, 0.425)
        assert r["GRID"] == want == 20.0, (r, want)  # 若误禁 Pbat==0 → 会得 0
        # Pbat 极小负：仍禁（<0）
        pload2 = 0.97 * 20.0 - 1e-12
        r = d.cmd("gridreal %.17g 0.945 0.425" % pload2)
        want = m.grid_search(m.real_cst(), pload2, 0.945, 0.425)
        assert r["GRID"] == want == 0.0, (r, want)
    finally:
        d.close()


@case
def test_bsfc_clamp_interp(ctx):
    """R9 BSFC 端点钳位 + 严格小于分段 + 节点插值舍入保真（EML:196-206）。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        bp = [10.0, 20.0, 30.0, 40.0]
        tbl = [100.0, 200.0, 300.0, 400.0]
        pg = 0.0
        while pg <= 50.0:
            r = d.cmd("bsfc 4 %.17g %s %s" % (
                pg, " ".join(map(repr, bp)), " ".join(map(repr, tbl))))
            want = m.bsfc_at(bp, tbl, 4, pg)
            assert r["BSFC"] == want, (pg, r["BSFC"], want)
            pg += 0.25
        # 钳位端点
        assert d.cmd("bsfc 4 5 %s %s" % ("10 20 30 40", "100 200 300 400"))["BSFC"] == 100.0
        assert d.cmd("bsfc 4 10 %s %s" % ("10 20 30 40", "100 200 300 400"))["BSFC"] == 100.0
        assert d.cmd("bsfc 4 40 %s %s" % ("10 20 30 40", "100 200 300 400"))["BSFC"] == 400.0
        assert d.cmd("bsfc 4 45 %s %s" % ("10 20 30 40", "100 200 300 400"))["BSFC"] == 400.0
        # nb=1 表
        assert d.cmd("bsfc 1 2 5 500")["BSFC"] == 500.0
        assert d.cmd("bsfc 1 8 5 500")["BSFC"] == 500.0
        # 严格小于 + 节点处 EML lerp 公式的舍入行为（与“节点直查”可区分）：
        # 内部节点 Pg=bp[1]：strict-less 停在 ii=0，w=1 → tbl[0]+(tbl[1]-tbl[0])，
        # 本组数值下该值与 tbl[1] 在 double 上可区分（若误用 <= 则得 0.2）。
        bp3 = [1.0, 2.0, 3.0]
        tbl3 = [1.1, 0.2, 0.3]
        r = d.cmd("bsfc 3 2 %s %s" % ("1 2 3", "1.1 0.2 0.3"))
        want = m.bsfc_at(bp3, tbl3, 3, 2.0)   # = 1.1+(0.2-1.1) = 0.19999999999999996
        assert r["BSFC"] == want and want != 0.2, (r, want)
        # 末节点走端点钆位分支（EML:197 先于插值）
        r = d.cmd("bsfc 3 3 %s %s" % ("1 2 3", "1.1 0.2 0.3"))
        assert r["BSFC"] == 0.3, r
    finally:
        d.close()


@case
def test_unit_chain(ctx):
    """R10 mf/mbe 单位链：3.6e6（mf）与 3600（mbe）混用保真（EML:193-214），误“统一”即翻车。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        # 用例 1：λ·mbe 主导 → 充电支路负 mbe 使 Pg=40 胜出
        r = d.cmd("grid 0.9 0.5 0.5 0.5 0.0 0.95 3.6 20 40 1 30 0.5 2.0 5 500")
        want = m.grid_search({
            "eta_rect": 0.9, "eta_dcdc": 0.5, "eff_dis": 0.5, "eff_chg": 0.5,
            "soc_dmax": 0.0, "soc_cmin": 0.95, "idle": 3.6, "dp": 20.0,
            "pmax": 40.0, "bp": [5.0], "tbl": [500.0], "nb": 1,
        }, 30.0, 0.5, 2.0)
        assert r["GRID"] == want == 40.0, (r, want)
        # 用例 2：mf 主导（idle=0、表 720 g/kWh）→ Pg=20 胜出；
        # mf 若误用 /3600 → mf=4.0 → 会误选 Pg=0（可检测）
        r = d.cmd("grid 1 1 1 1 0.0 0.95 0 20 20 1 20 0.5 1.0 0 720")
        want = m.grid_search({
            "eta_rect": 1.0, "eta_dcdc": 1.0, "eff_dis": 1.0, "eff_chg": 1.0,
            "soc_dmax": 0.0, "soc_cmin": 0.95, "idle": 0.0, "dp": 20.0,
            "pmax": 20.0, "bp": [0.0], "tbl": [720.0], "nb": 1,
        }, 20.0, 0.5, 1.0)
        assert r["GRID"] == want == 20.0, (r, want)
    finally:
        d.close()


@case
def test_hysteresis(ctx):
    """R8 迟滞状态机：首判采纳/同类复位/连续 3 tick 切换/新候选计数归 1（EML:136-148）。"""
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        d.cmd("hystreset")
        g = {"cls_eff": -1, "run_cls": -1, "run_cnt": 0}
        seq = [3, 3, 5, 3, 5, 5, 5, 6, 7, 7, 6, 6, 6]
        want = [(3, 3, 0), (3, 3, 0), (3, 5, 1), (3, 3, 0), (3, 5, 1), (3, 5, 2),
                (5, 5, 0), (5, 6, 1), (5, 7, 1), (5, 7, 2), (5, 6, 1), (5, 6, 2),
                (6, 6, 0)]
        for cls, exp in zip(seq, want):
            r = d.cmd("hyst %d" % cls)
            Mirror.hysteresis(g, cls)
            got = (r["cls_eff"], r["run_cls"], r["run_cnt"])
            assert got == exp == (g["cls_eff"], g["run_cls"], g["run_cnt"]), \
                (cls, got, exp)
        # 边界：run_cnt==2 不切换、==3 切换（>= 语义）已在序列 5,5,5 覆盖
        # K_w 映射（EML:149-153）
        assert d.cmd("kw 6")["KW"] == 0
        for c in (1, 2, 3, 4, 5, 7, 0, 99, -1):
            assert d.cmd("kw %d" % c)["KW"] == 1
    finally:
        d.close()


@case
def test_gate_kq(ctx):
    """G1 分位数门控边界：s_q≤S1→1、s_q≥S2→0、区间线性（EML:72-79）。"""
    d = Driver(ctx["exe"])
    try:
        assert d.cmd("kq 20")["KQ"] == 1.0
        assert d.cmd("kq %.17g" % GATE_S1)["KQ"] == 1.0        # ≤ 边界含等号
        assert d.cmd("kq %.17g" % (GATE_S1 - 1e-9))["KQ"] == 1.0
        assert d.cmd("kq %.17g" % GATE_S2)["KQ"] == 0.0        # ≥ 边界含等号
        assert d.cmd("kq %.17g" % (GATE_S2 + 1e-9))["KQ"] == 0.0
        mid = (GATE_S1 + GATE_S2) / 2
        r = d.cmd("kq %.17g" % mid)["KQ"]
        want = (GATE_S2 - mid) / (GATE_S2 - GATE_S1)
        assert r == want, (r, want)
        assert abs(r - 0.5) < 1e-12
    finally:
        d.close()


@case
def test_gate_modes(ctx):
    """G0~G3 门控 K 计算（EML:155-164）：G0=1 / G1=K_q / G2=K_w / G3=K_q·K_w。"""
    d = Driver(ctx["exe"])
    try:
        for mode in (0, 1, 2, 3):
            for kq in (0.0, 0.37, 1.0):
                for kw in (0, 1):
                    r = d.cmd("kcomb %d %.17g %d" % (mode, kq, kw))
                    want = {0: 1.0, 1: kq, 2: float(kw), 3: kq * float(kw)}[mode]
                    assert r["KCOMB"] == want, (mode, kq, kw, r)
    finally:
        d.close()


@case_for("mlpq")
def test_mlpq_mirror(ctx):
    """R11a MLP-Q 前向逐位镜像（Dense 302-64-32-3，输出层无 ReLU，EML:65-67）。"""
    import numpy as np
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        rng = np.random.default_rng(20260831)
        for _ in range(20):
            x = rng.normal(size=FEAT)
            r = d.cmd("mlpq " + " ".join(repr(float(v)) for v in x))
            want = m.mlpq([float(v) for v in x])
            assert r["ret"] == 0
            assert (r["q10"], r["q50"], r["q90"]) == tuple(want), \
                (r, want)
    finally:
        d.close()


@case
def test_workclf_mirror(ctx):
    """R11b workclf 前向逐位镜像（因果 conv/pool 丢尾/GAP/旁路融合，EML:82-134）。

    2026-09 口径修订：固件喂入已改为归一化切片（xnorm[10*j] + xnorm[300]/[301]，
    与 T-312 训练口径一致），直喂用例输入取归一化域典型范围 uniform(-3,3)；
    修订前用例为 raw 量纲（pd 0..900 / v 0..9 / a -0.5..0.5）。
    """
    import numpy as np
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"])
    try:
        rng = np.random.default_rng(20260901)
        for _ in range(20):
            pd = rng.uniform(-3.0, 3.0, 30)
            v = rng.uniform(-3.0, 3.0)
            a = rng.uniform(-3.0, 3.0)
            r = d.cmd("workclf %s %.17g %.17g" % (
                " ".join(repr(float(x)) for x in pd), v, a))
            want = m.workclf([float(x) for x in pd], float(v), float(a))
            assert r["ret"] == 0
            assert r["cls"] == want, (r["cls"], want)
    finally:
        d.close()


def _trace_inputs():
    """test_trace_mirror / test_tcnq_trace_mirror 共用的合成输入轨迹：t 单调 +1，
    注入 hold/倒退/NaN/nn_ok=0/低高 SOC 段（R2/R3/R4/R6/R7 覆盖）。"""
    import numpy as np
    rng = np.random.default_rng(20260829)
    inputs = []
    for i in range(60):
        t = 1.0 + i
        if i in (48, 49, 50):
            soc = 0.05 + 0.02 * (i - 48)       # 低 SOC 段 → 禁放电 break
        elif i in (55, 56, 57, 58):
            soc = 0.95 + 0.004 * (i - 55)      # 高 SOC 段 → 禁充电
        elif i == 52:
            soc = 0.104                          # 恰在限值上（含等号分支）
        else:
            soc = 0.6 + 0.3 * math.sin(i / 7.0)
        p = 380.0 + 250.0 * math.sin(i / 5.0) + float(rng.uniform(-60, 60))
        v = 5.0 + 1.5 * math.sin(i / 9.0)
        nn_ok = 0 if i == 40 else 1
        inputs.append((t, p, v, soc, nn_ok))
    inputs[20] = (inputs[20][0] + 0.5,) + inputs[20][1:]   # tick 门 → 保持
    inputs[30] = (inputs[30][0], float("nan")) + inputs[30][2:]  # NaN 跳写
    inputs[33] = (3.0,) + inputs[33][1:]                    # t<lastT → 全复位
    return inputs


@case_for("mlpq")
def test_trace_mirror(ctx):
    """全拍 step 端到端逐位镜像（G2 默认模式）：滑窗/门控/λ/网格全链路（EML:26-224）。"""
    import numpy as np
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"], gate_mode=2)
    try:
        d.cmd("reset")
        inputs = _trace_inputs()
        p_prev = 0.0
        for i, (t, p, v, soc, ok_flag) in enumerate(inputs):
            r = d.cmd("step %.17g %.17g %.17g %.17g %d" % (t, p, v, soc, ok_flag))
            want = m.step(t, p, v, soc, ok_flag)
            if want.get("held") == 1:
                assert r["held"] == 1 and r["ok"] == 1 and r["P"] == want["P"], (i, r, want)
            else:
                for k, wv in want.items():
                    assert eq(r[k], wv), (i, k, r[k], wv)
                p_prev = r["P"]
        st = d.cmd("state")
        assert st["lastT"] == m.lastT and st["k"] == m.k
        assert st["vprev"] == m.vprev and st["Pheld"] == m.Pheld
        assert (st["cls_eff"], st["run_cls"], st["run_cnt"]) == \
            (m.g["cls_eff"], m.g["run_cls"], m.g["run_cnt"])
        assert p_prev != 0.0
    finally:
        d.close()


@case_for("tcnq")
def test_tcnq_mirror(ctx):
    """R11a' TCN 前向镜像（tcnq 模式，T-804/T-805 单元段）：C ems_infer_mlpq（=TCN）
    vs Mirror.tcnq（numpy float64 同源权重），kWh 域容差 1e-8
    （双精度求和序噪声底 ~1e-10；结构错误为 kWh 量级可区分）。"""
    import numpy as np
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"], predictor="tcnq")
    try:
        rng = np.random.default_rng(20260831)
        worst = 0.0
        for _ in range(20):
            x = rng.normal(size=FEAT)
            r = d.cmd("mlpq " + " ".join(repr(float(v)) for v in x))
            want = m.tcnq([float(v) for v in x])
            assert r["ret"] == 0
            dq = max(abs(r["q10"] - want[0]), abs(r["q50"] - want[1]),
                     abs(r["q90"] - want[2])) * m.ys
            worst = max(worst, dq)
            assert dq <= TCN_Q_TOL_KWH, (r, want, dq)
    finally:
        d.close()
    print("\n      worst |dq| = %.2e kWh（容差 %.0e）" % (worst, TCN_Q_TOL_KWH))


@case_for("tcnq")
def test_tcnq_trace_mirror(ctx):
    """tcnq 模式全拍 step 端到端（T-805 单元段）：C（TCN 槽位）vs Mirror.tcnq
    决策链；q 族字段容差 TCN_Q_TOL_KWH，P_gen 网格点必须完全一致（决策链装配检查）。"""
    import numpy as np
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"], gate_mode=2, predictor="tcnq")
    try:
        d.cmd("reset")
        p_prev = 0.0
        for i, (t, p, v, soc, ok_flag) in enumerate(_trace_inputs()):
            r = d.cmd("step %.17g %.17g %.17g %.17g %d" % (t, p, v, soc, ok_flag))
            want = m.step(t, p, v, soc, ok_flag)
            if want.get("held") == 1:
                assert r["held"] == 1 and r["ok"] == 1 and r["P"] == want["P"], (i, r, want)
                continue
            assert r["held"] == 0 and r["ok"] == want["ok"], (i, r, want)
            for k in ("cls", "cls_inv", "K_w", "cls_eff"):
                assert r[k] == want[k], (i, k, r[k], want[k])
            assert eq(r["K"], want["K"]), (i, r["K"], want["K"])
            for k in ("q10", "q50", "q90", "sq", "K_q", "e_hat", "e_hat_raw"):
                assert abs(r[k] - want[k]) <= TCN_Q_TOL_KWH, (i, k, r[k], want[k])
            assert r["P"] == want["P"], (i, r["P"], want["P"])
            p_prev = r["P"]
        st = d.cmd("state")
        assert st["lastT"] == m.lastT and st["k"] == m.k
        assert st["vprev"] == m.vprev and st["Pheld"] == m.Pheld
        assert (st["cls_eff"], st["run_cls"], st["run_cnt"]) == \
            (m.g["cls_eff"], m.g["run_cls"], m.g["run_cnt"])
        assert p_prev != 0.0
    finally:
        d.close()


@case
def test_gate_mode0_end_to_end(ctx):
    """G0 编译变体端到端：K≡1、e_hat==e_hat_raw（编译期门控缝合面生效）。"""
    d = Driver(ctx["exe_g0"])
    m = Mirror(ctx["data"], gate_mode=0, predictor=ctx["predictor"])
    tol_q = ("q10", "q50", "q90", "sq", "e_hat", "e_hat_raw", "K_q") \
        if ctx["predictor"] == "tcnq" else ()
    try:
        d.cmd("reset")
        for i in range(6):
            r = d.cmd("step %.17g %.17g %.17g %.17g 1" % (
                1.0 + i, 400.0 + 30.0 * i, 5.0, 0.6 - 0.05 * i))
            want = m.step(1.0 + i, 400.0 + 30.0 * i, 5.0, 0.6 - 0.05 * i, 1)
            for k, wv in want.items():
                if k in tol_q:
                    assert abs(r[k] - wv) <= TCN_Q_TOL_KWH, (i, k, r[k], wv)
                else:
                    assert eq(r[k], wv), (i, k, r[k], wv)
    finally:
        d.close()


@case_for("tcnq")
def test_tcnq_l0_diff(ctx):
    """T-805 L0 host 差分（T2 层，design §Testing T2 / FR-708b）：gcc REF 链
    （ems_infer_tcn.c）vs keras float32 参考（export_tcnq_reference.py，比对统一
    kWh 域）。向量 = val200（seed=20260831，Host/reference/vectors_raw.csv）+ 32
    随机窗（seed=20260902）+ S20/voyage002 全轨迹（v1 M2 INJECT 轨迹）；门禁
    |Δq50| ≤ 1 LSB_TCN = 0.266214 kWh 逐 tick + P_gen 网格一致率 ≥99% + 链数值
    健康（合法网格点、无 FALLBACK 误触发）；产物 l0_tcn_diff.json。"""
    import numpy as np
    ws = os.path.join(REPO, "model", "simulink_modelD_(EMS_NN_AECMS)")
    ref_dir = os.path.join(P, "Host", "results", "tcn_swap")
    os.makedirs(ref_dir, exist_ok=True)
    ref_script = os.path.join(ws, "python", "export_tcnq_reference.py")
    fw_ref = os.path.join(ws, "results", "ecms_phase25", "fw_reference")
    py = VENV_PY if os.path.isfile(VENV_PY) else sys.executable
    build = ctx.get("build_dir") or tempfile.mkdtemp(prefix="ems01_l0_tcn_")
    d = Driver(ctx["exe"])
    m = Mirror(ctx["data"], gate_mode=2, predictor="tcnq")
    ym, ys = m.ym, m.ys
    results = {}

    def rel(p):
        return os.path.relpath(p, REPO)

    def sha16(p):
        return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16]

    def run_reference(mode_args, out_csv):
        r = subprocess.run([py, ref_script] + mode_args + ["--out", out_csv],
                           capture_output=True, text=True)
        assert r.returncode == 0, \
            "参考链重生成失败：%s\n%s" % (mode_args, r.stderr[-2000:])

    def compare_vectors(name, xn, ref_csv):
        """向量级比对（槽位直喂，同 FR-504 INFER_TEST 语义）：C q3_norm ×y_std+ym
        vs keras 参考 kWh，逐向量门禁。"""
        ref = np.loadtxt(ref_csv, delimiter=",", skiprows=1)
        assert ref.shape[0] == xn.shape[0], (name, ref.shape, xn.shape)
        dq = np.zeros((xn.shape[0], 3))
        for r_i in range(xn.shape[0]):
            rr = d.cmd("mlpq " + " ".join(repr(float(v)) for v in xn[r_i]))
            assert rr["ret"] == 0, (name, r_i, rr)
            q_kwh = np.array([rr["q10"], rr["q50"], rr["q90"]]) * ys + ym
            dq[r_i] = np.abs(q_kwh - ref[r_i, 1:4])
        a50 = dq[:, 1]
        results[name] = {
            "n": int(xn.shape[0]),
            "max_abs_dq10_kwh": float(dq[:, 0].max()),
            "max_abs_dq50_kwh": float(a50.max()),
            "max_abs_dq90_kwh": float(dq[:, 2].max()),
            "mean_abs_dq50_kwh": float(a50.mean()),
            "p99_abs_dq50_kwh": float(np.percentile(a50, 99)),
            "worst_row": int(a50.argmax()),
            "gate_pass": bool(a50.max() <= LSB_TCN_KWH),
        }

    # ---- 参考链重生成（keras float32 单一来源；产物覆写至 whitelisted results） ----
    ref_val200 = os.path.join(ref_dir, "ref_keras_val200.csv")
    raw_csv = os.path.join(P, "Host", "reference", "vectors_raw.csv")
    run_reference(["--vectors-raw", raw_csv], ref_val200)
    raw = np.loadtxt(raw_csv, delimiter=",")
    xm64 = np.asarray(m.xm, dtype=np.float64)
    xs64 = np.asarray(m.xs, dtype=np.float64)
    compare_vectors("val200", (raw - xm64[None, :]) / xs64[None, :], ref_val200)

    rng = np.random.default_rng(SEED_RANDOM32)
    xn32 = rng.normal(size=(N_RANDOM32, FEAT))
    tmp_raw = os.path.join(build, "l0_rand32_raw.csv")
    tmp_ref = os.path.join(build, "l0_rand32_ref.csv")
    np.savetxt(tmp_raw, xn32 * xs64[None, :] + xm64[None, :],
               fmt="%.17g", delimiter=",")
    run_reference(["--vectors-raw", tmp_raw], tmp_ref)
    compare_vectors("random32", xn32, tmp_ref)

    traces = {
        "S20": (os.path.join(fw_ref, "ref_G2_S20_composite_voyage_matrix.csv"),
                os.path.join(ref_dir, "ref_keras_S20.csv")),
        "voyage002": (os.path.join(fw_ref, "ref_G2_test_voyage_002_v2.csv"),
                      os.path.join(ref_dir, "ref_keras_voyage002.csv")),
    }
    for name, (trace_csv, ref_csv) in traces.items():
        run_reference(["--trace", trace_csv], ref_csv)
        ref_rows = np.loadtxt(ref_csv, delimiter=",", skiprows=1)  # tick,q10,q50,q90
        ref_by_tick = {int(r[0]): r[1:4] for r in ref_rows}
        dq10s, dq50s, dq90s = [], [], []
        n_grid = n_match = fallback = illegal = 0
        worst_dq50 = 0.0
        worst_tick = -1
        m2 = Mirror(ctx["data"], gate_mode=2, predictor="tcnq")
        d.cmd("reset")
        with open(trace_csv, "r", newline="") as f:
            for row in csv.DictReader(f):
                t = float(row["t"])
                pld = float(row["P_load_kW"])
                v = float(row["v_ship_mps"])
                soc = float(row["SOC"])
                rr = d.cmd("step %.17g %.17g %.17g %.17g 1" % (t, pld, v, soc))
                mw = m2.step(t, pld, v, soc, 1)
                if mw["held"]:
                    assert rr["held"] == 1, (name, row["tick"])
                    continue
                tick = int(row["tick"])
                q3_ref = ref_by_tick[tick]
                n_grid += 1
                if rr["P"] == mw["P"]:
                    n_match += 1
                # 链健康：输入全有限 + nn_ok=1 → ok 必须 = 1（无 FALLBACK 误触发）
                if rr["ok"] == 0 or mw["ok"] == 0:
                    fallback += 1
                # P_gen 合法网格点：Pg = i·dp ∈ [0, pmax]
                k_g = rr["P"] / m.dp
                if (abs(rr["P"] - round(k_g) * m.dp) > 1e-6
                        or rr["P"] < -1e-9 or rr["P"] > m.pmax + 1e-6):
                    illegal += 1
                assert not np.isnan(q3_ref).any(), \
                    "tick %d：C ok=1 但参考 NaN（参考链 tick 错位）" % tick
                q_c = np.array([rr["q10"], rr["q50"], rr["q90"]])  # step 输出已 kWh
                dqs = np.abs(q_c - q3_ref)
                dq10s.append(dqs[0])
                dq50s.append(dqs[1])
                dq90s.append(dqs[2])
                if dqs[1] > worst_dq50:
                    worst_dq50 = float(dqs[1])
                    worst_tick = tick
        a50 = np.asarray(dq50s)
        cons = n_match / float(n_grid)
        results[name] = {
            "trace": rel(trace_csv),
            "n_ticks": int(n_grid),
            "max_abs_dq10_kwh": float(max(dq10s)),
            "max_abs_dq50_kwh": float(a50.max()),
            "max_abs_dq90_kwh": float(max(dq90s)),
            "mean_abs_dq50_kwh": float(a50.mean()),
            "p99_abs_dq50_kwh": float(np.percentile(a50, 99)),
            "worst_tick": int(worst_tick),
            "grid_consistency": cons,
            "n_grid_cmp": int(n_grid),
            "n_grid_mismatch": int(n_grid - n_match),
            "fallback_ticks": int(fallback),
            "illegal_pgen": int(illegal),
            "gate_pass": bool(a50.max() <= LSB_TCN_KWH and cons >= GRID_CONSIST_MIN
                              and fallback == 0 and illegal == 0),
        }
    d.close()

    overall = all(res["gate_pass"] for res in results.values())
    ctx["l0_diff_results"] = results  # 供 test_tcnq_stream_diff 汇总 vs-keras 分布
    report = {
        "task": "tcn64-swap-l0-diff",
        "spec": "ems01-chil-tcn64-swap",
        "predictor": "tcnq",
        "host_chain": "gcc -O2 -Wall -Wextra -ffp-contract=off "
                      "ems_ctrl.c + ems_infer_tcn.c + ems_params.c + test_driver.c",
        "reference_chain": "export_tcnq_reference.py（keras float32 tcn_f64.keras，"
                           "fleet_norm_v2 归一化，kWh 域输出）",
        "gate_abs_dq50_kwh": LSB_TCN_KWH,
        "gate_grid_consistency_min": GRID_CONSIST_MIN,
        "inputs": {
            "val200_vectors": rel(raw_csv),
            "random32": {"seed": SEED_RANDOM32, "n": N_RANDOM32,
                         "domain": "normalized N(0,1)，反归一化后经参考链同口径比对"},
            "traces": {name: res.get("trace") for name, res in results.items()
                       if "trace" in res},
            "sha16": {
                rel(ref_val200): sha16(ref_val200),
                rel(traces["S20"][1]): sha16(traces["S20"][1]),
                rel(traces["voyage002"][1]): sha16(traces["voyage002"][1]),
                rel(raw_csv): sha16(raw_csv),
            },
        },
        "datasets": results,
        "overall": overall,
    }
    out_json = os.path.join(ref_dir, "l0_tcn_diff.json")
    with open(out_json, "w", encoding="utf-8", newline="\n") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
        f.write("\n")
    failed = [k for k, v in results.items() if not v["gate_pass"]]
    assert overall, "L0 差分门禁 FAIL：%s" % failed
    print("\n      l0_tcn_diff.json overall=true（%s）"
          % ", ".join("%s |dq50|max=%.2e kWh" % (k, v["max_abs_dq50_kwh"])
                      for k, v in results.items()))


@case_for("tcnq")
def test_tcnq_stream_diff(ctx):
    """closure 精确增量 vs batch 前向位级断言 + vs-keras LSB 分布汇总（spec
    ems01-chil-tcn64-swap 后继，主 Agent 裁决 closure 方案；T-804 batch 为位级
    基准）。1) 向量级：val200 + random32 同喂主链（closure）与 batch 参考变体
    （-DEMS_TCN_BATCH_REF），mlpq 响应行逐字节相等（%.17g 往返无损 = IEEE754
    位级）；2) 轨迹级：S20/voyage002 全轨迹 step 逐 tick 全字段位级相等
    （q 族/K/P_gen——决策链同源，位等预测器输出 ⇒ 位等决策）；3) max|Δ| = 0
    硬断言（非容差）；4) 产物 l0_tcn_stream_diff.json = bit-exact 断言 +
    vs-keras LSB 分布（自 test_tcnq_l0_diff 同会话结果，CASES 顺序保证）。"""
    import numpy as np
    assert "exe_batch" in ctx, "batch 参考变体未编译（test_zero_warning_compile）"
    ws = os.path.join(REPO, "model", "simulink_modelD_(EMS_NN_AECMS)")
    fw_ref = os.path.join(ws, "results", "ecms_phase25", "fw_reference")
    ref_dir = os.path.join(P, "Host", "results", "tcn_swap")
    m = Mirror(ctx["data"], predictor="tcnq")
    d = Driver(ctx["exe"])
    db = Driver(ctx["exe_batch"])
    results = {}

    def rel(p):
        return os.path.relpath(p, REPO)

    def bitexact_pair(name, lines_c, lines_b):
        """响应行逐字节比较（%.17g 往返无损 = 位级等价），返回断言结果。"""
        n = len(lines_c)
        bad = [i for i in range(n) if lines_c[i] != lines_b[i]]
        results[name] = {
            "n": int(n),
            "n_bitexact": int(n - len(bad)),
            "n_mismatch": int(len(bad)),
            "max_abs_dq50_kwh": 0.0 if not bad else None,
            "first_mismatch": (int(bad[0]) if bad else -1),
            "bitexact": bool(not bad),
        }
        return not bad

    ok_all = True

    # ---- 1) 向量级：val200 + random32（同 l0_diff 口径：归一化域直喂 mlpq；
    # random32 与 l0_diff 同种子同消耗序重放） ----
    raw_csv = os.path.join(P, "Host", "reference", "vectors_raw.csv")
    raw = np.loadtxt(raw_csv, delimiter=",")
    xm64 = np.asarray(m.xm, dtype=np.float64)
    xs64 = np.asarray(m.xs, dtype=np.float64)
    rng32 = np.random.default_rng(SEED_RANDOM32)
    vec_sets = {
        "val200": (raw - xm64[None, :]) / xs64[None, :],
        "random32": rng32.normal(size=(N_RANDOM32, FEAT)),
    }
    for name, vec in vec_sets.items():
        lc, lb = [], []
        for r_i in range(vec.shape[0]):
            cmd = "mlpq " + " ".join(repr(float(v)) for v in vec[r_i])
            lc.append(d.cmd(cmd))
            lb.append(db.cmd(cmd))
        ok_all &= bitexact_pair(name, lc, lb)

    # ---- 2) 轨迹级：全轨迹 step 逐 tick 全字段位级（两驱动同序独立推进） ----
    traces = {
        "S20": os.path.join(fw_ref, "ref_G2_S20_composite_voyage_matrix.csv"),
        "voyage002": os.path.join(fw_ref, "ref_G2_test_voyage_002_v2.csv"),
    }
    for name, trace_csv in traces.items():
        d.cmd("reset")
        db.cmd("reset")
        lc, lb = [], []
        with open(trace_csv, "r", newline="") as f:
            for row in csv.DictReader(f):
                cmd = "step %.17g %.17g %.17g %.17g 1" % (
                    float(row["t"]), float(row["P_load_kW"]),
                    float(row["v_ship_mps"]), float(row["SOC"]))
                lc.append(d.cmd(cmd))
                lb.append(db.cmd(cmd))
        ok_all &= bitexact_pair(name, lc, lb)
    d.close()
    db.close()

    # ---- 3) vs-keras LSB 分布（自 test_tcnq_l0_diff 同会话结果） ----
    l0 = ctx.get("l0_diff_results")
    assert l0 is not None, "vs-keras 段依赖 test_tcnq_l0_diff 先行（CASES 顺序）"
    ok_all &= all(v["gate_pass"] for v in l0.values())

    report = {
        "task": "tcn64-swap-l0-stream-diff",
        "spec": "ems01-chil-tcn64-swap",
        "predictor": "tcnq",
        "backend": "ems_infer_tcn.c closure 精确增量（依赖闭包，无跨拍状态；"
                   "plain 流式 fast/cold 经 L0 数值实验否决后主 Agent 裁决方案）",
        "batch_reference": "同文件 -DEMS_TCN_BATCH_REF 测试专用 T-804 batch 前向"
                           "（位级基准；固件构建编译剔除）",
        "bitexact_method": "驱动 %.17g 响应行逐字节相等（round-trip 无损 = "
                           "IEEE754 double 位级等价；非容差）",
        "bitexact": results,
        "gate_bitexact": bool(ok_all),
        "vs_keras": {
            "source": "test_tcnq_l0_diff（同会话，l0_tcn_diff.json 同源结果）",
            "gate_abs_dq50_kwh": LSB_TCN_KWH,
            "datasets": l0,
            "overall": bool(all(v["gate_pass"] for v in l0.values())),
        },
        "overall": bool(ok_all),
    }
    out_json = os.path.join(ref_dir, "l0_tcn_stream_diff.json")
    with open(out_json, "w", encoding="utf-8", newline="\n") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
        f.write("\n")
    assert ok_all, "closure-vs-batch 位级断言 FAIL：%s" % \
        [k for k, v in results.items() if not v["bitexact"]]
    print("\n      bit-exact closure vs batch：%s（全部 max|Δ|=0）"
          % ", ".join("%s n=%d" % (k, v["n"]) for k, v in results.items()))


@case
def test_params_idempotent(ctx):
    """gen_fw_params.py 幂等：重跑逐字节一致；头部 sha256 与 6 源实算一致（含
    selected_model_weights_tcnq.json，T-803）；--check exit 0。"""
    py = VENV_PY if os.path.isfile(VENV_PY) else sys.executable
    h_path = os.path.join(APP, "ems_params.h")
    c_path = os.path.join(APP, "ems_params.c")
    before = {p: open(p, "rb").read() for p in (h_path, c_path)}
    r1 = subprocess.run([py, GEN_PY], capture_output=True, text=True)
    assert r1.returncode == 0, r1.stderr
    after = {p: open(p, "rb").read() for p in (h_path, c_path)}
    for p in before:
        assert before[p] == after[p], "%s 重跑不一致（非幂等）" % p
    # 头部 sha256 溯源与源文件实算一致
    header = after[h_path].decode("utf-8")
    found = dict(re.findall(r"^\s{4}(\S+)\s+sha256=([0-9a-f]{64})$", header, re.M))
    assert len(found) == 6, found  # 5 源（T-503）+ selected_model_weights_tcnq.json（T-803）
    for name, path in gen_fw_params.SOURCES:
        h = hashlib.sha256(open(path, "rb").read()).hexdigest()
        assert found.get(name) == h, (name, found.get(name), h)
    # --check 模式（源→现文件一致性）
    r2 = subprocess.run([py, GEN_PY, "--check"], capture_output=True, text=True)
    assert r2.returncode == 0, r2.stdout + r2.stderr


@case
def test_no_absolute_paths(ctx):
    """AGENTS.md §20.2：本任务新增文件绝对路径扫描无命中（含 WS 侧脚本与 JSON）。"""
    pat = re.compile(r"[A-Za-z]:[\\/]|/(Users|home)/")
    ws_root = os.path.join(REPO, "model", "simulink_modelD_(EMS_NN_AECMS)")
    hits = []
    for base, rels in ((P, NEW_FILES), (ws_root, NEW_FILES_WS)):
        for rel in rels:
            path = os.path.join(base, rel)
            if not os.path.isfile(path):
                continue  # 生成产物（如 l0_tcn_diff.json）在对应模式下才存在
            with open(path, "r", encoding="utf-8") as f:
                for ln, line in enumerate(f, 1):
                    if pat.search(line):
                        hits.append("%s:%d" % (rel, ln))
    assert not hits, hits


# ---------------------------------------------------------------- 主流程


def main():
    ap = argparse.ArgumentParser(
        description="EMS01 CHIL ECMS L0 主机单元用例 + 预测器 L0 差分（T-503/T-805）")
    ap.add_argument("--predictor", choices=("mlpq", "tcnq"), default="mlpq",
                    help="预测器槽位（默认 mlpq = v1 既有链不变；tcnq = T-805 L0 差分）")
    args = ap.parse_args()
    ctx = {"data": gen_fw_params.load_all(), "predictor": args.predictor}
    cases = [fn for fn in CASES
             if getattr(fn, "_predictor", None) in (None, args.predictor)]
    failed = []
    print("=" * 72)
    print("predictor=%s  cases=%d" % (args.predictor, len(cases)))
    try:
        for fn in cases:
            try:
                fn(ctx)
                print("PASS  %s — %s" % (fn.__name__, fn.__doc__.strip().splitlines()[0]))
            except Exception as exc:  # noqa: BLE001 — 用例失败即记录，继续跑其余用例
                failed.append(fn.__name__)
                print("FAIL  %s\n      %s: %s" % (fn.__name__, type(exc).__name__, exc))
    finally:
        build = ctx.get("build_dir")
        if build and os.path.isdir(build):
            shutil.rmtree(build, ignore_errors=True)
    print("=" * 72)
    print("total=%d pass=%d fail=%d" % (len(cases), len(cases) - len(failed), len(failed)))
    if failed:
        print("failed: %s" % ", ".join(failed))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
