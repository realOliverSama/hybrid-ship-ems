/**
  ******************************************************************************
  * @file    n1g7.h
  * @date    2026-09-25T02:22:37+0800
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
#ifndef STAI_N1G7_DETAILS_H
#define STAI_N1G7_DETAILS_H

#include "stai.h"
#include "layers.h"

const stai_network_details g_n1g7_details = {
  .tensors = (const stai_tensor[8]) {
   { .size_bytes = 28800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {1, (const float[1]){0.10051041841506958}}, .zeropoint = {1, (const int16_t[1]){-96}}, .name = "reatt_n1_1_past_add5_1_Add0_output" },
   { .size_bytes = 300, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 300, 1, 1}}, .scale = {1, (const float[1]){0.4802679419517517}}, .zeropoint = {1, (const int16_t[1]){6}}, .name = "reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 300, 1, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n1_1_past_pool_1_Softmax0_output" },
   { .size_bytes = 300, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {1, (const float[1]){0.003921568859368563}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion_output" },
   { .size_bytes = 28800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {1, (const float[1]){0.06137390807271004}}, .zeropoint = {1, (const int16_t[1]){-99}}, .name = "reatt_n1_1_past_pool_1_mul0_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion_output" },
   { .size_bytes = 384, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 1, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "past_pool_output" }
  },
  .nodes = (const stai_node_details[7]){
    {.id = 7, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){1}} }, /* reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0 */
    {.id = 7, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){1}}, .output_tensors = {1, (const int32_t[1]){2}} }, /* reatt_n1_1_past_pool_1_dense_1_1_Tensordot_MatMul0_0_0_reatt_n1_1_past_pool_1_Softmax0_conversion */
    {.id = 13, .type = AI_LAYER_SM_TYPE, .input_tensors = {1, (const int32_t[1]){2}}, .output_tensors = {1, (const int32_t[1]){3}} }, /* reatt_n1_1_past_pool_1_Softmax0 */
    {.id = 13, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){3}}, .output_tensors = {1, (const int32_t[1]){4}} }, /* reatt_n1_1_past_pool_1_Softmax0_0_1_reatt_n1_1_past_pool_1_mul0_conversion */
    {.id = 19, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){0, 4}}, .output_tensors = {1, (const int32_t[1]){5}} }, /* reatt_n1_1_past_pool_1_mul0 */
    {.id = 19, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){5}}, .output_tensors = {1, (const int32_t[1]){6}} }, /* reatt_n1_1_past_pool_1_mul0_0_0_past_pool_conversion */
    {.id = 22, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){6}}, .output_tensors = {1, (const int32_t[1]){7}} } /* past_pool */
  },
  .n_nodes = 7
};
#endif

