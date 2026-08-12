/**
 ****************************************************************************************************
 * @file        app_usbx_host.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_usbx_host.c文件
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

#include "app_usbx_host.h"
#include "ux_api.h"
#include "ux_host_class_storage.h"
#include "ux_hcd_stm32.h"
#include "ux_host_msc.h"
#include "main.h"

extern HCD_HandleTypeDef hhcd_USB_OTG_HS1;

static UCHAR usbx_host_memory_stack[USBX_HOST_MEMORY_STACK_SIZE];

static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance);

uint8_t app_usbx_host_init(void)
{
    if (ux_system_initialize(usbx_host_memory_stack, USBX_HOST_MEMORY_STACK_SIZE, UX_NULL, 0) != UX_SUCCESS)
    {
        return 1;
    }

    if (ux_host_stack_initialize(ux_host_event_callback) != UX_SUCCESS)
    {
        return 1;
    }

    if (ux_host_stack_class_register(_ux_system_host_class_storage_name, ux_host_class_storage_entry) != UX_SUCCESS)
    {
        return 1;
    }

    MX_USB1_OTG_HS_HCD_Init();
    ux_host_stack_hcd_register(_ux_system_host_hcd_stm32_name, _ux_hcd_stm32_initialize, USB1_OTG_HS_BASE, (ULONG)&hhcd_USB_OTG_HS1);
    HAL_HCD_Start(&hhcd_USB_OTG_HS1);

    return 0;
}

__weak void app_usbx_host_device_connect_callback(void)
{
    return;
}

__weak void app_usbx_host_device_disconnect_callback(void)
{
    return;
}

static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance)
{
    switch (event)
    {
        case UX_DEVICE_INSERTION:
        {
            if (current_class->ux_host_class_entry_function == ux_host_class_storage_entry)
            {
                ux_host_msc_read_write_init((UX_HOST_CLASS_STORAGE *)current_instance);
            }
            break;
        }
        case UX_DEVICE_REMOVAL:
        {
            if (current_class->ux_host_class_entry_function == ux_host_class_storage_entry)
            {
                ux_host_msc_read_write_deinit();
            }
            break;
        }
        case UX_STORAGE_MEDIA_INSERTION:
        {
            if (current_class->ux_host_class_entry_function == ux_host_class_storage_entry)
            {
                app_usbx_host_device_connect_callback();
            }
            break;
        }
        case UX_STORAGE_MEDIA_REMOVAL:
        {
            if (current_class->ux_host_class_entry_function == ux_host_class_storage_entry)
            {
                app_usbx_host_device_disconnect_callback();
            }
            break;
        }
        default:
        {
            break;
        }
    }

    return UX_SUCCESS;
}

ULONG _ux_utility_time_get(VOID)
{
    return (ULONG)HAL_GetTick();
}

ALIGN_TYPE _ux_utility_interrupt_disable(VOID)
{
    ALIGN_TYPE flags;

    flags = __get_PRIMASK();
    __disable_irq();

    return flags;
}

VOID _ux_utility_interrupt_restore(ALIGN_TYPE flags)
{
    __set_PRIMASK((uint32_t)flags);
}
