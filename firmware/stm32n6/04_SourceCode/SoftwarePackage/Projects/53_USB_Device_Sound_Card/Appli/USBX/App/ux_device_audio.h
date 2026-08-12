/**
 ****************************************************************************************************
 * @file        ux_device_audio.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_device_audio.h文件
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

#ifndef __UX_DEVICE_AUDIO_H
#define __UX_DEVICE_AUDIO_H

#include "ux_api.h"
#include "ux_device_class_audio.h"

VOID ux_device_audio_stream_change(struct UX_DEVICE_CLASS_AUDIO_STREAM_STRUCT *stream, ULONG alternate_setting);
VOID ux_device_audio_stream_frame_done(struct UX_DEVICE_CLASS_AUDIO_STREAM_STRUCT *stream, ULONG length);
VOID ux_device_audio_instance_activate(VOID *audio);
VOID ux_device_audio_instance_deactivate(VOID *audio);
UINT ux_device_audio_control_process(struct UX_DEVICE_CLASS_AUDIO_STRUCT *audio, UX_SLAVE_TRANSFER *transfer);

#endif
