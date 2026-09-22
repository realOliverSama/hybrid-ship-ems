/* test_driver_int8ref.c — T-508 层② host-int8 P_gen 参考驱动（Host/tests）。
 * 由 Host/tools/gen_host_int8_pgen.py 主机 gcc 编译，组合：
 *   ems_ctrl.c + ems_params.c + 本文件（刻意不编译 ems_infer_ref.c/npu.c——
 * 模式 B CSV 的 NN 输出由本文件内 stub 后端按 ems_infer.h 签名注入）。
 * 控制律单一来源 = Appli/App/ems_ctrl.c：本文件只做 CSV 回放 + stub 注入，
 * 不复制任何控制逻辑。编译选项沿用 test_l0_trace_diff.py（零警告硬检查）。
 *
 * stub 语义（design 层②参考工具，spec ems01-chil-ecms-v1）：
 *   - stub_set_row(q10,q50,q90,cls)：driver 每 tick 在 ems_ctrl_step 前调用，
 *     载入按 tick 键控查得的模式 B 行（q*_kwh 与 cls_raw；与调用次序解耦）；
 *   - ems_infer_mlpq：忽略 x302，返回归一化域 q3n[i]=(q_kwh[i]-Y_MEAN)/Y_STD
 *     （Y_MEAN/Y_STD 单一来源 ems_params.h；经 ems_ctrl 反归一化后还原模式 B
 *     的 q*_kwh）；未 set 行时写安全零值并返回 1（失败语义，正常流程不触发）；
 *   - ems_infer_workclf：返回模式 B 行 cls_raw（迟滞/门控由 ems_ctrl 照常执行）；
 *   - ems_infer_last_us_reset/last_us 恒 0（同 REF 后端语义）；us 输出写 0。
 *
 * main：argv = <trace_csv> <modeb_csv> <out_csv>。逐 tick 读 trace 行
 * （ref_G2 同款 7 信号 CSV，取 t/P_load/SOC/v_ship）→ tick 键控查模式 B 行 →
 * stub_set_row → 按 test_driver.c/test_l0_trace_diff.py 约定（nn_ok=1）调
 * ems_ctrl_step → 写行：
 *   tick,t,P_load_kW,SOC,e_hat,K,cls_raw,cls_eff,held,ok,P_gen_kW
 * （e_hat/K/cls_raw/cls_eff/held/ok 直通模式 B 行值；P_gen_kW = ems_ctrl out.P）。
 * 仅 stdio/stdlib/string，无 POSIX 头（macOS gcc/clang 均可编）。仅测试工具，不入固件。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ems_ctrl.h"
#include "ems_infer.h"
#include "ems_params.h"

#define MAX_FLDS 32
#define LINE_BUF 65536

/* ---------------- stub 后端状态（当前模式 B 行） ---------------- */
static int    g_row_set;
static double g_row_q_kwh[3];
static int    g_row_cls_raw;

void stub_set_row(double q10n, double q50n, double q90n, int cls)
{
    g_row_q_kwh[0] = q10n;
    g_row_q_kwh[1] = q50n;
    g_row_q_kwh[2] = q90n;
    g_row_cls_raw = cls;
    g_row_set = 1;
}

/* ems_infer_mlpq stub：返回归一化域 q3（kWh→归一化域，常数单一来源 ems_params.h） */
int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us)
{
    int i;
    (void)x302;
    if (us) {
        *us = 0;
    }
    if (!g_row_set) {                      /* driver 保证每 tick 先 set；防御分支 */
        q3[0] = q3[1] = q3[2] = 0.0;
        return 1;
    }
    for (i = 0; i < 3; i++) {
        q3[i] = (g_row_q_kwh[i] - EMS_NORM_Y_MEAN) / EMS_NORM_Y_STD;
    }
    return 0;
}

/* ems_infer_workclf stub：返回模式 B 行 cls_raw（logits 不提供，控制律传 NULL） */
int ems_infer_workclf(const double pdec30[30], double v, double a, int *cls,
                      float out_logits[7], uint32_t *us)
{
    (void)pdec30;
    (void)v;
    (void)a;
    if (us) {
        *us = 0;
    }
    if (out_logits) {
        memset(out_logits, 0, 7 * sizeof(float));
    }
    if (!g_row_set) {
        if (cls) {
            *cls = 0;
        }
        return 1;
    }
    if (cls) {
        *cls = g_row_cls_raw;
    }
    return 0;
}

/* T-506 FR-505 遥测：stub 后端无独立计数，恒 0（同 REF 后端差异语义） */
void ems_infer_last_us_reset(void)
{
}

uint32_t ems_infer_last_us(void)
{
    return 0;
}

/* ---------------- 模式 B 行存储（tick 键控查表） ---------------- */
typedef struct {
    int    tick, held, ok, cls_raw, cls_eff;
    double q10, q50, q90, e_hat, k;
} modeb_row_t;

static modeb_row_t *g_mb;
static int g_mb_n;
static int g_mb_cap;

static void mb_push(const modeb_row_t *r)
{
    if (g_mb_n == g_mb_cap) {
        modeb_row_t *tmp;
        g_mb_cap = g_mb_cap ? g_mb_cap * 2 : 1024;
        tmp = (modeb_row_t *)realloc(g_mb, (size_t)g_mb_cap * sizeof(*g_mb));
        if (!tmp) {
            fprintf(stderr, "ERROR: 内存不足\n");
            exit(2);
        }
        g_mb = tmp;
    }
    g_mb[g_mb_n++] = *r;
}

static const modeb_row_t *mb_find(int tick)
{
    int i;
    for (i = 0; i < g_mb_n; i++) {
        if (g_mb[i].tick == tick) {
            return &g_mb[i];
        }
    }
    return NULL;
}

/* ---------------- 最小 CSV 解析（仅 stdio/stdlib/string） ---------------- */

/* 行内按逗号切分（原地改写，吃掉行尾 CR/LF）；返回字段数 */
static int split_csv(char *s, char **fld, int maxf)
{
    int n = 0;
    char *p = s;
    while (n < maxf) {
        fld[n++] = p;
        while (*p != '\0' && *p != ',' && *p != '\n' && *p != '\r') {
            p++;
        }
        if (*p == ',') {
            *p++ = '\0';
            continue;
        }
        *p = '\0';
        break;
    }
    return n;
}

static int col_idx(char **fld, int nf, const char *name)
{
    int i;
    for (i = 0; i < nf; i++) {
        if (strcmp(fld[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

static int load_modeb(const char *path)
{
    static char line[LINE_BUF];
    char *fld[MAX_FLDS];
    FILE *f = fopen(path, "r");
    int nh, c_tick, c_held, c_ok, c_q10, c_q50, c_q90, c_eh, c_k, c_craw, c_ceff;
    if (!f) {
        fprintf(stderr, "ERROR: 无法打开模式 B CSV: %s\n", path);
        return -1;
    }
    if (!fgets(line, sizeof line, f)) {
        fprintf(stderr, "ERROR: 模式 B CSV 为空\n");
        fclose(f);
        return -1;
    }
    nh = split_csv(line, fld, MAX_FLDS);
    c_tick = col_idx(fld, nh, "tick");
    c_held = col_idx(fld, nh, "held");
    c_ok   = col_idx(fld, nh, "ok");
    c_q10  = col_idx(fld, nh, "q10_kwh");
    c_q50  = col_idx(fld, nh, "q50_kwh");
    c_q90  = col_idx(fld, nh, "q90_kwh");
    c_eh   = col_idx(fld, nh, "e_hat");
    c_k    = col_idx(fld, nh, "K");
    c_craw = col_idx(fld, nh, "cls_raw");
    c_ceff = col_idx(fld, nh, "cls_eff");
    if (c_tick < 0 || c_held < 0 || c_ok < 0 || c_q10 < 0 || c_q50 < 0 ||
        c_q90 < 0 || c_eh < 0 || c_k < 0 || c_craw < 0 || c_ceff < 0) {
        fprintf(stderr, "ERROR: 模式 B CSV 缺必需列\n");
        fclose(f);
        return -1;
    }
    while (fgets(line, sizeof line, f)) {
        modeb_row_t r;
        int nf = split_csv(line, fld, MAX_FLDS);
        if (nf <= 1 && fld[0][0] == '\0') {
            continue;                          /* 空行 */
        }
        memset(&r, 0, sizeof r);
        r.tick    = atoi(fld[c_tick]);
        r.held    = atoi(fld[c_held]);
        r.ok      = atoi(fld[c_ok]);
        r.cls_raw = atoi(fld[c_craw]);
        r.cls_eff = atoi(fld[c_ceff]);
        r.q10   = strtod(fld[c_q10], NULL);
        r.q50   = strtod(fld[c_q50], NULL);
        r.q90   = strtod(fld[c_q90], NULL);
        r.e_hat = strtod(fld[c_eh], NULL);
        r.k     = strtod(fld[c_k], NULL);
        mb_push(&r);
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    static char line[LINE_BUF];
    static ems_ctrl_t ctrl;
    char *fld[MAX_FLDS];
    FILE *ftr, *fout;
    int nh, c_tick, c_t, c_pl, c_soc, c_vs;
    long i, n_rows = 0;

    if (argc != 4) {
        fprintf(stderr, "用法: %s <trace_csv> <modeb_csv> <out_csv>\n", argv[0]);
        return 2;
    }
    if (load_modeb(argv[2]) != 0) {
        return 2;
    }
    ftr = fopen(argv[1], "r");
    if (!ftr) {
        fprintf(stderr, "ERROR: 无法打开 trace CSV: %s\n", argv[1]);
        return 2;
    }
    if (!fgets(line, sizeof line, ftr)) {
        fprintf(stderr, "ERROR: trace CSV 为空\n");
        fclose(ftr);
        return 2;
    }
    nh = split_csv(line, fld, MAX_FLDS);
    c_t    = col_idx(fld, nh, "t");
    c_pl   = col_idx(fld, nh, "P_load_kW");
    c_soc  = col_idx(fld, nh, "SOC");
    c_vs   = col_idx(fld, nh, "v_ship_mps");
    c_tick = col_idx(fld, nh, "tick");       /* 缺列回退行序（load_trace 同约定） */
    if (c_t < 0 || c_pl < 0 || c_soc < 0 || c_vs < 0) {
        fprintf(stderr, "ERROR: trace CSV 缺必需列 t/P_load_kW/SOC/v_ship_mps\n");
        fclose(ftr);
        return 2;
    }
    fout = fopen(argv[3], "w");
    if (!fout) {
        fprintf(stderr, "ERROR: 无法写输出 CSV: %s\n", argv[3]);
        fclose(ftr);
        return 2;
    }
    fprintf(fout, "tick,t,P_load_kW,SOC,e_hat,K,cls_raw,cls_eff,held,ok,P_gen_kW\n");

    ems_ctrl_init(&ctrl);
    for (i = 0; fgets(line, sizeof line, ftr); i++) {
        const modeb_row_t *mb;
        ems_ctrl_in_t in;
        ems_ctrl_out_t out;
        int nf = split_csv(line, fld, MAX_FLDS);
        int tick;
        if (nf <= 1 && fld[0][0] == '\0') {
            continue;                          /* 空行 */
        }
        tick = (c_tick >= 0) ? atoi(fld[c_tick]) : (int)i;
        mb = mb_find(tick);                    /* 按 tick 键控查表，与次序解耦 */
        if (!mb) {
            fprintf(stderr, "ERROR: 模式 B CSV 缺 tick=%d 行\n", tick);
            fclose(ftr);
            fclose(fout);
            return 3;
        }
        stub_set_row(mb->q10, mb->q50, mb->q90, mb->cls_raw);
        in.t = strtod(fld[c_t], NULL);
        in.P_load = strtod(fld[c_pl], NULL);
        in.v_ship = strtod(fld[c_vs], NULL);
        in.SOC = strtod(fld[c_soc], NULL);
        in.nn_ok = 1;                        /* test_l0_trace_diff.py 约定 */
        memset(&out, 0, sizeof out);         /* held 早退只写部分字段，先清零 */
        (void)ems_ctrl_step(&ctrl, &in, &out);
        fprintf(fout, "%d,%.9g,%.9g,%.9g,%.6f,%.6f,%d,%d,%d,%d,%.6f\n",
                tick, in.t, in.P_load, in.SOC, mb->e_hat, mb->k,
                mb->cls_raw, mb->cls_eff, mb->held, mb->ok, out.P);
        n_rows++;
    }
    fclose(ftr);
    if (fclose(fout) != 0) {
        fprintf(stderr, "ERROR: 写出 CSV 失败\n");
        return 2;
    }
    printf("int8ref driver: %ld ticks (modeb rows=%d) -> %s\n",
           n_rows, g_mb_n, argv[3]);
    return 0;
}