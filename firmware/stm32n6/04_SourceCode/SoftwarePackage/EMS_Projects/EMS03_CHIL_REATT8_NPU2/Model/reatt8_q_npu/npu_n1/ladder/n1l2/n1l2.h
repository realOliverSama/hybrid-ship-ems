/**
  ******************************************************************************
  * @file    n1l2.h
  * @author  STEdgeAI
  * @date    2026-09-25 00:16:43
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
#ifndef LL_ATON_N1L2_H
#define LL_ATON_N1L2_H

/******************************************************************************/
#define LL_ATON_N1L2_C_MODEL_NAME        "n1l2"
#define LL_ATON_N1L2_ORIGIN_MODEL_NAME   "n1l2"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_N1L2_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_N1L2_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_N1L2_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_14_out_0
#define LL_ATON_N1L2_IN_1_ALIGNMENT   (32)
#define LL_ATON_N1L2_IN_1_SIZE_BYTES  (13200)

/************************** OUTPUTS *******************************************/
#define LL_ATON_N1L2_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Add_159_out_0
#define LL_ATON_N1L2_OUT_1_ALIGNMENT   (32)
#define LL_ATON_N1L2_OUT_1_SIZE_BYTES  (115200)

#endif /* LL_ATON_N1L2_H */
