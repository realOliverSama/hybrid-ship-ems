/**
  ******************************************************************************
  * @file    n1g3.h
  * @author  STEdgeAI
  * @date    2026-09-25 01:49:48
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
#ifndef LL_ATON_N1G3_H
#define LL_ATON_N1G3_H

/******************************************************************************/
#define LL_ATON_N1G3_C_MODEL_NAME        "n1g3"
#define LL_ATON_N1G3_ORIGIN_MODEL_NAME   "n1g3"

/************************** USER ALLOCATED IOs ********************************/
#define LL_ATON_N1G3_USER_ALLOCATED_INPUTS   (1)  // Number of input buffers not allocated by the compiler
#define LL_ATON_N1G3_USER_ALLOCATED_OUTPUTS  (1)  // Number of output buffers not allocated by the compiler

/************************** INPUTS ********************************************/
#define LL_ATON_N1G3_IN_NUM        (1)    // Total number of input buffers
// Input buffer 1 -- Input_2_out_0
#define LL_ATON_N1G3_IN_1_ALIGNMENT   (32)
#define LL_ATON_N1G3_IN_1_SIZE_BYTES  (28800)

/************************** OUTPUTS *******************************************/
#define LL_ATON_N1G3_OUT_NUM        (1)    // Total number of output buffers
// Output buffer 1 -- Add_15_out_0
#define LL_ATON_N1G3_OUT_1_ALIGNMENT   (32)
#define LL_ATON_N1G3_OUT_1_SIZE_BYTES  (115200)

#endif /* LL_ATON_N1G3_H */
