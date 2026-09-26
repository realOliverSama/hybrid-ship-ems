/* ems04_h1win.c — EMS04-S2 B1：H1 特征窗实现（板侧构造机构）。
 *
 * 浮点纪律（FPC-1.0 §1 冻结）：no fast-math / -ffp-contract=off /
 * IEEE round-to-nearest-even / 禁止代数重排。中点严格写作 (v0 + v1) * 0.5（f64），
 * water_depth 严格写作 (w + w) * 0.5（f64）；出口统一 cast f32。
 */
#include "ems04_h1win.h"

#include <math.h>
#include <string.h>

const char *const ems04_h1_col_names[EMS04_H1_COLS] = {
    "P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
    "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
    "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct"
};

/* CMD_TICK2 载荷字段序（契约 §2.1）→ 环通道序 的映射：
 *   [0] t_s [1] P_load [2] SOC [3] v_ref [4] v_ship [5] accel
 *   [6] x_ship [7] n_prop [8] P_hotel [9] v_current [10] P_prop_demand
 */
static const uint8_t TICK2_TO_RING[EMS04_H1_N_CH] = {
    1u,  /* P_load_total_kW  <- v[1]  */
    4u,  /* v_ship_mps       <- v[4]  */
    5u,  /* acceleration_mps2 <- v[5] */
    7u,  /* n_prop_rpm       <- v[7]  */
    8u,  /* P_hotel_kW       <- v[8]  */
    9u,  /* v_current_mps    <- v[9]  */
    3u,  /* v_ref_mps        <- v[3]  */
    10u  /* P_prop_demand_kW <- v[10] */
};

/* H1 输出列 → 环通道（0xFF = 非常量列，见 construct 分支） */
static const uint8_t H1COL_TO_RING[EMS04_H1_COLS] = {
    0u,   /* 0 P_load_total_kW      */
    1u,   /* 1 v_ship_mps           */
    2u,   /* 2 acceleration_mps2    */
    3u,   /* 3 n_prop_rpm           */
    4u,   /* 4 P_hotel_kW           */
    5u,   /* 5 v_current_mps        */
    0xFFu,/* 6 water_depth_m（航次常量，中点路径） */
    7u,   /* 7 P_prop_demand_kW     */
    0xFFu,/* 8 ctrl_mode（policy 常量） */
    6u,   /* 9 v_ref_mps（interval_endpoint = 右端点） */
    0xFFu /* 10 manual_power_cmd_pct（zero-fill 规则结果） */
};

static void st_f32le(uint8_t *p, float v)
{
    uint32_t bits = 0u;
    memcpy(&bits, &v, sizeof(bits));
    p[0] = (uint8_t)(bits & 0xFFu);
    p[1] = (uint8_t)((bits >> 8) & 0xFFu);
    p[2] = (uint8_t)((bits >> 16) & 0xFFu);
    p[3] = (uint8_t)((bits >> 24) & 0xFFu);
}

void ems04_h1win_reset(ems04_h1win_t *w)
{
    memset(w, 0, sizeof(*w));
}

int ems04_h1win_begin(ems04_h1win_t *w, uint32_t tick_first, double water_depth_m,
                      double ctrl_mode, uint8_t manual_rule)
{
    if (manual_rule != (uint8_t)EMS04_FP_MANUAL_RULE_ZERO_FILL) {
        return (int)EMS04_FP_ERR_POLICY; /* 仅 ZERO_FILL 合法 */
    }
    if (!isfinite(water_depth_m) || !isfinite(ctrl_mode)) {
        return (int)EMS04_FP_ERR_NONFINITE;
    }
    ems04_h1win_reset(w);
    w->water_depth_m = water_depth_m;
    w->ctrl_mode     = ctrl_mode;
    w->manual_rule   = manual_rule;
    w->tick_first    = tick_first;
    w->last_tick     = 0u;
    w->has_tick      = 0u;
    w->has_voyage    = 1u;
    return 0;
}

int ems04_h1win_tick(ems04_h1win_t *w, uint32_t tick_idx, const double v[EMS04_FP_N_TICK_F64])
{
    uint32_t slot;
    uint32_t i;

    if (w->has_voyage == 0u) {
        return (int)EMS04_FP_ERR_STATE; /* 未 INIT */
    }
    /* 连续性（PROTOCOL_FP §3）：首拍 == tick_first；其后逐拍 +1，其余全部 fail-closed */
    if (w->has_tick == 0u) {
        if (tick_idx != w->tick_first) {
            return (int)EMS04_FP_ERR_ORDER;
        }
    } else if (tick_idx != w->last_tick + 1u) {
        return (int)EMS04_FP_ERR_ORDER; /* duplicate / skip / out-of-order */
    }
    /* t_s 与 tick_idx 精确相等（f64 位级；NaN 亦落入此分支） */
    if (!(v[0] == (double)tick_idx)) {
        return (int)EMS04_FP_ERR_TICK_T;
    }
    /* 全部 11 字段 isfinite（含 SOC / x_ship：随帧携带、本阶段不消费但必须有限） */
    for (i = 0; i < EMS04_FP_N_TICK_F64; ++i) {
        if (!isfinite(v[i])) {
            return (int)EMS04_FP_ERR_NONFINITE;
        }
    }

    slot = tick_idx % EMS04_H1_RING;
    for (i = 0; i < EMS04_H1_N_CH; ++i) {
        w->ring[slot][i] = v[TICK2_TO_RING[i]];
    }
    w->last_tick = tick_idx;
    w->has_tick  = 1u;
    w->ticks_ok++;
    return 0;
}

int ems04_h1win_construct(ems04_h1win_t *w, uint32_t t0, uint8_t out[EMS04_H1_BYTES])
{
    uint32_t j;
    uint32_t c;

    if (w->has_tick == 0u) {
        return (int)EMS04_FP_ERR_STATE;
    }
    if (w->last_tick < (uint32_t)EMS04_H1_ROWS) {
        return (int)EMS04_FP_ERR_NOT_READY; /* 未收满 301 拍 */
    }
    if (t0 != w->last_tick) {
        /* 环深 301 = 窗长 301：仅 t0 == last_tick 时窗内 301 样本齐备 */
        return (int)EMS04_FP_ERR_NOT_READY;
    }
    /* zero-fill fail-closed（DGN-06 板侧分支：仅 AUTO 合法；MANUAL/其他 → 拒绝） */
    if (!(w->ctrl_mode == 0.0)) {
        return (int)EMS04_FP_ERR_POLICY;
    }

    for (j = 0; j < EMS04_H1_ROWS; ++j) {
        uint32_t ia = (t0 - EMS04_H1_ROWS + j) % EMS04_H1_RING; /* v(t0-300+j) */
        uint32_t ib = (t0 - (EMS04_H1_ROWS - 1u) + j) % EMS04_H1_RING; /* v(t0-299+j) */

        for (c = 0; c < EMS04_H1_COLS; ++c) {
            double bin;
            float  fbin;
            uint8_t *dst = out + (size_t)(j * EMS04_H1_COLS + c) * 4u;

            if (c == 6u) {
                /* 常量列 water_depth_m：同一中点路径（(w+w)*0.5 = w 精确） */
                bin = (w->water_depth_m + w->water_depth_m) * 0.5;
            } else if (c == 8u) {
                /* last_mode 于常量 = policy 值（合法 = 0.0 AUTO） */
                bin = w->ctrl_mode;
            } else if (c == 9u) {
                /* interval_endpoint：bin = 右端点样本值 */
                bin = w->ring[ib][H1COL_TO_RING[c]];
            } else if (c == 10u) {
                /* DGN-06 zero-fill（AUTO 分支）：manual_power_cmd_pct := 0.0（全列） */
                bin = 0.0;
            } else {
                /* time_mean 退化形式：bin = (v0 + v1) * 0.5（f64 构造，op 序冻结） */
                double v0 = w->ring[ia][H1COL_TO_RING[c]];
                double v1 = w->ring[ib][H1COL_TO_RING[c]];
                bin = (v0 + v1) * 0.5;
            }
            if (!isfinite(bin)) {
                memset(out, 0, EMS04_H1_BYTES);
                return (int)EMS04_FP_ERR_NONFINITE; /* 出口前有限性门 */
            }
            fbin = (float)bin; /* 出口 cast f32：IEEE RNE */
            if (!isfinite((double)fbin)) {
                memset(out, 0, EMS04_H1_BYTES);
                return (int)EMS04_FP_ERR_NONFINITE; /* f32 出口域有限性门（越界溢出 → 拒绝） */
            }
            st_f32le(dst, fbin);
        }
    }
    w->constructs_ok++;
    return 0;
}
