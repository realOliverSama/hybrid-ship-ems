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
#include "ux_device_msc.h"
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
    UX_SLAVE_CLASS_STORAGE_PARAMETER storage_parameter = {0};
    ULONG storage_configuration_number;
    ULONG storage_interface_number;

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

    storage_parameter.ux_slave_class_storage_instance_activate = ux_slave_msc_storage_instance_activate;
    storage_parameter.ux_slave_class_storage_instance_deactivate = ux_slave_msc_storage_instance_deactivate;
    storage_parameter.ux_slave_class_storage_parameter_number_lun = STORAGE_NUMBER_LUN;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_last_lba = ux_slave_msc_storage_media_last_lba(SDNAND_LUN);
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_block_length = ux_slave_msc_storage_media_block_length(SDNAND_LUN);
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_type = UX_SLAVE_CLASS_STORAGE_MEDIA_FAT_DISK;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_removable_flag = 0x80;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_read_only_flag = UX_FALSE;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_read = ux_slave_msc_storage_media_read;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_write = ux_slave_msc_storage_media_write;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_flush = ux_slave_msc_storage_media_flush;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_status = ux_slave_msc_storage_media_status;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDNAND_LUN].ux_slave_class_storage_media_notification = ux_slave_msc_storage_media_notification;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_last_lba = ux_slave_msc_storage_media_last_lba(SDCARD_LUN);
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_block_length = ux_slave_msc_storage_media_block_length(SDCARD_LUN);
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_type = UX_SLAVE_CLASS_STORAGE_MEDIA_FAT_DISK;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_removable_flag = 0x80;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_read_only_flag = UX_FALSE;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_read = ux_slave_msc_storage_media_read;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_write = ux_slave_msc_storage_media_write;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_flush = ux_slave_msc_storage_media_flush;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_status = ux_slave_msc_storage_media_status;
    storage_parameter.ux_slave_class_storage_parameter_lun[SDCARD_LUN].ux_slave_class_storage_media_notification = ux_slave_msc_storage_media_notification;
    storage_configuration_number = USBD_Get_Configuration_Number(CLASS_TYPE_MSC, 0);
    storage_interface_number = USBD_Get_Interface_Number(CLASS_TYPE_MSC, 0);
    if (ux_device_stack_class_register(_ux_system_slave_class_storage_name, ux_device_class_storage_entry, storage_configuration_number, storage_interface_number, &storage_parameter) != UX_SUCCESS)
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
