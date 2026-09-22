/* ==========================================================================
 * plantstep_shim.c — T-604（spec: ems01-chil-v3-closedloop, FR-602 / AC-602）
 *
 * 用途: 把 Simulink Coder（ERT）为被控对象子集模型 HybridShip_ModelD_PlantStep
 *       生成的 C 接口封装为简单 C API（Host/plantstep/<scenario>/ 生成目录下
 *       的代码 + 本文件由 tools/build_plantstep.sh 一起编译为
 *       libplantstep_<scenario>.dylib，供 Host/plantstep_advancer.py 经
 *       ctypes 加载）。单步 = 0.01 s（模型 Fixed-step ode3 @0.01 s）；控制拍
 *       （1 s）= 100 次单步，由 Python 侧承担（design §4.1 定案）。
 *
 * C 导出 API（FD3，六函数，符号集中本文件）:
 *   int    plantstep_init(const char* scenario)   校验场景名 + model_initialize
 *                                                 复位 IC（可重复调用 = 重置）
 *   void   plantstep_set_pgen(double p_gen)       写根 Inport P_gen_ref_kW
 *   void   plantstep_step(void)                   单步 0.01 s
 *   int    plantstep_get_outputs(double* out11)   按根 Outport 1..11 顺序拷出
 *   double plantstep_get_fuel(void)               fuel_total_kg（累计）
 *   int    plantstep_get_voyage_stop(void)        Outport 11 (0/1)
 *   int    plantstep_get_states(double* out9)     9 连续状态直读（拍边界真值；
 *          T-604 执行期增补: Y 写出于末级阶段时刻 k+1-0.0025，拍边界位级比值须
 *          经 X 直读——G-P2 状态门禁与闭环 harness 读点定案依据）
 *
 * 返回码约定（plantstep_init / plantstep_get_outputs）: 0 成功；负值失败
 *   （init: -1 scenario 为 NULL / -2 与烘焙场景不符；get_outputs: -1 out11 为 NULL）。
 *
 * ---------------------------------------------------------------------------
 * 【ERT 符号适配区】—— 所有对生成代码符号的引用集中在此，以生成代码实际
 * 符号为准（T-604 交付时主 Agent 执行 codegen 后逐项核对，见文件尾清单）:
 *   1) 模型头文件名（slbuild 产物，位于同一生成目录，-I 由编译脚本提供）;
 *   2) 初始化/单步入口函数名;
 *   3) 根 Inport/Outport 数据存储实例名与字段名。
 * 预期命名（ERT 默认规则，R2026a; model=<model>）:
 *   - 头文件      "HybridShip_ModelD_PlantStep.h"
 *   - 入口        HybridShip_ModelD_PlantStep_initialize() / _step()
 *   - 外部输入    全局实例 HybridShip_ModelD_PlantStep_U，
 *                 类型 ExtU_HybridShip_ModelD_PlantStep_T，字段 P_gen_ref_kW
 *   - 根输出      全局实例 HybridShip_ModelD_PlantStep_Y，
 *                 类型 ExtY_HybridShip_ModelD_PlantStep_T，11 字段见 PS_Y_*
 * 备选路径 B（若生成代码不 extern U/Y 全局实例，仅经实时模型指针暴露）:
 *   HybridShip_ModelD_PlantStep_M->inputs（ExtU_*_T*）/ ->outputs（ExtY_*_T*）
 *   ——将 PS_U/PS_Y 两宏改为经 M 指针解引用即可，其余代码不动。
 * 多速率拆步（若模型存在多个离散采样率，ERT 可能把 step 拆为
 *   _step0（连续积分）+ _step1（离散更新）等）: 调整 PS_STEP_FN 为按采样命中
 *   顺序调用各速率函数（模型基率 0.01 s，单任务下预期单一 _step——核对生成码）。
 * ========================================================================== */
#include <string.h>
#include "HybridShip_ModelD_PlantStep.h"

/* ------------------------- ERT 符号适配区（唯一改动点） ------------------ */
#ifndef PS_BAKED_SCENARIO
#define PS_BAKED_SCENARIO "S20"            /* 由 build_plantstep.sh -D 注入 */
#endif

#define PS_INIT_FN()  HybridShip_ModelD_PlantStep_initialize()
#define PS_STEP_FN()  HybridShip_ModelD_PlantStep_step()
#define PS_U          HybridShip_ModelD_PlantStep_U
#define PS_U_PGEN     P_gen_ref_kW          /* 根 Inport（端口 1，唯一） */
#define PS_Y          HybridShip_ModelD_PlantStep_Y
/* 根 Outport 1..11 字段名（顺序 = T-601 audit root_outports 1..11，禁重排） */
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
#define PS_Y_VSTOP    voyage_stop            /* boolean_T（uint8） */
/* 连续状态直读（拍边界位级可比值；G-P2 门禁状态比对与 harness 用）:
 *   X 结构字段顺序 = 生成头 X_HybridShip_ModelD_PlantStep_T 定义顺序（禁重排） */
#define PS_X          HybridShip_ModelD_PlantStep_X
#define PS_X_X        x_ship_CSTATE
#define PS_X_V        v_ship_CSTATE
#define PS_X_OMEGA    omega_prop_CSTATE
#define PS_X_DRIVE    DriveLag_CSTATE
#define PS_X_SOC      SOC_int_CSTATE
#define PS_X_VDC      V_int_CSTATE
#define PS_X_INERTIA  inertia_CSTATE
#define PS_X_KI       Ki_int_CSTATE
#define PS_X_FUEL     fuel_int_CSTATE
/* ------------------------------------------------------------------------ */

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
    PS_INIT_FN();    /* model_initialize: 复位全部 IC（SOC=0.60、x/v=0、fuel=0） */
    return 0;
}

void plantstep_set_pgen(double p_gen)
{
    PS_U.PS_U_PGEN = p_gen;   /* ZOH: 一次 set 后由 100 次 step 保持（wrapper 负责） */
}

void plantstep_step(void)
{
    PS_STEP_FN();             /* 恰好一个 0.01 s 子步 */
}

int plantstep_get_outputs(double* out11)
{
    if (out11 == NULL) {
        return -1;
    }
    out11[0]  = (double)PS_Y.PS_Y_T;       /* 1  t_s                [s]   */
    out11[1]  = (double)PS_Y.PS_Y_PLOAD;   /* 2  P_load_total_kW    [kW]  */
    out11[2]  = (double)PS_Y.PS_Y_SOC;     /* 3  SOC                [-]   */
    out11[3]  = (double)PS_Y.PS_Y_VCMD;    /* 4  v_cmd_mps          [m/s] */
    out11[4]  = (double)PS_Y.PS_Y_VSHIP;   /* 5  v_ship_mps         [m/s] */
    out11[5]  = (double)PS_Y.PS_Y_ACCEL;   /* 6  acceleration_mps2  [m/s²]*/
    out11[6]  = (double)PS_Y.PS_Y_X;       /* 7  x_ship_m           [m]   */
    out11[7]  = (double)PS_Y.PS_Y_FUEL;    /* 8  fuel_total_kg      [kg]  */
    out11[8]  = (double)PS_Y.PS_Y_PBAT;    /* 9  P_bat_kW           [kW]  */
    out11[9]  = (double)PS_Y.PS_Y_PPROP;   /* 10 P_prop_kW          [kW]  */
    out11[10] = (double)PS_Y.PS_Y_VSTOP;   /* 11 voyage_stop        0/1   */
    return 0;
}

/* 连续状态直读（拍边界真值；顺序 = X 结构定义序）:
 *   0 x_ship  1 v_ship  2 omega_prop  3 DriveLag  4 SOC  5 V_dc
 *   6 inertia(P_gen 实际)  7 Ki_int(速度 PI)  8 fuel_total */
int plantstep_get_states(double* out9)
{
    if (out9 == NULL) {
        return -1;
    }
    out9[0] = PS_X.PS_X_X;
    out9[1] = PS_X.PS_X_V;
    out9[2] = PS_X.PS_X_OMEGA;
    out9[3] = PS_X.PS_X_DRIVE;
    out9[4] = PS_X.PS_X_SOC;
    out9[5] = PS_X.PS_X_VDC;
    out9[6] = PS_X.PS_X_INERTIA;
    out9[7] = PS_X.PS_X_KI;
    out9[8] = PS_X.PS_X_FUEL;
    return 0;
}

double plantstep_get_fuel(void)
{
    return (double)PS_Y.PS_Y_FUEL;
}

int plantstep_get_voyage_stop(void)
{
    return (PS_Y.PS_Y_VSTOP != 0) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

/* ==========================================================================
 * 【主 Agent codegen 后核对清单】（对照 <scenario>/HybridShip_ModelD_PlantStep_ert_rtw/）:
 *   1) HybridShip_ModelD_PlantStep.h 是否 extern 声明
 *      HybridShip_ModelD_PlantStep_U（ExtU_*_T）与 _Y（ExtY_*_T）——若无，改用
 *      备选路径 B（经 HybridShip_ModelD_PlantStep_M->inputs/->outputs）;
 *   2) _initialize/_step 函数名与单/多速率拆步形态（单一 _step 预期）;
 *   3) ExtY 字段名与 11 Outport 一一对应（voyage_stop 为 boolean_T）;
 *   4) Inport U 字段名 P_gen_ref_kW、类型 real_T（double）;
 *   5) 时间语义: 每调用 _step 一次 = 0.01 s；t_s（Outport 1）在 100 步循环后的
 *      读数用于核对 Outport 写出时机（pre/post-integration，wrapper 冒烟打印）。
 *   编译入口: tools/build_plantstep.sh <scenario>（gcc -O2 -fPIC -shared，
 *   -DPS_BAKED_SCENARIO="<scenario>"，产物 libplantstep_<scenario>.dylib）。
 * ========================================================================== */