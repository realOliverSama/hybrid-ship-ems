# gen_reference_int8.py — T-506 交付物 2：Host 侧 int8 参考值生成工具
#
# 用途（design FR-502/FR-504/FR-505，T-506）：
#   1) --verify            T-307/T-312 冻结指标复现硬门（模式 A/B 前自动执行）：
#      MLP-Q int8 全 val 必须逐项等于 T-307 报告值；workclf int8 全 val（T-312
#      train_workclf.py 数据管线口径）必须等于 T-312 报告值。任一对不上 →
#      AGENTS.md §5 ESCALATE 并 exit 2（禁止换 runtime/改口径/放宽指标）。
#   2) --sample-val200 DIR  模式 A：200 个 val 代表向量（seed=20260831，排序后
#      导出 vectors_raw.csv（302 维原始物理量 f32）+ vectors_norm.csv +
#      reference_int8_val200.csv（int8 q10/q50/q90 [kWh] + e_hat + workclf cls）。
#      供 FR-504 INFER_TEST 机上对照（q50 |Δ|<=0.25 kWh）与 M1 遥测对照。
#   3) --trace CSV --out CSV  模式 B：以 ems_ctrl.c 为语义基准逐 tick 复刻固件
#      NN 特征构造（窗口/门控/迟滞）→ MLP-Q int8 + workclf int8 逐 tick CSV。
#
# workclf 输入口径说明：控制环内识别器输入 = 同一 xnorm 的切片——Pdec 10s 重采样
#   30 点 xnorm[10*j] + v/accel 归一化值 xnorm[300]/xnorm[301]（ems_ctrl.c 已同步
#   修订），与 T-312 训练口径 X[:, :300:10]+X[:,300:] 严格一致（2026-09 口径修订；
#   修订前 raw 口径致 int8 饱和坍缩，docs/agent/AI_MODELING.md §22）。
#   模式 A 保留 cls_raw/cls_norm 两列：修订后固件口径 = 归一化（=cls_norm），
#   cls_raw 列留作修订前 raw 口径对照；模式 B 按修订后固件语义（归一化切片）
#   逐 tick 输出。
# T-809（spec ems01-chil-tcn64-swap）：--model tcnq 预测器变体——TCN tflite
#   （Model/tcn_q/tcn_q.tflite，QDQ float32 I/O）双输入拆分喂解释器：
#   窗 [1,300,1] = xnorm[0:300]（升序）+ aux [1,2] = xnorm[300:302]（[v,a]），
#   输出 (N,3) 归一化 q3 与 mlpq 同域；向量/轨迹/输出格式与 mlpq 完全一致
#   （Fixed Decisions 10：不新造 schema），模式 A 产物文件名加 tcnq_ 前缀与
#   既有 mlpq 产物并存不覆盖；默认（无 --model 或 --model mlpq）行为逐字节不变。
#   路径纪律：全部由 __file__ 相对推导，禁止硬编码绝对路径。

import argparse
import csv
import hashlib
import os
import sys

import numpy as np

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJ = os.path.abspath(os.path.join(TOOLS_DIR, "..", ".."))
REPO = os.path.abspath(os.path.join(PROJ, "..", "..", "..", "..", "..", ".."))
WS = os.path.join(REPO, "model", "simulink_modelD_(EMS_NN_AECMS)")

MLPQ_TFLITE = os.path.join(WS, "results", "ecms_phase25", "models",
                           "mlp_h64_32_lr3e-4_q", "mlp_h64_32_lr3e-4_q_int8.tflite")
WORKCLF_TFLITE = os.path.join(WS, "results", "ecms_phase25", "classifier",
                              "workclf_v1_int8.tflite")
TCNQ_TFLITE = os.path.join(PROJ, "Model", "tcn_q", "tcn_q.tflite")
TCNQ_V2_TFLITE = os.path.join(PROJ, "Model", "tcn_q_v2", "tcn_q_v2.tflite")  # T-816b：原生 dilated Conv2D 重导出
PARAMS_C = os.path.join(PROJ, "Appli", "App", "ems_params.c")

MLPQ_SHA16 = "56fc3488ff0b210c"
WORKCLF_SHA16 = "7c3571e4eb7c9f0f"
TCNQ_SHA16 = "a3406b8466b5ae10"  # Model/tcn_q/tcn_q.tflite（NOTE.md 溯源，T-801）
TCNQ_V2_SHA16 = "fe130373b9baf152"  # Model/tcn_q_v2/tcn_q_v2.tflite（T-816b 重导出）
BATCH = 512
SEED_VAL200 = 20260831
N_VAL200 = 200
T307_MLPQ_INT8 = {"val_mae_kwh": 5.0385, "val_rmse_kwh": 9.9659,
                  "under_freq": 0.518, "under_mag_kwh": 5.1933,
                  "over_mag_kwh": 4.872, "q_coverage": 0.7905,
                  "q_pinball_norm": 0.1043}
T312_WORKCLF_INT8 = {"accuracy": 0.8437, "macro_f1": 0.6182}
GATE_S1_KWH = 28.9477
GATE_S2_KWH = 40.771
GATE_M = 3
GATE_UPSTREAM_CLS = 6
GATE_MODE = 2

def make_interp(path):
    os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
    import tensorflow as tf
    return tf.lite.Interpreter(model_path=path, num_threads=4)


def sha16(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()[:16]


def parse_ks_from_params():
    """从 ems_params.c 的 EMS_KS 初始化器解析 ebar_h_kwh / e_scale_kwh。"""
    txt = open(PARAMS_C, "r", encoding="utf-8", errors="replace").read()
    i = txt.find("const ems_ks_t EMS_KS = {")
    if i < 0:
        raise RuntimeError("EMS_KS not found in ems_params.c")
    ebar = None
    escale = None
    for line in txt[i:i + 400].splitlines():
        val = line.split("/*")[0].strip().rstrip(",")
        if "ebar_h_kwh" in line:
            ebar = float(val)
        if "e_scale_kwh" in line:
            escale = float(val)
    if ebar is None or escale is None:
        raise RuntimeError("EMS_KS ebar/e_scale missing in ems_params.c")
    return ebar, escale


def setup_tp_and_data():
    """t307 口径：train_predictor v2 作用域 + load_data。"""
    sys.path.insert(0, os.path.join(WS, "python"))
    import train_predictor as tp
    tp.DATA_NPZ = tp.BASE / "datasets" / "fleet_windows_v2.npz"
    tp.DATA_NORM_JSON = tp.BASE / "datasets" / "fleet_norm_v2.json"
    tp.DATA_MANIFEST_JSON = tp.BASE / "results" / "ecms_phase25" / "dataset_manifest_v2.json"
    tp.DATA_DATASET_VERSION = "v2"
    args = argparse.Namespace(dataset="v2", arch="tcn", quantiles=True, batch=BATCH,
                              epochs=100, patience=10, seed=42, only="all",
                              no_plots=True, scoped=True, decision_note=None,
                              select_run_id=None, finalize=False)
    return tp, tp.load_data(args)


def tflite_eval(interp, X):
    """t307/t312 同款逐批推理（首批 resize、末批重复补齐、裁剪输出）。"""
    out_dt = interp.get_output_details()[0]
    is_list = isinstance(X, (list, tuple))
    n = X[0].shape[0] if is_list else X.shape[0]
    outs = []
    for i in range(0, n, BATCH):
        if is_list:
            feeds = [x[i:i + BATCH].astype(np.float32) for x in X]
        else:
            feeds = [X[i:i + BATCH].astype(np.float32)]
        nb = feeds[0].shape[0]
        if nb < BATCH:
            pad = BATCH - nb
            feeds = [np.concatenate([f, f[-1:].repeat(pad, axis=0)], axis=0)
                     for f in feeds]
        if i == 0:
            for d, f in zip(interp.get_input_details(), feeds):
                interp.resize_tensor_input(d["index"], f.shape)
            interp.allocate_tensors()
        for d, f in zip(interp.get_input_details(), feeds):
            interp.set_tensor(d["index"], f)
        interp.invoke()
        o = interp.get_tensor(out_dt["index"])
        outs.append(np.asarray(o)[:nb])
    return np.concatenate(outs, axis=0)


def tcnq_feed_split(interp, Xn):
    """TCN 双输入装配（T-809）：x302 归一化 → [窗 (N,300,1) | aux (N,2)]。

    输入张量按形状识别（末两维 (300,1) = 窗，末维 2 = aux），顺序对齐
    interp.get_input_details()（本模型 IN0=窗 IN1=aux，sha16 门禁锁定）；
    拆分口径 = ems_ctrl R8 归一化后 xnorm[0:300]（升序窗）+ xnorm[300:302]
    （[v,a]），与训练 split_inputs 布局一致（m3 calib report 三口径核对）。
    Xn: (N,302) 归一化特征（float32）。
    """
    win = np.ascontiguousarray(Xn[:, :300].reshape(-1, 300, 1).astype(np.float32))
    aux = np.ascontiguousarray(Xn[:, 300:302].astype(np.float32))
    feeds = []
    for dt in interp.get_input_details():
        shp = tuple(int(v) for v in dt["shape"])
        if shp[-2:] == (300, 1):
            feeds.append(win)
        elif shp[-1:] == (2,):
            feeds.append(aux)
        else:
            raise RuntimeError("TCN tflite 输入形状不符合预期: %s %s"
                               % (dt["name"], shp))
    if len(feeds) != 2:
        raise RuntimeError("TCN tflite 应为双输入，实际 %d" % len(feeds))
    return feeds


def tcnq_eval(interp, Xn):
    """TCN tflite 批量推理（双输入拆分），返回 (N,3) 归一化 q3（tflite_eval 同口径）。"""
    return tflite_eval(interp, tcnq_feed_split(interp, Xn))


def tcnq_identity_check():
    """TCN tflite 身份 sha16 硬门（风格同 run_verify；失败 exit 2）。"""
    m16 = sha16(TCNQ_TFLITE)
    if m16 != TCNQ_SHA16:
        print("ESCALATE: TCN tflite 身份 sha16 不一致 got=%s want=%s"
              % (m16, TCNQ_SHA16))
        sys.exit(2)


def workclf_name_index(interp):
    idx = {}
    for dt in interp.get_input_details():
        if "P_dec30" in dt["name"]:
            idx["P_dec30"] = dt["index"]
        elif "V_accel" in dt["name"]:
            idx["V_accel"] = dt["index"]
    if set(idx.keys()) != {"P_dec30", "V_accel"}:
        raise RuntimeError("workclf input names unexpected: %s" % list(idx))
    return idx


def workclf_eval_feeds(interp, fd_list):
    """fd_list: [P_dec30(N,30,1), V_accel(N,2)]，逐批推理返回 logits (N,7)。"""
    idx = workclf_name_index(interp)
    out_dt = interp.get_output_details()[0]
    n = fd_list[0].shape[0]
    parts = []
    for i in range(0, n, BATCH):
        feeds = [x[i:i + BATCH].astype(np.float32) for x in fd_list]
        nb = feeds[0].shape[0]
        if nb < BATCH:
            feeds = [np.concatenate([f, f[-1:].repeat(BATCH - nb, axis=0)], axis=0)
                     for f in feeds]
        if i == 0:
            interp.resize_tensor_input(idx["P_dec30"], feeds[0].shape)
            interp.resize_tensor_input(idx["V_accel"], feeds[1].shape)
            interp.allocate_tensors()
        interp.set_tensor(idx["P_dec30"], feeds[0])
        interp.set_tensor(idx["V_accel"], feeds[1])
        interp.invoke()
        o = np.asarray(interp.get_tensor(out_dt["index"]))[:nb]
        parts.append(o.reshape(o.shape[0], -1))
    return np.concatenate(parts, axis=0)


def workclf_eval_prep(interp, P30, S2):
    """P30: (N,30) 已按口径重采样的 P 通道；S2: (N,2) 侧输入 [v,accel]。"""
    P = np.ascontiguousarray(P30.astype(np.float32)[:, :, None])
    S = np.ascontiguousarray(S2.astype(np.float32))
    return workclf_eval_feeds(interp, [P, S])



def verify_t307(tp, data):
    """MLP-Q int8 全 val 指标逐项等于冻结 T-307 值，否则 False。"""
    Xv = np.ascontiguousarray(data["X_val"].astype(np.float32))
    interp = make_interp(MLPQ_TFLITE)
    pi = tflite_eval(interp, Xv)
    mq = tp.val_metrics_quantiles(pi, data["y_val"], data["y_mean"], data["y_std"])
    got = {k: round(float(v), 4) for k, v in mq.items()}
    ok = True
    print("T-307 MLP-Q int8 全 val 复现:")
    for k, want in T307_MLPQ_INT8.items():
        got_v = got.get(k)
        eq = (got_v == want)
        ok = ok and eq
        print("  %-16s got=%-10s want=%-10s %s"
              % (k, got_v, want, "OK" if eq else "FAIL"))
    return ok


def verify_t312():
    """workclf int8 全 val（train_workclf.py 归一化数据管线口径）= T-312。"""
    sys.path.insert(0, os.path.join(WS, "python"))
    import train_workclf as tw
    d = np.load(os.path.join(WS, "datasets", "fleet_windows_v2.npz"))
    lab = np.load(os.path.join(WS, "results", "ecms_phase25",
                               "segment_labels", "seg_labels_v2.npz"))
    y_val = lab["seg_labels_val"].astype(np.int64)
    Xv = d["X_val"].astype(np.float32)
    Pn = np.ascontiguousarray(Xv[:, :300:10])
    Sv = np.ascontiguousarray(Xv[:, 300:])
    interp = make_interp(WORKCLF_TFLITE)
    logits = workclf_eval_prep(interp, Pn, Sv)
    pred = logits.argmax(axis=1) + 1
    cm = tw.confmat(y_val, pred)
    _, _, f1 = tw.metrics_from_cm(cm)
    acc = round(float((pred == y_val).mean()), 4)
    macro = round(float(f1.mean()), 4)
    ok = (acc == T312_WORKCLF_INT8["accuracy"]
          and macro == T312_WORKCLF_INT8["macro_f1"])
    print("T-312 workclf int8 全 val 复现: acc=%s/%s macro_f1=%s/%s %s"
          % (acc, T312_WORKCLF_INT8["accuracy"], macro,
             T312_WORKCLF_INT8["macro_f1"], "OK" if ok else "FAIL"))
    return ok


def run_verify():
    """身份 sha + T-307 + T-312 硬门。失败 → §5 格式 ESCALATE，exit 2。"""
    m16 = sha16(MLPQ_TFLITE)
    w16 = sha16(WORKCLF_TFLITE)
    print("sha16: mlpq=%s workclf=%s" % (m16, w16))
    if m16 != MLPQ_SHA16 or w16 != WORKCLF_SHA16:
        print("ESCALATE: 模型身份 sha16 与冻结报告不一致")
        sys.exit(2)
    tp, data = setup_tp_and_data()
    ok307 = verify_t307(tp, data)
    ok312 = verify_t312()
    if not ok307 or not ok312:
        print("=" * 64)
        print("Observed: 冻结指标未复现（见上 FAIL 行）")
        print("Investigated: sha16 身份校验 + t307/t312 同款评估口径（BATCH=%d、"
              "首批 resize、末批补齐）" % BATCH)
        print("Blocker: Host int8 参考层前提不成立；禁止换 runtime/改口径/放宽指标")
        print("Candidates: 1) 数据/模型路径错位 2) 评估口径漂移 3) 部署副本不同源")
        print("Decision Required: 主 Agent 决定修复方向；复现通过前禁止交付模式 A/B")
        print("=" * 64)
        sys.exit(2)
    print("T-307 + T-312 冻结指标全部复现 OK")
    return tp, data


def mode_a_val200(out_dir, model="mlpq"):
    """模式 A：val200 代表向量（f32 原始 + 归一化）+ int8 参考值 CSV。

    T-809：model="tcnq" 时预测器换 TCN tflite（双输入拆分），产物文件名加
    tcnq_ 前缀（与既有 mlpq 产物并存不覆盖）；CSV schema/列序与 mlpq 完全
    一致（Fixed Decisions 10），workclf cls / e_hat 口径不变。
    """
    os.makedirs(out_dir, exist_ok=True)
    tp, data = run_verify()
    tcnq_v2 = (model == "tcnq_v2")
    if model in ("tcnq", "tcnq_v2"):
        if tcnq_v2:
            m16 = sha16(TCNQ_V2_TFLITE)
            if m16 != TCNQ_V2_SHA16:
                print("ESCALATE: TCN v2 tflite sha16 不一致 got=%s want=%s"
                      % (m16, TCNQ_V2_SHA16)); sys.exit(2)
        else:
            tcnq_identity_check()
        print("predictor model: %s (%s)" % (model, os.path.relpath(
            TCNQ_V2_TFLITE if tcnq_v2 else TCNQ_TFLITE, os.path.dirname(TOOLS_DIR))))
    Xv = np.asarray(data["X_val"], dtype=np.float64)
    n_val = int(Xv.shape[0])
    rng = np.random.default_rng(SEED_VAL200)
    idx = np.sort(rng.choice(n_val, N_VAL200, replace=False))
    print("val200: n_val=%d picked=%d idx[0]=%d idx[-1]=%d"
          % (n_val, len(idx), idx[0], idx[-1]))

    x_mean = np.asarray(data["X_mean"], dtype=np.float64)
    x_std = np.asarray(data["X_std"], dtype=np.float64)
    y_mean, y_std = float(data["y_mean"]), float(data["y_std"])
    Xr = Xv[idx] * x_std[None, :] + x_mean[None, :]
    Xs_in = Xv[idx].astype(np.float32)
    Xr32 = Xr.astype(np.float32)

    Xs = Xs_in
    if model == "tcnq":
        qn = tcnq_eval(make_interp(TCNQ_V2_TFLITE if model == "tcnq_v2" else TCNQ_TFLITE), Xs)
    else:
        qn = tflite_eval(make_interp(MLPQ_TFLITE), Xs)
    q_kwh = qn.astype(np.float64) * y_std + y_mean
    # workclf 双口径参考（2026-09 口径修订后固件 = 归一化 = cls_norm；cls_raw
    # 列保留，仅作修订前 raw 口径对照，不删除）。
    logits_raw = workclf_eval_prep(make_interp(WORKCLF_TFLITE),
                                   Xr32[:, 0:300:10], Xr32[:, 300:])
    logits_norm = workclf_eval_prep(make_interp(WORKCLF_TFLITE),
                                    Xs[:, :300:10], Xs[:, 300:])
    cls_raw = logits_raw.argmax(axis=1) + 1
    cls_norm = logits_norm.argmax(axis=1) + 1
    ebar, escale = parse_ks_from_params()
    e_hat = (q_kwh[:, 1] - ebar) / escale

    prefix = "tcnq_" if model == "tcnq" else ""
    raw_p = os.path.join(out_dir, prefix + "vectors_raw.csv")
    norm_p = os.path.join(out_dir, prefix + "vectors_norm.csv")
    ref_p = os.path.join(out_dir, prefix + "reference_int8_val200.csv")
    logits_p = os.path.join(out_dir, prefix + "reference_int8_val200_logits.csv")
    np.savetxt(raw_p, Xr32, fmt="%.9g", delimiter=",")
    np.savetxt(norm_p, Xs, fmt="%.9g", delimiter=",")
    with open(ref_p, "w") as f:
        f.write("idx,q10_kwh,q50_kwh,q90_kwh,e_hat,cls_raw,cls_norm\n")
        for r in range(len(idx)):
            f.write("%d,%.6f,%.6f,%.6f,%.6f,%d,%d\n" % (
                int(idx[r]), q_kwh[r, 0], q_kwh[r, 1], q_kwh[r, 2],
                e_hat[r], cls_raw[r], cls_norm[r]))
    # B2 新增：workclf int8 logits 参考（EMS01 WC_TEST 对拍用，行序 = 向量行序）
    with open(logits_p, "w") as f:
        f.write("idx," + ",".join("logit%d" % j for j in range(7)) + "\n")
        for r in range(len(idx)):
            f.write("%d," % int(idx[r])
                    + ",".join("%.6f" % v for v in logits_raw[r]) + "\n")
    print("wrote %s (%d rows)" % (raw_p, len(idx)))
    print("wrote %s" % norm_p)
    print("wrote %s" % ref_p)
    print("wrote %s (%d rows)" % (logits_p, len(idx)))
    print("q50_kwh: min=%.4f max=%.4f mean=%.4f"
          % (q_kwh[:, 1].min(), q_kwh[:, 1].max(), q_kwh[:, 1].mean()))
    print("cls_raw counts:", counts_of(cls_raw))
    print("cls_norm counts:", counts_of(cls_norm))
    return 0


def counts_of(cls_arr):
    out = {}
    for c in sorted(set(int(v) for v in cls_arr.tolist())):
        out[c] = int((np.asarray(cls_arr) == c).sum())
    return out


class MirrorCtrl:
    """ems_ctrl.c（EML ecms_nn）窗口/门控/迟滞语义的 Python 逐 tick 镜像（NN 层）。

    与 ems_ctrl_step 对齐：lastT=-1e9 起始、t<lastT 平 fill 复位、
    t-lastT<dt-1e-9 held、NaN 跳写+指针无条件前进、accel=v-vprev、
    xP 升序展开、x=[xP,v,accel]、xnorm=(x-XMean)/XStd（R8）。
    """

    def __init__(self, x_mean, x_std, y_mean, y_std, dt=1.0):
        self.x_mean = np.asarray(x_mean, dtype=np.float64)
        self.x_std = np.asarray(x_std, dtype=np.float64)
        self.y_mean, self.y_std = float(y_mean), float(y_std)
        self.dt = dt
        self.last_t = -1e9
        self.k = 0
        self.buf = np.zeros(300)
        self.vprev = 0.0
        self.cls_eff = -1
        self.run_cls = -1
        self.run_cnt = 0

    def reset(self, t, p_load, v_ship):
        self.last_t = t
        self.buf[:] = p_load
        self.vprev = v_ship
        self.cls_eff = -1
        self.run_cls = -1
        self.run_cnt = 0

    def step(self, t, p_load, v_ship):
        """窗口/门控时序推进 + 特征收集（不推理）。"""
        out = dict(held=0, ok=0, cls_eff=self.cls_eff)
        if t < self.last_t:
            self.reset(t, p_load, v_ship)
        if t - self.last_t < self.dt - 1e-9:
            out["held"] = 1
            return out
        self.last_t = t
        if np.isfinite(p_load):
            self.buf[self.k] = p_load
        self.k = (self.k + 1) % 300
        accel = v_ship - self.vprev
        self.vprev = v_ship
        xP = np.empty(300)
        xP[:300 - self.k] = self.buf[self.k:]
        xP[300 - self.k:] = self.buf[:self.k]
        x = np.concatenate([xP, [v_ship, accel]])
        out["xP"] = xP
        out["xnorm"] = (x - self.x_mean) / self.x_std
        out["pdec30"] = xP[::10]
        out["v"] = float(v_ship)
        out["accel"] = float(accel)
        out["ok"] = bool(np.isfinite(x).all())
        return out

    def hysteresis(self, cls):
        if self.cls_eff < 0:
            self.cls_eff = cls
            self.run_cls = cls
            self.run_cnt = 0
        elif cls == self.cls_eff:
            self.run_cls = cls
            self.run_cnt = 0
        elif cls == self.run_cls:
            self.run_cnt += 1
            if self.run_cnt >= GATE_M:
                self.cls_eff = cls
                self.run_cls = cls
                self.run_cnt = 0
        else:
            self.run_cls = cls
            self.run_cnt = 1
        return self.cls_eff


def mode_b_trace(trace_csv, dt_ctrl=1.0):
    """模式 B 阶段 1：纯时序窗口/门控特征收集（不推理）。

    返回 (m, feats, tp, data, ebar, escale, x_mean, x_std, y_mean, y_std)。
    """
    tp, data = run_verify()
    ebar, escale = parse_ks_from_params()
    x_mean = np.asarray(data["X_mean"], dtype=np.float64)
    x_std = np.asarray(data["X_std"], dtype=np.float64)
    y_mean, y_std = float(data["y_mean"]), float(data["y_std"])
    tks = []
    with open(trace_csv, "r") as f:
        rd = csv.DictReader(f)
        cols = rd.fieldnames
        need = {"tick", "t", "P_load_kW", "SOC", "v_ship_mps"}
        if not need.issubset(set(cols or [])):
            raise RuntimeError("trace CSV missing cols %s" % need)
        for r in rd:
            tks.append((int(r["tick"]), float(r["t"]),
                        float(r["P_load_kW"]), float(r["SOC"]),
                        float(r["v_ship_mps"])))
    print("trace: %d ticks" % len(tks))

    m = MirrorCtrl(x_mean, x_std, y_mean, y_std, dt=dt_ctrl)
    feats = []
    for (tick, t, pl, soc, v) in tks:
        feats.append((tick, t, soc, m.step(t, pl, v)))
    return m, feats, data, ebar, escale

def mode_b_assemble(inf, feats, m, qn, logits, y_mean, y_std, ebar, escale):
    out_rows = []
    j = 0
    lastq = None
    for (tick, t, soc, ft) in feats:
        row = dict(tick=tick, t=t, SOC=soc,
                   held=0, ok=0, q10=0.0, q50=0.0, q90=0.0, s_q=0.0,
                   e_hat=0.0, e_hat_raw=0.0, K_q=1.0, K_w=1, K=1.0,
                   cls_raw=0, cls_eff=m.cls_eff, cls_invalid=0)
        if ft["held"]:
            row["held"] = 1
            if lastq is not None:
                row.update(lastq)
                row["held"] = 1
                row["cls_eff"] = m.cls_eff
            out_rows.append(row)
            continue
        if not ft["ok"]:
            row["ok"] = 0
            row["cls_eff"] = m.cls_eff
            row["K"] = 1.0
            out_rows.append(row)
            continue
        qi = inf[j]
        j += 1
        q3n = qn[qi].astype(np.float64)
        q10 = q3n[0] * y_std + y_mean
        q50 = q3n[1] * y_std + y_mean
        q90 = q3n[2] * y_std + y_mean
        s_q = q90 - q10
        if s_q <= GATE_S1_KWH:
            k_q = 1.0
        elif s_q >= GATE_S2_KWH:
            k_q = 0.0
        else:
            k_q = (GATE_S2_KWH - s_q) / (GATE_S2_KWH - GATE_S1_KWH)
        cls = int(logits[qi].argmax()) + 1
        cls_eff = m.hysteresis(cls)
        invalid = 0 if 1 <= cls <= 7 else 1
        k_w = 1 if (invalid or cls_eff != GATE_UPSTREAM_CLS) else 0
        k = float(k_w)
        e_hat_raw = (q50 - ebar) / escale
        e_hat = k * e_hat_raw
        row.update(ok=1, q10=q10, q50=q50, q90=q90, s_q=s_q,
                   e_hat=e_hat, e_hat_raw=e_hat_raw, K_q=k_q,
                   K_w=k_w, K=k, cls_raw=cls, cls_eff=cls_eff,
                   cls_invalid=invalid)
        lastq = dict(ok=1, q10=q10, q50=q50, q90=q90, s_q=s_q,
                     e_hat=e_hat, e_hat_raw=e_hat_raw, K_q=k_q,
                     K_w=k_w, K=k, cls_raw=cls, cls_eff=cls_eff)
        out_rows.append(row)
    return out_rows


def mode_b_write_csv(rows, out_csv):
    with open(out_csv, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["tick", "t", "SOC", "held", "ok", "q10_kwh", "q50_kwh",
                    "q90_kwh", "s_q", "e_hat", "K_q", "K_w", "K",
                    "cls_raw", "cls_eff", "cls_invalid"])
        for r in rows:
            w.writerow([r["tick"], "%.9g" % r["t"], "%.9g" % r["SOC"],
                        r["held"], r["ok"], "%.6f" % r["q10"],
                        "%.6f" % r["q50"], "%.6f" % r["q90"],
                        "%.6f" % r["s_q"], "%.6f" % r["e_hat"],
                        "%.4f" % r["K_q"], r["K_w"], "%.6f" % r["K"],
                        r["cls_raw"], r["cls_eff"], r["cls_invalid"]])
    print("wrote %s (%d rows)" % (out_csv, len(rows)))
    return 0


def mode_b_main(trace_csv, out_csv, dt_ctrl, model="mlpq"):
    """模式 B：特征收集 → 批量 int8 前向 → 迟滞/门控装配 → 写 CSV。

    workclf 输入 = 归一化切片 xnorm[:300:10] + xnorm[300:302]（2026-09 口径修订，
    与 T-312 训练口径 X[:, :300:10]+X[:,300:] 一致；修订前为 raw 口径，见文件头）。
    T-809：model="tcnq" 时预测器换 TCN tflite 双输入拆分（输出 CSV schema 不变）。
    """
    m, feats, data, ebar, escale = mode_b_trace(trace_csv, dt_ctrl=dt_ctrl)
    y_mean, y_std = float(data["y_mean"]), float(data["y_std"])
    inf = [i for i, (_, _, _, ft) in enumerate(feats) if ft["ok"]]
    Xn = np.stack([feats[i][3]["xnorm"] for i in inf]).astype(np.float32)
    # 2026-09 口径修订：workclf 输入 = 同一 xnorm 的切片（与 T-312 训练口径
    # X[:, :300:10]+X[:,300:] 一致；修订前 raw 口径致 int8 饱和坍缩，§22）。
    Pr = np.stack([feats[i][3]["xnorm"][:300:10] for i in inf]).astype(np.float32)
    Sva = np.stack([[feats[i][3]["xnorm"][300], feats[i][3]["xnorm"][301]]
                    for i in inf]).astype(np.float32)
    if model == "tcnq":
        tcnq_identity_check()
        qn = tcnq_eval(make_interp(TCNQ_V2_TFLITE if model == "tcnq_v2" else TCNQ_TFLITE), Xn)
        pred_name = "tcnq"
    else:
        qn = tflite_eval(make_interp(MLPQ_TFLITE), Xn)
        pred_name = "mlpq"
    logits = workclf_eval_prep(make_interp(WORKCLF_TFLITE), Pr, Sva)
    print("int8 batched: %s %s workclf %s" % (pred_name, qn.shape, logits.shape))
    rows = mode_b_assemble(inf, feats, m, qn, logits, y_mean, y_std, ebar, escale)
    mode_b_write_csv(rows, out_csv)
    n_held = sum(1 for r in rows if r["held"])
    print("inference ticks=%d held=%d" % (len(inf), n_held))
    return 0


def main():
    ap = argparse.ArgumentParser(
        description="T-506: Host 侧 int8 参考值生成（冻结口径，见文件头）")
    ap.add_argument("--verify", action="store_true",
                    help="仅执行 T-307/T-312 冻结指标复现硬门后退出")
    ap.add_argument("--sample-val200", metavar="OUT_DIR",
                    help="模式 A：导出 val200 向量 + int8 参考值 CSV")
    ap.add_argument("--trace", metavar="TRACE_CSV", help="模式 B 输入轨迹 CSV")
    ap.add_argument("--out", metavar="OUT_CSV", help="模式 B 输出 CSV")
    ap.add_argument("--dt-ctrl", type=float, default=1.0,
                    help="EMS 控制周期 [s]（默认 1.0 = EMS_dt_ctrl_s）")
    ap.add_argument("--model", choices=("mlpq", "tcnq", "tcnq_v2"), default="mlpq",
                    help="预测器变体（T-809，默认 mlpq 行为不变）：tcnq = TCN "
                         "tflite（Model/tcn_q/tcn_q.tflite）双输入拆分喂解释器，"
                         "输出 schema 与 mlpq 一致；模式 A 产物文件名加 tcnq_ 前缀")
    a = ap.parse_args()
    if not (a.verify or a.sample_val200 or (a.trace and a.out)):
        ap.print_help()
        return 1
    if a.verify:
        run_verify()
        return 0
    if a.sample_val200:
        return mode_a_val200(a.sample_val200, model=a.model)
    return mode_b_main(a.trace, a.out, a.dt_ctrl, model=a.model)


if __name__ == "__main__":
    sys.exit(main())
