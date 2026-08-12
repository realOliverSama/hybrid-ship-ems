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
#include "./KEY/key.h"

#define MAIN_THREAD_STACK_SIZE  512
#define MAIN_THREAD_PRIORITY    5

static TX_TIMER timer;
static TX_THREAD main_thread;

static VOID timer_expiration(ULONG id);
static VOID main_thread_entry(ULONG id);

UINT app_threadx_init(VOID *memory_ptr)
{
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)memory_ptr;
    CHAR *pointer;

    if (tx_timer_create(&timer, "Timer", timer_expiration, 0, 50, 50, TX_AUTO_ACTIVATE) != TX_SUCCESS)
    {
        return TX_TIMER_ERROR;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, MAIN_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&main_thread, "Main Thread", main_thread_entry, 0, pointer, MAIN_THREAD_STACK_SIZE, MAIN_THREAD_PRIORITY, MAIN_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    return TX_SUCCESS;
}

static VOID timer_expiration(ULONG id)
{
    TX_PARAMETER_NOT_USED(id);

    LED1_TOGGLE();
}

static VOID main_thread_entry(ULONG id)
{
    TX_PARAMETER_NOT_USED(id);

    uint8_t t = 0;
    uint8_t key;
    UINT timer_active;

    while (1)
    {
        key = key_scan(0);
        if (key == KEY0_PRES)
        {
            tx_timer_info_get(&timer, NULL, &timer_active, NULL, NULL, NULL);
            if (timer_active == TX_TRUE)
            {
                tx_timer_deactivate(&timer);
            }
            else
            {
                tx_timer_activate(&timer);
            }
        }

        if (++t == 20)
        {
            t = 0;
            LED0_TOGGLE();
        }

        tx_thread_sleep(1);
    }
}
