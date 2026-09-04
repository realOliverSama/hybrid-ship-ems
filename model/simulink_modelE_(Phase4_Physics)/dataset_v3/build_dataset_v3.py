#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build_dataset_v3.py — Dataset v3 构建编排（D0：仅 view 构建层，FR-464/477~480，T-008~010）.

本文件在 D0 阶段只实现 model view 构建函数；E2E 编排 main（MATLAB producer 产物装载 →
aggregate/windowing/targets 组合 → split/审计/manifest）由后续任务接入，**当前不实现**。

View 白名单契约（FR-464/477~480，机器事实源 = registry role，禁止 hard-coded 清单）：
- H1-view 特征   = role INPUT_H1 的 11 列（registry 顺序）；
- H2-view 特征   = INPUT_H1 + INPUT_H2 共 21 列（registry 顺序）；
- trajectory target view = Y[30]（y_k=E_k/10，kW）；
- legacy scalar view     = E300（kWh）；
- FORBIDDEN / METADATA / ORACLE（及 TARGET/未知信号）列进入任何 model view 载荷
  （X_H_cols / X_KF_cols）即 ValueError（负向测试覆盖）；
- E300 与 trajectory 一致性：|E300 − Σ(y_k·10)/3600| < 1e-12（逐样本）。

Quantile 契约（FR-478/479，E 包职责）：数据集**不物化** target_q10/q50/q90 标签；
τ={0.10,0.50,0.90} 的 pinball loss 学习语义与输出不变量 q10 ≤ q50 ≤ q90（防 crossing）
属 Phase 4-E 包实现，本包仅提供 assert_no_quantile_labels 守卫函数证明未物化。
"""

from __future__ import annotations

import numpy as np

from . import contracts

# model view 允许的 storage role（FR-464 八类 role 中仅此四类可进 view 载荷）
_HIST_ALLOWED_ROLES = ("INPUT_H1", "INPUT_H2")
_KF_ALLOWED_ROLES = ("INPUT_K", "INPUT_F")

# FR-477 E300 一致性容差（任务冻结 1e-12，绝对值，kWh）
_E300_CONSISTENCY_TOL = 1e-12
# FR-476 canonical 未来 bin 数（trajectory target view 形状）
_Y_TRAJ_BINS = 30
# FR-478/479 禁止物化的 quantile 标签关键词（小写匹配）
_QUANTILE_MARKERS = ("q10", "q50", "q90", "quantile")


def _role_map(registry: dict) -> dict:
    """name → set(role)（同名多 role，如 P_load_total_kW 的 INPUT_H1/TARGET）。"""
    out = {}
    for s in registry["signals"]:
        out.setdefault(s["name"], set()).add(s["role"])
    return out


def _reject_non_whitelist(payload: dict, role_map: dict, allowed_roles, payload_name: str) -> None:
    """白名单外的列（FORBIDDEN/METADATA/ORACLE/TARGET/未知）出现即 ValueError。"""
    for name in payload:
        roles = role_map.get(name)
        if not roles:
            raise ValueError(
                f"{payload_name} 含 registry 未登记信号: {name!r}（契约违例，禁入 view）")
        if not (roles & set(allowed_roles)):
            raise ValueError(
                f"{payload_name} 列 {name!r} 的 role={sorted(roles)} 不在允许集合 "
                f"{list(allowed_roles)} 内——FORBIDDEN/METADATA/ORACLE/TARGET 列禁入 model view")


def build_views(X_H_cols: dict, X_KF_cols: dict, Y: np.ndarray, E300: np.ndarray,
                registry: dict) -> dict:
    """按 registry role 白名单构建 model views（FR-464/477~480）。

    Parameters
    ----------
    X_H_cols : {col: (N,300)} 聚合后历史载荷（应恰含 INPUT_H1+INPUT_H2 全部列）
    X_KF_cols : {col: (N,30)} K/F vintage 载荷（role 须 ⊆ {INPUT_K, INPUT_F}；
                完整性由 producer schema/E2E 校验，本函数做白名单拒绝检查）
    Y : (N,30) trajectory target（y_k=E_k/10，kW，targets.y_trajectory 产物）
    E300 : (N,) legacy scalar target（kWh，targets.e300_of 产物）
    registry : contracts.load_registry() 产物（机器契约，唯一白名单事实源）

    Returns
    -------
    {"h1_view": {11 列: (N,300)}，"h2_view": {21 列: (N,300)}，
     "y_traj": Y，"e300": E300}——列键序 = registry 文件顺序（确定性，测试覆盖）

    Raises
    ------
    ValueError: 白名单外列出现 / H1∪H2 列缺失 / Y·E300 形状或一致性违反。
    """
    role_map = _role_map(registry)
    h1_names = contracts.signal_names(registry, "INPUT_H1")
    h2_names = h1_names + contracts.signal_names(registry, "INPUT_H2")

    _reject_non_whitelist(X_H_cols, role_map, _HIST_ALLOWED_ROLES, "X_H_cols")
    _reject_non_whitelist(X_KF_cols, role_map, _KF_ALLOWED_ROLES, "X_KF_cols")

    Y = np.asarray(Y, dtype=float)
    E300 = np.asarray(E300, dtype=float)
    if Y.ndim != 2 or Y.shape[1] != _Y_TRAJ_BINS or Y.shape[0] == 0:
        raise ValueError(f"Y 形状须为 (N,{_Y_TRAJ_BINS}) 且 N≥1，实际 {Y.shape}")
    if E300.shape != (Y.shape[0],):
        raise ValueError(f"E300 形状须为 ({Y.shape[0]},)，实际 {E300.shape}")

    # E300 ↔ trajectory 一致性（FR-477：E300 = Σ(y_k·10)/3600）
    e300_from_y = np.sum(Y * 10.0 / 3600.0, axis=1)
    worst = float(np.max(np.abs(E300 - e300_from_y))) if E300.size else 0.0
    if not worst < _E300_CONSISTENCY_TOL:
        raise ValueError(
            f"E300 与 trajectory target 不一致：max|Δ|={worst:.3e} ≥ "
            f"{_E300_CONSISTENCY_TOL} kWh（FR-477 派生恒等被违反）")

    # 白名单视图：按 registry 顺序取列，缺列/形状不符即 fail-fast
    n, f_h = Y.shape[0], 300  # 300 = canonical history bins（FR-469）
    h1_view, h2_view = {}, {}
    for name in h2_names:
        if name not in X_H_cols:
            raise ValueError(f"X_H_cols 缺少 INPUT_H1/H2 契约列: {name}")
        col = np.asarray(X_H_cols[name], dtype=float)
        if col.shape != (n, f_h):
            raise ValueError(f"X_H_cols 列 {name} 形状须为 ({n},{f_h})，实际 {col.shape}")
        if name in h1_names:
            h1_view[name] = col
        h2_view[name] = col

    return {"h1_view": h1_view, "h2_view": h2_view, "y_traj": Y, "e300": E300}


def assert_no_quantile_labels(views: dict) -> None:
    """守卫：views 内任何层级键名含 q10/q50/q90/quantile 即 ValueError（FR-478/479）。

    数据集不得预生成 predictive quantile 标签——pinball 学习语义属 E 包。
    通过（含 h1_view/h2_view 列名的递归检查）则静默返回。
    """
    def _walk(obj, path):
        if isinstance(obj, dict):
            for k, v in obj.items():
                kl = str(k).lower()
                if any(m in kl for m in _QUANTILE_MARKERS):
                    raise ValueError(
                        f"检测到 quantile 标签物化: {path}.{k}（FR-478：数据集禁预生成 "
                        f"target_q10/q50/q90，τ={{0.10,0.50,0.90}} pinball 学习属 E 包）")
                _walk(v, f"{path}.{k}")
    _walk(views, "views")


__all__ = ["build_views", "assert_no_quantile_labels"]
