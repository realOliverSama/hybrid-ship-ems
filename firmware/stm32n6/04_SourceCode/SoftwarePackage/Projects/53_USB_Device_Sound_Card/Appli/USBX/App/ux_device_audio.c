/**
 ****************************************************************************************************
 * @file        ux_device_audio.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       ux_device_audio.c文件
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

#include "ux_device_audio.h"
#include "ux_device_descriptors.h"
#include "./ES8388/es8388.h"

static void es8388_sai_tx_dma_cplt_user_cb(void);

#define AUDIO_FRAME_BUFFER_COUNT    10
#define AUDIO_FRAME_BUFFER_SIZE     57600
static uint8_t audio_frame_buffer[AUDIO_FRAME_BUFFER_COUNT][AUDIO_FRAME_BUFFER_SIZE];
static uint8_t audio_frame_buffer_reader = 0;
static uint8_t audio_frame_buffer_writer = 0;
static uint32_t audio_frame_buffer_write_offset = 0;

VOID ux_device_audio_stream_change(struct UX_DEVICE_CLASS_AUDIO_STREAM_STRUCT *stream, ULONG alternate_setting)
{
    if (alternate_setting != 0)
    {
        ux_device_class_audio_reception_start(stream);
        es8388_sai_tx_dma(&audio_frame_buffer[audio_frame_buffer_reader][0], AUDIO_FRAME_BUFFER_SIZE / 2);
    }
}

VOID ux_device_audio_stream_frame_done(struct UX_DEVICE_CLASS_AUDIO_STREAM_STRUCT *stream, ULONG length)
{
    UCHAR *frame_data;
    ULONG frame_length;

    ux_device_class_audio_read_frame_get(stream, &frame_data, &frame_length);

    if (length != 0)
    {
        ux_utility_memory_copy(&audio_frame_buffer[audio_frame_buffer_writer][audio_frame_buffer_write_offset], frame_data, frame_length);
        audio_frame_buffer_write_offset += frame_length;
        if (audio_frame_buffer_write_offset == AUDIO_FRAME_BUFFER_SIZE)
        {
            audio_frame_buffer_write_offset = 0;
            if (++audio_frame_buffer_writer == AUDIO_FRAME_BUFFER_COUNT)
            {
                audio_frame_buffer_writer = 0;
            }
        }
    }

    ux_device_class_audio_read_frame_free(stream);
}

VOID ux_device_audio_instance_activate(VOID *audio)
{
    UX_PARAMETER_NOT_USED(audio);

    es8388_adda_cfg(1, 0);
    es8388_output_cfg(1, 1);
    es8388_hpvol_set(9);
    es8388_spkvol_set(9);
    es8388_sai_cfg(0, 3);
    es8388_sai_clock_config(48000);
    es8388_sai_init_tx(2, 48000, 16);
    es8388_sai_tx_dma_cplt_user_cb_register(es8388_sai_tx_dma_cplt_user_cb);
}

VOID ux_device_audio_instance_deactivate(VOID *audio)
{
    UX_PARAMETER_NOT_USED(audio);
}

UINT ux_device_audio_control_process(struct UX_DEVICE_CLASS_AUDIO_STRUCT *audio, UX_SLAVE_TRANSFER *transfer)
{
    UINT status;
    UX_DEVICE_CLASS_AUDIO20_CONTROL audio_control = {0};
    UX_DEVICE_CLASS_AUDIO20_CONTROL_GROUP group;

    audio_control.ux_device_class_audio20_control_cs_id = USBD_AUDIO_PLAY_CLOCK_SOURCE_ID;
    audio_control.ux_device_class_audio20_control_sampling_frequency = USBD_AUDIO_PLAY_DEFAULT_FREQ;
    audio_control.ux_device_class_audio20_control_fu_id = USBD_AUDIO_PLAY_FEATURE_UNIT_ID;
    audio_control.ux_device_class_audio20_control_mute[0] = 0;
    audio_control.ux_device_class_audio20_control_volume_min[0] = 0;
    audio_control.ux_device_class_audio20_control_volume_max[0] = 100;
    audio_control.ux_device_class_audio20_control_volume_res[0] = 1;
    audio_control.ux_device_class_audio20_control_volume[0] = 30;
    group.ux_device_class_audio20_control_group_controls_nb = 1;
    group.ux_device_class_audio20_control_group_controls = &audio_control;
    status = ux_device_class_audio20_control_process(audio, transfer, &group);
    if (status != UX_SUCCESS)
    {
        return status;
    }

    switch (audio_control.ux_device_class_audio20_control_changed)
    {
        case UX_DEVICE_CLASS_AUDIO20_CONTROL_MUTE_CHANGED:
        {
            es8388_output_cfg(0, 0);
            break;
        }
        case UX_DEVICE_CLASS_AUDIO20_CONTROL_VOLUME_CHANGED:
        {
            es8388_output_cfg(1, 1);
            es8388_hpvol_set(audio_control.ux_device_class_audio20_control_volume[0] * 0.3);
            es8388_spkvol_set(audio_control.ux_device_class_audio20_control_volume[0] * 0.3);
            break;
        }
        case UX_DEVICE_CLASS_AUDIO20_CONTROL_FREQUENCY_CHANGED:
        default:
        {
            break;
        }
    }

    return status;
}

static void es8388_sai_tx_dma_cplt_user_cb(void)
{
    if (++audio_frame_buffer_reader == AUDIO_FRAME_BUFFER_COUNT)
    {
        audio_frame_buffer_reader = 0;
    }
    es8388_sai_tx_dma(&audio_frame_buffer[audio_frame_buffer_reader][0], AUDIO_FRAME_BUFFER_SIZE / 2);
}
