/* ==========================================================================
 * plantstep_e_stream_shim.c — S4-P3 B2（spec: S4_P3_STREAM_PLANT_SPEC.md §2）
 *
 * 用途: 把 Simulink Coder（GRT）为 HybridShip_ModelE_PlantStepE_Stream 生成的
 *       C 接口封装为简单 C API，并由本 shim 在 init 时读取同目录航次流回放
 *       bin（<vid>/<vid>.stream.f64.bin，tools/s4_p3_pack_stream.py 产物，
 *       格式 EMS04STM v1 = 20B 头 + f64×4 行 [t_s, P_prop_realized_kW,
 *       P_hotel_kW, v_ship_mps]），每 substep 以 ZOH 喂 4 根 Inport
 *       （stream 1 Hz 行 → 每 100 substeps 换行）。同一份 codegen 生成码 +
 *       本 shim 编译出的 libplantstepE_stream.dylib 每航次目录一份（4 航次
 *       共用同一二进制），供 Host/plantstep_advancer.py backend='streamE'
 *       经 ctypes 加载。
 *
 * 单步 = 0.01 s（模型 Fixed-step ode3 @0.01 s）；控制拍（10 s）= 1000 次单步
 *       由 Python 侧承担（Model D / modelE 先例同口径）。
 *
 * C 导出 API（与 plantstep_e_shim.c 同形，无烘焙场景校验——航次由 init 参数
 *            动态指定，通过 advancer SCENARIOS_E_STREAM 注册表约束）:
 *   int    plantstep_init(const char* scenario)   读 bin + initialize 复位 IC
 *   void   plantstep_set_pgen(double p_gen)       写根 Inport P_gen_ref_kW
 *   void   plantstep_step(void)                   ZOH 喂流行 + 单步 0.01 s
 *   int    plantstep_get_outputs(double* out16)   根 Outport 1..16 契约序拷出
 *   double plantstep_get_fuel(void)               fuel_total_kg（累计）
 *   int    plantstep_get_voyage_stop(void)        行尽 ∨ 模型 voyage_stop（先到为准）
 *   int    plantstep_get_states(double* out10)    10 连续状态【契约序】直读
 *
 * get_states 契约序（advancer ModelEBackend 位置契约，禁变）:
 *   [0]V_dc  [1]SOC  [2]drive_lag  [3]ki  [4]v_ship  [5]x_ship  [6]omega
 *   [7]int_3 [8]int_1 [9]int_2
 *   ⚠ 本变体生成码 X 字段序与 PlantStepE 不同（实测 2026-09-29：
 *     V_int,SOC_int,v_ship,omega_prop,x_ship,int_3,Ki_int,DriveLag,int_1,int_2），
 *     此处按名映射归一到契约序。
 *
 * bin 路径解析: dladdr 取本 dylib 目录 → <dir>/<scenario>.stream.f64.bin
 *   （每航次目录一份 dylib 的布局保证同目录性；禁路径分隔符防穿越）。
 *
 * 返回码: 0 成功；init: -1 NULL / -2 场景名非法 / -3 bin 缺失或格式不符 /
 *         -4 dladdr 失败 / -5 内存分配失败；get_*: -1 NULL。
 * ========================================================================== */
#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HybridShip_ModelE_PlantStepE_Stream.h"

#define PS_INIT_FN()  HybridShip_ModelE_PlantStepE_Stream_initialize()
#define PS_STEP_FN()  HybridShip_ModelE_PlantStepE_Stream_step()
#define PS_U          HybridShip_ModelE_PlantStepE__U
#define PS_Y          HybridShip_ModelE_PlantStepE__Y
#define PS_X          HybridShip_ModelE_PlantStepE__X

/* 根 Outport 1..16 字段名（顺序 = OUT_KEYS_E 契约，禁重排） */
#define PS_Y_T        t_s
#define PS_Y_PLOAD    P_load_total_kW
#define PS_Y_SOC      SOC
#define PS_Y_VCMD     v_cmd_mps
#define PS_Y_VSHIP    v_ship_mps
#define PS_Y_ACCEL    acceleration_mps2
#define PS_Y_X        x_ship_m
#define PS_Y_FUEL     fuel_total_kg
#define PS_Y_PBAT     P_bat_kW
#define PS_Y_PPROP    P_prop_kW
#define PS_Y_SDG1     state_DG1
#define PS_Y_SDG2     state_DG2
#define PS_Y_PDG1     P_DG1_kW
#define PS_Y_PDG2     P_DG2_kW
#define PS_Y_VDC      V_dc_V
#define PS_Y_VSTOP    voyage_stop            /* boolean_T (uint8) */

/* stream bin 常量（tools/s4_p3_pack_stream.py 冻结格式） */
#define STM_MAGIC     "EMS04STM"
#define STM_HDR_BYTES 20
#define SUBSTEPS_PER_ROW 100                /* 1 Hz 行 × 0.01 s 子步 */

typedef struct {
    double *rows;        /* n_rows × 4 [t_s, prop, hotel, v] */
    unsigned long n_rows;
    unsigned long long k;   /* 已执行 substep 数（init 复位） */
    int row_exhausted;
} stream_state;

static stream_state g_stream = {NULL, 0, 0, 0};

#ifdef __cplusplus
extern "C" {
#endif

static int stream_load(const char* scenario)
{
    char self_path[4096];
    char dir_buf[4096];
    char bin_path[4096];
    Dl_info info;
    FILE* fh;
    char magic[8];
    unsigned int ver, n_rows, reserved;
    unsigned long i;
    double* rows;

    if (dladdr((void*)&stream_load, &info) == 0 || info.dli_fname == NULL) {
        return -4;
    }
    snprintf(self_path, sizeof(self_path), "%s", info.dli_fname);
    snprintf(dir_buf, sizeof(dir_buf), "%s", self_path);
    {
        char* d = dirname(dir_buf);
        snprintf(bin_path, sizeof(bin_path), "%s/%s.stream.f64.bin", d,
                 scenario);
    }

    fh = fopen(bin_path, "rb");
    if (fh == NULL) {
        return -3;
    }
    if (fread(magic, 1, 8, fh) != 8 || memcmp(magic, STM_MAGIC, 8) != 0) {
        fclose(fh);
        return -3;
    }
    if (fread(&ver, 4, 1, fh) != 1 || ver != 1u ||
        fread(&n_rows, 4, 1, fh) != 1 || n_rows < 2u ||
        fread(&reserved, 4, 1, fh) != 1) {
        fclose(fh);
        return -3;
    }
    rows = (double*)malloc((size_t)n_rows * 4u * sizeof(double));
    if (rows == NULL) {
        fclose(fh);
        return -5;
    }
    if (fread(rows, sizeof(double) * 4u, n_rows, fh) != n_rows) {
        free(rows);
        fclose(fh);
        return -3;
    }
    fclose(fh);

    if (g_stream.rows != NULL) {
        free(g_stream.rows);   /* 重复 init（同 dylib 换航次）安全 */
    }
    g_stream.rows = rows;
    g_stream.n_rows = (unsigned long)n_rows;
    return 0;
}

int plantstep_init(const char* scenario)
{
    if (scenario == NULL || scenario[0] == '\0') {
        return -1;
    }
    if (strchr(scenario, '/') != NULL || strchr(scenario, '\\') != NULL ||
        strstr(scenario, "..") != NULL) {
        return -2;   /* 禁路径穿越（bin 名 = <scenario>.stream.f64.bin） */
    }
    if (stream_load(scenario) != 0) {
        return -3;
    }
    g_stream.k = 0;
    g_stream.row_exhausted = 0;
    PS_INIT_FN();    /* 复位全部 IC（SOC=Bat_SOC_init、fuel=0、x_ship_int=0） */
    return 0;
}

void plantstep_set_pgen(double p_gen)
{
    PS_U.P_gen_ref_kW = p_gen;
}

void plantstep_step(void)
{
    unsigned long row;
    const double* r;
    if (g_stream.rows == NULL || g_stream.n_rows == 0) {
        return;   /* 未 init：模型按零输入推进（调用方契约错误，由
                     Python 侧 fail-closed 承担） */
    }
    row = (unsigned long)(g_stream.k / SUBSTEPS_PER_ROW);
    if (row >= g_stream.n_rows) {
        row = g_stream.n_rows - 1;   /* 行尽钳位末行 */
        g_stream.row_exhausted = 1;
    }
    r = g_stream.rows + row * 4u;
    PS_U.P_prop_realized_kW = r[1];
    PS_U.P_hotel_kW = r[2];
    PS_U.v_ship_mps_replay = r[3];
    PS_STEP_FN();
    g_stream.k++;
}

int plantstep_get_outputs(double* out16)
{
    if (out16 == NULL) {
        return -1;
    }
    out16[0]  = PS_Y.PS_Y_T;
    out16[1]  = PS_Y.PS_Y_PLOAD;
    out16[2]  = PS_Y.PS_Y_SOC;
    out16[3]  = PS_Y.PS_Y_VCMD;
    out16[4]  = PS_Y.PS_Y_VSHIP;
    out16[5]  = PS_Y.PS_Y_ACCEL;
    out16[6]  = PS_Y.PS_Y_X;
    out16[7]  = PS_Y.PS_Y_FUEL;
    out16[8]  = PS_Y.PS_Y_PBAT;
    out16[9]  = PS_Y.PS_Y_PPROP;
    out16[10] = (double)PS_Y.PS_Y_SDG1;
    out16[11] = (double)PS_Y.PS_Y_SDG2;
    out16[12] = PS_Y.PS_Y_PDG1;
    out16[13] = PS_Y.PS_Y_PDG2;
    out16[14] = PS_Y.PS_Y_VDC;
    out16[15] = (double)PS_Y.PS_Y_VSTOP;
    return 0;
}

double plantstep_get_fuel(void)
{
    return PS_Y.PS_Y_FUEL;
}

int plantstep_get_voyage_stop(void)
{
    if (g_stream.row_exhausted) {
        return 1;   /* 行尽（先到为准语义之一） */
    }
    return (int)PS_Y.PS_Y_VSTOP;
}

int plantstep_get_states(double* out10)
{
    if (out10 == NULL) {
        return -1;
    }
    /* 契约序（见文件头）：本变体 X 字段序不同，按名映射归一 */
    out10[0] = PS_X.V_int_CSTATE;        /* V_dc_V        */
    out10[1] = PS_X.SOC_int_CSTATE;      /* SOC           */
    out10[2] = PS_X.DriveLag_CSTATE;     /* drive_lag_s   */
    out10[3] = PS_X.Ki_int_CSTATE;       /* ki_integrator */
    out10[4] = PS_X.v_ship_CSTATE;       /* v_ship_mps    */
    out10[5] = PS_X.x_ship_CSTATE;       /* x_ship_m      */
    out10[6] = PS_X.omega_prop_CSTATE;   /* omega_prop    */
    out10[7] = PS_X.int_3_CSTATE;        /* fuel_int_3    */
    out10[8] = PS_X.int_1_CSTATE;        /* fuel_int_1    */
    out10[9] = PS_X.int_2_CSTATE;        /* fuel_int_2    */
    return 0;
}

#ifdef __cplusplus
}
#endif
