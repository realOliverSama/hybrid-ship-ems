/**
  ******************************************************************************
  * @file    reattn2s.h
  * @author  STEdgeAI
  * @date    2026-09-23 09:37:58
  * @brief   Minimal description of the generated c-implemention of the network
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */
#ifndef LL_ATON_REATTN2S_H
#define LL_ATON_REATTN2S_H

/******************************************************************************/
#define LL_ATON_REATTN2S_C_MODEL_NAME        "reattn2s"
#define LL_ATON_REATTN2S_ORIGIN_MODEL_NAME   "reattn2n_src"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_REATTN2S_USER_ALLOCATED_INPUTS   (2)  // Number of input buffers not allocated by the compiler
#define LL_ATON_REATTN2S_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_REATTN2S_IN_NUM        (2)    // Total number of input buffers
// Input buffer 1 -- Input_59_out_0
#define LL_ATON_REATTN2S_IN_1_ALIGNMENT   (32)
#define LL_ATON_REATTN2S_IN_1_SIZE_BYTES  (180)
// Input buffer 2 -- Input_53_out_0
#define LL_ATON_REATTN2S_IN_2_ALIGNMENT   (32)
#define LL_ATON_REATTN2S_IN_2_SIZE_BYTES  (384)

/************************** OUTPUTS *******************************************/
#define LL_ATON_REATTN2S_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Quantize_768_out_0
#define LL_ATON_REATTN2S_OUT_1_ALIGNMENT   (32)
#define LL_ATON_REATTN2S_OUT_1_SIZE_BYTES  (90)

#endif /* LL_ATON_REATTN2S_H */
