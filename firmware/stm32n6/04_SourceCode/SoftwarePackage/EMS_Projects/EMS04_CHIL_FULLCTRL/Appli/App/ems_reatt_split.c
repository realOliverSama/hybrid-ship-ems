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
#include <string.h>

#include "stm32n6xx_hal.h"
#include "stai.h"
#include "stai_ext.h"

#include "ems_stai_rt.h"
#include "ems_reatt.h"
#include "ems_reatt_params.h"
/* phase4-g NPU2：N1 整支路迁 NPU（reattn1n，EMS03 时延优化线）。生成码 I/O
 * 契约与 CPU 版逐位对齐：hist f32 [1,300,11] 13,200 B（OE 图无布局变换，t-major
 * 直写兼容）+ h_past f32 384 B；激活由编译期 atonbuf 规划（npuRAM6 398 KB），
 * 应用不再提供 arena。权重 blob 修补后 @0x70B00000（BUG-1 复审 0 不符 +
 * BUG-3 swconv 1 站点替换，2026-09-24）。 */
#include "../../Model/reatt8_q_npu/npu_n1/gen/reattn1n.h"
#include "../../Model/reatt8_q_npu/npu_n1/gen/stai_reattn1n.h"
#include "../../Model/reatt8_q_npu/npu_safe/n2_lnfloat/reattn2l.h"
#include "../../Model/reatt8_q_npu/npu_safe/n2_lnfloat/stai_reattn2l.h"

#if defined(EMS_N2_EPCB) && EMS_N2_EPCB
/* ---- N2 逐 epoch-block 身份精确计时（2026-09-25 时延优化线）----
 * stai 层每次 run 按 nn_context->callback 注册运行时 epoch 回调（ll_aton_stai_internal.c
 * __ll_aton_stai_run），回调在每块 PRE_START/POST_END 触发；记录 (块指针,dt) 表，
 * 首个真实查询的 N2 完成后倒出。主机侧按 base+sizeof(EpochBlock_ItemTypeDef)
 * 对齐 reattn2l.c 的 ll_atonn_rt_epoch_block_array 索引 → 算子身份。
 * 附：一次性首触读带宽 bench（NOR/HyperRAM/AXISRAM 各 256KB）。 */
#include "stai_aton_common.h"
#include "ll_aton_NN_interface.h"
extern stai_network rs_ctx_n2[]; /* 定义在本文件后段（STAI_NETWORK_CONTEXT_DECLARE） */
#define RS_EPCB_N 768u
static uintptr_t rs_epcb_base;
static uint32_t rs_epcb_ptr[RS_EPCB_N];
static uint32_t rs_epcb_dt[RS_EPCB_N];
static uint32_t rs_epcb_n;
static uint32_t rs_epcb_open_t0;
static uint32_t rs_epcb_open_ptr;

static void rs_n2_epcb_cb(void *cookie, const stai_event_type ev, const void *payload)
{
    (void)cookie;
    if (payload == NULL) {
        return;
    }
    if (ev == (stai_event_type)LL_ATON_RT_Callbacktype_PRE_START) {
        uintptr_t p = (uintptr_t)payload;
        if (rs_epcb_base == 0u || p < rs_epcb_base) {
            rs_epcb_base = p;
        }
        rs_epcb_open_ptr = (uint32_t)p;
        rs_epcb_open_t0 = DWT->CYCCNT;
    } else if (ev == (stai_event_type)LL_ATON_RT_Callbacktype_POST_END) {
        if (rs_epcb_n < RS_EPCB_N) {
            rs_epcb_ptr[rs_epcb_n] = rs_epcb_open_ptr;
            rs_epcb_dt[rs_epcb_n] = DWT->CYCCNT - rs_epcb_open_t0;
            rs_epcb_n++;
        }
    }
}

static void rs_n2_epcb_register(void)
{
    _stai_aton_context *c2 = (_stai_aton_context *)(void *)rs_ctx_n2;
    c2->callback = rs_n2_epcb_cb;
    c2->callback_cookie = NULL;
}

static void rs_n2_epcb_dump(void)
{
    printf("RS-EPCB n=%lu base=%08lx item=%u\r\n",
           (unsigned long)rs_epcb_n, (unsigned long)rs_epcb_base,
           (unsigned)sizeof(EpochBlock_ItemTypeDef));
    for (uint32_t i = 0u; i < rs_epcb_n; i++) {
        printf("RS-EPCB %lu %08lx %lu\r\n", (unsigned long)i,
               (unsigned long)rs_epcb_ptr[i], (unsigned long)rs_epcb_dt[i]);
    }
}

static void rs_mem_bench(void)
{
    /* 首触读带宽代理测量（CPU 视角，32KB D-cache 一次扫穿）：NPU 侧绝对值不同，
     * 但 NOR/HyperRAM/AXISRAM 的相对档位用于判定权重迁移收益上限。 */
    static const struct {
        const char *name;
        volatile const uint32_t *p;
    } srcs[3] = {
        { "nor", (volatile const uint32_t *)0x70A00000u },
        { "hyper", (volatile const uint32_t *)0x90000000u },
        { "axis", (volatile const uint32_t *)0x34200000u },
    };
    uint32_t sink = 0u;
    for (uint32_t s = 0u; s < 3u; s++) {
        uint32_t t0 = DWT->CYCCNT;
        uint32_t acc = 0u;
        for (uint32_t i = 0u; i < (256u * 1024u / 4u); i++) {
            acc += srcs[s].p[i];
        }
        uint32_t dt = DWT->CYCCNT - t0;
        sink += acc;
        printf("RS-MB %s 256KB %lu cyc\r\n", srcs[s].name, (unsigned long)dt);
    }
    printf("RS-MB sink=%08lx\r\n", (unsigned long)sink);
}
#endif /* EMS_N2_EPCB */

int ems_reatt_n2_golden_probe(const float kf_norm[EMS_REATT_KF_N],
                              const float h_past[96u],
                              int8_t raw_out[EMS_REATT_Q_N]);
#if defined(EMS_N2_S0) && EMS_N2_S0
#include "ems_diag_ladder_golden.h" /* dl_f1_in0/dl_f1_in1 golden（TU 私有副本，探针专用） */
static void rs_s0_probe_once(void)
{
    int8_t raw[90];
    if (ems_reatt_n2_golden_probe(dl_f1_in0, dl_f1_in1, raw) == 0) {
        printf("RS-S0 raw=");
        for (uint32_t e = 0u; e < 90u; e++) {
            printf("%d,", (int)raw[e]);
        }
        printf("\r\n");
    } else {
        printf("RS-S0 FAIL\r\n");
    }
}
#endif /* EMS_N2_S0 */

/* 两网络上下文（生成宏展开为 stai_network=uint8_t 字节数组，stai.h） */
STAI_NETWORK_CONTEXT_DECLARE(rs_ctx_n1, STAI_REATTN1N_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(rs_ctx_n2, STAI_REATTN2L_CONTEXT_SIZE)

/* N1（NPU 形态）：I/O f32 user-allocated，激活在 NPURAM（编译期规划） */
static float rs_n1_hist[EMS_REATT_HIST_N] __attribute__((aligned(32)));
static float rs_n1_hpast[96u] __attribute__((aligned(32)));

/* N2（NPU 形态）：kf int8 + h_past f32（与 N1 输出同缓冲，零拷贝）+ 输出 int8 */
static int8_t rs_n2_kf[LL_ATON_REATTN2L_IN_1_SIZE_BYTES] __attribute__((aligned(32)));
static int8_t rs_n2_out[LL_ATON_REATTN2L_OUT_1_SIZE_BYTES] __attribute__((aligned(32)));

static float rs_kf_scale;
static int32_t rs_kf_zp;
static float rs_out_scale;
static int32_t rs_out_zp;
static bool rs_ready;
static bool rs_ran_before; /* N2 new_inference 仅在上一次异步推理之后需要（stai 契约） */
static bool rs_n1_ran_before; /* N1 同契约（NPU 异步形态，NPU2 线） */

static int rs_wait_done(stai_network *ctx, stai_return_code rc,
                        stai_return_code (*run_continue)(stai_network *));

/* ---- D2 判别诊断（2026-09-23）：NOR 权重区 CPU 侧校验和 + NPU 平凡自检图 ----
 * 自检图 = 双恒等 MatMul（scale 1.0 / zp 0）⇒ 期望输出 = 输入 int8 逐字节相等；
 * 权重 145 B 落 NOR 0x703E0000（phase4-g D2 池），激活 320 B npuRAM5。 */
#include "../../Model/reatt8_q_npu/selftest/reattself.h"
#include "../../Model/reatt8_q_npu/selftest/stai_reattself.h"

STAI_NETWORK_CONTEXT_DECLARE(rs_ctx_self, STAI_REATTSELF_CONTEXT_SIZE)

static int8_t rs_self_in[LL_ATON_REATTSELF_IN_1_SIZE_BYTES] __attribute__((aligned(32)));
static int8_t rs_self_out[LL_ATON_REATTSELF_OUT_1_SIZE_BYTES] __attribute__((aligned(32)));

/* 简单滚动和（与 host 侧 python 同式，用于比对 appli 读 NOR 的内容） */
static uint32_t rs_nor_sum(const volatile uint8_t *p, uint32_t n)
{
    uint32_t s = 0u;
    for (uint32_t i = 0u; i < n; i++) {
        s = s * 31u + (uint32_t)p[i];
    }
    return s;
}

static void rs_nor_probe(void)
{
    const volatile uint8_t *n2 = (const volatile uint8_t *)0x70A00000u; /* N2 权重（池已迁 0x70A00000） */
    const volatile uint8_t *sf = (const volatile uint8_t *)0x703E0000u; /* 自检图权重 */
    const volatile uint8_t *wc = (const volatile uint8_t *)0x70400000u; /* workclf 权重 */
    printf("RS-NOR n2=%08lx,%02x%02x%02x%02x sf=%08lx,%02x%02x%02x%02x wc=%08lx,%02x%02x%02x%02x\r\n",
           (unsigned long)rs_nor_sum(n2, 4096u), n2[0], n2[1], n2[2], n2[3],
           (unsigned long)rs_nor_sum(sf, 145u), sf[0], sf[1], sf[2], sf[3],
           (unsigned long)rs_nor_sum(wc, 4096u), wc[0], wc[1], wc[2], wc[3]);
}

static int rs_selftest_run(void)
{
    static const int8_t probe[LL_ATON_REATTSELF_IN_1_SIZE_BYTES] =
        { -128, -100, -1, 0, 1, 50, 100, 127 };
    stai_ptr in_ptrs[LL_ATON_REATTSELF_IN_NUM];
    stai_ptr out_ptrs[LL_ATON_REATTSELF_OUT_NUM];
    stai_return_code rc;

    for (uint32_t i = 0u; i < sizeof(probe); i++) {
        rs_self_in[i] = probe[i];
    }
    rc = stai_reattself_init(rs_ctx_self);
    if (rc != STAI_SUCCESS) {
        printf("RS-SELF init rc=%d\r\n", (int)rc);
        return 1;
    }
    SCB_CleanDCache_by_Addr((uint32_t *)rs_self_in, (int32_t)sizeof(rs_self_in));
    SCB_InvalidateDCache_by_Addr(rs_self_out, (int32_t)sizeof(rs_self_out));
    in_ptrs[0] = (stai_ptr)rs_self_in;
    out_ptrs[0] = (stai_ptr)rs_self_out;
    rc = stai_reattself_set_inputs(rs_ctx_self, in_ptrs, LL_ATON_REATTSELF_IN_NUM);
    if (rc != STAI_SUCCESS) {
        printf("RS-SELF setin rc=%d\r\n", (int)rc);
        return 1;
    }
    rc = stai_reattself_set_outputs(rs_ctx_self, out_ptrs, LL_ATON_REATTSELF_OUT_NUM);
    if (rc != STAI_SUCCESS) {
        printf("RS-SELF setout rc=%d\r\n", (int)rc);
        return 1;
    }
    /* 首拍 STILL_RUNNING(0x50000) 预期内忽略（官方示例同语义，见 ems_infer_npu.c 注） */
    rc = stai_ext_reattself_new_inference(rs_ctx_self);
    if (rc != STAI_SUCCESS && rc != STAI_ERROR_NETWORK_STILL_RUNNING) {
        printf("RS-SELF newinf rc=%d\r\n", (int)rc);
        return 1;
    }
    rc = stai_reattself_run(rs_ctx_self, STAI_MODE_ASYNC);
    if (rs_wait_done(rs_ctx_self, rc, stai_ext_reattself_run_continue) != 0) {
        printf("RS-SELF run rc=%d err=%d\r\n", (int)rc,
               (int)stai_reattself_get_error(rs_ctx_self));
        return 1;
    }
    SCB_InvalidateDCache_by_Addr(rs_self_out, (int32_t)sizeof(rs_self_out));
    printf("RS-SELF in=[-128 -100 -1 0 1 50 100 127] out=[%d %d %d %d %d %d %d %d]\r\n",
           (int)rs_self_out[0], (int)rs_self_out[1], (int)rs_self_out[2],
           (int)rs_self_out[3], (int)rs_self_out[4], (int)rs_self_out[5],
           (int)rs_self_out[6], (int)rs_self_out[7]);
    return 0;
}

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

#if defined(EMS_N2_PROF) && EMS_N2_PROF
/* N2 逐 epoch 块耗时：每次 run_continue 返回=一个 epoch 块完成/推进 */
#define RS_N2EPROF_N 2048u
static uint32_t rs_n2eprof[RS_N2EPROF_N];
static uint32_t rs_n2eprof_n;
static int rs_wait_done_prof(stai_network *ctx, stai_return_code rc,
                             stai_return_code (*run_continue)(stai_network *))
{
    while (rc != STAI_DONE) {
        if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
            return 1;
        }
        if (rc == STAI_RUNNING_WFE) {
            stai_ext_wfe();
        }
        uint32_t t1 = DWT->CYCCNT;
        rc = run_continue(ctx);
        uint32_t dt = DWT->CYCCNT - t1;
        if (rs_n2eprof_n < RS_N2EPROF_N) {
            rs_n2eprof[rs_n2eprof_n++] = dt;
        }
    }
    return 0;
}

static void rs_n2eprof_report(void)
{
    const uint32_t buckets[5] = { 1000u, 10000u, 100000u, 1000000u, 0u };
    const char *bn[5] = { "<1k", "<10k", "<100k", "<1M", ">=1M" };
    uint32_t bc[5] = { 0, 0, 0, 0, 0 };
    uint64_t tot = 0u;
    uint32_t top5[5] = { 0, 0, 0, 0, 0 }, top5i[5] = { 0, 0, 0, 0, 0 };
    for (uint32_t i = 0u; i < rs_n2eprof_n; i++) {
        uint32_t v = rs_n2eprof[i];
        tot += v;
        uint32_t bi = 4u;
        for (uint32_t k = 0u; k < 4u; k++) {
            if (v < buckets[k]) { bi = k; break; }
        }
        bc[bi]++;
        for (uint32_t k = 0u; k < 5u; k++) {
            if (v > top5[k]) {
                for (uint32_t m = 4u; m > k; m--) {
                    top5[m] = top5[m - 1u]; top5i[m] = top5i[m - 1u];
                }
                top5[k] = v; top5i[k] = i;
                break;
            }
        }
    }
    printf("RS-N2EP n=%u total_ms=%lu\r\n", (unsigned)rs_n2eprof_n,
           (unsigned long)(tot / (SystemCoreClock / 1000u)));
    for (uint32_t k = 0u; k < 5u; k++) {
        printf("RS-N2EP %s %u\r\n", bn[k], (unsigned)bc[k]);
    }
    for (uint32_t k = 0u; k < 5u; k++) {
        printf("RS-N2EP top idx=%u ms=%lu\r\n", (unsigned)top5i[k],
               (unsigned long)(top5[k] / (SystemCoreClock / 1000u)));
    }
    rs_n2eprof_n = 0u;
}
#endif /* EMS_N2_PROF */

#if defined(EMS_N2_PROF) && EMS_N2_PROF
static int rs_wait_done_prof_n2(stai_network *ctx, stai_return_code rc,
                                stai_return_code (*run_continue)(stai_network *))
{
    rs_n2eprof_n = 0u;
    return rs_wait_done_prof(ctx, rc, run_continue);
}
#else
static int rs_wait_done_prof_n2(stai_network *ctx, stai_return_code rc,
                                stai_return_code (*run_continue)(stai_network *))
{
    return rs_wait_done(ctx, rc, run_continue);
}
#endif

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

    /* ---- N1（NPU 形态）：init（激活由编译期 atonbuf 规划，无需 arena） ---- */
    if (stai_reattn1n_init(rs_ctx_n1) != STAI_SUCCESS) {
        printf("RS-FAIL n1init\r\n"); return 1;
    }

    /* ---- N2（NPU 形态）：init + 量化参数（按 format/size 选取，不依赖顺序） ---- */
    {
        stai_network_info info;
        const stai_tensor *t_kf = NULL;
        const stai_tensor *t_hp = NULL;
        const stai_tensor *t_out = NULL;

        if (stai_reattn2l_init(rs_ctx_n2) != STAI_SUCCESS) {
            printf("RS-FAIL n2init\r\n"); return 1;
        }
        if (stai_reattn2l_get_info(rs_ctx_n2, &info) != STAI_SUCCESS) {
            printf("RS-FAIL n2info\r\n"); return 1;
        }
        if (info.n_inputs < 2u || info.n_outputs < 1u) {
            printf("RS-FAIL n2cnt %u/%u\r\n",
                   (unsigned)info.n_inputs, (unsigned)info.n_outputs); return 1;
        }
        for (stai_size i = 0u; i < info.n_inputs; i++) {
            const stai_tensor *t = &info.inputs[i];
            if (t->format == STAI_FORMAT_S8
                    && t->size_bytes == LL_ATON_REATTN2L_IN_1_SIZE_BYTES) {
                t_kf = t;
            } else if (t->format == STAI_FORMAT_FLOAT32
                    && t->size_bytes == LL_ATON_REATTN2L_IN_2_SIZE_BYTES) {
                t_hp = t;
            }
        }
        t_out = &info.outputs[0];
        /* 混合精度契约（形态漂移在此拒绝，而非静默量化错） */
        if (t_kf == NULL || t_hp == NULL
                || t_out->format != STAI_FORMAT_S8
                || t_out->size_bytes != LL_ATON_REATTN2L_OUT_1_SIZE_BYTES) {
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

/* S0 判别探针：golden 输入（kf 归一化 f32 + h_past f32）直接走**服务生命周期**
 * 跑 reattn2l，输出原始 int8 90 B。若 S0 输出正确而服务路径退化 ⇒ 毒在 N1 的
 * h_past；若 S0 也退化 ⇒ 图/编译本身坏（与输入无关）。
 * 2026-09-24 两处修正（ORT A/B 实锤）：
 *  ① kf_norm 已是 norm_ext 域，不再套 KF_MEAN/SCALE（原写法=双重归一化）；
 *  ② reattn2l 编译图把 kf 输入声明为 [1,6,30]（内部 Transpose 回 [1,30,6]），
 *     物理缓冲必须 ch-major（转置序）写入——转置喂入 maxd=18.1kW vs golden，
 *     直写序 maxd=315.9kW。 */
int ems_reatt_n2_golden_probe(const float kf_norm[EMS_REATT_KF_N],
                              const float h_past[96u],
                              int8_t raw_out[EMS_REATT_Q_N])
{
    stai_ptr in_ptrs[LL_ATON_REATTN2L_IN_NUM], out_ptrs[1];
    stai_return_code rc;

    if (rs_init_once() != 0) {
        return 1;
    }
    /* 2026-09-24 修复：kf_norm 已是 norm_ext 归一化域（golden dl_f1_in0），
     * 不可再套 (v-KF_MEAN)/KF_SCALE（双重归一化）；且编译图 kf 输入为
     * [1,6,30] ch-major，本缓冲必须转置序写入（idx=ch*30+k）。 */
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_KF_CH_N; ch++) {
            rs_n2_kf[ch * EMS_REATT_KF_T + k] = rs_quant(kf_norm[k * EMS_REATT_KF_CH_N + ch],
                                                         rs_kf_scale, rs_kf_zp);
        }
    }
    memcpy(rs_n1_hpast, h_past, 96u * sizeof(float));
    SCB_CleanDCache_by_Addr((uint32_t *)rs_n1_hpast, (int32_t)sizeof(rs_n1_hpast));
    SCB_CleanDCache_by_Addr((uint32_t *)rs_n2_kf, (int32_t)sizeof(rs_n2_kf));
    SCB_InvalidateDCache_by_Addr(rs_n2_out, (int32_t)sizeof(rs_n2_out));
    in_ptrs[0] = (stai_ptr)rs_n2_kf;
    in_ptrs[1] = (stai_ptr)rs_n1_hpast;
    out_ptrs[0] = (stai_ptr)rs_n2_out;
    if (stai_reattn2l_set_inputs(rs_ctx_n2, in_ptrs, LL_ATON_REATTN2L_IN_NUM) != STAI_SUCCESS
            || stai_reattn2l_set_outputs(rs_ctx_n2, out_ptrs, LL_ATON_REATTN2L_OUT_NUM) != STAI_SUCCESS) {
        printf("RS-S0 FAIL setio\r\n");
        return 1;
    }
    if (rs_ran_before) {
        if (stai_ext_reattn2l_new_inference(rs_ctx_n2) != STAI_SUCCESS) {
            printf("RS-S0 FAIL newinf\r\n");
            return 1;
        }
    }
    rc = stai_reattn2l_run(rs_ctx_n2, STAI_MODE_ASYNC);
    if (rs_wait_done(rs_ctx_n2, rc, stai_ext_reattn2l_run_continue) != 0) {
        printf("RS-S0 FAIL run\r\n");
        return 1;
    }
    rs_ran_before = true;
    SCB_InvalidateDCache_by_Addr(rs_n2_out, (int32_t)sizeof(rs_n2_out));
    memcpy(raw_out, rs_n2_out, EMS_REATT_Q_N);
    return 0;
}

int ems_reatt_query(const float hist_raw[EMS_REATT_HIST_N],
                    const float kf_raw[EMS_REATT_KF_N],
                    float q_out[EMS_REATT_Q_N],
                    uint32_t *infer_us)
{
    uint32_t t0 = rs_now_us();
    const uint32_t t0_cyc = DWT->CYCCNT;  /* infer_us 回绕安全基准（周期域） */
    uint32_t t_n1;
    stai_return_code rc;

    if (rs_init_once() != 0) {
        return 1;
    }

#if defined(EMS_N2_S0) && EMS_N2_S0
    /* S0 独立探针：首个真实查询前跑 golden 输入（先于 EPCB 注册，不污染块表） */
    {
        static bool rs_s0_done;
        if (!rs_s0_done) {
            rs_s0_done = true;
            rs_s0_probe_once();
        }
    }
#endif
#if defined(EMS_N2_EPCB) && EMS_N2_EPCB
    {
        static bool rs_epcb_armed;
        if (!rs_epcb_armed) {
            rs_epcb_armed = true;
            rs_mem_bench();
            rs_n2_epcb_register();
        }
    }
#endif

    /* ---- D2 判别诊断：仅首次 query 执行（NOR 校验和 + 平凡自检图 + 阶梯/S0） ----
     * 生产关闭：阶梯含 5 级对拍 + S0，首窗连带 ~40s，不得进 soft-HIL 航次。
     * 回开：cmake -DEMS_REATT_DIAG=1 重编（CMake cache 变量，同 UART_BAUD 接线）。 */
#if EMS_REATT_DIAG
    {
        static bool rs_diag_done;
        if (!rs_diag_done) {
            rs_diag_done = true;
            rs_nor_probe();
            (void)rs_selftest_run();
            {
                extern void ems_diag_ladder_run(void);   /* 逐级 bisection（夜备） */
                ems_diag_ladder_run();
            }
        }
    }
#endif

    /* ---- 阶段 1：hist 归一化（冻结常数，ems_reatt_params.h） ---- */
    for (uint32_t t = 0u; t < EMS_REATT_H1_T; t++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_H1_CH_N; ch++) {
            uint32_t idx = t * EMS_REATT_H1_CH_N + ch;
            rs_n1_hist[idx] = (hist_raw[idx] - EMS_REATT_H1_MEAN[ch * EMS_REATT_H1_T + t])
                              / EMS_REATT_H1_STD[ch * EMS_REATT_H1_T + t];
        }
    }

    /* ---- 阶段 2：N1 → h_past[96] f32 ----
     * EMS_N1_LADDER=1：段链形态（proj/dil 段 NPU + 手写 CPU LN + attn 头全 CPU）
     * —— atonn 工具链 LN variance epoch 丢失缺陷（FINDINGS.md）的服务级修复；
     * 首拍由段链内部 deinit+init 复位，后续拍直接复用。
     * 否则：reattn1n 整图 NPU ASYNC（缺陷形态，仅对照保留）。 */
#if defined(EMS_N1_LADDER) && EMS_N1_LADDER
    {
        static bool rs_seg_ran;
        uint32_t seg_cyc = 0u;
        extern int ems_n1_seg_chain(const float *hist, float *hp_out,
                                    int first_run, uint32_t *total_cycles);
        rc = ems_n1_seg_chain(rs_n1_hist, rs_n1_hpast, !rs_seg_ran, &seg_cyc);
        if (rc != 0) {
            printf("RS-FAIL n1seg rc=%d\r\n", rc); return 1;
        }
        rs_seg_ran = true;
        t_n1 = rs_now_us();
#if defined(EMS_N1_Q1R_AB) && EMS_N1_Q1R_AB
        {
            static bool rs_ab_done;
            if (!rs_ab_done) {
                rs_ab_done = true;
                extern void n1_q1r_ab_report(void);
                n1_q1r_ab_report();
            }
        }
#endif
    }
#else
    SCB_CleanDCache_by_Addr((uint32_t *)rs_n1_hist, (int32_t)sizeof(rs_n1_hist));
    {
        stai_ptr in_ptrs[STAI_REATTN1N_IN_NUM];
        stai_ptr out_ptrs[STAI_REATTN1N_OUT_NUM];
        in_ptrs[0] = (stai_ptr)rs_n1_hist;
        out_ptrs[0] = (stai_ptr)rs_n1_hpast;
        if (stai_reattn1n_set_inputs(rs_ctx_n1, in_ptrs, STAI_REATTN1N_IN_NUM) != STAI_SUCCESS
                || stai_reattn1n_set_outputs(rs_ctx_n1, out_ptrs, STAI_REATTN1N_OUT_NUM) != STAI_SUCCESS) {
            printf("RS-FAIL n1setio\r\n"); return 1;
        }
        if (rs_n1_ran_before) {
            /* stai 契约：new_inference 仅在上一异步推理之后准备下一拍；首拍非法 */
            if (stai_ext_reattn1n_new_inference(rs_ctx_n1) != STAI_SUCCESS) {
                printf("RS-FAIL n1newinf\r\n"); return 1;
            }
        }
        rc = stai_reattn1n_run(rs_ctx_n1, STAI_MODE_ASYNC);
        if (rs_wait_done(rs_ctx_n1, rc, stai_ext_reattn1n_run_continue) != 0) {
            printf("RS-FAIL n1run rc=%d\r\n", (int)rc); return 1;
        }
        rs_n1_ran_before = true;
    }
    SCB_InvalidateDCache_by_Addr(rs_n1_hpast, (int32_t)sizeof(rs_n1_hpast));
    t_n1 = rs_now_us();
#endif /* EMS_N1_LADDER */

#if EMS_REATT_DIAG_N1
    /* N1 输出探针（NPU2 数值回归定位，2026-09-24）：h_past 96×f32 位模式 hex
     * 倒出，主机侧与 OE 编译图 ORT 输出对拍（RCA 判据：板 vs OE 为主）。
     * 生产保持 0：每查询 ~800B UART（~75ms @115200）。 */
    {
        const uint32_t *hp = (const uint32_t *)rs_n1_hpast;
        printf("RS-N1 hp=");
        for (uint32_t i = 0u; i < 96u; i++) {
            printf("%08lx", (unsigned long)hp[i]);
        }
        printf("\r\n");
    }
#endif

    /* ---- 阶段 3：kf 归一化 + 量化 → N2 输入 1（int8）----
     * 2026-09-24：reattn2l 编译图 kf 输入声明为 [1,6,30]（内部 Transpose 回
     * [1,30,6]），物理缓冲按 ch-major（转置序）写入（ORT A/B 实锤：转置喂入
     * maxd=18.1kW vs golden，直写序 maxd=315.9kW）。 */
    for (uint32_t k = 0u; k < EMS_REATT_KF_T; k++) {
        for (uint32_t ch = 0u; ch < EMS_REATT_KF_CH_N; ch++) {
            uint32_t idx = k * EMS_REATT_KF_CH_N + ch;
            float v = (kf_raw[idx] - EMS_REATT_KF_MEAN[ch * EMS_REATT_KF_T + k])
                      / EMS_REATT_KF_SCALE[ch * EMS_REATT_KF_T + k];
            rs_n2_kf[ch * EMS_REATT_KF_T + k] = rs_quant(v, rs_kf_scale, rs_kf_zp);
        }
    }

    /* ---- 阶段 4：N2（Attention/Dense，NPU 形态）----
     * h_past 与 N1 输出共用缓冲（零拷贝）；由 MCU 写入 → 交 NPU 前 CLEAN。 */
    SCB_CleanDCache_by_Addr((uint32_t *)rs_n1_hpast, (int32_t)sizeof(rs_n1_hpast));
    SCB_CleanDCache_by_Addr((uint32_t *)rs_n2_kf, (int32_t)sizeof(rs_n2_kf));
    SCB_InvalidateDCache_by_Addr(rs_n2_out, (int32_t)sizeof(rs_n2_out));

    {
        static uint32_t rs_diag_n;
        stai_ptr in_ptrs[LL_ATON_REATTN2L_IN_NUM];
        stai_ptr out_ptrs[LL_ATON_REATTN2L_OUT_NUM];
        in_ptrs[0] = (stai_ptr)rs_n2_kf;
        in_ptrs[1] = (stai_ptr)rs_n1_hpast;
        out_ptrs[0] = (stai_ptr)rs_n2_out;
        if (stai_reattn2l_set_inputs(rs_ctx_n2, in_ptrs, LL_ATON_REATTN2L_IN_NUM) != STAI_SUCCESS
                || stai_reattn2l_set_outputs(rs_ctx_n2, out_ptrs, LL_ATON_REATTN2L_OUT_NUM) != STAI_SUCCESS) {
            printf("RS-FAIL n2setio\r\n"); return 1;
        }
        if (rs_ran_before) {
            /* stai 契约：new_inference 仅在上一异步推理之后准备下一拍；首拍非法 */
            if (stai_ext_reattn2l_new_inference(rs_ctx_n2) != STAI_SUCCESS) {
                printf("RS-FAIL n2newinf\r\n"); return 1;
            }
        }
        rc = stai_reattn2l_run(rs_ctx_n2, STAI_MODE_ASYNC);
        if (rs_wait_done_prof_n2(rs_ctx_n2, rc, stai_ext_reattn2l_run_continue) != 0) {
            printf("RS-FAIL n2run rc=%d\r\n", (int)rc); return 1;
        }
        rs_ran_before = true;
#if defined(EMS_N2_EPCB) && EMS_N2_EPCB
        {
            static bool rs_epcb_dumped;
            if (!rs_epcb_dumped) {
                rs_epcb_dumped = true;
                rs_n2_epcb_dump();
            }
        }
#endif

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
        /* 周期域差值再换算：u32 模算术天然吸收 CYCCNT 7.16s 回绕
         * （旧实现 µs 域相减在跨绕点产生 ~2^32 伪值，755 战役已观测）。 */
        *infer_us = (uint32_t)(DWT->CYCCNT - t0_cyc) /
                    (uint32_t)(SystemCoreClock / 1000000u);
    }
    {
        static uint32_t rs_t_n;
        if (rs_t_n < 3u) {
            rs_t_n++;
#if defined(EMS_N2_PROF) && EMS_N2_PROF
            { extern void rs_n2eprof_report(void); rs_n2eprof_report(); }
#endif
            printf("RS-T n1=%lu n2=%lu total=%lu us\r\n",
                   (unsigned long)(t_n1 - t0), (unsigned long)(rs_now_us() - t_n1),
                   (unsigned long)(rs_now_us() - t0));
        }
    }
    return 0;
}

#if defined(EMS04_BDDIAG_UART) && EMS04_BDDIAG_UART
/* S3-C BD 腿批量诊断（qualification-only，开关默认 0）：只读访问器，
 * 供 ems04_s3c_bddiag.c 打包 UART 回传。不改本文件任何既有行为。 */
const float       *ems04_s3c_bddiag_hpast(void) { return rs_n1_hpast; }
const signed char *ems04_s3c_bddiag_n2out(void) { return rs_n2_out; }
const signed char *ems04_s3c_bddiag_n2kf(void)  { return rs_n2_kf; }
#endif

#endif /* EMS_REATT_SPLIT && EMS_REATT_SERVICE && NPU && tcnq */
