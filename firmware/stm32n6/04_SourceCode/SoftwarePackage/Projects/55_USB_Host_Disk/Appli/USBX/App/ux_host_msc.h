/**
 ****************************************************************************************************
 * @file        ux_host_msc.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_host_msc.h文件
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

#ifndef __UX_HOST_MSC_H
#define __UX_HOST_MSC_H

#include "ux_api.h"
#include "ux_host_class_storage.h"

void ux_host_msc_read_write_init(UX_HOST_CLASS_STORAGE *current_instance);
void ux_host_msc_read_write_deinit(void);
uint8_t ux_host_msc_disk_init(void);
uint8_t ux_host_msc_disk_read(uint8_t *buffer, uint32_t sector, uint32_t count);
uint8_t ux_host_msc_disk_write(uint8_t *buffer, uint32_t sector, uint32_t count);
uint32_t ux_host_msc_disk_get_sector_size(void);
uint32_t ux_host_msc_disk_get_sector_number(void);

#endif
