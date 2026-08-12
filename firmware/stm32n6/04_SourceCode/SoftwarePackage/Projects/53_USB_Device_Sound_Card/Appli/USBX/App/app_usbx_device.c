/**
 ****************************************************************************************************
 * @file        app_usbx_device.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_usbx_device.c文件
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

#include "app_usbx_device.h"
#include "ux_api.h"
#include "ux_device_descriptors.h"
#include "ux_device_audio.h"
#include "ux_dcd_stm32.h"
#include "main.h"

extern PCD_HandleTypeDef hpcd_USB_OTG_HS1;

static UCHAR usbx_device_memory_stack[USBX_DEVICE_MEMORY_STACK_SIZE];

uint8_t app_usbx_device_init(void)
{
    UCHAR *device_framework_high_speed;
    ULONG device_framework_hs_length;
    UCHAR *device_framework_full_speed;
    ULONG device_framework_fs_length;
    UCHAR *string_framework;
    ULONG string_framework_length;
    UCHAR *language_id_framework;
    ULONG language_id_framework_length;
    ULONG audio_configuration_number;
    ULONG audio_interface_number;
    UX_DEVICE_CLASS_AUDIO_STREAM_PARAMETER audio_stream_parameter;
    UX_DEVICE_CLASS_AUDIO_PARAMETER audio_parameter;

    if (ux_system_initialize(usbx_device_memory_stack, USBX_DEVICE_MEMORY_STACK_SIZE, UX_NULL, 0) != UX_SUCCESS)
    {
        return 1;
    }

    device_framework_high_speed = USBD_Get_Device_Framework_Speed(USBD_HIGH_SPEED, &device_framework_hs_length);
    device_framework_full_speed = USBD_Get_Device_Framework_Speed(USBD_FULL_SPEED, &device_framework_fs_length);
    string_framework = USBD_Get_String_Framework(&string_framework_length);
    language_id_framework = USBD_Get_Language_Id_Framework(&language_id_framework_length);
    if (ux_device_stack_initialize(device_framework_high_speed, device_framework_hs_length, device_framework_full_speed, device_framework_fs_length, string_framework, string_framework_length, language_id_framework, language_id_framework_length, UX_NULL) != UX_SUCCESS)
    {
        return 1;
    }

    audio_configuration_number = USBD_Get_Configuration_Number(CLASS_TYPE_AUDIO_20, 0);
    audio_interface_number = USBD_Get_Interface_Number(CLASS_TYPE_AUDIO_20, 0);
    audio_stream_parameter.ux_device_class_audio_stream_parameter_callbacks.ux_device_class_audio_stream_change = ux_device_audio_stream_change;
    audio_stream_parameter.ux_device_class_audio_stream_parameter_callbacks.ux_device_class_audio_stream_frame_done = ux_device_audio_stream_frame_done;
    audio_stream_parameter.ux_device_class_audio_stream_parameter_max_frame_buffer_nb = 3;
    audio_stream_parameter.ux_device_class_audio_stream_parameter_max_frame_buffer_size = USBD_AUDIO_PLAY_EPOUT_HS_MPS;
    audio_stream_parameter.ux_device_class_audio_stream_parameter_task_function = ux_device_class_audio_read_task_function;
    audio_parameter.ux_device_class_audio_parameter_streams_nb = 1;
    audio_parameter.ux_device_class_audio_parameter_streams = &audio_stream_parameter;
    audio_parameter.ux_device_class_audio_parameter_callbacks.ux_slave_class_audio_instance_activate = ux_device_audio_instance_activate;
    audio_parameter.ux_device_class_audio_parameter_callbacks.ux_slave_class_audio_instance_deactivate = ux_device_audio_instance_deactivate;
    audio_parameter.ux_device_class_audio_parameter_callbacks.ux_device_class_audio_control_process = ux_device_audio_control_process;
    if (ux_device_stack_class_register(_ux_system_slave_class_audio_name, ux_device_class_audio_entry, audio_configuration_number, audio_interface_number, &audio_parameter) != UX_SUCCESS)
    {
        return 1;
    }

    MX_USB1_OTG_HS_PCD_Init();
    ux_dcd_stm32_initialize((ULONG)USB1_OTG_HS, (ULONG)&hpcd_USB_OTG_HS1);
    HAL_PCD_Start(&hpcd_USB_OTG_HS1);

    return 0;
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
