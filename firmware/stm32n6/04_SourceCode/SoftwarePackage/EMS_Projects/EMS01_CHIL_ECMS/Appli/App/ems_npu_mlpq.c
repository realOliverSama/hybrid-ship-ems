/* ems_npu_mlpq.c — MLP-Q 部署包编译单元（T-505，spec ems01-chil-ecms-v1）。
 *
 * 两模型（mlp_q / workclf）经 STEdgeAI generate 时均未指定 --network-name，
 * 生成符号全部带 "_Default" 后缀（LL_ATON_Set_User_Input_Buffer_Default 等
 * 10 个外部符号，见 design.md §CMake 集成"经宏区分实例名"）。两份 network.c
 * 直接同链接会多重定义；本文件用宏把 MLP-Q 实例的全部 Default 符号改名为
 * "*_Mlpq" 后再包含生成源，与 ems_npu_workclf.c 的 "*_Workclf" 并存。
 *
 * 改名清单 = 生成代码的全部非 static 符号（network.c 8 个 + network_ecblobs.h
 * 2 个），与 ll_aton_NN_interface.h 的 LL_ATON_DECLARE_NAMED_NN_PROTOS 声明集
 * 一一对应；ems_infer_npu.c 以 LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE
 * (Mlpq) 声明实例并链接到这些改名后符号。
 *
 * 只引用 vendor 生成物原路径（Model/mlp_q/，T-502 产物只读），不复制文件。
 * Model 目录名来自工程结构，路径不含绝对路径（AGENTS.md §20.2）。
 */

/* ---- network.c 的 8 个外部符号（生成代码，禁改源，故在编译单元改名） ---- */
#define LL_ATON_Set_User_Input_Buffer_Default    LL_ATON_Set_User_Input_Buffer_Mlpq
#define LL_ATON_Get_User_Input_Buffer_Default    LL_ATON_Get_User_Input_Buffer_Mlpq
#define LL_ATON_Set_User_Output_Buffer_Default   LL_ATON_Set_User_Output_Buffer_Mlpq
#define LL_ATON_Get_User_Output_Buffer_Default   LL_ATON_Get_User_Output_Buffer_Mlpq
#define LL_ATON_EpochBlockItems_Default          LL_ATON_EpochBlockItems_Mlpq
#define LL_ATON_Input_Buffers_Info_Default       LL_ATON_Input_Buffers_Info_Mlpq
#define LL_ATON_Output_Buffers_Info_Default      LL_ATON_Output_Buffers_Info_Mlpq
#define LL_ATON_Internal_Buffers_Info_Default    LL_ATON_Internal_Buffers_Info_Mlpq

/* ---- network_ecblobs.h 的 2 个外部符号 ---- */
#define LL_ATON_EC_Network_Init_Default          LL_ATON_EC_Network_Init_Mlpq
#define LL_ATON_EC_Inference_Init_Default        LL_ATON_EC_Inference_Init_Mlpq

#include "../../Model/mlp_q/network.c"
