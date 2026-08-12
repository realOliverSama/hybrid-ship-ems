/**
 ****************************************************************************************************
 * @file        ux_device_msc.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_device_msc.h文件
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

#ifndef __UX_DEVICE_MSC_H
#define __UX_DEVICE_MSC_H

#include "ux_api.h"

enum {
    SDNAND_LUN = 0,
    SDCARD_LUN,
    STORAGE_NUMBER_LUN,
};

VOID ux_slave_msc_storage_instance_activate(VOID *storage);
VOID ux_slave_msc_storage_instance_deactivate(VOID *storage);
ULONG ux_slave_msc_storage_media_last_lba(ULONG lun);
ULONG ux_slave_msc_storage_media_block_length(ULONG lun);
UINT ux_slave_msc_storage_media_read(VOID *storage, ULONG lun, UCHAR *data_pointer, ULONG number_blocks, ULONG lba, ULONG *media_status);
UINT ux_slave_msc_storage_media_write(VOID *storage, ULONG lun, UCHAR *data_pointer, ULONG number_blocks, ULONG lba, ULONG *media_status);
UINT ux_slave_msc_storage_media_flush(VOID *storage, ULONG lun, ULONG number_blocks, ULONG lba, ULONG *media_status);
UINT ux_slave_msc_storage_media_status(VOID *storage, ULONG lun, ULONG media_id, ULONG *media_status);
UINT ux_slave_msc_storage_media_notification(VOID *storage, ULONG lun, ULONG media_id, ULONG notification_class, UCHAR **media_notification, ULONG *media_notification_length);

#endif
