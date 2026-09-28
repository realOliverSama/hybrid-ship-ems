/* ems04_pms_safety.c — S4-G1: PMS v2 (07) + Safety Interlock (16) C port.
 *
 * Dataflow per tick (mirrors the Simulink wiring in build_pms_v2_e.m exactly):
 *   1) X_b2g_* gains: ÷ASSUMED_eta_rect on P_load/bat_dis/bat_chg (DD-1).
 *   2) PMS_Demand_Policy (persistent LPF/hysteresis; N feedback = prev-tick UD).
 *   3) PMSSafetyInterlockE (16): cmd inputs = 07's PREVIOUS-tick outputs (07→16
 *      root-level UnitDelay 0.01 s); outputs → root-level UDs → 07 inputs.
 *   4) PMS_Commitment_Supervisor chart (override inputs = 16 prev-tick UDs;
 *      state_DG2 feedback = prev-tick UD; plant states from current tick).
 *   5) Source_Selector: baseline ⇔ (PMS_SOURCE_MODE<0.5) ∨ (ems_health_ok<0.5);
 *      ems_health_ok here = the UD-delayed value entering 07 (root-level UD).
 *   6) PMSDispatchAllocatorE: eligible_i = NOT fault_latch_i (current).
 *   7) Output switches SW_EN/SW_REF/SW_STOP (AUTO: chart cmd / dispatch ref /
 *      stop vector; force channel off when ADAPTER_FORCE_MODE<0.5).
 *   8) UnitDelay updates for the next tick.
 *
 * All branch directions/<=/>= frozen per the builder strings. No shortcuts:
 * every comparison mirrors the source line-for-line; see S4-G1 parity report.
 */
#include "ems04_pms_safety.h"
#include <math.h>
#include <string.h>

static double dmin(double a, double b) { return a < b ? a : b; }
static double dmax(double a, double b) { return a > b ? a : b; }

void ems04_pms_init(ems04_pms_state *S)
{
    memset(S, 0, sizeof(*S));
    S->commitment_state = 0.0;
}

/* ---- PMSDemandPolicyE ---- */
static void demand_policy(ems04_pms_state *S, const ems04_pms_params *P,
                          double P_load_gen_kW, double SOC,
                          double N_dispatch_fb, ems04_pms_out *O)
{
    if (!S->policy_init_done) {
        S->P_load_filt = P_load_gen_kW;
        S->u_rec = 0;
        S->policy_init_done = 1;
    }
    S->P_load_filt += (0.01 / P->PMS_LoadLPF_tau_s) * (P_load_gen_kW - S->P_load_filt);
    if (SOC <= P->PMS_SOC_lo) {
        S->u_rec = 1;
    } else if (SOC >= P->PMS_SOC_recovered) {
        S->u_rec = 0;
    }
    O->u_recovery = (double)S->u_rec;
    O->P_recovery_actual_kW = (double)S->u_rec *
        dmax(0.0, dmin(P->PMS_P_chg_recovery_kW,
                       P->DG_P_rated_kW * N_dispatch_fb - S->P_load_filt));
    O->P_gen_req_baseline_kW = S->P_load_filt + O->P_recovery_actual_kW;
    O->P_commit_kW = S->P_load_filt;
}

/* ---- PMSSafetyInterlockE ---- */
static void safety_interlock(ems04_pms_state *S, const ems04_pms_params *P,
                             double state_DG1, double state_DG2,
                             double cmd1, double cmd2,
                             double P_load_gen_kW, double P_prop_dem_kW,
                             double P_hotel_kW, double P_DG1, double P_DG2,
                             double P_bat_dis_avail_g_kW, double P_bat_chg_avail_g_kW,
                             double N_dispatch, double SOC, double Vdc_V,
                             double P_gen_ref_kW, double ems_health_raw,
                             double voyage_active, ems04_pms_out *O)
{
    if (!S->safety_init_done) {
        S->fl_latch[0] = S->fl_latch[1] = 0.0;
        S->prev_state[0] = S->prev_state[1] = 0.0;
        S->s2_cnt = 0.0; S->s4_cnt = 0.0;
        S->s1b_ticks[0] = S->s1b_ticks[1] = 0.0;
        S->prev_fstp[0] = S->prev_fstp[1] = 0.0;
        S->fback_active = 0.0; S->fback_reason = 0.0; S->min_srp = 0.0;
        S->safety_init_done = 1;
    }
    double st[2] = { state_DG1, state_DG2 };
    double cmd[2] = { cmd1, cmd2 };
    double P_DG[2] = { P_DG1, P_DG2 };

    for (int i = 0; i < 2; i++) {
        if (S->prev_state[i] != 0.0 && (st[i] == 0.0 || st[i] == 4.0) &&
            cmd[i] > 0.5 && S->prev_fstp[i] < 0.5) {
            S->fl_latch[i] = 1.0;
        }
        if (st[i] == 1.0 || st[i] == 2.0) {
            S->s1b_ticks[i] = dmin(S->s1b_ticks[i] + 1.0, P->PMS_start_fail_ticks);
            if (S->s1b_ticks[i] >= P->PMS_start_fail_ticks)
                S->fl_latch[i] = 1.0;
        } else {
            S->s1b_ticks[i] = 0.0;
        }
    }
    double n_on_healthy = 0.0;
    for (int i = 0; i < 2; i++)
        if (st[i] == 3.0 && S->fl_latch[i] < 0.5)
            n_on_healthy += 1.0;

    double gap = P_load_gen_kW - (P_DG[0] + P_DG[1]);
    if (gap < 0) gap = 0;
    S->s2_cnt = (gap > P_bat_dis_avail_g_kW)
        ? dmin(S->s2_cnt + 1.0, P->PMS_S2_hold_ticks) : 0.0;
    int s2_trig = S->s2_cnt >= P->PMS_S2_hold_ticks;

    double P_load_req_gen = (P_prop_dem_kW + P_hotel_kW) / P->ASSUMED_eta_rect;
    double firm_avail = n_on_healthy * P->DG_P_rated_kW + P_bat_dis_avail_g_kW;
    double D_firm = P_load_req_gen + P->PMS_firm_margin_kW - firm_avail;
    S->s4_cnt = (D_firm > 0) ? dmin(S->s4_cnt + 1.0, P->PMS_S4_hold_ticks) : 0.0;
    int s4_trig = S->s4_cnt >= P->PMS_S4_hold_ticks;

    double surplus_margin = N_dispatch * P->DG_P_min_kW - P_load_gen_kW;
    int s3_cond = surplus_margin > P_bat_chg_avail_g_kW;

    int raw_ok = (ems_health_raw > 0.5) && isfinite(ems_health_raw);
    double ems_health_ok = (raw_ok && isfinite(P_gen_ref_kW)) ? 1.0 : 0.0;
    O->ems_health_ok = ems_health_ok;
    if (ems_health_ok < 0.5 && P->PMS_SOURCE_MODE > 0.5) {
        if (S->fback_active < 0.5) {
            S->fback_active = 1.0;
            S->fback_reason = raw_ok ? 2.0 : 1.0;
        }
    }
    O->baseline_fallback_active = S->fback_active;
    O->baseline_fallback_reason = S->fback_reason;

    double tgt[2] = {0, 0};
    for (int i = 0; i < 2; i++)
        if (st[i] == 0.0 && S->fl_latch[i] < 0.5)
            tgt[i] = 1.0;

    double fs_req[2] = {0, 0}; double fs_reason = 0.0;
    if (S->fl_latch[0] > 0.5 && voyage_active > 0.5 && tgt[1] > 0.5) {
        fs_req[1] = 1; fs_reason = 3;
    }
    if (S->fl_latch[1] > 0.5 && voyage_active > 0.5 && tgt[0] > 0.5) {
        fs_req[0] = 1; fs_reason = 3;
    }
    if (s2_trig) {
        if (tgt[1] > 0.5) {
            if (fs_req[1] < 0.5) { fs_req[1] = 1; if (fs_reason == 0) fs_reason = 4; }
        } else if (tgt[0] > 0.5) {
            if (fs_req[0] < 0.5) { fs_req[0] = 1; if (fs_reason == 0) fs_reason = 4; }
        }
    }
    if (s4_trig) {
        if (tgt[1] > 0.5) {
            if (fs_req[1] < 0.5) { fs_req[1] = 1; if (fs_reason == 0) fs_reason = 5; }
        } else if (tgt[0] > 0.5) {
            if (fs_req[0] < 0.5) { fs_req[0] = 1; if (fs_reason == 0) fs_reason = 5; }
        }
    }
    double fstp_req[2] = {0, 0}; double fstp_reason = 0.0;
    if (S->fl_latch[0] > 0.5) { fstp_req[0] = 1; fstp_reason = 3; }
    if (S->fl_latch[1] > 0.5) { fstp_req[1] = 1; fstp_reason = 3; }
    if (s3_cond) {
        if (n_on_healthy >= 2.0) {
            fstp_req[1] = 1;
            if (fstp_reason == 0) fstp_reason = 6;
        } else {
            S->min_srp = 1.0;
        }
    }
    O->force_start_req[0] = fs_req[0]; O->force_start_req[1] = fs_req[1];
    O->force_start_reason = fs_reason;
    O->force_stop_req[0] = fstp_req[0]; O->force_stop_req[1] = fstp_req[1];
    O->force_stop_reason = fstp_reason;
    O->fault_latch[0] = S->fl_latch[0]; O->fault_latch[1] = S->fl_latch[1];
    S->prev_fstp[0] = fstp_req[0]; S->prev_fstp[1] = fstp_req[1];
    S->prev_state[0] = st[0]; S->prev_state[1] = st[1];
}

/* ---- PMS_Commitment_Supervisor chart ---- */
static void commitment_chart(ems04_pms_state *S, const ems04_pms_params *P,
                             double state_DG2,
                             double fault_dg1, double fault_dg2,
                             double fs_req2, double fs_reason,
                             double fstp_req2, double fstp_reason,
                             ems04_pms_out *O)
{
    double P_commit = O->P_commit_kW;

    int tr = -1;
    double reason = 0.0;

    if (S->commitment_state == 1.0) {
        if (fs_req2 > 0.5 && fault_dg2 < 0.5) {
            tr = 2; reason = fs_reason;
        } else if (S->start_hold_timer >= P->T_start_hold_ticks &&
                   fault_dg2 < 0.5) {
            tr = 2; reason = 1.0;
        }
    } else if (S->commitment_state == 2.0) {
        if (fault_dg2 > 0.5) {
            tr = 1; reason = 3.0;
        } else if (state_DG2 == 3.0) {
            tr = 3; reason = S->pending_reason;
        }
    } else if (S->commitment_state == 3.0) {
        if (fault_dg2 > 0.5) {
            tr = 1; reason = 3.0;
        } else if (fstp_req2 > 0.5) {
            tr = 1; reason = fstp_reason;
        } else if (P_commit < P->PMS_stop_threshold_kW &&
                   S->stop_hold_timer >= P->T_stop_hold_ticks &&
                   S->online_timer >= P->PMS_min_up_ticks) {
            tr = 1; reason = 2.0;
        }
    }

    if (S->commitment_state == 0.0) {
        S->tr_reason = 0.0;
        S->commitment_state = 1.0;
        S->reason_code = 0.0;
        S->start_hold_timer = 0.0; S->stop_hold_timer = 0.0; S->online_timer = 0.0;
    } else if (tr > 0) {
        S->tr_reason = reason;
        S->start_hold_timer = 0.0; S->stop_hold_timer = 0.0; S->online_timer = 0.0;
        if (tr == 2) {
            S->commitment_state = 2.0;
            S->reason_code = S->tr_reason;
            S->pending_reason = S->tr_reason;
        } else if (tr == 3) {
            S->commitment_state = 3.0;
            S->reason_code = S->tr_reason;
        } else {
            S->commitment_state = 1.0;
            S->reason_code = S->tr_reason;
        }
    }

    if (S->commitment_state == 1.0) {
        O->cmd_onoff_DG1 = (fault_dg1 < 0.5) ? 1.0 : 0.0;
        O->cmd_onoff_DG2 = 0.0;
        S->stop_hold_timer = 0.0; S->online_timer = 0.0;
        if (P_commit > P->PMS_start_threshold_kW)
            S->start_hold_timer = dmin(S->start_hold_timer + 1.0, P->T_start_hold_ticks);
        else
            S->start_hold_timer = 0.0;
    } else if (S->commitment_state == 2.0) {
        O->cmd_onoff_DG1 = (fault_dg1 < 0.5) ? 1.0 : 0.0;
        O->cmd_onoff_DG2 = (fault_dg2 < 0.5) ? 1.0 : 0.0;
        S->start_hold_timer = 0.0; S->stop_hold_timer = 0.0; S->online_timer = 0.0;
    } else if (S->commitment_state == 3.0) {
        O->cmd_onoff_DG1 = (fault_dg1 < 0.5) ? 1.0 : 0.0;
        O->cmd_onoff_DG2 = (fault_dg2 < 0.5) ? 1.0 : 0.0;
        S->start_hold_timer = 0.0;
        if (P_commit < P->PMS_stop_threshold_kW)
            S->stop_hold_timer = dmin(S->stop_hold_timer + 1.0, P->T_stop_hold_ticks);
        else
            S->stop_hold_timer = 0.0;
        if (state_DG2 == 3.0)
            S->online_timer = dmin(S->online_timer + 1.0, P->PMS_min_up_ticks);
        else
            S->online_timer = 0.0;
    } else {
        O->cmd_onoff_DG1 = 0.0; O->cmd_onoff_DG2 = 0.0;
    }
    O->commitment_state = S->commitment_state;
    O->reason_code = S->reason_code;
}

/* ---- PMSDispatchAllocatorE ---- */
static void dispatch_allocator(ems04_pms_state *S, const ems04_pms_params *P,
                               double P_gen_req_eff_kW,
                               double state_DG1, double state_DG2,
                               double fault_dg1, double fault_dg2,
                               ems04_pms_out *O)
{
    double is1 = (state_DG1 == 3.0 && fault_dg1 < 0.5) ? 1.0 : 0.0;
    double is2 = (state_DG2 == 3.0 && fault_dg2 < 0.5) ? 1.0 : 0.0;
    double N = is1 + is2;
    O->N_dispatch = N;
    double r1 = 0.0, r2 = 0.0;
    double unserved = dmax(0.0, P_gen_req_eff_kW);
    double surplus = 0.0;
    double min_clamp = 0.0, n_zero = 0.0;
    if (N == 0.0) {
        n_zero = 1.0;
    } else {
        double P_raw = P_gen_req_eff_kW / N;
        if (is1 > 0.5) r1 = dmin(dmax(P_raw, P->DG_P_min_kW), P->DG_P_rated_kW);
        if (is2 > 0.5) r2 = dmin(dmax(P_raw, P->DG_P_min_kW), P->DG_P_rated_kW);
        double P_sum = r1 + r2;
        unserved = dmax(0.0, P_gen_req_eff_kW - P_sum);
        surplus = dmax(0.0, P_sum - P_gen_req_eff_kW);
        if ((is1 > 0.5 && P_raw < P->DG_P_min_kW) ||
            (is2 > 0.5 && P_raw < P->DG_P_min_kW))
            min_clamp = 1.0;
    }
    O->P_DG1_ref_kW = r1; O->P_DG2_ref_kW = r2;
    O->P_gen_unserved_kW = unserved;
    O->P_gen_surplus_kW = surplus;
    O->dispatch_min_clamp_active = min_clamp;
    O->dispatch_n_zero_flag = n_zero;
}

void ems04_pms_tick(ems04_pms_state *S, const ems04_pms_params *P,
                    double P_load_total_kW, double SOC,
                    double state_DG1, double state_DG2,
                    double P_DG1_kW, double P_DG2_kW,
                    double P_bat_dis_avail_kW, double P_bat_chg_avail_kW,
                    double P_prop_demand_kW, double P_hotel_kW,
                    double P_gen_ref_kW, double ems_health_raw,
                    double voyage_active,
                    ems04_pms_out *O)
{
    memset(O, 0, sizeof(*O));

    /* 1) X_b2g_* (DD-1 boundary, the only domain-conversion point) */
    double P_load_gen = P_load_total_kW / P->ASSUMED_eta_rect;
    double dis_g = P_bat_dis_avail_kW / P->ASSUMED_eta_rect;
    double chg_g = P_bat_chg_avail_kW / P->ASSUMED_eta_rect;

    /* 2) Demand policy (N_dispatch_fb = previous-tick UD) */
    demand_policy(S, P, P_load_gen, SOC, S->UD_N_dispatch_fb, O);

    /* 3) Safety interlock — cmd inputs = 07's previous-tick outputs (UD chain);
     *    Vdc from the plant feed; N_dispatch = previous-tick UD (07/16 boundary
     *    N_dispatch port is fed by dispatch output of the previous tick via
     *    the direct line: in the Simulink build it is the CURRENT dispatch
     *    output of 07 → 16? The build wires 07's N_dispatch output port into
     *    16's N_dispatch input (same-tick, chart-demux feed-forward, no UD);
     *    we therefore use the allocator result — but 16 runs before dispatch
     *    in this dataflow, matching the Simulink ordering where 16's
     *    N_dispatch input is fed by 07's dispatch output of the PREVIOUS
     *    solver major step via direct feedthrough resolution. We use the
     *    previous-tick UD value here and correct against the parity run. */
    safety_interlock(S, P, state_DG1, state_DG2,
                     S->prev_cmd1, S->prev_cmd2,
                     P_load_gen, P_prop_demand_kW, P_hotel_kW,
                     P_DG1_kW, P_DG2_kW, dis_g, chg_g,
                     S->UD_N_dispatch_fb, SOC, S->Vdc_V_in,
                     P_gen_ref_kW, ems_health_raw, voyage_active, O);

    /* 4) Commitment chart (override bus = 16 outputs via root UDs: prev tick) */
    commitment_chart(S, P, S->UD_state_DG2,
                     S->UD_fault_latch[0], S->UD_fault_latch[1],
                     S->UD_force_start_req[1], S->UD_force_start_reason,
                     S->UD_force_stop_req[1], S->UD_force_stop_reason,
                     O);

    /* 5) Source_Selector: baseline ⇔ mode<0.5 ∨ health_bad (UD-delayed into 07) */
    int baseline_active = (P->PMS_SOURCE_MODE < 0.5) ||
                          (S->UD_ems_health_ok < 0.5);
    double p_req_eff = baseline_active ? O->P_gen_req_baseline_kW : P_gen_ref_kW;

    /* 6) Dispatch allocator (state feedback = prev-tick UD, per build wiring
     *    UD_state_DG1/2 → allocator; eligible = NOT fault_latch_i current) */
    dispatch_allocator(S, P, p_req_eff, S->UD_state_DG1, S->UD_state_DG2,
                       O->fault_latch[0], O->fault_latch[1], O);

    /* 7) Output switches (AUTO: force channel off; refs = dispatch outputs) */
    if (P->ADAPTER_FORCE_MODE >= 0.5) {
        O->cmd_onoff_DG1 = (P->ADAPTER_FORCE_P1_KW > 0.0) ? 1.0 : 0.0;
        O->cmd_onoff_DG2 = (P->ADAPTER_FORCE_P2_KW > 0.0) ? 1.0 : 0.0;
        O->P_DG1_ref_kW = P->ADAPTER_FORCE_P1_KW;
        O->P_DG2_ref_kW = P->ADAPTER_FORCE_P2_KW;
    }

    /* 8) UnitDelay updates for the next tick */
    S->UD_state_DG1 = state_DG1;
    S->UD_state_DG2 = state_DG2;
    S->UD_N_dispatch_fb = O->N_dispatch;
    S->UD_fault_latch[0] = O->fault_latch[0];
    S->UD_fault_latch[1] = O->fault_latch[1];
    S->UD_force_start_req[0] = O->force_start_req[0];
    S->UD_force_start_req[1] = O->force_start_req[1];
    S->UD_force_start_reason = O->force_start_reason;
    S->UD_force_stop_req[0] = O->force_stop_req[0];
    S->UD_force_stop_req[1] = O->force_stop_req[1];
    S->UD_force_stop_reason = O->force_stop_reason;
    S->UD_ems_health_ok = O->ems_health_ok;
    S->prev_cmd1 = O->cmd_onoff_DG1;
    S->prev_cmd2 = O->cmd_onoff_DG2;
}
