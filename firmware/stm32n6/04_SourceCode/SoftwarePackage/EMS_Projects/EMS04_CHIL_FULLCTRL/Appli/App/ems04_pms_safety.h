/* ems04_pms_safety.h — S4-G1: PMS v2 (07) + Safety Interlock (16) C port, host parity.
 *
 * Contract: Model E Phase 4-B sources (single-source strings, read-only):
 *   +model/code_pms_demand_e.m        PMSDemandPolicyE      (LPF + SOC hysteresis + recovery)
 *   +model/build_commitment_chart_e.m PMS_Commitment_Supervisor (Stateflow 3-state chart)
 *   +model/code_dispatch_e.m          PMSDispatchAllocatorE (equal-share allocator)
 *   +model/code_safety_interlock_e.m  PMSSafetyInterlockE   (S1/S1b/S2/S4/S3/S5 + overrides)
 * Parameter values: config/ModelE_Control_Parameters.m + ModelE_Physics_Parameters.m §5
 * (passed in by the caller as doubles; no literals re-declared here beyond dt=0.01
 *  which the sources hard-code as the discrete tick convention).
 *
 * Discrete convention: one call = 1 tick = 0.01 s (OQ-6). State carried in
 * ems04_pms_state; UnitDelay(0.01s) semantics for state_DG feedback, override bus
 * and N_dispatch_fb are reproduced by this struct (caller advances one plant tick
 * per call; the board loop calls this at 0.01 s logical cadence per control tick).
 *
 * Bit-domain: all doubles; comparisons/branches mirror the MATLAB sources
 * line-for-line (<= / >= / < / > directions frozen; see per-block notes).
 */
#ifndef EMS04_PMS_SAFETY_H
#define EMS04_PMS_SAFETY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_PMS_NPAR 8

/* Frozen parameters (values from ModelE_Control_Parameters.m / Physics §5). */
typedef struct {
    double PMS_LoadLPF_tau_s;        /* 60   */
    double PMS_SOC_lo;               /* 0.55 */
    double PMS_SOC_recovered;        /* 0.60 */
    double PMS_P_chg_recovery_kW;    /* 100  */
    double PMS_start_threshold_kW;   /* 425  (alpha_start 0.85 × DG_P_rated 500) */
    double PMS_stop_threshold_kW;    /* 350  (alpha_stop  0.70 × DG_P_rated 500) */
    double T_start_hold_ticks;       /* 3000 (30 s / 0.01 s)                     */
    double T_stop_hold_ticks;        /* 12000(120 s / 0.01 s)                    */
    double PMS_min_up_ticks;         /* 30000(300 s / 0.01 s)                    */
    double PMS_S2_hold_ticks;        /* 200  (2 s)                               */
    double PMS_S4_hold_ticks;        /* 200  (2 s)                               */
    double PMS_start_fail_ticks;     /* 4000 (40 s)                              */
    double PMS_firm_margin_kW;       /* 50                                       */
    double ASSUMED_eta_rect;         /* 0.97                                     */
    double DG_P_min_kW;              /* 125                                      */
    double DG_P_rated_kW;            /* 500                                      */
    double PMS_SOURCE_MODE;          /* 1 = EMS_AGGREGATE (nominal)              */
    double ADAPTER_FORCE_MODE;       /* 0 = AUTO (bench force channel off)       */
    double ADAPTER_FORCE_P1_KW;      /* 0                                        */
    double ADAPTER_FORCE_P2_KW;      /* 0                                        */
} ems04_pms_params;

/* Controller state (persistent blocks + Stateflow data + UnitDelays). */
typedef struct {
    /* PMSDemandPolicyE persistents */
    double P_load_filt;
    int    u_rec;
    int    policy_init_done;
    /* Commitment chart (Stateflow data) */
    double commitment_state;         /* 0 pre-entry, 1 ONE_DG, 2 PENDING, 3 TWO_DG */
    double reason_code;
    double start_hold_timer, stop_hold_timer, online_timer;
    double pending_reason;
    double tr_reason;                /* local, carried across entry actions */
    int    chart_entered;            /* default transition done */
    /* Safety InterlockE persistents */
    double fl_latch[2], prev_state[2], prev_fstp[2];
    double s2_cnt, s4_cnt, s1b_ticks[2];
    double fback_active, fback_reason, min_srp;
    int    safety_init_done;
    /* UnitDelays (0.01 s) at 07 boundary */
    double UD_state_DG1, UD_state_DG2;       /* plant state feedback, IC=0      */
    double UD_N_dispatch_fb;                 /* dispatch N feedback, IC=0       */
    double UD_fault_latch[2];                /* override bus DG1/DG2, IC=0      */
    double UD_force_start_req[2];
    double UD_force_start_reason;
    double UD_force_stop_req[2];
    double UD_force_stop_reason;
    double UD_ems_health_ok;                 /* IC=0 (matches Simulink default) */
    /* 16 的 cmd_onoff 输入 = 07 上一拍输出（07→16 经根级 UD 0.01s；根级 override
     * 链 fault/force_* 亦为 UD——已并入上方 UD_* 字段）。IC=0。 */
    double prev_cmd1, prev_cmd2;
    double Vdc_V_in;                          /* 植物母线电压（当拍喂入） */
} ems04_pms_state;

/* Per-tick outputs (subset consumed by the closed loop / telemetry). */
typedef struct {
    double P_gen_req_baseline_kW, P_commit_kW, u_recovery, P_recovery_actual_kW;
    double cmd_onoff_DG1, cmd_onoff_DG2;
    double P_DG1_ref_kW, P_DG2_ref_kW;
    double N_dispatch;
    double P_gen_unserved_kW, P_gen_surplus_kW;
    double dispatch_min_clamp_active, dispatch_n_zero_flag;
    double commitment_state, reason_code;
    double force_start_req[2], force_start_reason;
    double force_stop_req[2], force_stop_reason;
    double fault_latch[2];
    double ems_health_ok, baseline_fallback_active, baseline_fallback_reason;
} ems04_pms_out;

/* Reset all state to Simulink ICs (persistent isempty() equivalents + UD ICs). */
void ems04_pms_init(ems04_pms_state *S);

/* One 0.01 s tick. Inputs:
 *   P_load_total_kW   bus-side total load (07 root in, ÷eta_rect inside X_b2g)
 *   SOC               battery SOC
 *   state_DG1/2       plant genset states (current tick, from PlantStepE outputs)
 *   P_DG1/2_kW        plant genset actual power
 *   P_bat_dis_avail_kW, P_bat_chg_avail_kW  bus-side battery availabilities
 *   P_prop_demand_kW, P_hotel_kW            bus-side demands (S4 formula)
 *   P_bat_dis_avail_g_kW                    genset-side availability (÷η already
 *                    applied at 07 boundary? NO — sources: dis/chg g-ports come
 *                    from X_b2g gains; caller passes BUS-side values here and
 *                    the port applies 1/eta_rect, mirroring X_b2g_* exactly)
 *   P_gen_ref_kW      root inport (EMS aggregate request)
 *   ems_health_raw    health channel (EMS_HEALTH_TS table value)
 *   voyage_active     1 while voyage running
 */
void ems04_pms_tick(ems04_pms_state *S, const ems04_pms_params *P,
                    double P_load_total_kW, double SOC,
                    double state_DG1, double state_DG2,
                    double P_DG1_kW, double P_DG2_kW,
                    double P_bat_dis_avail_kW, double P_bat_chg_avail_kW,
                    double P_prop_demand_kW, double P_hotel_kW,
                    double P_gen_ref_kW, double ems_health_raw,
                    double voyage_active,
                    ems04_pms_out *O);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_PMS_SAFETY_H */
