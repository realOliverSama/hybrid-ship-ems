/**
 ****************************************************************************************************
 * @file        app.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app.c文件
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

#include "app.h"
#include "app_config.h"
#include "app_lcd.h"
#include "app_camera.h"
#include "app_postprocess.h"
#include "ll_aton_runtime.h"

static const char *nn_classes_table[NN_CLASSES] = NN_CLASSES_TABLE;
static volatile uint32_t app_nn_frame_count = 0;
static const uint32_t colors[] = {
    UTIL_LCD_COLOR_GREEN,
    UTIL_LCD_COLOR_RED,
    UTIL_LCD_COLOR_CYAN,
    UTIL_LCD_COLOR_MAGENTA,
    UTIL_LCD_COLOR_YELLOW,
    UTIL_LCD_COLOR_GRAY,
    UTIL_LCD_COLOR_BLACK,
    UTIL_LCD_COLOR_BROWN,
    UTIL_LCD_COLOR_BLUE,
    UTIL_LCD_COLOR_ORANGE
};

static void app_camera_nn_pipe_frame_cb(void);
static void app_display_network_output(iseg_postprocess_out_t *postprocess, uint32_t inference_ms);

void app_run(void)
{
    LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default);
    const LL_Buffer_InfoTypeDef *nn_in_info = LL_ATON_Input_Buffers_Info_Default();
    const LL_Buffer_InfoTypeDef *nn_out_info = LL_ATON_Output_Buffers_Info_Default();
    uint8_t *nn_in;
    float_t *nn_out[NN_OUTPUT_NUMBER];
    uint32_t nn_out_len[NN_OUTPUT_NUMBER];
    yolov8_seg_pp_static_param_t pp_params;
    uint8_t i;
    uint32_t time_stamp[2];
    iseg_postprocess_out_t pp_output;

    nn_in = (uint8_t *)LL_Buffer_addr_start(&nn_in_info[0]);

    for (i = 0; i < NN_OUTPUT_NUMBER; i++)
    {
        nn_out[i] = (float_t *)LL_Buffer_addr_start(&nn_out_info[i]);
        nn_out_len[i] = LL_Buffer_len(&nn_out_info[i]);
    }

    app_lcd_init();
    app_camera_init(NULL, NULL, NULL, app_camera_nn_pipe_frame_cb);

    app_camera_display_pipe_start(app_lcd_get_bg_buffer(), CMW_MODE_CONTINUOUS);

    app_postprocess_init(&pp_params);

    while (1)
    {
        app_camera_isp_update();

        app_camera_nn_pipe_start(nn_in, CMW_MODE_SNAPSHOT);
        while (app_nn_frame_count == 0);
        app_nn_frame_count = 0;

        time_stamp[0] = HAL_GetTick();
        LL_ATON_RT_Main(&NN_Instance_Default);
        time_stamp[1] = HAL_GetTick();

        app_postprocess_run((void **)nn_out, NN_OUTPUT_NUMBER, &pp_output, &pp_params);
        app_display_network_output(&pp_output, time_stamp[1] - time_stamp[0]);

        for (i = 0; i < NN_OUTPUT_NUMBER; i++)
        {
            SCB_InvalidateDCache_by_Addr(nn_out[i], nn_out_len[i]);
        }
    }
}

static void app_camera_nn_pipe_frame_cb(void)
{
    app_nn_frame_count++;
}

static void app_display_network_output(iseg_postprocess_out_t *postprocess, uint32_t inference_ms)
{
    int32_t i;
    uint32_t x;
    uint32_t y;
    app_rectangle_t *lcd_bg_area;
    app_rectangle_t rect;
    uint32_t color;
    uint8_t line_nb = 0;

    app_lcd_draw_area_update();

    UTIL_LCD_FillRect(0, 0, LCD_FG_WIDTH, LCD_FG_HEIGHT, 0x00000000);

    lcd_bg_area = app_lcd_get_bg_area();

    for (i = 0; i < postprocess->nb_detect; i++)
    {
        for (x = 0; x < AI_YOLOV8_SEG_PP_MASK_SIZE; x++)
        {
            for (y = 0; y < AI_YOLOV8_SEG_PP_MASK_SIZE; y++)
            {
                if (postprocess->pOutBuff[i].pMask[y * AI_YOLOV8_SEG_PP_MASK_SIZE + x] > 0.1f)
                {
                    rect.x_pos = lcd_bg_area->x_pos + x * lcd_bg_area->width / AI_YOLOV8_SEG_PP_MASK_SIZE;
                    rect.y_pos = lcd_bg_area->y_pos + y * lcd_bg_area->height / AI_YOLOV8_SEG_PP_MASK_SIZE;
                    rect.width = lcd_bg_area->width / AI_YOLOV8_SEG_PP_MASK_SIZE + 1;
                    rect.height = lcd_bg_area->height / AI_YOLOV8_SEG_PP_MASK_SIZE + 1;
                    color = colors[i % (sizeof(colors) / sizeof(colors[0]))] & 0x40FFFFFF;
                    UTIL_LCD_FillRect(rect.x_pos, rect.y_pos, rect.width, rect.height, color);
                }
            }
        }
    }

    for (i = 0; i < postprocess->nb_detect; i++)
    {
        rect.x_pos = (uint32_t)((postprocess->pOutBuff[i].x_center - postprocess->pOutBuff[i].width / 2) * ((float32_t)lcd_bg_area->width)) + lcd_bg_area->x_pos;
        rect.y_pos = (uint32_t)((postprocess->pOutBuff[i].y_center - postprocess->pOutBuff[i].height / 2) * ((float32_t)lcd_bg_area->height)) + lcd_bg_area->y_pos;
        rect.width = (uint32_t)(postprocess->pOutBuff[i].width * ((float32_t)lcd_bg_area->width));
        rect.height = (uint32_t)(postprocess->pOutBuff[i].height * ((float32_t)lcd_bg_area->height));
        rect.x_pos = (rect.x_pos < (lcd_bg_area->x_pos + lcd_bg_area->width)) ? (rect.x_pos) : (lcd_bg_area->x_pos + lcd_bg_area->width - 1);
        rect.y_pos = (rect.y_pos < (lcd_bg_area->y_pos + lcd_bg_area->height)) ? (rect.y_pos) : (lcd_bg_area->y_pos + lcd_bg_area->height  - 1);
        rect.width = ((rect.x_pos + rect.width) < (lcd_bg_area->x_pos + lcd_bg_area->width)) ? (rect.width) : (lcd_bg_area->x_pos + lcd_bg_area->width - rect.x_pos - 1);
        rect.height = ((rect.y_pos + rect.height) < (lcd_bg_area->y_pos + lcd_bg_area->height)) ? (rect.height) : (lcd_bg_area->y_pos + lcd_bg_area->height - rect.y_pos - 1);
        color = colors[i % (sizeof(colors) / sizeof(colors[0]))];
        UTIL_LCD_DrawRect(rect.x_pos, rect.y_pos, rect.width, rect.height, color);
        UTIL_LCDEx_PrintfAt(rect.x_pos, rect.y_pos, LEFT_MODE, nn_classes_table[postprocess->pOutBuff[i].class_index]);
    }

    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Inference");
    line_nb += 1;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "%ums", inference_ms);
    line_nb += 2;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Objects");
    line_nb += 1;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "%u", postprocess->nb_detect);

    app_lcd_draw_area_commit();
}
