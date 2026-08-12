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
#include "ux_hcd_stm32.h"
#include "ux_host_hid_mouse.h"
#include "ux_host_hid_keyboard.h"
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

    if (ux_host_stack_class_register(_ux_system_host_class_hid_name, ux_host_class_hid_entry) != UX_SUCCESS)
    {
        return 1;
    }

    if (ux_host_class_hid_client_register(_ux_system_host_class_hid_client_mouse_name, ux_host_class_hid_mouse_entry) != UX_SUCCESS)
    {
        return 1;
    }

    if (ux_host_class_hid_client_register(_ux_system_host_class_hid_client_keyboard_name, ux_host_class_hid_keyboard_entry) != UX_SUCCESS)
    {
        return 1;
    }

    MX_USB1_OTG_HS_HCD_Init();
    ux_host_stack_hcd_register(_ux_system_host_hcd_stm32_name, _ux_hcd_stm32_initialize, USB1_OTG_HS_BASE, (ULONG)&hhcd_USB_OTG_HS1);
    HAL_HCD_Start(&hhcd_USB_OTG_HS1);

    return 0;
}

static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance)
{
    switch (event)
    {
        case UX_HID_CLIENT_INSERTION:
        {
            if (((UX_HOST_CLASS_HID_CLIENT *)current_instance)->ux_host_class_hid_client_handler == ux_host_class_hid_mouse_entry)
            {
                mouse = (UX_HOST_CLASS_HID_MOUSE *)((UX_HOST_CLASS_HID_CLIENT *)current_instance)->ux_host_class_hid_client_local_instance;
            }
            else if (((UX_HOST_CLASS_HID_CLIENT *)current_instance)->ux_host_class_hid_client_handler == ux_host_class_hid_keyboard_entry)
            {
                keyboard = (UX_HOST_CLASS_HID_KEYBOARD *)((UX_HOST_CLASS_HID_CLIENT *)current_instance)->ux_host_class_hid_client_local_instance;
            }
            break;
        }
        case UX_HID_CLIENT_REMOVAL:
        {
            if (((UX_HOST_CLASS_HID_CLIENT *)current_instance)->ux_host_class_hid_client_handler == ux_host_class_hid_mouse_entry)
            {
                mouse = UX_NULL;
            }
            else if (((UX_HOST_CLASS_HID_CLIENT *)current_instance)->ux_host_class_hid_client_handler == ux_host_class_hid_keyboard_entry)
            {
                keyboard = UX_NULL;
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
