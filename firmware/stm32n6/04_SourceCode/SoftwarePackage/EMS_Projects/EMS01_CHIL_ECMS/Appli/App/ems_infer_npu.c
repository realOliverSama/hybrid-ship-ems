/* ems_infer_npu.c — NPU 推理后端：LL_ATON int8 封装（T-505，spec ems01-chil-ecms-v1）。
 *
 * 推理序列照 991 应用（Projects/99_Applications/991_AI_People_Detection/
 * Appli/Core/Src/app.c:155-161，ThreadX 队列部分不移植，仅取四步核心序列）：
 *   LL_ATON_Set_User_Input_Buffer_x(0, in, len);
 *   SCB_InvalidateDCache_by_Addr(out, out_len);
 *   LL_ATON_Set_User_Output_Buffer_x(0, out, out_len);
 *   LL_ATON_RT_Main(&NN_Instance_x);
 * 逐模型实例化：LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Mlpq/Workclf)，
 * 生成符号的改名见 ems_npu_mlpq.c / ems_npu_workclf.c。
 *
 * 输入语义（design R8，板上实测验证归 T-507）：模型为 QDQ float I/O（int8 量化
 * 参数在图内，T-502 analyze），固件向 LL_ATON 输入缓冲喂 float32：
 *   mlpq    —— ems_ctrl 归一化后的 302 维窗口（EML:64，R8 单份预处理）；
 *   workclf —— 原始物理量（Pdec 30 点 + 旁路 [v, a]，识别器不吃归一化，EML:82）。
 * double 入参 → float32 的转换与 T-307 Python int8 reference 的 f32 口径一致。
 *
 * 后端职责边界（design R8）：只做张量前向，无预处理/反预处理（归一化、Pdec 抽取、
 * 反归一化都在 ems_ctrl.c / 调用方）。返回值契约见 ems_infer.h（0=OK，非 0=失败，
 * 触发 ê=0 回退）。us 输出用 DWT CYCCNT 实测（使能见 main.c USER CODE，NFR-505）。
 *
 * 权重在 NOR 0x70200000（MLP-Q）/ 0x70400000（workclf），绝对寻址烘于生成代码
 * （991 同款机制），不占 ELF 段；读取前提是 DEBUG 模式 MX_XSPI2_Init + NOR
 * memory-mapped（main.c USER CODE，991 同序列）。
 */
#include <math.h>
#include <stdint.h>

#include "ll_aton_NN_interface.h"
#include "ll_aton_runtime.h"

#include "ems_infer.h"

/* ---- 双模型实例（design §CMake 集成：经宏区分实例名） ---- */
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Mlpq)
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Workclf)

/* 张量字节长度（生成物 network.h：mlp_q IN_1=1208/OUT_1=12；workclf
 * IN_1=8/IN_2=120/OUT_1=28；int8 QDQ 模型的 I/O 为 f32） */
#define EMS_NPU_MLPQ_IN_BYTES   (302u * 4u) /* LL_ATON_MLPQ_IN_1_SIZE_BYTES = 1208 */
#define EMS_NPU_MLPQ_OUT_BYTES  (3u * 4u)   /* 12 */
#define EMS_NPU_CLF_IN0_BYTES   (2u * 4u)   /* 旁路 [v, a]，8 */
#define EMS_NPU_CLF_IN1_BYTES   (30u * 4u)  /* Pdec 30 点，120 */
#define EMS_NPU_CLF_OUT_BYTES   (7u * 4u)   /* 7 类 logits，28 */

/* 用户 I/O 缓冲：32 B 对齐（LL_ATON_DEFAULT_IN_x_ALIGNMENT/OUT_x_ALIGNMENT）。
 * NPU 侧经 npu_cache 访问，MCU 侧在 RT_Main 前后由生成代码插入的
 * LL_ATON_Cache_MCU_* 与此处的 SCB_InvalidateDCache_by_Addr 维护一致性（991 同款）。 */
static float mlpq_in[302]  __attribute__((aligned(32)));
static float mlpq_out[3]   __attribute__((aligned(32)));
static float clf_bypass[2] __attribute__((aligned(32)));
static float clf_pdec[30]  __attribute__((aligned(32)));
static float clf_out[7]    __attribute__((aligned(32)));

/* T-506 遥测累加器：上一控制拍 mlpq+workclf 合计 [μs]（ems_infer_last_us 契约）。
 * 调用方（ems_app.c）在 ems_ctrl_step 前 reset、拍后读取；INFER_TEST 路径也会
 * 累加，但应用层每控制拍前置 reset，RSP_TICK 只反映最近控制拍，链路独立。 */
static uint32_t ems_npu_last_us;

/* DWT CYCCNT → μs（NFR-505）。使能见 main.c USER CODE；SystemCoreClock 由
 * SystemCoreClockUpdate() 维护（DEBUG 时钟配置后为 600 MHz，时钟树 Do-Not-Change）。 */
static uint32_t ems_npu_now_us(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000u));
}

/* T-506 遥测（ems_infer.h 契约）：mlpq+workclf 合计时长。 */
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

    /* input 1 = Pdec 30 点（原始物理量）；input 0 = 旁路 [v, a]（EML:82-133 s8 序）。
     * 旁路元素序 [v, a] 为训练导出契约，板上实测验证归 T-507（design R8）。 */
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
