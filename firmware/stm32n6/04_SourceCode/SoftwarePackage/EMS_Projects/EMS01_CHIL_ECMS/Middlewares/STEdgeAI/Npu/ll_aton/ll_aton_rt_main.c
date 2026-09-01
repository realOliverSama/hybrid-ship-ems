/**
 ******************************************************************************
 * @file    ll_aton_rt_main.c
 * @author  SRA Artificial Intelligence & Embedded Architectures
 * @brief   Template `main()` function (named `LL_ATON_RT_Main()`) for
 *          Cube.AI/ATON integration in a RTOS-less application.
 * @note    This file is intended to be just a template and is limited to run the
 *          ATON LL runtime with a single network instance for a single inference!
 *          Please, generate your own embodiment of this file and customize it
 *          so to fit the needs of your application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "ll_aton_rt_user_api.h"

#include <stdint.h>
#include <stdio.h>

/* FR-505 归因临时（验收后移除）：逐 epoch 块调用墙钟计时环形日志。
 * 本工程 LL_ATON_RT_Main 为单次调用、内部 do-while 逐 epoch 推进（ST 模板），
 * 故逐次插桩点在循环内 RunEpochBlock 返回处：chunk[i] = 相邻两次 epoch-block
 * 调用出口的 DWT CYCCNT 差（EC epoch 的 NPU 异步执行时间经 WFE 等待落入下一
 * chunk），DONE 时清零索引，下一次推理从 chunk0 重新计。 */
uint32_t ems_epoch_cycles[32];
uint8_t  ems_epoch_idx;
static uint32_t ems_epoch_last;
void ems_epoch_dump(void)
{
    printf("FR505 epoch chunks (us, chunk0=推理起点对齐chunk含帧间空窗,忽略):\r\n");
    for (int i = 0; i < ems_epoch_idx; i++)
        printf("  [%d] %lu us\r\n", i,
               (unsigned long)(ems_epoch_cycles[i] / (SystemCoreClock / 1000000u)));
}

/*** Main Template ***/

/**
 * @brief Template for synchronously executing a single inference of a single network instance (e.g. regression tests)
 * @param network_instance pointer to the network instance representing the network and execution instance to execute.
 *                         The instance object MUST have already set a valid link to a network interface.
 *                         The user may declare/instantiate such an object by using either macro
 *                          `LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE()` to create both the execution instance
 *                         and the network interface, or macros
 *                         `LL_ATON_DECLARE_NAMED_NN_INTERFACE()` & `LL_ATON_DECLARE_NAMED_NN_INSTANCE()` to
 *                         create/instantiate the objects separately.
 */
void LL_ATON_RT_Main(NN_Instance_TypeDef *network_instance)
{
  LL_ATON_RT_RetValues_t ll_aton_rt_ret;

  /*** Start of user initialization code ***/

  /*** End of user initialization code ***/

  LL_ATON_ASSERT(network_instance != NULL);
  LL_ATON_ASSERT(network_instance->network != NULL);
  LL_ATON_RT_RuntimeInit();                  // Initialize runtime
  LL_ATON_RT_Init_Network(network_instance); // Initialize passed network instance object

  do
  {
    /* Execute first/next step of Cube.AI/ATON runtime */
    ll_aton_rt_ret = LL_ATON_RT_RunEpochBlock(network_instance);

    /*** Start of user event handling code ***/

    /* FR-505 归因临时（验收后移除）：本函数唯一插桩点，位于 do-while 循环体内、
     * RunEpochBlock 返回之后 WFE 之前——循环体每轮恰好执行一次，即每次
     * epoch-block 调用恰好记一次（函数为 void 单循环，无其他出口；顶部
     * network_instance NULL assert 属早退路径，按规格忽略不计）。 */
    {
        uint32_t now = DWT->CYCCNT;
        if (ems_epoch_idx < 32)
            ems_epoch_cycles[ems_epoch_idx] = now - ems_epoch_last;
        ems_epoch_last = now;
        ems_epoch_idx++;
        if (ll_aton_rt_ret == LL_ATON_RT_DONE)
            ems_epoch_idx = 0; /* 下一次推理从 0 重新计 */
    }

    /*** End of user event handling code ***/

    /* Wait for next event */
    if (ll_aton_rt_ret == LL_ATON_RT_WFE)
    { /*** subject to change to fit also user code requirements ***/
      LL_ATON_OSAL_WFE();
    }
  } while (ll_aton_rt_ret != LL_ATON_RT_DONE); /*** subject to change to fit also user code requirements ***/

  LL_ATON_RT_DeInit_Network(network_instance); // De-initialize the network instance object
  LL_ATON_RT_RuntimeDeInit();                  // De-initialize runtime

  /*** Start of user de-initialization code ***/

  /*** End of user de-initialization code ***/
}
