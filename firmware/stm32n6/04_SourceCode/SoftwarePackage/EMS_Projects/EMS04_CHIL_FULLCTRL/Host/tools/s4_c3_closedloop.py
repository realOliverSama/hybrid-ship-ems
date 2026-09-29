#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4_c3_closedloop.py — S4-C3：plantE↔board 实时闭环单航次 + 三链对照（板在环）。

架构（S4 spec §3 C3 + S4_G6_ACCEPTANCE §0-§5；前置证据 s4_c3_preamble_report.json）：
  plantE（Host/plantstep_e/S20_composite_voyage dylib，10 s 拍 = 1000×0.01 s 子步）
    → 每控制拍把状态发板（TICK_CTRL 0x0E：seq+tick+8×f32 + hist 300×11 f32 + kf
    30×6 f32）→ 板跑完整控制器（REATT int8 NPU + law6 + PMS/Safety×1000）回
    P_gen_ref（RSP 0x0F）→ P_gen_ref 反馈植物 → 闭环。
  三链同录（同植物、同拍序、同 kf 表行）：
    A2_CANONICAL_HOST: canonical 域输入（ppe.h1_assembly.h1_from_raw_logs 冻结管线
        聚合活通道子步真样本 + 场景侧 stream 时间表列；kf_raw = 表行 f32 值）→
        ppe.c2_inference.C2Predictor（FR836 float keras，q50 kW 域）→ law6 host
        dylib → 执行层 → PMS host 实例。
    B_HOST: 与下行完全相同的 hist/kf f32 字节 → gen_ref_split_v2.Chain（板源 OE
        int8 N1+N2 ORT 代理，S3-C 已证）→ law6 → 执行层 → PMS host 实例。
    B_BOARD: 板 RSP（NEURAL-ART int8 REATT + law6 + PMS/Safety 板上）。

拍序与域（本文件冻结的设计决策，全文见 s4_c3_design_notes.md）：
  - t=0 先向 H1 环推 IC 样本（tick_first=0，场景初值口径）。
  - 预热段（is_formal=0，30 控制拍，t∈(0,300]）：板不参与（H1 环 301 样本未满、
    kf issue 网格 300 起始，板侧 fail-closed 无法运行）。植物由 law6 冷启动控制
    驱动（d1=0 → soc_ref_eff=0.60，λ=max(0.425+30×(0.60−SOC),0.43)，执行层网格）。
  - 正式段（is_formal=1，541 拍，控制拍 kk=30..570，查询 t0=kk×10=300..5700，
    与 kf issue 网格逐拍重合）：每拍先跑本拍第 1 个子步（pgen=上一拍板值）——
    该子步 OUT@stamp=t0 恰好闭合 A2 窗 (t0-300,t0] 与环边界（整秒推环），随后
    TICK_CTRL 下行 → 板 RSP=B_BOARD → B_HOST/A2 两链 host 计算（消费与下行相同
    的 f32 回读值）→ 逐拍 CSV → 本拍剩余 999 子步用新板值（ZOH 前沿 0.01 s，
    全程一致，登记）。首拍下行前 PING + CMD_RESET，host 两 PMS 实例
    ems04_pms_init 与板复位同步（前置同法）。

采样网格（登记，design notes §列源表）：
  - 活通道（P_load/v_ship/accel）= 子步起点 OUT 读（自有 0.01 网格时钟 stamp；
    OUT 为 ERT pre-integration 写出=子步起点值，t_s 信号自带 +0.0075 场景时移
    故不作 stamp）；omega/SOC/Vdc/x_ship = states() X 直读（子步末真值；
    n_prop = omega×30/π 平移对齐 stamp，0.01 s，登记）。
  - 场景侧 4 列（v_ref/P_hotel/v_current/P_prop_demand）= S20 pilot stream 100 Hz
    时间表（场景侧信号不依赖闭环状态；只按 t≤t0 因果取样本；不触任何真值列）。
  - B 环边界推样 = stamp 命中整秒 e 时（OUT@e + 上一子步 X 真值@e；v[0]=名义
    整秒）；A2 聚合 = 0.01 s stamps 全样本（含端点 t0），聚合用 dataset_v3 冻结
    管线（bin (e-1,e]，因果 t≤t0，覆盖门要求窗端点样本齐备）。
  - 下行 8 态：P_load/P_bat=OUT 读点、SOC/Vdc=X 边界真值、prop/hotel=stream t0
    样本；dis/chg=P_bat_actual 符号拆分（G1 *_avail 能力信号不在 PlantStepE
    出口，C3 口径登记）；全 f32 RNE cast，host 链消费 f32 回读值（板 rd_f32 同域）。

G6 机械化（S4_G6_ACCEPTANCE 定稿 v1.0）：
  G6-1 运行性（零 NACK/超时/中断、SOC∈[0.15,0.95]、|ΔP_gen_ref|≤ramp×dt=200
  kW/拍）＋ G6-2（A2 vs B_HOST 差异=CLOSED_LOOP_TRANSFORM_EFFECT 基线档案，非
  FAIL）＋ G6-3（B_BOARD vs B_HOST 逐拍）＋ G6-4（频次门）＋ G6-5（is_formal
  边界）＋ G6-6（事件冻结清单）。停机条件（运行时）：连续 ≥3 拍 action 不一致 /
  连续 ≥3 拍 P_gen_ref 超 1 网格 / NACK 重试仍失败 / SOC 出界 / flags≠0 / 植物
  异常 → park（写部分产物，exit 2）。

运行（板在环）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python tools/s4_c3_closedloop.py \
      --port /dev/cu.usbserial-1120
host 干跑（无板，验证 runner 机制；不出正式 gate，overall 恒 false）：
  PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python tools/s4_c3_closedloop.py --dry
产物：Host/results/s4_c3/{s4_c3_closedloop.csv, s4_c3_warmup.csv, s4_c3_gate.json}

口径 v2（2026-09-29 用户裁决：执行层 1 s 门控直接补齐，契约 §4.3）：
  --exec-period（默认 10=冻结 10 s 口径，行为逐字节不变，基线回归锚不动）。=1 时：
  正式拍剩余 999 子步按整秒边界拆 99+9×100 段（9×100≠999，规格以"整秒边界
  t0+j, j=1..9"与固件"植物子步 100 边界"语义为准），每段前 EXEC_REFRESH
  (0x12, 16B=[seq][SOC][v_ship][P_load] f32 → 0x13, 12B=[seq][P_gen_ref][lam_eff]
  f32) 三链同步刷新：bb_1s=板返回值（板内 λ 输入=自持 m_last_d1=上拍 TICK_CTRL
  d1）；bh_1s/a2_1s=host law.exec_layer/lam_eff 用本拍 TICK_CTRL 的 d1_bh/d1_a2
  （段内 e_hat≡d1 恒定）；输入全 f32 线域（与 10 s 拍下行同口径）。观测 CSV
  s4_c3_exec1s.csv（列见 EXEC1S_COLS）；gate 增 exec1s_observation 观测面
  （exec1s_n/exec1s_diff_n/exec1s_max_diff）——观测轮，不进 G6 停机面，判据由
  主 Agent 看首轮板跑数据后定。dry：不发 EXEC_REFRESH、无 bb_1s；bh_1s/a2_1s
  照算并作为植物 pgen 刷新源（与 10 s 干跑 pgen 源=pg_bh 同构）。预热段 =1 时
  也逐 1 s 刷新（无板 → law6 冷启动执行层；与既有 10 s 粒度差异登记于
  warm_refresh_1s 注释，is_formal=0 不入判据域）。
"""
from __future__ import annotations

import argparse
import collections
import ctypes
import hashlib
import json
import math
import os
import struct
import sys
import time
from datetime import datetime, timezone

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.path.dirname(HERE)


def _find_repo_root(start):
    d = os.path.abspath(start)
    while True:
        if (os.path.isfile(os.path.join(d, "AGENTS.md"))
                and os.path.isdir(os.path.join(d, "model"))
                and os.path.isdir(os.path.join(d, "firmware"))):
            return d
        parent = os.path.dirname(d)
        if parent == d:
            raise RuntimeError("repo root not found")
        d = parent


REPO = _find_repo_root(HOST)
EXP = os.path.join(REPO, "experiments", "softhil_ems03_2026-09-25")
S1 = os.path.join(EXP, "ems04_s1")
S3_TOOLS = os.path.join(EXP, "ems04_s3", "tools")
MODELE = os.path.join(REPO, "model", "simulink_modelE_(Phase4_Physics)")
RESULTS = os.path.join(HOST, "results", "s4_c3")
KF_TABLE = os.path.join(EXP, "results", "s4_c3", "S20_composite.kf_table.bin")
S20_MAT = os.path.join(MODELE, "datasets_v3", "pilot", "S20_composite.mat")
PMS_LIB = os.path.join(EXP, "results", "s4_c2", "libems04_pms_safety.dylib")
PLANT_DYLIB = os.path.join(HOST, "plantstep_e", "S20_composite_voyage",
                           "libplantstepE_S20_composite_voyage.dylib")
SCENARIO = "S20_composite_voyage"

sys.path.insert(0, HERE)
sys.path.insert(0, HOST)
sys.path.insert(0, S3_TOOLS)                 # gen_ref_split_v2 / s3c_reatt_replay
sys.path.insert(0, os.path.join(MODELE, "python"))   # ppe / predictor_v3

from s3c_reatt_replay import enc_frame, crc16_ccitt, SYNC0, SYNC1  # noqa: E402
from s4_c3_h1win_host import H1WindowHost, COL_NAMES  # noqa: E402

DT_SUB = 0.01
SUB_PER_TICK = 1000
SUB_PER_SEC = 100
TICK_S = 10.0
WARMUP_TICKS = 30                            # t∈(0,300]；正式首拍首子步 stamp=300
FORMAL_TICKS = 541                           # t0=300..5700（kf issue 网格）
KK_FIRST = 30                                # 正式首拍控制拍序号（查询 t0=300）
RSP_LEN = 4 + 20 * 4 + 4 + 4 + 1
CMD_PING, CMD_RESET, CMD_TICK_CTRL, RSP_TICK_CTRL = 0x06, 0x01, 0x0E, 0x0F
# S4-P4 执行层 1 s 门控（口径 v2；线格式与 ems04_tick_ctrl.h EXEC_REFRESH 段同源）：
CMD_EXEC_REFRESH, RSP_EXEC_REFRESH = 0x12, 0x13
EXEC_REFRESH_CMD_LEN, EXEC_REFRESH_RSP_LEN = 16, 12   # [seq][SOC][v][P_load] / [seq][P_gen_ref][lam]

# ---- 冻结常量（禁手打；全部自 constants.json 读出）----
CONST = json.load(open(os.path.join(S1, "exported", "constants.json")))
LAW_PORTS = CONST["law6_ports"]
LAMBDA0 = float(CONST["lambda0"])
K_SOC = float(CONST["K_soc"])
SOC_REF = float(CONST["SOC_ref"])
K_S_B2 = float(CONST["K_S_B2"])
LAMBDA_FLOOR = float(CONST["lambda_floor"])
RAMP_KWPS = float(CONST["ramp"])             # 20 kW/s（law6 发电机组斜率参数）
PGEN_STEP_BOUND = RAMP_KWPS * TICK_S         # G6-1 每控制拍界限 = 200 kW
GRID_DP = float(LAW_PORTS["ECMS_Grid_dP_kW"])
GRID_DLAM = float(CONST["dlam_step"])        # action index 网格（0.005）

RADPS2RPM = 30.0 / math.pi                   # build_model_e.m radps2rpm 增益
SOC_LO, SOC_HI = 0.15, 0.95                  # G6-1 场景界（定稿 v1.0 原文）
SCENARIO_COLS = ("v_ref_mps", "P_hotel_kW", "v_current_mps", "P_prop_demand_kW",
                 "water_depth_m")
A2_BUF_CAP = 31000                           # 0.01 stamps 滚动缓冲（>300 s 窗）

# CPUF32 形态 host 代理模型（偏差④修复交付形态；S4_PRED_PHYSICS_RCA.md §5-A）
TFLITE_FP32 = os.path.join(HOST, "..", "Model", "reatt8_q",
                           "reatt_raw_fp32.tflite")


class TfliteFp32Chain:
    """CPUF32 板形态的 host 代理（--bh-chain tflite-fp32 用）。

    口径 = ems_reatt.c CPU 段逐项同式：f32 域冻结归一化（gen_ref_split_v2.
    normalize f32_domain=True，与固件 ems_reatt.c 归一化环同域）→
    reatt_raw_fp32.tflite（== 板载生成码的源模型；host 实测 == keras fp32
    逐位级一致）→ 固件同式单调分位变换（gen_ref_split_v2.
    firmware_quantile_transform）。与 Chain 同接口：run(hist,kf)->{"q":(30,3)}。
    """

    def __init__(self):
        import tensorflow as tf
        import gen_ref_split_v2 as g
        self._g = g
        self._norm = g.load_norm_artifacts()
        self._ip = tf.lite.Interpreter(model_path=TFLITE_FP32)
        self._ip.allocate_tensors()
        ins = self._ip.get_input_details()
        self._i_hist = [i["index"] for i in ins if "hist" in i["name"]][0]
        self._i_kf = [i["index"] for i in ins if "kf" in i["name"]][0]
        self._i_out = self._ip.get_output_details()[0]["index"]

    def run(self, hist_raw, kf_raw):
        g = self._g
        hist_n, kf_n = g.normalize(np.asarray(hist_raw, dtype=np.float32),
                                   np.asarray(kf_raw, dtype=np.float32),
                                   self._norm, f32_domain=True)
        self._ip.set_tensor(self._i_hist, hist_n[None].astype(np.float32))
        self._ip.set_tensor(self._i_kf, kf_n[None].astype(np.float32))
        self._ip.invoke()
        raw = np.asarray(self._ip.get_tensor(self._i_out),
                         dtype=np.float32).reshape(30, 3)
        return {"q": g.firmware_quantile_transform(raw)}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def f32(v):
    """线上 f32 RNE cast（返回 f32 回读值——板 rd_f32 同域）。"""
    return struct.unpack("<f", struct.pack("<f", float(v)))[0]


# ---------------------------------------------------------------- kf 表读入
def load_kf_table(path):
    """EMS04KFT v1 解析（s4_c3_pack_kf_table.py 产物，只读）。"""
    blob = open(path, "rb").read()
    magic, fv, cv, code, n = struct.unpack_from("<8sIIII", blob, 0)
    issue_first, issue_step = struct.unpack_from("<dd", blob, 24)
    assert magic == b"EMS04KFT" and fv == 1, "kf 表 magic/版本不符"
    # S4-P3 B1（2026-09-29 向后兼容扩展）：放宽为登记过的 profile 域
    # {1=GOOD_V1, 2=DEGRADED_V1, 3=PILOT_FIXTURE}；S20 默认路径（code=3）行为不变。
    assert code in (1, 2, 3), "kf 表 profile_code 非法: %r" % (code,)
    payload = blob[216:]
    assert len(payload) == n * 728, "kf 表 payload 长度不符"
    rows = {}
    for i in range(n):
        off = i * 728
        (t,) = struct.unpack_from("<d", payload, off)
        rows[int(round(t))] = (payload[off + 8:off + 728],
                               np.frombuffer(payload, "<f4", 180, off + 8)
                               .reshape(30, 6))
    return {"n": n, "issue_first": issue_first, "issue_step": issue_step,
            "voyage_id": blob[40:72].split(b"\x00")[0].decode(),
            "rows": rows, "sha256": hashlib.sha256(blob).hexdigest()}


# ---------------------------------------------------------------- 植物推进器
class PlantE:
    """modelE dylib 子步级推进。子步粒度不在 plantstep_advancer 公共 API 内，
    直接复用其后端 ctypes 句柄（adv/_lib）；init/IC 语义仍由 advancer 管理。"""

    def __init__(self, scenario):
        from plantstep_advancer import make_advancer
        self.adv = make_advancer(backend="modelE", scenario=scenario)
        self._lib = self.adv._lib
        self.t = 0.0                          # 已推进到的模型时刻（子步末）
        self.last_out = None
        self.last_states = None

    def step_substep(self, p_gen):
        """推进 1 个 0.01 s 子步；返回 (stamp, out, states)。
        stamp = 子步起点模型时刻（自有 0.01 网格时钟；OUT 为 ERT pre-integration
        写出=子步起点值，t_s 信号自带 +0.0075 场景时移故不作 stamp——G-P2E 已证
        写点语义）；states() 为 X 直读 post-integration 真值（stamp+0.01）。"""
        stamp = round(self.t, 10)
        self._lib.plantstep_set_pgen(float(p_gen))
        self._lib.plantstep_step()
        out = self.adv._read_outputs()
        st = self.adv.states()
        self.t = round(self.t + DT_SUB, 10)
        return stamp, out, st

    def close(self):
        self.adv.close()


# ---------------------------------------------------------------- law6 host
class Law6:
    def __init__(self):
        self.lib = ctypes.CDLL(os.path.join(S1, "libems04_law6_c2.dylib"))
        self.lib.ems04_load_tables.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
        self.lib.ems04_load_tables.restype = ctypes.c_int
        self.lib.ems04_dp_tick.argtypes = [
            ctypes.c_void_p,
            np.ctypeslib.ndpointer(dtype=np.float64, flags="C_CONTIGUOUS"),
            ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_void_p,
            ctypes.c_void_p]
        self.lib.ems04_dp_tick.restype = ctypes.c_int
        self.T = self.Tables()
        assert self.lib.ems04_load_tables(
            os.path.join(S1, "exported").encode(), ctypes.byref(self.T)) == 0

    class Tables(ctypes.Structure):
        _fields_ = [("Pg", ctypes.POINTER(ctypes.c_double)),
                    ("bsfc_lut", ctypes.POINTER(ctypes.c_double)),
                    ("socx", ctypes.POINTER(ctypes.c_double)),
                    ("dlam", ctypes.POINTER(ctypes.c_double)),
                    ("socx_owned", ctypes.POINTER(ctypes.c_double)),
                    ("dlam_owned", ctypes.POINTER(ctypes.c_double))]

    class GenState(ctypes.Structure):
        _fields_ = [("st", ctypes.c_double * 2), ("P_ramp", ctypes.c_double * 2),
                    ("P_lag", ctypes.c_double * 2), ("t_dwell", ctypes.c_double * 2)]

    class TickResult(ctypes.Structure):
        _fields_ = [("dlambda_first", ctypes.c_double), ("J_window_kg", ctypes.c_double),
                    ("soc_end", ctypes.c_double), ("fuel_proxy_kg", ctypes.c_double),
                    ("oob_query_count", ctypes.c_int64),
                    ("infeasible_fallback_n", ctypes.c_int64),
                    ("n_fallback_steps", ctypes.c_int),
                    ("dlambda_all", ctypes.c_double * 30),
                    ("soc_path", ctypes.c_double * 31),
                    ("N_path", ctypes.c_double * 31)]

    def dp_tick(self, pbar, soc, n0):
        g = self.GenState()
        g.st[0] = g.st[1] = 3.0                # A-02：PG 恒 ON，t_dwell 强制 0
        g.P_ramp[0] = g.P_ramp[1] = 0.0
        g.P_lag[0] = g.P_lag[1] = 0.0
        g.t_dwell[0] = g.t_dwell[1] = 0.0
        r = self.TickResult()
        rc = self.lib.ems04_dp_tick(ctypes.byref(self.T),
                                    np.ascontiguousarray(pbar, dtype=np.float64),
                                    30, float(soc), float(n0),
                                    ctypes.byref(g), ctypes.byref(r))
        assert rc == 0, "ems04_dp_tick rc=%d" % rc
        return float(r.dlambda_first)

    def lam_eff(self, d1, soc):
        soc_ref_eff = SOC_REF + K_S_B2 * d1
        return max(LAMBDA0 + K_SOC * (soc_ref_eff - soc), LAMBDA_FLOOR)

    def exec_layer(self, p_load_kw, soc, lam):
        """契约 §4.3 网格穷举（板 s4_exec_grid 同式；常数全自 constants.json）。"""
        eta_rect = LAW_PORTS["ASSUMED_eta_rect"]
        eta_dcdc = LAW_PORTS["ASSUMED_eta_dcdc"]
        eta_dis = LAW_PORTS["ASSUMED_Bat_Eff_Discharge"]
        eta_chg = LAW_PORTS["ASSUMED_Bat_Eff_Charge"]
        p_max = LAW_PORTS["Gen_P_max"]
        sdm = LAW_PORTS["ECMS_SOC_Discharge_Max"]
        scm = LAW_PORTS["ECMS_SOC_Charge_Min"]
        idle = LAW_PORTS["ECMS_Idle_Fuel_kgph"]
        grid = LAW_PORTS["ECMS_Grid_dP_kW"]
        bp = np.asarray(LAW_PORTS["BSFC_Breakpoints_kW"], dtype=np.float64)
        tbl = np.asarray(LAW_PORTS["BSFC_Table_gkWh"], dtype=np.float64)
        best_p, best_j = 0.0, float("inf")
        n = int(p_max // grid) + 1
        for i in range(n):
            pg = i * grid
            pbat = p_load_kw - eta_rect * pg
            if soc <= sdm:
                if pbat > 1e-9:
                    continue
                best_p = pg
                break
            if soc >= scm and pbat < 0:
                continue
            if pg < 1e-9:
                mf = idle / 3600.0
            else:
                nb = len(bp)
                if pg >= bp[nb - 1]:
                    bsfc = tbl[nb - 1]
                elif pg <= bp[0]:
                    bsfc = tbl[0]
                else:
                    ii = 0
                    while ii < nb - 2 and bp[ii + 1] < pg:
                        ii += 1
                    w = (pg - bp[ii]) / (bp[ii + 1] - bp[ii])
                    bsfc = tbl[ii] + w * (tbl[ii + 1] - tbl[ii])
                mf = pg * bsfc / 3.6e6
            mbe = (pbat / (eta_dcdc * eta_dis) / 3600.0 if pbat > 0
                   else pbat * (eta_dcdc * eta_chg) / 3600.0)
            j = mf + lam * mbe
            if j < best_j:
                best_j, best_p = j, pg
        if soc <= sdm and best_p < 1e-9 and p_load_kw > eta_rect * p_max:
            best_p = p_max
        return best_p


# ---------------------------------------------------------------- PMS/Safety
class _Arr2(ctypes.Structure):
    _fields_ = [("v", ctypes.c_double * 2)]


class PmsHost:
    """libems04_pms_safety（S4-G1 6001/6001）；State 布局 = run_g1_pms_gate.py。"""

    class Params(ctypes.Structure):
        _fields_ = [(n, ctypes.c_double) for n in (
            "PMS_LoadLPF_tau_s", "PMS_SOC_lo", "PMS_SOC_recovered",
            "PMS_P_chg_recovery_kW", "PMS_start_threshold_kW",
            "PMS_stop_threshold_kW", "T_start_hold_ticks", "T_stop_hold_ticks",
            "PMS_min_up_ticks", "PMS_S2_hold_ticks", "PMS_S4_hold_ticks",
            "PMS_start_fail_ticks", "PMS_firm_margin_kW", "ASSUMED_eta_rect",
            "DG_P_min_kW", "DG_P_rated_kW", "PMS_SOURCE_MODE",
            "ADAPTER_FORCE_MODE", "ADAPTER_FORCE_P1_KW", "ADAPTER_FORCE_P2_KW")]

    class State(ctypes.Structure):
        _fields_ = [
            ("P_load_filt", ctypes.c_double), ("u_rec", ctypes.c_int32),
            ("policy_init_done", ctypes.c_int32),
            ("commitment_state", ctypes.c_double), ("reason_code", ctypes.c_double),
            ("start_hold_timer", ctypes.c_double), ("stop_hold_timer", ctypes.c_double),
            ("online_timer", ctypes.c_double), ("pending_reason", ctypes.c_double),
            ("tr_reason", ctypes.c_double), ("chart_entered", ctypes.c_int32),
            ("fl_latch", _Arr2), ("prev_state", _Arr2), ("prev_fstp", _Arr2),
            ("s2_cnt", ctypes.c_double), ("s4_cnt", ctypes.c_double),
            ("s1b_ticks", _Arr2),
            ("fback_active", ctypes.c_double), ("fback_reason", ctypes.c_double),
            ("min_srp", ctypes.c_double), ("safety_init_done", ctypes.c_int32),
            ("UD_state_DG1", ctypes.c_double), ("UD_state_DG2", ctypes.c_double),
            ("UD_N_dispatch_fb", ctypes.c_double),
            ("UD_fault_latch", _Arr2), ("UD_force_start_req", _Arr2),
            ("UD_force_start_reason", ctypes.c_double),
            ("UD_force_stop_req", _Arr2), ("UD_force_stop_reason", ctypes.c_double),
            ("UD_ems_health_ok", ctypes.c_double),
            ("prev_cmd1", ctypes.c_double), ("prev_cmd2", ctypes.c_double),
            ("Vdc_V_in", ctypes.c_double)]

    class Out(ctypes.Structure):
        _fields_ = [(n, ctypes.c_double) for n in (
            "P_gen_req_baseline_kW", "P_commit_kW", "u_recovery",
            "P_recovery_actual_kW", "cmd_onoff_DG1", "cmd_onoff_DG2",
            "P_DG1_ref_kW", "P_DG2_ref_kW", "N_dispatch",
            "P_gen_unserved_kW", "P_gen_surplus_kW", "dispatch_min_clamp_active",
            "dispatch_n_zero_flag", "commitment_state", "reason_code",
            "fs0", "fs1", "force_start_reason", "fstp0", "fstp1",
            "force_stop_reason", "fault0", "fault1", "ems_health_ok",
            "baseline_fallback_active", "baseline_fallback_reason")]

    PARAMS = Params(
        PMS_LoadLPF_tau_s=60.0, PMS_SOC_lo=0.55, PMS_SOC_recovered=0.60,
        PMS_P_chg_recovery_kW=100.0, PMS_start_threshold_kW=425.0,
        PMS_stop_threshold_kW=350.0, T_start_hold_ticks=3000.0,
        T_stop_hold_ticks=12000.0, PMS_min_up_ticks=30000.0,
        PMS_S2_hold_ticks=200.0, PMS_S4_hold_ticks=200.0,
        PMS_start_fail_ticks=4000.0, PMS_firm_margin_kW=50.0,
        ASSUMED_eta_rect=0.97, DG_P_min_kW=125.0, DG_P_rated_kW=500.0,
        PMS_SOURCE_MODE=1.0, ADAPTER_FORCE_MODE=0.0,
        ADAPTER_FORCE_P1_KW=0.0, ADAPTER_FORCE_P2_KW=0.0)

    FACE_KEYS = ("P_commit", "cstate", "rcode", "on1", "on2", "ref1", "ref2",
                 "N", "fault0", "fault1", "fs_reason", "fstp_reason",
                 "health", "fb")

    def __init__(self):
        self.lib = ctypes.CDLL(PMS_LIB)
        self.lib.ems04_pms_init.argtypes = [ctypes.c_void_p]
        self.lib.ems04_pms_tick.argtypes = ([ctypes.c_void_p, ctypes.c_void_p]
                                            + [ctypes.c_double] * 13
                                            + [ctypes.c_void_p])
        self.lib.ems04_pms_tick.restype = None
        self.S = self.State()
        self.O = self.Out()
        self.init()

    def init(self):
        self.lib.ems04_pms_init(ctypes.byref(self.S))

    def tick1000(self, p_load, soc, dis, chg, prop, hotel, p_gen_ref):
        """板上 ems04_tick_ctrl 同构：st1=3.0、st2=UD 反馈、DG 实际 0 喂、
        voyage_active=1.0（板 s4 形态注释同源）。"""
        S, P, O = self.S, self.PARAMS, self.O
        for _ in range(1000):
            self.lib.ems04_pms_tick(ctypes.byref(S), ctypes.byref(P),
                                    p_load, soc, 3.0, S.UD_state_DG2,
                                    0.0, 0.0, dis, chg, prop, hotel,
                                    p_gen_ref, 1.0, 1.0, ctypes.byref(O))

    def faces(self):
        O = self.O
        return {"P_commit": O.P_commit_kW, "cstate": O.commitment_state,
                "rcode": O.reason_code, "on1": O.cmd_onoff_DG1,
                "on2": O.cmd_onoff_DG2, "ref1": O.P_DG1_ref_kW,
                "ref2": O.P_DG2_ref_kW, "N": O.N_dispatch,
                "fault0": O.fault0, "fault1": O.fault1,
                "fs_reason": O.force_start_reason,
                "fstp_reason": O.force_stop_reason,
                "health": O.ems_health_ok, "fb": O.baseline_fallback_active}


# ---------------------------------------------------------------- 板客户端
class BoardClient:
    def __init__(self, port, baud=115200):
        import serial
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.nack_n = 0
        self.retry_n = 0

    def _send(self, ftype, pl=b""):
        self.ser.write(enc_frame(ftype, pl))
        self.ser.flush()

    def _wait(self, ftype, timeout_ms):
        deadline = time.monotonic() + timeout_ms / 1000.0
        buf = b""
        while time.monotonic() < deadline:
            chunk = self.ser.read(self.ser.in_waiting or 1)
            if chunk:
                buf += chunk
            while True:
                i = buf.find(bytes([SYNC0, SYNC1]))
                if i < 0:
                    if len(buf) > 2:
                        buf = buf[-1:]
                    break
                if len(buf) < i + 6:
                    break
                rtype = buf[i + 2]
                ln = struct.unpack_from("<H", buf, i + 3)[0]
                if len(buf) < i + 5 + ln + 2:
                    break
                frame = buf[i:i + 5 + ln + 2]
                body = frame[2:-2]
                if struct.unpack("<H", frame[-2:])[0] != crc16_ccitt(body):
                    buf = buf[i + 2:]
                    continue
                payload = frame[5:5 + ln]
                buf = buf[i + 5 + ln + 2:]
                if rtype == ftype:
                    return payload
        return None

    def ping(self, attempts=5):
        for _ in range(attempts):
            self._send(CMD_PING)
            if self._wait(0x7F, 500) is not None:
                return True
        return False

    def reset(self):
        self._send(CMD_RESET)
        return self._wait(0x7F, 1000) is not None

    def tick_ctrl(self, seq, t0, st8, hist_f32, kf_f32):
        """下行 + 等响应；超时同 seq 重试一次；仍失败返回 None（park 条件）。"""
        payload = (struct.pack("<II", seq, int(t0))
                   + struct.pack("<8f", *st8)
                   + hist_f32.astype("<f4").tobytes()
                   + kf_f32.astype("<f4").tobytes())
        for attempt in (0, 1):
            self._send(CMD_TICK_CTRL, payload)
            pl = self._wait(RSP_TICK_CTRL, 12000)
            if pl is not None and len(pl) >= RSP_LEN:
                if attempt == 0:
                    pass
                vals = struct.unpack_from("<20f", pl, 4)
                infer_us, ctrl_us = struct.unpack_from("<II", pl, 4 + 80)
                flags = pl[4 + 88]
                (pg, pcom, cst, rcode, on1, on2, r1, r2, nd, d1, lam,
                 f0, f1, fs0, fs1, fstp0, fstp1, health, fb) = vals[:19]
                return {"P_gen_ref": pg, "P_commit": pcom, "cstate": cst,
                        "rcode": rcode, "on1": on1, "on2": on2, "ref1": r1,
                        "ref2": r2, "N": nd, "d1": d1, "lam": lam,
                        "fault0": f0, "fault1": f1, "fs0": fs0, "fs1": fs1,
                        "fstp0": fstp0, "fstp1": fstp1, "health": health,
                        "fb": fb, "infer_us": infer_us, "ctrl_us": ctrl_us,
                        "flags": flags}
            if attempt == 0:
                self.retry_n += 1
        self.nack_n += 1
        return None

    def exec_refresh(self, seq, soc_f32, v_ship_f32, p_load_f32):
        """EXEC_REFRESH（口径 v2 执行层 1 s 门控）：下行 16 B = seq+SOC+v_ship+
        P_load f32 → RSP 12 B = seq+P_gen_ref+lam_eff f32（ems04_exec_refresh
        线格式）。超时同 seq 重试一次；仍失败返回 None（调用方 park——运行性
        语义同 TICK_CTRL；nack/retry 计入同一计数面，G6-1 零 NACK 门覆盖）。"""
        payload = struct.pack("<Ifff", int(seq), soc_f32, v_ship_f32, p_load_f32)
        for attempt in (0, 1):
            self._send(CMD_EXEC_REFRESH, payload)
            pl = self._wait(RSP_EXEC_REFRESH, 2000)
            if pl is not None and len(pl) >= EXEC_REFRESH_RSP_LEN:
                pg, lam = struct.unpack_from("<2f", pl, 4)
                return {"P_gen_ref": pg, "lam": lam}
            if attempt == 0:
                self.retry_n += 1
        self.nack_n += 1
        return None

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass


# ---------------------------------------------------------------- A2 输入装配
class A2Assembler:
    """A2 canonical 域输入装配（登记见 design notes §列源表）：
    活通道 = 子步真样本（OUT pre-integration stamps；omega X 读平移至同一 stamp）；
    场景侧列 = stream 100 Hz 同 stamp 因果切片（不触真值列）；
    聚合 = ppe.h1_assembly.h1_from_raw_logs（dataset_v3 冻结管线，bin (e-1,e]）；
    预测器 = ppe.c2_inference.C2Predictor（FR836 float keras，SHA fail-closed）。"""

    H1_LIVE = ("P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm")

    def __init__(self, stream):
        import os as _os
        _os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
        import tensorflow as tf
        tf.config.set_visible_devices([], "GPU")   # 强制 CPU（可复现纪律）
        from ppe.c2_inference import C2Predictor
        from ppe.h1_assembly import h1_from_raw_logs
        from ppe.causality import REQUIRED_H1, REQUIRED_K, REQUIRED_F
        assert tuple(REQUIRED_H1) == COL_NAMES, "REQUIRED_H1 与板列序不符"
        self._h1 = h1_from_raw_logs
        self.kf_order = list(REQUIRED_K) + list(REQUIRED_F)
        self.pred = C2Predictor.instance()
        self.stream = stream
        self.t_buf = collections.deque(maxlen=A2_BUF_CAP)
        self.v_buf = {c: collections.deque(maxlen=A2_BUF_CAP)
                      for c in self.H1_LIVE}

    def push_substep(self, stamp, out, states):
        """每子步调用：stamp = 子步起点（自有 0.01 网格时钟）；omega X 读
        （真值 stamp+0.01）平移对齐至 stamp（登记 n_prop 0.01 s 平移）。"""
        self.t_buf.append(float(stamp))
        self.v_buf["P_load_total_kW"].append(float(out["P_load_total_kW"]))
        self.v_buf["v_ship_mps"].append(float(out["v_ship_mps"]))
        self.v_buf["acceleration_mps2"].append(float(out["acceleration_mps2"]))
        self.v_buf["n_prop_rpm"].append(float(states["omega_prop_radps"]) * RADPS2RPM)

    def predict(self, t0, kf_row_f32):
        t = np.fromiter(self.t_buf, dtype=np.float64, count=len(self.t_buf))
        m = t <= t0 + 1e-9
        t = t[m]
        cols = {c: np.fromiter(self.v_buf[c], dtype=np.float64,
                               count=len(self.v_buf[c]))[m]
                for c in self.H1_LIVE}
        idx = np.rint(t * SUB_PER_SEC).astype(np.int64)
        for c in SCENARIO_COLS:
            if c == "water_depth_m":
                continue
            cols[c] = self.stream[c][idx]
        cols["water_depth_m"] = np.full_like(t, float(self.stream["water_depth_m"][0]))
        cols["ctrl_mode"] = np.zeros_like(t)
        cols["manual_power_cmd_pct"] = np.zeros_like(t)
        h1 = self._h1(t, cols, float(t0))            # (300,11) f64 冻结管线
        hist_raw = {c: h1[:, j] for j, c in enumerate(COL_NAMES)}
        kf_raw = {c: kf_row_f32[:, j].astype(np.float64)
                  for j, c in enumerate(self.kf_order)}
        return np.asarray(self.pred.predict_q50(hist_raw, kf_raw), dtype=np.float64)


# ---------------------------------------------------------------- 主流程
CSV_COLS = (
    ["tick", "t0", "seq", "is_formal", "pgen_used_kw"]
    + ["plant_" + k for k in ("P_load", "SOC", "Vdc", "P_bat", "v_ship",
                              "n_prop_rpm", "x_ship", "voyage_stop")]
    + ["dl_" + k for k in ("P_load", "SOC", "Vdc", "dis", "chg", "prop", "hotel")]
    + ["q50_head_A2", "q50_head_BH"]
    + ["bb_" + k for k in ("d1", "action", "lam", "P_gen_ref", "P_commit",
                           "cstate", "rcode", "on1", "on2", "ref1", "ref2", "N",
                           "fault0", "fault1", "fs0", "fs1", "fstp0", "fstp1",
                           "health", "fb", "flags", "infer_us", "ctrl_us")]
    + ["bh_" + k for k in ("d1", "action", "lam", "P_gen_ref") + PmsHost.FACE_KEYS]
    + ["a2_" + k for k in ("d1", "action", "lam", "P_gen_ref") + PmsHost.FACE_KEYS]
    + ["bb_bh_action_eq", "bb_bh_attr", "bb_bh_sub", "bb_bh_pgen_diff",
       "a2_bh_action_eq", "a2_bh_pgen_diff", "host_ms", "board_ms"]
)


def _ring_v(sec, out, st, stream):
    """环推样 v[0..10]（TICK2 载荷序 = 契约 §2.1 = 固件 ems04_h1win.c
    TICK2_TO_RING 注释序：[0]t_s [1]P_load [2]SOC [3]v_ref [4]v_ship
    [5]accel [6]x_ship [7]n_prop [8]P_hotel [9]v_current [10]P_prop_demand）。
    边界 stamp = 秒边界前最后子步读点（登记：OUT pre-integration 读点平移，
    design notes §采样网格）；v[0]=名义整秒（环契约 v[0]==tick_idx）。

    偏差④修复（2026-09-29）：v[1]/v[2]/v[3] 历史错误序 (SOC,v_ref,P_load)
    更正为契约序 (P_load,SOC,v_ref)——旧序使环 P_load 通道≈SOC(0.6)、
    v_ref 通道≈P_load（数百 m/s），S1~S4 判据盲区未覆盖（A2 vs B_HOST 差异
    被归档为"变换效应"未逐通道核对）；RCA 见 S4_PRED_PHYSICS_RCA.md。"""
    idx = max(0, int(round(sec * 100)) - 1)
    return (float(sec), float(out["P_load_total_kW"]), float(st["SOC"]),
            float(stream["v_ref_mps"][idx]), float(out["v_ship_mps"]),
            float(out["acceleration_mps2"]), float(st["x_ship_m"]),
            float(st["omega_prop_radps"]) * RADPS2RPM,
            float(stream["P_hotel_kW"][idx]), float(stream["v_current_mps"][idx]),
            float(stream["P_prop_demand_kW"][idx]))


def classify_action_face(a_bb, a_bh, a_a2):
    """G6 v2.1 action-face 归因（用户裁决 2026-09-29 + 演绎闭合，见 design_notes
    §5.1/§5.2）。bb-vs-bh 的差异只能经已登记的 REATT 后端通道产生（下行字节
    同源 + DP/law6/PMS 同输入位级同构已证：S1 377/377、G1 6001/6001），故
    action 不一致一律记 CLOSED_LOOP_BACKEND_DRIFT 档案（带子类标注）；
    硬判据面只剩：执行层同档 P_gen_ref 发散、板自振荡、运行性、事件面。
    返回 (attr, subclass)。"""
    if a_bb == a_bh:
        return "equal", "-"
    if a_bh == a_a2:
        return "drift", "host_consensus"
    if a_bb == a_a2:
        return "drift", "board_canonical"
    return "drift", "triple"


# ---------------------------------------------------------------- 口径 v2（执行层 1 s 门控）
EXEC1S_COLS = ("t", "soc", "p_load", "v_ship", "bb_pg", "lam_bb", "bh_pg",
               "a2_pg", "diff_bb_bh", "diff_a2_bh")


def exec1s_refresh(law, board, d1_bh, d1_a2, t_edge, out, st_prev, exec_seq):
    """整秒边界 t_edge 的执行层三链同步刷新（口径 v2，契约 §4.3 1 s 门控）。
    输入 = 植物当前读数 f32 线域（P_load=OUT 读点、SOC/v_ship=X 边界真值——
    与 10 s 拍下行同采样口径）；e_hat 段内恒定：bh 用本拍 TICK_CTRL 的 d1_bh、
    a2 用 d1_a2（板内同构 = 自持 m_last_d1）。板在环：EXEC_REFRESH 下行 →
    bb_1s=板 P_gen_ref/lam；dry（board=None）：不发帧、无 bb_1s（NaN），
    bh_1s/a2_1s 照算，植物 pgen 刷新源 = bh_1s（dry 语义，与 10 s 干跑
    pgen 源=pg_bh 同构——登记）。返回 dict；ok=False = 板失联（调用方 park，
    运行性语义同 TICK_CTRL 失联）。"""
    p_load_f = f32(out["P_load_total_kW"])
    soc_f = f32(st_prev["SOC"])
    v_f = f32(st_prev["v_ship_mps"] * 1.94384)   # m/s → kn（LCD SPEED 窗口径，同 EMS03 CMD_TICK）
    bh_pg = law.exec_layer(p_load_f, soc_f, law.lam_eff(d1_bh, soc_f))
    a2_pg = law.exec_layer(p_load_f, soc_f, law.lam_eff(d1_a2, soc_f))
    bb_pg = lam_bb = float("nan")
    pgen_next = bh_pg                      # dry 兜底（板在环成功时被覆盖）
    ok = True
    if board is not None:
        rsp = board.exec_refresh(exec_seq, soc_f, v_f, p_load_f)
        if rsp is None:
            ok = False
        else:
            bb_pg = float(rsp["P_gen_ref"])
            lam_bb = float(rsp["lam"])
            pgen_next = bb_pg
    return {"t": float(t_edge), "soc": soc_f, "p_load": p_load_f, "v_ship": v_f,
            "bb_pg": bb_pg, "lam_bb": lam_bb, "bh_pg": bh_pg, "a2_pg": a2_pg,
            "pgen_next": pgen_next, "ok": ok}


def warm_refresh_1s(law, run_fn, pg, out_last, st_prev):
    """口径 v2 预热段 1 s 粒度推进（--exec-period 1 时调用）：首段沿用拍首
    冷启动 pg；其后每 1 s 边界以当前植物读数重算冷启动执行层（无板无预测 →
    d1=0 的 law6 冷启动律）。返回 (out_last, st_prev, pg 末值)。
    与既有 10 s 粒度的差异登记：is_formal=0 预热控制粒度 10 s→1 s；输入用
    植物 f64 原值（预热无板无对拍，与既有预热计算同域）；不入 G6 判据域。"""
    out_last, _, st_prev = run_fn(SUB_PER_SEC, pg, st_prev)
    for _ in range(SUB_PER_TICK // SUB_PER_SEC - 1):
        pg = law.exec_layer(out_last["P_load_total_kW"], st_prev["SOC"],
                            law.lam_eff(0.0, st_prev["SOC"]))
        out_last, _, st_prev = run_fn(SUB_PER_SEC, pg, st_prev)
    return out_last, st_prev, pg


def exec1s_stats(rows, exec_period, csv_rel):
    """exec1s 观测面统计（gate JSON exec1s_observation；观测轮——不进 G6 停机
    面，判据由主 Agent 看首轮板跑数据后定）。rows = exec1s CSV 数据行元组
    （末两位 = diff_bb_bh / diff_a2_bh）。"""
    if exec_period != 1:
        return None
    diffs = [d for r in rows for d in (r[8], r[9]) if math.isfinite(d)]
    return {
        "exec_period_s": 1, "exec1s_n": len(rows),
        "exec1s_diff_n": sum(1 for r in rows
                             if (math.isfinite(r[8]) and r[8] > 1e-6)
                             or (math.isfinite(r[9]) and r[9] > 1e-6)),
        "exec1s_max_diff": (max(diffs) if diffs else 0.0),
        "diff_def": "行级：|bb_pg−bh_pg| 或 |a2_pg−bh_pg| 任一有限值 >1e-6 计 1"
                    "（NaN=干跑无 bb_1s，不参与）；max_diff 同域取最大",
        "csv": csv_rel,
    }


def _fmt(v):
    if isinstance(v, (bool, np.bool_)):
        return str(int(v))
    if isinstance(v, (int, np.integer)):
        return str(int(v))
    if isinstance(v, (float, np.floating)):
        return "nan" if math.isnan(float(v)) else "%.9g" % float(v)
    return str(v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/cu.usbserial-1120")
    ap.add_argument("--baud", type=int, default=115200,
                    help="板串口波特率（默认 115200=板当前固件口径；UART 提速切换时改此参数）")
    ap.add_argument("--dry", action="store_true", help="无板干跑（不出正式 gate）")
    ap.add_argument("--formal-ticks", type=int, default=FORMAL_TICKS)
    ap.add_argument("--exec-period", type=int, default=10,
                    help="执行层刷新周期 s（口径 v2；默认 10=冻结 10 s 口径逐字节"
                         "不变；=1 每整秒边界 EXEC_REFRESH 三链同步刷新）")
    ap.add_argument("--kf-table", default=KF_TABLE,
                    help="kf 表路径（默认 = 冻结 S20 口径；偏差④修复后用 "
                         "S20_composite_v2.kf_table.bin，见 S4_PRED_PHYSICS_RCA.md §3-R3）")
    ap.add_argument("--mat", default=S20_MAT,
                    help="场景侧 stream 源 .mat（默认 = 冻结 S20；v2 修复件同列语义）")
    ap.add_argument("--bh-chain", default="split",
                    choices=("split", "tflite-fp32"),
                    help="B_HOST 链：split=板源 OE int8 N1+N2 ORT 代理（历史默认）；"
                         "tflite-fp32=CPUF32 板形态 host 代理（偏差④修复后对拍口径）")
    args = ap.parse_args()
    if args.exec_period not in (1, 10):
        print("[c3] FAIL-CLOSED: --exec-period 仅支持 1/10，实际 %r"
              % (args.exec_period,))
        return 2

    os.makedirs(RESULTS, exist_ok=True)
    t_start_wall = time.time()

    # ---- 冻结件 ----
    kft = load_kf_table(args.kf_table)
    print("[c3] kf 表: n=%d issue=%g..%g step=%g vid=%s sha=%s…"
          % (kft["n"], kft["issue_first"],
             kft["issue_first"] + (kft["n"] - 1) * kft["issue_step"],
             kft["issue_step"], kft["voyage_id"], kft["sha256"][:12]))

    from scipy.io import loadmat
    print("[c3] 载入场景 stream（场景侧时间表列，只读）：%s"
          % os.path.basename(args.mat))
    mat = loadmat(args.mat, simplify_cells=True)
    stream = {c: np.asarray(mat["stream"][c], dtype=np.float64).ravel()
              for c in set(SCENARIO_COLS) | {"time_s"}}
    meta = {"voyage_id": str(mat["meta"]["voyage_id"]),
            "fixture_kind": str(mat["meta"]["fixture_kind"])}
    del mat

    print("[c3] 初始化植物/law6/PMS/host 链（keras/ORT 首载约 10-20 s）…")
    plant = PlantE(SCENARIO)
    law = Law6()
    pms_bh = PmsHost()
    pms_a2 = PmsHost()
    from gen_ref_split_v2 import Chain
    if args.bh_chain == "tflite-fp32":
        chain = TfliteFp32Chain()   # CPUF32 板形态 host 代理（偏差④修复口径）
    else:
        chain = Chain()                          # 板源 OE int8 N1+N2 ORT 代理（host 侧）
    a2 = A2Assembler(stream)
    ring = H1WindowHost()
    ring.begin(0, float(stream["water_depth_m"][0]), 0.0, 0)

    board = None
    if not args.dry:
        board = BoardClient(args.port, args.baud)
        if not board.ping():
            print("[c3] PARK: PING 失败（板失联；需按加载通道重载后重跑）")
            return 2
        print("[c3] 板 PING OK（S4Smoke SRAM 形态在环）")

    # ---- 环 IC 样本（tick_first=0，场景初值口径，harness ref_G2 row0 同法）----
    st_ic = {"SOC": 0.60, "x_ship_m": 0.0, "omega_prop_radps": 0.0,
             "v_ship_mps": 0.0, "V_dc_V": 750.0}
    ic = _ring_v(0, {"P_load_total_kW": 100.0, "v_ship_mps": 0.0,
                     "acceleration_mps2": 0.0}, st_ic, stream)
    ring.tick(0, ic)

    def _run_substeps(n, pgen, st_prev):
        """跑 n 个子步（恒 pgen）；推 A2；stamp 命中整秒（>0）时推环
        （OUT=stamp 值 + 上一子步 X 真值=stamp 值）。返回 (out_last, st_last,
        st_prev, voyage_stop_seen)。"""
        out = st = None
        for _ in range(n):
            stamp, out, st = plant.step_substep(pgen)
            a2.push_substep(stamp, out, st)
            if abs(stamp - round(stamp)) < 1e-9 and int(round(stamp)) > 0:
                sec = int(round(stamp))
                ring.tick(sec, _ring_v(sec, out, st_prev, stream))
            st_prev = st
        return out, st, st_prev

    # ---- 预热段（is_formal=0；板不参与；law6 冷启动控制）----
    warm_csv = os.path.join(RESULTS, "s4_c3_warmup.csv")
    f_warm = open(warm_csv, "w")
    f_warm.write("tick,t_end,pgen_cold_kw,P_load,SOC,Vdc,voyage_stop\n")
    pgen_next = 0.0
    st_prev = st_ic
    out_last = None
    print("[c3] 预热 %d 拍（law6 冷启动，is_formal=0）…" % WARMUP_TICKS)
    for kk in range(WARMUP_TICKS):
        if kk == 0:
            # k=0 拍 X/OUT 未算（ERT initialize 不算根 Outport）：场景 IC 构造
            p_load, soc = 100.0, 0.60
        else:
            p_load = out_last["P_load_total_kW"]
            soc = st_prev["SOC"]
        lam_cold = law.lam_eff(0.0, soc)      # 冷启动：无窗无预测 → e_hat=0
        pg = law.exec_layer(p_load, soc, lam_cold)
        if args.exec_period == 1:
            # 口径 v2：预热段也逐 1 s 刷新（无板 → law6 冷启动执行层）；
            # 与既有 10 s 粒度差异登记于 warm_refresh_1s（is_formal=0 不入判据域）
            out_last, st_prev, pg = warm_refresh_1s(law, _run_substeps, pg,
                                                    out_last, st_prev)
        else:
            out_last, _, st_prev = _run_substeps(SUB_PER_TICK, pg, st_prev)
        pgen_next = pg
        f_warm.write("%d,%g,%.9g,%.9g,%.9g,%.9g,%d\n"
                     % (kk, plant.t, pg, p_load, soc, st_prev["V_dc_V"],
                        int(out_last["voyage_stop"])))
    f_warm.close()
    print("[c3] 预热完成 t=%.0f SOC=%.4f 环 ticks=%d（-> %s）"
          % (plant.t, st_prev["SOC"], WARMUP_TICKS * 10 + 1,
             os.path.basename(warm_csv)))

    # ---- 偏差④治理（判据盲区，2026-09-29）：预热段植物 P_load 均值基线
    # （环 sanity 在正式首拍环构造后执行——环契约要求 t0==last_tick≥300，
    # 预热末 last_tick=299 不可构造）。
    _warm_pl = float(np.mean([float(l.split(",")[3]) for l in
                              open(warm_csv).read().splitlines()[2:]]))

    # ---- 正式段 ----
    if not args.dry:
        assert board.reset(), "板 CMD_RESET 失败"
        pms_bh.init()                        # host PMS 与板复位同步
        pms_a2.init()

    csv_path = os.path.join(RESULTS, "s4_c3_closedloop.csv")
    f_csv = open(csv_path, "w")
    f_csv.write(",".join(CSV_COLS) + "\n")
    exec_1s = args.exec_period == 1
    exec1s_path = os.path.join(RESULTS, "s4_c3_exec1s.csv")
    f_exec1s = open(exec1s_path, "w") if exec_1s else None
    if f_exec1s is not None:
        f_exec1s.write(",".join(EXEC1S_COLS) + "\n")
    exec1s_rows = []                         # 观测面统计源（末两位=diff 列）
    rows = []
    seq = 0
    exec_seq = 0                             # EXEC_REFRESH 独立序号（板回显不校验）
    stop_reason = None
    consec_unattr_pgen = 0
    n_drift_action = n_unattr_pgen = n_drift_pgen = 0
    n_a2_action_diff = n_a2_pgen_gt1 = 0
    first_unattr_pgen = None
    drift_ticks = []
    drift_sub = {}
    bb_pg_hist = []
    pg_flips = 0
    voyage_stop_tick = None

    print("[c3] 正式段 %d 拍（t0=%d..%d，is_formal=1）…"
          % (args.formal_ticks, KK_FIRST * TICK_S,
             (KK_FIRST + args.formal_ticks - 1) * TICK_S))
    for kk in range(KK_FIRST, KK_FIRST + args.formal_ticks):
        t0 = kk * TICK_S                     # 首子步 stamp=t0（闭合窗与环边界）
        if int(t0) not in kft["rows"]:
            stop_reason = "kf 表缺 issue=%.0f（fail-closed）" % t0
            break
        # 1) 本拍第一个子步（pgen=上一拍板值；ZOH 0.01 s 前沿已登记）：
        #    OUT@t0 恰好闭合 A2 窗 (t0-300,t0] 与环边界（stamp=t0 整秒推环）
        out, _, st_prev = _run_substeps(1, pgen_next, st_prev)
        if out["voyage_stop"] > 0 and voyage_stop_tick is None:
            voyage_stop_tick = t0
        if not all(math.isfinite(x) for x in
                   (out["SOC"], out["P_load_total_kW"], st_prev["v_ship_mps"],
                    st_prev["V_dc_V"])):
            stop_reason = "STOP: 植物数值异常（非有限）"
            break

        # 2) 下行 8 态（f32 线域；host 链消费同 f32 回读值）
        #    SOC/Vdc = X 边界真值（st_prev=上一子步末=t0）；P_load/P_bat = OUT@t0
        soc_x, vdc_x = st_prev["SOC"], st_prev["V_dc_V"]
        i_t0 = int(round(t0 * SUB_PER_SEC))
        st8_f64 = (out["P_load_total_kW"], soc_x, vdc_x,
                   max(out["P_bat_kW"], 0.0), max(-out["P_bat_kW"], 0.0),
                   stream["P_prop_demand_kW"][i_t0], stream["P_hotel_kW"][i_t0])
        st8 = tuple(f32(v) for v in st8_f64) + (0.0,)

        # 3) B hist（板算法 host 复刻）+ kf 行
        hist_b = ring.construct(int(t0))     # (300,11) f32
        if kk == KK_FIRST:
            # 偏差④治理（判据盲区）：环 P_load 通道 vs 植物实况量级 sanity
            # （fail-closed）——_ring_v 字段序错位类缺陷在此拦截。容差
            # [0.5×, 2×]：窗为 300 s 平滑 vs 段均值，正常差异远小于 2×。
            _ring_pl = float(np.mean(np.asarray(hist_b[:, 0],
                                                dtype=np.float64)))
            if not (0.5 * _warm_pl <= _ring_pl <= 2.0 * _warm_pl):
                stop_reason = ("ring_pl_sanity_fail: 环 P_load 均值 %.2f vs "
                               "预热植物 %.2f（疑似 TICK2 字段序/通道源错位）"
                               % (_ring_pl, _warm_pl))
                break
            print("[c3] 环 sanity OK：P_load 环窗均值 %.1f vs 预热植物 %.1f"
                  % (_ring_pl, _warm_pl))
        kf_bytes, kf_f32 = kft["rows"][int(t0)]

        # 4) 板在环
        board_ms = float("nan")
        if board is not None:
            t_b0 = time.time()
            bb = board.tick_ctrl(seq, t0, st8, hist_b, kf_f32)
            board_ms = (time.time() - t_b0) * 1000.0
            if bb is None:
                stop_reason = "板失联（TICK_CTRL 重试后仍无 RSP）"
                break
            if bb["flags"] != 0:
                stop_reason = "STOP: 板 flags=%d（infer/DP fail）" % bb["flags"]
                break
        else:
            bb = {k: float("nan") for k in
                  ("d1", "lam", "P_gen_ref", "P_commit", "cstate", "rcode",
                   "on1", "on2", "ref1", "ref2", "N", "fault0", "fault1",
                   "fs0", "fs1", "fstp0", "fstp1", "health", "fb",
                   "infer_us", "ctrl_us", "flags")}

        # 5) B_HOST 链（下行同字节）
        t_h0 = time.time()
        p_load_f, soc_f = st8[0], st8[1]
        dis_f, chg_f, prop_f, hotel_f = st8[3], st8[4], st8[5], st8[6]
        q_bh = np.asarray(chain.run(hist_b.reshape(300, 11),
                                    kf_f32.reshape(30, 6))["q"],
                          dtype=np.float64).reshape(30, 3)[:, 1]
        n0_bh = max(1.0, pms_bh.S.UD_N_dispatch_fb)
        d1_bh = law.dp_tick(q_bh, soc_f, n0_bh)
        lam_bh = law.lam_eff(d1_bh, soc_f)
        pg_bh = law.exec_layer(p_load_f, soc_f, lam_bh)
        pms_bh.tick1000(p_load_f, soc_f, dis_f, chg_f, prop_f, hotel_f, pg_bh)
        fc_bh = pms_bh.faces()

        # 6) A2 链（canonical 域）
        q_a2 = a2.predict(t0, kf_f32)
        n0_a2 = max(1.0, pms_a2.S.UD_N_dispatch_fb)
        d1_a2 = law.dp_tick(q_a2, soc_f, n0_a2)
        lam_a2 = law.lam_eff(d1_a2, soc_f)
        pg_a2 = law.exec_layer(p_load_f, soc_f, lam_a2)
        pms_a2.tick1000(p_load_f, soc_f, dis_f, chg_f, prop_f, hotel_f, pg_a2)
        fc_a2 = pms_a2.faces()
        host_ms = (time.time() - t_h0) * 1000.0

        # 7) 记录 + 判据面（G6 v2.1：classify_action_face 归因 + 板自振荡遥测）
        a_bh = int(round(d1_bh / GRID_DLAM))
        a_a2 = int(round(d1_a2 / GRID_DLAM))
        eq_bb_bh = pg_diff_bb_bh = None
        a_bb = float("nan")
        attr = "equal"
        sub = "-"
        if board is not None:
            a_bb = int(round(float(bb["d1"]) / GRID_DLAM))
            pg_diff_bb_bh = abs(float(bb["P_gen_ref"]) - pg_bh)
            eq_bb_bh = a_bb == a_bh
            attr, sub = classify_action_face(a_bb, a_bh, a_a2)
        pg_diff_a2_bh = abs(pg_a2 - pg_bh)
        eq_a2_bh = a_a2 == a_bh
        if board is not None:
            if attr == "drift":
                n_drift_action += 1
                drift_ticks.append(t0)
                drift_sub[sub] = drift_sub.get(sub, 0) + 1
            if pg_diff_bb_bh > GRID_DP + 1e-9:
                if eq_bb_bh:
                    n_unattr_pgen += 1          # action 同档但执行层发散=真缺陷
                    consec_unattr_pgen += 1
                    if first_unattr_pgen is None:
                        first_unattr_pgen = t0
                else:
                    n_drift_pgen += 1
                    consec_unattr_pgen = 0
            else:
                consec_unattr_pgen = 0
            # 板自振荡遥测：滑动 10 拍内 P_gen_ref 值变化次数 ≥6 → 停机硬面
            bb_pg_hist.append(float(bb["P_gen_ref"]))
            if len(bb_pg_hist) > 10:
                bb_pg_hist.pop(0)
            pg_flips = sum(1 for i in range(1, len(bb_pg_hist))
                           if bb_pg_hist[i] != bb_pg_hist[i - 1])
        if not eq_a2_bh:
            n_a2_action_diff += 1
        if pg_diff_a2_bh > GRID_DP + 1e-9:
            n_a2_pgen_gt1 += 1

        row = ([kk, t0, seq, 1, pgen_next]
               + [out["P_load_total_kW"], soc_x, vdc_x, out["P_bat_kW"],
                  st_prev["v_ship_mps"], st_prev["omega_prop_radps"] * RADPS2RPM,
                  st_prev["x_ship_m"], out["voyage_stop"]]
               + list(st8[:7])
               + [float(q_a2[0]), float(q_bh[0])]
               + [bb["d1"], a_bb, bb["lam"], bb["P_gen_ref"], bb["P_commit"],
                  bb["cstate"], bb["rcode"], bb["on1"], bb["on2"], bb["ref1"],
                  bb["ref2"], bb["N"], bb["fault0"], bb["fault1"], bb["fs0"],
                  bb["fs1"], bb["fstp0"], bb["fstp1"], bb["health"], bb["fb"],
                  bb["flags"], bb["infer_us"], bb["ctrl_us"]]
               + [d1_bh, a_bh, lam_bh, pg_bh] + [fc_bh[k] for k in PmsHost.FACE_KEYS]
               + [d1_a2, a_a2, lam_a2, pg_a2] + [fc_a2[k] for k in PmsHost.FACE_KEYS]
               + [int(eq_bb_bh) if eq_bb_bh is not None else "nan", attr, sub,
                  pg_diff_bb_bh if pg_diff_bb_bh is not None else "nan",
                  int(eq_a2_bh), pg_diff_a2_bh, host_ms, board_ms])
        rows.append(row)
        f_csv.write(",".join(_fmt(v) for v in row) + "\n")
        f_csv.flush()
        seq += 1

        # 8) 植物推进本拍剩余 999 子步（新板 P_gen_ref；ZOH 前沿 0.01 s 已登记）。
        #    默认 10 s 口径：单段 ZOH 不变（冻结行为逐字节不动）。口径 v2
        #    --exec-period 1：拆 99+9×100 段——首 99 子步（stamps t0+0.01..t0+0.99）
        #    仍用决策 pgen，其后每整秒边界 t0+j（j=1..9）段前刷新三链并切换
        #    植物 pgen（9×100≠999，以整秒边界+子步 100 边界语义为准，见文件头）。
        new_pgen = (float(bb["P_gen_ref"]) if board is not None else pg_bh)
        if exec_1s:
            out, _, st_prev = _run_substeps(SUB_PER_SEC - 1, new_pgen, st_prev)
            for j in range(1, int(TICK_S)):
                r1s = exec1s_refresh(law, board, d1_bh, d1_a2, t0 + j, out,
                                     st_prev, exec_seq)
                if not r1s["ok"]:
                    stop_reason = "板失联（EXEC_REFRESH 重试后仍无 RSP）"
                    break
                exec_seq += 1
                row1s = (r1s["t"], r1s["soc"], r1s["p_load"], r1s["v_ship"],
                         r1s["bb_pg"], r1s["lam_bb"], r1s["bh_pg"],
                         r1s["a2_pg"],
                         abs(r1s["bb_pg"] - r1s["bh_pg"])
                         if board is not None else float("nan"),
                         abs(r1s["a2_pg"] - r1s["bh_pg"]))
                exec1s_rows.append(row1s)
                f_exec1s.write(",".join(_fmt(v) for v in row1s) + "\n")
                out, _, st_prev = _run_substeps(SUB_PER_SEC, r1s["pgen_next"],
                                                st_prev)
                new_pgen = r1s["pgen_next"]
            if stop_reason is not None:
                break
        else:
            out, _, st_prev = _run_substeps(SUB_PER_TICK - 1, new_pgen, st_prev)
        pgen_next = new_pgen

        # 停机条件（G6 v2.1 硬面：同档执行层发散 / 板自振荡 / 运行性）
        if consec_unattr_pgen >= 3:
            stop_reason = ("STOP: B_BOARD vs B_HOST 连续 %d 拍同档 P_gen_ref 发散"
                           % consec_unattr_pgen)
            break
        if len(bb_pg_hist) == 10 and pg_flips >= 6:
            stop_reason = "STOP: 板 P_gen_ref 自振荡（10 拍内 %d 次翻转）" % pg_flips
            break
        if not (SOC_LO - 1e-9 <= soc_x <= SOC_HI + 1e-9):
            stop_reason = "STOP: SOC=%.4f 出界 [%.2f,%.2f]" % (soc_x, SOC_LO, SOC_HI)
            break
        if (kk - KK_FIRST) % 25 == 0:
            print("[c3] t0=%.0f bb_d1=%+.3f bh_d1=%+.3f a2_d1=%+.3f "
                  "pgen bb=%.0f bh=%.0f a2=%.0f soc=%.3f (%.0fs)"
                  % (t0, bb["d1"], d1_bh, d1_a2, bb["P_gen_ref"], pg_bh, pg_a2,
                     soc_x, time.time() - t_start_wall))

    f_csv.close()
    if f_exec1s is not None:
        f_exec1s.close()
    n_done = len(rows)

    # ---- G6 机械化（v2.1，classify_action_face 归因 + 子类）----
    gate = build_gate(rows, args, kft, stop_reason, n_drift_action,
                      n_unattr_pgen, n_drift_pgen, n_a2_action_diff,
                      n_a2_pgen_gt1, first_unattr_pgen, drift_ticks, drift_sub,
                      voyage_stop_tick, board, seq, t_start_wall, meta,
                      exec1s_stats=exec1s_stats(exec1s_rows, args.exec_period,
                                                os.path.relpath(exec1s_path,
                                                                REPO)))
    gate_path = os.path.join(RESULTS, "s4_c3_gate.json")
    json.dump(gate, open(gate_path, "w"), indent=1, ensure_ascii=False)
    print("[c3] 完成 %d/%d 拍；stop=%s" % (n_done, args.formal_ticks, stop_reason))
    print("[c3] G6 overall:", gate["overall"], "->", gate_path)
    if board is not None:
        board.close()
    plant.close()
    return 0 if gate["overall"] else 1


def build_gate(rows, args, kft, stop_reason, n_drift_action, n_unattr_pgen,
               n_drift_pgen, n_a2_action_diff, n_a2_pgen_gt1,
               first_unattr_pgen, drift_ticks, drift_sub, voyage_stop_tick,
               board, seq, t_start_wall, meta, exec1s_stats=None):
    n = len(rows)
    col = {name: i for i, name in enumerate(CSV_COLS)}

    def series(prefix, key):
        j = col[prefix + key]
        return [float(r[j]) for r in rows]

    soc = series("plant_", "SOC")
    step_max = {}
    for pre in ("bb", "bh", "a2"):
        pg = series(pre, "_P_gen_ref")
        steps = [abs(pg[i] - pg[i - 1]) for i in range(1, len(pg))
                 if not (math.isnan(pg[i]) or math.isnan(pg[i - 1]))]
        step_max[pre] = max(steps) if steps else 0.0
    max_consec_drift = run = 0
    for r in rows:
        if r[col["bb_bh_attr"]] == "drift":
            run += 1
            max_consec_drift = max(max_consec_drift, run)
        else:
            run = 0

    # G6-6 事件面（host 链运行时已计；板面从 rows 复核）。
    # 机械化修正（2026-09-29，首跑 62 拍证据）：RSP 无 force_start_reason 字段，
    # fs0/fs1 位 = 已声明 realized-gap 机制（reason=4，C2 冒烟声明）的锁存请求，
    # 板面声明性由 host 链 fs_reason∈{0,4} 佐证；板面未声明事件 = fault/health/
    # fallback/force_stop 位异常。
    undeclared = []
    host_reason_declared = all(
        r[col[pre + "_fs_reason"]] in (0.0, 4.0)
        and r[col[pre + "_fstp_reason"]] == 0.0
        for r in rows for pre in ("bh", "a2"))
    for r in rows:
        t0 = r[col["t0"]]
        if board is not None:
            if r[col["bb_fault0"]] or r[col["bb_fault1"]] \
                    or r[col["bb_health"]] != 1.0 or r[col["bb_fb"]] != 0.0 \
                    or r[col["bb_fstp0"]] != 0.0 or r[col["bb_fstp1"]] != 0.0 \
                    or (not host_reason_declared
                        and (r[col["bb_fs0"]] or r[col["bb_fs1"]])):
                undeclared.append({"t0": t0, "chain": "bb",
                                   "fault": [r[col["bb_fault0"]], r[col["bb_fault1"]]],
                                   "fs": [r[col["bb_fs0"]], r[col["bb_fs1"]]],
                                   "fstp": [r[col["bb_fstp0"]], r[col["bb_fstp1"]]],
                                   "health": r[col["bb_health"]],
                                   "fb": r[col["bb_fb"]]})
        for pre in ("bh", "a2"):
            if r[col[pre + "_fault0"]] or r[col[pre + "_fault1"]] \
                    or r[col[pre + "_health"]] != 1.0 \
                    or r[col[pre + "_fb"]] != 0.0 \
                    or r[col[pre + "_fs_reason"]] not in (0.0, 4.0) \
                    or r[col[pre + "_fstp_reason"]] != 0.0:
                undeclared.append({"t0": t0, "chain": pre,
                                   "fault": [r[col[pre + "_fault0"]],
                                             r[col[pre + "_fault1"]]],
                                   "fs_reason": r[col[pre + "_fs_reason"]],
                                   "fstp_reason": r[col[pre + "_fstp_reason"]],
                                   "health": r[col[pre + "_health"]],
                                   "fb": r[col[pre + "_fb"]]})

    n_nack = board.nack_n if board is not None else 0
    flags_bad = [v for v in series("bb_", "flags") if not math.isnan(v) and v != 0]
    g61 = {
        "n_ticks": n, "nack_n": n_nack,
        "retry_n": board.retry_n if board is not None else 0,
        "timeout_or_lost": bool(stop_reason and "失联" in str(stop_reason)),
        "flags_nonzero_n": len(flags_bad),
        "soc_min": min(soc) if soc else None,
        "soc_max": max(soc) if soc else None,
        "soc_bounds": [SOC_LO, SOC_HI],
        "soc_ok": bool(soc) and min(soc) >= SOC_LO - 1e-9
        and max(soc) <= SOC_HI + 1e-9,
        "pgen_ramp_bound_kw_per_tick": PGEN_STEP_BOUND,
        "pgen_ramp_param": "constants.ramp=%g kW/s × dt=%g s（law6 发电机组斜率"
                           "参数按控制拍折算；执行层输出无斜坡限制，见"
                           " LAW6_C2_CONSUMPTION_CONTRACT §4.3）"
                           % (RAMP_KWPS, TICK_S),
        "pgen_step_max_kw": step_max,
        "pgen_step_over_bound_kw": {k: (v if v > PGEN_STEP_BOUND + 1e-9 else 0.0)
                                    for k, v in step_max.items()},
        "mechanization_note": "首跑证据修正（2026-09-29）：执行层契约 §4.3 输出="
            "clamp[0,1000] 逐拍 argmin、无斜坡限制，ramp=20 kW/s 是植物侧发电机"
            "模型参数（L147）——|ΔP_gen_ref|≤200 kW/拍界与冻结契约冲突，降级为"
            "遥测面（step 分布全量记录）；G6-1 运行性门保留：零 NACK/超时/中断、"
            "SOC 界、零 flags。植物侧实际跟踪（p_gen_inertia+τ=5 s）由植物数值"
            "健康检查覆盖（首跑 62 拍吸收 0↔800 跳变无非有限/异常）。",
        "voyage_stop_tick": voyage_stop_tick,
    }
    g61_pass = ((comm_ok := (n_nack == 0 if board is not None else None)) is not False
                and g61["soc_ok"] and len(flags_bad) == 0
                and not g61["timeout_or_lost"])
    g62 = {
        "action_diff_n": n_a2_action_diff, "pgen_gt1grid_n": n_a2_pgen_gt1,
        "grid_kw": GRID_DP,
        "classification": "CLOSED_LOOP_TRANSFORM_EFFECT（基线档案，非 FAIL；"
                          "A2 canonical 域 vs B 部署同形域的已登记部署变换效应）",
    }
    g63 = {
        "action_equal_n": n - n_drift_action,
        "drift_action_diff_n": n_drift_action,
        "drift_subclasses": drift_sub,
        "unattributed_action_diff_n": 0,
        "unattributed_note": "v2.1：action 差全部经 classify_action_face 归入"
                             "后端漂移档案（演绎闭合：下行同源 + 下游位级同构"
                             "已证，差异仅能经 REATT 后端通道产生）；硬面只剩"
                             "同档 P_gen_ref 发散 / 板自振荡 / 运行性 / 事件面",
        "unattributed_pgen_n": n_unattr_pgen,
        "first_unattributed_pgen_t0": first_unattr_pgen,
    }
    g64 = {
        "n_formal": n,
        "unattributed_pgen_n": n_unattr_pgen,
        "max_pgen_gt1_allowed": max(1, math.ceil(0.01 * n)),
        "oscillation": "板 P_gen_ref 10 拍滑动窗翻转计数遥测（≥6 停机），见 CSV",
        "drift_rate_telemetry": {
            "drift_action_n": n_drift_action, "n": n,
            "rate": (n_drift_action / n) if n else None,
            "max_consec_drift": max_consec_drift,
        },
    }
    g64_pass = n_unattr_pgen <= g64["max_pgen_gt1_allowed"]
    g66 = {
        "declared_events": {
            "force_start_reason": "4（S2 realized-gap，C2 冒烟已声明）",
            "flags_nonzero": "0（infer/DP fail）",
            "fault_latch": "0/0", "baseline_fallback": "0",
        },
        "undeclared_events": undeclared[:20],
        "undeclared_n": len(undeclared),
    }
    overall = bool(g61_pass and g64_pass and g66["undeclared_n"] == 0
                   and n == args.formal_ticks and stop_reason is None
                   and not args.dry)
    # 口径 v2 观测面（2026-09-29 用户裁决）：--exec-period 1 时激活 EXEC_REFRESH
    # 三链 1 s 对拍统计；10 s 冻结口径不激活（n=0）。纯观测轮——不进 G6 停机面、
    # 不进 overall，判据由主 Agent 看首轮板跑数据后定。
    exec1s_block = dict(exec1s_stats) if exec1s_stats is not None else {
        "exec_period_s": 10, "exec1s_n": 0, "exec1s_diff_n": 0,
        "exec1s_max_diff": None,
    }
    exec1s_block["stop_face"] = False
    exec1s_block["note"] = ("口径 v2 观测轮：执行层 1 s 门控三链对拍只记录不停机"
                            "（diff 判据待首轮板跑数据后由主 Agent 定）；板失联"
                            "仍走运行性 G6-1 面（同 TICK_CTRL 语义）")
    return {
        "schema": "s4_c3_gate_v2_1",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "mode": "dry" if args.dry else "board",
        "dry_note": None if not args.dry else
            "干跑无板：B_BOARD 面=NaN，G6-3/G6-4 不适用，overall 恒 false",
        "adjudication": {
            "decision": "用户裁决 2026-09-29：认定 B_BOARD vs B_HOST action 差异"
                        "为 S3-C 已登记 REATT 后端数值漂移传播，G6-3/G6-4 一致性门"
                        "对该类拍次降级资格（CLOSED_LOOP_BACKEND_DRIFT 档案，非 "
                        "FAIL）；停机条件只对未归因不一致触发。",
            "evidence": "首跑（run1，62 拍 park）：t0=890/900/910 三拍 host 两链"
                        "（A2 float 与 B_HOST ORT-OE int8）彼此一致给出 action "
                        "-13/-17/-18/-15 档而板一致偏离（d1 钉 -0.1 网格下界）→"
                        "偏离定位于板后端；同签名已在 C3 前置对拍基线"
                        "（s4_c3_preamble_report.json，11 连拍）登记。",
        },
        "backend_drift_archive": {
            "rule": "G6-2 §3 语义延伸（v2.1 演绎闭合归因）：逐拍全量记录、分位数、"
                    "不作为 FAIL；硬面=同档 P_gen_ref 发散/板自振荡/运行性/事件面",
            "drift_action_diff_n": n_drift_action,
            "drift_subclasses": drift_sub,
            "drift_pgen_gt1grid_n": n_drift_pgen,
            "drift_ticks": drift_ticks,
            "drift_consec_max": max_consec_drift,
            "note": "漂移拍伴随执行层 argmin 翻转（0↔800 kW）：P_load 平稳段的 "
                    "J 值刀锋区（float canonical 链自身亦在相邻拍翻转），量化/"
                    "构造域不同的链在平局处分辨不同——登记变换效应。",
        },
        "stop_reason": stop_reason,
        "domain": {
            "warmup_ticks": WARMUP_TICKS, "formal_ticks": args.formal_ticks,
            "formal_done": n, "t0_first": KK_FIRST * TICK_S,
            "decision_period_s": TICK_S,
            "is_formal_rule": "预热段 is_formal=0 不入判据域（G6-5）；正式段="
                              "kf issue 网格 300..5700",
        },
        "form": {
            "plant": "PlantStepE GRT dylib (%s) sha=%s…"
                     % (SCENARIO, sha256_file(PLANT_DYLIB)[:12]),
            "board": None if board is None else
                "S4Smoke SRAM 直载（ELF 8b71d341 plan==readback，前会话载入；"
                "本会话 PING 探活 + CMD_RESET 后闭环）",
            "pms_lib_sha": sha256_file(PMS_LIB)[:12],
            "law6_lib": "libems04_law6_c2.dylib（S1 377/377 位级证据）",
            "b_host_chain": ("gen_ref_split_v2.Chain（板源 OE int8 N1+N2 ORT 代理）"
                             if getattr(args, "bh_chain", "split") == "split" else
                             "TfliteFp32Chain（CPUF32 host 代理：固件同式 f32 归一化"
                             "+reatt_raw_fp32.tflite+同式分位变换）"),
            "a2_chain": "ppe.c2_inference.C2Predictor（FR836 float keras，"
                        "q50 kW 域）",
            "kf_table": {"path": os.path.relpath(
                             getattr(args, "kf_table", KF_TABLE), REPO),
                         "sha256": kft["sha256"], "n_rows": kft["n"],
                         "voyage_id": kft["voyage_id"], "profile_code": 3,
                         "profile_meaning": "PILOT_FIXTURE"},
            "constants": os.path.relpath(os.path.join(S1, "exported",
                                                      "constants.json"), REPO),
            "s20_mat": os.path.relpath(getattr(args, "mat", S20_MAT), REPO),
            "s20_mat_fixture_kind": meta["fixture_kind"],
            "scenario_side_cols": list(SCENARIO_COLS),
            "col_provenance": {
                "live(P_load/v_ship/accel/n_prop)":
                    "plantE 子步读数（OUT pre-integration stamp 自洽；"
                    "omega=states() X 读 ×30/π）",
                "scenario(v_ref/P_hotel/v_current/P_prop_demand)":
                    "S20 pilot stream 时间表（t≤t0 因果切片）",
                "const(water_depth/ctrl_mode/manual)": "20.0 / 0.0 / 0.0",
            },
            "sampling_grid": "A2=0.01 s stamps 全样本冻结管线聚合（右端点 t0 不采"
                             "样）；B 环边界=秒边界前最后 stamp（v[0]=名义整秒）；"
                             "下行 SOC/Vdc=X 边界真值、P_load/P_bat=OUT 读点",
            "warmup_policy": "law6 冷启动（d1=0，λ=SOC 律，执行层网格）；板不参与",
            "downlink_dis_chg": "P_bat_actual 符号拆分（G1 *_avail 能力信号不在 "
                                "PlantStepE 出口；C3 口径登记）",
            "known_items": ["S4Smoke 形态 ctrl/infer_us=0（DWT 未使能）——遥测面项"],
        },
        "wall_s": round(time.time() - t_start_wall, 1),
        "seq_final": seq,
        "runnability_G6_1": g61, "g6_1_pass": g61_pass,
        "transform_archive_G6_2": g62,
        "board_parity_G6_3": g63,
        "g6_3_amended_pass": n_unattr_pgen == 0,
        "frequency_G6_4": g64, "g6_4_pass": g64_pass,
        "is_formal_G6_5": {"is_formal_only": True, "warmup_excluded": True},
        "events_G6_6": g66, "g6_6_pass": g66["undeclared_n"] == 0,
        "exec1s_observation": exec1s_block,
        "overall": overall,
        "overall_note": "G6-2 与 CLOSED_LOOP_BACKEND_DRIFT 均为基线档案语义（非 "
                        "FAIL，逐拍全量记录）；overall(v2)=G6-1∧G6-3amended∧G6-4"
                        "∧G6-6∧全拍完成∧无停机∧非干跑；未归因判据面=严格门。",
    }


if __name__ == "__main__":
    sys.exit(main())
