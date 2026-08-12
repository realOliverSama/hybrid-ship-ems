/**
 ****************************************************************************************************
 * @file        app_threadx.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_threadx.c文件
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

#include "app_threadx.h"
#include "./LED/led.h"

#define LED0_THREAD_STACK_SIZE  512
#define LED0_THREAD_PRIORITY    5
#define LED1_THREAD_STACK_SIZE  512
#define LED1_THREAD_PRIORITY    5

static TX_THREAD led0_thread;
static TX_THREAD led1_thread;

static VOID led0_thread_entry(ULONG id);
static VOID led1_thread_entry(ULONG id);

UINT app_threadx_init(VOID *memory_ptr)
{
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)memory_ptr;
    CHAR *pointer;

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, LED0_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&led0_thread, "LED0 Thread", led0_thread_entry, 0, pointer, LED0_THREAD_STACK_SIZE, LED0_THREAD_PRIORITY, LED0_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, LED1_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&led1_thread, "LED1 Thread", led1_thread_entry, 0, pointer, LED1_THREAD_STACK_SIZE, LED1_THREAD_PRIORITY, LED1_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    return TX_SUCCESS;
}

static VOID led0_thread_entry(ULONG id)
{
    TX_PARAMETER_NOT_USED(id);

    while (1)
    {
        LED0(0);
        tx_thread_sleep(8);
        LED0(1);
        tx_thread_sleep(92);
    }
}

static VOID led1_thread_entry(ULONG id)
{
    TX_PARAMETER_NOT_USED(id);

    while (1)
    {
        LED1(0);
        tx_thread_sleep(30);
        LED1(1);
        tx_thread_sleep(30);
    }
}
