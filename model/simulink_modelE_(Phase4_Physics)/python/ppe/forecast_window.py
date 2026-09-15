"""T-004 — ForecastWindow contract + C0/C1/C2 assembly + Option-B warm-up.

Canonical object (FR-820). Only the forecast *source* differs across arms
(FR-825). Formal C1 reads the frozen store only (D2); C2 consumes the
de-normalized REAT-T q50 vector (injected by the runner; this module never
touches keras). Option-B warm-up (DGN-PPE-02): for t0 < 300 s all C* arms use
the SAME C0-persistence ForecastWindow; differentiation starts at the first
Dataset-v3 anchor t0 = 300 s.
"""
from __future__ import annotations

import numpy as np

SOURCE_IDS = ("C0_PERSISTENCE", "C1_TRUTH", "C2_REATT_Q50")
WARMUP_S = 300.0
HORIZON_S = 300.0
BIN_S = 10.0
MAX_BINS = 30
_TOL = 1e-9


class ForecastWindowError(ValueError):
    pass


class IllegalUpdateTime(ForecastWindowError):
    """t0 is not a legal differentiated update anchor for this voyage."""


class ForecastWindow:
    """Canonical forecast window (FR-820). Immutable value object."""

    def __init__(self, t0_s, Pbar_kW, bin_duration_s, T_end_s, source_id,
                 provenance=None):
        Pbar = np.asarray(Pbar_kW, dtype=float).ravel()
        dur = np.asarray(bin_duration_s, dtype=float).ravel()
        if source_id not in SOURCE_IDS:
            raise ForecastWindowError(f"source_id {source_id} 非法")
        if Pbar.size == 0 or Pbar.size > MAX_BINS:
            raise ForecastWindowError(f"K={Pbar.size} 越界 (1..{MAX_BINS})")
        if Pbar.shape != dur.shape:
            raise ForecastWindowError("Pbar 与 bin_duration 形状不一致")
        if not np.all(np.isfinite(Pbar)):
            raise ForecastWindowError("Pbar 含非有限值")
        if np.any(dur <= 0) or np.any(dur > BIN_S + _TOL):
            raise ForecastWindowError("bin_duration 非法")
        if abs(float(dur[:-1].sum() if dur.size > 1 else 0.0)
               - BIN_S * (dur.size - 1)) > 1e-6:
            raise ForecastWindowError("仅末 bin 可为 partial")
        self.t0_s = float(t0_s)
        self.Pbar_kW = Pbar
        self.bin_duration_s = dur
        self.K_remain = int(Pbar.size)
        self.T_end_s = float(T_end_s)
        self.source_id = source_id
        self.provenance = dict(provenance or {})

    def config_dump(self) -> dict:
        """C*-consuming view; identical across sources except
        source_id/Pbar/provenance (FR-825, AC-803)."""
        return {
            "t0_s": self.t0_s,
            "Pbar_kW": self.Pbar_kW.tolist(),
            "bin_duration_s": self.bin_duration_s.tolist(),
            "K_remain": self.K_remain,
            "T_end_s": self.T_end_s,
            "source_id": self.source_id,
        }


def horizon_bins(t0: float, T_eval: float) -> np.ndarray:
    """Shrinking-horizon bin durations from the canonical T_eval (D4).

    Covers (t0, min(t0+300, T_eval)]; K <= 30; only the final bin may be
    partial. Trailing bins are omitted, never zero-filled.
    """
    span = min(HORIZON_S, float(T_eval) - float(t0))
    if span <= 0:
        raise ForecastWindowError(f"t0={t0} 超出 T_eval={T_eval}")
    n_full = int(np.floor(span / BIN_S + _TOL))
    rem = span - BIN_S * n_full
    durs = [BIN_S] * n_full
    if rem > _TOL:
        durs.append(rem)
    durs = np.asarray(durs[:MAX_BINS], dtype=float)
    if durs.size == 0:
        raise ForecastWindowError(f"t0={t0}: 空窗口")
    return durs


def is_differentiated_update(t0: float, teval_row: dict) -> bool:
    """C1/C2 windows are consumed exactly at the Dataset-v3 anchors (D3)."""
    if t0 < WARMUP_S - _TOL:
        return False
    if abs(t0 % BIN_S) > _TOL and abs(t0 % BIN_S - BIN_S) > _TOL:
        return False
    return t0 <= float(teval_row["T_fcst_s"]) + _TOL


def effective_source(t0: float, arm: str, teval_row: dict) -> str:
    """Option-B: before the first anchor every C* arm is C0 (D3)."""
    if arm == "C0":
        return "C0_PERSISTENCE"
    if arm == "C1":
        src = "C1_TRUTH"
    elif arm == "C2":
        src = "C2_REATT_Q50"
    else:
        raise ForecastWindowError(f"arm {arm} 非法")
    if not is_differentiated_update(t0, teval_row):
        if t0 < WARMUP_S - _TOL:
            return "C0_PERSISTENCE"  # common causal warm-up
        raise IllegalUpdateTime(
            f"t0={t0} 不是合法 anchor（>T_fcst 或非网格）——保持上一 Δλ (ZOH)")
    return src


def assemble_c0(t0: float, T_eval: float, load_history_t: np.ndarray,
                load_history_p: np.ndarray, provenance=None) -> ForecastWindow:
    """C0 persistence (FR-821): mean of the most recently COMPLETED 10 s bin;
    t0 < 10 s → current sample P_load_total(t0). No future samples."""
    t = np.asarray(load_history_t, dtype=float).ravel()
    p = np.asarray(load_history_p, dtype=float).ravel()
    if np.any(t > t0 + _TOL):
        raise ForecastWindowError("C0 历史含 t>t0 样本（未来泄漏）")
    if t0 >= BIN_S - _TOL:
        lo, hi = t0 - BIN_S, t0
        mask = (t > lo + _TOL) & (t <= hi + _TOL)
        if not np.any(mask):
            raise ForecastWindowError(f"t0={t0}: 已完成 bin 无样本")
        p_persist = float(np.mean(p[mask]))
    else:
        mask = np.abs(t - t0) <= _TOL
        if not np.any(mask):
            raise ForecastWindowError(f"t0={t0}: 无当前样本")
        p_persist = float(p[mask][-1])
    durs = horizon_bins(t0, T_eval)
    return ForecastWindow(
        t0_s=t0, Pbar_kW=np.full(durs.size, p_persist),
        bin_duration_s=durs, T_end_s=T_eval,
        source_id="C0_PERSISTENCE",
        provenance=provenance or {"rule": "completed-bin mean persistence"})


def assemble_c1(t0: float, teval_row: dict, index) -> ForecastWindow:
    """C1 = stored Dataset-v3 y_k lookup ONLY (D2). Fail-closed on miss."""
    if not is_differentiated_update(t0, teval_row):
        raise IllegalUpdateTime(f"t0={t0} 非 C1 合法 anchor")
    y = index.get(teval_row["voyage_id"], t0)  # raises C1TargetMissing
    durs = horizon_bins(t0, teval_row["T_eval"])
    k = durs.size
    return ForecastWindow(
        t0_s=t0, Pbar_kW=y[:k], bin_duration_s=durs,
        T_end_s=teval_row["T_eval"], source_id="C1_TRUTH",
        provenance={"store_sha256": index.store_sha256(),
                    "teval_table_sha256": teval_row.get("_table_sha", "")})


def assemble_c2(t0: float, teval_row: dict, q50_kw: np.ndarray,
                provenance=None) -> ForecastWindow:
    """C2 = de-normalized REAT-T q50 (injected; this layer never reads keras)."""
    if not is_differentiated_update(t0, teval_row):
        raise IllegalUpdateTime(f"t0={t0} 非 C2 合法 anchor")
    q = np.asarray(q50_kw, dtype=float).ravel()
    if q.size != MAX_BINS:
        raise ForecastWindowError(f"q50 维度 {q.size} != {MAX_BINS}")
    if not np.all(np.isfinite(q)):
        raise ForecastWindowError("q50 含非有限值")
    durs = horizon_bins(t0, teval_row["T_eval"])
    k = durs.size
    return ForecastWindow(
        t0_s=t0, Pbar_kW=q[:k], bin_duration_s=durs,
        T_end_s=teval_row["T_eval"], source_id="C2_REATT_Q50",
        provenance=provenance or {})
