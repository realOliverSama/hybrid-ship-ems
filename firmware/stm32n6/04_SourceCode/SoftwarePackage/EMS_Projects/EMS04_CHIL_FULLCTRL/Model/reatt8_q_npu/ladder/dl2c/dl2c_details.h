/**
  ******************************************************************************
  * @file    dl2c.h
  * @date    2026-09-23T02:04:52+0800
  * @brief   ST.AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef STAI_DL2C_DETAILS_H
#define STAI_DL2C_DETAILS_H

#include "stai.h"
#include "layers.h"

const stai_network_details g_dl2c_details = {
  .tensors = (const stai_tensor[25]) {
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {1, (const float[1]){0.028821656480431557}}, .zeropoint = {1, (const int16_t[1]){5}}, .name = "reatt_n2_1_fut0_ln1_1_add_20_DequantizeLinear_Output_output" },
   { .size_bytes = 4, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {2, (const int32_t[2]){1, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion_output" },
   { .size_bytes = 4, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {2, (const int32_t[2]){1, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 30, 1, 128}}, .scale = {1, (const float[1]){0.08840194344520569}}, .zeropoint = {1, (const int16_t[1]){-3}}, .name = "reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {1, (const float[1]){0.08840463310480118}}, .zeropoint = {1, (const int16_t[1]){-3}}, .name = "reatt_n2_1_fut0_mha_1_k_1_BiasAdd0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 4, 32, 30}}, .scale = {1, (const float[1]){0.08840463310480118}}, .zeropoint = {1, (const int16_t[1]){-3}}, .name = "reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 32, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 30, 1, 128}}, .scale = {1, (const float[1]){0.09217384457588196}}, .zeropoint = {1, (const int16_t[1]){-14}}, .name = "reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {1, (const float[1]){0.09258094429969788}}, .zeropoint = {1, (const int16_t[1]){-15}}, .name = "reatt_n2_1_fut0_mha_1_q_1_BiasAdd0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {1, (const float[1]){0.09258094429969788}}, .zeropoint = {1, (const int16_t[1]){-15}}, .name = "reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_MatMul0_output" },
   { .size_bytes = 3600, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {1, (const float[1]){4.831122875213623}}, .zeropoint = {1, (const int16_t[1]){-26}}, .name = "reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion_output" },
   { .size_bytes = 3600, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {1, (const float[1]){0.8540298938751221}}, .zeropoint = {1, (const int16_t[1]){-26}}, .name = "reatt_n2_1_fut0_mha_1_mul0_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_Softmax0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 30, 1, 128}}, .scale = {1, (const float[1]){0.06436857581138611}}, .zeropoint = {1, (const int16_t[1]){7}}, .name = "reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {1, (const float[1]){0.06434761732816696}}, .zeropoint = {1, (const int16_t[1]){7}}, .name = "reatt_n2_1_fut0_mha_1_v_1_BiasAdd0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {1, (const float[1]){0.06434761732816696}}, .zeropoint = {1, (const int16_t[1]){7}}, .name = "reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n2_1_fut0_mha_1_MatMul_10_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {1, (const float[1]){0.05957232788205147}}, .zeropoint = {1, (const int16_t[1]){8}}, .name = "reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {1, (const float[1]){0.05957232788205147}}, .zeropoint = {1, (const int16_t[1]){8}}, .name = "reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 30, 1, 128}}, .scale = {1, (const float[1]){0.16120177507400513}}, .zeropoint = {1, (const int16_t[1]){-14}}, .name = "reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0_output" },
   { .size_bytes = 3840, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {1, (const float[1]){0.16095532476902008}}, .zeropoint = {1, (const int16_t[1]){-14}}, .name = "reatt_n2_1_fut0_mha_1_o_1_BiasAdd0_output" }
  },
  .nodes = (const stai_node_details[24]){
    {.id = 67, .type = AI_LAYER_NL_TYPE, .input_tensors = {0, NULL}, .output_tensors = {1, (const int32_t[1]){1}} }, /* reatt_n2_1_fut0_mha_1_MatMul_10_bias_0_conversion */
    {.id = 58, .type = AI_LAYER_NL_TYPE, .input_tensors = {0, NULL}, .output_tensors = {1, (const int32_t[1]){2}} }, /* reatt_n2_1_fut0_mha_1_MatMul0_bias_0_2_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
    {.id = 15, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){3}} }, /* reatt_n2_1_fut0_mha_1_k_1_Tensordot_MatMul0 */
    {.id = 33, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {1, (const int32_t[1]){3}}, .output_tensors = {1, (const int32_t[1]){4}} }, /* reatt_n2_1_fut0_mha_1_k_1_BiasAdd0 */
    {.id = 51, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){4}}, .output_tensors = {1, (const int32_t[1]){5}} }, /* reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0 */
    {.id = 51, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){5}}, .output_tensors = {1, (const int32_t[1]){6}} }, /* reatt_n2_1_fut0_mha_1_pt_kt_1_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
    {.id = 14, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){7}} }, /* reatt_n2_1_fut0_mha_1_q_1_Tensordot_MatMul0 */
    {.id = 32, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {1, (const int32_t[1]){7}}, .output_tensors = {1, (const int32_t[1]){8}} }, /* reatt_n2_1_fut0_mha_1_q_1_BiasAdd0 */
    {.id = 50, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){8}}, .output_tensors = {1, (const int32_t[1]){9}} }, /* reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0 */
    {.id = 50, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){9}}, .output_tensors = {1, (const int32_t[1]){10}} }, /* reatt_n2_1_fut0_mha_1_pt_heads_1_transpose0_0_0_reatt_n2_1_fut0_mha_1_MatMul0_conversion */
    {.id = 58, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {3, (const int32_t[3]){10, 6, 2}}, .output_tensors = {1, (const int32_t[1]){11}} }, /* reatt_n2_1_fut0_mha_1_MatMul0 */
    {.id = 58, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){11}}, .output_tensors = {1, (const int32_t[1]){12}} }, /* reatt_n2_1_fut0_mha_1_MatMul0_0_0_reatt_n2_1_fut0_mha_1_mul0_conversion */
    {.id = 61, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {1, (const int32_t[1]){12}}, .output_tensors = {1, (const int32_t[1]){13}} }, /* reatt_n2_1_fut0_mha_1_mul0 */
    {.id = 61, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){13}}, .output_tensors = {1, (const int32_t[1]){14}} }, /* reatt_n2_1_fut0_mha_1_mul0_0_0_reatt_n2_1_fut0_mha_1_Softmax0_conversion */
    {.id = 64, .type = AI_LAYER_SM_TYPE, .input_tensors = {1, (const int32_t[1]){14}}, .output_tensors = {1, (const int32_t[1]){15}} }, /* reatt_n2_1_fut0_mha_1_Softmax0 */
    {.id = 13, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){16}} }, /* reatt_n2_1_fut0_mha_1_v_1_Tensordot_MatMul0 */
    {.id = 31, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {1, (const int32_t[1]){16}}, .output_tensors = {1, (const int32_t[1]){17}} }, /* reatt_n2_1_fut0_mha_1_v_1_BiasAdd0 */
    {.id = 49, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){17}}, .output_tensors = {1, (const int32_t[1]){18}} }, /* reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0 */
    {.id = 49, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){18}}, .output_tensors = {1, (const int32_t[1]){19}} }, /* reatt_n2_1_fut0_mha_1_pt_heads_5_transpose0_0_1_reatt_n2_1_fut0_mha_1_MatMul_10_conversion */
    {.id = 67, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {3, (const int32_t[3]){15, 19, 1}}, .output_tensors = {1, (const int32_t[1]){20}} }, /* reatt_n2_1_fut0_mha_1_MatMul_10 */
    {.id = 67, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){20}}, .output_tensors = {1, (const int32_t[1]){21}} }, /* reatt_n2_1_fut0_mha_1_MatMul_10_0_0_reatt_n2_1_fut0_mha_1_pt_back_1_transpose0_conversion */
    {.id = 70, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){21}}, .output_tensors = {1, (const int32_t[1]){22}} }, /* reatt_n2_1_fut0_mha_1_pt_back_1_transpose0 */
    {.id = 76, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){22}}, .output_tensors = {1, (const int32_t[1]){23}} }, /* reatt_n2_1_fut0_mha_1_o_1_Tensordot_MatMul0 */
    {.id = 82, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {1, (const int32_t[1]){23}}, .output_tensors = {1, (const int32_t[1]){24}} } /* reatt_n2_1_fut0_mha_1_o_1_BiasAdd0 */
  },
  .n_nodes = 24
};
#endif

