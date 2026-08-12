/**
 ****************************************************************************************************
 * @file        app_azure_rtos.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_azure_rtos.c文件
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#include "app_azure_rtos.h"
#include "app_threadx.h"
#include "main.h"

#if (USE_STATIC_ALLOCATION == 1)
__ALIGN_BEGIN static UCHAR tx_byte_pool_buffer[TX_APP_MEM_POOL_SIZE] __ALIGN_END;
static TX_BYTE_POOL tx_app_byte_pool;
#endif

VOID tx_application_define(VOID *first_unused_memory)
{
#if (USE_STATIC_ALLOCATION == 1)
    UINT status = TX_SUCCESS;
    VOID *memory_ptr;

    if (tx_byte_pool_create(&tx_app_byte_pool, "ThreadX App Memory Pool", tx_byte_pool_buffer, TX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
    {
        Error_Handler();
    }

    memory_ptr = (VOID *)&tx_app_byte_pool;
    status = app_threadx_init(memory_ptr);
    if (status != TX_SUCCESS)
    {
        Error_Handler();
    }
#else
    TX_PARAMETER_NOT_USED(first_unused_memory);

    /*
     * Add the following section into the .ld file:
     * ._threadx_heap :
     * {
     *   . = ALIGN(8);
     *   __RAM_segment_used_end__ = .;
     *   . = . + 64K;
     *   . = ALIGN(8);
     * } >RAM
     */
#endif
}
