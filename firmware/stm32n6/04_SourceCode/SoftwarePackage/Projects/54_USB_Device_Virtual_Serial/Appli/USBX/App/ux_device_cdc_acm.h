/**
 ****************************************************************************************************
 * @file        ux_device_cdc_acm.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_device_cdc_acm.h文件
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

#ifndef __UX_DEVICE_CDC_ACM_H
#define __UX_DEVICE_CDC_ACM_H

#include "ux_api.h"

#define USBX_DEVICE_CDC_ACM_WRITE_BUFFER_SIZE   200
#define USBX_DEVICE_CDC_ACM_READ_BUFFER_SIZE    200
#define USBX_DEVICE_CDC_ACM_WRITE_TIMEOUT       200

extern uint8_t *g_ux_device_cdc_acm_rx_buffer;
extern uint16_t g_ux_device_cdc_acm_rx_len;

VOID ux_device_cdc_acm_instance_activate(VOID *cdc_acm);
VOID ux_device_cdc_acm_instance_deactivate(VOID *cdc_acm);
VOID ux_device_cdc_acm_parameter_change(VOID *cdc_acm);
void ux_device_cdc_acm_transmit(uint8_t *data, uint32_t len);
void ux_device_cdc_acm_printf(char *fmt, ...);

#endif
