# -*- coding: utf-8 -*-
"""load_models_mcp.py — MATLAB py. 桥：加载 TCN/N-BEATS/MLP 模型供闭环外部预测

用法（MATLAB）：py.importlib.import_module('load_models_mcp'); m = py.load_models_mcp.get_model('tcn_f64');
每个进程加载一次（py. 缓存）。返回 keras Model。
"""
import os, io, sys
if hasattr(sys.stdout, 'buffer'):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
os.environ.setdefault('TF_CPP_MIN_LOG_LEVEL', '3')
import numpy as np
import tensorflow as tf
import architectures  # noqa: F401  注册 LastTimeStep 等自定义层

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
MD = os.path.join(BASE, 'results', 'ecms_phase25', 'models')

_CACHE = {}

def get_model(run_id):
    """run_id: 'mlp_h64_32_lr3e-4_q' / 'tcn_f64' / 'nbeats_f256_lr1e-3'"""
    if run_id in _CACHE:
        return _CACHE[run_id]
    p = os.path.join(MD, run_id, run_id + '.keras')
    m = tf.keras.models.load_model(p, safe_mode=False, compile=False)
    _CACHE[run_id] = m
    return m

def predict_q(run_id, P, S):
    """P: (300,) 1-D 或 (1,300,1); S: (2,) -> [1,3] q10/q50/q90"""
    m = get_model(run_id)
    if len(m.inputs) == 1 and m.inputs[0].name.startswith('P_in_'):
        # 单输入 MLP：直接 302 向量
        a = np.asarray(P if P is not None else S, dtype=np.float32)
        print('DBG MLP raw:', a.shape, a.dtype, file=sys.stderr)
        a = a.reshape([1, -1])
        print('DBG MLP final:', a.shape, file=sys.stderr)
        y = m.predict(a, verbose=0)
    else:
        # 双输入 TCN/N-BEATS：[P(300,1), S(2,)]
        aP = np.asarray(P, dtype=np.float32).reshape(-1)
        aS = np.asarray(S, dtype=np.float32).reshape(-1)
        if aP.size == 300 and aS.size == 2:
            aP = aP.reshape([1, 300, 1]); aS = aS.reshape([1, 2])
        else:
            aP = aP.reshape([-1, 300, 1]); aS = aS.reshape([-1, 2])
        y = m.predict([aP, aS], verbose=0)
    return y


import scipy.io as sio

def predict_file(run_id, in_mat, out_mat):
    """MATLAB 文件交换桥：读 in_mat(x 302x1), 前向, 写 out_mat(q50_norm)
    绕开 MATLAB->py 数组传递的形状缺陷（实测 302 列向量传丢元素）。"""
    d = sio.loadmat(in_mat)
    x = d['xn'].reshape(-1).astype(np.float32)         # 302
    m = get_model(run_id)
    if len(m.inputs) == 1 and m.inputs[0].name.startswith('P_in_'):
        y = m.predict(x.reshape([1, -1]), verbose=0)    # (1,3)
    else:
        P = x[:300].reshape([1, 300, 1])
        S = x[300:302].reshape([1, 2])
        y = m.predict([P, S], verbose=0)
    q50 = float(np.asarray(y).reshape(-1)[1])           # q50 归一化域
    sio.savemat(out_mat, {'q50_norm': q50})
    return 0

def predict_point(run_id, X):
    """单点预测（非分位数模型，如 N-BEATS 可能无 q 头）-> [N,1]"""
    m = get_model(run_id)
    y = m.predict(X, verbose=0)
    if y.ndim == 2 and y.shape[1] > 1:
        y = y[:, 0:1]
    return y
