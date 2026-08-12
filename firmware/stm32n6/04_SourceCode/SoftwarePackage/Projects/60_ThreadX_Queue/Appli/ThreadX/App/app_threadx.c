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
#include "./RGBLCD/rgblcd.h"
#include <stdio.h>

#define MESSAGE_QUEUE_SIZE      1
#define MAIN_THREAD_STACK_SIZE  512
#define MAIN_THREAD_PRIORITY    5
#define LCD_THREAD_STACK_SIZE   512
#define LCD_THREAD_PRIORITY     5

static TX_QUEUE message_queue;
static TX_THREAD main_thread;
static TX_THREAD lcd_thread;

static VOID main_thread_entry(ULONG id);
static VOID lcd_thread_entry(ULONG id);

UINT app_threadx_init(VOID *memory_ptr)
{
    TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)memory_ptr;
    CHAR *pointer;

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, MESSAGE_QUEUE_SIZE * sizeof(ULONG), TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_queue_create(&message_queue, "Message Queue", 1, pointer, MESSAGE_QUEUE_SIZE * sizeof(ULONG)) != TX_SUCCESS)
    {
        return TX_QUEUE_ERROR;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, MAIN_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&main_thread, "Main Thread", main_thread_entry, 0, pointer, MAIN_THREAD_STACK_SIZE, MAIN_THREAD_PRIORITY, MAIN_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    if (tx_byte_allocate(byte_pool, (VOID **)&pointer, LCD_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
    {
        return TX_POOL_ERROR;
    }

    if (tx_thread_create(&lcd_thread, "LCD Thread", lcd_thread_entry, 0, pointer, LCD_THREAD_STACK_SIZE, LCD_THREAD_PRIORITY, LCD_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
    {
        return TX_THREAD_ERROR;
    }

    return TX_SUCCESS;
}

static VOID main_thread_entry(ULONG id)
{
    TX_PARAMETER_NOT_USED(id);

    uint8_t t = 0;
    uint8_t key;
    ULONG messages[5] = {
        (ULONG)"This is message 0.",
        (ULONG)"This is message 1.",
        (ULONG)"This is message 2.",
        (ULONG)"This is message 3.",
        (ULONG)"This is message 4."
    };

    while (1)
    {
        key = key_scan(0);
        if (key == KEY0_PRES)
        {
            tx_queue_send(&message_queue, &messages[t % 5], TX_WAIT_FOREVER);
        }

        if (++t == 20)
        {
            t = 0;
            LED0_TOGGLE();
        }

        tx_thread_sleep(1);
    }
}

static VOID lcd_thread_entry(ULONG id)
{
    TX_PARAMETER_NOT_USED(id);

    ULONG message;
    
    while (1)
    {
        if (tx_queue_receive(&message_queue, &message, TX_WAIT_FOREVER) == TX_SUCCESS)
        {
            rgblcd_show_string(30, 110, 200, 16, 16, (char *)message, BLUE);
        }
    }
}
