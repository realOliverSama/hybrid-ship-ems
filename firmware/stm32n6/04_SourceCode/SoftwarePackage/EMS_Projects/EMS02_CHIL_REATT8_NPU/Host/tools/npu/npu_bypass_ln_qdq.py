#!/usr/bin/env python3
"""把 N2 int8 图里 9 个 LayerNorm 块内部的激活 Q/DQ 对拆掉。

方差、sqrt、reciprocal 不再经过 int8。块出口仍是浮点，下游 QuantizeLinear
照旧。权重 Dequantize 保留。用法（在本文件所在 Host/tools/npu 下）:

  python3 npu_bypass_ln_qdq.py
"""
from __future__ import annotations

import os

import numpy as np
import onnx
from onnx import TensorProto

SRC = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "..", "Model", "reatt8_q_npu", "part_n2", "reattn2n_src.onnx"))
DST = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "..", "Model", "reatt8_q_npu", "npu_safe", "reattn2n_lnfloat.onnx"))

BLOCKS = (
    "fut0_ln1_1", "fut0_ln2_1",
    "fut1_ln1_1", "fut1_ln2_1",
    "fut2_ln1_1", "fut2_ln2_1",
    "fut3_ln1_1", "fut3_ln2_1",
    "fusion_ln_1",
)


def _in_block(name: str) -> bool:
    return any(b in name for b in BLOCKS)


def build(model: onnx.ModelProto) -> tuple[onnx.ModelProto, int]:
    prod = {}
    for node in model.graph.node:
        for out in node.output:
            prod[out] = node
    inits = {i.name for i in model.graph.initializer}
    repl = {}
    n_pairs = 0
    for node in model.graph.node:
        if node.op_type != "QuantizeLinear" or not _in_block(node.name):
            continue
        src = node.input[0]
        if src in inits or src not in prod:
            continue
        q_out = node.output[0]
        for other in model.graph.node:
            if other.op_type != "DequantizeLinear":
                continue
            if not other.input or other.input[0] != q_out:
                continue
            repl[other.output[0]] = src
            n_pairs += 1
    for node in model.graph.node:
        for i, name in enumerate(node.input):
            if name in repl:
                node.input[i] = repl[name]
    # 从输出反向留下仍被引用的节点
    consumers = {}
    for node in model.graph.node:
        for name in node.input:
            consumers.setdefault(name, []).append(node)
    live = set()
    stack = [o.name for o in model.graph.output]
    while stack:
        name = stack.pop()
        node = prod.get(name)
        if node is None or node.name in live:
            continue
        live.add(node.name)
        for inp in node.input:
            if inp in prod:
                stack.append(inp)
    kept = [n for n in model.graph.node if n.name in live]
    del model.graph.node[:]
    model.graph.node.extend(kept)
    return model, n_pairs


def _ort_m(path: str, seed: int = 0) -> np.ndarray:
    import onnxruntime as ort
    rng = np.random.default_rng(seed)
    sess = ort.InferenceSession(path, providers=["CPUExecutionProvider"])
    feeds = {
        "kf_input": rng.standard_normal((1, 30, 6)).astype(np.float32),
        "h_past_input": rng.standard_normal((1, 96)).astype(np.float32),
    }
    y = sess.run(None, feeds)[0]
    return np.asarray(y).reshape(30, 3)


def main() -> int:
    model = onnx.load(SRC)
    model, n_pairs = build(model)
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    onnx.save(model, DST)
    ops = {}
    for n in model.graph.node:
        ops[n.op_type] = ops.get(n.op_type, 0) + 1
    print(f"removed_qdq_pairs {n_pairs}  nodes {len(model.graph.node)}")
    print("sqrt", ops.get("Sqrt", 0), "reciprocal", ops.get("Reciprocal", 0),
          "q", ops.get("QuantizeLinear", 0), "dq", ops.get("DequantizeLinear", 0))
    a = _ort_m(SRC)
    b = _ort_m(DST)
    print("ort_orig m", np.round(a[:, 0], 2))
    print("ort_lnfloat m", np.round(b[:, 0], 2))
    print("max|Δ|", float(np.max(np.abs(a - b))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
