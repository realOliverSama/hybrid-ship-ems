/**
 ****************************************************************************************************
 * @file        ux_host_hid_keyboard.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_host_hid_keyboard.c文件
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

#include "ux_host_hid_keyboard.h"
#include "stm32n6xx_hal.h"

UX_HOST_CLASS_HID_KEYBOARD *keyboard = UX_NULL;

__weak void ux_host_hid_keyboard_user_process(UX_HOST_CLASS_HID_KEYBOARD *keyboard)
{
    UX_PARAMETER_NOT_USED(keyboard);

    return;
}

void ux_host_hid_keyboard_process(void)
{
    if (keyboard != UX_NULL)
    {
        ux_host_hid_keyboard_user_process(keyboard);
    }
}
