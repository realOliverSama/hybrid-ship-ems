/* ems_reatt.c — REAT-T 板上预测服务实现（phase4-g，T-005）。
 *
 * 推理栈：Model/reatt8_q（stedgeai 4.0.1 全 CPU 编译形态，stai_reatt8_* API，
 * 同步全图执行）。生命周期 = TCN CPU 形态先例（ems_infer_npu.c tcnq 分支）：
 *   ems_stai_rt_init_once（全局一次，共享助手）
 *   → stai_reatt8_init + stai_reatt8_set_activations（每网络一次，懒）
 *   → set_inputs/set_outputs + run(SYNC)（每窗）
 * run 返回语义（TCN 板上实证教训）：CPU 形态成功返回 STAI_DONE 而非
 * STAI_SUCCESS；仅错误码域（>= STAI_ERROR_GENERIC）判失败。
 *
 * 调用语境：单任务顺序（ems_app 分发路径），无并发；init 状态静态。
 */
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "ems_config.h"

/* 服务可用性契约（spec phase4-g D4）：
 * - Release（NOR appli，正式交付形态）：完整实现（EMS_REATT_SERVICE 由
 *   CMake 于 NPU+tcnq 栈 Release 配置注入）；推理形态三选一（EMS_REATT_FORM）：
 *     SPLIT → 本文件不定义 ems_reatt_query（实现见 ems_reatt_split.c：
 *             N1=TCN CPU 形态 + N2=Attention/Dense NPU 形态）；
 *     NPU   → 下列 EMS_REATT_NPU 分支（整图 NPU 形态，phase4-g 实验形态）；
 *     CPU   → 下列 #else 分支（整图 CPU 形态，CPU 工程交付形态）。
 * - Debug（LRUN_RAMxspi1 2047K SRAM 窗口）：REATT 权重 1.43MB + 激活 333KB
 *   + 图代码 ≈2.09MB，加应用本体物理超窗（实测溢出 1.15MB）——Debug 编译
 *   明确桩（CMD_REATT → NACK ERR_INFER），Debug 镜像 = TCN64 基线形态，
 *   保 fw_gate 双构建门禁与既有调试工作流；
 * - REF / mlpq 可逆性分支：stai 4.0.1 栈未链接，同为桩。 */
#if defined(EMS_REATT_SPLIT)

/* 拆分形态：实现体在 ems_reatt_split.c（本 TU 空，避免重复符号） */

#elif defined(EMS_REATT_SERVICE) && (EMS_FW_MODE == EMS_FW_MODE_NPU) && defined(EMS_PREDICTOR_TCNQ)

#if defined(EMS_REATT_NPU)

/* =====================================================================
 * NPU 形态实现（EMS02_CHIL_REATT8_NPU；Neural-ART EC blobs + NOR 权重）
 * =====================================================================
 * 与 CPU 形态的契约差异：I/O 为 **int8**（IN 3300+180 B / OUT 90 B），
 * 固件负责
 *   - 输入量化：q = clamp(round(x_norm / scale_in) + zp_in)
 *   - 输出反量化：x = (q - zp_out) * scale_out
 * 量化参数**运行时**经 stai_reattnpu_get_info 读取（stai_tensor.scale/
 * zeropoint，per-tensor），不硬编码——转换工具策略变化时自动跟随。
 * 生命周期沿 workclf NPU 先例：ext new_inference → run(ASYNC) →
 * RUNNING_WFE: stai_ext_wfe + run_continue 循环。 */

#include "stm32n6xx_hal.h"
#include "stai.h"
#include "stai_ext.h"

#include "ems_stai_rt.h"
#include "ems_reatt.h"
#include "ems_reatt_params.h"
#include "../../Model/reatt8_q_npu/reattnpu.h"
#include "../../Model/reatt8_q_npu/stai_reattnpu.h"

STAI_NETWORK_CONTEXT_DECLARE(ems_stai_ctx_reattnpu, STAI_REATTNPU_CONTEXT_SIZE)

/* 混合精度契约：hist=f32（与 CPU 形态同域）、kf=int8、out=int8 */
static float rn_in_hist[3300u] __attribute__((aligned(32)));
static int8_t rn_in_kf[LL_ATON_REATTNPU_IN_2_SIZE_BYTES] __attribute__((aligned(32)));
static int8_t rn_out[LL_ATON_REATTNPU_OUT_1_SIZE_BYTES] __attribute__((aligned(32)));

static float rn_qscale_kf;
static int32_t rn_qzp_kf;
static float rn_qscale_out;
static int32_t rn_qzp_out;
static bool rn_ready;
static bool rn_ran_before; /* new_inference 仅在上一次异步推理之后需要（stai 契约） */

static uint32_t rn_now_us(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
}

static int rn_wait_done(stai_network *ctx, stai_return_code rc,
                        stai_return_code (*run_continue)(stai_network *))
{
    while (rc != STAI_DONE) {
        if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
            return 1;
        }
        if (rc == STAI_RUNNING_WFE) {
            stai_ext_wfe();
        }
        rc = run_continue(ctx);
    }
    return 0;
}

static int rn_init_once(void)
{
    ems_stai_rt_init_once();
    if (!rn_ready) {
        stai_network_info info;
        if (stai_reattnpu_init(ems_stai_ctx_reattnpu) != STAI_SUCCESS) {
            printf("RN-FAIL init\r\n"); return 1;
        }
        if (stai_reattnpu_get_info(ems_stai_ctx_reattnpu, &info) != STAI_SUCCESS) {
            printf("RN-FAIL getinfo\r\n"); return 1;
        }
        if (info.n_inputs < 2u || info.n_outputs < 1u) {
            printf("RN-FAIL ni=%u no=%u\r\n", (unsigned)info.n_inputs, (unsigned)info.n_outputs); return 1;
        }
        const stai_tensor *ti0 = &info.inputs[0];
        const stai_tensor *ti1 = &info.inputs[1];
        const stai_tensor *to = &info.outputs[0];
        /* 格式断言（混合精度契约；生成物形态漂移在此拒绝而非静默量化错） */
        if (ti0->format != STAI_FORMAT_FLOAT32
                || ti1->format != STAI_FORMAT_S8
                || to->format != STAI_FORMAT_S8) {
            printf("RN-FAIL fmt %d %d %d\r\n", (int)ti0->format, (int)ti1->format, (int)to->format); return 1;
        }
        if (ti1->scale.size != 1u || to->scale.size != 1u) {
            printf("RN-FAIL sc %u %u\r\n", (unsigned)ti1->scale.size, (unsigned)to->scale.size); return 1;
        }
        rn_qscale_kf = ti1->scale.data[0];
        rn_qzp_kf = (int32_t)ti1->zeropoint.data[0];
        rn_qscale_out = to->scale.data[0];
        rn_qzp_out = (int32_t)to->zeropoint.data[0];
        printf("RN-Q f=%d,%d,%d s=%.6f,%.6f,%.6f zp=%d,%d,%d\r\n",
               (int)ti0->format, (int)ti1->format, (int)to->format,
               (double)ti0->scale.size ? 0.0 : 0.0, (double)rn_qscale_kf,
               (double)rn_qscale_out, (int)ti0->zeropoint.size ? (int)ti0->zeropoint.data[0] : 0,
               (int)rn_qzp_kf, (int)rn_qzp_out);
        rn_ready = true;
    }
    return 0;
}

static int8_t rn_quant(float x, float s, int32_t zp)
{
    int32_t q = (int32_t)lrintf(x / s) + zp;
    if (q < -128) {
        q = -128;
    } else if (q > 127) {
        q = 127;
    }
    return (int8_t)q;
}

static float rn_dequant(int8_t q, float s, int32_t zp)
{
    return ((float)((int32_t)q - zp)) * s;
}

/* 稳定 softplus（与 CPU 分支 ems_reatt_softplus 同式） */
static float ems_reatt_softplus_pub(float x)
{
    float sp = (x > 0.0f) ? x : 0.0f;
    sp += log1pf(expf(-fabsf(x)));
    return sp;
}

int ems_reatt_query(const float hist_raw[EMS_REATT_HIST_N],
                    const float kf_raw[EMS_REATT_KF_N],
                    float q_out[EMS_REATT_Q_N],
                    uint32_t *infer_us)
{
    uint32_t t0 = rn_now_us();
    stai_ptr in_ptrs[LL_ATON_REATTNPU_IN_NUM];
    stai_ptr out_ptrs[LL_ATON_REATTNPU_OUT_NUM];
    stai_return_code rc;

    if (rn_init_once() != 0) {
        return 1;
    }

    /* 冻结归一化（hist 保持 f32 直喂；kf 归一化后量化） */
    for (uint32_t t = 0u; t < EMS_REATT_H1_T; t++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_H1_CH_N; ch++) {
            uint32_t idx = t * EMS_REATT_H1_CH_N + ch;
            rn_in_hist[idx] = (hist_raw[idx] - EMS_REATT_H1_MEAN[ch * EMS_REATT_H1_T + t])
                              / EMS_REATT_H1_STD[ch * EMS_REATT_H1_T + t];
        }
    }
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_KF_CH_N; ch++) {
            uint32_t idx = k * EMS_REATT_KF_CH_N + ch;
            float v = (kf_raw[idx] - EMS_REATT_KF_MEAN[ch * EMS_REATT_KF_T + k])
                      / EMS_REATT_KF_SCALE[ch * EMS_REATT_KF_T + k];
            rn_in_kf[idx] = rn_quant(v, rn_qscale_kf, rn_qzp_kf);
        }
    }

    /* 官方缓存维护契约（stneuralart_cache_maintenance，用户缓冲必须由应用维护）：
     * 输入由 MCU 写入 → CPU cache CLEAN（写回）后才可交 NPU 读取；
     * 输出由 NPU 写入 → CPU cache INVALIDATE 后再读。缺 clean 会让 NPU 读到
     * 陈旧数据（phase4-g NPU bring-up 实证修复点）。 */
    SCB_CleanDCache_by_Addr((uint32_t *)rn_in_hist, (int32_t)sizeof(rn_in_hist));
    SCB_CleanDCache_by_Addr((uint32_t *)rn_in_kf, (int32_t)sizeof(rn_in_kf));
    SCB_InvalidateDCache_by_Addr(rn_out, (int32_t)sizeof(rn_out));

    in_ptrs[0] = (stai_ptr)rn_in_hist;
    in_ptrs[1] = (stai_ptr)rn_in_kf;
    out_ptrs[0] = (stai_ptr)rn_out;
    {
        static uint32_t diag2_n;
        if (diag2_n < 1u) {
            diag2_n++;
            stai_ptr cur_in[2] = {0};
            stai_size n_in = 0;
            (void)stai_reattnpu_get_inputs(ems_stai_ctx_reattnpu, cur_in, &n_in);
            printf("RN-IO pre  n=%u a0=%p a1=%p | ours %p %p\r\n",
                   (unsigned)n_in, (void*)cur_in[0], (void*)cur_in[1],
                   (void*)rn_in_hist, (void*)rn_in_kf);
            printf("RN-DATA h0=%.3f k0=%d k1=%d k2=%d\r\n",
                   (double)rn_in_hist[0], (int)rn_in_kf[0], (int)rn_in_kf[1], (int)rn_in_kf[2]);
        }
    }
    if (stai_reattnpu_set_inputs(ems_stai_ctx_reattnpu, in_ptrs,
                                 LL_ATON_REATTNPU_IN_NUM) != STAI_SUCCESS
            || stai_reattnpu_set_outputs(ems_stai_ctx_reattnpu, out_ptrs,
                                         LL_ATON_REATTNPU_OUT_NUM) != STAI_SUCCESS) {
        printf("RN-FAIL setio\r\n"); return 1;
    }

    if (rn_ran_before) {
        /* stai 契约：new_inference 仅在上一异步推理之后准备下一拍；
         * 首拍调用非法（stai_reattnpu.h 注记），故首拍跳过。 */
        if (stai_ext_reattnpu_new_inference(ems_stai_ctx_reattnpu) != STAI_SUCCESS) {
            printf("RN-FAIL newinf\r\n"); return 1;
        }
    }
    rc = stai_reattnpu_run(ems_stai_ctx_reattnpu, STAI_MODE_ASYNC);
    if (rn_wait_done(ems_stai_ctx_reattnpu, rc,
                     stai_ext_reattnpu_run_continue) != 0) {
        printf("RN-FAIL run rc=%d\r\n", (int)rc); return 1;
    }

    rn_ran_before = true;
    {
        static uint32_t diag_n;
        if (diag_n < 3u) {
            diag_n++;
            printf("RN-OUT b=%d,%d,%d,%d,%d,%d m=%.2f a=%.2f b2=%.2f\r\n",
                   (int)rn_out[0], (int)rn_out[1], (int)rn_out[2],
                   (int)rn_out[3], (int)rn_out[4], (int)rn_out[5],
                   (double)rn_dequant(rn_out[0], rn_qscale_out, rn_qzp_out),
                   (double)rn_dequant(rn_out[1], rn_qscale_out, rn_qzp_out),
                   (double)rn_dequant(rn_out[2], rn_qscale_out, rn_qzp_out));
        }
    }

    /* 输出反量化 + 单调分位变换（与 CPU 形态同式） */
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        float m = rn_dequant(rn_out[k * 3u + 0u], rn_qscale_out, rn_qzp_out);
        float a = rn_dequant(rn_out[k * 3u + 1u], rn_qscale_out, rn_qzp_out);
        float b = rn_dequant(rn_out[k * 3u + 2u], rn_qscale_out, rn_qzp_out);
        float q10 = m - ems_reatt_softplus_pub(a);
        float q90 = m + ems_reatt_softplus_pub(b);
        q_out[k * 3u + 0u] = q10;
        q_out[k * 3u + 1u] = m;
        q_out[k * 3u + 2u] = q90;
        if (!isfinite(q10) || !isfinite(m) || !isfinite(q90)) {
            return 1;
        }
    }

    if (infer_us != NULL) {
        *infer_us = rn_now_us() - t0;
    }
    return 0;
}

#else /* CPU 形态实现（原样保留，供 EMS_REATT_NPU=0 构建/回归） */

/* ---- 正式实现（Release + NPU 工程 + tcnq 栈 = 本工程默认交付形态） ---- */

#include "stm32n6xx_hal.h"

#include "stai.h"

#include "ems_stai_rt.h"
#include "ems_reatt.h"
#include "ems_reatt_params.h"
#include "../../Model/reatt8_q/reatt8.h"

/* I/O 契约编译期防漂移（生成物再生成后布局变化在此失败） */
_Static_assert(STAI_REATT8_IN_1_SIZE_BYTES == 300u * 11u * 4u,
               "reatt8 in1 (normalized hist [1,300,11]) must be 3300 x f32 = 13200 B");
_Static_assert(STAI_REATT8_IN_2_SIZE_BYTES == 30u * 6u * 4u,
               "reatt8 in2 (normalized kf [1,30,6]) must be 180 x f32 = 720 B");
_Static_assert(STAI_REATT8_OUT_1_SIZE_BYTES == 30u * 3u * 4u,
               "reatt8 out1 (raw (m,a,b) [1,30,3]) must be 90 x f32 = 360 B");
_Static_assert(STAI_REATT8_WEIGHTS_SIZE_BYTES == 1432556u,
               "reatt8 CPU-form weights (in-binary) = 1432556 B");
_Static_assert(STAI_REATT8_ACTIVATIONS_SIZE_BYTES == 332928u,
               "reatt8 CPU-form activations arena = 332928 B");

/* stai 网络上下文（生成宏，stai.h；对齐 8 由宏内部保证） */
STAI_NETWORK_CONTEXT_DECLARE(ems_stai_ctx_reatt8, STAI_REATT8_CONTEXT_SIZE)

static uint8_t reatt8_act[STAI_REATT8_ACTIVATIONS_SIZE_BYTES]
    __attribute__((aligned(32)));
static float reatt_in_hist[300u * 11u] __attribute__((aligned(32)));
static float reatt_in_kf[30u * 6u]    __attribute__((aligned(32)));
static float reatt_out[30u * 3u]      __attribute__((aligned(32)));

static bool ems_reatt_ready;

/* DWT CYCCNT → μs（main.c USER CODE 使能；与 ems_infer_npu.c 同式） */
static uint32_t ems_reatt_now_us(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
}

static int ems_reatt_init_once(void)
{
    ems_stai_rt_init_once();
    if (!ems_reatt_ready) {
        if (stai_reatt8_init(ems_stai_ctx_reatt8) != STAI_SUCCESS) {
            return 1;
        }
        stai_ptr act_ptrs[STAI_REATT8_ACTIVATIONS_NUM] = { (stai_ptr)reatt8_act };
        if (stai_reatt8_set_activations(ems_stai_ctx_reatt8, act_ptrs,
                                        STAI_REATT8_ACTIVATIONS_NUM)
                != STAI_SUCCESS) {
            return 1;
        }
        ems_reatt_ready = true;
    }
    return 0;
}

/* 稳定 softplus：max(x,0) + log1p(exp(-|x|))——TFLite SOFTPLUS 溢出对策（D1） */
static float ems_reatt_softplus(float x)
{
    float sp = (x > 0.0f) ? x : 0.0f;
    sp += log1pf(expf(-fabsf(x)));
    return sp;
}

int ems_reatt_query(const float hist_raw[EMS_REATT_HIST_N],
                    const float kf_raw[EMS_REATT_KF_N],
                    float q_out[EMS_REATT_Q_N],
                    uint32_t *infer_us)
{
    uint32_t t0 = ems_reatt_now_us();
    stai_ptr in_ptrs[STAI_REATT8_IN_NUM];
    stai_ptr out_ptrs[STAI_REATT8_OUT_NUM];
    stai_return_code rc;

    if (ems_reatt_init_once() != 0) {
        return 1;
    }

    /* ---- 冻结归一化（f32 域；常数布局 [ch][bin]/[ch][lead]，ems_reatt_params.h） ---- */
    for (uint32_t t = 0u; t < EMS_REATT_H1_T; t++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_H1_CH_N; ch++) {
            uint32_t idx = t * EMS_REATT_H1_CH_N + ch;
            reatt_in_hist[idx] = (hist_raw[idx] - EMS_REATT_H1_MEAN[ch * EMS_REATT_H1_T + t])
                                 / EMS_REATT_H1_STD[ch * EMS_REATT_H1_T + t];
        }
    }
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_KF_CH_N; ch++) {
            uint32_t idx = k * EMS_REATT_KF_CH_N + ch;
            reatt_in_kf[idx] = (kf_raw[idx] - EMS_REATT_KF_MEAN[ch * EMS_REATT_KF_T + k])
                               / EMS_REATT_KF_SCALE[ch * EMS_REATT_KF_T + k];
        }
    }

    SCB_InvalidateDCache_by_Addr(reatt_out, (int32_t)sizeof(reatt_out));

    in_ptrs[0] = (stai_ptr)reatt_in_hist;
    in_ptrs[1] = (stai_ptr)reatt_in_kf;
    out_ptrs[0] = (stai_ptr)reatt_out;
    if (stai_reatt8_set_inputs(ems_stai_ctx_reatt8, in_ptrs, STAI_REATT8_IN_NUM)
            != STAI_SUCCESS
        || stai_reatt8_set_outputs(ems_stai_ctx_reatt8, out_ptrs, STAI_REATT8_OUT_NUM)
            != STAI_SUCCESS) {
        return 1;
    }

    /* CPU 形态同步全图执行：成功返回 STAI_DONE（非 STAI_SUCCESS），
     * 仅 >= STAI_ERROR_GENERIC 判失败（TCN CPU 形态板上实证语义）。 */
    rc = stai_reatt8_run(ems_stai_ctx_reatt8, STAI_MODE_SYNC);
    if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
        return 1;
    }

    /* ---- 单调分位变换（kW 域）：q10=m−sp(a)，q50=m，q90=m+sp(b) ---- */
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        float m = reatt_out[k * 3u + 0u];
        float a = reatt_out[k * 3u + 1u];
        float b = reatt_out[k * 3u + 2u];
        float q10 = m - ems_reatt_softplus(a);
        float q90 = m + ems_reatt_softplus(b);
        q_out[k * 3u + 0u] = q10;
        q_out[k * 3u + 1u] = m;
        q_out[k * 3u + 2u] = q90;
        if (!isfinite(q10) || !isfinite(m) || !isfinite(q90)) {
            return 1;
        }
    }

    if (infer_us != NULL) {
        *infer_us = ems_reatt_now_us() - t0;
    }
    return 0;
}

#endif /* EMS_REATT_NPU（NPU / CPU 形态二选一结束） */
#else /* !(EMS_REATT_SERVICE && NPU && tcnq) —— Debug SRAM 超窗 / REF / mlpq 桩 */

/* ---- 桩（REF 镜像 / mlpq 可逆性分支：stai 4.0.1 栈未链接） ----
 * CMD_REATT 在这些配置下恒 NACK ERR_INFER（协议面完整、服务面明确不可用，
 * 不产生静默错误）。 */

#include <stddef.h>

#include "ems_reatt.h"

int ems_reatt_query(const float hist_raw[EMS_REATT_HIST_N],
                    const float kf_raw[EMS_REATT_KF_N],
                    float q_out[EMS_REATT_Q_N],
                    uint32_t *infer_us)
{
    (void)hist_raw;
    (void)kf_raw;
    (void)q_out;
    if (infer_us != NULL) {
        *infer_us = 0u;
    }
    return 1;
}

#endif /* EMS_REATT_SERVICE / EMS_FW_MODE / EMS_PREDICTOR_TCNQ */
