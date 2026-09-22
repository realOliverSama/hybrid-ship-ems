# -*- coding: utf-8 -*-
"""onnx_fix_midpad.py — 中间维 Pad 手术（NPU 线）v2（原位替换版）。

对每个 pads 作用在非最内维的 3D Pad 节点，原位替换为：
    x -> Transpose(0,2,1) -> Pad(重排 pads，作用于最内维) -> Transpose(0,2,1) -> 原输出名
数值等价；下游消费者引用原输出名不变。
"""
from __future__ import annotations

import argparse
import sys

import numpy as np
import onnx
from onnx import helper, numpy_helper


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="src", required=True)
    ap.add_argument("--out", dest="dst", required=True)
    args = ap.parse_args()

    m = onnx.load(args.src)
    m = onnx.shape_inference.infer_shapes(m, strict_mode=False)
    shapes = {vi.name: [d.dim_value for d in vi.type.tensor_type.shape.dim]
              for vi in list(m.graph.value_info) + list(m.graph.input)
              + list(m.graph.output)}
    inits = {i.name: i for i in m.graph.initializer}

    new_inits, new_nodes, n_fixed = [], [], 0
    for node in m.graph.node:
        if node.op_type != "Pad" or len(node.input) < 2:
            new_nodes.append(node)
            continue
        if node.input[1] not in inits:
            new_nodes.append(node)
            continue
        pads = numpy_helper.to_array(inits[node.input[1]]).astype(np.int64)
        rank = pads.size // 2
        if rank != 3:
            new_nodes.append(node)
            continue
        begin, end = pads[:rank], pads[rank:]
        if begin[1] == 0 and end[1] == 0:
            new_nodes.append(node)
            continue
        x = node.input[0]
        if shapes.get(x) is None or len(shapes.get(x, [])) != 3:
            print(f"WARN: {node.name} 输入形状未知; 跳过")
            new_nodes.append(node)
            continue
        nb = np.array([begin[0], begin[2], begin[1]], dtype=np.int64)
        ne = np.array([end[0], end[2], end[1]], dtype=np.int64)
        npads = np.concatenate([nb, ne])
        pad_in = node.name + "_mid_pads"
        new_inits.append(numpy_helper.from_array(npads, pad_in))
        t1_out = node.name + "_t1"
        p_out = node.name + "_p"
        orig_out = node.output[0]
        new_nodes.append(helper.make_node(
            "Transpose", [x], [t1_out], name=node.name + "_t1n", perm=[0, 2, 1]))
        new_nodes.append(helper.make_node(
            "Pad", [t1_out, pad_in], [p_out], name=node.name + "_padn",
            mode=node.attribute and "constant" or "constant"))
        new_nodes.append(helper.make_node(
            "Transpose", [p_out], [orig_out], name=node.name + "_t2n",
            perm=[0, 2, 1]))
        n_fixed += 1

    del m.graph.node[:]
    m.graph.node.extend(new_nodes)
    m.graph.initializer.extend(new_inits)
    del m.graph.value_info[:]
    m = onnx.shape_inference.infer_shapes(m, strict_mode=False)
    onnx.checker.check_model(m, full_check=False)
    onnx.save(m, args.dst)
    print(f"fixed {n_fixed} mid-dim Pad nodes -> {args.dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
