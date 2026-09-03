# -*- coding: utf-8 -*-
"""t312_workclf_int8.py — workclf 真 int8 量化重导出（T-308 缺陷修复，EMS01 CHIL 前置）

缺陷（2026-08-31 主 Agent 核实）：历史 `workclf_v1_int8.tflite`
（sha256 3dd5a031…d739，已改名 `workclf_v1_float.tflite` 留证）实为 float32，
根因是 train_workclf.py 导出段裸 `TFLiteConverter.convert()`（无量化参数）。
T-307 的 int8 量化只覆盖负荷预测候选，从未覆盖 workclf 分类器。

本脚本（--reuse 语义：权重固定，禁止重训练）复刻 T-307 量化口径：
  - 源模型   results/ecms_phase25/classifier/workclf_v1.keras（只读）
  - 数据管线 与 train_workclf.py 完全一致（同一 fleet_windows_v2.npz + 同一 prep()）
  - 校准集   val 前 1000 样本（仅校准，与 T-307 口径一致；严禁 test）
  - 转换     Optimize.DEFAULT + representative_dataset（失败时 T-307 同款
             SELECT_TF_OPS fallback 并记录）；QDQ 形态（float I/O + 内部
             量化节点，与 mlp_q int8 对照）
  - 输出     results/ecms_phase25/classifier/workclf_v1_int8.tflite（真 int8）
             results/ecms_phase25/classifier/workclf_int8_report.json

评估（AI_MODELING.md §15 强制）：float(keras) vs int8(tflite) 在 val 上的
准确率/混淆矩阵 + delta；test 仅作评估记录，不据此调任何东西。
"""
import os, sys, json, time, hashlib, subprocess
os.environ.setdefault("TF_CPP_MIN_LOG_LEVEL", "3")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import tensorflow as tf
import train_workclf as tw  # confmat / metrics_from_cm：指标定义单一来源

BASE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(BASE, "..")
OUT = os.path.join(ROOT, "results", "ecms_phase25", "classifier")
KERAS_PATH = os.path.join(OUT, "workclf_v1.keras")
TFLITE_PATH = os.path.join(OUT, "workclf_v1_int8.tflite")
FLOAT_EVIDENCE_PATH = os.path.join(OUT, "workclf_v1_float.tflite")
REPORT_PATH = os.path.join(OUT, "workclf_int8_report.json")
MLPQ_TFLITE = os.path.join(ROOT, "results", "ecms_phase25", "models",
                           "mlp_h64_32_lr3e-4_q", "mlp_h64_32_lr3e-4_q_int8.tflite")
N_REP = 1000          # 代表性样本数（int8 校准，val 前 N_REP）
BATCH = 512
DEFECT_SHA256 = "3dd5a031d4d52f6a680fd0f60fef043d8f232c38b4c9f84b024493fc5f6fd739"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def git_sha():
    try:
        return subprocess.check_output(["git", "rev-parse", "HEAD"],
                                       cwd=BASE, text=True).strip()
    except Exception:
        return "unknown"


def tflite_eval(interp, P, S):
    """逐批解释器推理（双输入按 name 匹配，避免 flatbuffer 输入顺序差异）。"""
    inputs = {("P_dec30" if "P_dec30" in d["name"] else "V_accel"): d["index"]
              for d in interp.get_input_details()}
    out = interp.get_output_details()[0]
    n = P.shape[0]
    outs = []
    for i in range(0, n, BATCH):
        feeds = {"P_dec30": P[i:i + BATCH].astype(np.float32),
                 "V_accel": S[i:i + BATCH].astype(np.float32)}
        nb = feeds["P_dec30"].shape[0]
        if nb < BATCH:  # 末批补齐到固定形状，输出后裁剪（T-307 同款）
            pad = BATCH - nb
            feeds = {k: np.concatenate([f, f[-1:].repeat(pad, axis=0)], axis=0)
                     for k, f in feeds.items()}
        if i == 0:
            for d in interp.get_input_details():
                key = "P_dec30" if "P_dec30" in d["name"] else "V_accel"
                interp.resize_tensor_input(d["index"], feeds[key].shape)
            interp.allocate_tensors()
        for key, idx in inputs.items():
            interp.set_tensor(idx, feeds[key])
        interp.invoke()
        outs.append(interp.get_tensor(out["index"])[:nb])
    return np.concatenate(outs, axis=0)


def tflite_structure(path):
    """解释器结构证据：输入/输出 dtype + 量化参数 + 权重 dtype 分布。"""
    interp = tf.lite.Interpreter(model_path=path)
    interp.allocate_tensors()
    def io(d):
        return {"name": d["name"], "dtype": d["dtype"].__name__,
                "shape": [int(x) for x in d["shape"]],
                "quantization": {"scale": float(d["quantization"][0]),
                                 "zero_point": int(d["quantization"][1])}}
    tensors = interp.get_tensor_details()
    int8_t = [t for t in tensors if t["dtype"] == np.int8]
    # 权重核 = 常量 kernel（Conv1D lowering 的 *convolution、全连接的
    # *MatMul/tfl.pseudo_qconst），无 batch 维；激活量化张量均带 batch=1 首维，
    # 携带真实 scale/zero_point（即校准参数）。逐通道权重的标量 quantization
    # 字段在解释器显示哨兵值 0.0，逐通道 scale 存于 flatbuffer。
    def is_weight(t):
        return t["shape"][0] != 1
    def q(t):
        return {"scale": [round(float(s), 6) for s in np.atleast_1d(t["quantization"][0])[:4]],
                "zero_point": [int(z) for z in np.atleast_1d(t["quantization"][1])[:4]]}
    weights = [{"name": t["name"], "shape": [int(x) for x in t["shape"]]}
               for t in int8_t if is_weight(t)]
    acts = [{"name": t["name"], "shape": [int(x) for x in t["shape"]],
             "quantization": q(t)}
            for t in int8_t if not is_weight(t)]
    hist = {}
    for t in tensors:
        hist[t["dtype"].__name__] = hist.get(t["dtype"].__name__, 0) + 1
    return {"inputs": [io(d) for d in interp.get_input_details()],
            "outputs": [io(d) for d in interp.get_output_details()],
            "tensor_dtype_histogram": hist,
            "int8_weight_kernels": weights,
            "int8_activation_quantizers": acts,
            "note": "权重核为逐通道 int8（解释器对逐通道常量标量 quantization 显示哨兵"
                    "值 0.0，逐通道 scale 存于 flatbuffer）；输入量化节点 scale 即校准参数"}


def main():
    t0 = time.time()
    assert os.path.isfile(KERAS_PATH), "missing " + KERAS_PATH
    # 缺陷证据文件必须在场（历史误标文件改名留证，不得静默丢失）
    evidence_sha = None
    if os.path.isfile(FLOAT_EVIDENCE_PATH):
        evidence_sha = sha256(FLOAT_EVIDENCE_PATH)
    else:
        raise SystemExit("DEFECT-EVIDENCE-MISSING: %s 不存在。"
                         "历史误标文件必须先改名为 workclf_v1_float.tflite 留证，"
                         "禁止静默覆盖历史。" % FLOAT_EVIDENCE_PATH)

    # 数据管线与 train_workclf.py 完全一致（同一 npz + 同一 prep()）
    d = np.load(os.path.join(ROOT, "datasets", "fleet_windows_v2.npz"))
    lab = np.load(os.path.join(ROOT, "results", "ecms_phase25", "segment_labels",
                               "seg_labels_v2.npz"))
    seg_names = [str(s) for s in lab["seg_names"]]

    def prep(split):
        X = d[f"X_{split}"]
        P = np.ascontiguousarray(X[:, :300:10][..., None].astype(np.float32))
        S = np.ascontiguousarray(X[:, 300:].astype(np.float32))
        return P, S, lab[f"seg_labels_{split}"]

    Pva, Sva, yva = prep("val")
    Pte, Ste, yte = prep("test")
    print(f"data ready {time.time()-t0:.0f}s | val {Pva.shape} test {Pte.shape}",
          flush=True)

    # 源模型（--reuse 语义：权重固定，禁止重训练）
    model = tf.keras.models.load_model(KERAS_PATH, safe_mode=False, compile=False)
    n_params = int(model.count_params())
    print("reused workclf_v1.keras, params:", n_params, flush=True)

    # 代表性数据集：val 前 N_REP 样本（仅校准；与 T-307 口径一致；严禁 test）
    # 顺序 = 本模型转换器校准入口顺序 [V_accel(2), P_dec30(30,1)]。该顺序由形状
    # 决定性地验证：[P_dec30, V_accel] 顺序（T-307 对 TCN 的写法）在本模型上触发
    # calibrator PAD rank 错误（rank-3 张量喂进 rank-2 入口位）转换失败，
    # [V_accel, P_dec30] 顺序转换成功——转换成功本身即顺序正确性的机器判据，
    # 非按指标挑选。解释器评估侧仍按输入 name 匹配，不受 flatbuffer 顺序影响。
    def rep_gen():
        for i in range(min(N_REP, Pva.shape[0])):
            yield [Sva[i:i + 1], Pva[i:i + 1]]

    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = rep_gen
    fallback = False
    try:
        tfl = conv.convert()
    except Exception as e:
        print("DEFAULT convert failed (%s) -> T-307 同款 SELECT_TF_OPS fallback"
              % type(e).__name__, flush=True)
        conv2 = tf.lite.TFLiteConverter.from_keras_model(model)
        conv2.optimizations = [tf.lite.Optimize.DEFAULT]
        conv2.representative_dataset = rep_gen
        conv2.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS,
                                           tf.lite.OpsSet.SELECT_TF_OPS]
        conv2._experimental_lower_tensor_list_ops = False
        tfl = conv2.convert()
        fallback = True
    open(TFLITE_PATH, "wb").write(tfl)
    print("int8 tflite written: %d bytes" % len(tfl), flush=True)

    # 结构验证：输入 dtype/量化参数 + 卷积权重 int8（QDQ 形态，对照 mlp_q）
    st_workclf = tflite_structure(TFLITE_PATH)
    st_mlpq = tflite_structure(MLPQ_TFLITE)
    n_int8 = len(st_workclf["int8_weight_kernels"]) + len(st_workclf["int8_activation_quantizers"])
    print("structure: int8 tensors=%d hist=%s | mlp_q int8 tensors=%d"
          % (n_int8, st_workclf["tensor_dtype_histogram"],
             len(st_mlpq["int8_weight_kernels"]) + len(st_mlpq["int8_activation_quantizers"])),
          flush=True)

    # float（keras）vs int8（tflite 解释器）对照；test 仅评估记录
    def cls_metrics(y, pr):
        cm = tw.confmat(y, pr)
        _, _, f1 = tw.metrics_from_cm(cm)
        return {"accuracy": round(float((y == pr).mean()), 4),
                "macro_f1": round(float(f1.mean()), 4),
                "confusion_matrix": cm.tolist(),
                "per_class_recall": {seg_names[i]: round(
                    float(cm[i, i] / max(cm[i, :].sum(), 1)), 4) for i in range(7)}}

    def int8_pred(P, S):
        interp = tf.lite.Interpreter(model_path=TFLITE_PATH, num_threads=4)
        return tflite_eval(interp, P, S).argmax(1) + 1

    result = {}
    for split, (P, S, y) in (("val", (Pva, Sva, yva)), ("test", (Pte, Ste, yte))):
        pf = model.predict([P, S], batch_size=2048, verbose=0).argmax(1) + 1
        pi = int8_pred(P, S)
        result[split] = {"float": cls_metrics(y, pf), "int8": cls_metrics(y, pi)}
        print("%s float acc=%.4f | int8 acc=%.4f"
              % (split, result[split]["float"]["accuracy"],
                 result[split]["int8"]["accuracy"]), flush=True)

    dval = round(100 * (result["val"]["int8"]["accuracy"]
                        - result["val"]["float"]["accuracy"]), 2)
    dtest = round(100 * (result["test"]["int8"]["accuracy"]
                         - result["test"]["float"]["accuracy"]), 2)
    print("delta(val)=%+.2fpp delta(test)=%+.2fpp" % (dval, dtest), flush=True)
    if dval < -2.0:
        print("ESCALATE: val int8 准确率劣化 >2pp，禁止自行改量化方案救指标", flush=True)

    report = {
        "task": "workclf 真 int8 量化重导出（T-308 缺陷修复，EMS01 CHIL ECMS v1 前置）",
        "script": "python/t312_workclf_int8.py",
        "generated": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "git_commit_sha": git_sha(),
        "reuse_semantics": "workclf_v1.keras 权重固定仅重导出，未重训练（--reuse 语义）",
        "defect_record": {
            "was_named": "workclf_v1_int8.tflite",
            "now_named": "workclf_v1_float.tflite",
            "sha256": DEFECT_SHA256,
            "evidence_file_sha256": evidence_sha,
            "evidence_sha_matches": bool(evidence_sha == DEFECT_SHA256),
            "actual_dtype": "float32（解释器输入/输出 float32，quantization 全零）",
            "size_kb": round(os.path.getsize(FLOAT_EVIDENCE_PATH) / 1024, 1),
            "root_cause": "train_workclf.py 导出段裸 TFLiteConverter.from_keras_model("
                          "model).convert() 无任何量化参数；T-307 int8 量化只覆盖负荷"
                          "预测候选，从未覆盖 workclf 分类器",
            "disposition": "误标文件改名留证；train_workclf.py 裸 convert 输出已改名 "
                           "workclf_v1_float.tflite，真 int8 统一由本脚本产出",
        },
        "model": {"keras_path": "results/ecms_phase25/classifier/workclf_v1.keras",
                  "keras_sha256": sha256(KERAS_PATH), "params": n_params},
        "quant_config": {
            "method": "复刻 T-307（t307_int8.py）口径",
            "optimizations": ["tf.lite.Optimize.DEFAULT"],
            "representative_dataset": "fleet_windows_v2.npz val 前 %d 样本（仅校准，"
                                      "与 T-307 口径一致；严禁 test）" % min(N_REP, Pva.shape[0]),
            "rep_order": "[V_accel(2), P_dec30(30,1)] = 本模型转换器校准入口顺序；"
                         "形状决定性验证：[P_dec30, V_accel] 顺序触发 calibrator PAD "
                         "rank 错误转换失败，[V_accel, P_dec30] 顺序成功（机器判据，"
                         "非按指标挑选）；解释器评估按输入 name 匹配",
            "form": "QDQ：float I/O + 内部量化节点（与 mlp_q int8 对照）",
            "fallback_select_tf_ops": fallback,
            "output_dtype_note": "未设 inference_input_type/int8 输出，保持 QDQ float I/O",
        },
        "output": {"path": "results/ecms_phase25/classifier/workclf_v1_int8.tflite",
                   "sha256": sha256(TFLITE_PATH)},
        "size": {"keras_fp32_kb": round(os.path.getsize(KERAS_PATH) / 1024, 1),
                 "old_mislabeled_float_kb": round(os.path.getsize(FLOAT_EVIDENCE_PATH) / 1024, 1),
                 "int8_kb": round(len(tfl) / 1024, 1)},
        "float_vs_int8": result,
        "delta": {"val_acc_pp": dval, "test_acc_pp": dtest},
        "structure": {"workclf_int8": st_workclf, "mlp_q_int8_reference": st_mlpq,
                      "note": "workclf Conv1D 权重应与 mlp_q 同为 QDQ 形态：float I/O + "
                              "int8 权重张量；Conv1D SW fallback 属预期内（design.md R2）"},
        "compliance": {
            "test_usage": "test 仅评估记录（与 T-308/T-307 既有口径同性质），"
                          "未参与训练/校准/调参/量化配置选择",
            "val_usage": "val 前 1000 样本仅作校准集 + float/int8 对照评估",
            "no_retrain": True, "no_metric_redefinition": True,
        },
    }
    json.dump(report, open(REPORT_PATH, "w", encoding="utf-8"),
              ensure_ascii=False, indent=2)
    print("REPORT ->", REPORT_PATH, flush=True)
    print("TOTAL %.0f s" % (time.time() - t0), flush=True)


if __name__ == "__main__":
    main()
