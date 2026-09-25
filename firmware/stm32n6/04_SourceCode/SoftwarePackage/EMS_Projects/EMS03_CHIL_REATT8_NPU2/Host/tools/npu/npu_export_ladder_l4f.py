#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""npu_export_ladder_l4f.py — L4 阶梯的 float-LN 部署变体重切（phase4-g，2026-09-24）。

背景：阶梯 dl4 是 int8 Q/DQ LN 变体（97 个 Q/DQ 节点），L4 首分歧=LN 块的阻断
结论只对该变体成立（experiments/npu_rca_2026-09-23/l4_localization/FINDINGS.md）。
服务图 reattn2l 是 npu_bypass_ln_qdq.py 的 float-LN 变体。本工具从
Model/reatt8_q_npu/npu_safe/reattn2n_lnfloat.onnx 按与 dl4 相同的边界重切 L4
（net 名 dl4f）：
  ins = reatt_n2_1/fut3_add2_1/Add:0 , reatt_n2_1/h_tile_1/Tile:0
  out = raw_proj_QuantizeLinear_Input
L4 子图在 attention 之后，内部无 Softmax，因此 BUG-2 的显式轴修正与本级无关
（整图重建时才需要）。

golden：复用 ladder/golden_manifest.json 的窗 0 部署口径输入（kf 归一化 f32 +
reatt_n1_int8 的 h_past，与 int8 链完全相同），ORT 跑 lnfloat 整图取切点张量，
再跑 dl4f 子图得期望输出；同时记录与 int8 链切点值的偏差（文档用）。

用法（仓库根 .venv_tf）：
  python3 npu_export_ladder_l4f.py cut      # ladder/dl4f/dl4f.onnx + golden_l4f.json
  python3 npu_export_ladder_l4f.py emit-c   # 编译完成后重生成 ems_diag_ladder_golden.h
"""
from __future__ import annotations

import glob
import json
import os
import sys

import numpy as np

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)
import npu_export_ladder as base  # noqa: E402  （复用 _ancestors/cut_stage/路径常量）

LAD = base.LAD
APP = base.APP
NPU = base.NPU
SRC_LNF = os.path.join(NPU, "npu_safe", "reattn2n_lnfloat.onnx")
DL4F_DIR = os.path.join(LAD, "dl4f")
GOLDEN_L4F = os.path.join(DL4F_DIR, "golden_l4f.json")
MAN_ORIG = os.path.join(LAD, "golden_manifest.json")

L4F_INS = ["reatt_n2_1/fut3_add2_1/Add:0", "reatt_n2_1/h_tile_1/Tile:0"]
L4F_OUT = "raw_proj_QuantizeLinear_Input"


def _load_inferred(path):
    import onnx
    from onnx import shape_inference
    m = onnx.load(path)
    mi = shape_inference.infer_shapes(m)
    vi = {}
    for v in list(mi.graph.value_info) + list(mi.graph.input) + list(mi.graph.output):
        vi[v.name] = v
    return mi, vi


def cut() -> str:
    import onnx
    mi, vi = _load_inferred(SRC_LNF)
    #  ancestors 自检：不得触及源图输入（kf_input/h_past_input）
    nodes, _inits, extra = base._ancestors(mi.graph, [L4F_OUT], set(L4F_INS))
    if extra:
        raise SystemExit(f"FATAL: L4f 切图触及源图额外输入 {sorted(extra)}")
    ops = {}
    for n in nodes:
        ops[n.op_type] = ops.get(n.op_type, 0) + 1
    print("L4f 子图算子分布:", json.dumps(ops, sort_keys=True))
    if "Softmax" in ops:
        raise SystemExit("FATAL: L4f 子图含 Softmax（预期无；若有需显式轴修正）")
    p = base.cut_stage(mi, vi, "dl4f", L4F_INS, L4F_OUT)
    m = onnx.load(p)
    print("子图:", p)
    print("inputs :", [(i.name, [d.dim_value for d in
                             i.type.tensor_type.shape.dim]) for i in m.graph.input])
    print("outputs:", [(o.name, [d.dim_value for d in
                              o.type.tensor_type.shape.dim]) for o in m.graph.output])
    return p


def golden() -> None:
    import onnx
    from onnx import helper, shape_inference
    import onnxruntime as ort

    man = json.load(open(MAN_ORIG))
    kf = np.asarray(man["inputs"]["kf_input"], np.float32).reshape(1, 30, 6)
    hp = np.asarray(man["inputs"]["h_past_input"], np.float32).reshape(1, 96)

    # lnfloat 整图 + 切点输出
    m = onnx.load(SRC_LNF)
    mi = shape_inference.infer_shapes(m)
    vi = {v.name: v for v in list(mi.graph.value_info) + list(mi.graph.input)}
    for t in L4F_INS + [L4F_OUT]:
        v = vi[t]
        m.graph.output.append(helper.make_tensor_value_info(
            t, v.type.tensor_type.elem_type,
            [d.dim_value for d in v.type.tensor_type.shape.dim]))
    m.ir_version = 8
    tmp = os.path.join(DL4F_DIR, "_lnfloat_with_cuts.onnx")
    onnx.save(m, tmp)
    s_full = ort.InferenceSession(tmp, providers=["CPUExecutionProvider"])
    names = [o.name for o in s_full.get_outputs()]
    outs = s_full.run(None, {"kf_input": kf, "h_past_input": hp})
    vals = dict(zip(names, [np.asarray(o, np.float32) for o in outs]))

    feed = {t: vals[t] for t in L4F_INS}
    s_sub = ort.InferenceSession(os.path.join(DL4F_DIR, "dl4f.onnx"),
                                 providers=["CPUExecutionProvider"])
    out_sub = np.asarray(s_sub.run(None, feed)[0], np.float32)
    out_full = np.asarray(vals[L4F_OUT], np.float32)
    selfcheck = float(np.max(np.abs(out_sub - out_full)))

    # 与 int8 链切点值的偏差（文档用；int8 链 golden 取自原 manifest）
    l4o = man["stages"]["l4"]
    d_in0 = float(np.max(np.abs(
        feed[L4F_INS[0]].reshape(-1) - np.asarray(l4o["ins"][L4F_INS[0]], np.float32))))
    d_in1 = float(np.max(np.abs(
        feed[L4F_INS[1]].reshape(-1) - np.asarray(l4o["ins"][L4F_INS[1]], np.float32))))
    d_out = float(np.max(np.abs(
        out_full.reshape(-1) - np.asarray(l4o["out"], np.float32))))

    doc = {
        "window": man["window"],
        "source": os.path.relpath(SRC_LNF, base.REPO_ROOT),
        "ins": {t: vals[t].reshape(-1).tolist() for t in L4F_INS},
        "out": out_full.reshape(-1).tolist(),
        "out_shape": list(out_full.shape),
        "selfcheck_sub_vs_full_maxd": selfcheck,
        "vs_int8_chain": {"in0_maxd": d_in0, "in1_maxd": d_in1, "out_maxd": d_out},
    }
    with open(GOLDEN_L4F, "w") as fh:
        json.dump(doc, fh)
    print(f"golden_l4f: selfcheck(子图vs整图)={selfcheck:.3g}")
    print(f"lnfloat 链 vs int8 链切点偏差: in0={d_in0:.4g} in1={d_in1:.4g} out={d_out:.4g}")
    print("写出:", GOLDEN_L4F)


def _c_arr(name, arr):
    import math
    vals = [float(v) for v in arr]
    for v in vals:
        if not math.isfinite(v):
            raise SystemExit(f"FATAL: golden {name} 含非有限值 {v}")
    parts = [f"{v:.9g}" for v in vals]
    rows = [",".join(parts[i:i + 8]) for i in range(0, len(parts), 8)]
    return ("static const float " + name + "[] = {\n"
            + ",\n".join(rows) + "\n};")


def _bounds(net):
    """读编译头 + Q.json → (n_in, out_bytes, bounds)；未编译返回 None。"""
    import re
    hdrs = [os.path.join(LAD, net, f"{net}.h"),
            os.path.join(NPU, "npu_safe", "dl1s", f"{net}.h")]
    hdr = next((h for h in hdrs if os.path.isfile(h)), None)
    if hdr is None:
        return None
    txt = open(hdr).read()

    def macro(rx):
        mm = re.search(rx + r"\s+\(([^)]*)\)", txt)
        return mm.group(1).strip()
    pre = "LL_ATON_" + net.upper()
    n_in = int(macro("#define " + pre + "_IN_NUM"))
    n_out = int(macro("#define " + pre + "_OUT_1_SIZE_BYTES"))
    qcands = sorted(glob.glob(os.path.join(LAD, net, "*_Q.json"))) + \
        sorted(glob.glob(os.path.join(LAD, net, "st_ai_output", "*_Q.json"))) + \
        sorted(glob.glob(os.path.join(NPU, "npu_safe", "dl1s", "*_Q.json")))
    if not qcands:
        return None
    q = json.load(open(qcands[0]))
    bounds = {k: v["format"] for k, v in q["tensors"].items()
              if v.get("user_allocated")}
    return (n_in, n_out, bounds)


def emit_c() -> None:
    """重生成整份 golden 头：l1/l2/l3/f1 取自原 manifest（值不变），
    l4 三组数组（dl_l4_in0/in1/exp）替换为 lnfloat 链的 dl4f golden。"""
    man = json.load(open(MAN_ORIG))
    g4 = json.load(open(GOLDEN_L4F))
    lines_h = [
        "/* ems_diag_ladder_golden.h — 由 npu_export_ladder.py emit-c 生成（禁手改）。",
        " * 2026-09-24: L4 级改 dl4f（float-LN 部署变体，npu_export_ladder_l4f.py），",
        " * dl_l4_* 三组数组为 lnfloat 链窗 0 golden；其余各级数组不变。",
        " * 阶梯 golden 输入/期望输出 + 边界契约；供 ems_diag_ladder.c 运行时自比对。 */",
        "#ifndef EMS_DIAG_LADDER_GOLDEN_H",
        "#define EMS_DIAG_LADDER_GOLDEN_H",
        "#include <stdint.h>", ""]
    rows = []

    def add_stage(stage, net, ins_names, ins_lists, out_list):
        b = _bounds(net)
        if b is None:
            print(f"  [skip] {stage}: 未编译（缺 {net}.h/Q.json）")
            return
        for j, arr in enumerate(ins_lists):
            lines_h.append(_c_arr(f"dl_{stage}_in{j}", arr))
        lines_h.append(_c_arr(f"dl_{stage}_exp", out_list))
        lines_h.append("")
        rows.append({"stage": stage, "net": net, "n_in": b[0],
                     "out_bytes": b[1], "bounds": b[2]})

    for stage in ("l1", "l2", "l3"):
        cfg = base.STAGES[stage]
        s = man["stages"][stage]
        add_stage(stage, cfg["net"], cfg["ins"],
                  [s["ins"][t] for t in cfg["ins"]], s["out"])
    # L4 → dl4f（float-LN 变体）
    add_stage("l4", "dl4f", L4F_INS,
              [g4["ins"][t] for t in L4F_INS], g4["out"])
    # F1 整图（不变）
    add_stage("f1", "dlf1", ["kf_input", "h_past_input"],
              [man["inputs"]["kf_input"], man["inputs"]["h_past_input"]],
              man["full_out"])

    lines_h.append("#endif /* EMS_DIAG_LADDER_GOLDEN_H */")
    with open(os.path.join(APP, "ems_diag_ladder_golden.h"), "w") as fh:
        fh.write("\n".join(lines_h))
    with open(os.path.join(LAD, "ladder_manifest.json"), "w") as fh:
        json.dump(rows, fh, indent=2)
    print(f"emit-c 完成：{len(rows)} 级 → Appli/App/ems_diag_ladder_golden.h")
    for r in rows:
        print("  ", r["stage"], r["net"], "n_in=", r["n_in"],
              "out_bytes=", r["out_bytes"])


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else "cut"
    os.makedirs(DL4F_DIR, exist_ok=True)
    if cmd == "cut":
        cut()
        golden()
    elif cmd == "golden":
        golden()
    elif cmd == "emit-c":
        emit_c()
    else:
        raise SystemExit(f"未知子命令 {cmd}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
