#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""aggregate.py — channel-aware causal aggregation（DGN-05/06，FR-470，T-008）.

冻结语义（唯一事实源：.specs/phase4-d-dataset-v3/design.md DGN-05 + 修订 R1、
config/dataset_v3_signal_registry.json enums.aggregation / aggregation_semantics）：

- bin 语义右闭 (a, b]：mask = (t > a) & (t <= b)；样本 t == a 属于左侧相邻 bin，
  样本 t == b 属于本 bin（off-by-one 契约，单测覆盖）。
- 四规则枚举 = registry ``enums.aggregation``（经 contracts 层消费，禁止本地硬编码清单）：
  * ``time_mean``：bin 内 trapezoidal 时间加权均值 = ∫_a^b v dt / (b-a)。
    积分节点 = 左边界 a（v(a) 线性内插；0.01 s 网格且边沿对齐时等于该格点值）
    + bin 内全部 native 样本 + 右端点 b（若 b 恰为样本点，零宽段贡献为 0，
    等价于直接取该样本值）。**禁止 mean(samples)（naive decimation）**。
    功率列满足 Σ_k P̄_k·Δt ≈ ∫P dt（相对容差 1e-9，见 time_mean_energy_check）。
  * ``interval_endpoint``：bin 内最后一个样本值（状态积累量/约束快照）。
  * ``last_mode``：bin 内最后一个样本值（与 endpoint 同计算、语义不同：离散模式/状态）。
  * ``interval_any``：bin 内任一样本非零为真，返回 bool（事件/flag）。
- DGN-06 zero-fill（post-aggregation，仅作用于 1 Hz history 视图列）：
  ctrl_mode==0（AUTO）→ manual_power_cmd_pct 置 0.0；
  ctrl_mode==1（MANUAL）→ v_ref_mps 置 0.0；确定性、无 NaN、无双写。
  ORACLE 层（v_ref_mps_unmasked / manual_power_cmd_pct_unmasked）保留原始真值，
  不由本模块触碰（由调用方保证）。

空 bin 行为（fail-fast 原则，§6 优先暴露真实问题）：
- ``time_mean``：bin 内无样本时退化为 [a,b] 两端线性内插节点的梯形积分（数学上
  良定义——两端点值按线性内插评估）；若 a/b 超出数据覆盖范围则 ValueError。
- ``interval_endpoint`` / ``last_mode``：bin 内无样本 → ValueError（无定义，禁静默补数）。
- ``interval_any``：空集 any() = False（数学自然语义）。
"""

from __future__ import annotations

import numpy as np

from . import contracts

# 判等/边界容差（秒）：0.01 s 网格上远小于一个 native 步长（vintage 对齐同容差）
_TOL_S = 1e-9

# registry enums.aggregation 进程级缓存（fail-fast 契约校验每次进程只做一次）
_VALID_RULES_CACHE = None


# DGN-06 通道名冻结（registry INPUT_H1 条目 + ctrl_mode 语义）
CTRL_MODE_KEY = "ctrl_mode"
V_REF_KEY = "v_ref_mps"
MANUAL_CMD_KEY = "manual_power_cmd_pct"
_CTRL_MODE_AUTO = 0
_CTRL_MODE_MANUAL = 1


def _valid_rules() -> tuple:
    """aggregation 合法规则枚举 = registry enums.aggregation（FR-465，进程级缓存）。"""
    global _VALID_RULES_CACHE
    if _VALID_RULES_CACHE is None:
        _VALID_RULES_CACHE = tuple(
            contracts.load_registry(validate=True)["enums"]["aggregation"])
    return _VALID_RULES_CACHE


def _check_coverage(t: np.ndarray, a: float, b: float) -> None:
    """bin 边界必须落在数据覆盖范围内（容差 _TOL_S），否则 fail-fast。"""
    if a < t[0] - _TOL_S or b > t[-1] + _TOL_S:
        raise ValueError(
            f"bin ({a}, {b}] 超出数据覆盖范围 [{t[0]}, {t[-1]}]，拒绝静默外推")


def aggregate_bin(t, v, a, b, rule):
    """单个右闭 bin (a, b] 按规则聚合，返回 float 或 bool（interval_any）。

    Parameters
    ----------
    t : (M,) 升序 native 采样时刻（秒）
    v : (M,) 信号值
    a, b : bin 左开右闭边界（要求 b > a）
    rule : registry enums.aggregation 之一
    """
    t = np.asarray(t, dtype=float)
    v = np.asarray(v)
    if t.ndim != 1 or v.shape != t.shape:
        raise ValueError("t/v 必须为同形状一维数组")
    if not b > a:
        raise ValueError(f"bin 边界非法：要求 b > a，实际 ({a}, {b}]")
    if rule not in _valid_rules():
        raise ValueError(f"aggregation 规则非法: {rule!r}（合法={_valid_rules()}）")

    # 右闭 (a, b]（含容差，R1 审查 MINOR-1 主 Agent 裁决）：native 时间戳的 fp
    # 表示噪声（≤1e-9，远小于 0.01 s 步长）不得改变 bin 归属——语义仍为右闭。
    mask = (t > a + _TOL_S) & (t <= b + _TOL_S)
    t_in = t[mask]
    v_in = v[mask]

    if rule == "time_mean":
        _check_coverage(t, a, b)
        # 积分节点：边界 a（内插）+ bin 内样本 + 右端点 b（恰为样本时零宽段，贡献 0）
        v_a = float(np.interp(a, t, v))
        v_b = float(np.interp(b, t, v))
        nodes_t = np.concatenate((np.array([a], dtype=float), t_in, np.array([b], dtype=float)))
        nodes_v = np.concatenate((np.array([v_a], dtype=float),
                                  np.asarray(v_in, dtype=float),
                                  np.array([v_b], dtype=float)))
        integral = float(np.trapezoid(nodes_v, nodes_t))
        return integral / (b - a)

    if rule in ("interval_endpoint", "last_mode"):
        if t_in.size == 0:
            raise ValueError(f"bin ({a}, {b}] 内无样本，{rule} 无定义（禁静默补数）")
        return float(v_in[-1])

    if rule == "interval_any":
        if t_in.size == 0:
            return False
        return bool(np.any(np.asarray(v_in, dtype=float) != 0.0))

    raise ValueError(f"aggregation 规则非法: {rule!r}")  # 防御分支（enum 已校验）


def aggregate_stream(t, cols: dict, edges: np.ndarray, agg_map: dict) -> dict:
    """整条流按 bin 边界序列聚合。

    Parameters
    ----------
    t : (M,) native 采样时刻
    cols : {col_name: (M,) 数组}
    edges : (K+1,) 升序 bin 边界；第 i 个 bin = (edges[i], edges[i+1]]
    agg_map : {col_name: rule}（值须为 registry enums.aggregation 之一）

    Returns
    -------
    {col_name: (K,) 数组}（interval_any 列为 bool，其余 float）
    """
    edges = np.asarray(edges, dtype=float)
    if edges.ndim != 1 or edges.size < 2 or not np.all(np.diff(edges) > 0):
        raise ValueError("edges 必须为长度 ≥2 的严格升序一维数组")
    out = {}
    for name, rule in agg_map.items():
        if name not in cols:
            raise KeyError(f"agg_map 引用了不存在的列: {name}")
        v = np.asarray(cols[name])
        out[name] = np.array(
            [aggregate_bin(t, v, edges[i], edges[i + 1], rule)
             for i in range(edges.size - 1)])
    return out


def zero_fill_ctrl_channels(hist: dict) -> dict:
    """DGN-06：按聚合后 ctrl_mode 列对双指令通道做 zero-fill（post-aggregation）。

    - ctrl_mode == 0（AUTO）→ manual_power_cmd_pct 置 0.0（v_ref_mps 保留）
    - ctrl_mode == 1（MANUAL）→ v_ref_mps 置 0.0（manual_power_cmd_pct 保留）
    - ctrl_mode 出现 {0,1} 之外取值 → ValueError（编译期 0/1 编码，异常值暴露不降级）

    输入 dict 不被就地修改（返回浅拷贝，受影响两列替换为新 float 数组）；
    ORACLE 原始列（*_unmasked）不在本函数职责内，由调用方保证不经过此通路。
    """
    for key in (CTRL_MODE_KEY, V_REF_KEY, MANUAL_CMD_KEY):
        if key not in hist:
            raise KeyError(f"zero_fill_ctrl_channels 缺少必需键: {key}")

    mode = np.asarray(hist[CTRL_MODE_KEY])
    bad = ~np.isin(np.asarray(mode, dtype=float), (_CTRL_MODE_AUTO, _CTRL_MODE_MANUAL))
    if np.any(bad):
        raise ValueError(f"ctrl_mode 出现非法取值 {np.unique(np.asarray(mode, dtype=float)[bad])!r}"
                         "（DGN-06 编译期编码仅允许 0=AUTO/1=MANUAL）")

    out = dict(hist)  # 浅拷贝：只替换受影响两键，其余键共享
    auto = np.asarray(mode, dtype=float) == float(_CTRL_MODE_AUTO)
    manual = np.asarray(mode, dtype=float) == float(_CTRL_MODE_MANUAL)

    v_ref = np.asarray(hist[V_REF_KEY], dtype=float).copy()
    v_ref[manual] = 0.0
    out[V_REF_KEY] = v_ref

    cmd = np.asarray(hist[MANUAL_CMD_KEY], dtype=float).copy()
    cmd[auto] = 0.0
    out[MANUAL_CMD_KEY] = cmd
    return out


def time_mean_energy_check(t, p, edges) -> float:
    """time_mean 聚合的能量守恒自检（DGN-05 能量语义自检）。

    返回逐 bin 相对误差 |P̄_k·Δt − ∫_bin P dt| / |∫_bin P dt| 的最大值。
    真值积分由 native 流梯形原函数（含边界部分段线性内插）独立计算——
    与聚合路径不同代码路径，交叉可查。对齐网格上典型 ~1e-16，阈值 1e-9。

    bin 内真值能量为 0 时（如 P≡0）分母退化为 1.0（此时分子应≈0）。
    """
    t = np.asarray(t, dtype=float)
    p = np.asarray(p, dtype=float)
    edges = np.asarray(edges, dtype=float)

    # native 梯形原函数：E[i] = ∫_{t[0]}^{t[i]} P dt（kW·s）
    seg = 0.5 * (p[1:] + p[:-1]) * np.diff(t)
    E = np.concatenate((np.array([0.0]), np.cumsum(seg)))

    def _energy_at(tau: float) -> float:
        j = int(np.searchsorted(t, tau, side="left"))
        if j < t.size and abs(t[j] - tau) <= _TOL_S:
            return float(E[j])
        if j == 0 or j >= t.size:
            raise ValueError(f"能量评估时刻 {tau} 超出数据覆盖范围")
        # tau 严格位于 (t[j-1], t[j]) 内：部分段梯形（P(τ) 线性内插）
        v_tau = p[j - 1] + (p[j] - p[j - 1]) * (tau - t[j - 1]) / (t[j] - t[j - 1])
        return float(E[j - 1] + 0.5 * (p[j - 1] + v_tau) * (tau - t[j - 1]))

    worst = 0.0
    for i in range(edges.size - 1):
        a, b = float(edges[i]), float(edges[i + 1])
        true_e = _energy_at(b) - _energy_at(a)
        agg_e = float(aggregate_bin(t, p, a, b, "time_mean")) * (b - a)
        denom = abs(true_e) if abs(true_e) > 0.0 else 1.0
        worst = max(worst, abs(agg_e - true_e) / denom)
    return float(worst)


__all__ = ["aggregate_bin", "aggregate_stream", "zero_fill_ctrl_channels",
           "time_mean_energy_check"]
