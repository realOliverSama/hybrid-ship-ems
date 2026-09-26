/* ems_n1_ladder.c — N1(reattn1n) 数值回归逐级 bisection 阶梯执行器
 * （phase4-g 通宵调试，2026-09-24 夜；手法 = EMS02 ems_diag_ladder.c N2 同款）。
 *
 * 阶梯（前缀子图，切点=源图 f32 张量，主机侧位一致自检已过）：
 *   FULL  服务图 reattn1n（权重 NOR @0x70B00000）4 窗 —— 案发现场锚点，
 *         预期复现 h_past 偏差（HANDOFF §3: 板 vs OE MAE 1.31-2.30）
 *   L1    hist → past_add0（proj + zpad0/ccat0 + dil0(D=1) + ln0 + 残差）
 *   L2    hist → past_add1（+ dil1(D=2) 层）
 *   L4    hist → past_add3（+ dil2(D=4) + dil3(D=8) 层）
 * 子图权重在 NOR @0x70C00000 起（探针启动前由 STM32_Programmer_CLI 烧入，
 * Dev boot 下不切 boot；N2 阶梯 0x70410000 同款先例）。
 *
 * 判读：L1 PASS 而 L2 FAIL ⇒ dilation(D=2) 寻址类；L2 PASS L4 FAIL ⇒ D=4/8；
 * 全 PASS 而 FULL DIFF ⇒ 病灶在 dil4/D=16、dil5/D=32 或 attention-pool 头
 * （二轮补 L5/L6/L7 细化）；FULL PASS ⇒ 探针环境与服务环境差异（cache/时钟）。
 *
 * 触发：EMS_N1_LADDER=1 时 ems_handle_cmd_reatt 每次收到 CMD_REATT 执行一轮。
 * golden：ems_n1_ladder_golden.h（n1_ladder_export.py emit-c 生成，禁手改）。
 */
#if defined(EMS_N1_LADDER) && EMS_N1_LADDER

/* G7 全 CPU 形态：get_info 为条件编译（stedgeai 全 CPU 生成码惯例） */
#ifndef HAVE_N1G7_INFO
#define HAVE_N1G7_INFO 1
#endif

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "stai.h"
#include "stai_ext.h"

#include "ems_stai_rt.h"
#if !(defined(EMS_N1_SEG_ONLY) && EMS_N1_SEG_ONLY)
#include "ems_n1_ladder_golden.h"
#endif

#if !(defined(EMS_N1_SEG_ONLY) && EMS_N1_SEG_ONLY)
#include "../../Model/reatt8_q_npu/npu_n1/gen/reattn1n.h"
#include "../../Model/reatt8_q_npu/npu_n1/gen/stai_reattn1n.h"
#endif
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l0/n1l0.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l0/stai_n1l0.h"
#if !(defined(EMS_N1_SEG_ONLY) && EMS_N1_SEG_ONLY)
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1za0/n1za0.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1za0/stai_n1za0.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1dc0/n1dc0.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1dc0/stai_n1dc0.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1ln0/n1ln0.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1ln0/stai_n1ln0.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l1/n1l1.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l1/stai_n1l1.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l2/n1l2.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l2/stai_n1l2.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l4/n1l4.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1l4/stai_n1l4.h"
#endif
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g1/n1g1.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g1/stai_n1g1.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g2/n1g2.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g2/stai_n1g2.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g3/n1g3.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g3/stai_n1g3.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g4/n1g4.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g4/stai_n1g4.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g5/n1g5.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g5/stai_n1g5.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g6/n1g6.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g6/stai_n1g6.h"
#include "../../Model/reatt8_q_npu/npu_n1/ladder/n1g7c/n1g7.h"
#include "n1_seg_golden.h"
#include "n1_ck_qparams.h"

void ems_n1_seg_probe_run(void);

#if !(defined(EMS_N1_SEG_ONLY) && EMS_N1_SEG_ONLY)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_full, STAI_REATTN1N_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_za0, STAI_N1ZA0_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_dc0, STAI_N1DC0_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_ln0, STAI_N1LN0_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_l1, STAI_N1L1_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_l2, STAI_N1L2_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_l4, STAI_N1L4_CONTEXT_SIZE)
#endif
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g0, STAI_N1L0_CONTEXT_SIZE)

/* user-allocated IO 契约缓冲（各生成头 USER_ALLOCATED_INPUTS/OUTPUTS=1；
 * 必须先 set_inputs/set_outputs 再 get_*，否则运行时 NULL 解引用 HardFault
 * ——EMS02 阶梯 2026-09-23 hot-attach 实锤教训）。 */
/* 115200B = 最大段输入 28800×f32（服务段链 G1-G6）；13200 只够探针 P 段——
 * 2026-09-25 RamRel 实锤：小缓冲 + 大 memcpy 越界 102KB，砸毁 BSS 后段的
 * NPU context 与 EC 微码暂存区（EC ERR_START 假象） */
static uint8_t n1_in_buf[115200u] __attribute__((aligned(32)));
static uint8_t n1_out_buf[115200u] __attribute__((aligned(32)));

/* 最大输出元素数（L1/L2/L4 = [1,300,96] f32 = 28800；FULL = 96） */
#define N1_MAX_OUT_ELEMS 28800u

/* DWT 周期计数计时 */
static void n1_dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t n1_dwt_cycles(void)
{
    return DWT->CYCCNT;
}

static int n1_wait_done(stai_network *ctx, stai_return_code rc,
                        stai_return_code (*cont)(stai_network *),
                        stai_return_code (*stat)(stai_network *), const char *tag)
{
    uint32_t spins = 0u;
    while (rc != STAI_DONE) {
        if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
            printf("RS-%s FAIL run rc=%d\r\n", tag, (int)rc);
            return 1;
        }
        if (rc == STAI_RUNNING_WFE || rc == STAI_RUNNING_NO_WFE) {
            rc = cont(ctx);
        } else {
            rc = stat(ctx);
        }
        if (++spins > 20000000u) {
            printf("RS-%s TIMEOUT st=%d\r\n", tag, (int)rc);
            return 3;
        }
    }
    return 0;
}

/* 单级执行：in_f32 → 图 → f32 输出对拍 exp。返回 0=PASS 2=DIFF 1/3=错误 */
static int n1_run_stage(const char *tag, stai_network *ctx,
                        stai_return_code (*init_fn)(stai_network *),
                        stai_return_code (*info_fn)(stai_network *, stai_network_info *),
                        stai_return_code (*setin_fn)(stai_network *, const stai_ptr *, stai_size),
                        stai_return_code (*setout_fn)(stai_network *, const stai_ptr *, stai_size),
                        stai_return_code (*stat_fn)(stai_network *),
                        stai_return_code (*run_fn)(stai_network *, stai_run_mode),
                        stai_return_code (*cont_fn)(stai_network *),
                        stai_return_code (*newinf_fn)(stai_network *),
                        const float *in_f32, uint32_t in_elems,
                        const float *exp_f32, uint32_t exp_elems,
                        uint32_t out_bytes)
{
    stai_network_info info;
    stai_ptr in_p = (stai_ptr)n1_in_buf;
    stai_ptr out_p = (stai_ptr)n1_out_buf;
    stai_return_code rc;
    uint32_t out_elems;
    float maxd = 0.0f, mae = 0.0f, mabs = 0.0f, tol;
    uint32_t match = 0u;
    uint32_t t0, t_elapsed;

    ems_stai_rt_init_once();
    n1_dwt_enable();

    if (init_fn(ctx) != STAI_SUCCESS) {
        printf("RS-%s FAIL init\r\n", tag);
        return 1;
    }
    if (info_fn(ctx, &info) != STAI_SUCCESS) {
        printf("RS-%s FAIL info\r\n", tag);
        return 1;
    }
    if (info.inputs[0].size_bytes != (uint32_t)(in_elems * 4) ||
        (uint32_t)(in_elems * 4) > sizeof(n1_in_buf)) {
        printf("RS-%s FAIL insize %u exp %u\r\n", tag,
               (unsigned)info.inputs[0].size_bytes, (unsigned)(in_elems * 4));
        return 1;
    }
    memcpy(n1_in_buf, in_f32, in_elems * 4u);
    SCB_CleanDCache_by_Addr((uint32_t *)n1_in_buf, (int32_t)(in_elems * 4u));
    if (setin_fn(ctx, &in_p, 1u) != STAI_SUCCESS ||
        setout_fn(ctx, &out_p, 1u) != STAI_SUCCESS) {
        printf("RS-%s FAIL setio\r\n", tag);
        return 1;
    }
    rc = newinf_fn(ctx);
    if (rc != STAI_SUCCESS && rc != STAI_ERROR_NETWORK_STILL_RUNNING) {
        printf("RS-%s FAIL newinf rc=%d\r\n", tag, (int)rc);
        return 1;
    }
    t0 = n1_dwt_cycles();
    rc = run_fn(ctx, STAI_MODE_ASYNC);
    if (n1_wait_done(ctx, rc, cont_fn, stat_fn, tag) != 0) {
        return 3;
    }
    t_elapsed = n1_dwt_cycles() - t0;
    SCB_InvalidateDCache_by_Addr((uint32_t *)n1_out_buf, (int32_t)out_bytes);

    /* 输出契约自适应：size==elems*4 → f32 直读；size==elems → S8 + scale/zp
     * 反量化（QDQ 融合把 f32 边界折叠为量化存储，stai info 提供边界参数）。 */
    out_elems = exp_elems;
    uint32_t real_bytes = info.outputs[0].size_bytes;
    int out_is_f32 = (real_bytes == out_elems * 4u);
    float out_scale = (info.outputs[0].scale.size == 1u)
                      ? info.outputs[0].scale.data[0] : 1.0f;
    int32_t out_zp = (info.outputs[0].zeropoint.size == 1u)
                     ? (int32_t)info.outputs[0].zeropoint.data[0] : 0;
    if (!out_is_f32) {
        printf("RS-%s qout bytes=%u scale=%.6g zp=%d\r\n", tag,
               (unsigned)real_bytes, (double)out_scale, (int)out_zp);
    }
    for (uint32_t e = 0u; e < out_elems; e++) {
        float v, d, a;
        if (out_is_f32) {
            memcpy(&v, &n1_out_buf[e * 4u], 4u);
        } else {
            v = ((float)((int8_t)n1_out_buf[e]) - (float)out_zp) * out_scale;
        }
        d = fabsf(v - exp_f32[e]);
        maxd = (d > maxd) ? d : maxd;
        mae += d;
        a = (exp_f32[e] < 0.0f) ? -exp_f32[e] : exp_f32[e];
        mabs = (a > mabs) ? a : mabs;
    }
    mae /= (float)out_elems;
    tol = 0.05f * (1.0f + mabs);  /* f32 相对容差（S8 量化传播可到百分之几） */
    for (uint32_t e = 0u; e < out_elems; e++) {
        float v, d;
        if (out_is_f32) {
            memcpy(&v, &n1_out_buf[e * 4u], 4u);
        } else {
            v = ((float)((int8_t)n1_out_buf[e]) - (float)out_zp) * out_scale;
        }
        d = fabsf(v - exp_f32[e]);
        if (d <= tol) {
            match++;
        }
    }
    printf("RS-%s match=%u/%u maxd=%.4g mae=%.4g tol=%.4g cyc=%u %s\r\n",
           tag, (unsigned)match, (unsigned)out_elems,
           (double)maxd, (double)mae, (double)tol, (unsigned)t_elapsed,
           (match == out_elems) ? "PASS" : "DIFF");
    /* LN 段模式分析：倒原始量化输出（int8 codes hex） */
    if (tag[0] == 'L' && tag[1] == 'N' && tag[2] == '0' && tag[3] == '\0') {
        printf("RS-%s raw=", tag);
        for (uint32_t b2 = 0u; b2 < real_bytes; b2++) {
            printf("%02x", (unsigned)n1_out_buf[b2]);
        }
        printf("\r\n");
    }
    return (match == out_elems) ? 0 : 2;
}

#if !(defined(EMS_N1_SEG_ONLY) && EMS_N1_SEG_ONLY)

void ems_n1_ladder_run(void)
{
    /* FULL：服务图 reattn1n（NOR 权重）单窗锚点（win0）。
     * 二轮起收紧到 L1 内部：L0 proj 段 / ZA0 zpad0 Slice 段 / DC0 dil0 conv 段 /
     * LN0 ln0 段（D=1 层已证存在真偏差，dilation 假设排除）。 */
    (void)n1_run_stage("FULL", n1_ctx_full,
                       stai_reattn1n_init, stai_reattn1n_get_info,
                       stai_reattn1n_set_inputs, stai_reattn1n_set_outputs,
                       stai_ext_reattn1n_get_nn_run_status,
                       stai_reattn1n_run, stai_ext_reattn1n_run_continue,
                       stai_ext_reattn1n_new_inference,
                       n1_hist_in_0, 3300u, n1_n1l7_exp, 96u, 384u);
    (void)n1_run_stage("L0", n1_ctx_l0,
                       stai_n1l0_init, stai_n1l0_get_info,
                       stai_n1l0_set_inputs, stai_n1l0_set_outputs,
                       stai_ext_n1l0_get_nn_run_status,
                       stai_n1l0_run, stai_ext_n1l0_run_continue,
                       stai_ext_n1l0_new_inference,
                       n1_hist_in_0, 3300u, n1_n1l0_exp, 28800u, 115200u);
    (void)n1_run_stage("ZA0", n1_ctx_za0,
                       stai_n1za0_init, stai_n1za0_get_info,
                       stai_n1za0_set_inputs, stai_n1za0_set_outputs,
                       stai_ext_n1za0_get_nn_run_status,
                       stai_n1za0_run, stai_ext_n1za0_run_continue,
                       stai_ext_n1za0_new_inference,
                       n1_hist_in_0, 3300u, n1_n1za0_exp, 28800u, 115200u);
    (void)n1_run_stage("DC0", n1_ctx_dc0,
                       stai_n1dc0_init, stai_n1dc0_get_info,
                       stai_n1dc0_set_inputs, stai_n1dc0_set_outputs,
                       stai_ext_n1dc0_get_nn_run_status,
                       stai_n1dc0_run, stai_ext_n1dc0_run_continue,
                       stai_ext_n1dc0_new_inference,
                       n1_hist_in_0, 3300u, n1_n1dc0_exp, 28800u, 115200u);
    (void)n1_run_stage("LN0", n1_ctx_ln0,
                       stai_n1ln0_init, stai_n1ln0_get_info,
                       stai_n1ln0_set_inputs, stai_n1ln0_set_outputs,
                       stai_ext_n1ln0_get_nn_run_status,
                       stai_n1ln0_run, stai_ext_n1ln0_run_continue,
                       stai_ext_n1ln0_new_inference,
                       n1_hist_in_0, 3300u, n1_n1ln0_exp, 28800u, 115200u);
    /* L1：基础构件段（proj + zpad0 + dil0 D=1 + ln0 + add0）——首轮已证 DIFF */
    (void)n1_run_stage("L1", n1_ctx_l1,
                       stai_n1l1_init, stai_n1l1_get_info,
                       stai_n1l1_set_inputs, stai_n1l1_set_outputs,
                       stai_ext_n1l1_get_nn_run_status,
                       stai_n1l1_run, stai_ext_n1l1_run_continue,
                       stai_ext_n1l1_new_inference,
                       n1_hist_in_0, 3300u, n1_n1l1_exp, 28800u, 115200u);
    printf("RS-N1LADDER done\r\n");
    /* 服务重切段链全链探针（修复验证主对象） */
    ems_n1_seg_probe_run();
}
#endif /* !(EMS_N1_SEG_ONLY)（探针 bisection 级仅 Debug 形态编入） */

/* ==================== 服务重切段链（LN 工具链缺陷修复） ====================
 * N1 = n1l0(proj 段 NPU) + [gK(NPU dil 段) + CPU LN 块]×6 + g7(attn 头 NPU)。
 * CPU LN 块语义（主机 6 层 golden 验证 mae ≤0.07）：
 *   mu/var = 标量全张量（opset17 ReduceMean 无 axis = 全维）
 *   y = (d-mu)*inv*gamma[c] + beta[c]；g = gelu_tanh(y)；out = x + g
 * 域：段出 d [96][300] c-major；段间/残差统一 t-major [300][96]；
 * c0 的残差 x=proj（c-major，转置读）；c1..c5 残差 x=add(K-1)（t-major，原地）。
 * 段级对拍 = 统计（max/sum vs SEG_* 常数）；全链正确性 = hpast vs 服务图 golden。 */
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g1, STAI_N1G1_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g2, STAI_N1G2_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g3, STAI_N1G3_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g4, STAI_N1G4_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g5, STAI_N1G5_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g6, STAI_N1G6_CONTEXT_SIZE)
STAI_NETWORK_CONTEXT_DECLARE(n1_ctx_g7, STAI_N1G7_CONTEXT_SIZE)

static uint8_t n1_act_buf[115200u] __attribute__((aligned(32)));
static float n1_f_proj[28800u];   /* proj f32 [96][300]（g1 输入 + c0 残差） */
static float n1_f_d[28800u];      /* 段输出反量化 [96][300] */
static float n1_f_add[28800u];    /* addK [300][96]（段间，in-place 更新） */

typedef stai_return_code (*n1_rc1_t)(stai_network *);
typedef stai_return_code (*n1_info_t)(stai_network *, stai_network_info *);
typedef stai_return_code (*n1_setio_t)(stai_network *, const stai_ptr *, stai_size);
typedef stai_return_code (*n1_run_t)(stai_network *, stai_run_mode);

struct n1_seg_fns {
    n1_rc1_t init, deinit, stat, cont, newinf;
    n1_info_t info;
    n1_setio_t setin, setout;
    n1_run_t run;
    n1_setio_t act_set;   /* 全 CPU 形态的 set_activations（NPU 段为 NULL） */
    stai_run_mode mode;   /* 全 CPU=SYNC，NPU=ASYNC */
};

/* 段执行：in f32 → NPU → 反量化 f32。输出统计 vs SEG_* 常数给 RS- 行。 */
static int n1_seg_run(const char *tag, stai_network *ctx,
                      const struct n1_seg_fns *f, int init_en,
                      const float *in_f32, uint32_t in_elems,
                      float *out_f32, uint32_t out_elems)
{
    stai_network_info info;
    stai_ptr in_p = (stai_ptr)n1_in_buf;
    stai_ptr out_p = (stai_ptr)n1_out_buf;
    stai_return_code rc;
    float out_scale;
    int32_t out_zp;
    int out_is_f32;
    uint32_t i, real;

    {
        (void)f->deinit(ctx);
        stai_return_code irc = f->init(ctx);
        if (irc != STAI_SUCCESS) {
            printf("RS-%s FAIL init rc=%d\r\n", tag, (int)irc);
            return 1;
        }
        if (f->act_set != NULL) {
            stai_ptr ap = (stai_ptr)n1_act_buf;
            stai_return_code arc = f->act_set(ctx, &ap, 1u);
            if (arc != STAI_SUCCESS) {
                printf("RS-%s FAIL setact rc=%d\r\n", tag, (int)arc);
                return 1;
            }
        }
    }
    if (f->info(ctx, &info) != STAI_SUCCESS) {
        printf("RS-%s FAIL info\r\n", tag);
        return 1;
    }
    real = info.inputs[0].size_bytes;
    float in_scale = (info.inputs[0].scale.size == 1u)
                     ? info.inputs[0].scale.data[0] : 1.0f;
    int32_t in_zp = (info.inputs[0].zeropoint.size == 1u)
                    ? (int32_t)info.inputs[0].zeropoint.data[0] : 0;
    if (real == in_elems * 4u) {
        memcpy(n1_in_buf, in_f32, in_elems * 4u);
    } else if (real == in_elems) {
        /* 输入契约 = int8 codes（源图 Q 折叠到应用侧）：CPU 量化（源图固有 Q） */
        for (i = 0u; i < in_elems; i++) {
            int32_t q = (int32_t)lrintf(in_f32[i] / in_scale) + in_zp;
            if (q < -128) {
                q = -128;
            } else if (q > 127) {
                q = 127;
            }
            n1_in_buf[i] = (uint8_t)(int8_t)q;
        }
    } else {
        printf("RS-%s FAIL insize %u\r\n", tag, (unsigned)real);
        return 1;
    }
    SCB_CleanDCache_by_Addr((uint32_t *)n1_in_buf, (int32_t)real);
    if (f->setin(ctx, &in_p, 1u) != STAI_SUCCESS ||
        f->setout(ctx, &out_p, 1u) != STAI_SUCCESS) {
        printf("RS-%s FAIL setio\r\n", tag);
        return 1;
    }
    if (f->mode == STAI_MODE_ASYNC) {
        /* cK 借用 n1_out_buf 做 scratch 的脏 cache 行必须写回 SRAM，
         * 否则 NPU 硬件直写后 cache 逐出旧数据会覆盖 NPU 输出
         * （2026-09-25 板实锤：转置融合后 4 窗全崩、corr=nan） */
        SCB_CleanDCache_by_Addr((uint32_t *)n1_out_buf, 115200);
    }
    printf("RS-%s prerun\r\n", tag);
    rc = f->run(ctx, f->mode);
    printf("RS-%s postrun rc=%d\r\n", tag, (int)rc);
    if (f->mode == STAI_MODE_SYNC) {
        /* 全 CPU 形态：run 同步完成，返回 STAI_SUCCESS 即结束 */
        if (rc != STAI_SUCCESS) {
            printf("RS-%s FAIL run rc=%d\r\n", tag, (int)rc);
            return 3;
        }
    } else if (n1_wait_done(ctx, rc, f->cont, f->stat, tag) != 0) {
        return 3;
    }
    real = info.outputs[0].size_bytes;
    if (f->mode != STAI_MODE_SYNC) {
        /* NPU 段：NPU 直写内存，读前 Invalidate；全 CPU 段（SYNC）为同核
         * cache 写，Invalidate 反而丢数据，故仅 ASYNC 段做 */
        SCB_InvalidateDCache_by_Addr((uint32_t *)n1_out_buf, (int32_t)real);
    }
    out_scale = (info.outputs[0].scale.size == 1u)
                ? info.outputs[0].scale.data[0] : 1.0f;
    out_zp = (info.outputs[0].zeropoint.size == 1u)
             ? (int32_t)info.outputs[0].zeropoint.data[0] : 0;
    out_is_f32 = (real == out_elems * 4u);
    if (out_is_f32) {
        memcpy(out_f32, n1_out_buf, out_elems * 4u);
    } else {
        for (i = 0u; i < out_elems; i++) {
            out_f32[i] = ((float)((int8_t)n1_out_buf[i]) - (float)out_zp) * out_scale;
        }
    }
    /* 段统计（主机 golden 对照；inv 情形不打印） */
    {
        float smax = out_f32[0], ssum = 0.0f;
        for (i = 0u; i < out_elems; i++) {
            if (out_f32[i] > smax) {
                smax = out_f32[i];
            }
            ssum += out_f32[i];
        }
        printf("RS-%s stat max=%.5e sum=%.5e\r\n", tag,
               (double)smax, (double)ssum);
    }
    return 0;
}

/* CPU LN 块（手写；已被 n1_ck_exact 取代——保留作语义注释与回退参考）。
 * 语义（ORT dump 实锤）：mean/var 沿通道轴（96 元素，per-time-step）；
 * y = (d-mu_t)*inv_t*gamma[c] + beta[c]；g = gelu_tanh(y)；out = x + g。 */
static void n1_ln_block(const float *d_cf, const float *x_cf, const float *x_tm,
                        float *out_tm, const float *gamma, const float *beta)
{
    float mt[300u], inv[300u];
    uint32_t t, c;

    for (t = 0u; t < 300u; t++) {
        float s = 0.0f, v = 0.0f;
        for (c = 0u; c < 96u; c++) {
            s += d_cf[c * 300u + t];
        }
        mt[t] = s / 96.0f;
        for (c = 0u; c < 96u; c++) {
            float e = d_cf[c * 300u + t] - mt[t];
            v += e * e;
        }
        inv[t] = 1.0f / sqrtf(v / 96.0f + 0.001f);
    }
    for (c = 0u; c < 96u; c++) {
        const float *dc = &d_cf[c * 300u];
        for (t = 0u; t < 300u; t++) {
            float xv = (x_cf != NULL) ? x_cf[c * 300u + t]
                                       : x_tm[t * 96u + c];
            float y = (dc[t] - mt[t]) * inv[t] * gamma[c] + beta[c];
            float g = 0.5f * y *
                      (1.0f + tanhf(0.7978845608f * (y + 0.044715f * y * y * y)));
            out_tm[t * 96u + c] = xv + g;
        }
    }
}

/* ===== cK 精确语义（源图全 QDQ 复刻，f32；numpy f32 仿真对 golden 位一致） =====
 * 按图节点序复刻 LN+GELU+残差全部量化点（n1_ck_qparams.h：c0=24、c1-5=23），
 * 消除"f32 手写比源图精确"的噪声模式差异（w41 等敏感窗的正解）。
 * 缓冲（零新增 BSS）：d=n1_f_d(c-major 消费后覆写)；x/out=n1_f_add(t-major)；
 * S1=n1_out_buf 借用；S2=n1_f_proj（c0 的 x 源先搬出）。 */
static inline void n1_ck_q1r(float *v, float rcp, float sc, int32_t zp)
{
    /* 内联 VCVTR.S32.F32（FPSCR/RNE，与 lrintf 逐位一致；注意 vcvt 无 R 变体固定 RZ，
     * 曾致 1.22M/3.6M 调用差一码，勿用）。 */
    int32_t q;
    __asm__ volatile ("vcvtr.s32.f32 %0, %1" : "=t"(q) : "t"(*v * rcp));
    q += zp;
    if (q < -128) {
        q = -128;
    } else if (q > 127) {
        q = 127;
    }
    *v = ((float)q - (float)zp) * sc;
}

#if defined(EMS_N1_Q1R_AB) && EMS_N1_Q1R_AB
/* q1r A/B 判别（2026-09-25）：lrintf 路径 vs vcvtr.s32.f32 内联在同输入上逐值对拍，
 * 前 16 个不一致点倒位模式。结论用于裁定"语义等价但板上 hp 偏移"的根因层。 */
static uint32_t n1_q1r_ab_nmismatch;
static void n1_ck_q1r_ab(float *v, float rcp, float sc, int32_t zp)
{
    const float in = *v * rcp;
    const int32_t q_old = (int32_t)lrintf(in);
    int32_t q_new;
    __asm__ volatile ("vcvtr.s32.f32 %0, %1" : "=t"(q_new) : "t"(in));
    if (q_old != q_new && n1_q1r_ab_nmismatch < 16u) {
        union { float f; uint32_t u; } cv;
        cv.f = in;
        printf("RS-Q1RAB in=%08lx old=%ld new=%ld\r\n",
               (unsigned long)cv.u, (long)q_old, (long)q_new);
    }
    if (q_old != q_new) {
        n1_q1r_ab_nmismatch++;
    }
    int32_t q = q_old + zp;   /* 保持旧语义：A/B 探针不改变数值路径 */
    if (q < -128) {
        q = -128;
    } else if (q > 127) {
        q = 127;
    }
    *v = ((float)q - (float)zp) * sc;
}
void n1_q1r_ab_report(void)
{
    printf("RS-Q1RAB total_mismatch=%lu\r\n", (unsigned long)n1_q1r_ab_nmismatch);
}
#define n1_ck_q1r n1_ck_q1r_ab
#endif /* EMS_N1_Q1R_AB */

#define CK_EPS   0.0010000000474974513f
#define CK_ONE   1.0f
#define CK_HALF  0.5f
#define CK_K0798 0.7978845834732056f
#define CK_K0044 0.044714998453855515f

/* tanhf 快速近似（worker-3 交付：6×256 网格穷举零码翻转 + golden 23 万点零翻转；
 * 饱和阈值与当前 qparams 绑定，模型再量化需重跑 tanh_study.py） */
static inline float n1_tanh_fast(float x)
{
    const float ax = fabsf(x);
    if (ax >= 9.01f) {
        return copysignf(1.0f, x);
    }
    if (ax >= 8.04f) {
        return copysignf(0.99999988f, x);
    }
    if (ax >= 4.0f) {
        return copysignf(0.999999f, x);
    }
    {
        const float y = x * x;
        const float n = 0.9999979f +
                        y * (0.1125629f +
                             y * (0.001178671f - y * 3.938927e-6f));
        const float dd = 1.0f + y * (0.445887f + y * 0.01648655f);
        return x * n / dd;
    }
}


static const n1_ck_q_t *n1_ck_sel(int k, uint32_t *qn)
{
    switch (k) {
    case 0: *qn = N1_CK_QN0; return n1_ck_p0;
    case 1: *qn = N1_CK_QN1; return n1_ck_p1;
    case 2: *qn = N1_CK_QN2; return n1_ck_p2;
    case 3: *qn = N1_CK_QN3; return n1_ck_p3;
    case 4: *qn = N1_CK_QN4; return n1_ck_p4;
    default: *qn = N1_CK_QN5; return n1_ck_p5;
    }
}

static uint32_t n1ck_prof[6];
static void n1_ck_prof_mark(uint32_t sec, uint32_t t0)
{
    n1ck_prof[sec] += DWT->CYCCNT - t0;
}

/* 8×8 平铺转置：src[c*300+t] → dst[t*96+c]（worker-3 交付，cache 友好） */
static void n1_ck_tpose(const float *src, float *dst)
{
    for (uint32_t c0 = 0u; c0 < 96u; c0 += 8u) {
        for (uint32_t t0 = 0u; t0 < 300u; t0 += 8u) {
            const uint32_t te = (t0 + 8u < 300u) ? t0 + 8u : 300u;
            const uint32_t ce = (c0 + 8u < 96u) ? c0 + 8u : 96u;
            for (uint32_t t = t0; t < te; t++) {
                for (uint32_t c = c0; c < ce; c++) {
                    dst[t * 96u + c] = src[c * 300u + t];
                }
            }
        }
    }
}

static void n1_ck_exact(int k, float *d, float *x, float *out,
                        const float *gamma, const float *beta)
{
    uint32_t qn;
    uint32_t pt = DWT->CYCCNT;
    const n1_ck_q_t *p = n1_ck_sel(k, &qn);
    const uint32_t nx = (qn == 24u) ? 1u : 0u;  /* c0: x 双重量化 */
    float *s1 = (float *)(void *)n1_out_buf;
    float *s2 = n1_f_proj;
    float mean[300u], inv[300u], negq[300u];
    uint32_t i, t, c;

    /* [0]（c0 另有 [1]）x 入界 Q/DQ → xq_t=out */
    if (nx != 0u) {
        for (i = 0u; i < 28800u; i++) {
            n1_ck_q1r(&x[i], p[0].rcp, p[0].sc, p[0].zp);
        }
    n1_ck_tpose(x, out);
    for (uint32_t i = 0u; i < 28800u; i++) {
        n1_ck_q1r(&out[i], p[1].rcp, p[1].sc, p[1].zp);
    }
    } else {
        for (i = 0u; i < 28800u; i++) {
            n1_ck_q1r(&x[i], p[0].rcp, p[0].sc, p[0].zp);
        }
    }

    n1_ck_prof_mark(0, pt);
    pt = DWT->CYCCNT;
    /* [nx+1] d Q/DQ（就地）；[nx+2] d_t 转置再量化 → S1 */
    for (i = 0u; i < 28800u; i++) {
        n1_ck_q1r(&d[i], p[nx + 1].rcp, p[nx + 1].sc, p[nx + 1].zp);
    }
    n1_ck_tpose(d, s1);
    for (uint32_t i = 0u; i < 28800u; i++) {
        n1_ck_q1r(&s1[i], p[nx + 2].rcp, p[nx + 2].sc, p[nx + 2].zp);
    }

    n1_ck_prof_mark(1, pt);
    pt = DWT->CYCCNT;
    /* mean（沿通道）；neg Q/DQ → negq[t] */
    for (t = 0u; t < 300u; t++) {
        float s = 0.0f;
        for (c = 0u; c < 96u; c++) {
            s += d[c * 300u + t];
        }
        mean[t] = s / 96.0f;
        float v = -mean[t];
        n1_ck_q1r(&v, p[nx + 3].rcp, p[nx + 3].sc, p[nx + 3].zp);
        negq[t] = v;
    }

    /* [nx+4] sqdiff（就地）；[nx+5] square（就地） */
    for (c = 0u; c < 96u; c++) {
        for (t = 0u; t < 300u; t++) {
            float v = d[c * 300u + t] - mean[t];
            n1_ck_q1r(&v, p[nx + 4].rcp, p[nx + 4].sc, p[nx + 4].zp);
            d[c * 300u + t] = v;
        }
    }
    for (i = 0u; i < 28800u; i++) {
        float v = d[i] * d[i];
        n1_ck_q1r(&v, p[nx + 5].rcp, p[nx + 5].sc, p[nx + 5].zp);
        d[i] = v;
    }

    /* [nx+6] var → [nx+7] +eps → [nx+8] 1/sqrt */
    for (t = 0u; t < 300u; t++) {
        float s = 0.0f;
        for (c = 0u; c < 96u; c++) {
            s += d[c * 300u + t];
        }
        float v = s / 96.0f;
        n1_ck_q1r(&v, p[nx + 6].rcp, p[nx + 6].sc, p[nx + 6].zp);
        v += CK_EPS;
        n1_ck_q1r(&v, p[nx + 7].rcp, p[nx + 7].sc, p[nx + 7].zp);
        inv[t] = 1.0f / sqrtf(v);
        n1_ck_q1r(&inv[t], p[nx + 8].rcp, p[nx + 8].sc, p[nx + 8].zp);
    }

    n1_ck_prof_mark(2, pt);
    pt = DWT->CYCCNT;
    /* [nx+9] a=inv*gamma（覆写 d）→ [nx+10] a_t → S2 */
    for (c = 0u; c < 96u; c++) {
        for (t = 0u; t < 300u; t++) {
            float v = inv[t] * gamma[c];
            n1_ck_q1r(&v, p[nx + 9].rcp, p[nx + 9].sc, p[nx + 9].zp);
            d[c * 300u + t] = v;
        }
    }
    n1_ck_tpose(d, s2);
    for (uint32_t i = 0u; i < 28800u; i++) {
        n1_ck_q1r(&s2[i], p[nx + 10].rcp, p[nx + 10].sc, p[nx + 10].zp);
    }

    n1_ck_prof_mark(3, pt);
    pt = DWT->CYCCNT;
    /* 融合尾段：mul_2/mul_1/add_1/add_2 + GELU 全链 + 残差。
     * [nx+11] mul_2=d_tq(S1)*a_t(S2)；[nx+12] mul_1=negq*a_t；
     * [nx+13] add_1=mul_1+beta；[nx+14] add_2=mul_2+add_1；
     * [nx+15..22] gelu 八步；残差 out=xq_t+gelu。 */
    /* 图节点序逐层不同：c0 先 mul_2 后 mul_1；c1-5 相反（索引换位） */
    const uint32_t q_m2 = nx ? (nx + 11) : (nx + 12);
    const uint32_t q_m1 = nx ? (nx + 12) : (nx + 11);
    for (i = 0u; i < 28800u; i++) {
        float v = s1[i] * s2[i];
        n1_ck_q1r(&v, p[q_m2].rcp, p[q_m2].sc, p[q_m2].zp);
        float w = negq[i / 96u] * s2[i];
        n1_ck_q1r(&w, p[q_m1].rcp, p[q_m1].sc, p[q_m1].zp);
        w += beta[i % 96u];
        n1_ck_q1r(&w, p[nx + 13].rcp, p[nx + 13].sc, p[nx + 13].zp);
        float a2 = v + w;
        n1_ck_q1r(&a2, p[nx + 14].rcp, p[nx + 14].sc, p[nx + 14].zp);
        float m05 = CK_HALF * a2;
        n1_ck_q1r(&m05, p[nx + 15].rcp, p[nx + 15].sc, p[nx + 15].zp);
        float pw = a2 * a2 * a2;
        n1_ck_q1r(&pw, p[nx + 16].rcp, p[nx + 16].sc, p[nx + 16].zp);
        float m1g = CK_K0044 * pw;
        n1_ck_q1r(&m1g, p[nx + 17].rcp, p[nx + 17].sc, p[nx + 17].zp);
        float ad = a2 + m1g;
        n1_ck_q1r(&ad, p[nx + 18].rcp, p[nx + 18].sc, p[nx + 18].zp);
        float m2v = CK_K0798 * ad;
        n1_ck_q1r(&m2v, p[nx + 19].rcp, p[nx + 19].sc, p[nx + 19].zp);
        float th = tanhf(m2v);
        n1_ck_q1r(&th, p[nx + 20].rcp, p[nx + 20].sc, p[nx + 20].zp);
        float a1v = CK_ONE + th;
        n1_ck_q1r(&a1v, p[nx + 21].rcp, p[nx + 21].sc, p[nx + 21].zp);
        float m3 = m05 * a1v;
        n1_ck_q1r(&m3, p[nx + 22].rcp, p[nx + 22].sc, p[nx + 22].zp);
        s2[i] = m3;                    /* gelu（S2 腾出） */
        s1[i] = a2;                    /* add_2 暂存（残差用） */
    }
    n1_ck_prof_mark(4, pt);
    for (i = 0u; i < 28800u; i++) {
        out[i] += s2[i];               /* out = xq_t + gelu（残差） */
    }
}

/* 全链：hist [300][11] → h_past [96]。返回 0=OK。 */
int ems_n1_seg_chain(const float *hist, float *hp_out, int first_run,
                     uint32_t *total_cycles)
{
    static const struct n1_seg_fns gt[5] = {
        { stai_n1g2_init, stai_n1g2_deinit, stai_ext_n1g2_get_nn_run_status,
          stai_ext_n1g2_run_continue, stai_ext_n1g2_new_inference,
          stai_n1g2_get_info, stai_n1g2_set_inputs, stai_n1g2_set_outputs,
          stai_n1g2_run, NULL, STAI_MODE_ASYNC },
        { stai_n1g3_init, stai_n1g3_deinit, stai_ext_n1g3_get_nn_run_status,
          stai_ext_n1g3_run_continue, stai_ext_n1g3_new_inference,
          stai_n1g3_get_info, stai_n1g3_set_inputs, stai_n1g3_set_outputs,
          stai_n1g3_run, NULL, STAI_MODE_ASYNC },
        { stai_n1g4_init, stai_n1g4_deinit, stai_ext_n1g4_get_nn_run_status,
          stai_ext_n1g4_run_continue, stai_ext_n1g4_new_inference,
          stai_n1g4_get_info, stai_n1g4_set_inputs, stai_n1g4_set_outputs,
          stai_n1g4_run, NULL, STAI_MODE_ASYNC },
        { stai_n1g5_init, stai_n1g5_deinit, stai_ext_n1g5_get_nn_run_status,
          stai_ext_n1g5_run_continue, stai_ext_n1g5_new_inference,
          stai_n1g5_get_info, stai_n1g5_set_inputs, stai_n1g5_set_outputs,
          stai_n1g5_run, NULL, STAI_MODE_ASYNC },
        { stai_n1g6_init, stai_n1g6_deinit, stai_ext_n1g6_get_nn_run_status,
          stai_ext_n1g6_run_continue, stai_ext_n1g6_new_inference,
          stai_n1g6_get_info, stai_n1g6_set_inputs, stai_n1g6_set_outputs,
          stai_n1g6_run, NULL, STAI_MODE_ASYNC },
    };
    static stai_network * const gctx[5] = {
        n1_ctx_g2, n1_ctx_g3, n1_ctx_g4, n1_ctx_g5, n1_ctx_g6 };
    static const float *gtab[5] = {
        n1_ln1_gamma, n1_ln2_gamma, n1_ln3_gamma, n1_ln4_gamma, n1_ln5_gamma };
    static const float *btab[5] = {
        n1_ln1_beta, n1_ln2_beta, n1_ln3_beta, n1_ln4_beta, n1_ln5_beta };
    static const struct n1_seg_fns f_g1 = {
        stai_n1g1_init, stai_n1g1_deinit, stai_ext_n1g1_get_nn_run_status,
        stai_ext_n1g1_run_continue, stai_ext_n1g1_new_inference,
        stai_n1g1_get_info, stai_n1g1_set_inputs, stai_n1g1_set_outputs,
        stai_n1g1_run, NULL, STAI_MODE_ASYNC };
    static const struct n1_seg_fns f_g7 = {
        stai_n1g7_init, stai_n1g7_deinit, NULL, NULL, NULL,
        stai_n1g7_get_info, stai_n1g7_set_inputs, stai_n1g7_set_outputs,
        stai_n1g7_run, stai_n1g7_set_activations, STAI_MODE_SYNC };
    static const struct n1_seg_fns f_p = {
        stai_n1l0_init, stai_n1l0_deinit, stai_ext_n1l0_get_nn_run_status,
        stai_ext_n1l0_run_continue, stai_ext_n1l0_new_inference,
        stai_n1l0_get_info, stai_n1l0_set_inputs, stai_n1l0_set_outputs,
        stai_n1l0_run, NULL, STAI_MODE_ASYNC };
    uint32_t t0, k;

    ems_stai_rt_init_once();
    n1_dwt_enable();
    t0 = n1_dwt_cycles();
    if (n1_seg_run("P", n1_ctx_g0, &f_p, first_run, hist, 3300u,
                   n1_f_proj, 28800u) != 0) {
        return 1;
    }
    if (n1_seg_run("G1", n1_ctx_g1, &f_g1, first_run, n1_f_proj, 28800u,
                   n1_f_d, 28800u) != 0) {
        return 1;
    }
    n1_ck_exact(0, n1_f_d, n1_f_proj, n1_f_add,
                n1_ln0_gamma, n1_ln0_beta);
    {
        float smx = n1_f_add[0], ssm = 0.0f;
        for (uint32_t i = 0u; i < 28800u; i++) {
            if (n1_f_add[i] > smx) {
                smx = n1_f_add[i];
            }
            ssm += n1_f_add[i];
        }
        printf("RS-C0 stat max=%.5e sum=%.5e\r\n", (double)smx, (double)ssm);
    }
    for (k = 0u; k < 5u; k++) {
        if (n1_seg_run("G", gctx[k], &gt[k], first_run, n1_f_add, 28800u,
                       n1_f_d, 28800u) != 0) {
            return 1;
        }
        n1_ck_exact(k + 1, n1_f_d, n1_f_add, n1_f_add,
                    gtab[k], btab[k]);
        {
            float smx = n1_f_add[0], ssm = 0.0f;
            for (uint32_t i = 0u; i < 28800u; i++) {
                if (n1_f_add[i] > smx) {
                    smx = n1_f_add[i];
                }
                ssm += n1_f_add[i];
            }
            printf("RS-C stat max=%.5e sum=%.5e\r\n", (double)smx, (double)ssm);
        }
    }
    if (n1_seg_run("G7", n1_ctx_g7, &f_g7, first_run, n1_f_add, 28800u,
                   hp_out, 96u) != 0) {
        return 1;
    }
    if (total_cycles != NULL) {
        *total_cycles = n1_dwt_cycles() - t0;
    }
    return 0;
}

/* 探针：全链 + hpast vs 服务图 golden + 段统计对照（仅 Debug 探针形态） */
#if !(defined(EMS_N1_SEG_ONLY) && EMS_N1_SEG_ONLY)
void ems_n1_seg_probe_run(void)
{
     static const float *wins[4] = {
        n1_hist_in_7, n1_hist_in_23, n1_hist_in_41, n1_hist_in_0 };
    static const uint8_t wtag[4] = { 7u, 23u, 41u, 0u };
    uint32_t cyc, cyc_tot = 0u;
    int rc, rc_tot = 0;
    float hp[96];
    uint32_t w, i;

    for (w = 0u; w < 4u; w++) {
        float sum = 0.0f;
        cyc = 0u;
        rc = ems_n1_seg_chain(wins[w], hp, (w == 0u), &cyc);
        rc_tot |= rc;
        cyc_tot += cyc;
        {
            char hexbuf[800u];
            for (i = 0u; i < 96u; i++) {
                uint32_t bits;
                sum += hp[i];
                memcpy(&bits, &hp[i], 4u);
                (void)snprintf(&hexbuf[i * 8u], 9u, "%08x",
                               (unsigned)bits);
            }
            hexbuf[768u] = '\0';
            printf("RS-N1HP w=%u rc=%d cyc=%u hp=%s sum=%.5e\r\n",
                   wtag[w], rc, cyc, hexbuf, (double)sum);
        }
    }
    printf("RS-SECHAIN rc=%d cyc_tot=%u\r\n", rc_tot, cyc_tot);
}
#endif /* !(EMS_N1_SEG_ONLY) */

#endif /* EMS_N1_LADDER */
