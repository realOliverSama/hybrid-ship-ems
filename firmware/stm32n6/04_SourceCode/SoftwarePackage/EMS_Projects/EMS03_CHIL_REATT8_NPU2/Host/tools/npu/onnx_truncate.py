# -*- coding: utf-8 -*-
"""onnx_truncate.py — 按张量截断 ONNX（NPU 编译器故障二分用）。"""
from __future__ import annotations

import argparse
import sys

import onnx


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="src", required=True)
    ap.add_argument("--out", dest="dst", required=True)
    ap.add_argument("--cut", required=True, help="截断处张量名（该张量成为图输出）")
    args = ap.parse_args()

    m = onnx.load(args.src)
    m = onnx.shape_inference.infer_shapes(m, strict_mode=False)
    shapes = {vi.name: vi for vi in list(m.graph.value_info)
              + list(m.graph.input) + list(m.graph.output)}
    if args.cut not in shapes:
        raise SystemExit(f"cut tensor 不存在: {args.cut}")

    needed = {args.cut}
    keep = []
    for n in reversed(m.graph.node):
        if any(o in needed for o in n.output):
            keep.append(n)
            needed.update(i for i in n.input if i)
    keep.reverse()
    out_vi = onnx.helper.make_tensor_value_info(
        args.cut,
        shapes[args.cut].type.tensor_type.elem_type,
        [d.dim_value for d in shapes[args.cut].type.tensor_type.shape.dim])
    g = onnx.helper.make_graph(keep, m.graph.name, list(m.graph.input),
                               [out_vi], list(m.graph.initializer))
    m2 = onnx.helper.make_model(g, opset_imports=list(m.opset_import),
                                producer_name=m.producer_name)
    onnx.checker.check_model(m2)
    onnx.save(m2, args.dst)
    print(f"truncated: {len(m.graph.node)} -> {len(keep)} nodes -> {args.dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
