/**
 ****************************************************************************************************
 * @file        ux_host_hid_keyboard.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_host_hid_keyboard.h文件
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

#ifndef __UX_HOST_HID_KEYBOARD_H
#define __UX_HOST_HID_KEYBOARD_H

#include "ux_api.h"
#include "ux_host_class_hid.h"
#include "ux_host_class_hid_keyboard.h"

extern UX_HOST_CLASS_HID_KEYBOARD *keyboard;

void ux_host_hid_keyboard_user_process(UX_HOST_CLASS_HID_KEYBOARD *keyboard);
void ux_host_hid_keyboard_process(void);

#endif
