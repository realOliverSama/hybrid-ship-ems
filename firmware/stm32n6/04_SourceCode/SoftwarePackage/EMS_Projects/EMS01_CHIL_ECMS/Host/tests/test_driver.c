/* test_driver.c — L0 单测驱动（Host/tests，T-503）。
 * 由 test_ctrl_diff.py 以主机 gcc 编译（ems_ctrl.c + ems_infer_ref.c + ems_params.c），
 * 从 stdin 逐行读命令、向 stdout 输出 key=value 结果；%.17g 保证 double 往返无损。
 * 仅测试工具，不入固件。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ems_ctrl.h"
#include "ems_infer.h"

#define LINE_BUF 65536

static ems_ctrl_t g_ctrl;

/* 合成 BSFC/常数网格命令的白盒入口（真实常数路径用 gridreal） */
static int cmd_grid(const char *s)
{
    ems_ctrl_ecms_cst_t c;
    double lam, pload, soc;
    int nb;
    if (sscanf(s, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %d %lf %lf %lf",
               &c.eta_rect, &c.eta_dcdc, &c.bat_eff_dis, &c.bat_eff_chg,
               &c.soc_discharge_max, &c.soc_charge_min, &c.idle_fuel_kgph,
               &c.grid_dp_kw, &c.gen_p_max_kw, &nb, &pload, &soc, &lam) != 13) {
        return 2;
    }
    if (nb < 1 || nb > 64) {
        return 2;
    }
    double bp[64], tbl[64];
    char *p = (char *)s;
    for (int k = 0; k < 13; k++) { /* 跳过前 13 个已读字段 */
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        strtod(p, &p);
    }
    for (int k = 0; k < nb; k++) {
        bp[k] = strtod(p, &p);
    }
    for (int k = 0; k < nb; k++) {
        tbl[k] = strtod(p, &p);
    }
    c.bsfc_bp = bp;
    c.bsfc_tbl = tbl;
    c.bsfc_n = nb;
    printf("GRID %.17g\n", ems_ctrl_grid_search(&c, pload, soc, lam));
    return 0;
}

int main(void)
{
    static char line[LINE_BUF];
    ems_ctrl_init(&g_ctrl);

    while (fgets(line, sizeof line, stdin) != NULL) {
        char *s = line;
        /* 去掉行尾换行 */
        size_t n = strlen(s);
        while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
            s[--n] = '\0';
        }

        if (strncmp(s, "reset", 5) == 0) {
            ems_ctrl_init(&g_ctrl);
            printf("RESET 1\n");
        } else if (strncmp(s, "state", 5) == 0) {
            double sum = 0.0;
            for (int i = 0; i < EMS_CTRL_WIN_N; i++) {
                sum += g_ctrl.buf[i];
            }
            printf("STATE init=%d k=%d lastT=%.17g vprev=%.17g Pheld=%.17g "
                   "cls_eff=%d run_cls=%d run_cnt=%d buf0=%.17g buf1=%.17g "
                   "bufmid=%.17g buf299=%.17g bufsum=%.17g\n",
                   g_ctrl.initialized, g_ctrl.k, g_ctrl.lastT, g_ctrl.vprev,
                   g_ctrl.Pheld, g_ctrl.gate.cls_eff, g_ctrl.gate.run_cls,
                   g_ctrl.gate.run_cnt, g_ctrl.buf[0], g_ctrl.buf[1],
                   g_ctrl.buf[150], g_ctrl.buf[EMS_CTRL_WIN_N - 1], sum);
        } else if (strncmp(s, "step ", 5) == 0) {
            ems_ctrl_in_t in;
            ems_ctrl_out_t out;
            int nn_ok;
            memset(&out, 0, sizeof out); /* held 早退只写部分字段，先清零保证输出确定 */
            if (sscanf(s + 5, "%lf %lf %lf %lf %d", &in.t, &in.P_load,
                       &in.v_ship, &in.SOC, &nn_ok) != 5) {
                printf("ERR parse\n");
                continue;
            }
            in.nn_ok = nn_ok;
            (void)ems_ctrl_step(&g_ctrl, &in, &out);
            printf("STEP held=%d ok=%d P=%.17g e_hat=%.17g e_hat_raw=%.17g "
                   "K=%.17g K_q=%.17g K_w=%d cls_eff=%d cls=%d cls_inv=%d "
                   "q10=%.17g q50=%.17g q90=%.17g sq=%.17g\n",
                   out.held, out.ok, out.P, out.e_hat, out.e_hat_raw, out.K,
                   out.K_q, out.K_w, out.cls_eff, out.cls_raw, out.cls_invalid,
                   out.q3_kwh[0], out.q3_kwh[1], out.q3_kwh[2], out.s_q);
        } else if (strncmp(s, "bsfc ", 5) == 0) {
            int nb;
            double pg;
            if (sscanf(s + 5, "%d %lf", &nb, &pg) != 2 || nb < 1 || nb > 64) {
                printf("ERR parse\n");
                continue;
            }
            double bp[64], tbl[64];
            char *p = (char *)s + 5;
            for (int k = 0; k < 2; k++) {
                while (*p == ' ' || *p == '\t') {
                    p++;
                }
                strtod(p, &p);
            }
            for (int k = 0; k < nb; k++) {
                bp[k] = strtod(p, &p);
            }
            for (int k = 0; k < nb; k++) {
                tbl[k] = strtod(p, &p);
            }
            printf("BSFC %.17g\n", ems_ctrl_bsfc_at(bp, tbl, nb, pg));
        } else if (strncmp(s, "gridreal ", 9) == 0) {
            /* 真实常数（ems_params）+ 传入 P_load/SOC/λ 的白盒网格 */
            double pload, soc, lam;
            if (sscanf(s + 9, "%lf %lf %lf", &pload, &soc, &lam) != 3) {
                printf("ERR parse\n");
                continue;
            }
            ems_ctrl_ecms_cst_t c = {
                EMS_PLANT.eta_rect, EMS_PLANT.eta_dcdc,
                EMS_PLANT.bat_eff_dis, EMS_PLANT.bat_eff_chg,
                EMS_PLANT.soc_discharge_max, EMS_PLANT.soc_charge_min,
                EMS_PLANT.idle_fuel_kgph, EMS_PLANT.grid_dp_kw,
                EMS_PLANT.gen_p_max_kw, EMS_BSFC_BP, EMS_BSFC_TBL,
                (int)EMS_PARAMS_BSFC_N,
            };
            printf("GRID %.17g\n", ems_ctrl_grid_search(&c, pload, soc, lam));
        } else if (strncmp(s, "grid ", 5) == 0) {
            if (cmd_grid(s + 5) != 0) {
                printf("ERR parse\n");
            }
        } else if (strncmp(s, "kq ", 3) == 0) {
            double sq = strtod(s + 3, NULL);
            printf("KQ %.17g\n", ems_ctrl_gate_kq(sq));
        } else if (strncmp(s, "kcomb ", 6) == 0) {
            int mode, kw;
            double kq;
            if (sscanf(s + 6, "%d %lf %d", &mode, &kq, &kw) != 3) {
                printf("ERR parse\n");
                continue;
            }
            printf("KCOMB %.17g\n", ems_ctrl_gate_combine(mode, kq, kw));
        } else if (strncmp(s, "kw ", 3) == 0) {
            int ce = atoi(s + 3);
            printf("KW %d\n", ems_ctrl_gate_kw(ce));
        } else if (strncmp(s, "hystreset", 9) == 0) {
            g_ctrl.gate.cls_eff = -1;
            g_ctrl.gate.run_cls = -1;
            g_ctrl.gate.run_cnt = 0;
            printf("HYST cls_eff=%d run_cls=%d run_cnt=%d\n", g_ctrl.gate.cls_eff,
                   g_ctrl.gate.run_cls, g_ctrl.gate.run_cnt);
        } else if (strncmp(s, "hyst ", 5) == 0) {
            int cls = atoi(s + 5);
            ems_ctrl_hysteresis_step(&g_ctrl.gate, cls);
            printf("HYST cls_eff=%d run_cls=%d run_cnt=%d\n", g_ctrl.gate.cls_eff,
                   g_ctrl.gate.run_cls, g_ctrl.gate.run_cnt);
        } else if (strncmp(s, "argmax ", 7) == 0) {
            double v[64];
            int cnt = 0;
            char *p = (char *)s + 7;
            while (cnt < 64) {
                char *end = p;
                v[cnt] = strtod(p, &end);
                if (end == p) {
                    break;
                }
                cnt++;
                p = end;
            }
            printf("ARGMAX %d\n", ems_ctrl_argmax_first(v, cnt));
        } else if (strncmp(s, "mlpq ", 5) == 0) {
            double x[302], q[3];
            char *p = (char *)s + 5;
            for (int i = 0; i < 302; i++) {
                x[i] = strtod(p, &p);
            }
            int ret = ems_infer_mlpq(x, q, NULL);
            printf("MLPQ ret=%d q10=%.17g q50=%.17g q90=%.17g\n", ret, q[0], q[1], q[2]);
        } else if (strncmp(s, "workclf ", 8) == 0) {
            double pd[30], v, a;
            int cls = 0;
            char *p = (char *)s + 8;
            for (int i = 0; i < 30; i++) {
                pd[i] = strtod(p, &p);
            }
            v = strtod(p, &p);
            a = strtod(p, &p);
            int ret = ems_infer_workclf(pd, v, a, &cls, NULL, NULL);
            printf("WORKCLF ret=%d cls=%d\n", ret, cls);
        } else if (s[0] == '\0' || s[0] == '#') {
            /* 空行/注释 */
        } else {
            printf("ERR unknown cmd\n");
        }
        fflush(stdout);
    }
    return 0;
}
