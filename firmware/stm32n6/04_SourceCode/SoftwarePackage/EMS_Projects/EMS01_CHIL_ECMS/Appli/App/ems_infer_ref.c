/* ems_infer_ref.c — REF 推理后端：CPU double 前向参考实现（T-503）。
 *
 * 逐条复刻 EML eml_ecms_step_mode2.m 的网络前向（design §Data Model、§Testing L0）：
 *   MLP-Q  （EML:65-67）  302→64→32→3 Dense，ReLU 于 h1/h2，输出层线性；
 *   workclf（EML:82-134） causal Conv1D×3（核 5/3/3，仅左侧补零）→ MaxPool
 *            pool=2/stride=2（30→15→7，奇数长丢尾）→ GAP → 旁路融合
 *            s8=[v;a]·FC1 → hcl=[gfeat;s8]·FC2 → ocl=hcl·FC3 → argmax（平局取首）。
 *
 * 权重来自 ems_params.c 生成物（selected_model_weights_v2.json 全精度 double）。
 * 求和顺序 = 代码书写顺序（输入通道/时间步升序，Dense 先累加后加偏置，Conv 先加
 * 偏置后累加——与 EML 各行运算树一致）；REF 的该顺序即 L0 镜像比对的位级基准。
 * 平台无关：无 HAL、无动态内存；us 输出在 host/REF 侧写 0（板上时延由 T-505/507
 * 的 DWT CYCCNT 封装负责）。
 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "ems_infer.h"
#include "ems_params.h"

/* T-506 FR-505 遥测（ems_infer.h 契约）：REF 后端无独立计数，恒 0。
 * 板上 REF 模式推理时延并入 ctrl_us（ems_app.c DWT 全量）；不构成口径变更。 */
void ems_infer_last_us_reset(void)
{
}

uint32_t ems_infer_last_us(void)
{
    return 0u;
}

#define CLF_T1 30 /* conv1 输出时间步（=Pdec 长度） */
#define CLF_C1 16
#define CLF_T2 15
#define CLF_C2 32
#define CLF_T3 7
#define CLF_C3 32

/* EML:65-67 的 Dense 语义：acc = Σ_i W[i][j]·x[i]（升序），再 +b，可选 ReLU。
 * EML:65-66 用 max(·,0)：MATLAB max(NaN,0)=0，fmax 同语义。 */
static double dense_unit(const double *w, int in_n, int out_stride,
                         const double *x, const double *bias, int j, int relu)
{
    double acc = w[0 * out_stride + j] * x[0];
    for (int i = 1; i < in_n; i++) {
        acc += w[i * out_stride + j] * x[i];
    }
    acc += bias[j];
    return relu ? fmax(acc, 0.0) : acc;
}

int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us)
{
    double h1[64];
    double h2[32];

    for (int j = 0; j < 64; j++) { /* EML:65 h1 = max(W1.'*xnorm+b1, 0) */
        h1[j] = dense_unit(EMS_MLPQ_W1, 302, 64, x302, EMS_MLPQ_B1, j, 1);
    }
    for (int j = 0; j < 32; j++) { /* EML:66 h2 = max(W2.'*h1+b2, 0) */
        h2[j] = dense_unit(EMS_MLPQ_W2, 64, 32, h1, EMS_MLPQ_B2, j, 1);
    }
    for (int j = 0; j < 3; j++) { /* EML:67 y_norm = W3.'*h2+b3（无 ReLU） */
        q3[j] = dense_unit(EMS_MLPQ_W3, 32, 3, h2, EMS_MLPQ_B3, j, 0);
    }

    for (int j = 0; j < 3; j++) {
        if (!isfinite(q3[j])) {
            return 1; /* 输出非有限 → 调用方回退（design Error Handling） */
        }
    }
    if (us != NULL) {
        *us = 0u; /* host/REF 不计时；板上由 DWT 封装负责（T-505/507） */
    }
    return 0;
}

int ems_infer_workclf(const double pdec30[30], double v, double a, int *cls,
                      float out_logits[7], uint32_t *us)
{
    double c1[CLF_T1][CLF_C1];
    double p1[CLF_T2][CLF_C1];
    double c2[CLF_T2][CLF_C2];
    double p2[CLF_T3][CLF_C2];
    double c3[CLF_T3][CLF_C3];
    double gfeat[CLF_C3];
    double s8[8];
    double hcl[64];
    double ocl[7];
    double x40[40];

    /* ---- EML:83-95 conv1：causal 核 5、1 通道、16 滤波；acc 从偏置起（运算树同 EML） ---- */
    for (int f = 0; f < CLF_C1; f++) {
        for (int t = 0; t < CLF_T1; t++) {
            double acc = EMS_CLF_CONV1_B[f]; /* EML:86 */
            for (int kk = 0; kk < 5; kk++) {
                const int j = t + kk - 4; /* EML:88 jj=jt-5+kk（1 基）→ 0 基左补 4 */
                if (j >= 0) {             /* EML:89 左补零边界 */
                    acc += EMS_CLF_CONV1_W[kk * 16 + f] * pdec30[j];
                }
            }
            c1[t][f] = fmax(acc, 0.0); /* EML:93 */
        }
    }

    /* ---- EML:96-99 pool1：max pool 2/2，30→15（偶数无丢尾） ---- */
    for (int t = 0; t < CLF_T2; t++) {
        for (int f = 0; f < CLF_C1; f++) {
            p1[t][f] = fmax(c1[2 * t][f], c1[2 * t + 1][f]); /* EML:98 */
        }
    }

    /* ---- EML:100-112 conv2：causal 核 3、16→32 通道（通道点积后再加 acc，EML:107） ---- */
    for (int f = 0; f < CLF_C2; f++) {
        for (int t = 0; t < CLF_T2; t++) {
            double acc = EMS_CLF_CONV2_B[f]; /* EML:103 */
            for (int kk = 0; kk < 3; kk++) {
                const int j = t + kk - 2; /* EML:105 左补 2 */
                if (j >= 0) {
                    double s = 0.0;
                    for (int c = 0; c < CLF_C1; c++) {
                        s += p1[j][c] * EMS_CLF_CONV2_W[(kk * CLF_C1 + c) * CLF_C2 + f];
                    }
                    acc += s;
                }
            }
            c2[t][f] = fmax(acc, 0.0); /* EML:110 */
        }
    }

    /* ---- EML:113-116 pool2：15→7，使用行 0..13（丢尾：第 15 行被丢弃，design 风险清单） ---- */
    for (int t = 0; t < CLF_T3; t++) {
        for (int f = 0; f < CLF_C2; f++) {
            p2[t][f] = fmax(c2[2 * t][f], c2[2 * t + 1][f]); /* EML:115 */
        }
    }

    /* ---- EML:117-129 conv3：causal 核 3、32→32 通道 ---- */
    for (int f = 0; f < CLF_C3; f++) {
        for (int t = 0; t < CLF_T3; t++) {
            double acc = EMS_CLF_CONV3_B[f]; /* EML:120 */
            for (int kk = 0; kk < 3; kk++) {
                const int j = t + kk - 2; /* EML:122 左补 2 */
                if (j >= 0) {
                    double s = 0.0;
                    for (int c = 0; c < CLF_C2; c++) {
                        s += p2[j][c] * EMS_CLF_CONV3_W[(kk * CLF_C2 + c) * CLF_C3 + f];
                    }
                    acc += s;
                }
            }
            c3[t][f] = fmax(acc, 0.0); /* EML:127 */
        }
    }

    /* ---- EML:130 GAP：时间维均值（按行序累加后 /7） ---- */
    for (int c = 0; c < CLF_C3; c++) {
        double s = c3[0][c];
        for (int t = 1; t < CLF_T3; t++) {
            s += c3[t][c];
        }
        gfeat[c] = s / (double)CLF_T3;
    }

    /* ---- EML:131 旁路支路：s8 = max(F1W.'*[v;a]+F1B, 0)（F1W 2x8，升序 2 项点积） ---- */
    for (int j = 0; j < 8; j++) {
        double acc = EMS_CLF_FC1_W[0 * 8 + j] * v;
        acc += EMS_CLF_FC1_W[1 * 8 + j] * a;
        s8[j] = fmax(acc + EMS_CLF_FC1_B[j], 0.0);
    }

    /* ---- EML:132 hcl = max(F2W.'*[gfeat;s8]+F2B, 0)（40→64，Dense 语义同 mlpq） ---- */
    for (int i = 0; i < CLF_C3; i++) {
        x40[i] = gfeat[i];
    }
    for (int i = 0; i < 8; i++) {
        x40[CLF_C3 + i] = s8[i];
    }
    for (int j = 0; j < 64; j++) {
        hcl[j] = dense_unit(EMS_CLF_FC2_W, 40, 64, x40, EMS_CLF_FC2_B, j, 1);
    }

    /* ---- EML:133 ocl = F3W.'*hcl+F3B（64→7，线性） ---- */
    for (int j = 0; j < 7; j++) {
        ocl[j] = dense_unit(EMS_CLF_FC3_W, 64, 7, hcl, EMS_CLF_FC3_B, j, 0);
    }

    for (int j = 0; j < 7; j++) {
        if (!isfinite(ocl[j])) {
            return 1;
        }
    }

    if (out_logits != NULL) {
        /* REF 后端 logits 可得（EML:133 ocl 前向值），f32 截断后回填调用者
         * （CMD_WC_TEST 回显；与 NPU 后端 f32 输出同口径） */
        for (int j = 0; j < 7; j++) {
            out_logits[j] = (float)ocl[j];
        }
    }

    /* ---- EML:134 [~,cls] = max(ocl)：平局取首个（与 ems_ctrl_argmax_first 同规则） ---- */
    int best = 0;
    for (int j = 1; j < 7; j++) {
        if (ocl[j] > ocl[best]) {
            best = j;
        }
    }
    *cls = best + 1; /* 1..7（EML 1 基；1=depart..6=upstream..7=berth） */

    if (us != NULL) {
        *us = 0u;
    }
    return 0;
}
