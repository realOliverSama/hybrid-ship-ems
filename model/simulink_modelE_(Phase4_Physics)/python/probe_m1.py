# -*- coding: utf-8 -*-
"""probe_m1.py — MacBook M1 训练路径探针（T-305，2026-08-29）

用法（在仓库根目录、M1 venv 激活状态下）:
    python "model/simulink_modelD_(EMS_NN_AECMS)/python/probe_m1.py"          # GPU(metal)
    CUDA_VISIBLE_DEVICES=-1 python ".../probe_m1.py"                          # 纯CPU 对照
    taskpolicy -c user-initiated python ".../probe_m1.py"                     # P核 QoS 修正

输出: 16k 样本 × 1 epoch 的 wall time 与 GPU 设备列表。
判定:
    GPU ≤ 60 s   -> metal 可用，正式扫描走 GPU（预计单配置 15~40 min）
    GPU > 120 s  -> metal 负优化，转 CPU 路线（见交接文档补充）
    CPU 明显快于 GPU -> 同上
"""
import os
import sys
import time

os.environ.setdefault("KMP_DUPLICATE_LIB_OK", "TRUE")

import numpy as np
import tensorflow as tf

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import architectures as A

print("TF:", tf.__version__)
gpus = tf.config.list_physical_devices("GPU")
print("GPU devices:", gpus)
if not gpus and "CUDA_VISIBLE_DEVICES" not in os.environ:
    print("!! metal 插件未生效（无 GPU 设备）——本探针结果是纯 CPU 路径")

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
npz = os.path.join(ROOT, "datasets", "fleet_windows_v2.npz")
d = np.load(npz)
N = 16000
X, y = d["X_train"][:N], d["y_train"][:N]
P, S = A.split_inputs(X)

m = A.build_tcn(filters=16, quantiles=True)

# 1 epoch 预热 + 正式计时（首 epoch 含 GPU 编译，不代表稳态）
m.fit({"P_window_300x1": P, "V_scale_2": S}, y, epochs=1, batch_size=512, verbose=0)
t0 = time.time()
m.fit({"P_window_300x1": P, "V_scale_2": S}, y, epochs=1, batch_size=512, verbose=0)
dt = time.time() - t0

print("16k 样本 × 1 epoch（稳态）= %.1f s" % dt)
print("折算全量 train+val 每 epoch ≈ %.0f s -> 100 epoch ≈ %.1f h/配置" % (
    dt * (108000 + 23807) / N, dt * (108000 + 23807) / N * 100 / 3600))
