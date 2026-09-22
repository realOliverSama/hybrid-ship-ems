/* ems_reatt_split.c — REAT-T **拆分形态**板上预测服务实现（phase4-g NPU 线）。
 *
 * 架构（用户 2026-09-23 裁决；整图 NPU 形态输出退化的对策）：
 *   hist ──归一化──> N1（TCN 支路，CPU 编译形态，f32 I/O，SYNC）
 *                       └─> h_past[96] f32
 *   kf ──归一化+量化──> N2（Attention/Dense/Fusion/Decoder，NPU 形态）
 *                       ↑ h_past f32 直喂（**零拷贝共享缓冲**，无量化往返）
 *                       └─> raw (m,a,b) int8 → 反量化 → 分位变换 → q10/q50/q90
 *
 * 与另两种形态的关系：ems_reatt.c 在 EMS_REATT_SPLIT 下不定义
 * ems_reatt_query（本文件独占实现）；整图 CPU / 整图 NPU 两形态代码原样保留，
 * 由 CMake 的 EMS_REATT_FORM 选择。三形态共用 ems_reatt_params 冻结归一化常数
 * 与单调分位变换语义（D1）。
 *
 * 生命周期：
 *   N1 = CPU 形态先例（ems_infer_npu.c tcnq 分支）：init + set_activations，
 *        每窗 set_inputs/set_outputs + run(SYNC)；成功返回 STAI_DONE（非
 *        STAI_SUCCESS），仅 >= STAI_ERROR_GENERIC 判失败。
 *   N2 = NPU 形态先例（workclf/reattnpu）：ext new_inference（首拍跳过）→
 *        run(ASYNC) → RUNNING_WFE 循环。量化参数运行时读取，不硬编码。
 * 缓存维护（stneuralart_cache_maintenance 官方契约）：N1 输出 h_past 由 MCU 写、
 * NPU 读 → CLEAN；N2 的 kf 输入同；N2 输出由 NPU 写 → INVALIDATE。
 *
 * 调用语境：单任务顺序（ems_app 分发路径），无并发；init 状态静态。
 */
#if defined(EMS_REATT_SPLIT) && defined(EMS_REATT_SERVICE) \
    && (EMS_FW_MODE == EMS_FW_MODE_NPU) && defined(EMS_PREDICTOR_TCNQ)

#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "stm32n6xx_hal.h"
#include "stai.h"
#include "stai_ext.h"

#include "ems_stai_rt.h"
#include "ems_reatt.h"
#include "ems_reatt_params.h"
#include "../../Model/reatt8_q_npu/part_n1/reattn1c.h"
#include "../../Model/reatt8_q_npu/part_n2/reattn2n.h"
#include "../../Model/reatt8_q_npu/part_n2/stai_reattn2n.h"

/* 两网络上下文（生成宏展开为 stai_network=uint8_t 字节数组，stai.h） */
STAI_NETWORK_CONTEXT_DECLARE(rs_ctx_n1, STAI_REATTN1C_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(rs_ctx_n2, STAI_REATTN2N_CONTEXT_SIZE)

/* N1（CPU 形态）：I/O f32 + 激活 arena（权重在 .rodata，无需 set_weights） */
static uint8_t rs_n1_act[STAI_REATTN1C_ACTIVATIONS_SIZE_BYTES] __attribute__((aligned(32)));
static float rs_n1_hist[EMS_REATT_HIST_N] __attribute__((aligned(32)));
static float rs_n1_hpast[96u] __attribute__((aligned(32)));

/* N2（NPU 形态）：kf int8 + h_past f32（与 N1 输出同缓冲，零拷贝）+ 输出 int8 */
static int8_t rs_n2_kf[LL_ATON_REATTN2N_IN_1_SIZE_BYTES] __attribute__((aligned(32)));
static int8_t rs_n2_out[LL_ATON_REATTN2N_OUT_1_SIZE_BYTES] __attribute__((aligned(32)));

static float rs_kf_scale;
static int32_t rs_kf_zp;
static float rs_out_scale;
static int32_t rs_out_zp;
static bool rs_ready;
static bool rs_ran_before; /* N2 new_inference 仅在上一次异步推理之后需要（stai 契约） */

static uint32_t rs_now_us(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
}

static int rs_wait_done(stai_network *ctx, stai_return_code rc,
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

static int8_t rs_quant(float x, float s, int32_t zp)
{
    int32_t q = (int32_t)lrintf(x / s) + zp;
    if (q < -128) {
        q = -128;
    } else if (q > 127) {
        q = 127;
    }
    return (int8_t)q;
}

static float rs_dequant(int8_t q, float s, int32_t zp)
{
    return ((float)((int32_t)q - zp)) * s;
}

/* 稳定 softplus（与整图两形态同式，D1） */
static float rs_softplus(float x)
{
    float sp = (x > 0.0f) ? x : 0.0f;
    sp += log1pf(expf(-fabsf(x)));
    return sp;
}

static int rs_init_once(void)
{
    ems_stai_rt_init_once();
    if (rs_ready) {
        return 0;
    }

    /* ---- N1（CPU 形态）：init + 激活 arena ---- */
    if (stai_reattn1c_init(rs_ctx_n1) != STAI_SUCCESS) {
        printf("RS-FAIL n1init\r\n"); return 1;
    }
    {
        stai_ptr act_ptrs[STAI_REATTN1C_ACTIVATIONS_NUM] = { (stai_ptr)rs_n1_act };
        if (stai_reattn1c_set_activations(rs_ctx_n1, act_ptrs,
                                          STAI_REATTN1C_ACTIVATIONS_NUM) != STAI_SUCCESS) {
            printf("RS-FAIL n1act\r\n"); return 1;
        }
    }

    /* ---- N2（NPU 形态）：init + 量化参数（按 format/size 选取，不依赖顺序） ---- */
    {
        stai_network_info info;
        const stai_tensor *t_kf = NULL;
        const stai_tensor *t_hp = NULL;
        const stai_tensor *t_out = NULL;

        if (stai_reattn2n_init(rs_ctx_n2) != STAI_SUCCESS) {
            printf("RS-FAIL n2init\r\n"); return 1;
        }
        if (stai_reattn2n_get_info(rs_ctx_n2, &info) != STAI_SUCCESS) {
            printf("RS-FAIL n2info\r\n"); return 1;
        }
        if (info.n_inputs < 2u || info.n_outputs < 1u) {
            printf("RS-FAIL n2cnt %u/%u\r\n",
                   (unsigned)info.n_inputs, (unsigned)info.n_outputs); return 1;
        }
        for (stai_size i = 0u; i < info.n_inputs; i++) {
            const stai_tensor *t = &info.inputs[i];
            if (t->format == STAI_FORMAT_S8
                    && t->size_bytes == LL_ATON_REATTN2N_IN_1_SIZE_BYTES) {
                t_kf = t;
            } else if (t->format == STAI_FORMAT_FLOAT32
                    && t->size_bytes == LL_ATON_REATTN2N_IN_2_SIZE_BYTES) {
                t_hp = t;
            }
        }
        t_out = &info.outputs[0];
        /* 混合精度契约（形态漂移在此拒绝，而非静默量化错） */
        if (t_kf == NULL || t_hp == NULL
                || t_out->format != STAI_FORMAT_S8
                || t_out->size_bytes != LL_ATON_REATTN2N_OUT_1_SIZE_BYTES) {
            printf("RS-FAIL n2io kf=%p hp=%p of=%d os=%u\r\n",
                   (const void *)t_kf, (const void *)t_hp,
                   (int)t_out->format, (unsigned)t_out->size_bytes); return 1;
        }
        if (t_kf->scale.size != 1u || t_out->scale.size != 1u) {
            printf("RS-FAIL n2sc %u/%u\r\n",
                   (unsigned)t_kf->scale.size, (unsigned)t_out->scale.size); return 1;
        }
        rs_kf_scale = t_kf->scale.data[0];
        rs_kf_zp = (int32_t)t_kf->zeropoint.data[0];
        rs_out_scale = t_out->scale.data[0];
        rs_out_zp = (int32_t)t_out->zeropoint.data[0];
        printf("RS-Q kf=%.6f/%d out=%.6f/%d\r\n",
               (double)rs_kf_scale, (int)rs_kf_zp,
               (double)rs_out_scale, (int)rs_out_zp);
        rs_ready = true;
    }
    return 0;
}

int ems_reatt_query(const float hist_raw[EMS_REATT_HIST_N],
                    const float kf_raw[EMS_REATT_KF_N],
                    float q_out[EMS_REATT_Q_N],
                    uint32_t *infer_us)
{
    uint32_t t0 = rs_now_us();
    uint32_t t_n1;
    stai_return_code rc;

    if (rs_init_once() != 0) {
        return 1;
    }

    /* ---- 阶段 1：hist 归一化（冻结常数，ems_reatt_params.h） ---- */
    for (uint32_t t = 0u; t < EMS_REATT_H1_T; t++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_H1_CH_N; ch++) {
            uint32_t idx = t * EMS_REATT_H1_CH_N + ch;
            rs_n1_hist[idx] = (hist_raw[idx] - EMS_REATT_H1_MEAN[ch * EMS_REATT_H1_T + t])
                              / EMS_REATT_H1_STD[ch * EMS_REATT_H1_T + t];
        }
    }

    /* ---- 阶段 2：N1（TCN，CPU 形态，SYNC 全图）→ h_past[96] f32 ---- */
    {
        stai_ptr in_ptrs[STAI_REATTN1C_IN_NUM];
        stai_ptr out_ptrs[STAI_REATTN1C_OUT_NUM];
        in_ptrs[0] = (stai_ptr)rs_n1_hist;
        out_ptrs[0] = (stai_ptr)rs_n1_hpast;
        if (stai_reattn1c_set_inputs(rs_ctx_n1, in_ptrs, STAI_REATTN1C_IN_NUM) != STAI_SUCCESS
                || stai_reattn1c_set_outputs(rs_ctx_n1, out_ptrs, STAI_REATTN1C_OUT_NUM) != STAI_SUCCESS) {
            printf("RS-FAIL n1setio\r\n"); return 1;
        }
        rc = stai_reattn1c_run(rs_ctx_n1, STAI_MODE_SYNC);
        /* CPU 形态成功返回 STAI_DONE 而非 STAI_SUCCESS（板上实证语义） */
        if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
            printf("RS-FAIL n1run rc=%d\r\n", (int)rc); return 1;
        }
    }
    t_n1 = rs_now_us();

    /* ---- 阶段 3：kf 归一化 + 量化 → N2 输入 1（int8） ---- */
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_KF_CH_N; ch++) {
            uint32_t idx = k * EMS_REATT_KF_CH_N + ch;
            float v = (kf_raw[idx] - EMS_REATT_KF_MEAN[ch * EMS_REATT_KF_T + k])
                      / EMS_REATT_KF_SCALE[ch * EMS_REATT_KF_T + k];
            rs_n2_kf[idx] = rs_quant(v, rs_kf_scale, rs_kf_zp);
        }
    }

    /* ---- 阶段 4：N2（Attention/Dense，NPU 形态）----
     * h_past 与 N1 输出共用缓冲（零拷贝）；由 MCU 写入 → 交 NPU 前 CLEAN。 */
    SCB_CleanDCache_by_Addr((uint32_t *)rs_n1_hpast, (int32_t)sizeof(rs_n1_hpast));
    SCB_CleanDCache_by_Addr((uint32_t *)rs_n2_kf, (int32_t)sizeof(rs_n2_kf));
    SCB_InvalidateDCache_by_Addr(rs_n2_out, (int32_t)sizeof(rs_n2_out));

    {
        static uint32_t rs_diag_n;
        stai_ptr in_ptrs[LL_ATON_REATTN2N_IN_NUM];
        stai_ptr out_ptrs[LL_ATON_REATTN2N_OUT_NUM];
        in_ptrs[0] = (stai_ptr)rs_n2_kf;
        in_ptrs[1] = (stai_ptr)rs_n1_hpast;
        out_ptrs[0] = (stai_ptr)rs_n2_out;
        if (stai_reattn2n_set_inputs(rs_ctx_n2, in_ptrs, LL_ATON_REATTN2N_IN_NUM) != STAI_SUCCESS
                || stai_reattn2n_set_outputs(rs_ctx_n2, out_ptrs, LL_ATON_REATTN2N_OUT_NUM) != STAI_SUCCESS) {
            printf("RS-FAIL n2setio\r\n"); return 1;
        }
        if (rs_ran_before) {
            /* stai 契约：new_inference 仅在上一异步推理之后准备下一拍；首拍非法 */
            if (stai_ext_reattn2n_new_inference(rs_ctx_n2) != STAI_SUCCESS) {
                printf("RS-FAIL n2newinf\r\n"); return 1;
            }
        }
        rc = stai_reattn2n_run(rs_ctx_n2, STAI_MODE_ASYNC);
        if (rs_wait_done(rs_ctx_n2, rc, stai_ext_reattn2n_run_continue) != 0) {
            printf("RS-FAIL n2run rc=%d\r\n", (int)rc); return 1;
        }
        rs_ran_before = true;

        if (rs_diag_n < 2u) {
            rs_diag_n++;
            float hp_min = rs_n1_hpast[0];
            float hp_max = rs_n1_hpast[0];
            for (uint32_t i = 1u; i < 96u; i++) {
                float v = rs_n1_hpast[i];
                if (v < hp_min) { hp_min = v; }
                if (v > hp_max) { hp_max = v; }
            }
            printf("RS-N1 hp[0..3]=%.4f,%.4f,%.4f,%.4f min=%.4f max=%.4f | kf[0..2]=%d,%d,%d\r\n",
                   (double)rs_n1_hpast[0], (double)rs_n1_hpast[1],
                   (double)rs_n1_hpast[2], (double)rs_n1_hpast[3],
                   (double)hp_min, (double)hp_max,
                   (int)rs_n2_kf[0], (int)rs_n2_kf[1], (int)rs_n2_kf[2]);
            printf("RS-N2 raw=%d,%d,%d,%d,%d,%d m01=%.4f,%.4f a0=%.4f b0=%.4f\r\n",
                   (int)rs_n2_out[0], (int)rs_n2_out[1], (int)rs_n2_out[2],
                   (int)rs_n2_out[3], (int)rs_n2_out[4], (int)rs_n2_out[5],
                   (double)rs_dequant(rs_n2_out[0], rs_out_scale, rs_out_zp),
                   (double)rs_dequant(rs_n2_out[3], rs_out_scale, rs_out_zp),
                   (double)rs_dequant(rs_n2_out[1], rs_out_scale, rs_out_zp),
                   (double)rs_dequant(rs_n2_out[2], rs_out_scale, rs_out_zp));
        }
    }

    /* ---- 阶段 5：反量化 + 单调分位变换（kW 域） ---- */
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        float m = rs_dequant(rs_n2_out[k * 3u + 0u], rs_out_scale, rs_out_zp);
        float a = rs_dequant(rs_n2_out[k * 3u + 1u], rs_out_scale, rs_out_zp);
        float b = rs_dequant(rs_n2_out[k * 3u + 2u], rs_out_scale, rs_out_zp);
        float q10 = m - rs_softplus(a);
        float q90 = m + rs_softplus(b);
        q_out[k * 3u + 0u] = q10;
        q_out[k * 3u + 1u] = m;
        q_out[k * 3u + 2u] = q90;
        if (!isfinite(q10) || !isfinite(m) || !isfinite(q90)) {
            return 1;
        }
    }

    if (infer_us != NULL) {
        *infer_us = rs_now_us() - t0;
    }
    {
        static uint32_t rs_t_n;
        if (rs_t_n < 3u) {
            rs_t_n++;
            printf("RS-T n1=%lu n2=%lu total=%lu us\r\n",
                   (unsigned long)(t_n1 - t0), (unsigned long)(rs_now_us() - t_n1),
                   (unsigned long)(rs_now_us() - t0));
        }
    }
    return 0;
}

#endif /* EMS_REATT_SPLIT && EMS_REATT_SERVICE && NPU && tcnq */
