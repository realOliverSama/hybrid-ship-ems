/* ems04_law6_c2.c — bit-level C port of the law6 C2 decision/execution law.
 * Contract: LAW6_C2_CONSUMPTION_CONTRACT.md §4 (equations) / §5 (tables).
 * Reference (read-only): +t445/b2_window_dp.m, +t445/law6_lambda_eff.m,
 * +model/build_model_e.m CODE_ECMS_ORACLE. Known MATLAB-inherited
 * approximations are reproduced verbatim (A-02 t_dwell reset, A-03 lambda
 * equivalence). No design changes.
 *
 * EMS04 board copy: Pg access goes through ems04_pg_at() (header). With
 * EMS04_PG_PALETTE defined (this project always defines it — see Appli/
 * CMakeLists.txt) the board serves the frozen table from the generated
 * palette + u8 index (ems04_tables_gen.c); the file loaders below and the
 * PG_F32 probe path are compiled out on board. Bit-level equivalence of the
 * palette path is verified offline by Host/tools/check_ems04_tables.py.
 */
#include "ems04_law6_c2.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- small helpers (MATLAB semantics) ---- */

/* MATLAB round: half away from zero. C round() is the same. */
static double m_round(double x) { return round(x); }

static double m_min(double a, double b) { return a < b ? a : b; }
static double m_max(double a, double b) { return a > b ? a : b; }

/* interp1(soc_grid, V, x, 'linear', BIG) — bit-level port of the MATLAB
 * R2026a implementation (coder.internal.interpolate.interp1LinearLoopBody):
 *   bracket via binary search on the actual grid values,
 *   r = (x - x[n]) / (x[n+1] - x[n])   (adjacent stored-value difference),
 *   r==0 -> y[n]; r==1 -> y[n+1]; y1==y2 -> y1;
 *   otherwise (1-r)*y1 + r*y2 (convex combination, NOT the increment form).
 * Outside [x[0], x[n-1]] -> extrapval. This closes the ULP gap that made
 * deep backward-value iterations diverge (see beat_debug_diff.json). */
static double interp_uniform(const double *V, const double *X, int n,
                             double x, double extrap) {
    if (x < X[0] || x > X[n - 1]) return extrap;
    int lo = 0, hi = n - 1;
    /* last index with X[i] <= x (upper-bound binary search minus one) */
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (X[mid] <= x) lo = mid; else hi = mid - 1;
    }
    if (lo > n - 2) lo = n - 2;
    double r = (x - X[lo]) / (X[lo + 1] - X[lo]);
    if (r == 0.0) return V[lo];
    if (r == 1.0) return V[lo + 1];
    double y1 = V[lo], y2 = V[lo + 1];
    if (y1 == y2) return y1;
    double onemr = 1.0 - r;
    return onemr * y1 + r * y2;
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ---- table binding ---- */

#if defined(EMS04_PG_PALETTE)

int ems04_tables_board(ems04_tables *t) {
    t->Pg = NULL; /* unused on the palette path */
    t->bsfc_lut = ems04_bsfc_lut;
    t->socx = ems04_soc_grid;
    t->dlam = ems04_dlam_grid;
    t->socx_owned = NULL;
    t->dlam_owned = NULL;
    if (!(t->socx[0] == EMS04_SOC_GRID_LO) ||
        !(t->socx[EMS04_N_SOC - 1] > 0.94 && t->socx[EMS04_N_SOC - 1] < 0.96))
        return -2; /* sanity: grid must span 0.1..0.95 */
    return 0;
}

#else /* host replay paths (f64 file load / PG_F32 probe), unchanged from S1 */

static double *read_f64(const char *path, size_t expect_bytes) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (expect_bytes && (size_t)sz != expect_bytes) { fclose(f); return NULL; }
    double *buf = (double *)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); free(buf); return NULL;
    }
    fclose(f);
    return buf;
}

#ifdef PG_F32
/* S1.5 Task B probe (memory-plan evidence for S2 RamRel): read the f32-
 * quantized policy table (little-endian '<f4', produced by make_pg_f32.py,
 * IEEE-754 round-to-nearest-even) and promote each element to double at load
 * time. The promotion float->double is exact, so every downstream double
 * operation sees exactly the f32-quantized value — bit-identical to what an
 * S2 target would compute when converting per access from an f32 table in
 * SRAM. Element count must match expect_elems exactly. */
static double *read_f32_promote(const char *path, size_t expect_elems) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if ((size_t)sz != expect_elems * sizeof(float)) { fclose(f); return NULL; }
    float *buf32 = (float *)malloc((size_t)sz);
    if (!buf32) { fclose(f); return NULL; }
    if (fread(buf32, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f); free(buf32); return NULL;
    }
    fclose(f);
    double *out = (double *)malloc(expect_elems * sizeof(double));
    if (!out) { free(buf32); return NULL; }
    for (size_t i = 0; i < expect_elems; i++) out[i] = (double)buf32[i];
    free(buf32);
    return out;
}
#endif

int ems04_load_tables(const char *exported_dir, ems04_tables *t) {
    char path[1024];
#ifdef PG_F32
    /* S1.5 Task B: f32 table file (probe build only; f64 default untouched) */
    snprintf(path, sizeof path, "%s/policy_table_Pg.f32", exported_dir);
    t->Pg = read_f32_promote(path,
        (size_t)EMS04_N_LAM * EMS04_N_PBAR * EMS04_N_REG);
#else
    snprintf(path, sizeof path, "%s/policy_table_Pg.f64", exported_dir);
    t->Pg = read_f64(path,
        (size_t)EMS04_N_LAM * EMS04_N_PBAR * EMS04_N_REG * sizeof(double));
#endif
    snprintf(path, sizeof path, "%s/bsfc_lut.f64", exported_dir);
    t->bsfc_lut = read_f64(path, (size_t)EMS04_N_BSFC * sizeof(double));
    snprintf(path, sizeof path, "%s/soc_grid.f64", exported_dir);
    t->socx_owned = read_f64(path, (size_t)EMS04_N_SOC * sizeof(double));
    snprintf(path, sizeof path, "%s/dlam_grid.f64", exported_dir);
    t->dlam_owned = read_f64(path, (size_t)EMS04_N_DLAM * sizeof(double));
    if (!t->socx_owned || !t->dlam_owned) return -1;
    t->socx = t->socx_owned;
    t->dlam = t->dlam_owned;
    if (!(t->socx[0] == EMS04_SOC_GRID_LO) ||
        !(t->socx[EMS04_N_SOC - 1] > 0.94 && t->socx[EMS04_N_SOC - 1] < 0.96))
        return -2; /* sanity: grid must span 0.1..0.95 */
    return 0;
}

#endif /* EMS04_PG_PALETTE */

/* ---- execution layer (DP-internal realization), mirrors local_realize ---- */

static double local_fuel(const ems04_tables *T, double P_kW, double st) {
    if (st == 3.0) {
        double Pc = m_min(m_max(P_kW, EMS04_PMIN), EMS04_PRATED);
        int idx = clampi((int)m_round(Pc) - (int)EMS04_BSFC_P0 + 1, 1, EMS04_N_BSFC);
        return P_kW * T->bsfc_lut[idx - 1] / 3.6e6;
    } else if (st == 1.0 || st == 2.0) {
        return EMS04_C_START / (EMS04_T_START + EMS04_T_SYNC);
    }
    return 0.0;
}

static void local_genset(ems04_gen_state *g, int u, double P_ref,
                         double cmd_start, double cmd_stop, double dt,
                         double *P_act) {
    double st = g->st[u];
    double P_ramp = g->P_ramp[u];
    double P_lag = g->P_lag[u];
    double t_dwell = g->t_dwell[u];
    double pa = 0.0;
    if (st == 0.0) {
        if (cmd_start > 0.5 && cmd_stop < 0.5) { st = 1.0; t_dwell = 0.0; P_ramp = 0.0; P_lag = 0.0; }
    } else if (st == 1.0) {
        t_dwell += dt;
        if (t_dwell >= EMS04_T_START - 1e-12) { st = 2.0; t_dwell = 0.0; }
    } else if (st == 2.0) {
        t_dwell += dt;
        if (t_dwell >= EMS04_T_SYNC - 1e-12) { st = 3.0; t_dwell = 0.0; }
    } else if (st == 3.0) {
        if (cmd_stop > 0.5) { st = 4.0; t_dwell = 0.0; pa = 0.0; }
        else {
            double P_ref_c = m_min(m_max(P_ref, EMS04_PMIN), EMS04_PRATED);
            if (P_ramp < P_ref_c) P_ramp = m_min(P_ramp + EMS04_RAMP * dt, P_ref_c);
            else if (P_ramp > P_ref_c) P_ramp = m_max(P_ramp - EMS04_RAMP * dt, P_ref_c);
            P_lag += (P_ramp - P_lag) * dt / EMS04_TAU;
            pa = m_min(m_max(P_lag, 0.0), EMS04_PRATED);
        }
    } else if (st == 4.0) {
        t_dwell += dt;
        if (t_dwell >= EMS04_T_COOL - 1e-12) { st = 0.0; t_dwell = 0.0; }
    } else {
        st = 0.0; t_dwell = 0.0;
    }
    g->st[u] = st;
    g->P_ramp[u] = P_ramp;
    g->P_lag[u] = P_lag;
    g->t_dwell[u] = t_dwell;
    if (st != 3.0) pa = 0.0;
    *P_act = pa;
}

/* Mirrors local_realize. gen==NULL => need_eq initialization (backward pass).
 * Returns fuel (kg) over the bin; outputs next soc/N/gen; adds oob count. */
static double local_realize(const ems04_tables *T, double Pd, double soc,
                            double N, const ems04_gen_state *gen_in,
                            double dlam, int64_t *oob,
                            double *soc1, double *N1, ems04_gen_state *gen_out) {
    double dt = EMS04_PHYS_DT;
    int nI = (int)lround(EMS04_PHYS_DT_BIN / EMS04_PHYS_DT); /* 10 */
    ems04_gen_state g;
    int need_eq;
    if (gen_in) { g = *gen_in; need_eq = 0; }
    else { memset(&g, 0, sizeof g); need_eq = 1; }
    double Ncur = m_max(1.0, m_min(2.0, m_round(N)));
    double fuel = 0.0;
    for (int i = 0; i < nI; i++) {
        double lam_eff = m_max(EMS04_LAMBDA0 + EMS04_K_SOC * (EMS04_SOC_REF - soc) + dlam,
                               EMS04_LAMBDA_FLOOR);
        int o1 = 0;
        if (lam_eff < EMS04_LAMBDA_TBL_MIN - 1e-12 ||
            lam_eff > EMS04_LAMBDA_TBL_MAX + 1e-12) o1 = 1;
        if (Pd < -1e-12 || Pd > EMS04_PBAR_MAX + 1e-12) o1 = 1;
        *oob += o1;
        int il = clampi((int)m_round((lam_eff - EMS04_LAMBDA_TBL_MIN) / EMS04_LAMBDA_TBL_STEP) + 1,
                        1, EMS04_N_LAM);
        int ip = clampi((int)m_round(Pd / EMS04_PBAR_STEP) + 1, 1, EMS04_N_PBAR);
        int ir;
        if (soc <= 0.105) ir = 2;
        else if (soc >= 0.945) ir = 3;
        else ir = 1;
        /* MATLAB column-major: Pg(il, ip, ir) -> il + (ip-1)*nL + (ir-1)*nL*nP */
        size_t idx = (size_t)(il - 1)
                   + (size_t)(ip - 1) * EMS04_N_LAM
                   + (size_t)(ir - 1) * EMS04_N_LAM * EMS04_N_PBAR;
        double Pg_star = ems04_pg_at(T, idx);
        double Nn;
        if (Ncur >= 2.0) Nn = 2.0;
        else if (Pg_star > EMS04_PRATED + 1e-9 || Pg_star > EMS04_P_START + 1e-9) Nn = 2.0;
        else Nn = 1.0;
        double Pref[2] = {0.0, 0.0};
        double Praw = Pg_star / Nn;
        double Pi = m_min(m_max(Praw, EMS04_PMIN), EMS04_PRATED);
        int nNi = (int)Nn;
        for (int u = 0; u < nNi; u++) Pref[u] = Pi;
        if (need_eq) {
            for (int u = 0; u < nNi; u++) {
                g.st[u] = 3.0;
                g.P_ramp[u] = Pref[u];
                g.P_lag[u] = Pref[u];
            }
            need_eq = 0;
        }
        Ncur = Nn;
        double P_act[2] = {0.0, 0.0};
        for (int u = 0; u < 2; u++) {
            double cmd_start = (u < nNi) ? 1.0 : 0.0;
            double cmd_stop = (u < nNi) ? 0.0 : 1.0;
            local_genset(&g, u, Pref[u], cmd_start, cmd_stop, dt, &P_act[u]);
            fuel += local_fuel(T, P_act[u], g.st[u]) * dt;
        }
        double P_gen = P_act[0] + P_act[1];
        double Pbat = Pd - EMS04_ETA_RECT * P_gen;
        double dE;
        if (Pbat > 0.0) {
            double eta_rt = EMS04_ETA_DCDC * EMS04_ETA_DIS_DP;
            dE = -Pbat / eta_rt * (dt / 3600.0);
        } else if (Pbat < 0.0) {
            double eta_rt = EMS04_ETA_DCDC * EMS04_ETA_CHG_DP;
            dE = (-Pbat) * eta_rt * (dt / 3600.0);
        } else {
            dE = 0.0;
        }
        soc += dE / EMS04_CAP_KWH;
    }
    *soc1 = soc;
    *N1 = Ncur;
    *gen_out = g;
    return fuel;
}

/* local_cost: realize + terminal interpolation on Vnext. */
static double local_cost(const ems04_tables *T, double SOC, double N, double Pd,
                         double dlam_u, const double *Vnext1, const double *Vnext2,
                         int64_t *oob, double *socn_out, double *n2_out) {
    ems04_gen_state gtmp;
    double socn, n2, fuel;
    fuel = local_realize(T, Pd, SOC, N, NULL, dlam_u, oob, &socn, &n2, &gtmp);
    n2 = m_max(1.0, m_min(2.0, m_round(n2)));
    /* MATLAB: socn outside the grid (no 1e-12 slack in the comparison itself;
     * interp1 returns extrapval for xik>maxx / xik<minx) -> J = BIG */
    if (socn < T->socx[0] || socn > T->socx[EMS04_N_SOC - 1]) {
        return EMS04_BIG_M;
    }
    const double *Vn = (n2 >= 1.5) ? Vnext2 : Vnext1;
    double vn = interp_uniform(Vn, T->socx, EMS04_N_SOC, socn, EMS04_BIG_M);
    *socn_out = socn;
    *n2_out = n2;
    return fuel + vn;
}

/* local_best: scan all 41 dlam levels, first-minimum wins with 1e-15 tol. */
static void local_best(const ems04_tables *T, double SOC, double N, double Pd,
                       const double *dlam, const double *Vnext1,
                       const double *Vnext2, int izero, int64_t *oob,
                       double *bestJ, int *bestU, int *valid) {
    double bj = EMS04_BIG_M;
    int bu = izero;
    int v = 0;
    for (int u = 0; u < EMS04_N_DLAM; u++) {
        double socn_dummy, n2_dummy;
        double J = local_cost(T, SOC, N, Pd, dlam[u], Vnext1, Vnext2, oob,
                              &socn_dummy, &n2_dummy);
        if (J < bj - 1e-15) {
            bj = J;
            bu = u;
            v = isfinite(J) && J < EMS04_BIG_M * 0.5;
        }
    }
    *bestJ = bj; *bestU = bu; *valid = v;
}

/* ---- public: one 10 s decision tick (b2_window_dp, AC-611) ---- */

static double b2_vterm(double SOC_H) {
    double lin = -EMS04_VTERM_KEQ * EMS04_VTERM_CAP * (SOC_H - EMS04_SOC_TARGET);
    double exc = m_max(0.0, fabs(SOC_H - EMS04_SOC_TARGET) - EMS04_VTERM_DELTA);
    return lin + EMS04_VTERM_W * exc * exc;
}

int ems04_dp_tick(const ems04_tables *T, const double *Pbar, int K,
                  double SOC0, double N0, const ems04_gen_state *gen0,
                  ems04_tick_result *out) {
    if (K < 1 || K > EMS04_K_MAX) return -1;
    /* action + SOC grids come from T (bit-identical MATLAB colon dumps) */
    const double *dlam = T->dlam;
    int izero = -1;
    for (int u = 0; u < EMS04_N_DLAM; u++)
        if (fabs(dlam[u]) < 1e-12) { izero = u; break; }
    if (izero < 0) return -2;
    const double *socx = T->socx;
    double soc_hi = socx[EMS04_N_SOC - 1];

    /* V_all[(K+1) x 2 x nS], flattened k*2*nS + iN*nS + i:
     * for fixed k,iN the SOC axis is contiguous (interp column semantics). */
    double *V_all = (double *)calloc((size_t)(K + 1) * EMS04_N_SOC * 2, sizeof(double));
    if (!V_all) return -3;
    double *infeas_step = (double *)calloc((size_t)K, sizeof(double));
    if (!infeas_step) { free(V_all); return -3; }

    for (int i = 0; i < EMS04_N_SOC; i++) {
        double vt = b2_vterm(socx[i]);
        V_all[(size_t)K * 2 * EMS04_N_SOC + 0 * EMS04_N_SOC + i] = vt;
        V_all[(size_t)K * 2 * EMS04_N_SOC + 1 * EMS04_N_SOC + i] = vt;
    }

    int64_t oob = 0;
    for (int k = K - 1; k >= 0; k--) {
        double Pd = Pbar[k];
        double step_infeas_all = 1.0;
        const double *V0 = &V_all[(size_t)(k + 1) * 2 * EMS04_N_SOC + 0 * EMS04_N_SOC];
        const double *V1 = &V_all[(size_t)(k + 1) * 2 * EMS04_N_SOC + 1 * EMS04_N_SOC];
        for (int iN = 0; iN < 2; iN++) {
            for (int i = 0; i < EMS04_N_SOC; i++) {
                double SOC = socx[i];
                double bestJ; int bestU, valid;
                local_best(T, SOC, (double)(iN + 1), Pd, dlam, V0, V1,
                           izero, &oob, &bestJ, &bestU, &valid);
                if (valid) step_infeas_all = 0.0;
                if (!valid) {
                    double socn_d, n2_d;
                    bestJ = local_cost(T, SOC, (double)(iN + 1), Pd, 0.0,
                                       V0, V1, &oob, &socn_d, &n2_d);
                }
                V_all[(size_t)k * 2 * EMS04_N_SOC + (size_t)iN * EMS04_N_SOC + i] = bestJ;
            }
        }
        infeas_step[k] = step_infeas_all;
    }

    /* forward pass from the plant-measured initial state */
    double soc = SOC0;
    double N = m_max(1.0, m_min(2.0, m_round(N0)));
    ems04_gen_state gen = *gen0;
    double fuel_proxy = 0.0;
    int64_t fallback_n = 0;
    double d1 = 0.0;
    double dlam_all[EMS04_K_MAX];
    double soc_path[EMS04_K_MAX + 1];
    double N_path[EMS04_K_MAX + 1];
    soc_path[0] = soc;
    N_path[0] = N;
    for (int k = 0; k < K; k++) {
        double Pd = Pbar[k];
        const double *V0 = &V_all[(size_t)(k + 1) * 2 * EMS04_N_SOC + 0 * EMS04_N_SOC];
        const double *V1 = &V_all[(size_t)(k + 1) * 2 * EMS04_N_SOC + 1 * EMS04_N_SOC];
        double bestJ; int bestU, valid;
        local_best(T, soc, N, Pd, dlam, V0, V1, izero, &oob, &bestJ, &bestU, &valid);
        if (!valid || infeas_step[k] > 0.5) {
            bestU = izero;
            fallback_n += 1;
        }
        double dlam_k = dlam[bestU];
        dlam_all[k] = dlam_k;
        if (k == 0) d1 = dlam_k;
        ems04_gen_state gnext;
        double socn, Nn;
        double fuel_k = local_realize(T, Pd, soc, N, &gen, dlam_k, &oob,
                                      &socn, &Nn, &gnext);
        soc = socn; N = Nn; gen = gnext;
        fuel_proxy += fuel_k;
        soc = m_min(m_max(soc, socx[0]), soc_hi);
        soc_path[k + 1] = soc;
        N_path[k + 1] = N;
    }

    out->dlambda_first = d1;
    memcpy(out->dlambda_all, dlam_all, sizeof dlam_all);
    memcpy(out->soc_path, soc_path, sizeof soc_path);
    memcpy(out->N_path, N_path, sizeof N_path);
    out->soc_end = soc;
    out->fuel_proxy_kg = fuel_proxy;
    out->J_window_kg = fuel_proxy + b2_vterm(soc);
    out->oob_query_count = oob;
    out->infeasible_fallback_n = fallback_n;
    out->n_fallback_steps = 0;
    free(V_all);
    free(infeas_step);
    return 0;
}

/* ---- execution layer lambda law (pure, contract §4.3) ---- */

double ems04_law6_lambda_eff(double SOC, double e_hat) {
    double soc_ref_eff = EMS04_SOC_REF + EMS04_K_S_B2 * e_hat;
    double lam = EMS04_LAMBDA0 + EMS04_K_SOC * (soc_ref_eff - SOC);
    lam = m_max(lam, EMS04_LAMBDA_FLOOR); /* K_S != 0 in the C2 arm */
    return lam;
}
