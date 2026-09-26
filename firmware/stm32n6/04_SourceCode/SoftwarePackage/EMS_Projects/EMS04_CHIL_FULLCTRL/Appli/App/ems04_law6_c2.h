/* ems04_law6_c2.h — law6 (Oracle ECMS) C2-arm per-decision-tick control law, C port.
 *
 * EMS04 S1 (host-only parity). Contract: LAW6_C2_CONSUMPTION_CONTRACT.md (same dir).
 * Frozen constants: exported/constants.json + policy_table_Pg.f64 + bsfc_lut.f64
 * (exported by export_constants_table.m from the SAME frozen MATLAB entry points
 *  used by the formal runner; the C side MUST NOT rebuild the policy table).
 *
 * Bit-level semantics mirror (read-only analysis, no .slx change):
 *   +t445/b2_window_dp.m   decision layer  (per 10 s tick, action = delta-lambda)
 *   +t445/law6_lambda_eff.m / CODE_ECMS_ORACLE (build_model_e.m)
 *                          execution layer (lambda_eff + 1 s gated grid search)
 * Deviations: none intended; known MATLAB-inherited approximations are
 * reproduced verbatim (see ASSUMPTION A-02/A-03 in the contract).
 */
#ifndef EMS04_LAW6_C2_H
#define EMS04_LAW6_C2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- frozen scalar constants (generated header, from exported/constants.json) */
#include "ems04_s1_constants.h"

#define EMS04_N_SOC   EMS04_SOC_GRID_N   /* 86 */
#define EMS04_N_DLAM  EMS04_DLAM_N       /* 41 */
#define EMS04_N_LAM   EMS04_N_LAMBDA     /* 3020 */
#define EMS04_N_PBAR  EMS04_N_PBAR_GRID  /* 66 */
#define EMS04_N_REG   3
#define EMS04_K_MAX   30

/* genset realization state, mirroring t445 local_realize's `gen` struct
 * (st: 0 OFF, 1 STARTING, 2 SYNCHRONIZING, 3 ON, 4 COOLDOWN; doubles on
 * purpose to keep bit-level parity with the MATLAB reference). */
typedef struct {
    double st[2];
    double P_ramp[2];
    double P_lag[2];
    double t_dwell[2];
} ems04_gen_state;

/* loaded frozen tables + scalars
 *
 * Pg storage (switch design preserved from S1/S1.5):
 *   default (host replay) : T->Pg points at a loaded f64 table.
 *   PG_F32  (S1.5 probe)  : T->Pg points at f32->f64 promoted table.
 *   EMS04_PG_PALETTE (board): T->Pg is unused; access goes through the
 *      generated palette ems04_pg_palette[ems04_pg_index[idx]] (51 unique
 *      frozen dispatch values, u8 index over 597960 entries; 584 KiB instead
 *      of 4.56 MiB). Bit-level equivalence holds by construction (the frozen
 *      Pg values are exactly the 51 palette doubles) and is verified
 *      element-wise by Host/tools/check_ems04_tables.py (597960/597960). */
typedef struct {
    const double *Pg;        /* file paths only: EMS04_N_LAM*EMS04_N_PBAR*EMS04_N_REG,
                                MATLAB column-major:
                                idx = il + (ip-1)*nL + (ir-1)*nL*nP  (0-based in C) */
    const double *bsfc_lut;  /* EMS04_N_BSFC points, P = 125:1:500 kW, pchip samples */
    /* SOC/action grids: bit-identical to the MATLAB colon vectors (board:
     * generated const arrays; host: loaded from exported f64 dumps, NOT
     * formula-generated: colon's symmetric algorithm differs from x0+i*h in
     * 38/171 soc elements and 11/41 dlam elements). */
    const double *socx;      /* EMS04_N_SOC values spanning 0.1..0.95 */
    const double *dlam;      /* EMS04_N_DLAM values spanning -0.1..+0.1 */
    double *socx_owned;      /* backing store (host file path only) */
    double *dlam_owned;
} ems04_tables;

/* Board table binding (EMS04_PG_PALETTE): wires the tables struct to the
 * generated const arrays in ems04_tables_gen.c/.h. No allocation, no I/O.
 * Returns 0 on success. */
int ems04_tables_board(ems04_tables *t);

#if defined(EMS04_PG_PALETTE)
#include "ems04_tables_gen.h"
/* Palette accessor: identical double bits as the original f64 table entry
 * (verified offline element-wise), so every downstream operation is
 * bit-identical to the host replay. */
static inline double ems04_pg_at(const ems04_tables *t, size_t idx) {
    (void)t;
    return ems04_pg_palette[ems04_pg_index[idx]];
}
#else
static inline double ems04_pg_at(const ems04_tables *t, size_t idx) {
    return t->Pg[idx];
}
#endif

/* per-tick diagnostics (mirrors b2_window_dp sol fields used for parity) */
typedef struct {
    double  dlambda_first;   /* d1: the only value entering the execution layer */
    double  J_window_kg;     /* fuel_proxy + v_term(soc_end) (informational) */
    double  soc_end;
    double  fuel_proxy_kg;
    int64_t oob_query_count;
    int64_t infeasible_fallback_n;
    int     n_fallback_steps;
    double  dlambda_all[EMS04_K_MAX];  /* debug: full forward action sequence */
    double  soc_path[EMS04_K_MAX + 1]; /* debug: DP-internal soc trajectory  */
    double  N_path[EMS04_K_MAX + 1];   /* debug: DP-internal N trajectory    */
} ems04_tick_result;

/* Load the two .f64 tables from the exported dir. Returns 0 on success. */
int ems04_load_tables(const char *exported_dir, ems04_tables *t);

/* Decision layer: full b2_window_dp for one 10 s tick, returns dlambda(1).
 * Inputs (contract §4.1): Pbar[0..K-1] bus-side kW; SOC0; N0 (1..2);
 * gen (t_dwell forced [0,0] by the caller, mirroring gen0_from_ds).
 * SOC_target fixed 0.60 (run_cstar hard-codes it).
 * Returns 0 on success. */
int ems04_dp_tick(const ems04_tables *T, const double *Pbar, int K,
                  double SOC0, double N0, const ems04_gen_state *gen0,
                  ems04_tick_result *out);

/* Execution-layer lambda law (contract §4.3), pure function.
 * lam = max(lambda0 + K_soc*(SOC_ref + K_S*e_hat - SOC), floor)  (K_S != 0). */
double ems04_law6_lambda_eff(double SOC, double e_hat);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_LAW6_C2_H */
