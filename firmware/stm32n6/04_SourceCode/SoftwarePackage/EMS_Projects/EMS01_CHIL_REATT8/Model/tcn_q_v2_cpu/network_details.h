/**
  ******************************************************************************
  * @file    network.h
  * @date    2026-09-03T10:20:49+0800
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
#ifndef STAI_NETWORK_DETAILS_H
#define STAI_NETWORK_DETAILS_H

#include "stai.h"
#include "layers.h"

const stai_network_details g_network_details = {
  .tensors = (const stai_tensor[70]) {
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "serving_default_P_window_1x300x10_output" },
   { .size_bytes = 8, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {2, (const int32_t[2]){1, 2}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "serving_default_V_scale_20_output" },
   { .size_bytes = 2, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {2, (const int32_t[2]){1, 2}}, .scale = {1, (const float[1]){0.05349510535597801}}, .zeropoint = {1, (const int16_t[1]){-19}}, .name = "conversion_1_output" },
   { .size_bytes = 8, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {2, (const int32_t[2]){1, 8}}, .scale = {1, (const float[1]){0.027194013819098473}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "gemm_43_output" },
   { .size_bytes = 300, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 1}}, .scale = {1, (const float[1]){0.012579751200973988}}, .zeropoint = {1, (const int16_t[1]){-22}}, .name = "conversion_0_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.03341173753142357}}, .zeropoint = {1, (const int16_t[1]){-107}}, .name = "conv2d_6_output" },
   { .size_bytes = 302, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 302, 1}}, .scale = {1, (const float[1]){0.012579751200973988}}, .zeropoint = {1, (const int16_t[1]){-22}}, .name = "pad_2_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.026113374158740044}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_3_output" },
   { .size_bytes = 19328, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 302, 64}}, .scale = {1, (const float[1]){0.026113374158740044}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_4_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.031269706785678864}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_5_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.030721191316843033}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "eltwise_7_output" },
   { .size_bytes = 19456, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 304, 64}}, .scale = {1, (const float[1]){0.030721191316843033}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_8_output" },
   { .size_bytes = 77824, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 304, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_8_0_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_9_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.02891252376139164}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_9_0_0_pad_10_conversion_output" },
   { .size_bytes = 19456, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 304, 64}}, .scale = {1, (const float[1]){0.02891252376139164}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_10_output" },
   { .size_bytes = 77824, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 304, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_10_0_0_conv2d_11_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_11_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.04149854928255081}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_11_0_1_eltwise_12_conversion_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.04149854928255081}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "eltwise_12_output" },
   { .size_bytes = 19712, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 308, 64}}, .scale = {1, (const float[1]){0.04149854928255081}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_13_output" },
   { .size_bytes = 78848, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 308, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_13_0_0_conv2d_14_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_14_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.040445659309625626}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_14_0_0_pad_15_conversion_output" },
   { .size_bytes = 19712, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 308, 64}}, .scale = {1, (const float[1]){0.040445659309625626}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_15_output" },
   { .size_bytes = 78848, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 308, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_15_0_0_conv2d_16_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_16_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.04149854928255081}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_16_0_1_eltwise_17_conversion_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.04149854928255081}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "eltwise_17_output" },
   { .size_bytes = 20224, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 316, 64}}, .scale = {1, (const float[1]){0.04149854928255081}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_18_output" },
   { .size_bytes = 80896, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 316, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_18_0_0_conv2d_19_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_19_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.0626107007265091}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_19_0_0_pad_20_conversion_output" },
   { .size_bytes = 20224, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 316, 64}}, .scale = {1, (const float[1]){0.0626107007265091}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_20_output" },
   { .size_bytes = 80896, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 316, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_20_0_0_conv2d_21_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_21_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.062476057559251785}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_21_0_1_eltwise_22_conversion_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.062476057559251785}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "eltwise_22_output" },
   { .size_bytes = 21248, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 332, 64}}, .scale = {1, (const float[1]){0.062476057559251785}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_23_output" },
   { .size_bytes = 84992, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 332, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_23_0_0_conv2d_24_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_24_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.05941449850797653}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_24_0_0_pad_25_conversion_output" },
   { .size_bytes = 21248, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 332, 64}}, .scale = {1, (const float[1]){0.05941449850797653}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_25_output" },
   { .size_bytes = 84992, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 332, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_25_0_0_conv2d_26_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_26_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.06406153738498688}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_26_0_1_eltwise_27_conversion_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.06406153738498688}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "eltwise_27_output" },
   { .size_bytes = 23296, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {1, (const float[1]){0.06406153738498688}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_28_output" },
   { .size_bytes = 93184, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_28_0_0_conv2d_29_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_29_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.08398376405239105}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_29_0_0_pad_30_conversion_output" },
   { .size_bytes = 23296, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {1, (const float[1]){0.08398376405239105}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_30_output" },
   { .size_bytes = 93184, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_30_0_0_conv2d_31_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_31_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.07169536501169205}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_31_0_1_eltwise_32_conversion_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.07169536501169205}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "eltwise_32_output" },
   { .size_bytes = 23296, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {1, (const float[1]){0.07169536501169205}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_33_output" },
   { .size_bytes = 93184, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_33_0_0_conv2d_34_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_34_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.11800151318311691}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_34_0_0_pad_35_conversion_output" },
   { .size_bytes = 23296, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {1, (const float[1]){0.11800151318311691}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "pad_35_output" },
   { .size_bytes = 93184, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 364, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_35_0_0_conv2d_36_conversion_output" },
   { .size_bytes = 76800, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_36_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.07169536501169205}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "conv2d_36_0_1_eltwise_37_conversion_output" },
   { .size_bytes = 19200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {4, (const int32_t[4]){1, 1, 300, 64}}, .scale = {1, (const float[1]){0.027194013819098473}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "eltwise_37_output" },
   { .size_bytes = 64, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {3, (const int32_t[3]){1, 1, 64}}, .scale = {1, (const float[1]){0.027194013819098473}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "slice_42_gather_0_output" },
   { .size_bytes = 72, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {2, (const int32_t[2]){1, 72}}, .scale = {1, (const float[1]){0.027194013819098473}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "concat_44_output" },
   { .size_bytes = 64, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {2, (const int32_t[2]){1, 64}}, .scale = {1, (const float[1]){0.019172484055161476}}, .zeropoint = {1, (const int16_t[1]){-128}}, .name = "gemm_45_output" },
   { .size_bytes = 3, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_S8, .shape = {2, (const int32_t[2]){1, 3}}, .scale = {1, (const float[1]){0.016138950362801552}}, .zeropoint = {1, (const int16_t[1]){-28}}, .name = "gemm_46_output" },
   { .size_bytes = 12, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {2, (const int32_t[2]){1, 3}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conversion_47_output" }
  },
  .nodes = (const stai_node_details[68]){
    {.id = 1, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){1}}, .output_tensors = {1, (const int32_t[1]){2}} }, /* conversion_1 */
    {.id = 43, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){2}}, .output_tensors = {1, (const int32_t[1]){3}} }, /* gemm_43 */
    {.id = 0, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){4}} }, /* conversion_0 */
    {.id = 6, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){4}}, .output_tensors = {1, (const int32_t[1]){5}} }, /* conv2d_6 */
    {.id = 2, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){4}}, .output_tensors = {1, (const int32_t[1]){6}} }, /* pad_2 */
    {.id = 3, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){6}}, .output_tensors = {1, (const int32_t[1]){7}} }, /* conv2d_3 */
    {.id = 4, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){7}}, .output_tensors = {1, (const int32_t[1]){8}} }, /* pad_4 */
    {.id = 5, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){8}}, .output_tensors = {1, (const int32_t[1]){9}} }, /* conv2d_5 */
    {.id = 7, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){5, 9}}, .output_tensors = {1, (const int32_t[1]){10}} }, /* eltwise_7 */
    {.id = 8, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){10}}, .output_tensors = {1, (const int32_t[1]){11}} }, /* pad_8 */
    {.id = 8, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){11}}, .output_tensors = {1, (const int32_t[1]){12}} }, /* pad_8_0_conversion */
    {.id = 9, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){12}}, .output_tensors = {1, (const int32_t[1]){13}} }, /* conv2d_9 */
    {.id = 9, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){13}}, .output_tensors = {1, (const int32_t[1]){14}} }, /* conv2d_9_0_0_pad_10_conversion */
    {.id = 10, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){14}}, .output_tensors = {1, (const int32_t[1]){15}} }, /* pad_10 */
    {.id = 10, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){15}}, .output_tensors = {1, (const int32_t[1]){16}} }, /* pad_10_0_0_conv2d_11_conversion */
    {.id = 11, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){16}}, .output_tensors = {1, (const int32_t[1]){17}} }, /* conv2d_11 */
    {.id = 11, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){17}}, .output_tensors = {1, (const int32_t[1]){18}} }, /* conv2d_11_0_1_eltwise_12_conversion */
    {.id = 12, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){10, 18}}, .output_tensors = {1, (const int32_t[1]){19}} }, /* eltwise_12 */
    {.id = 13, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){19}}, .output_tensors = {1, (const int32_t[1]){20}} }, /* pad_13 */
    {.id = 13, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){20}}, .output_tensors = {1, (const int32_t[1]){21}} }, /* pad_13_0_0_conv2d_14_conversion */
    {.id = 14, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){21}}, .output_tensors = {1, (const int32_t[1]){22}} }, /* conv2d_14 */
    {.id = 14, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){22}}, .output_tensors = {1, (const int32_t[1]){23}} }, /* conv2d_14_0_0_pad_15_conversion */
    {.id = 15, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){23}}, .output_tensors = {1, (const int32_t[1]){24}} }, /* pad_15 */
    {.id = 15, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){24}}, .output_tensors = {1, (const int32_t[1]){25}} }, /* pad_15_0_0_conv2d_16_conversion */
    {.id = 16, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){25}}, .output_tensors = {1, (const int32_t[1]){26}} }, /* conv2d_16 */
    {.id = 16, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){26}}, .output_tensors = {1, (const int32_t[1]){27}} }, /* conv2d_16_0_1_eltwise_17_conversion */
    {.id = 17, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){19, 27}}, .output_tensors = {1, (const int32_t[1]){28}} }, /* eltwise_17 */
    {.id = 18, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){28}}, .output_tensors = {1, (const int32_t[1]){29}} }, /* pad_18 */
    {.id = 18, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){29}}, .output_tensors = {1, (const int32_t[1]){30}} }, /* pad_18_0_0_conv2d_19_conversion */
    {.id = 19, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){30}}, .output_tensors = {1, (const int32_t[1]){31}} }, /* conv2d_19 */
    {.id = 19, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){31}}, .output_tensors = {1, (const int32_t[1]){32}} }, /* conv2d_19_0_0_pad_20_conversion */
    {.id = 20, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){32}}, .output_tensors = {1, (const int32_t[1]){33}} }, /* pad_20 */
    {.id = 20, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){33}}, .output_tensors = {1, (const int32_t[1]){34}} }, /* pad_20_0_0_conv2d_21_conversion */
    {.id = 21, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){34}}, .output_tensors = {1, (const int32_t[1]){35}} }, /* conv2d_21 */
    {.id = 21, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){35}}, .output_tensors = {1, (const int32_t[1]){36}} }, /* conv2d_21_0_1_eltwise_22_conversion */
    {.id = 22, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){28, 36}}, .output_tensors = {1, (const int32_t[1]){37}} }, /* eltwise_22 */
    {.id = 23, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){37}}, .output_tensors = {1, (const int32_t[1]){38}} }, /* pad_23 */
    {.id = 23, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){38}}, .output_tensors = {1, (const int32_t[1]){39}} }, /* pad_23_0_0_conv2d_24_conversion */
    {.id = 24, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){39}}, .output_tensors = {1, (const int32_t[1]){40}} }, /* conv2d_24 */
    {.id = 24, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){40}}, .output_tensors = {1, (const int32_t[1]){41}} }, /* conv2d_24_0_0_pad_25_conversion */
    {.id = 25, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){41}}, .output_tensors = {1, (const int32_t[1]){42}} }, /* pad_25 */
    {.id = 25, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){42}}, .output_tensors = {1, (const int32_t[1]){43}} }, /* pad_25_0_0_conv2d_26_conversion */
    {.id = 26, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){43}}, .output_tensors = {1, (const int32_t[1]){44}} }, /* conv2d_26 */
    {.id = 26, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){44}}, .output_tensors = {1, (const int32_t[1]){45}} }, /* conv2d_26_0_1_eltwise_27_conversion */
    {.id = 27, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){37, 45}}, .output_tensors = {1, (const int32_t[1]){46}} }, /* eltwise_27 */
    {.id = 28, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){46}}, .output_tensors = {1, (const int32_t[1]){47}} }, /* pad_28 */
    {.id = 28, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){47}}, .output_tensors = {1, (const int32_t[1]){48}} }, /* pad_28_0_0_conv2d_29_conversion */
    {.id = 29, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){48}}, .output_tensors = {1, (const int32_t[1]){49}} }, /* conv2d_29 */
    {.id = 29, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){49}}, .output_tensors = {1, (const int32_t[1]){50}} }, /* conv2d_29_0_0_pad_30_conversion */
    {.id = 30, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){50}}, .output_tensors = {1, (const int32_t[1]){51}} }, /* pad_30 */
    {.id = 30, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){51}}, .output_tensors = {1, (const int32_t[1]){52}} }, /* pad_30_0_0_conv2d_31_conversion */
    {.id = 31, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){52}}, .output_tensors = {1, (const int32_t[1]){53}} }, /* conv2d_31 */
    {.id = 31, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){53}}, .output_tensors = {1, (const int32_t[1]){54}} }, /* conv2d_31_0_1_eltwise_32_conversion */
    {.id = 32, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){46, 54}}, .output_tensors = {1, (const int32_t[1]){55}} }, /* eltwise_32 */
    {.id = 33, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){55}}, .output_tensors = {1, (const int32_t[1]){56}} }, /* pad_33 */
    {.id = 33, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){56}}, .output_tensors = {1, (const int32_t[1]){57}} }, /* pad_33_0_0_conv2d_34_conversion */
    {.id = 34, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){57}}, .output_tensors = {1, (const int32_t[1]){58}} }, /* conv2d_34 */
    {.id = 34, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){58}}, .output_tensors = {1, (const int32_t[1]){59}} }, /* conv2d_34_0_0_pad_35_conversion */
    {.id = 35, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){59}}, .output_tensors = {1, (const int32_t[1]){60}} }, /* pad_35 */
    {.id = 35, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){60}}, .output_tensors = {1, (const int32_t[1]){61}} }, /* pad_35_0_0_conv2d_36_conversion */
    {.id = 36, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){61}}, .output_tensors = {1, (const int32_t[1]){62}} }, /* conv2d_36 */
    {.id = 36, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){62}}, .output_tensors = {1, (const int32_t[1]){63}} }, /* conv2d_36_0_1_eltwise_37_conversion */
    {.id = 37, .type = AI_LAYER_ELTWISE_INTEGER_TYPE, .input_tensors = {2, (const int32_t[2]){55, 63}}, .output_tensors = {1, (const int32_t[1]){64}} }, /* eltwise_37 */
    {.id = 42, .type = AI_LAYER_GATHER_TYPE, .input_tensors = {1, (const int32_t[1]){64}}, .output_tensors = {1, (const int32_t[1]){65}} }, /* slice_42_gather_0 */
    {.id = 44, .type = AI_LAYER_CONCAT_TYPE, .input_tensors = {2, (const int32_t[2]){65, 3}}, .output_tensors = {1, (const int32_t[1]){66}} }, /* concat_44 */
    {.id = 45, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){66}}, .output_tensors = {1, (const int32_t[1]){67}} }, /* gemm_45 */
    {.id = 46, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){67}}, .output_tensors = {1, (const int32_t[1]){68}} }, /* gemm_46 */
    {.id = 47, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){68}}, .output_tensors = {1, (const int32_t[1]){69}} } /* conversion_47 */
  },
  .n_nodes = 68
};
#endif

