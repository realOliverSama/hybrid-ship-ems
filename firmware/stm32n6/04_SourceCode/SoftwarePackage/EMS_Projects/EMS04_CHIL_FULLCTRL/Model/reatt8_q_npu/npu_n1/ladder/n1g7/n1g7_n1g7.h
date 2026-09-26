/**
  ******************************************************************************
  * @file    n1g7.h
  * @author  STEdgeAI
  * @date    2026-09-25 01:51:13
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
#ifndef LL_ATON_N1G7_H
#define LL_ATON_N1G7_H

/******************************************************************************/
#define LL_ATON_N1G7_C_MODEL_NAME        "n1g7"
#define LL_ATON_N1G7_ORIGIN_MODEL_NAME   "n1g7"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_N1G7_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_N1G7_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_N1G7_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_0_out_0
#define LL_ATON_N1G7_IN_1_ALIGNMENT   (32)
#define LL_ATON_N1G7_IN_1_SIZE_BYTES  (28800)

/************************** OUTPUTS *******************************************/
#define LL_ATON_N1G7_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- ReduceSum_14_out_0
#define LL_ATON_N1G7_OUT_1_ALIGNMENT   (32)
#define LL_ATON_N1G7_OUT_1_SIZE_BYTES  (384)

#endif /* LL_ATON_N1G7_H */
