# -*- coding: utf-8 -*-
"""onnx_static_fold.py — ONNX 常量化手术（phase4-g NPU 线）。

问题：attention（MHA）分解出的 Reshape 目标形状由 Shape/Gather/Concat 运行时
计算（TF2ONNX 导出的动态链），ST NPU 编译器报 "Unknown dimensions: W"。
E 段处方（st_cloud_notes #4）：常量化全部 Reshape shape 输入。

做法：
  1. onnx.shape_inference 推断全图形状（输入已静态 → 全部张量形状可定）；
  2. 对每个 Reshape 节点：若 shape 输入不是 initializer：
     a) 若该 shape 张量已在 initializer（常量折叠结果）→ 直接用；
     b) 否则取其**推断出的输出形状**（Reshape 的 output shape 已由 shape
        inference 求出）作为新 initializer，重接 Reshape 的 shape 输入；
  3. 删除不再被引用的 shape 计算子图节点（可选清理，保守起见保留）。
"""
from __future__ import annotations

import argparse
import sys

import numpy as np
import onnx
from onnx import numpy_helper, shape_inference, helper


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="src", required=True)
    ap.add_argument("--out", dest="dst", required=True)
    args = ap.parse_args()

    m = onnx.load(args.src)
    m = shape_inference.infer_shapes(m, strict_mode=False)

    # 张量形状表
    shapes = {}
    for vi in list(m.graph.value_info) + list(m.graph.input) + list(m.graph.output):
        dims = [d.dim_value if d.HasField("dim_value") else None
                for d in vi.type.tensor_type.shape.dim]
        shapes[vi.name] = dims
    init_names = {i.name for i in m.graph.initializer}

    new_inits, rewired = [], 0
    for node in m.graph.node:
        if node.op_type != "Reshape" or len(node.input) < 2:
            continue
        shp_in = node.input[1]
        if shp_in in init_names:
            continue
        out_shape = shapes.get(node.output[0])
        if out_shape is None or any(v is None for v in out_shape):
            print(f"WARN: 无静态输出形状 {node.output[0]} {out_shape}; 跳过")
            continue
        vals = [int(v) for v in out_shape]
        if 0 in vals:
            # 0 语义（copy dim）→ 用对应输入维替换（批=1 场景）
            in_shape = shapes.get(node.input[0])
            if in_shape is None:
                continue
            vals = [in_shape[i] if v == 0 else v for i, v in enumerate(vals)]
            vals = [1 if v == 0 else v for v in vals]  # copy 的 0 维
        name = shp_in + "_const"
        if name not in init_names:
            new_inits.append(numpy_helper.from_array(
                np.asarray(vals, dtype=np.int64), name))
            init_names.add(name)
        node.input[1] = name
        rewired += 1

    m.graph.initializer.extend(new_inits)
    m = shape_inference.infer_shapes(m, strict_mode=False)
    onnx.checker.check_model(m, full_check=False)
    onnx.save(m, args.dst)
    print(f"rewired {rewired} Reshape nodes, +{len(new_inits)} initializers")
    print("saved:", args.dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
