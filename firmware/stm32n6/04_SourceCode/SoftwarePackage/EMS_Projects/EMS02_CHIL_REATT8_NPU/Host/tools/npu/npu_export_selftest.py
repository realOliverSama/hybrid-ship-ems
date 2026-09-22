#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""npu_export_selftest.py — Neural-ART 平凡自检图导出（phase4-g D2 判别实验）。

目的：在"REATT 类图在 NPU 上输出退化"的排查中，把两个层次分开——
  ① NPU 能否正确**执行**一个语义可手算校验的图；
  ② 权重从 **NOR**（xSPI2 memory-mapped）读取是否正常。
构造：x → Q(1.0,0) → DQ → MatMul(I8 恒等) → Q → DQ → MatMul(恒等) → Q → 输出 int8。
      恒等权重 + scale=1/zp=0 ⇒ **期望输出 = 输入 int8（逐字节相等）**。
      两层串联同时覆盖中间量化/反量化交接。

产物：Model/reatt8_q_npu/selftest/reatt_selftest_int8.onnx（+ fp32 变体）
用法（仓库根 .venv_tf）：python3 <本文件>
"""
from __future__ import annotations

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
OUT_DIR = os.path.join(PROJ, "Model", "reatt8_q_npu", "selftest")

N = 8  # 输入维度


def build_int8_identity_chain():
    import onnx
    from onnx import TensorProto, helper, numpy_helper

    I8 = np.eye(N, dtype=np.int8)
    inits = [
        numpy_helper.from_array(np.array(1.0, dtype=np.float32), "s_in"),
        numpy_helper.from_array(np.array(0, dtype=np.int8), "z_in"),
        numpy_helper.from_array(I8, "W1_q"),
        numpy_helper.from_array(np.array(1.0, dtype=np.float32), "s_w1"),
        numpy_helper.from_array(np.array(0, dtype=np.int8), "z_w1"),
        numpy_helper.from_array(np.array(1.0, dtype=np.float32), "s_m1"),
        numpy_helper.from_array(np.array(0, dtype=np.int8), "z_m1"),
        numpy_helper.from_array(I8, "W2_q"),
        numpy_helper.from_array(np.array(1.0, dtype=np.float32), "s_w2"),
        numpy_helper.from_array(np.array(0, dtype=np.int8), "z_w2"),
        numpy_helper.from_array(np.array(1.0, dtype=np.float32), "s_out"),
        numpy_helper.from_array(np.array(0, dtype=np.int8), "z_out"),
    ]
    nodes = [
        helper.make_node("QuantizeLinear", ["x", "s_in", "z_in"], ["x_q"]),
        helper.make_node("DequantizeLinear", ["x_q", "s_in", "z_in"], ["x_dq"]),
        helper.make_node("DequantizeLinear", ["W1_q", "s_w1", "z_w1"], ["W1_dq"]),
        helper.make_node("MatMul", ["x_dq", "W1_dq"], ["m1"]),
        helper.make_node("QuantizeLinear", ["m1", "s_m1", "z_m1"], ["m1_q"]),
        helper.make_node("DequantizeLinear", ["m1_q", "s_m1", "z_m1"], ["m1_dq"]),
        helper.make_node("DequantizeLinear", ["W2_q", "s_w2", "z_w2"], ["W2_dq"]),
        helper.make_node("MatMul", ["m1_dq", "W2_dq"], ["m2"]),
        helper.make_node("QuantizeLinear", ["m2", "s_out", "z_out"], ["y_q"]),
    ]
    graph = helper.make_graph(
        nodes, "reatt_selftest",
        [helper.make_tensor_value_info("x", TensorProto.FLOAT, [1, N])],
        [helper.make_tensor_value_info("y_q", TensorProto.INT8, [1, N])],
        initializer=inits)
    model = helper.make_model(
        graph, opset_imports=[helper.make_opsetid("", 17)],
        producer_name="phase4g-selftest")
    model.ir_version = 8
    onnx.checker.check_model(model)
    return model


def main() -> int:
    import onnx
    os.makedirs(OUT_DIR, exist_ok=True)
    m = build_int8_identity_chain()
    p = os.path.join(OUT_DIR, "reatt_selftest_int8.onnx")
    onnx.save(m, p)

    import onnxruntime as ort
    s = ort.InferenceSession(p, providers=["CPUExecutionProvider"])
    x = np.array([[-128, -100, -1, 0, 1, 50, 100, 127]], dtype=np.float32)
    y = s.run(None, {"x": x})[0]
    print("ONNX 自检（ORT）:", y.reshape(-1).tolist(), "期望 = 输入 int8 值")
    print("产物:", p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
