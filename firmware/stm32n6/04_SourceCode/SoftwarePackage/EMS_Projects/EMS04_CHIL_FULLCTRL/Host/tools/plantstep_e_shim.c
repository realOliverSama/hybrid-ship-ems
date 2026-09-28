/* ==========================================================================
 * plantstep_e_shim.c — S4-G2（spec: S4_FULLCTRL_CONSTRUCTION_SPEC.md）
 *
 * 用途: 把 Simulink Coder（GRT）为 HybridShip_ModelE_PlantStepE 生成的 C 接口
 *       封装为简单 C API（Host/plantstep_e/<scenario>/ 生成目录 + 本文件由
 *       tools/build_plantstep_e.sh 编译为 libplantstepE_<scenario>.dylib，供
 *       Host/plantstep_advancer.py backend='modelE' 经 ctypes 加载）。
 *       单步 = 0.01 s（模型 Fixed-step ode3 @0.01 s）；控制拍（10 s）= 1000 次
 *       单步，由 Python 侧承担（Model D 先例：拍内子步循环在 advancer）。
 *
 * TLC 选择依据: GRT 主案（Model D 先例定案）——本类含连续状态模型 ERT 报
 *   ContinuousTimeNotSupported 拒绝（2026-09-28 实测 ×10；同 Model D 2026-09-02
 *   fallback 记录），GRT 产物即 Model D 生产 dylib 同路径。
 *
 * C 导出 API（与 Model D plantstep_shim.c 同形，输出扩至 16）:
 *   int    plantstep_init(const char* scenario)   校验场景名 + initialize 复位 IC
 *   void   plantstep_set_pgen(double p_gen)       写根 Inport P_gen_ref_kW
 *   void   plantstep_step(void)                   单步 0.01 s
 *   int    plantstep_get_outputs(double* out16)   按根 Outport 1..16 顺序拷出
 *   double plantstep_get_fuel(void)               fuel_total_kg（累计）
 *   int    plantstep_get_voyage_stop(void)        Outport 11' (0/1)
 *   int    plantstep_get_states(double* out10)    10 连续状态直读（拍边界真值；
 *          GRT ode3 下 Y 写出与 X 直读的时序差异与 Model D 同理——比值以 X 为准）
 *
 * 返回码: 0 成功；init: -1 NULL / -2 场景不符；get_*: -1 NULL。
 * ========================================================================== */
#include <string.h>
#include "HybridShip_ModelE_PlantStepE.h"

#ifndef PS_BAKED_SCENARIO
#define PS_BAKED_SCENARIO "S20_composite_voyage"   /* 由 build_plantstep_e.sh -D 注入 */
#endif

#define PS_INIT_FN()  HybridShip_ModelE_PlantStepE_initialize()
#define PS_STEP_FN()  HybridShip_ModelE_PlantStepE_step()
#define PS_U          HybridShip_ModelE_PlantStepE_U
#define PS_U_PGEN     P_gen_ref_kW
#define PS_Y          HybridShip_ModelE_PlantStepE_Y
/* 根 Outport 1..16 字段名（顺序 = 20E audit root_outports，禁重排） */
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
/* 连续状态直读（X 结构字段顺序 = 生成头定义顺序，禁重排） */
#define PS_X          HybridShip_ModelE_PlantStepE_X
#define PS_X_VDC      V_int_CSTATE
#define PS_X_SOC      SOC_int_CSTATE
#define PS_X_DRIVE    DriveLag_CSTATE
#define PS_X_KI       Ki_int_CSTATE
#define PS_X_V        v_ship_CSTATE
#define PS_X_XPOS     x_ship_CSTATE
#define PS_X_OMEGA    omega_prop_CSTATE
#define PS_X_I3       int_3_CSTATE
#define PS_X_I1       int_1_CSTATE
#define PS_X_I2       int_2_CSTATE

#ifdef __cplusplus
extern "C" {
#endif

int plantstep_init(const char* scenario)
{
    if (scenario == NULL) {
        return -1;
    }
    if (strcmp(scenario, PS_BAKED_SCENARIO) != 0) {
        return -2;   /* 场景与烘焙常量不符（dylib 选错）——拒绝初始化 */
    }
    PS_INIT_FN();    /* 复位全部 IC（SOC=Bat_SOC_init、x/v=0、fuel=0） */
    return 0;
}

void plantstep_set_pgen(double p_gen)
{
    PS_U.PS_U_PGEN = p_gen;
}

void plantstep_step(void)
{
    PS_STEP_FN();
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
    /* out16[15] = voyage_stop 由 get_voyage_stop 承担；此处填 0 占位 */
    out16[15] = (double)PS_Y.PS_Y_VSTOP;
    return 0;
}

double plantstep_get_fuel(void)
{
    return PS_Y.PS_Y_FUEL;
}

int plantstep_get_voyage_stop(void)
{
    return (int)PS_Y.PS_Y_VSTOP;
}

int plantstep_get_states(double* out10)
{
    if (out10 == NULL) {
        return -1;
    }
    out10[0] = PS_X.PS_X_VDC;
    out10[1] = PS_X.PS_X_SOC;
    out10[2] = PS_X.PS_X_DRIVE;
    out10[3] = PS_X.PS_X_KI;
    out10[4] = PS_X.PS_X_V;
    out10[5] = PS_X.PS_X_XPOS;
    out10[6] = PS_X.PS_X_OMEGA;
    out10[7] = PS_X.PS_X_I3;
    out10[8] = PS_X.PS_X_I1;
    out10[9] = PS_X.PS_X_I2;
    return 0;
}

#ifdef __cplusplus
}
#endif
