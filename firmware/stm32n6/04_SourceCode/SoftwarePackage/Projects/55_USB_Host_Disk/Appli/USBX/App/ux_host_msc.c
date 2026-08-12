/**
 ****************************************************************************************************
 * @file        ux_host_msc.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_host_msc.c文件
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

#include "ux_host_msc.h"

static UX_HOST_CLASS_STORAGE *g_storage = UX_NULL;

void ux_host_msc_read_write_init(UX_HOST_CLASS_STORAGE *current_instance)
{
    g_storage = current_instance;
}

void ux_host_msc_read_write_deinit(void)
{
    g_storage = UX_NULL;
}

uint8_t ux_host_msc_disk_init(void)
{
    if (g_storage == UX_NULL)
    {
        return 1;
    }

    return 0;
}

uint8_t ux_host_msc_disk_read(uint8_t *buffer, uint32_t sector, uint32_t count)
{
    if (g_storage == UX_NULL)
    {
        return 1;
    }

    if (ux_host_class_storage_media_read(g_storage, (ULONG)sector, (ULONG)count, (UCHAR *)buffer) != UX_SUCCESS)
    {
        return 1;
    }

    return 0;
}

uint8_t ux_host_msc_disk_write(uint8_t *buffer, uint32_t sector, uint32_t count)
{
    if (g_storage == UX_NULL)
    {
        return 1;
    }

    if (ux_host_class_storage_media_write(g_storage, (ULONG)sector, (ULONG)count, (UCHAR *)buffer) != UX_SUCCESS)
    {
        return 1;
    }

    return 0;
}

uint32_t ux_host_msc_disk_get_sector_size(void)
{
    if (g_storage == UX_NULL)
    {
        return 0;
    }

    return (uint32_t)g_storage->ux_host_class_storage_sector_size;
}

uint32_t ux_host_msc_disk_get_sector_number(void)
{
    if (g_storage == UX_NULL)
    {
        return 0;
    }

    return (uint32_t)g_storage->ux_host_class_storage_last_sector_number;
}
