/**
 ****************************************************************************************************
 * @file        ux_device_cdc_acm.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_device_cdc_acm.c文件
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

#include "ux_device_cdc_acm.h"
#include "ux_dcd_stm32.h"
#include "ux_device_class_cdc_acm.h"
#include <stdarg.h>

static UX_SLAVE_CLASS_CDC_ACM *g_cdc_acm = NULL;
static uint8_t g_ux_device_cdc_acm_write_buffer[USBX_DEVICE_CDC_ACM_WRITE_BUFFER_SIZE];
static uint8_t g_ux_device_cdc_acm_read_buffer[USBX_DEVICE_CDC_ACM_READ_BUFFER_SIZE];
static uint8_t g_ux_device_cdc_acm_read_actual_length;
static uint8_t g_ux_device_cdc_acm_write_sta = 0;
uint8_t *g_ux_device_cdc_acm_rx_buffer = NULL;
uint16_t g_ux_device_cdc_acm_rx_len = 0;

static UINT ux_device_cdc_acm_parameter_write_callback(struct UX_SLAVE_CLASS_CDC_ACM_STRUCT *cdc_acm, UINT status, ULONG length);
static UINT ux_device_cdc_acm_parameter_read_callback(struct UX_SLAVE_CLASS_CDC_ACM_STRUCT *cdc_acm, UINT status, UCHAR *data_pointer, ULONG length);

VOID ux_device_cdc_acm_instance_activate(VOID *cdc_acm)
{
    UX_SLAVE_CLASS_CDC_ACM_CALLBACK_PARAMETER parameter;

    g_cdc_acm = (UX_SLAVE_CLASS_CDC_ACM *)cdc_acm;

    parameter.ux_device_class_cdc_acm_parameter_write_callback = ux_device_cdc_acm_parameter_write_callback;
    parameter.ux_device_class_cdc_acm_parameter_read_callback = ux_device_cdc_acm_parameter_read_callback;
    ux_device_class_cdc_acm_ioctl(g_cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_TRANSMISSION_START, (VOID *)&parameter);

    ux_device_class_cdc_acm_read_run(g_cdc_acm, (UCHAR *)&g_ux_device_cdc_acm_read_buffer, USBX_DEVICE_CDC_ACM_READ_BUFFER_SIZE, (ULONG *)&g_ux_device_cdc_acm_read_actual_length);
}

VOID ux_device_cdc_acm_instance_deactivate(VOID *cdc_acm)
{
    UX_PARAMETER_NOT_USED(cdc_acm);

    g_cdc_acm = NULL;
}

VOID ux_device_cdc_acm_parameter_change(VOID *cdc_acm)
{
    UX_SLAVE_CLASS_CDC_ACM_LINE_CODING_PARAMETER line_coding = {0};

    line_coding.ux_slave_class_cdc_acm_parameter_baudrate = ((UX_SLAVE_CLASS_CDC_ACM *)cdc_acm)->ux_slave_class_cdc_acm_baudrate;
    line_coding.ux_slave_class_cdc_acm_parameter_stop_bit = ((UX_SLAVE_CLASS_CDC_ACM *)cdc_acm)->ux_slave_class_cdc_acm_stop_bit;
    line_coding.ux_slave_class_cdc_acm_parameter_parity = ((UX_SLAVE_CLASS_CDC_ACM *)cdc_acm)->ux_slave_class_cdc_acm_parity;
    line_coding.ux_slave_class_cdc_acm_parameter_data_bit = ((UX_SLAVE_CLASS_CDC_ACM *)cdc_acm)->ux_slave_class_cdc_acm_data_bit;
    ux_device_class_cdc_acm_ioctl((UX_SLAVE_CLASS_CDC_ACM *)cdc_acm, UX_SLAVE_CLASS_CDC_ACM_IOCTL_SET_LINE_CODING, (VOID *)&line_coding);
}

void ux_device_cdc_acm_transmit(uint8_t *data, uint32_t len)
{
    uint32_t timeout = USBX_DEVICE_CDC_ACM_WRITE_TIMEOUT;

    if ((g_cdc_acm != NULL))
    {
        g_ux_device_cdc_acm_write_sta = 1;
        ux_device_class_cdc_acm_write_with_callback(g_cdc_acm, (UCHAR *)data, (ULONG)len);

        while ((g_ux_device_cdc_acm_write_sta == 1) && (timeout != 0))
        {
            timeout--;
            HAL_Delay(1);
        }
    }
}

void ux_device_cdc_acm_printf(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsprintf((char *)g_ux_device_cdc_acm_write_buffer, fmt, ap);
    va_end(ap);
    ux_device_cdc_acm_transmit(g_ux_device_cdc_acm_write_buffer, strlen((const char *)g_ux_device_cdc_acm_write_buffer));
}

static UINT ux_device_cdc_acm_parameter_write_callback(struct UX_SLAVE_CLASS_CDC_ACM_STRUCT *cdc_acm, UINT status, ULONG length)
{
    UX_PARAMETER_NOT_USED(cdc_acm);
    UX_PARAMETER_NOT_USED(status);
    UX_PARAMETER_NOT_USED(length);

    g_ux_device_cdc_acm_write_sta = 0;

    return UX_SUCCESS;
}

static UINT ux_device_cdc_acm_parameter_read_callback(struct UX_SLAVE_CLASS_CDC_ACM_STRUCT *cdc_acm, UINT status, UCHAR *data_pointer, ULONG length)
{
    UX_PARAMETER_NOT_USED(cdc_acm);
    UX_PARAMETER_NOT_USED(status);

    g_ux_device_cdc_acm_rx_buffer = (uint8_t *)data_pointer;
    g_ux_device_cdc_acm_rx_len = length;

    ux_device_class_cdc_acm_read_run(g_cdc_acm, (UCHAR *)&g_ux_device_cdc_acm_read_buffer, USBX_DEVICE_CDC_ACM_READ_BUFFER_SIZE, (ULONG *)&g_ux_device_cdc_acm_read_actual_length);

    return UX_SUCCESS;
}
