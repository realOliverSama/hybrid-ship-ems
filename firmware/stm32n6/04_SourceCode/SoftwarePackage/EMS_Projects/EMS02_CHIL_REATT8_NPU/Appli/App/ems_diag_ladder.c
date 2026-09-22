/* ems_diag_ladder.c — N2 逐级 bisection 阶梯执行器（phase4-g，2026-09-23 夜备）。
 *
 * 背景：NPU 通路已被平凡自检图（RS-SELF 输出=输入）与 workclf（40/40）证明正常；
 * REATT N2 图输出退化为"与输入无关的常数三元组"。本 TU 把 N2 的 4 张阶梯子图
 *   L1 dl1  kf → fut_proj+位置      （kf 输入通路）
 *   L2 dl2  单块 attention           （Q/K/V、QKᵀ、scale、Softmax、AV、o_proj）
 *   L3 dl3  h_past → Tile            （Unsqueeze/Reshape/Q/DQ/Slice/Tile——头号嫌疑）
 *   L4 dl4  fusion+decoder           （concat/LN/Dense/头部，golden fut_seq+h_tile 喂入）
 * 加整图 --Ono-clone-dma 消融（F1 dlf1）装进同一固件，首次 REATT query 时依次执行，
 * 与 ORT golden（ems_diag_ladder_golden.h，禁手改）在 float 域自比对并逐级给一行判决。
 *
 * 判读：首个 FAIL 级 = 首个坏段 → 修复 = 该段退回 CPU 形态（N1 同款已证可靠），其余留 NPU；
 * L1–L4 全 PASS 而 F1 FAIL ⇒ 排除图结构，指向编译选项（clone_dma 等）；
 * F1 PASS ⇒ 直接把服务 N2 重编译为 --Ono-clone-dma 即修复。
 *
 * 生命周期：每 run 前 new_inference，容忍首拍 STILL_RUNNING(0x50000)（官方语义）。
 * 数据生成：Host/tools/npu/npu_export_ladder.py（cut / compile / emit-c）。
 */
#if defined(EMS_REATT_SERVICE)

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "stai.h"
#include "stai_ext.h"

#include "ems_stai_rt.h"
#include "ems_diag_ladder_golden.h"

#include "../../Model/reatt8_q_npu/ladder/dl1/dl1.h"
#include "../../Model/reatt8_q_npu/ladder/dl2/dl2.h"
#include "../../Model/reatt8_q_npu/ladder/dl3/dl3.h"
#include "../../Model/reatt8_q_npu/ladder/dl4/dl4.h"
#include "../../Model/reatt8_q_npu/ladder/dlf1/dlf1.h"
#include "../../Model/reatt8_q_npu/ladder/dl1/stai_dl1.h"
#include "../../Model/reatt8_q_npu/ladder/dl2/stai_dl2.h"
#include "../../Model/reatt8_q_npu/ladder/dl3/stai_dl3.h"
#include "../../Model/reatt8_q_npu/ladder/dl4/stai_dl4.h"
#include "../../Model/reatt8_q_npu/ladder/dlf1/stai_dlf1.h"

STAI_NETWORK_CONTEXT_DECLARE(dl_ctx_dl1, STAI_DL1_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(dl_ctx_dl2, STAI_DL2_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(dl_ctx_dl3, STAI_DL3_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(dl_ctx_dl4, STAI_DL4_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(dl_ctx_dlf1, STAI_DLF1_CONTEXT_SIZE)

#define DL_MAX_ELEMS  4096u
#define DL_MAX_BYTES  16384u
#define DL_MAX_INS    2u

typedef stai_return_code (*dl_fn_simple)(stai_network *);
typedef stai_return_code (*dl_fn_info)(stai_network *, stai_network_info *);
typedef stai_return_code (*dl_fn_io)(stai_network *, const stai_ptr *, stai_size);
typedef stai_return_code (*dl_fn_run)(stai_network *, const stai_run_mode);

static int8_t dl_i8_in[DL_MAX_INS][DL_MAX_ELEMS];
static float dl_f32_in[DL_MAX_INS][DL_MAX_ELEMS];
static uint8_t dl_out_raw[DL_MAX_BYTES];

static int8_t dl_quant(float x, float s, int32_t zp)
{
    int32_t q = (int32_t)lrintf(x / s) + zp;
    if (q < -128) {
        q = -128;
    } else if (q > 127) {
        q = 127;
    }
    return (int8_t)q;
}

static int dl_wait_done(stai_network *ctx, stai_return_code rc,
                        dl_fn_simple cont)
{
    while (rc != STAI_DONE) {
        if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
            return 1;
        }
        if (rc == STAI_RUNNING_WFE) {
            stai_ext_wfe();
        }
        rc = cont(ctx);
    }
    return 0;
}

static int dl_run_stage_impl(const char *tag, stai_network *ctx,
                             dl_fn_simple init, dl_fn_info get_info,
                             dl_fn_io set_inputs, dl_fn_io set_outputs,
                             dl_fn_run run, dl_fn_simple cont,
                             dl_fn_simple new_inf,
                             const float * const *ins, const uint32_t *in_elems,
                             const float *exp)
{
    stai_network_info info;
    stai_ptr in_ptrs[DL_MAX_INS], out_ptrs[1];
    stai_return_code rc;
    uint32_t n_in;
    float tol, maxd = 0.0f, out_scale = 1.0f;
    int32_t out_zp = 0;
    uint32_t out_elems, match = 0;
    int out_is_f32;
    uint32_t used[DL_MAX_INS] = { 0u, 0u };

    ems_stai_rt_init_once();
    if (init(ctx) != STAI_SUCCESS) {
        printf("RS-%s FAIL init\r\n", tag);
        return 1;
    }
    if (get_info(ctx, &info) != STAI_SUCCESS || info.n_inputs > DL_MAX_INS) {
        printf("RS-%s FAIL info\r\n", tag);
        return 1;
    }
    n_in = info.n_inputs;
    for (stai_size i = 0u; i < n_in; i++) {
        const stai_tensor *t = &info.inputs[i];
        int is_f32 = (t->format == STAI_FORMAT_FLOAT32);
        uint32_t elems = is_f32 ? (uint32_t)(t->size_bytes / 4u)
                                : (uint32_t)t->size_bytes;
        int j = -1;
        for (uint32_t k = 0u; k < DL_MAX_INS; k++) {
            if (!used[k] && in_elems[k] == elems) {
                j = (int)k;
                used[k] = 1u;
                break;
            }
        }
        if (j < 0) {
            printf("RS-%s FAIL ingold %u\r\n", tag, (unsigned)elems);
            return 1;
        }
        if (is_f32) {
            memcpy(dl_f32_in[i], ins[j], elems * sizeof(float));
            in_ptrs[i] = (stai_ptr)dl_f32_in[i];
        } else {
            float s = (t->scale.size == 1u) ? t->scale.data[0] : 1.0f;
            int32_t zp = (t->zeropoint.size == 1u) ? (int32_t)t->zeropoint.data[0] : 0;
            for (uint32_t e = 0u; e < elems; e++) {
                dl_i8_in[i][e] = dl_quant(ins[j][e], s, zp);
            }
            in_ptrs[i] = (stai_ptr)dl_i8_in[i];
        }
    }
    rc = set_inputs(ctx, in_ptrs, n_in);
    if (rc != STAI_SUCCESS) {
        printf("RS-%s FAIL setin rc=%d\r\n", tag, (int)rc);
        return 1;
    }
    /* 输出契约：f32 → 元素数 = bytes/4；S8 → 元素数 = bytes，反量化后比对 */
    rc = get_info(ctx, &info);
    (void)rc;
    if (info.outputs[0].format == STAI_FORMAT_FLOAT32) {
        out_is_f32 = 1;
        out_elems = (uint32_t)(info.outputs[0].size_bytes / 4u);
        /* f32 输出用相对容差：板上输入经 S8 量化（≤0.5 LSB）传播的偏差可到
         * 百分之几量级；结构化坏段（常数输出/通道错位）偏差为 100%+ 量级，
         * 5% 阈值足以区分，maxd 一并打印供人工复核 */
        float mabs = 0.0f;
        for (uint32_t e = 0u; e < out_elems && e < DL_MAX_ELEMS; e++) {
            float a = exp[e] < 0.0f ? -exp[e] : exp[e];
            if (a > mabs) {
                mabs = a;
            }
        }
        tol = 0.05f * (1.0f + mabs);
    } else {
        out_is_f32 = 0;
        out_elems = (uint32_t)info.outputs[0].size_bytes;
        out_scale = (info.outputs[0].scale.size == 1u)
                    ? info.outputs[0].scale.data[0] : 1.0f;
        out_zp = (info.outputs[0].zeropoint.size == 1u)
                 ? (int32_t)info.outputs[0].zeropoint.data[0] : 0;
        tol = 2.0f * out_scale;
    }
    SCB_CleanDCache_by_Addr((uint32_t *)dl_f32_in, (int32_t)sizeof(dl_f32_in));
    SCB_CleanDCache_by_Addr((uint32_t *)dl_i8_in, (int32_t)sizeof(dl_i8_in));
    SCB_InvalidateDCache_by_Addr(dl_out_raw, (int32_t)sizeof(dl_out_raw));
    out_ptrs[0] = (stai_ptr)dl_out_raw;
    rc = set_outputs(ctx, out_ptrs, info.n_outputs);
    if (rc != STAI_SUCCESS) {
        printf("RS-%s FAIL setout rc=%d\r\n", tag, (int)rc);
        return 1;
    }
    rc = new_inf(ctx);
    if (rc != STAI_SUCCESS && rc != STAI_ERROR_NETWORK_STILL_RUNNING) {
        printf("RS-%s FAIL newinf rc=%d\r\n", tag, (int)rc);
        return 1;
    }
    rc = run(ctx, STAI_MODE_ASYNC);
    if (dl_wait_done(ctx, rc, cont) != 0) {
        printf("RS-%s FAIL run rc=%d\r\n", tag, (int)rc);
        return 1;
    }
    SCB_InvalidateDCache_by_Addr(dl_out_raw, (int32_t)sizeof(dl_out_raw));
    if (out_elems > DL_MAX_ELEMS) {
        out_elems = DL_MAX_ELEMS;
    }
    for (uint32_t e = 0u; e < out_elems; e++) {
        float v;
        if (out_is_f32) {
            memcpy(&v, &dl_out_raw[e * 4u], 4u);   /* little-endian f32 直读 */
        } else {
            v = ((float)((int8_t)dl_out_raw[e]) - (float)out_zp) * out_scale;
        }
        float d = fabsf(v - exp[e]);
        if (d > maxd) {
            maxd = d;
        }
        if (d <= tol) {
            match++;
        }
    }
    printf("RS-%s match=%u/%u maxd=%.4g tol=%.4g %s\r\n", tag,
           (unsigned)match, (unsigned)out_elems,
           (double)maxd, (double)tol, (match == out_elems) ? "PASS" : "DIFF");
    return (match == out_elems) ? 0 : 2;
}

void ems_diag_ladder_run(void)
{
    /* L1 kf → fut_proj+位置 */
    {
        const float *ins[] = { dl_l1_in0 };
        const uint32_t ne[] = { (uint32_t)(sizeof(dl_l1_in0) / sizeof(float)) };
        (void)dl_run_stage_impl("L1", dl_ctx_dl1, stai_dl1_init, stai_dl1_get_info,
                                stai_dl1_set_inputs, stai_dl1_set_outputs,
                                stai_dl1_run, stai_ext_dl1_run_continue,
                                stai_ext_dl1_new_inference,
                                ins, ne, dl_l1_exp);
    }
    /* L2 单块 attention */
    {
        const float *ins[] = { dl_l2_in0 };
        const uint32_t ne[] = { (uint32_t)(sizeof(dl_l2_in0) / sizeof(float)) };
        (void)dl_run_stage_impl("L2", dl_ctx_dl2, stai_dl2_init, stai_dl2_get_info,
                                stai_dl2_set_inputs, stai_dl2_set_outputs,
                                stai_dl2_run, stai_ext_dl2_run_continue,
                                stai_ext_dl2_new_inference,
                                ins, ne, dl_l2_exp);
    }
    /* L3 h_past → Tile（头号嫌疑） */
    {
        const float *ins[] = { dl_l3_in0 };
        const uint32_t ne[] = { (uint32_t)(sizeof(dl_l3_in0) / sizeof(float)) };
        (void)dl_run_stage_impl("L3", dl_ctx_dl3, stai_dl3_init, stai_dl3_get_info,
                                stai_dl3_set_inputs, stai_dl3_set_outputs,
                                stai_dl3_run, stai_ext_dl3_run_continue,
                                stai_ext_dl3_new_inference,
                                ins, ne, dl_l3_exp);
    }
    /* L4 fusion+decoder（golden fut_seq + h_tile） */
    {
        const float *ins[] = { dl_l4_in0, dl_l4_in1 };
        const uint32_t ne[] = { (uint32_t)(sizeof(dl_l4_in0) / sizeof(float)),
                                (uint32_t)(sizeof(dl_l4_in1) / sizeof(float)) };
        (void)dl_run_stage_impl("L4", dl_ctx_dl4, stai_dl4_init, stai_dl4_get_info,
                                stai_dl4_set_inputs, stai_dl4_set_outputs,
                                stai_dl4_run, stai_ext_dl4_run_continue,
                                stai_ext_dl4_new_inference,
                                ins, ne, dl_l4_exp);
    }
    /* F1 整图 --Ono-clone-dma 消融（golden kf + h_past） */
    {
        const float *ins[] = { dl_f1_in0, dl_f1_in1 };
        const uint32_t ne[] = { (uint32_t)(sizeof(dl_f1_in0) / sizeof(float)),
                                (uint32_t)(sizeof(dl_f1_in1) / sizeof(float)) };
        (void)dl_run_stage_impl("F1", dl_ctx_dlf1, stai_dlf1_init, stai_dlf1_get_info,
                                stai_dlf1_set_inputs, stai_dlf1_set_outputs,
                                stai_dlf1_run, stai_ext_dlf1_run_continue,
                                stai_ext_dlf1_new_inference,
                                ins, ne, dl_f1_exp);
    }
}

#endif /* EMS_REATT_SERVICE */
