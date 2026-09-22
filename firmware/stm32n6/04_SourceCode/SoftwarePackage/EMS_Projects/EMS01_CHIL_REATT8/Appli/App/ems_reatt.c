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
#include <stdbool.h>
#include <stdint.h>

#include "ems_config.h"

/* 服务可用性契约（spec phase4-g D4）：
 * - Release（NOR appli，正式交付形态）：完整实现（EMS_REATT_SERVICE 由
 *   CMake 于 NPU+tcnq 栈 Release 配置注入）；
 * - Debug（LRUN_RAMxspi1 2047K SRAM 窗口）：REATT 权重 1.43MB + 激活 333KB
 *   + 图代码 ≈2.09MB，加应用本体物理超窗（实测溢出 1.15MB）——Debug 编译
 *   明确桩（CMD_REATT → NACK ERR_INFER），Debug 镜像 = TCN64 基线形态，
 *   保 fw_gate 双构建门禁与既有调试工作流；
 * - REF / mlpq 可逆性分支：stai 4.0.1 栈未链接，同为桩。 */
#if defined(EMS_REATT_SERVICE) && (EMS_FW_MODE == EMS_FW_MODE_NPU) && defined(EMS_PREDICTOR_TCNQ)

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
