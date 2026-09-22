/* ems_reatt_params.h — REAT-T 板上归一化常数（生成物，禁手改）。
 *
 * 生成器：Host/tools/gen_reatt_params.py（phase4-g T-005；确定性幂等，重跑逐字节一致）。
 * 源（全部只读冻结件，sha256）：
 *   formal_norm_v3.json: b64210bc47a035afe9fd164c65ba63fca84aead53dc7e2dcaada79bd88f065ba
 *   predictor_norm_ext_v1.json: a1cace1fb6f6eda58ad3be76723663c6a196b98bbc4c2c34c7b537dc4c2fe92c
 *   dataset_v3_signal_registry.json: 1520ea26d253f5ddf5fd66f706ef4299b8a713fd90080d29528a7ff4895b8744
 *   reatt_raw_int8.tflite: efde1aff326caa6230cf6b8ab89ff85a75e28a04e39d5d4348c26084023f066a
 *
 * 通道序（= 模型输入堆叠序 = CMD_REATT 线上传输序）：
 *   H1: P_load_total_kW, v_ship_mps, acceleration_mps2, n_prop_rpm, P_hotel_kW, v_current_mps, water_depth_m, P_prop_demand_kW, ctrl_mode, v_ref_mps, manual_power_cmd_pct
 *   KF: v_plan_mps, s_plan_m, h_chart_m, P_hotel_plan_kW, u_fcst_mps, wl_fcst_m
 *
 * 布局：H1_MEAN/STD [ch][bin]（ch 主序，ch<11, bin<300）；
 *       KF_MEAN/SCALE [ch][lead]（ch<6, lead<30）。
 */
#ifndef EMS_REATT_PARAMS_H
#define EMS_REATT_PARAMS_H

#include <stdint.h>

#define EMS_REATT_H1_CH_N 11u   /* H1 通道数（元素计数宏在 ems_reatt.h：EMS_REATT_HIST_N） */
#define EMS_REATT_H1_T    300u  /* H1 bin 数（时间步） */
#define EMS_REATT_KF_CH_N 6u    /* KF 通道数（元素计数宏在 ems_reatt.h：EMS_REATT_KF_N） */
#define EMS_REATT_KF_T    30u   /* KF lead 数 */

extern const float EMS_REATT_H1_MEAN[EMS_REATT_H1_CH_N * EMS_REATT_H1_T];
extern const float EMS_REATT_H1_STD[EMS_REATT_H1_CH_N * EMS_REATT_H1_T];
extern const float EMS_REATT_KF_MEAN[EMS_REATT_KF_CH_N * EMS_REATT_KF_T];
extern const float EMS_REATT_KF_SCALE[EMS_REATT_KF_CH_N * EMS_REATT_KF_T];

#endif /* EMS_REATT_PARAMS_H */
