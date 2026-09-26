/* ems_infer_npu.c — NPU 推理后端（T-505，spec ems01-chil-ecms-v1；FR-511 编译期
 * 换型 + 4.0.1 stai 迁移；TCN 槽位最终路线 = 4.0.1 全 CPU 编译形态，用户指令
 * 2026-09-03，spec ems01-chil-tcn64-swap 层②重试后换型）。
 *
 * 预测器槽位（FR-511）编译期二选一，与 CMake 选编的包装 TU 配套：
 *   EMS_PREDICTOR_TCNQ（默认，CMake EMS_PREDICTOR=tcnq）：ST Edge AI **4.0.1**
 *     stai API 路径，运行时栈 = Middlewares/STEdgeAI/v401 +
 *     NetworkRuntime1201_CM55_GCC.a。两网络生命周期不同（同 stai 接口）：
 *     - TCN（tcn_q_v2_cpu，全 CPU 编译形态，Lite 内核在 CM55 执行）：
 *       stai_runtime_init（一次，全局）→ stai_network_init（一次）→
 *       stai_network_set_activations（CPU 形态激活区用户缓冲，init 一次；NPU
 *       形态无此步——激活在 ATON 存储池分配）→
 *       stai_network_set_inputs/set_outputs（用户缓冲绑定，每次推理）→
 *       stai_network_run(STAI_MODE_SYNC)：CPU 形态为同步全图执行，mode 形参被
 *       生成代码忽略，返回 STAI_SUCCESS/错误码，无 RUNNING/DONE 状态、无
 *       stai_ext_* 生命周期。
 *     - workclf（NPU 形态生成代码）：照官方 hello_world main_stai.c
 *       （STEdgeAI_4.0.1/4.0/Projects/STM32N6570-DK/Applications/hello_world/
 *       Src/main_stai.c）stai_ext 生命周期：set_inputs/set_outputs →
 *       stai_ext_network_new_inference → run(STAI_MODE_ASYNC) → RUNNING_WFE:
 *       stai_ext_wfe + run_continue / RUNNING_NO_WFE: run_continue 循环至
 *       STAI_DONE（错误码 rc >= STAI_ERROR_GENERIC 显式失败出口）。
 *     用户 I/O 缓冲为 32B 对齐静态数组（官方 NPU_Validation
 *     ai_io_buffers_ATON.c connect_input_buffers 同款 user-allocated 模式）：
 *     TCN CPU 形态前向全在 CPU 可缓存域执行，无 epoch 级 cache 维护需求；
 *     workclf NPU 形态由生成代码 epoch 级 LL_ATON_Cache_MCU_* 完成（输出缓冲
 *     在 SW 写出前 invalidate）；run 前 SCB_InvalidateDCache_by_Addr 为 2.2
 *     T-507 板上验证过的同款序列保留（CPU 形态无害双保险）。
 *   未定义（EMS_PREDICTOR=mlpq，可逆性出口）：v3 现状 2.2 LL_ATON 路径逐字节
 *     保留（LL_ATON_Set_User_Input_Buffer_x + LL_ATON_RT_Main，运行时栈
 *     NetworkRuntime1020；-DEMS_PREDICTOR=mlpq + mlpq hex 即回 v3 收官态）。
 *
 * 接口契约（ems_infer.h，零改动）：ems_infer_mlpq（TCN 槽位实现 = TCN-f64
 * 双输入）/ ems_infer_workclf / ems_infer_last_us_reset / ems_infer_last_us；
 * 返回 0=OK，非 0=失败（触发调用方 ê=0 回退）；输出非有限 → 失败。
 *
 * 输入语义（design R8）：模型为 QDQ float I/O（量化在图内），固件喂 float32：
 *   tcnq    —— in0 = x302[0:300] 归一化窗（[1,300,1] NHWC），in1 = x302[300:302]
 *              = [v, a] aux（[1,2]，训练导出契约序，FR-704）；
 *   workclf —— in0 = 旁路 [v, a]（8B），in1 = Pdec 30 点（120B）。
 * double 入参 → float32 转换口径不变（与 host int8 reference 的 f32 口径一致）。
 *
 * 后端职责边界（design R8）：只做张量前向，无预处理/反预处理。
 * 权重：tcnq CPU 形态在二进制内（network_data.c 编入 ELF：Debug SRAM
 * .rodata / Release XSPI2 NOR .rodata，不占 RAM，TCN 不再依赖 NOR 权重
 * hex）；workclf 仍在 NOR 0x70400000，绝对寻址烘于生成代码，不占 ELF 段，
 * 读取前提是 DEBUG 模式 MX_XSPI2_Init + NOR memory-mapped（main.c USER
 * CODE，991 同序列）。us 遥测 = DWT CYCCNT 实测（NFR-505）。
 */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "ems_infer.h"
#include "ems_stai_rt.h" /* phase4-g：stai runtime 全局一次初始化共享助手 */

#if defined(EMS_PREDICTOR_TCNQ)

/* ---- 4.0.1 stai 路径（TCN + workclf，双网络共存经生成符号网络名后缀） ---- */
#include "stai.h"
#include "stai_ext.h" /* stai_ext_wfe 声明（workclf NPU 形态 STAI_RUNNING_WFE 等待序列） */
#include "../../Model/workclf/stai_workclf.h" /* workclf：4.0.1 NPU 形态（stai ext 生命周期） */
#if !defined(EMS_NO_TCNQ_NET) /* phase4-g：Release 配置排除 TCN 网络见文件尾注与 CMake 注释 */
#include "../../Model/tcn_q_v2_cpu/network.h" /* TCN：4.0.1 全 CPU 形态（stai 同名 API + STAI_NETWORK_* 宏） */
#endif

/* 张量字节长度（生成物 network.h / workclf.h；编译期防漂移 static assert
 * 见 ems_npu_tcnq.c） */
#define EMS_NPU_TCNQ_IN0_BYTES  (300u * 4u) /* STAI_NETWORK_IN_1_SIZE_BYTES = 1200 */
#define EMS_NPU_TCNQ_IN1_BYTES  (2u * 4u)   /* STAI_NETWORK_IN_2_SIZE_BYTES = 8 */
#define EMS_NPU_TCNQ_OUT_BYTES  (3u * 4u)   /* STAI_NETWORK_OUT_1_SIZE_BYTES = 12 */
#define EMS_NPU_CLF_IN0_BYTES   (2u * 4u)   /* LL_ATON_WORKCLF_IN_1_SIZE_BYTES = 8 */
#define EMS_NPU_CLF_IN1_BYTES   (30u * 4u)  /* LL_ATON_WORKCLF_IN_2_SIZE_BYTES = 120 */
#define EMS_NPU_CLF_OUT_BYTES   (7u * 4u)   /* LL_ATON_WORKCLF_OUT_1_SIZE_BYTES = 28 */

/* stai 上下文：官方 STAI_NETWORK_CONTEXT_DECLARE（stai.h；STAI_ALIGNED(8)
 * 零初始化字节数组；tcnq CPU 形态尺寸 = sizeof(_stai_network_context)，
 * workclf NPU 形态 = sizeof(_stai_aton_context)，.bss 常规放置）。 */
#if !defined(EMS_NO_TCNQ_NET)
STAI_NETWORK_CONTEXT_DECLARE(ems_stai_ctx_tcnq, STAI_NETWORK_CONTEXT_SIZE)
#endif
STAI_NETWORK_CONTEXT_DECLARE(ems_stai_ctx_workclf, STAI_WORKCLF_CONTEXT_SIZE)

/* 用户 I/O 缓冲：32 B 对齐（STAI_CACHE_USER_BUFFER_ALIGNMENT，生成代码
 * Set_User/set_inputs 同款校验；对齐+独占缓存行与 2.2 后端同惯例）。
 * workclf NPU 侧经 npu_cache 访问，用户缓冲进出 cache 维护由生成代码 epoch 级
 * LL_ATON_Cache_MCU_* 处理（输出缓冲在 SW 写出前 invalidate）；TCN CPU 形态
 * 前向全在 CPU 可缓存域，无 epoch 级 cache 维护需求。 */
#if !defined(EMS_NO_TCNQ_NET)
static float tcnq_win[300] __attribute__((aligned(32))); /* in0：归一化窗 [1,300,1] */
static float tcnq_aux[2]   __attribute__((aligned(32))); /* in1：aux [v, a] [1,2] */
static float tcnq_out[3]   __attribute__((aligned(32))); /* out1：q3_norm */
/* CPU 形态激活区（生成 network.h STAI_NETWORK_ACTIVATIONS_SIZE_BYTES = 194,356 B
 * 单段；尺寸随生成物自动同步，布局漂移编译期阻断）。对齐 32 ≥ 生成要求
 * STAI_NETWORK_ACTIVATION_1_ALIGNMENT = 4（与其他用户缓冲同 cache line 惯例）。*/
static uint8_t tcnq_act[STAI_NETWORK_ACTIVATIONS_SIZE_BYTES] __attribute__((aligned(32)));
#endif
static float clf_bypass[2] __attribute__((aligned(32))); /* clf in0：旁路 [v, a] */
static float clf_pdec[30]  __attribute__((aligned(32))); /* clf in1：Pdec 30 点 */
static float clf_out[7]    __attribute__((aligned(32))); /* clf out：7 类 logits */

/* 懒初始化状态：stai_runtime_init 全局一次；每网络 stai_network_init 一次。
 * 控制路径单任务顺序调用（ems_ctrl_step → ems_infer_mlpq/ems_infer_workclf），
 * INFER_TEST/WC_TEST 路径同任务，无并发。 */
static bool ems_stai_runtime_ready;
#if !defined(EMS_NO_TCNQ_NET)
static bool ems_stai_tcnq_ready;
#endif
static bool ems_stai_workclf_ready;

/* T-506 遥测累加器：上一控制拍 tcnq+workclf 合计 [μs]（ems_infer_last_us 契约）。 */
static uint32_t ems_npu_last_us;

/* DWT CYCCNT → μs（NFR-505）。使能见 main.c USER CODE；SystemCoreClock 由
 * SystemCoreClockUpdate() 维护（时钟树 Do-Not-Change）。 */
static uint32_t ems_npu_now_us(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
}

void ems_infer_last_us_reset(void)
{
    ems_npu_last_us = 0u;
}

uint32_t ems_infer_last_us(void)
{
    return ems_npu_last_us;
}

/* EML:134 的 MATLAB max 语义：平局取首个（严格大于才替换）。与
 * ems_ctrl_argmax_first 同语义但独立实现——后者是 L0 白盒缝合面
 * （ems_ctrl.h"生产代码勿另行调用"），后端不得反向依赖控制律单元。 */
static int ems_npu_argmax_first(const float *v, int n)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (v[i] > v[best]) {
            best = i;
        }
    }
    return best;
}

static void ems_stai_runtime_init_once(void)
{
    /* phase4-g：收敛到共享助手 ems_stai_rt（reatt 服务与槽位路径共用全局
     * 一次契约；main_stai.c 同构：runtime init + runtime 回调 NULL）。 */
    ems_stai_rt_init_once();
    ems_stai_runtime_ready = ems_stai_rt_ready();
}

/* ASYNC 推理等待（main_stai.c 循环同构 + 显式错误出口）：
 * rc = stai_network_run 返回值起，RUNNING_WFE → stai_ext_wfe() 后推进，
 * RUNNING_NO_WFE → 直接推进；run_continue 每步返回当前执行状态；
 * STAI_DONE → 0；rc >= STAI_ERROR_GENERIC（stai.h 错误码契约）→ 1。 */
static int ems_stai_wait_done(stai_network *ctx, stai_return_code rc,
                              stai_return_code (*run_continue)(stai_network *))
{
    while (rc != STAI_DONE) {
        if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
            return 1;
        }
        if (rc == STAI_RUNNING_WFE) {
            stai_ext_wfe();
        }
        /* RUNNING_WFE / RUNNING_NO_WFE：推进 epoch block（中间件
         * ll_aton_stai_internal.c __LL_ATON_START_CONT_EXEC）。 */
        rc = run_continue(ctx);
    }
    return 0;
}

/* TCN 槽位网络一次性初始化：stai_network_init + 激活区绑定（CPU 形态特有：
 * 生成代码 init 后 _activations=NULL，run 前不绑定则报
 * STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR）。用户 I/O 缓冲实际绑定每次推理
 * 重做（与 2.2 Set_User 序列同构）。 */
volatile uint32_t ems_dbg_dbg[8]; /* T-816b 诊断插桩（临时） */
#if !defined(EMS_NO_TCNQ_NET)
static int ems_stai_tcnq_init_once(void)
{
    ems_stai_runtime_init_once();
    if (!ems_stai_tcnq_ready) {
        ems_dbg_dbg[0] = 1;
        stai_return_code rc_i = stai_network_init(ems_stai_ctx_tcnq);
        ems_dbg_dbg[1] = (uint32_t)rc_i;
        if (rc_i != STAI_SUCCESS) {
            return 1;
        }
        stai_ptr act_ptrs[STAI_NETWORK_ACTIVATIONS_NUM] = { (stai_ptr)tcnq_act };
        stai_return_code rc_a = stai_network_set_activations(ems_stai_ctx_tcnq, act_ptrs,
                                         STAI_NETWORK_ACTIVATIONS_NUM);
        ems_dbg_dbg[2] = (uint32_t)rc_a;
        if (rc_a != STAI_SUCCESS) {
            return 1;
        }
        ems_stai_tcnq_ready = true;
    }
    return 0;
}
#endif /* !EMS_NO_TCNQ_NET */

static int ems_stai_workclf_init_once(void)
{
    ems_stai_runtime_init_once();
    if (!ems_stai_workclf_ready) {
        if (stai_workclf_init(ems_stai_ctx_workclf) != STAI_SUCCESS) {
            return 1;
        }
        ems_stai_workclf_ready = true;
    }
    return 0;
}

/* TCN-f64 双输入前向（FR-704）。in0 = 归一化窗 x302[0:300]（升序，[1,300,1]）；
 * in1 = aux = x302[300:302] = [v, a]（[1,2]，训练导出契约序）；输出 3×f32 =
 * q3_norm。double → float32 口径不变（QDQ float I/O，量化在图内）。 */
#if !defined(EMS_NO_TCNQ_NET)
int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us)
{
    uint32_t t0 = ems_npu_now_us();
    uint32_t dt;
    stai_return_code rc;
    stai_ptr in_ptrs[STAI_NETWORK_IN_NUM];
    stai_ptr out_ptrs[STAI_NETWORK_OUT_NUM];

    if (ems_stai_tcnq_init_once() != 0) {
        return 1;
    }

    for (int i = 0; i < 300; i++) {
        tcnq_win[i] = (float)x302[i];
    }
    tcnq_aux[0] = (float)x302[300];
    tcnq_aux[1] = (float)x302[301];

    /* 2.2 T-507 板上验证过的同款序列：run 前失配输出缓冲行。TCN CPU 形态
     * 输出由 CPU 直接写出（可缓存域天然一致），此前失效无害；workclf NPU
     * 形态生成代码在 SW 写出前还会再 invalidate 一次 user 输出缓冲。 */
    SCB_InvalidateDCache_by_Addr(tcnq_out, EMS_NPU_TCNQ_OUT_BYTES);

    /* 用户缓冲绑定（stai API，buffer index 序 = 生成 buffers_info 序：
     * 0 = 1200B 归一化窗，1 = 8B aux）。 */
    in_ptrs[0] = (stai_ptr)tcnq_win;
    in_ptrs[1] = (stai_ptr)tcnq_aux;
    out_ptrs[0] = (stai_ptr)tcnq_out;
    stai_return_code rc_si = stai_network_set_inputs(ems_stai_ctx_tcnq, in_ptrs, STAI_NETWORK_IN_NUM);
    stai_return_code rc_so = stai_network_set_outputs(ems_stai_ctx_tcnq, out_ptrs, STAI_NETWORK_OUT_NUM);
    ems_dbg_dbg[3] = (uint32_t)rc_si;
    ems_dbg_dbg[4] = (uint32_t)rc_so;
    if (rc_si != STAI_SUCCESS || rc_so != STAI_SUCCESS) {
        return 1;
    }

    /* ---- CPU 形态推理序列：stai_network_run 同步全图执行（mode 被生成代码
     * 忽略，用 SYNC 表意）。返回值 = 执行状态枚举（stai.h：RUNNING_WFE/
     * RUNNING_NO_WFE/STAI_DONE），CPU 形态内联全图完成返回 STAI_DONE——
     * 非 STAI_SUCCESS（2026-09-03 板上实证：原 rc != STAI_SUCCESS 判定把
     * STAI_DONE 误判为失败 → ê=0 回退，588ms 正确计算被丢弃）。
     * 修正：仅错误码域（>= STAI_ERROR_GENERIC）判失败——stai.h 错误码契约。
     * 无 stai_ext_network_new_inference/run_continue（NPU 形态专用，workclf
     * 路径仍走 ems_stai_wait_done）。---- */
    rc = stai_network_run(ems_stai_ctx_tcnq, STAI_MODE_SYNC);
    ems_dbg_dbg[5] = (uint32_t)rc;
    if ((uint32_t)rc >= (uint32_t)STAI_ERROR_GENERIC) {
        return 1;
    }

    dt = ems_npu_now_us() - t0;
    ems_npu_last_us += dt; /* FR-505：tcnq+workclf 合计（T-506 RSP_TICK 遥测） */
    if (us != NULL) {
        *us = dt;
    }

    /* 输出非有限 → 调用方 ê=0 回退（design Error Handling，与 REF 后端同契约） */
    for (int i = 0; i < 3; i++) {
        if (!isfinite((double)tcnq_out[i])) {
            return 1;
        }
        q3[i] = (double)tcnq_out[i]; /* 归一化域输出，反归一化属 ems_ctrl（EML:68） */
    }
    return 0;
}
#else /* EMS_NO_TCNQ_NET（Release：TCN 网络不链接，见文件头注） */
/* phase4-g（spec D4 细化）：Release 镜像中 TCN CPU 形态网络与 REATT CPU 形态
 * 网络不能共存——4.0.1 CPU 形态生成代码的每节点 forward_lite_* 函数为全局
 * 符号（与 NPU 形态 static epoch 函数不同），双图同链必撞（实测
 * forward_lite_eltwise_integer_INT8_eltwise_37 multiple definition）。本工程
 * 定位 = REATT 交付：Release 排除 TCN 网络（ems_infer_mlpq 槽位为明确回退桩，
 * INFER_TEST/TICK 预测路径恒 FALLBACK ê=0），TCN 槽位完整保留在 Debug
 * （= TCN64 基线形态）。legacy ECMS soft-HIL 的正式载体仍为
 * ../EMS01_CHIL_ECMS_TCN64/ 工程，不受本工程影响。 */
int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us)
{
    (void)x302;
    (void)q3;
    if (us != NULL) {
        *us = 0u;
    }
    return 1; /* 预测槽位不可用 → 调用方 FR-508 语义 FALLBACK（ê=0） */
}
#endif /* !EMS_NO_TCNQ_NET */

/* workclf 前向（2.2 路径语义不变，运行时迁 4.0.1 stai）。in0 = 旁路 [v, a]
 * （8B）；in1 = Pdec 30 点（120B）；out = 7 类 logits（28B）。 */
int ems_infer_workclf(const double pdec30[30], double v, double a, int *cls,
                      float out_logits[7], uint32_t *us)
{
    uint32_t t0 = ems_npu_now_us();
    uint32_t dt;
    stai_return_code rc;
    stai_ptr in_ptrs[STAI_WORKCLF_IN_NUM];
    stai_ptr out_ptrs[STAI_WORKCLF_OUT_NUM];

    if (ems_stai_workclf_init_once() != 0) {
        printf("WC-FAIL init\r\n");       /* D2 插桩：区分初始化 vs 执行阶段 */
        return 1;
    }

    for (int j = 0; j < 30; j++) {
        clf_pdec[j] = (float)pdec30[j];
    }
    clf_bypass[0] = (float)v;
    clf_bypass[1] = (float)a;

    /* 2.2 同款序列：run 前失配输出缓冲行。 */
    SCB_InvalidateDCache_by_Addr(clf_out, EMS_NPU_CLF_OUT_BYTES);

    /* 用户缓冲绑定（buffer index 序 = 生成 buffers_info 序：
     * 0 = 8B 旁路 [v, a]，1 = 120B Pdec 30 点）。 */
    in_ptrs[0] = (stai_ptr)clf_bypass;
    in_ptrs[1] = (stai_ptr)clf_pdec;
    out_ptrs[0] = (stai_ptr)clf_out;
    {
        stai_return_code rc_i = stai_workclf_set_inputs(ems_stai_ctx_workclf, in_ptrs,
                                                         STAI_WORKCLF_IN_NUM);
        stai_return_code rc_o = stai_workclf_set_outputs(ems_stai_ctx_workclf, out_ptrs,
                                                          STAI_WORKCLF_OUT_NUM);
        if (rc_i != STAI_SUCCESS || rc_o != STAI_SUCCESS) {
            printf("WC-FAIL setio i=%d o=%d\r\n", (int)rc_i, (int)rc_o);
            return 1;
        }
    }

    {
        /* 4.0.1 stai 语义（官方 hello_world/main_stai.c 同款）：每个 run 前调
         * new_inference（= __ll_aton_stai_reset），其要求 exec_status ∈ {DONE,
         * RUNNING_NO_WFE}，而 init 后为 STAI_SUCCESS ⇒ **首拍必返
         * STILL_RUNNING(0x50000)**，官方示例对该返回码不检查（预期内忽略）。
         * 本调用为单任务顺序（wait_done 后才返回），不存在真"仍在运行"。 */
        stai_return_code rc_n = stai_ext_workclf_new_inference(ems_stai_ctx_workclf);
        if (rc_n != STAI_SUCCESS && rc_n != STAI_ERROR_NETWORK_STILL_RUNNING) {
            printf("WC-FAIL newinf rc=%d\r\n", (int)rc_n);
            return 1;
        }
    }
    rc = stai_workclf_run(ems_stai_ctx_workclf, STAI_MODE_ASYNC);
    if (ems_stai_wait_done(ems_stai_ctx_workclf, rc, stai_ext_workclf_run_continue) != 0) {
        printf("WC-FAIL run rc=%d err=%d\r\n", (int)rc,
               (int)stai_workclf_get_error(ems_stai_ctx_workclf));   /* D2 插桩 */
        return 1;
    }

    dt = ems_npu_now_us() - t0;
    ems_npu_last_us += dt;
    if (us != NULL) {
        *us = dt;
    }

    for (int i = 0; i < 7; i++) {
        if (!isfinite((double)clf_out[i])) {
            return 1;
        }
    }

    if (out_logits != NULL) {
        /* 内部 clf_out[7] 拷给调用者（CMD_WC_TEST 回显；仅在有限性通过后拷贝） */
        for (int i = 0; i < 7; i++) {
            out_logits[i] = clf_out[i];
        }
    }

    /* 类别 1..7（EML:134 平局取首）；越界守卫在 ems_ctrl（design Error Handling） */
    *cls = ems_npu_argmax_first(clf_out, 7) + 1;
    return 0;
}

#else /* !EMS_PREDICTOR_TCNQ */

/* ---- v3 现状：MLP-Q 2.2 LL_ATON 路径（可逆性出口，逐字节保留 T-505 原样） ----
 * 推理序列照 991 应用（Projects/99_Applications/991_AI_People_Detection/
 * Appli/Core/Src/app.c:155-161）：Set_User_Input_Buffer_x / invalidate /
 * Set_User_Output_Buffer_x / LL_ATON_RT_Main。 */
#include "ll_aton_NN_interface.h"
#include "ll_aton_runtime.h"

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Mlpq)
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Workclf)

#define EMS_NPU_MLPQ_IN_BYTES   (302u * 4u) /* LL_ATON_NETWORK_MLPQ_IN_1_SIZE_BYTES = 1208 */
#define EMS_NPU_MLPQ_OUT_BYTES  (3u * 4u)   /* 12 */
#define EMS_NPU_CLF_IN0_BYTES   (2u * 4u)   /* 旁路 [v, a]，8 */
#define EMS_NPU_CLF_IN1_BYTES   (30u * 4u)  /* Pdec 30 点，120 */
#define EMS_NPU_CLF_OUT_BYTES   (7u * 4u)   /* 7 类 logits，28 */

static float mlpq_in[302]  __attribute__((aligned(32)));
static float mlpq_out[3]   __attribute__((aligned(32)));
static float clf_bypass[2] __attribute__((aligned(32)));
static float clf_pdec[30]  __attribute__((aligned(32)));
static float clf_out[7]    __attribute__((aligned(32)));

static uint32_t ems_npu_last_us;

static uint32_t ems_npu_now_us(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
}

void ems_infer_last_us_reset(void)
{
    ems_npu_last_us = 0u;
}

uint32_t ems_infer_last_us(void)
{
    return ems_npu_last_us;
}

static int ems_npu_argmax_first(const float *v, int n)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (v[i] > v[best]) {
            best = i;
        }
    }
    return best;
}

/* v3 现状：MLP-Q 单输入前向（T-505）——mlpq 选项（FR-511 可逆性出口）逐字节保留 */
int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us)
{
    uint32_t t0 = ems_npu_now_us();
    uint32_t dt;

    /* double → float32（QDQ float I/O 契约；量化在图内完成，design R8） */
    for (int i = 0; i < 302; i++) {
        mlpq_in[i] = (float)x302[i];
    }

    /* ---- 991 app.c:155-161 推理序列 ---- */
    LL_ATON_Set_User_Input_Buffer_Mlpq(0, mlpq_in, EMS_NPU_MLPQ_IN_BYTES);
    SCB_InvalidateDCache_by_Addr(mlpq_out, EMS_NPU_MLPQ_OUT_BYTES);
    LL_ATON_Set_User_Output_Buffer_Mlpq(0, mlpq_out, EMS_NPU_MLPQ_OUT_BYTES);
    LL_ATON_RT_Main(&NN_Instance_Mlpq);

    dt = ems_npu_now_us() - t0;
    ems_npu_last_us += dt; /* FR-505：mlpq+workclf 合计（T-506 RSP_TICK 遥测） */
    if (us != NULL) {
        *us = dt;
    }

    /* 输出非有限 → 调用方 ê=0 回退（design Error Handling，与 REF 后端同契约） */
    for (int i = 0; i < 3; i++) {
        if (!isfinite((double)mlpq_out[i])) {
            return 1;
        }
        q3[i] = (double)mlpq_out[i]; /* 归一化域输出，反归一化属 ems_ctrl（EML:68） */
    }
    return 0;
}

int ems_infer_workclf(const double pdec30[30], double v, double a, int *cls,
                      float out_logits[7], uint32_t *us)
{
    uint32_t t0 = ems_npu_now_us();
    uint32_t dt;

    /* input 1 = Pdec 30 点；input 0 = 旁路 [v, a]（EML:82-133 s8 序）。
     * 旁路元素序 [v, a] 为训练导出契约。 */
    for (int j = 0; j < 30; j++) {
        clf_pdec[j] = (float)pdec30[j];
    }
    clf_bypass[0] = (float)v;
    clf_bypass[1] = (float)a;

    /* ---- 991 app.c:155-161 推理序列（双输入模型） ---- */
    LL_ATON_Set_User_Input_Buffer_Workclf(1, clf_pdec, EMS_NPU_CLF_IN1_BYTES);
    LL_ATON_Set_User_Input_Buffer_Workclf(0, clf_bypass, EMS_NPU_CLF_IN0_BYTES);
    SCB_InvalidateDCache_by_Addr(clf_out, EMS_NPU_CLF_OUT_BYTES);
    LL_ATON_Set_User_Output_Buffer_Workclf(0, clf_out, EMS_NPU_CLF_OUT_BYTES);
    LL_ATON_RT_Main(&NN_Instance_Workclf);

    dt = ems_npu_now_us() - t0;
    ems_npu_last_us += dt;
    if (us != NULL) {
        *us = dt;
    }

    for (int i = 0; i < 7; i++) {
        if (!isfinite((double)clf_out[i])) {
            return 1;
        }
    }

    if (out_logits != NULL) {
        /* 内部 clf_out[7] 拷给调用者（CMD_WC_TEST 回显；仅在有限性通过后拷贝） */
        for (int i = 0; i < 7; i++) {
            out_logits[i] = clf_out[i];
        }
    }

    /* 类别 1..7（EML:134 平局取首）；越界守卫在 ems_ctrl（design Error Handling） */
    *cls = ems_npu_argmax_first(clf_out, 7) + 1;
    return 0;
}

#endif /* EMS_PREDICTOR_TCNQ */