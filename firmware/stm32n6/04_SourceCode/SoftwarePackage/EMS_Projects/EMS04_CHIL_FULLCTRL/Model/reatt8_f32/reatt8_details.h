/**
  ******************************************************************************
  * @file    reatt8.h
  * @date    2026-09-29T21:44:28+0800
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
#ifndef STAI_REATT8_DETAILS_H
#define STAI_REATT8_DETAILS_H

#include "stai.h"
#include "layers.h"

const stai_network_details g_reatt8_details = {
  .tensors = (const stai_tensor[300]) {
   { .size_bytes = 13200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 11}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "serving_default_hist_input0_output" },
   { .size_bytes = 720, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 6}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "serving_default_kf_input0_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_146_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_148_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_149_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_149_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_151_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_152_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_152_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_154_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_155_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_158_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_150_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_156_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_157_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_159_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_160_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_162_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 32, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_167_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_163_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_165_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_166_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_168_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_169_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_171_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_172_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_174_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_175_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_176_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_178_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_180_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_181_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_182_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_182_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_184_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_185_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_185_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_187_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_188_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_191_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_183_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_189_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_190_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_192_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_201_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_204_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_213_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_216_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_217_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_217_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_219_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_220_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_220_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_222_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_223_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_226_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_218_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_224_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_225_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_227_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_228_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_230_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 32, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_235_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_231_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_233_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_234_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_236_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_237_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_239_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_240_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_242_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_243_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_244_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_246_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_248_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_249_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_250_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_250_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_252_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_253_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_253_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_255_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_256_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_259_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_251_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_257_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_258_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_260_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_269_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_272_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_281_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_284_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_285_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_285_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_287_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_288_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_288_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_290_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_291_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_294_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_286_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_292_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_293_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_295_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_296_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_298_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 32, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_303_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_299_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_301_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_302_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_304_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_305_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_307_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_308_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_310_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_311_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_312_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_314_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_316_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_317_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_318_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_318_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_320_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_321_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_321_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_323_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_324_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_327_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_319_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_325_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_326_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_328_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_337_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_340_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_349_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_352_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_353_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_353_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_355_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_356_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_356_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_358_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_359_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_362_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_354_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_360_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_361_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_363_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_364_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_366_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 32, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_371_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_367_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_369_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_370_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_372_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_373_output" },
   { .size_bytes = 14400, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 30}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_375_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_376_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_378_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_379_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 4, 30, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_380_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 30, 4, 32}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "transpose_382_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_384_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_385_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_386_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_386_Mul_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_388_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_389_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_389_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_391_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_392_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_395_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_387_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_393_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_394_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_396_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_405_output" },
   { .size_bytes = 61440, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 512}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_408_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_417_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_420_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_1_output" },
   { .size_bytes = 115968, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 302, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_3_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {4, (const int32_t[4]){1, 1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_5_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_7_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_7_Mul_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_9_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_10_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_10_Mul_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_12_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_13_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_16_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_8_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_14_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_15_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_17_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_18_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_19_output" },
   { .size_bytes = 116736, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 304, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_20_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_23_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_27_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_27_Mul_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_29_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_30_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_30_Mul_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_32_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_33_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_36_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_28_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_34_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_35_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_37_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_38_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_39_output" },
   { .size_bytes = 118272, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 308, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_40_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_43_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_47_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_47_Mul_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_49_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_50_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_50_Mul_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_52_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_53_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_56_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_48_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_54_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_55_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_57_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_58_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_59_output" },
   { .size_bytes = 121344, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 316, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_60_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_63_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_67_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_67_Mul_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_69_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_70_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_70_Mul_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_72_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_73_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_76_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_68_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_74_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_75_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_77_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_78_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_79_output" },
   { .size_bytes = 127488, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 332, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_80_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_83_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_87_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_87_Mul_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_89_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_90_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_90_Mul_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_92_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_93_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_96_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_88_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_94_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_95_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_97_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_98_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_99_output" },
   { .size_bytes = 139776, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 364, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "pad_100_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "conv2d_103_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_107_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_107_Mul_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_109_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_110_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_110_Mul_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_112_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_113_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_116_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_108_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_114_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_115_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_117_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_118_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_119_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_128_output" },
   { .size_bytes = 1200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 1, 300}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_131_output" },
   { .size_bytes = 115200, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 300, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_133_output" },
   { .size_bytes = 384, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 1, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_134_output" },
   { .size_bytes = 11520, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 96}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "tile_137_output" },
   { .size_bytes = 26880, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 224}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "concat_421_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_422_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_422_Mul_output" },
   { .size_bytes = 26880, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 224}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_424_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_425_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "reduce_425_Mul_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_427_output" },
   { .size_bytes = 26880, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 224}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_428_output" },
   { .size_bytes = 26880, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 224}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_431_output" },
   { .size_bytes = 120, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 1}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_423_output" },
   { .size_bytes = 26880, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 224}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_429_output" },
   { .size_bytes = 26880, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 224}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_430_output" },
   { .size_bytes = 26880, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 224}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "eltwise_432_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_441_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_444_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_453_output" },
   { .size_bytes = 15360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 128}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "nl_456_output" },
   { .size_bytes = 360, .flags = (STAI_FLAG_HAS_BATCH|STAI_FLAG_CHANNEL_LAST), .format = STAI_FORMAT_FLOAT32, .shape = {3, (const int32_t[3]){1, 30, 3}}, .scale = {0, NULL}, .zeropoint = {0, NULL}, .name = "gemm_465_output" }
  },
  .nodes = (const stai_node_details[298]){
    {.id = 146, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){1}}, .output_tensors = {1, (const int32_t[1]){2}} }, /* gemm_146 */
    {.id = 148, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){2}}, .output_tensors = {1, (const int32_t[1]){3}} }, /* eltwise_148 */
    {.id = 149, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){3}}, .output_tensors = {1, (const int32_t[1]){4}} }, /* reduce_149 */
    {.id = 149, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){4}}, .output_tensors = {1, (const int32_t[1]){5}} }, /* reduce_149_Mul */
    {.id = 151, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){3, 5}}, .output_tensors = {1, (const int32_t[1]){6}} }, /* eltwise_151 */
    {.id = 153, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){6}}, .output_tensors = {1, (const int32_t[1]){7}} }, /* reduce_152 */
    {.id = 153, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){7}}, .output_tensors = {1, (const int32_t[1]){8}} }, /* reduce_152_Mul */
    {.id = 154, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){8}}, .output_tensors = {1, (const int32_t[1]){9}} }, /* nl_154 */
    {.id = 155, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){9}}, .output_tensors = {1, (const int32_t[1]){10}} }, /* eltwise_155 */
    {.id = 158, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){3, 10}}, .output_tensors = {1, (const int32_t[1]){11}} }, /* eltwise_158 */
    {.id = 150, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){5}}, .output_tensors = {1, (const int32_t[1]){12}} }, /* nl_150 */
    {.id = 156, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){12, 10}}, .output_tensors = {1, (const int32_t[1]){13}} }, /* eltwise_156 */
    {.id = 157, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){13}}, .output_tensors = {1, (const int32_t[1]){14}} }, /* eltwise_157 */
    {.id = 159, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){11, 14}}, .output_tensors = {1, (const int32_t[1]){15}} }, /* eltwise_159 */
    {.id = 160, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){15}}, .output_tensors = {1, (const int32_t[1]){16}} }, /* gemm_160 */
    {.id = 162, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){16}}, .output_tensors = {1, (const int32_t[1]){17}} }, /* eltwise_162 */
    {.id = 167, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){17}}, .output_tensors = {1, (const int32_t[1]){18}} }, /* transpose_167 */
    {.id = 163, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){15}}, .output_tensors = {1, (const int32_t[1]){19}} }, /* gemm_163 */
    {.id = 165, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){19}}, .output_tensors = {1, (const int32_t[1]){20}} }, /* eltwise_165 */
    {.id = 166, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){20}}, .output_tensors = {1, (const int32_t[1]){21}} }, /* transpose_166 */
    {.id = 168, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){21, 18}}, .output_tensors = {1, (const int32_t[1]){22}} }, /* gemm_168 */
    {.id = 169, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){22}}, .output_tensors = {1, (const int32_t[1]){23}} }, /* eltwise_169 */
    {.id = 171, .type = AI_LAYER_SM_TYPE, .input_tensors = {1, (const int32_t[1]){23}}, .output_tensors = {1, (const int32_t[1]){24}} }, /* nl_171 */
    {.id = 172, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){15}}, .output_tensors = {1, (const int32_t[1]){25}} }, /* gemm_172 */
    {.id = 174, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){25}}, .output_tensors = {1, (const int32_t[1]){26}} }, /* eltwise_174 */
    {.id = 175, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){26}}, .output_tensors = {1, (const int32_t[1]){27}} }, /* transpose_175 */
    {.id = 176, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){24, 27}}, .output_tensors = {1, (const int32_t[1]){28}} }, /* gemm_176 */
    {.id = 178, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){28}}, .output_tensors = {1, (const int32_t[1]){29}} }, /* transpose_178 */
    {.id = 180, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){29}}, .output_tensors = {1, (const int32_t[1]){30}} }, /* gemm_180 */
    {.id = 181, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){3, 30}}, .output_tensors = {1, (const int32_t[1]){31}} }, /* eltwise_181 */
    {.id = 182, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){31}}, .output_tensors = {1, (const int32_t[1]){32}} }, /* reduce_182 */
    {.id = 182, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){32}}, .output_tensors = {1, (const int32_t[1]){33}} }, /* reduce_182_Mul */
    {.id = 184, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){31, 33}}, .output_tensors = {1, (const int32_t[1]){34}} }, /* eltwise_184 */
    {.id = 186, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){34}}, .output_tensors = {1, (const int32_t[1]){35}} }, /* reduce_185 */
    {.id = 186, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){35}}, .output_tensors = {1, (const int32_t[1]){36}} }, /* reduce_185_Mul */
    {.id = 187, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){36}}, .output_tensors = {1, (const int32_t[1]){37}} }, /* nl_187 */
    {.id = 188, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){37}}, .output_tensors = {1, (const int32_t[1]){38}} }, /* eltwise_188 */
    {.id = 191, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){31, 38}}, .output_tensors = {1, (const int32_t[1]){39}} }, /* eltwise_191 */
    {.id = 183, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){33}}, .output_tensors = {1, (const int32_t[1]){40}} }, /* nl_183 */
    {.id = 189, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){40, 38}}, .output_tensors = {1, (const int32_t[1]){41}} }, /* eltwise_189 */
    {.id = 190, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){41}}, .output_tensors = {1, (const int32_t[1]){42}} }, /* eltwise_190 */
    {.id = 192, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){39, 42}}, .output_tensors = {1, (const int32_t[1]){43}} }, /* eltwise_192 */
    {.id = 203, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){43}}, .output_tensors = {1, (const int32_t[1]){44}} }, /* gemm_201 */
    {.id = 204, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){44}}, .output_tensors = {1, (const int32_t[1]){45}} }, /* nl_204 */
    {.id = 215, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){45}}, .output_tensors = {1, (const int32_t[1]){46}} }, /* gemm_213 */
    {.id = 216, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){31, 46}}, .output_tensors = {1, (const int32_t[1]){47}} }, /* eltwise_216 */
    {.id = 217, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){47}}, .output_tensors = {1, (const int32_t[1]){48}} }, /* reduce_217 */
    {.id = 217, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){48}}, .output_tensors = {1, (const int32_t[1]){49}} }, /* reduce_217_Mul */
    {.id = 219, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){47, 49}}, .output_tensors = {1, (const int32_t[1]){50}} }, /* eltwise_219 */
    {.id = 221, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){50}}, .output_tensors = {1, (const int32_t[1]){51}} }, /* reduce_220 */
    {.id = 221, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){51}}, .output_tensors = {1, (const int32_t[1]){52}} }, /* reduce_220_Mul */
    {.id = 222, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){52}}, .output_tensors = {1, (const int32_t[1]){53}} }, /* nl_222 */
    {.id = 223, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){53}}, .output_tensors = {1, (const int32_t[1]){54}} }, /* eltwise_223 */
    {.id = 226, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){47, 54}}, .output_tensors = {1, (const int32_t[1]){55}} }, /* eltwise_226 */
    {.id = 218, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){49}}, .output_tensors = {1, (const int32_t[1]){56}} }, /* nl_218 */
    {.id = 224, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){56, 54}}, .output_tensors = {1, (const int32_t[1]){57}} }, /* eltwise_224 */
    {.id = 225, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){57}}, .output_tensors = {1, (const int32_t[1]){58}} }, /* eltwise_225 */
    {.id = 227, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){55, 58}}, .output_tensors = {1, (const int32_t[1]){59}} }, /* eltwise_227 */
    {.id = 228, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){59}}, .output_tensors = {1, (const int32_t[1]){60}} }, /* gemm_228 */
    {.id = 230, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){60}}, .output_tensors = {1, (const int32_t[1]){61}} }, /* eltwise_230 */
    {.id = 235, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){61}}, .output_tensors = {1, (const int32_t[1]){62}} }, /* transpose_235 */
    {.id = 231, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){59}}, .output_tensors = {1, (const int32_t[1]){63}} }, /* gemm_231 */
    {.id = 233, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){63}}, .output_tensors = {1, (const int32_t[1]){64}} }, /* eltwise_233 */
    {.id = 234, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){64}}, .output_tensors = {1, (const int32_t[1]){65}} }, /* transpose_234 */
    {.id = 236, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){65, 62}}, .output_tensors = {1, (const int32_t[1]){66}} }, /* gemm_236 */
    {.id = 237, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){66}}, .output_tensors = {1, (const int32_t[1]){67}} }, /* eltwise_237 */
    {.id = 239, .type = AI_LAYER_SM_TYPE, .input_tensors = {1, (const int32_t[1]){67}}, .output_tensors = {1, (const int32_t[1]){68}} }, /* nl_239 */
    {.id = 240, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){59}}, .output_tensors = {1, (const int32_t[1]){69}} }, /* gemm_240 */
    {.id = 242, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){69}}, .output_tensors = {1, (const int32_t[1]){70}} }, /* eltwise_242 */
    {.id = 243, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){70}}, .output_tensors = {1, (const int32_t[1]){71}} }, /* transpose_243 */
    {.id = 244, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){68, 71}}, .output_tensors = {1, (const int32_t[1]){72}} }, /* gemm_244 */
    {.id = 246, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){72}}, .output_tensors = {1, (const int32_t[1]){73}} }, /* transpose_246 */
    {.id = 248, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){73}}, .output_tensors = {1, (const int32_t[1]){74}} }, /* gemm_248 */
    {.id = 249, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){47, 74}}, .output_tensors = {1, (const int32_t[1]){75}} }, /* eltwise_249 */
    {.id = 250, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){75}}, .output_tensors = {1, (const int32_t[1]){76}} }, /* reduce_250 */
    {.id = 250, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){76}}, .output_tensors = {1, (const int32_t[1]){77}} }, /* reduce_250_Mul */
    {.id = 252, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){75, 77}}, .output_tensors = {1, (const int32_t[1]){78}} }, /* eltwise_252 */
    {.id = 254, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){78}}, .output_tensors = {1, (const int32_t[1]){79}} }, /* reduce_253 */
    {.id = 254, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){79}}, .output_tensors = {1, (const int32_t[1]){80}} }, /* reduce_253_Mul */
    {.id = 255, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){80}}, .output_tensors = {1, (const int32_t[1]){81}} }, /* nl_255 */
    {.id = 256, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){81}}, .output_tensors = {1, (const int32_t[1]){82}} }, /* eltwise_256 */
    {.id = 259, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){75, 82}}, .output_tensors = {1, (const int32_t[1]){83}} }, /* eltwise_259 */
    {.id = 251, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){77}}, .output_tensors = {1, (const int32_t[1]){84}} }, /* nl_251 */
    {.id = 257, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){84, 82}}, .output_tensors = {1, (const int32_t[1]){85}} }, /* eltwise_257 */
    {.id = 258, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){85}}, .output_tensors = {1, (const int32_t[1]){86}} }, /* eltwise_258 */
    {.id = 260, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){83, 86}}, .output_tensors = {1, (const int32_t[1]){87}} }, /* eltwise_260 */
    {.id = 271, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){87}}, .output_tensors = {1, (const int32_t[1]){88}} }, /* gemm_269 */
    {.id = 272, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){88}}, .output_tensors = {1, (const int32_t[1]){89}} }, /* nl_272 */
    {.id = 283, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){89}}, .output_tensors = {1, (const int32_t[1]){90}} }, /* gemm_281 */
    {.id = 284, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){75, 90}}, .output_tensors = {1, (const int32_t[1]){91}} }, /* eltwise_284 */
    {.id = 285, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){91}}, .output_tensors = {1, (const int32_t[1]){92}} }, /* reduce_285 */
    {.id = 285, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){92}}, .output_tensors = {1, (const int32_t[1]){93}} }, /* reduce_285_Mul */
    {.id = 287, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){91, 93}}, .output_tensors = {1, (const int32_t[1]){94}} }, /* eltwise_287 */
    {.id = 289, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){94}}, .output_tensors = {1, (const int32_t[1]){95}} }, /* reduce_288 */
    {.id = 289, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){95}}, .output_tensors = {1, (const int32_t[1]){96}} }, /* reduce_288_Mul */
    {.id = 290, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){96}}, .output_tensors = {1, (const int32_t[1]){97}} }, /* nl_290 */
    {.id = 291, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){97}}, .output_tensors = {1, (const int32_t[1]){98}} }, /* eltwise_291 */
    {.id = 294, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){91, 98}}, .output_tensors = {1, (const int32_t[1]){99}} }, /* eltwise_294 */
    {.id = 286, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){93}}, .output_tensors = {1, (const int32_t[1]){100}} }, /* nl_286 */
    {.id = 292, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){100, 98}}, .output_tensors = {1, (const int32_t[1]){101}} }, /* eltwise_292 */
    {.id = 293, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){101}}, .output_tensors = {1, (const int32_t[1]){102}} }, /* eltwise_293 */
    {.id = 295, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){99, 102}}, .output_tensors = {1, (const int32_t[1]){103}} }, /* eltwise_295 */
    {.id = 296, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){103}}, .output_tensors = {1, (const int32_t[1]){104}} }, /* gemm_296 */
    {.id = 298, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){104}}, .output_tensors = {1, (const int32_t[1]){105}} }, /* eltwise_298 */
    {.id = 303, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){105}}, .output_tensors = {1, (const int32_t[1]){106}} }, /* transpose_303 */
    {.id = 299, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){103}}, .output_tensors = {1, (const int32_t[1]){107}} }, /* gemm_299 */
    {.id = 301, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){107}}, .output_tensors = {1, (const int32_t[1]){108}} }, /* eltwise_301 */
    {.id = 302, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){108}}, .output_tensors = {1, (const int32_t[1]){109}} }, /* transpose_302 */
    {.id = 304, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){109, 106}}, .output_tensors = {1, (const int32_t[1]){110}} }, /* gemm_304 */
    {.id = 305, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){110}}, .output_tensors = {1, (const int32_t[1]){111}} }, /* eltwise_305 */
    {.id = 307, .type = AI_LAYER_SM_TYPE, .input_tensors = {1, (const int32_t[1]){111}}, .output_tensors = {1, (const int32_t[1]){112}} }, /* nl_307 */
    {.id = 308, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){103}}, .output_tensors = {1, (const int32_t[1]){113}} }, /* gemm_308 */
    {.id = 310, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){113}}, .output_tensors = {1, (const int32_t[1]){114}} }, /* eltwise_310 */
    {.id = 311, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){114}}, .output_tensors = {1, (const int32_t[1]){115}} }, /* transpose_311 */
    {.id = 312, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){112, 115}}, .output_tensors = {1, (const int32_t[1]){116}} }, /* gemm_312 */
    {.id = 314, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){116}}, .output_tensors = {1, (const int32_t[1]){117}} }, /* transpose_314 */
    {.id = 316, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){117}}, .output_tensors = {1, (const int32_t[1]){118}} }, /* gemm_316 */
    {.id = 317, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){91, 118}}, .output_tensors = {1, (const int32_t[1]){119}} }, /* eltwise_317 */
    {.id = 318, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){119}}, .output_tensors = {1, (const int32_t[1]){120}} }, /* reduce_318 */
    {.id = 318, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){120}}, .output_tensors = {1, (const int32_t[1]){121}} }, /* reduce_318_Mul */
    {.id = 320, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){119, 121}}, .output_tensors = {1, (const int32_t[1]){122}} }, /* eltwise_320 */
    {.id = 322, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){122}}, .output_tensors = {1, (const int32_t[1]){123}} }, /* reduce_321 */
    {.id = 322, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){123}}, .output_tensors = {1, (const int32_t[1]){124}} }, /* reduce_321_Mul */
    {.id = 323, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){124}}, .output_tensors = {1, (const int32_t[1]){125}} }, /* nl_323 */
    {.id = 324, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){125}}, .output_tensors = {1, (const int32_t[1]){126}} }, /* eltwise_324 */
    {.id = 327, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){119, 126}}, .output_tensors = {1, (const int32_t[1]){127}} }, /* eltwise_327 */
    {.id = 319, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){121}}, .output_tensors = {1, (const int32_t[1]){128}} }, /* nl_319 */
    {.id = 325, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){128, 126}}, .output_tensors = {1, (const int32_t[1]){129}} }, /* eltwise_325 */
    {.id = 326, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){129}}, .output_tensors = {1, (const int32_t[1]){130}} }, /* eltwise_326 */
    {.id = 328, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){127, 130}}, .output_tensors = {1, (const int32_t[1]){131}} }, /* eltwise_328 */
    {.id = 339, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){131}}, .output_tensors = {1, (const int32_t[1]){132}} }, /* gemm_337 */
    {.id = 340, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){132}}, .output_tensors = {1, (const int32_t[1]){133}} }, /* nl_340 */
    {.id = 351, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){133}}, .output_tensors = {1, (const int32_t[1]){134}} }, /* gemm_349 */
    {.id = 352, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){119, 134}}, .output_tensors = {1, (const int32_t[1]){135}} }, /* eltwise_352 */
    {.id = 353, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){135}}, .output_tensors = {1, (const int32_t[1]){136}} }, /* reduce_353 */
    {.id = 353, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){136}}, .output_tensors = {1, (const int32_t[1]){137}} }, /* reduce_353_Mul */
    {.id = 355, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){135, 137}}, .output_tensors = {1, (const int32_t[1]){138}} }, /* eltwise_355 */
    {.id = 357, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){138}}, .output_tensors = {1, (const int32_t[1]){139}} }, /* reduce_356 */
    {.id = 357, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){139}}, .output_tensors = {1, (const int32_t[1]){140}} }, /* reduce_356_Mul */
    {.id = 358, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){140}}, .output_tensors = {1, (const int32_t[1]){141}} }, /* nl_358 */
    {.id = 359, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){141}}, .output_tensors = {1, (const int32_t[1]){142}} }, /* eltwise_359 */
    {.id = 362, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){135, 142}}, .output_tensors = {1, (const int32_t[1]){143}} }, /* eltwise_362 */
    {.id = 354, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){137}}, .output_tensors = {1, (const int32_t[1]){144}} }, /* nl_354 */
    {.id = 360, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){144, 142}}, .output_tensors = {1, (const int32_t[1]){145}} }, /* eltwise_360 */
    {.id = 361, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){145}}, .output_tensors = {1, (const int32_t[1]){146}} }, /* eltwise_361 */
    {.id = 363, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){143, 146}}, .output_tensors = {1, (const int32_t[1]){147}} }, /* eltwise_363 */
    {.id = 364, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){147}}, .output_tensors = {1, (const int32_t[1]){148}} }, /* gemm_364 */
    {.id = 366, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){148}}, .output_tensors = {1, (const int32_t[1]){149}} }, /* eltwise_366 */
    {.id = 371, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){149}}, .output_tensors = {1, (const int32_t[1]){150}} }, /* transpose_371 */
    {.id = 367, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){147}}, .output_tensors = {1, (const int32_t[1]){151}} }, /* gemm_367 */
    {.id = 369, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){151}}, .output_tensors = {1, (const int32_t[1]){152}} }, /* eltwise_369 */
    {.id = 370, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){152}}, .output_tensors = {1, (const int32_t[1]){153}} }, /* transpose_370 */
    {.id = 372, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){153, 150}}, .output_tensors = {1, (const int32_t[1]){154}} }, /* gemm_372 */
    {.id = 373, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){154}}, .output_tensors = {1, (const int32_t[1]){155}} }, /* eltwise_373 */
    {.id = 375, .type = AI_LAYER_SM_TYPE, .input_tensors = {1, (const int32_t[1]){155}}, .output_tensors = {1, (const int32_t[1]){156}} }, /* nl_375 */
    {.id = 376, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){147}}, .output_tensors = {1, (const int32_t[1]){157}} }, /* gemm_376 */
    {.id = 378, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){157}}, .output_tensors = {1, (const int32_t[1]){158}} }, /* eltwise_378 */
    {.id = 379, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){158}}, .output_tensors = {1, (const int32_t[1]){159}} }, /* transpose_379 */
    {.id = 380, .type = AI_LAYER_MATMUL_TYPE, .input_tensors = {2, (const int32_t[2]){156, 159}}, .output_tensors = {1, (const int32_t[1]){160}} }, /* gemm_380 */
    {.id = 382, .type = AI_LAYER_TRANSPOSE_TYPE, .input_tensors = {1, (const int32_t[1]){160}}, .output_tensors = {1, (const int32_t[1]){161}} }, /* transpose_382 */
    {.id = 384, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){161}}, .output_tensors = {1, (const int32_t[1]){162}} }, /* gemm_384 */
    {.id = 385, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){135, 162}}, .output_tensors = {1, (const int32_t[1]){163}} }, /* eltwise_385 */
    {.id = 386, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){163}}, .output_tensors = {1, (const int32_t[1]){164}} }, /* reduce_386 */
    {.id = 386, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){164}}, .output_tensors = {1, (const int32_t[1]){165}} }, /* reduce_386_Mul */
    {.id = 388, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){163, 165}}, .output_tensors = {1, (const int32_t[1]){166}} }, /* eltwise_388 */
    {.id = 390, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){166}}, .output_tensors = {1, (const int32_t[1]){167}} }, /* reduce_389 */
    {.id = 390, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){167}}, .output_tensors = {1, (const int32_t[1]){168}} }, /* reduce_389_Mul */
    {.id = 391, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){168}}, .output_tensors = {1, (const int32_t[1]){169}} }, /* nl_391 */
    {.id = 392, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){169}}, .output_tensors = {1, (const int32_t[1]){170}} }, /* eltwise_392 */
    {.id = 395, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){163, 170}}, .output_tensors = {1, (const int32_t[1]){171}} }, /* eltwise_395 */
    {.id = 387, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){165}}, .output_tensors = {1, (const int32_t[1]){172}} }, /* nl_387 */
    {.id = 393, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){172, 170}}, .output_tensors = {1, (const int32_t[1]){173}} }, /* eltwise_393 */
    {.id = 394, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){173}}, .output_tensors = {1, (const int32_t[1]){174}} }, /* eltwise_394 */
    {.id = 396, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){171, 174}}, .output_tensors = {1, (const int32_t[1]){175}} }, /* eltwise_396 */
    {.id = 407, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){175}}, .output_tensors = {1, (const int32_t[1]){176}} }, /* gemm_405 */
    {.id = 408, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){176}}, .output_tensors = {1, (const int32_t[1]){177}} }, /* nl_408 */
    {.id = 419, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){177}}, .output_tensors = {1, (const int32_t[1]){178}} }, /* gemm_417 */
    {.id = 420, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){163, 178}}, .output_tensors = {1, (const int32_t[1]){179}} }, /* eltwise_420 */
    {.id = 1, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){0}}, .output_tensors = {1, (const int32_t[1]){180}} }, /* conv2d_1 */
    {.id = 3, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){180}}, .output_tensors = {1, (const int32_t[1]){181}} }, /* pad_3 */
    {.id = 5, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){181}}, .output_tensors = {1, (const int32_t[1]){182}} }, /* conv2d_5 */
    {.id = 7, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){182}}, .output_tensors = {1, (const int32_t[1]){183}} }, /* reduce_7 */
    {.id = 7, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){183}}, .output_tensors = {1, (const int32_t[1]){184}} }, /* reduce_7_Mul */
    {.id = 9, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){182, 184}}, .output_tensors = {1, (const int32_t[1]){185}} }, /* eltwise_9 */
    {.id = 11, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){185}}, .output_tensors = {1, (const int32_t[1]){186}} }, /* reduce_10 */
    {.id = 11, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){186}}, .output_tensors = {1, (const int32_t[1]){187}} }, /* reduce_10_Mul */
    {.id = 12, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){187}}, .output_tensors = {1, (const int32_t[1]){188}} }, /* nl_12 */
    {.id = 13, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){188}}, .output_tensors = {1, (const int32_t[1]){189}} }, /* eltwise_13 */
    {.id = 16, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){182, 189}}, .output_tensors = {1, (const int32_t[1]){190}} }, /* eltwise_16 */
    {.id = 8, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){184}}, .output_tensors = {1, (const int32_t[1]){191}} }, /* nl_8 */
    {.id = 14, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){191, 189}}, .output_tensors = {1, (const int32_t[1]){192}} }, /* eltwise_14 */
    {.id = 15, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){192}}, .output_tensors = {1, (const int32_t[1]){193}} }, /* eltwise_15 */
    {.id = 17, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){190, 193}}, .output_tensors = {1, (const int32_t[1]){194}} }, /* eltwise_17 */
    {.id = 18, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){194}}, .output_tensors = {1, (const int32_t[1]){195}} }, /* nl_18 */
    {.id = 19, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){180, 195}}, .output_tensors = {1, (const int32_t[1]){196}} }, /* eltwise_19 */
    {.id = 20, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){196}}, .output_tensors = {1, (const int32_t[1]){197}} }, /* pad_20 */
    {.id = 26, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){197}}, .output_tensors = {1, (const int32_t[1]){198}} }, /* conv2d_23 */
    {.id = 27, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){198}}, .output_tensors = {1, (const int32_t[1]){199}} }, /* reduce_27 */
    {.id = 27, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){199}}, .output_tensors = {1, (const int32_t[1]){200}} }, /* reduce_27_Mul */
    {.id = 29, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){198, 200}}, .output_tensors = {1, (const int32_t[1]){201}} }, /* eltwise_29 */
    {.id = 31, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){201}}, .output_tensors = {1, (const int32_t[1]){202}} }, /* reduce_30 */
    {.id = 31, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){202}}, .output_tensors = {1, (const int32_t[1]){203}} }, /* reduce_30_Mul */
    {.id = 32, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){203}}, .output_tensors = {1, (const int32_t[1]){204}} }, /* nl_32 */
    {.id = 33, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){204}}, .output_tensors = {1, (const int32_t[1]){205}} }, /* eltwise_33 */
    {.id = 36, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){198, 205}}, .output_tensors = {1, (const int32_t[1]){206}} }, /* eltwise_36 */
    {.id = 28, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){200}}, .output_tensors = {1, (const int32_t[1]){207}} }, /* nl_28 */
    {.id = 34, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){207, 205}}, .output_tensors = {1, (const int32_t[1]){208}} }, /* eltwise_34 */
    {.id = 35, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){208}}, .output_tensors = {1, (const int32_t[1]){209}} }, /* eltwise_35 */
    {.id = 37, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){206, 209}}, .output_tensors = {1, (const int32_t[1]){210}} }, /* eltwise_37 */
    {.id = 38, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){210}}, .output_tensors = {1, (const int32_t[1]){211}} }, /* nl_38 */
    {.id = 39, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){196, 211}}, .output_tensors = {1, (const int32_t[1]){212}} }, /* eltwise_39 */
    {.id = 40, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){212}}, .output_tensors = {1, (const int32_t[1]){213}} }, /* pad_40 */
    {.id = 46, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){213}}, .output_tensors = {1, (const int32_t[1]){214}} }, /* conv2d_43 */
    {.id = 47, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){214}}, .output_tensors = {1, (const int32_t[1]){215}} }, /* reduce_47 */
    {.id = 47, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){215}}, .output_tensors = {1, (const int32_t[1]){216}} }, /* reduce_47_Mul */
    {.id = 49, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){214, 216}}, .output_tensors = {1, (const int32_t[1]){217}} }, /* eltwise_49 */
    {.id = 51, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){217}}, .output_tensors = {1, (const int32_t[1]){218}} }, /* reduce_50 */
    {.id = 51, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){218}}, .output_tensors = {1, (const int32_t[1]){219}} }, /* reduce_50_Mul */
    {.id = 52, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){219}}, .output_tensors = {1, (const int32_t[1]){220}} }, /* nl_52 */
    {.id = 53, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){220}}, .output_tensors = {1, (const int32_t[1]){221}} }, /* eltwise_53 */
    {.id = 56, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){214, 221}}, .output_tensors = {1, (const int32_t[1]){222}} }, /* eltwise_56 */
    {.id = 48, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){216}}, .output_tensors = {1, (const int32_t[1]){223}} }, /* nl_48 */
    {.id = 54, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){223, 221}}, .output_tensors = {1, (const int32_t[1]){224}} }, /* eltwise_54 */
    {.id = 55, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){224}}, .output_tensors = {1, (const int32_t[1]){225}} }, /* eltwise_55 */
    {.id = 57, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){222, 225}}, .output_tensors = {1, (const int32_t[1]){226}} }, /* eltwise_57 */
    {.id = 58, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){226}}, .output_tensors = {1, (const int32_t[1]){227}} }, /* nl_58 */
    {.id = 59, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){212, 227}}, .output_tensors = {1, (const int32_t[1]){228}} }, /* eltwise_59 */
    {.id = 60, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){228}}, .output_tensors = {1, (const int32_t[1]){229}} }, /* pad_60 */
    {.id = 66, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){229}}, .output_tensors = {1, (const int32_t[1]){230}} }, /* conv2d_63 */
    {.id = 67, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){230}}, .output_tensors = {1, (const int32_t[1]){231}} }, /* reduce_67 */
    {.id = 67, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){231}}, .output_tensors = {1, (const int32_t[1]){232}} }, /* reduce_67_Mul */
    {.id = 69, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){230, 232}}, .output_tensors = {1, (const int32_t[1]){233}} }, /* eltwise_69 */
    {.id = 71, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){233}}, .output_tensors = {1, (const int32_t[1]){234}} }, /* reduce_70 */
    {.id = 71, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){234}}, .output_tensors = {1, (const int32_t[1]){235}} }, /* reduce_70_Mul */
    {.id = 72, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){235}}, .output_tensors = {1, (const int32_t[1]){236}} }, /* nl_72 */
    {.id = 73, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){236}}, .output_tensors = {1, (const int32_t[1]){237}} }, /* eltwise_73 */
    {.id = 76, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){230, 237}}, .output_tensors = {1, (const int32_t[1]){238}} }, /* eltwise_76 */
    {.id = 68, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){232}}, .output_tensors = {1, (const int32_t[1]){239}} }, /* nl_68 */
    {.id = 74, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){239, 237}}, .output_tensors = {1, (const int32_t[1]){240}} }, /* eltwise_74 */
    {.id = 75, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){240}}, .output_tensors = {1, (const int32_t[1]){241}} }, /* eltwise_75 */
    {.id = 77, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){238, 241}}, .output_tensors = {1, (const int32_t[1]){242}} }, /* eltwise_77 */
    {.id = 78, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){242}}, .output_tensors = {1, (const int32_t[1]){243}} }, /* nl_78 */
    {.id = 79, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){228, 243}}, .output_tensors = {1, (const int32_t[1]){244}} }, /* eltwise_79 */
    {.id = 80, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){244}}, .output_tensors = {1, (const int32_t[1]){245}} }, /* pad_80 */
    {.id = 86, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){245}}, .output_tensors = {1, (const int32_t[1]){246}} }, /* conv2d_83 */
    {.id = 87, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){246}}, .output_tensors = {1, (const int32_t[1]){247}} }, /* reduce_87 */
    {.id = 87, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){247}}, .output_tensors = {1, (const int32_t[1]){248}} }, /* reduce_87_Mul */
    {.id = 89, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){246, 248}}, .output_tensors = {1, (const int32_t[1]){249}} }, /* eltwise_89 */
    {.id = 91, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){249}}, .output_tensors = {1, (const int32_t[1]){250}} }, /* reduce_90 */
    {.id = 91, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){250}}, .output_tensors = {1, (const int32_t[1]){251}} }, /* reduce_90_Mul */
    {.id = 92, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){251}}, .output_tensors = {1, (const int32_t[1]){252}} }, /* nl_92 */
    {.id = 93, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){252}}, .output_tensors = {1, (const int32_t[1]){253}} }, /* eltwise_93 */
    {.id = 96, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){246, 253}}, .output_tensors = {1, (const int32_t[1]){254}} }, /* eltwise_96 */
    {.id = 88, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){248}}, .output_tensors = {1, (const int32_t[1]){255}} }, /* nl_88 */
    {.id = 94, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){255, 253}}, .output_tensors = {1, (const int32_t[1]){256}} }, /* eltwise_94 */
    {.id = 95, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){256}}, .output_tensors = {1, (const int32_t[1]){257}} }, /* eltwise_95 */
    {.id = 97, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){254, 257}}, .output_tensors = {1, (const int32_t[1]){258}} }, /* eltwise_97 */
    {.id = 98, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){258}}, .output_tensors = {1, (const int32_t[1]){259}} }, /* nl_98 */
    {.id = 99, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){244, 259}}, .output_tensors = {1, (const int32_t[1]){260}} }, /* eltwise_99 */
    {.id = 100, .type = AI_LAYER_PAD_TYPE, .input_tensors = {1, (const int32_t[1]){260}}, .output_tensors = {1, (const int32_t[1]){261}} }, /* pad_100 */
    {.id = 106, .type = AI_LAYER_CONV2D_TYPE, .input_tensors = {1, (const int32_t[1]){261}}, .output_tensors = {1, (const int32_t[1]){262}} }, /* conv2d_103 */
    {.id = 107, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){262}}, .output_tensors = {1, (const int32_t[1]){263}} }, /* reduce_107 */
    {.id = 107, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){263}}, .output_tensors = {1, (const int32_t[1]){264}} }, /* reduce_107_Mul */
    {.id = 109, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){262, 264}}, .output_tensors = {1, (const int32_t[1]){265}} }, /* eltwise_109 */
    {.id = 111, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){265}}, .output_tensors = {1, (const int32_t[1]){266}} }, /* reduce_110 */
    {.id = 111, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){266}}, .output_tensors = {1, (const int32_t[1]){267}} }, /* reduce_110_Mul */
    {.id = 112, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){267}}, .output_tensors = {1, (const int32_t[1]){268}} }, /* nl_112 */
    {.id = 113, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){268}}, .output_tensors = {1, (const int32_t[1]){269}} }, /* eltwise_113 */
    {.id = 116, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){262, 269}}, .output_tensors = {1, (const int32_t[1]){270}} }, /* eltwise_116 */
    {.id = 108, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){264}}, .output_tensors = {1, (const int32_t[1]){271}} }, /* nl_108 */
    {.id = 114, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){271, 269}}, .output_tensors = {1, (const int32_t[1]){272}} }, /* eltwise_114 */
    {.id = 115, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){272}}, .output_tensors = {1, (const int32_t[1]){273}} }, /* eltwise_115 */
    {.id = 117, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){270, 273}}, .output_tensors = {1, (const int32_t[1]){274}} }, /* eltwise_117 */
    {.id = 118, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){274}}, .output_tensors = {1, (const int32_t[1]){275}} }, /* nl_118 */
    {.id = 119, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){260, 275}}, .output_tensors = {1, (const int32_t[1]){276}} }, /* eltwise_119 */
    {.id = 128, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){276}}, .output_tensors = {1, (const int32_t[1]){277}} }, /* gemm_128 */
    {.id = 131, .type = AI_LAYER_SM_TYPE, .input_tensors = {1, (const int32_t[1]){277}}, .output_tensors = {1, (const int32_t[1]){278}} }, /* nl_131 */
    {.id = 133, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){276, 278}}, .output_tensors = {1, (const int32_t[1]){279}} }, /* eltwise_133 */
    {.id = 134, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){279}}, .output_tensors = {1, (const int32_t[1]){280}} }, /* reduce_134 */
    {.id = 137, .type = AI_LAYER_TILE_TYPE, .input_tensors = {1, (const int32_t[1]){280}}, .output_tensors = {1, (const int32_t[1]){281}} }, /* tile_137 */
    {.id = 421, .type = AI_LAYER_CONCAT_TYPE, .input_tensors = {2, (const int32_t[2]){179, 281}}, .output_tensors = {1, (const int32_t[1]){282}} }, /* concat_421 */
    {.id = 422, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){282}}, .output_tensors = {1, (const int32_t[1]){283}} }, /* reduce_422 */
    {.id = 422, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){283}}, .output_tensors = {1, (const int32_t[1]){284}} }, /* reduce_422_Mul */
    {.id = 424, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){282, 284}}, .output_tensors = {1, (const int32_t[1]){285}} }, /* eltwise_424 */
    {.id = 426, .type = AI_LAYER_REDUCE_TYPE, .input_tensors = {1, (const int32_t[1]){285}}, .output_tensors = {1, (const int32_t[1]){286}} }, /* reduce_425 */
    {.id = 426, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){286}}, .output_tensors = {1, (const int32_t[1]){287}} }, /* reduce_425_Mul */
    {.id = 427, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){287}}, .output_tensors = {1, (const int32_t[1]){288}} }, /* nl_427 */
    {.id = 428, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {1, (const int32_t[1]){288}}, .output_tensors = {1, (const int32_t[1]){289}} }, /* eltwise_428 */
    {.id = 431, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){282, 289}}, .output_tensors = {1, (const int32_t[1]){290}} }, /* eltwise_431 */
    {.id = 423, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){284}}, .output_tensors = {1, (const int32_t[1]){291}} }, /* nl_423 */
    {.id = 429, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){291, 289}}, .output_tensors = {1, (const int32_t[1]){292}} }, /* eltwise_429 */
    {.id = 430, .type = AI_LAYER_BN_TYPE, .input_tensors = {1, (const int32_t[1]){292}}, .output_tensors = {1, (const int32_t[1]){293}} }, /* eltwise_430 */
    {.id = 432, .type = AI_LAYER_ELTWISE_TYPE, .input_tensors = {2, (const int32_t[2]){290, 293}}, .output_tensors = {1, (const int32_t[1]){294}} }, /* eltwise_432 */
    {.id = 443, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){294}}, .output_tensors = {1, (const int32_t[1]){295}} }, /* gemm_441 */
    {.id = 444, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){295}}, .output_tensors = {1, (const int32_t[1]){296}} }, /* nl_444 */
    {.id = 455, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){296}}, .output_tensors = {1, (const int32_t[1]){297}} }, /* gemm_453 */
    {.id = 456, .type = AI_LAYER_NL_TYPE, .input_tensors = {1, (const int32_t[1]){297}}, .output_tensors = {1, (const int32_t[1]){298}} }, /* nl_456 */
    {.id = 467, .type = AI_LAYER_DENSE_TYPE, .input_tensors = {1, (const int32_t[1]){298}}, .output_tensors = {1, (const int32_t[1]){299}} } /* gemm_465 */
  },
  .n_nodes = 298
};
#endif

