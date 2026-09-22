/* ems_npu_workclf.c — workclf 部署包编译单元（T-505，spec ems01-chil-ecms-v1）。
 *
 * 与 ems_npu_mlpq.c 同机制：把 workclf 生成代码（网络名 "Default"）的 10 个
 * 外部符号改名为 "*_Workclf"，避免与 MLP-Q 实例的多重定义。
 * ems_infer_npu.c 以 LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Workclf)
 * 声明实例并链接到这些改名后符号。
 *
 * workclf 输入面（T-502 生成物，Model/workclf/network.h）：
 *   input 0 = Input_41_out_0  f32(1x2)    = 旁路 [v_ship, accel]（8 B）
 *   input 1 = Input_0_out_0   f32(1x30x1) = Pdec 10s 重采样 30 点（120 B）
 *   output 0 = Dequantize_52_out_0 f32(1x7) = 7 类 logits（28 B）
 * 只引用 vendor 生成物原路径（Model/workclf/，T-502 产物只读），不复制文件。
 */

/* ---- network.c 的 8 个外部符号（生成代码，禁改源，故在编译单元改名） ---- */
#define LL_ATON_Set_User_Input_Buffer_Default    LL_ATON_Set_User_Input_Buffer_Workclf
#define LL_ATON_Get_User_Input_Buffer_Default    LL_ATON_Get_User_Input_Buffer_Workclf
#define LL_ATON_Set_User_Output_Buffer_Default   LL_ATON_Set_User_Output_Buffer_Workclf
#define LL_ATON_Get_User_Output_Buffer_Default   LL_ATON_Get_User_Output_Buffer_Workclf
#define LL_ATON_EpochBlockItems_Default          LL_ATON_EpochBlockItems_Workclf
#define LL_ATON_Input_Buffers_Info_Default       LL_ATON_Input_Buffers_Info_Workclf
#define LL_ATON_Output_Buffers_Info_Default      LL_ATON_Output_Buffers_Info_Workclf
#define LL_ATON_Internal_Buffers_Info_Default    LL_ATON_Internal_Buffers_Info_Workclf

/* ---- network_ecblobs.h 的 2 个外部符号 ---- */
#define LL_ATON_EC_Network_Init_Default          LL_ATON_EC_Network_Init_Workclf
#define LL_ATON_EC_Inference_Init_Default        LL_ATON_EC_Inference_Init_Workclf

#include "../../Model/workclf/network.c"
