/**
 ****************************************************************************************************
 * @file        ux_device_msc.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_device_msc.c文件
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

#include "ux_device_msc.h"
#include "./SD_NAND/sd_nand.h"
#include "./SD_CARD/sd_card.h"
#include "./UART/uart.h"

VOID ux_slave_msc_storage_instance_activate(VOID *storage)
{
    UX_PARAMETER_NOT_USED(storage);
}

VOID ux_slave_msc_storage_instance_deactivate(VOID *storage)
{
    UX_PARAMETER_NOT_USED(storage);
}

ULONG ux_slave_msc_storage_media_last_lba(ULONG lun)
{
    ULONG last_lba;

    switch (lun)
    {
        case SDNAND_LUN:
        {
            last_lba = g_sd_nand_info_struct.LogBlockNbr - SD_NAND_FONT_BLK_NUM - 1;
            break;
        }
        case SDCARD_LUN:
        {
            last_lba = g_sd_card_info_struct.LogBlockNbr - 1;
            break;
        }
        default:
        {
            last_lba = 0;
            break;
        }
    }

    return last_lba;
}

ULONG ux_slave_msc_storage_media_block_length(ULONG lun)
{
    ULONG block_length;

    switch (lun)
    {
        case SDNAND_LUN:
        {
            block_length = g_sd_nand_info_struct.LogBlockSize;
            break;
        }
        case SDCARD_LUN:
        {
            block_length = g_sd_card_info_struct.LogBlockSize;
            break;
        }
        default:
        {
            block_length = 0;
            break;
        }
    }

    return block_length;
}

UINT ux_slave_msc_storage_media_read(VOID *storage, ULONG lun, UCHAR *data_pointer, ULONG number_blocks, ULONG lba, ULONG *media_status)
{
    UX_INTERRUPT_SAVE_AREA

    UX_PARAMETER_NOT_USED(storage);
    UX_PARAMETER_NOT_USED(media_status);

    UX_DISABLE

    switch (lun)
    {
        case SDNAND_LUN:
        {
            sd_nand_read_disk(data_pointer, lba, number_blocks);
            break;
        }
        case SDCARD_LUN:
        {
            sd_card_read_disk(data_pointer, lba, number_blocks);
            break;
        }
        default:
        {
            break;
        }
    }

    UX_RESTORE

    return UX_STATE_NEXT;
}

UINT ux_slave_msc_storage_media_write(VOID *storage, ULONG lun, UCHAR *data_pointer, ULONG number_blocks, ULONG lba, ULONG *media_status)
{
    UX_INTERRUPT_SAVE_AREA

    UX_PARAMETER_NOT_USED(storage);
    UX_PARAMETER_NOT_USED(media_status);

    UX_DISABLE

    switch (lun)
    {
        case SDNAND_LUN:
        {
            sd_nand_write_disk(data_pointer, lba, number_blocks);
            break;
        }
        case SDCARD_LUN:
        {
            sd_card_write_disk(data_pointer, lba, number_blocks);
            break;
        }
        default:
        {
            break;
        }
    }

    UX_RESTORE

    return UX_STATE_NEXT;
}

UINT ux_slave_msc_storage_media_flush(VOID *storage, ULONG lun, ULONG number_blocks, ULONG lba, ULONG *media_status)
{
    UX_PARAMETER_NOT_USED(storage);
    UX_PARAMETER_NOT_USED(lun);
    UX_PARAMETER_NOT_USED(number_blocks);
    UX_PARAMETER_NOT_USED(lba);
    UX_PARAMETER_NOT_USED(media_status);

    return UX_STATE_NEXT;
}

UINT ux_slave_msc_storage_media_status(VOID *storage, ULONG lun, ULONG media_id, ULONG *media_status)
{
    UX_PARAMETER_NOT_USED(storage);
    UX_PARAMETER_NOT_USED(lun);
    UX_PARAMETER_NOT_USED(media_id);
    UX_PARAMETER_NOT_USED(media_status);

    return UX_SUCCESS;
}

UINT ux_slave_msc_storage_media_notification(VOID *storage, ULONG lun, ULONG media_id, ULONG notification_class, UCHAR **media_notification, ULONG *media_notification_length)
{
    UX_PARAMETER_NOT_USED(storage);
    UX_PARAMETER_NOT_USED(lun);
    UX_PARAMETER_NOT_USED(media_id);
    UX_PARAMETER_NOT_USED(notification_class);
    UX_PARAMETER_NOT_USED(media_notification);
    UX_PARAMETER_NOT_USED(media_notification_length);

    return UX_SUCCESS;
}
