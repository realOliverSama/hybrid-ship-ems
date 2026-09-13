# -*- coding: utf-8 -*-
"""quantile.py — 统一 quantile 头与 anti-crossing（T-005，DGN-E-06；FR-584）。

冻结决策：统一单调参数化（不用 crossing penalty）。
接口分两段：
- 架构专属 raw 投影（每架构自己产出 raw [B,30,3]=(m,a,b)——模型侧职责）；
- 共用变换模块 `MonotonicQuantileTransform`（全体五候选同一实现；
  模块内不得再含 Dense(3) 等二次投影——R4）。

    q50_k = m_k
    q10_k = m_k − softplus(a_k)
    q90_k = m_k + softplus(b_k)
    ⇒ q10 ≤ q50 ≤ q90 结构保证（softplus ≥ 0）

- 输出张量 [B,30,3]，末维顺序 [q10,q50,q90]（FR-589 输出契约）；
- Loss = kW 域 pinball（30 bin × 3 τ 等权；target 不做归一化）；
- crossing rate 仍逐窗审计（容差 0，结构上应恒 0）；
- 数值细节：softplus 用 tf.nn.softplus；a/b 的 bias 初始化用数值稳定 softplus 逆
  softplus_inv(s) = s（s>20）否则 log(expm1(s))——禁 naive log(exp(s0)−1)；
  s₀ = train 全局 y_std（来源 = eval_config.json 记录值，写入 model_config）。
"""
from __future__ import annotations

import numpy as np
import tensorflow as tf

TAUS = (0.1, 0.5, 0.9)
Q_ORDER = ("q10", "q50", "q90")
N_LEAD_BINS = 30
N_QUANTILES = 3
CROSSING_TOL = 0.0


def softplus_inv(s: float) -> float:
    """数值稳定 softplus 逆：s>20 直通，否则 log(expm1(s))（禁 naive 形式）。"""
    s = float(s)
    if s > 20.0:
        return s
    if s <= 0.0:
        raise ValueError(f"softplus_inv 定义域需 s>0，实际 {s}")
    return float(np.log(np.expm1(s)))


def quantile_bias_init(s0: float) -> np.ndarray:
    """raw 投影 bias 初始化（每 lead-bin 3 位 (m,a,b)）。

    a/b 位 = softplus_inv(s0)（初始区间宽 ≈ s0 = train 全局 y_std）；
    m 位 = 0（保守零初始化）。返回形状 (3,)，模型侧按 (m,a,b) 通道平铺。
    """
    return np.array([0.0, softplus_inv(s0), softplus_inv(s0)], dtype=np.float32)


def build_monotonic_quantile_transform():
    """构造共用 MonotonicQuantileTransform 实例。

    实现为模块级真实类（下方）——闭包工厂类无法跨进程 .keras 反序列化
    （T-007 事故 DGV 记录：custom_objects 必须拿类本身）。
    """
    return MonotonicQuantileTransform()


class MonotonicQuantileTransform(tf.keras.layers.Layer):
    """共用单调 quantile 变换（无参数；禁二次投影；全体五候选同一实现）。

    raw [B,30,3]=(m,a,b) → [B,30,3] 末维 [q10,q50,q90]。
    """

    def call(self, raw):
        raw = tf.convert_to_tensor(raw)
        m, a, b = raw[..., 0], raw[..., 1], raw[..., 2]
        q10 = m - tf.nn.softplus(a)
        q90 = m + tf.nn.softplus(b)
        return tf.stack([q10, m, q90], axis=-1)

    def get_config(self):
        return super().get_config()



def pinball_loss(y_true, y_pred):
    """kW 域 pinball：L = (1/90)·Σ_k Σ_τ ρ_τ(y_k − q_τ,k)（30×3 等权）。

    y_true [B,30]（kW，不归一化）；y_pred [B,30,3] 末维 [q10,q50,q90]。
    """
    import tensorflow as tf
    y = tf.convert_to_tensor(y_true)
    q = tf.convert_to_tensor(y_pred)
    if y.shape.rank == 3 and y.shape[-1] == 1:
        y = tf.squeeze(y, axis=-1)
    taus = tf.constant(TAUS, dtype=q.dtype)            # [3]
    u = y[..., None] - q                               # [B,30,3]
    rho = tf.maximum(taus * u, (taus - 1.0) * u)
    return tf.reduce_mean(rho)


def pinball_numpy(y_true: np.ndarray, y_pred: np.ndarray) -> np.ndarray:
    """numpy 参考实现（逐窗 pinball，[N] 向量）——单测对拍 + 指标栈原子。"""
    y = np.asarray(y_true, dtype=np.float64)
    q = np.asarray(y_pred, dtype=np.float64)
    taus = np.asarray(TAUS, dtype=np.float64)
    u = y[..., None] - q                               # [N,30,3]
    rho = np.maximum(taus * u, (taus - 1.0) * u)
    return rho.mean(axis=(1, 2))


def crossing_rate(y_pred: np.ndarray, tol: float = CROSSING_TOL) -> float:
    """crossing rate 逐窗审计（FR-584：即使结构上不可能也机器记录）。

    mean(q10 > q50+tol or q50 > q90+tol) over 全部窗×bin；单调参数化下应恒 0。
    """
    q = np.asarray(y_pred, dtype=np.float64)
    q10, q50, q90 = q[..., 0], q[..., 1], q[..., 2]
    bad = (q10 > q50 + tol) | (q50 > q90 + tol)
    return float(bad.mean()) if bad.size else 0.0
