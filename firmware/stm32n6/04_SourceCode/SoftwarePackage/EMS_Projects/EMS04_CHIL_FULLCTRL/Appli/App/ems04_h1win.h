/* ems04_h1win.h — EMS04-S2 B1：H1 特征窗（板侧构造机构）。
 *
 * 契约：FEATURE_PANEL_CONTRACT.md FPC-1.0 §1（canonical 1 Hz 面）、§2.3（常量通道）、
 *       §6.1（板侧构造算法伪代码）；线格式：PROTOCOL_FP.md §3（CMD_TICK2 校验）。
 *
 * 状态 = 8 条活通道 × 301 深 f64 环（槽位 = tick_idx % 301）+ 航次常量 + 会话态。
 * 内部通道序（冻结，禁重排）：
 *   [P_load_total_kW, v_ship_mps, acceleration_mps2, n_prop_rpm,
 *    P_hotel_kW, v_current_mps, v_ref_mps, P_prop_demand_kW]
 *
 * 出口 hist = 300×11 f32 LE，C-order t-major（列序 = REQUIRED_H1 冻结序）。
 * 构造域 = f64；出口 cast f32（IEEE round-to-nearest-even）。
 *
 * 环深 301 = 窗长 301 ⇒ 仅当锚点 t0 == last_tick_idx 时窗内 301 个样本齐备；
 * 其余一律 fail-closed（ERR_NOT_READY），绝不使用被覆盖槽位产出字节。
 */
#ifndef EMS04_H1WIN_H
#define EMS04_H1WIN_H

#include <stddef.h>
#include <stdint.h>

#include "ems04_fp_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_H1_N_CH    8u
#define EMS04_H1_RING    301u
#define EMS04_H1_ROWS    300u
#define EMS04_H1_COLS    11u
#define EMS04_H1_FLOATS  (EMS04_H1_ROWS * EMS04_H1_COLS)          /* 3300 */
#define EMS04_H1_BYTES   (EMS04_H1_FLOATS * 4u)                   /* 13200 */

/* 环内通道序（内部固定） */
enum {
    EMS04_H1R_P_LOAD = 0,
    EMS04_H1R_V_SHIP,
    EMS04_H1R_ACCEL,
    EMS04_H1R_N_PROP,
    EMS04_H1R_P_HOTEL,
    EMS04_H1R_V_CURRENT,
    EMS04_H1R_V_REF,
    EMS04_H1R_P_PROP_DEMAND
};

typedef struct {
    double   ring[EMS04_H1_RING][EMS04_H1_N_CH];
    double   water_depth_m;   /* 航次标量（VOYAGE_INIT）→ H1#6 常量列 */
    double   ctrl_mode;       /* policy 常量（合法 = 0.0 AUTO）→ H1#8 常量列 */
    uint8_t  manual_rule;     /* 0 = ZERO_FILL（唯一合法）→ H1#10 */
    uint32_t tick_first;      /* 会话首拍（VOYAGE_INIT 声明） */
    uint32_t last_tick;
    uint8_t  has_tick;
    uint8_t  has_voyage;
    uint64_t ticks_ok;
    uint64_t constructs_ok;
} ems04_h1win_t;

void ems04_h1win_reset(ems04_h1win_t *w);

/* 航次初始化（VOYAGE_INIT）：清空环 + 落常量；返回 0 / err 码。 */
int ems04_h1win_begin(ems04_h1win_t *w, uint32_t tick_first, double water_depth_m,
                      double ctrl_mode, uint8_t manual_rule);

/* CMD_TICK2 校验 + 写入环（PROTOCOL_FP §3 逐条）。返回 0 / err 码；失败不推进状态。 */
int ems04_h1win_tick(ems04_h1win_t *w, uint32_t tick_idx, const double v[EMS04_FP_N_TICK_F64]);

/* 锚点 t0 构造 hist（契约 §6.1）→ 13200 B。返回 0 / err 码；失败零字节输出。 */
int ems04_h1win_construct(ems04_h1win_t *w, uint32_t t0, uint8_t out[EMS04_H1_BYTES]);

/* H1 列序（冻结，供报告/自检使用） */
extern const char *const ems04_h1_col_names[EMS04_H1_COLS];

#ifdef __cplusplus
}
#endif

#endif /* EMS04_H1WIN_H */
