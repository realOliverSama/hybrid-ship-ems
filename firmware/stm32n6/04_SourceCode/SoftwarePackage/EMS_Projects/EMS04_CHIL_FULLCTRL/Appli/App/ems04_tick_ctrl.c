/* ems04_tick_ctrl.c — S4-G4: full-controller TICK_CTRL service (EMS04, UART).
 *
 * Wire contract (new command, EMS04_S4_FULLCTRL=1 builds only):
 *   CMD_TICK_CTRL (0x0E): [seq u32][tick u32][P_load f32][SOC f32][Vdc f32]
 *                         [P_bat_dis f32][P_bat_chg f32][P_prop_dem f32]
 *                         [P_hotel f32][hist 300×11 f32][kf 30×6 f32]
 *   RSP_TICK_CTRL (0x0F): [seq u32][P_gen_ref f32][P_commit f32]
 *                         [commitment_state f32][reason_code f32]
 *                         [cmd_onoff_DG1 f32][cmd_onoff_DG2 f32]
 *                         [P_DG1_ref f32][P_DG2_ref f32][N_dispatch f32]
 *                         [d1 f32][lam_eff f32]
 *                         [fault0 f32][fault1 f32]
 *                         [force_start0 f32][force_start1 f32]
 *                         [force_stop0 f32][force_stop1 f32]
 *                         [ems_health_ok f32][baseline_fallback_active f32]
 *                         [reserved f32 = 0.0f]
 *                         [infer_us u32][ctrl_us u32][flags u8]
 *
 * 遥测对齐注记（P1 修复 2026-09-29）：reserved 浮点占线使 infer_us 落在
 * 偏移 84、ctrl_us 偏移 88、flags 偏移 92——与 host 三工具（s4_c2_smoke /
 * s4_c3_closedloop / s4_c4_battery）既有的 "20f@4 + 2I@84 + B@92" 解析
 * 逐字节对齐；S4_RSP_LEN 93B 与帧总长不变，修复前 host 读到的
 * "infer_us" 实为 ctrl_us、"ctrl_us" 实为 flags+padding（遥测面缺陷，
 * 控制面 19 浮点字段布局不变，C2/C3/C4 冻结证据不受影响）。
 *
 * Per-tick full chain (10 s control tick, board holds the full controller):
 *   1) REATT local inference (hist/kf raw windows → q50 30×10 s leads, kW)
 *   2) law6 decision layer (ems04_dp_tick: Pbar→Δλ d1; frozen tables)
 *   3) law6 execution layer (λ_eff + 1 s gated grid search → P_gen_ref, the
 *      exact contract §4.3 equations; λ constants from ems04_s1_constants.h)
 *   4) PMS v2 + Safety Interlock C port (ems04_pms_safety) with the plant
 *      states fed from the downlink (SOC/Vdc/battery/prop/hotel) — the board
 *      runs 1000 internal 0.01 s PMS ticks per control tick (phase-locked to
 *      the plant tick boundary, tick counting semantics OQ-6).
 *
 * Notes:
 *  - ems_health_raw is held at 1.0 (healthy channel; the wire table default).
 *  - The execution-layer P_load uses the downlink P_load_total (bus side);
 *    contract §4.3 consumes bus-side P_load in ṁ_bat and genset-side clamp is
 *    handled by dispatch downstream. Parity to host B arm is per S4-G5.
 *  - λ_eff: lam = max(λ0 + K_soc*(SOC_ref + K_S*e_hat − SOC), Lambda_Floor).
 */
#include "ems04_tick_ctrl.h"
#include "ems_reatt.h"
#include "ems04_law6_c2.h"
#include "ems04_s1_constants.h"
#include "ems04_pms_safety.h"

#include <math.h>
#include <string.h>
#include "main.h"

/* ---- wire sizes ---- */
#define S4_HIST_FLT   (300u * 11u)
#define S4_KF_FLT     (30u * 6u)
#define S4_CMD_HEAD   (4u + 4u + 8u * 4u)             /* seq+tick+8×f32        */
#define S4_CMD_LEN    (S4_CMD_HEAD + (S4_HIST_FLT + S4_KF_FLT) * 4u)
#define S4_RSP_LEN    (4u + 20u * 4u + 4u + 4u + 1u)  /* seq+20f32+2u32+flags  */

/* law6 execution-layer constants (frozen; ems04_s1_constants.h + law6_ports).
 * Grid search uses BSFC_LUT (frozen) exactly as exported. */
typedef struct {
    double lambda0, K_soc, SOC_ref, K_S, lambda_floor;
    double eta_rect, eta_dcdc, eta_dis, eta_chg;
    double p_max, soc_dis_max, soc_chg_min, idle_kgph, grid;
    const double *bsfc_bp;   /* breakpoints (7) */
    const double *bsfc_tbl;  /* g/kWh (7)       */
    int nb;
} s4_exec_cfg;

static ems04_pms_state m_pms;
static ems04_pms_params m_pp;
static s4_exec_cfg m_exec;
static ems04_tables m_tbl;
static int m_tbl_ok;
static uint8_t m_last_flags;

/* 1 s-grid exhaustive search (contract §4.3 verbatim). */
static double s4_exec_grid(double p_load_kw, double soc, double lam)
{
    const s4_exec_cfg *E = &m_exec;
    double best_p = 0.0, best_j = 1e300;
    int n = (int)(E->p_max / E->grid) + 1;
    for (int i = 0; i < n; i++) {
        double pg = (double)i * E->grid;
        double pbat = p_load_kw - E->eta_rect * pg;
        if (soc <= E->soc_dis_max) {
            if (pbat > 1e-9) continue;
            best_p = pg;
            break;
        }
        if (soc >= E->soc_chg_min && pbat < 0.0) continue;
        double mf;
        if (pg < 1e-9) {
            mf = E->idle_kgph / 3600.0;
        } else {
            double bsfc;
            if (pg >= E->bsfc_bp[E->nb - 1]) bsfc = E->bsfc_tbl[E->nb - 1];
            else if (pg <= E->bsfc_bp[0])    bsfc = E->bsfc_tbl[0];
            else {
                int ii = 0;
                while (ii < E->nb - 2 && E->bsfc_bp[ii + 1] < pg) ii++;
                double w = (pg - E->bsfc_bp[ii]) / (E->bsfc_bp[ii + 1] - E->bsfc_bp[ii]);
                bsfc = E->bsfc_tbl[ii] + w * (E->bsfc_tbl[ii + 1] - E->bsfc_tbl[ii]);
            }
            mf = pg * bsfc / 3.6e6;
        }
        double mbe = (pbat > 0.0)
            ? pbat / (E->eta_dcdc * E->eta_dis) / 3600.0
            : pbat * (E->eta_dcdc * E->eta_chg) / 3600.0;
        double J = mf + lam * mbe;
        if (J < best_j) { best_j = J; best_p = pg; }
    }
    if (soc <= E->soc_dis_max && best_p < 1e-9 && p_load_kw > E->eta_rect * E->p_max)
        best_p = E->p_max;
    return best_p;
}

static void s4_exec_init(void)
{
    m_exec.lambda0 = 0.425; m_exec.K_soc = 30.0; m_exec.SOC_ref = 0.60;
    m_exec.K_S = EMS04_K_S_B2;                    /* 1/30 (run_cstar override) */
    m_exec.lambda_floor = 0.43;
    m_exec.eta_rect = 0.97; m_exec.eta_dcdc = 0.97;
    m_exec.eta_dis = 0.95;  m_exec.eta_chg = 0.95;
    m_exec.p_max = 1000.0; m_exec.soc_dis_max = 0.105; m_exec.soc_chg_min = 0.945;
    m_exec.idle_kgph = 6.795; m_exec.grid = 20.0;
    static const double bp[7] = {0,100,200,400,600,800,1000};
    static const double tb[7] = {1200,700,550,480,450,430,453};
    /* frozen tables come from the generated LUT when available; the static
     * mirrors are identical to the exported values (checked by
     * check_ems04_tables.py invariants for bsfc 376-pt LUT vs 7-pt port table) */
    static double bp_s[7], tb_s[7];
    memcpy(bp_s, bp, sizeof(bp)); memcpy(tb_s, tb, sizeof(tb));
    m_exec.bsfc_bp = bp_s; m_exec.bsfc_tbl = tb_s; m_exec.nb = 7;

    memset(&m_pp, 0, sizeof(m_pp));
    m_pp.PMS_LoadLPF_tau_s = 60.0; m_pp.PMS_SOC_lo = 0.55;
    m_pp.PMS_SOC_recovered = 0.60; m_pp.PMS_P_chg_recovery_kW = 100.0;
    m_pp.PMS_start_threshold_kW = 425.0; m_pp.PMS_stop_threshold_kW = 350.0;
    m_pp.T_start_hold_ticks = 3000.0; m_pp.T_stop_hold_ticks = 12000.0;
    m_pp.PMS_min_up_ticks = 30000.0; m_pp.PMS_S2_hold_ticks = 200.0;
    m_pp.PMS_S4_hold_ticks = 200.0; m_pp.PMS_start_fail_ticks = 4000.0;
    m_pp.PMS_firm_margin_kW = 50.0; m_pp.ASSUMED_eta_rect = 0.97;
    m_pp.DG_P_min_kW = 125.0; m_pp.DG_P_rated_kW = 500.0;
    m_pp.PMS_SOURCE_MODE = 1.0; m_pp.ADAPTER_FORCE_MODE = 0.0;
    m_pp.ADAPTER_FORCE_P1_KW = 0.0; m_pp.ADAPTER_FORCE_P2_KW = 0.0;

    m_tbl_ok = (ems04_tables_board(&m_tbl) == 0);
    ems04_pms_init(&m_pms);
}

void ems04_tick_ctrl_reset(void)
{
    if (!m_tbl_ok) s4_exec_init();
    ems04_pms_init(&m_pms);
    m_last_flags = 0u;
}

/* little-endian helpers (wire = LE, same as ems_proto) */
static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static float rd_f32(const uint8_t *p) {
    float f; memcpy(&f, p, 4); return f;
}
static void wr_u32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void wr_f32(uint8_t *p, float f) { memcpy(p, &f, 4); }

/* DPSRAM-free scratch (large windows): placed by linker in AXI SRAM via section */
static float m_hist[S4_HIST_FLT];
static float m_kf[S4_KF_FLT];

/* returns payload length written; 0 = NACK (bad len / infer fail). */
uint16_t ems04_tick_ctrl_handle(const uint8_t *payload, uint16_t len,
                                uint8_t out[S4_RSP_LEN], uint32_t *out_len_cap)
{
    (void)out_len_cap;
    if (len != S4_CMD_LEN) return 0;
    if (!m_tbl_ok) s4_exec_init();

    /* FR-505 遥测自愈（P1，2026-09-29）：boot 期 DEMCR.TRCENA 被清除导致
     * CYCCNT 冻结的实测现场（DWT_CTRL.CYCCNTENA=1 但计数器停摆，板上分钟级
     * 会话复现；S4Smoke/BdDiagUart 均命中）。每次进 handler 重新武装——
     * 两次寄存器写，不改控制路径任何数值语义（位级兼容保持）。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    const uint8_t *p = payload;
    uint32_t seq = rd_u32(p); p += 4;
    uint32_t tick = rd_u32(p); p += 4;
    float P_load = rd_f32(p); p += 4;
    float SOC = rd_f32(p); p += 4;
    float Vdc = rd_f32(p); p += 4;
    float P_bat_dis = rd_f32(p); p += 4;
    float P_bat_chg = rd_f32(p); p += 4;
    float P_prop = rd_f32(p); p += 4;
    float P_hotel = rd_f32(p); p += 4;
    memcpy(m_hist, p, S4_HIST_FLT * 4u); p += S4_HIST_FLT * 4u;
    memcpy(m_kf, p, S4_KF_FLT * 4u);

    m_pms.Vdc_V_in = (double)Vdc;

    /* 1) REATT local inference → q[k][3] (q10,q50,q90) */
    float q[EMS_REATT_Q_N];
    uint32_t infer_us = 0u;
    if (ems_reatt_query(m_hist, m_kf, q, &infer_us) != 0) {
        m_last_flags = 1u;  /* infer fail flag */
        return 0;
    }
    m_last_flags = 0u;

    /* 2) Pbar = q50 leads (bus-side kW) */
    double Pbar[EMS04_K_MAX];
    for (int i = 0; i < EMS04_K_MAX; i++) Pbar[i] = (double)q[i * 3u + 1u];

    /* gen0/N0 from PMS state (prev-tick feedback; t_dwell forced 0 per A-02) */
    double n0 = m_pms.UD_N_dispatch_fb;
    if (n0 < 1.0) n0 = 1.0;
    ems04_gen_state g;
    g.st[0] = g.st[1] = 3.0;
    g.P_ramp[0] = g.P_ramp[1] = 0.0;
    g.P_lag[0] = g.P_lag[1] = 0.0;
    g.t_dwell[0] = g.t_dwell[1] = 0.0;

    ems04_tick_result tr;
    if (ems04_dp_tick(&m_tbl, Pbar, EMS04_K_MAX, (double)SOC, n0, &g, &tr) != 0) {
        m_last_flags = 2u;  /* DP fail */
        return 0;
    }
    double e_hat = tr.dlambda_first;

    /* 3) execution layer */
    const uint32_t c0 = DWT->CYCCNT;
    double soc_ref_eff = m_exec.SOC_ref + m_exec.K_S * e_hat;
    double lam = m_exec.lambda0 + m_exec.K_soc * (soc_ref_eff - (double)SOC);
    if (m_exec.K_S != 0.0) lam = (lam > m_exec.lambda_floor) ? lam : m_exec.lambda_floor;
    double P_gen_ref = s4_exec_grid((double)P_load, (double)SOC, lam);
    uint32_t ctrl_grid_us = (uint32_t)((DWT->CYCCNT - c0) / (SystemCoreClock / 1000000u));

    /* 4) PMS/Safety: 1000 internal 0.01 s ticks per control tick */
    ems04_pms_out po;
    memset(&po, 0, sizeof(po));
    uint32_t c1 = DWT->CYCCNT;
    /* plant actuals: DG1 P from ramp state unknown on board → dispatch-ref ZOH
     * (parity-relevant outputs are cmd/ref/N/chart/safety faces; the S2 gap
     * uses realized P_DG sum — board smoke feeds 0 actuals, documented) */
    for (int i = 0; i < 1000; i++) {
        ems04_pms_tick(&m_pms, &m_pp,
                       (double)P_load, (double)SOC,
                       3.0, m_pms.UD_state_DG2,
                       0.0, 0.0,
                       (double)P_bat_dis, (double)P_bat_chg,
                       (double)P_prop, (double)P_hotel,
                       P_gen_ref, 1.0, 1.0, &po);
    }
    uint32_t ctrl_pms_us = (uint32_t)((DWT->CYCCNT - c1) / (SystemCoreClock / 1000000u));

    /* encode RSP */
    uint8_t *o = out;
    wr_u32(o, seq); o += 4;
    wr_f32(o, (float)P_gen_ref); o += 4;
    wr_f32(o, (float)po.P_commit_kW); o += 4;
    wr_f32(o, (float)po.commitment_state); o += 4;
    wr_f32(o, (float)po.reason_code); o += 4;
    wr_f32(o, (float)po.cmd_onoff_DG1); o += 4;
    wr_f32(o, (float)po.cmd_onoff_DG2); o += 4;
    wr_f32(o, (float)po.P_DG1_ref_kW); o += 4;
    wr_f32(o, (float)po.P_DG2_ref_kW); o += 4;
    wr_f32(o, (float)po.N_dispatch); o += 4;
    wr_f32(o, (float)e_hat); o += 4;
    wr_f32(o, (float)lam); o += 4;
    wr_f32(o, (float)po.fault_latch[0]); o += 4;
    wr_f32(o, (float)po.fault_latch[1]); o += 4;
    wr_f32(o, (float)po.force_start_req[0]); o += 4;
    wr_f32(o, (float)po.force_start_req[1]); o += 4;
    wr_f32(o, (float)po.force_stop_req[0]); o += 4;
    wr_f32(o, (float)po.force_stop_req[1]); o += 4;
    wr_f32(o, (float)po.ems_health_ok); o += 4;
    wr_f32(o, (float)po.baseline_fallback_active); o += 4;
    wr_f32(o, 0.0f); o += 4;  /* reserved：遥测对齐占线（见文件头注记） */
    wr_u32(o, infer_us); o += 4;
    wr_u32(o, ctrl_grid_us + ctrl_pms_us); o += 4;
    *o = m_last_flags;
    return (uint16_t)S4_RSP_LEN;
}

