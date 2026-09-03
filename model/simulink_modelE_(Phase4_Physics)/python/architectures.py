#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
architectures.py — Phase 2.5 模型族构建器（T-304，FR-305/306）
=================================================================================

主 Agent 批准的三类新架构（spec: nn-predictive-ecms-phase25 design §4）：
  TCN       扩张因果卷积 + 残差块 + GAP + MLP 头（主推）
  CNN-MLP   Conv1D×2 + MaxPool + Flatten + Dense 头
  N-BEATS   generic 全连接栈（backcast/forecast 残差，无趋势/季节基）

统一输入约定：
  X  = (N, 302) float32 归一化窗口
     = [P_demand(t-299..t) x300（1 s 升序） | v_ship(t) | accel(t)]
  分路（与 v1 LSTM 口径一致，P 序列单独成通道）：
    P = X[:, :300].reshape(-1, 300, 1)      # (N, 300, 1)
    S = X[:, 300:]                          # (N, 2) 标量 v_ship/accel

输出头：
  默认单输出 Dense(1)（MSE）；
  --quantiles 时 Dense(3)（q10/q50/q90），loss = 三个 pinball 平均，
  val 指标另加分位数覆盖率与平均 pinball。

所有构建器返回已 compile 的 keras.Model（两个输入：P_window_300x1 / V_scale_2）。
"""

import functools

import tensorflow as tf


@tf.keras.utils.register_keras_serializable()
def pinball_loss(q):
    """pinball/quantile loss：q∈(0,1)。返回可作 keras loss 的闭包。"""
    q = float(q)

    @functools.wraps(pinball_loss)
    def loss(y_true, y_pred):
        err = tf.cast(y_true, tf.float32) - tf.cast(y_pred, tf.float32)
        return tf.reduce_mean(tf.maximum(q * err, (q - 1.0) * err))

    return loss


@tf.keras.utils.register_keras_serializable()
def quantile_loss_3():
    """q10/q50/q90 三头联合 pinball（各头等权平均）。

    T-305 诊断修订（2026-08-29）：原实现 e10 = y1 - q10 中 y1 为 (N,1)、q10 为 (N,)，
    广播成 (N,N) 逐对交叉矩阵，损失退化为全局中位数回归（collapse-to-median 实锤）。
    修正：e = y1 - p 保持 (N,3) 逐样本逐头。tiny-overfit 验证：TCN-MSE 8.64 / 修后 TCN-pinball 应同水平。
    """
    @functools.wraps(quantile_loss_3)
    def loss(y_true, y_pred):
        y = tf.cast(y_true, tf.float32)
        p = tf.cast(y_pred, tf.float32)            # (N,3)
        y1 = tf.reshape(y, [-1, 1])                # (N,1)
        e = y1 - p                                  # (N,3) 逐样本逐头，禁止 (N,1)-(N,)
        l10 = tf.reduce_mean(tf.maximum(0.1 * e[..., 0], -0.9 * e[..., 0]))
        l50 = tf.reduce_mean(tf.maximum(0.5 * e[..., 1], -0.5 * e[..., 1]))
        l90 = tf.reduce_mean(tf.maximum(0.9 * e[..., 2], -0.1 * e[..., 2]))
        return (l10 + l50 + l90) / 3.0

    return loss


PINBALL_OBJECTS = {"pinball_loss": pinball_loss, "quantile_loss_3": quantile_loss_3}


def split_inputs(X):
    """302 维窗口 -> (P (N,300,1), S (N,2))。"""
    P = X[:, :300].reshape(-1, 300, 1)
    S = X[:, 300:]
    return P, S


@tf.keras.utils.register_keras_serializable(package="phase25")
class LastTimeStep(tf.keras.layers.Layer):
    """取序列末时刻特征（T-305）：causal 卷积末步已聚合全部感受野历史。

    用注册层而非 Lambda：keras 3 默认 safe_mode 拒绝反序列化裸 lambda，
    导致训练后 finalize 导出崩潧（2026-08-29 f16 实证）；注册层可安全往返，
    且 slice 语义对 TFLite/EdgeAI 友好。
    """

    def call(self, x):
        return x[:, -1, :]

    def compute_output_shape(self, input_shape):
        return (input_shape[0], input_shape[2])


def _head(x, quantiles):
    if quantiles:
        return tf.keras.layers.Dense(3, name="E_hat_q10_50_90")(x)
    return tf.keras.layers.Dense(1, name="E_hat_norm")(x)


def _compile(model, lr, quantiles):
    if quantiles:
        loss = quantile_loss_3()
    else:
        loss = "mse"
    model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=lr), loss=loss)
    return model


# ---------------------------------------------------------------- TCN

def tcn_block(x, filters, kernel, dilation, name):
    """扩张因果卷积残差块：Conv1D(causal) -> ReLU -> Conv1D(causal) -> +x -> ReLU。"""
    c1 = tf.keras.layers.Conv1D(
        filters, kernel, dilation_rate=dilation, padding="causal",
        kernel_initializer="he_normal", name=name + "_conv1")(x)
    c1 = tf.keras.layers.ReLU(name=name + "_relu1")(c1)
    c2 = tf.keras.layers.Conv1D(
        filters, kernel, dilation_rate=dilation, padding="causal",
        kernel_initializer="he_normal", name=name + "_conv2")(c1)
    c2 = tf.keras.layers.ReLU(name=name + "_relu2")(c2)
    if int(x.shape[-1]) != filters:
        x = tf.keras.layers.Conv1D(
            filters, 1, padding="causal", name=name + "_skip")(x)
    out = tf.keras.layers.Add(name=name + "_add")([x, c2])
    return tf.keras.layers.ReLU(name=name + "_out")(out)


def build_tcn(filters=64, kernel=3, dilations=(1, 2, 4, 8, 16, 32, 32), lr=1e-3, quantiles=False):
    """TCN：7 残差块（dilation 1,2,4,8,16,32,32）+ GAP；v/accel 经 Dense(8) concat；Dense(64) -> 头。

    T-305 感受野推导（FR-305，必须 ≥ 300）：
      R = 1 + Σ_layers (k−1)·d_l
        每块 2 层，k=3 → 每块贡献 2·(k−1)·d = 4·d；7 块 dilation=[1,2,4,8,16,32,32]：
      R = 1 + 4·(1+2+4+8+16+32+32) = 1 + 4·95 = 381 ≥ 300  ✓
      （末端重复 dilation=32 补足序列，全块无下采样，输出 T=300。）
    """
    inp_p = tf.keras.Input(shape=(300, 1), name="P_window_300x1")
    inp_s = tf.keras.Input(shape=(2,), name="V_scale_2")
    x = inp_p
    for i, d in enumerate(dilations):
        x = tcn_block(x, filters, kernel, d, name="tcn_block%d" % i)
    # T-305 诊断修订（2026-08-29）：原 GlobalAveragePooling1D 会抹平时刻位置信息，
    # 导致 q50 collapse 到无条件中位数（val RMSE=16.88≈naive-median 16.87 实证）。
    # 改取末时刻特征：causal 卷积保证末步已聚合 R=381≥300 全部历史，
    # 保留“当前时刻 t”定位（design §4 允许“全局池化/末时刻取特征”，取后者）。
    # LastTimeStep 为注册自定义层（Lambda 裸函数会被 safe_mode 拒绝反序列化）。
    x = LastTimeStep(name="tcn_last_step")(x)
    s = tf.keras.layers.Dense(8, activation="relu", name="scalar_dense8")(inp_s)
    z = tf.keras.layers.Concatenate(name="tcn_concat")([x, s])
    z = tf.keras.layers.Dense(64, activation="relu", name="tcn_head64")(z)
    out = _head(z, quantiles)
    model = tf.keras.Model([inp_p, inp_s], out, name="TCN")
    return _compile(model, lr, quantiles)


# ---------------------------------------------------------------- CNN-MLP

def build_cnn_mlp(filters1=32, kernel1=7, filters2=64, kernel2=5, lr=1e-3, quantiles=False):
    """CNN-MLP：Conv1D(32,k7)+MaxPool(2) -> Conv1D(64,k5)+MaxPool(2) -> Flatten -> Dense(128) -> 头。"""
    inp_p = tf.keras.Input(shape=(300, 1), name="P_window_300x1")
    inp_s = tf.keras.Input(shape=(2,), name="V_scale_2")
    x = tf.keras.layers.Conv1D(filters1, kernel1, padding="same", activation="relu",
                               kernel_initializer="he_normal", name="cnn_conv1")(inp_p)
    x = tf.keras.layers.MaxPooling1D(2, name="cnn_pool1")(x)
    x = tf.keras.layers.Conv1D(filters2, kernel2, padding="same", activation="relu",
                               kernel_initializer="he_normal", name="cnn_conv2")(x)
    x = tf.keras.layers.MaxPooling1D(2, name="cnn_pool2")(x)
    x = tf.keras.layers.Flatten(name="cnn_flatten")(x)
    x = tf.keras.layers.Dense(128, activation="relu", name="cnn_dense128")(x)
    s = tf.keras.layers.Dense(8, activation="relu", name="scalar_dense8")(inp_s)
    z = tf.keras.layers.Concatenate(name="cnn_concat")([x, s])
    out = _head(z, quantiles)
    model = tf.keras.Model([inp_p, inp_s], out, name="CNN_MLP")
    return _compile(model, lr, quantiles)


# ---------------------------------------------------------------- N-BEATS（generic）

def nbeats_block(x, fcs=256, backcast_len=300, forecast_len=1, name="nb_block"):
    """generic N-BEATS block：theta -> backcast(300) / forecast(1)；返回 (残差输入, forecast)。"""
    h = tf.keras.layers.Dense(fcs, activation="relu", name=name + "_fc1")(x)
    h = tf.keras.layers.Dense(fcs, activation="relu", name=name + "_fc2")(h)
    theta = h
    backcast = tf.keras.layers.Dense(backcast_len, name=name + "_backcast")(theta)
    forecast = tf.keras.layers.Dense(forecast_len, name=name + "_forecast")(theta)
    res = tf.keras.layers.Subtract(name=name + "_residual")([x, backcast])
    return res, forecast


def build_nbeats(n_stacks=4, blocks_per_stack=2, fcs=256, lr=1e-3, quantiles=False):
    """N-BEATS generic：4 stack × 2 block，FC(256)，backcast 残差回传，forecast 累加。"""
    inp_p = tf.keras.Input(shape=(300, 1), name="P_window_300x1")
    inp_s = tf.keras.Input(shape=(2,), name="V_scale_2")
    x = tf.keras.layers.Reshape((300,), name="nb_flatten_in")(inp_p)
    forecasts = []
    for si in range(n_stacks):
        xb = x
        for bi in range(blocks_per_stack):
            xb, fc = nbeats_block(xb, fcs=fcs, name="nb_s%d_b%d" % (si, bi))
            forecasts.append(fc)
    z = tf.keras.layers.Add(name="nb_sum_forecast")(forecasts)   # (N, 1)
    s = tf.keras.layers.Dense(8, activation="relu", name="scalar_dense8")(inp_s)
    z = tf.keras.layers.Concatenate(name="nb_concat")([z, s])
    z = tf.keras.layers.Dense(64, activation="relu", name="nb_head64")(z)
    out = _head(z, quantiles)
    model = tf.keras.Model([inp_p, inp_s], out, name="NBEATS")
    return _compile(model, lr, quantiles)


BUILDERS = {
    "tcn": build_tcn,
    "cnn_mlp": build_cnn_mlp,
    "nbeats": build_nbeats,
}


def count_params_from_summary(model):
    """构建后读取参数量（不运行训练）。"""
    return int(model.count_params())
