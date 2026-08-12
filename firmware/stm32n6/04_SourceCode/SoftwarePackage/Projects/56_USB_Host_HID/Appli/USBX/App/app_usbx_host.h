/**
 ****************************************************************************************************
 * @file        app_usbx_host.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_usbx_host.h文件
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

#ifndef __APP_USBX_HOST_H
#define __APP_USBX_HOST_H

#include "stm32n6xx_hal.h"

#define USBX_HOST_MEMORY_STACK_SIZE (24 * 1024)

uint8_t app_usbx_host_init(void);
void app_usbx_host_device_connect_callback(void);
void app_usbx_host_device_disconnect_callback(void);

#endif
