#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""npu_export_ladder.py — N2 逐级 bisection 阶梯（phase4-g，2026-09-23 夜备）。

背景：NPU 通路已被平凡自检图 + workclf(40/40) 证明正常；REATT N2 图在板上输出
退化为"与输入无关的常数三元组"。本工具把 N2 切成 4 张阶梯子图 + 1 个选项消融
变体，全部编译为**独立命名**的 NPU 图（--name reattlX，符号不撞），装进同一个
固件、一次烧录、一次上电全部跑完，板上逐级与 ORT golden 自比对并给出单行判决。

阶梯（切点全部为源图 float 张量，编译后边界类型以生成物为准、固件运行时适配）：
  L1  kf_input → lead_pos                 （kf 输入通路：Transpose/DQ/Reshape/Dense/位置加）
  L2  mha_in → mha_out                    （一整块 attention：Q/K/V、QKᵀ、scale、Softmax、AV、o_proj、残差）
  L3  h_past_input → h_tile               （h_past 的 Unsqueeze/Reshape/Q/DQ/Slice/Tile —— 头号嫌疑：
                                          板上"30 lead 全同"指纹最像 Tile/融合把未来分支贡献丢失）
  L4  {fut_seq, h_tile} → raw             （fusion+LN+Dense+decoder 头部）
  F1  整图重编译，选项加 --Ono-clone-dma   （clone_dma 为 4.0.0 新默认，可关——升级致坏类头号消融）
  S0  既有 reattn2n 喂 golden 输入         （无需重编；隔离"N1 的 h_past 是否毒化输入"）

golden：ORT 在**整图**上以部署口径输入（kf 归一化 f32 + reatt_n1_int8 的 h_past）
一次性取出全部切点张量值；每级再用 ORT 跑**该级子图**得到 golden 输出。

用法（仓库根 .venv_tf）：
  python3 npu_export_ladder.py cut      # 生成 Model/reatt8_q_npu/ladder/<stage>/…onnx + golden.npz
  python3 npu_export_ladder.py emit-c   # 编译完成后读生成头/Q.json → Appli/App/ems_diag_ladder_golden.h
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np


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


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = find_repo_root(SCRIPT_DIR)
PROJ = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", ".."))
NPU = os.path.join(PROJ, "Model", "reatt8_q_npu")
LAD = os.path.join(NPU, "ladder")
SRC = os.path.join(NPU, "reatt_n2_int8.onnx")
REF = os.path.join(PROJ, "Host", "reference", "reatt", "reatt_ref_vectors_split.npz")
APP = os.path.join(PROJ, "Appli", "App")

WIN = 0  # golden 用的参考窗号

# net = 编译网络名（--name），宏前缀 LL_ATON_<NET 大写>_
STAGES = {
    "l1": {"net": "dl1", "ins": ["kf_input"], "out": "reatt_n2_1/lead_pos_1/add:0"},
    "l2": {"net": "dl2", "ins": ["reatt_n2_1/fut0_ln1_1/add_2:0_DequantizeLinear_Output"],
           "out": "reatt_n2_1/fut0_mha_1/o_1/BiasAdd:0_DequantizeLinear_Output"},
    "l3": {"net": "dl3", "ins": ["h_past_input"], "out": "reatt_n2_1/h_tile_1/Tile:0"},
    "fut": {"net": "dlfut", "ins": ["kf_input"],
            "out": "reatt_n2_1/fut3_add2_1/Add:0"},
    "l4": {"net": "dl4", "ins": ["reatt_n2_1/fut3_add2_1/Add:0", "reatt_n2_1/h_tile_1/Tile:0"],
           "out": "raw_proj_QuantizeLinear_Input"},
}
# F1 = 整图（输入/输出 = 原图 I/O），消融选项在编译脚本里


def _load_inferred():
    import onnx
    from onnx import shape_inference
    m = onnx.load(SRC)
    mi = shape_inference.infer_shapes(m)
    vi = {}
    for v in list(mi.graph.value_info) + list(mi.graph.input) + list(mi.graph.output):
        vi[v.name] = v
    return mi, vi


def _ancestors(graph, outs, ins):
    """从 outs 反向收集节点；碰到 ins（切点输入）/源图输入/initializer 即停。

    注意：protobuf 容器每次访问产生新 Python 对象，id() 不稳定——必须用节点
    下标做集合成员，并按原图顺序输出（原图序 = 拓扑序）。
    返回 (nodes, inits, extra_ins)：extra_ins = 遍历中碰到的源图原始输入
    （如 kf_input/h_past_input），必须一并声明为子图输入。"""
    inits = {i.name for i in graph.initializer}
    src_in = {i.name for i in graph.input}
    prod = {}
    for idx, n in enumerate(graph.node):
        for o in n.output:
            prod[o] = idx
    seen = set()
    extra = set()
    stack = list(outs)
    while stack:
        t = stack.pop()
        if t in inits:
            continue
        if t in ins:
            continue
        if t in src_in:
            extra.add(t)
            continue
        idx = prod.get(t)
        if idx is None or idx in seen:
            continue
        seen.add(idx)
        for i in graph.node[idx].input:
            stack.append(i)
    return [graph.node[i] for i in sorted(seen)], inits, extra


def cut_stage(mi, vi, net, ins, out):
    """构造 [ins] → [out] 子图。"""
    import onnx
    from onnx import TensorProto, helper, numpy_helper

    nodes, inits, extra = _ancestors(mi.graph, [out], set(ins))
    ins = list(ins) + sorted(extra)   # 遍历碰到的源图输入并入子图输入
    used_inits, seen_init = [], set()
    for n in nodes:
        for i in n.input:
            if i in inits and i not in seen_init:
                seen_init.add(i)
    init_map = {i.name: i for i in mi.graph.initializer}
    used_inits = [init_map[i] for i in seen_init]

    def vinfo(t):
        v = vi[t]
        return helper.make_tensor_value_info(
            t, v.type.tensor_type.elem_type,
            [d.dim_value for d in v.type.tensor_type.shape.dim])

    g = helper.make_graph(
        nodes, f"reatt_ladder_{net}",
        [vinfo(t) for t in ins],
        [vinfo(out)],
        initializer=used_inits)
    m = helper.make_model(g, opset_imports=[helper.make_opsetid("", 17)],
                          producer_name="phase4g-ladder")
    m.ir_version = 8
    onnx.checker.check_model(m)
    od = os.path.join(LAD, net)
    os.makedirs(od, exist_ok=True)
    p = os.path.join(od, f"{net}.onnx")
    onnx.save(m, p)
    return p


def golden_all():
    """整图一次取全部切点张量值 → 每级 ORT 子图 golden 输入/输出。"""
    import onnx
    import onnxruntime as ort

    m = onnx.load(SRC)
    cut_tensors = sorted({t for s in STAGES.values() for t in s["ins"]} |
                         {s["out"] for s in STAGES.values()})
    # 给整图追加切点输出
    from onnx import shape_inference, helper
    mi = shape_inference.infer_shapes(m)
    vi = {v.name: v for v in list(mi.graph.value_info) + list(mi.graph.input)}
    for t in cut_tensors:
        if t in ("kf_input", "h_past_input"):
            continue
        v = vi[t]
        m.graph.output.append(helper.make_tensor_value_info(
            t, v.type.tensor_type.elem_type,
            [d.dim_value for d in v.type.tensor_type.shape.dim]))
    m.ir_version = 8
    onnx.save(m, os.path.join(LAD, "_full_with_cuts.onnx"))
    s_full = ort.InferenceSession(os.path.join(LAD, "_full_with_cuts.onnx"),
                                  providers=["CPUExecutionProvider"])

    # 部署口径输入：kf 归一化 f32 + N1(int8 CPU 形态) 的 h_past
    zs = np.load(REF)
    hist_raw, kf_raw = zs["hist_raw"], zs["kf_raw"]
    sys.path.insert(0, os.path.join(REPO_ROOT, "model",
                                    "simulink_modelE_(Phase4_Physics)"))
    from predictor_v3 import features, norm_ext
    fn = features.load_formal_norm()
    ne = norm_ext.load_artifact()
    order = features.registry_channel_order()
    kf = np.zeros((1, 30, 6), np.float32)
    for j, c in enumerate(order["kf"]):
        kf[0, :, j] = norm_ext.transform_channel(ne, c,
                                                 kf_raw[WIN, :, j].astype(np.float64))
    s1 = ort.InferenceSession(os.path.join(NPU, "reatt_n1_int8.onnx"),
                              providers=["CPUExecutionProvider"])
    hn = np.zeros((1, 300, 11), np.float32)
    for j, ch in enumerate(order["h1"]):
        mu = np.asarray(fn["x_h1_mean"][ch], np.float64)
        sd = np.asarray(fn["x_h1_std"][ch], np.float64)
        hn[0, :, j] = ((hist_raw[WIN, :, j].astype(np.float64) - mu) / sd).astype(np.float32)
    hp = np.asarray(s1.run(None, {"hist_input": hn})[0], np.float32).reshape(1, 96)

    names = [o.name for o in s_full.get_outputs()]
    outs = s_full.run(None, {"kf_input": kf, "h_past_input": hp})
    vals = dict(zip(names, [np.asarray(o) for o in outs]))
    vals["kf_input"] = kf          # 原图输入的 golden（切点遍历会用到）
    vals["h_past_input"] = hp

    manifest = {"window": WIN,
                "inputs": {"kf_input": kf.reshape(-1).tolist(),
                           "h_past_input": hp.reshape(-1).tolist()},
                "stages": {}}
    for name, cfg in STAGES.items():
        net = cfg["net"]
        p = os.path.join(LAD, net, f"{net}.onnx")
        ss = ort.InferenceSession(p, providers=["CPUExecutionProvider"])
        feed = {t: vals[t].astype(np.float32) for t in cfg["ins"]}
        out = np.asarray(ss.run(None, feed)[0], np.float32)
        manifest["stages"][name] = {
            "net": net,
            "ins": {t: vals[t].astype(np.float32).reshape(-1).tolist()
                    for t in cfg["ins"]},
            "out": out.reshape(-1).tolist(),
            "out_shape": list(out.shape),
        }
    # 整图 golden（S0/F1 用）
    manifest["full_out"] = np.asarray(vals["raw_proj_QuantizeLinear_Input"],
                                      np.float32).reshape(-1).tolist()
    with open(os.path.join(LAD, "golden_manifest.json"), "w") as fh:
        json.dump(manifest, fh)
    np.savez(os.path.join(LAD, "golden.npz"),
             kf=kf, hp=hp,
             **{f"{n}_out": np.asarray(s["out"], np.float32)
                for n, s in manifest["stages"].items()},
             full_out=np.asarray(manifest["full_out"], np.float32))
    print("golden 完成:", os.path.join(LAD, "golden_manifest.json"))


def emit_c():
    """编译完成后：读各生成头 + Q.json → 固件 golden/判决数据 C 文件。"""
    et = {1: "FLOAT32", 2: "INT8", 3: "INT8"}
    rows = []
    lines_h = ["/* ems_diag_ladder_golden.h — 由 npu_export_ladder.py emit-c 生成（禁手改）。",
               " * 阶梯 golden 输入/期望输出 + 边界契约；供 ems_diag_ladder.c 运行时自比对。 */",
               "#ifndef EMS_DIAG_LADDER_GOLDEN_H",
               "#define EMS_DIAG_LADDER_GOLDEN_H",
               "#include <stdint.h>", ""]
    man = json.load(open(os.path.join(LAD, "golden_manifest.json")))

    def c_arr(name, arr, dtype="float"):
        import math
        vals = [float(v) for v in arr]
        for v in vals:
            if not math.isfinite(v):
                raise SystemExit(f"FATAL: golden {name} 含非有限值 {v}")
        parts = [f"{v:.9g}" for v in vals]
        rows = [",".join(parts[i:i + 8]) for i in range(0, len(parts), 8)]
        return ("static const " + dtype + " " + name + "[] = {\n"
                + ",\n".join(rows) + "\n};")

    stages = dict(STAGES)
    stages["f1"] = {"ins": ["kf_input", "h_past_input"],
                    "out": "raw_proj_QuantizeLinear_Input"}
    for name, cfg in stages.items():
        net = cfg.get("net", "dlf1")
        gdir = os.path.join(LAD, net)
        hdr = os.path.join(gdir, f"{net}.h")
        if not os.path.isfile(hdr):
            print(f"  [skip] {name}: 未编译（缺 {hdr}）")
            continue
        txt = open(hdr).read()

        def macro(rx):
            import re
            mm = re.search(rx + r"\s+\(([^)]*)\)", txt)
            return mm.group(1).strip()
        pre = "LL_ATON_" + net.upper()
        n_in = int(macro("#define " + pre + "_IN_NUM"))
        n_out = int(macro("#define " + pre + "_OUT_1_SIZE_BYTES"))
        import glob as _glob
        qcands = sorted(_glob.glob(os.path.join(gdir, "*_Q.json"))) + \
            sorted(_glob.glob(os.path.join(gdir, "st_ai_output", "*_Q.json")))
        if not qcands:
            print(f"  [skip] {name}: 找不到 Q.json")
            continue
        q = json.load(open(qcands[0]))
        # 边界量化参数（user_allocated 张量）
        bounds = {k: v["format"] for k, v in q["tensors"].items()
                  if v.get("user_allocated")}
        rows.append((name, net, n_in, n_out, bounds))
        ins = cfg["ins"]
        for j, t in enumerate(ins):
            arr = man["stages"][name]["ins"][t] if name in STAGES else \
                man["inputs"][t]
            lines_h.append(c_arr(f"dl_{name}_in{j}", arr))
        out_ref = man["stages"][name]["out"] if name in STAGES else man["full_out"]
        lines_h.append(c_arr(f"dl_{name}_exp", out_ref))
        lines_h.append("")
    lines_h.append("#endif /* EMS_DIAG_LADDER_GOLDEN_H */")
    with open(os.path.join(APP, "ems_diag_ladder_golden.h"), "w") as fh:
        fh.write("\n".join(lines_h))
    with open(os.path.join(LAD, "ladder_manifest.json"), "w") as fh:
        json.dump([{"stage": r[0], "net": r[1], "n_in": r[2], "out_bytes": r[3],
                    "bounds": r[4]} for r in rows], fh, indent=2)
    print(f"emit-c 完成：{len(rows)} 级 → Appli/App/ems_diag_ladder_golden.h")
    for r in rows:
        print("  ", r[0], r[1], "n_in=", r[2], "out_bytes=", r[3])


def main() -> int:
    cmd = sys.argv[1] if len(sys.argv) > 1 else "cut"
    os.makedirs(LAD, exist_ok=True)
    if cmd == "cut":
        mi, vi = _load_inferred()
        for name, cfg in STAGES.items():
            p = cut_stage(mi, vi, cfg["net"], cfg["ins"], cfg["out"])
            print("子图:", p)
        golden_all()
    elif cmd == "golden":
        golden_all()
    elif cmd == "emit-c":
        emit_c()
    else:
        raise SystemExit(f"未知子命令 {cmd}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
