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
    """q10/q50/q90 三头联合 pinball（各头等权平均）。"""
    l10 = pinball_loss(0.1)
    l50 = pinball_loss(0.5)
    l90 = pinball_loss(0.9)

    @functools.wraps(quantile_loss_3)
    def loss(y_true, y_pred):
        y = tf.cast(y_true, tf.float32)
        p = tf.cast(y_pred, tf.float32)
        q10, q50, q90 = p[..., 0], p[..., 1], p[..., 2]
        y1 = tf.expand_dims(y, -1)          # 便于广播：以下按逐列取 err
        e10 = y1 - q10
        e50 = y1 - q50
        e90 = y1 - q90
        l = (tf.reduce_mean(tf.maximum(0.1 * e10, -0.9 * e10)) +
             tf.reduce_mean(tf.maximum(0.5 * e50, -0.5 * e50)) +
             tf.reduce_mean(tf.maximum(0.9 * e90, -0.1 * e90))) / 3.0
        return l

    return loss


PINBALL_OBJECTS = {"pinball_loss": pinball_loss, "quantile_loss_3": quantile_loss_3}


def split_inputs(X):
    """302 维窗口 -> (P (N,300,1), S (N,2))。"""
    P = X[:, :300].reshape(-1, 300, 1)
    S = X[:, 300:]
    return P, S


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


def build_tcn(filters=64, kernel=3, dilations=(1, 2, 4, 8), lr=1e-3, quantiles=False):
    """TCN：4 残差块（dilation 1,2,4,8）+ GAP；v/accel 经 Dense(8) concat；Dense(64) -> 头。"""
    inp_p = tf.keras.Input(shape=(300, 1), name="P_window_300x1")
    inp_s = tf.keras.Input(shape=(2,), name="V_scale_2")
    x = inp_p
    for i, d in enumerate(dilations):
        x = tcn_block(x, filters, kernel, d, name="tcn_block%d" % i)
    x = tf.keras.layers.GlobalAveragePooling1D(name="tcn_gap")(x)
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
