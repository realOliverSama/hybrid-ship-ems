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
#include "app_utils.h"
#include "app_lcd.h"
#include "app_camera.h"
#include "app_bqueue.h"
#include "app_cpuload.h"
#include "app_postprocess.h"
#include "tx_api.h"
#include "cmw_camera.h"
#include "ll_aton_runtime.h"

typedef struct {
    int32_t nb_detect;
    mpe_pp_outBuffer_t detects[AI_MPE_YOLOV8_PP_MAX_BOXES_LIMIT];
    uint32_t nn_period_ms;
    uint32_t inf_ms;
    uint32_t pp_ms;
    uint32_t disp_ms;
} app_display_info_t;

typedef struct {
    TX_SEMAPHORE update;
    TX_MUTEX lock;
    app_display_info_t info;
} app_display_t;

static TX_SEMAPHORE isp_semaphore;

static void app_camera_display_pipe_vsync_cb(void);
static void app_camera_display_pipe_frame_cb(void);
static void app_camera_nn_pipe_frame_cb(void);

static TX_THREAD nn_thread;
static UCHAR nn_thread_stack[4096];
static TX_THREAD pp_thread;
static UCHAR pp_thread_stack[4096];
static TX_THREAD dp_thread;
static UCHAR dp_thread_stack[4096];
static TX_THREAD isp_thread;
static UCHAR isp_thread_stack[4096];

static VOID nn_thread_entry(ULONG id);
static VOID pp_thread_entry(ULONG id);
static VOID dp_thread_entry(ULONG id);
static VOID isp_thread_entry(ULONG id);

static const uint32_t bindings[][3] = {
    {15, 13, COLOR_LEGS},
    {13, 11, COLOR_LEGS},
    {16, 14, COLOR_LEGS},
    {14, 12, COLOR_LEGS},
    {11, 12, COLOR_TRUNK},
    {5, 11, COLOR_TRUNK},
    {6, 12, COLOR_TRUNK},
    {5, 6, COLOR_ARMS},
    {5, 7, COLOR_ARMS},
    {6, 8, COLOR_ARMS},
    {7, 9, COLOR_ARMS},
    {8, 10, COLOR_ARMS},
    {1, 2, COLOR_HEAD},
    {0, 1, COLOR_HEAD},
    {0, 2, COLOR_HEAD},
    {1, 3, COLOR_HEAD},
    {2, 4, COLOR_HEAD},
    {3, 5, COLOR_HEAD},
    {4, 6, COLOR_HEAD},
};

static const uint32_t kp_color[AI_POSE_PP_POSE_KEYPOINTS_NB] = {
    COLOR_HEAD,
    COLOR_HEAD,
    COLOR_HEAD,
    COLOR_HEAD,
    COLOR_HEAD,
    COLOR_ARMS,
    COLOR_ARMS,
    COLOR_ARMS,
    COLOR_ARMS,
    COLOR_ARMS,
    COLOR_ARMS,
    COLOR_TRUNK,
    COLOR_TRUNK,
    COLOR_LEGS,
    COLOR_LEGS,
    COLOR_LEGS,
    COLOR_LEGS,
};

static app_display_t display;

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default);
static uint8_t nn_input_buffers[3][NN_WIDTH * NN_HEIGHT * NN_BPP] __attribute__((aligned(32))) __attribute__((section(".EXTRAM")));
static app_bqueue_t nn_input_queue;
static uint8_t nn_output_buffers[2][ALIGN_VALUE(NN_BUFFER_OUT_SIZE, 32)] __attribute__((aligned(32)));
static app_bqueue_t nn_output_queue;
static const char *nn_classes_table[NN_CLASSES] = NN_CLASSES_TABLE;

static app_cpuload_t cpuload;

static void app_display_network_output(app_display_info_t *display_info);

void app_run(void)
{
    app_lcd_init();
    app_bqueue_init(&nn_input_queue, 3, (uint8_t *[3]){nn_input_buffers[0], nn_input_buffers[1], nn_input_buffers[2]});
    app_bqueue_init(&nn_output_queue, 2, (uint8_t *[2]){nn_output_buffers[0], nn_output_buffers[1]});
    app_cpuload_init(&cpuload);
    app_camera_init(app_camera_display_pipe_vsync_cb, app_camera_display_pipe_frame_cb, NULL, app_camera_nn_pipe_frame_cb);

    tx_semaphore_create(&isp_semaphore, NULL, 0);
    tx_semaphore_create(&display.update, NULL, 0);
    tx_mutex_create(&display.lock, NULL, TX_INHERIT);

    app_camera_display_pipe_start(app_lcd_get_bg_buffer(), CMW_MODE_CONTINUOUS);

    tx_thread_create(&nn_thread, "NN Thread", nn_thread_entry, 0, nn_thread_stack, sizeof(nn_thread_stack), TX_MAX_PRIORITIES - 3, TX_MAX_PRIORITIES - 3, 10, TX_AUTO_START);
    tx_thread_create(&pp_thread, "PP Thread", pp_thread_entry, 0, pp_thread_stack, sizeof(pp_thread_stack), TX_MAX_PRIORITIES - 2, TX_MAX_PRIORITIES - 2, 10, TX_AUTO_START);
    tx_thread_create(&dp_thread, "DP Thread", dp_thread_entry, 0, dp_thread_stack, sizeof(dp_thread_stack), TX_MAX_PRIORITIES - 2, TX_MAX_PRIORITIES - 2, 10, TX_AUTO_START);
    tx_thread_create(&isp_thread, "ISP Thread", isp_thread_entry, 0, isp_thread_stack, sizeof(isp_thread_stack), TX_MAX_PRIORITIES - 4, TX_MAX_PRIORITIES - 4, 10, TX_AUTO_START);
}

static void app_camera_display_pipe_vsync_cb(void)
{
    tx_semaphore_put(&isp_semaphore);
}

static void app_camera_display_pipe_frame_cb(void)
{
    app_lcd_switch_bg_buffer();
    app_camera_display_pipe_set_address(app_lcd_get_bg_buffer());
}

static void app_camera_nn_pipe_frame_cb(void)
{
    uint8_t *buffer;

    buffer = app_bqueue_get_free(&nn_input_queue, 0);
    if (buffer != NULL)
    {
        app_camera_nn_pipe_set_address(buffer);
        app_bqueue_put_ready(&nn_input_queue);
    }
}

static VOID nn_thread_entry(ULONG id)
{
    uint32_t nn_out_len;
    uint32_t nn_in_len;
    uint8_t *nn_pipe_dst;
    uint8_t *capture_buffer;
    uint8_t *output_buffer;
    uint32_t nn_period[2];
    uint32_t nn_period_ms;
    uint32_t time_stamp;
    uint32_t inf_ms;

    nn_in_len = LL_Buffer_len(LL_ATON_Input_Buffers_Info_Default());
    nn_out_len = LL_Buffer_len(LL_ATON_Output_Buffers_Info_Default());

    nn_period[1] = HAL_GetTick();

    nn_pipe_dst = app_bqueue_get_free(&nn_input_queue, 0);

    app_camera_nn_pipe_start(nn_pipe_dst, CMW_MODE_CONTINUOUS);

    while (1)
    {
        nn_period[0] = nn_period[1];
        nn_period[1] = HAL_GetTick();
        nn_period_ms = nn_period[1] - nn_period[0];

        capture_buffer = app_bqueue_get_ready(&nn_input_queue);
        output_buffer = app_bqueue_get_free(&nn_output_queue, 1);

        time_stamp = HAL_GetTick();
        LL_ATON_Set_User_Input_Buffer_Default(0, capture_buffer, nn_in_len);
        SCB_InvalidateDCache_by_Addr(output_buffer, nn_out_len);
        LL_ATON_Set_User_Output_Buffer_Default(0, output_buffer, nn_out_len);
        LL_ATON_RT_Main(&NN_Instance_Default);
        inf_ms = HAL_GetTick() - time_stamp;

        app_bqueue_put_free(&nn_input_queue);
        app_bqueue_put_ready(&nn_output_queue);

        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.inf_ms = inf_ms;
        display.info.nn_period_ms = nn_period_ms;
        tx_mutex_put(&display.lock);
    }
}

static VOID pp_thread_entry(ULONG id)
{
    mpe_yolov8_pp_static_param_t pp_params;
    uint8_t *output_buffer;
    mpe_pp_out_t pp_output;
    uint32_t nn_pp[2];
    int32_t i;

    app_postprocess_init(&pp_params);

    while (1)
    {
        output_buffer = app_bqueue_get_ready(&nn_output_queue);
        pp_output.pOutBuff = NULL;

        nn_pp[0] = HAL_GetTick();
        app_postprocess_run((void *[]){(void *)output_buffer}, 1, &pp_output, &pp_params);
        nn_pp[1] = HAL_GetTick();

        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display.info.nb_detect = pp_output.nb_detect;
        for (i = 0; i < pp_output.nb_detect; i++)
        {
            display.info.detects[i] = pp_output.pOutBuff[i];
        }
        display.info.pp_ms = nn_pp[1] - nn_pp[0];
        tx_mutex_put(&display.lock);
        app_bqueue_put_free(&nn_output_queue);
        tx_semaphore_ceiling_put(&display.update, 1);
    }
}

static VOID dp_thread_entry(ULONG id)
{
    uint32_t disp_ms = 0;
    app_display_info_t display_info;
    uint32_t time_stamp;

    while (1)
    {
        tx_semaphore_get(&display.update, TX_WAIT_FOREVER);
        tx_mutex_get(&display.lock, TX_WAIT_FOREVER);
        display_info = display.info;
        tx_mutex_put(&display.lock);
        display_info.disp_ms = disp_ms;

        time_stamp = HAL_GetTick();
        app_display_network_output(&display_info);
        disp_ms = HAL_GetTick() - time_stamp;
    }
}

static VOID isp_thread_entry(ULONG id)
{
    while (1)
    {
        tx_semaphore_get(&isp_semaphore, TX_WAIT_FOREVER);

        app_camera_isp_update();
    }
}

static void app_convert_length(float32_t wi, float32_t hi, int32_t *wo, int32_t *ho)
{
    *wo = (int32_t)(LCD_BG_WIDTH * wi);
    *ho = (int32_t)(LCD_BG_HEIGHT * hi);
}

static void app_convert_point(float32_t xi, float32_t yi, int32_t *xo, int32_t *yo)
{
    *xo = (int32_t)(LCD_BG_WIDTH * xi);
    *yo = (int32_t)(LCD_BG_HEIGHT * yi);
}

static uint8_t app_clamp_point(int32_t *x, int32_t *y)
{
    int32_t xi;
    int32_t yi;

    xi = *x;
    yi = *y;

    if (*x < 0)
    {
        *x = 0;
    }

    if (*y < 0)
    {
        *y = 0;
    }

    if (*x >= LCD_BG_WIDTH)
    {
        *x = LCD_BG_WIDTH - 1;
    }

    if (*y >= LCD_BG_HEIGHT)
    {
        *y = LCD_BG_HEIGHT - 1;
    }

    return (xi != *x) || (yi != *y);
}

static void app_display_binding_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    app_clamp_point(&x0, &y0);
    app_clamp_point(&x1, &y1);

    UTIL_LCD_DrawLine(x0, y0, x1, y1, color);
}

static void app_display_binding(mpe_pp_keyPoints_t *from, mpe_pp_keyPoints_t *to, uint32_t color)
{
    int32_t x0, y0;
    int32_t x1, y1;
    int i;

    if ((from->conf < AI_MPE_YOLOV8_PP_CONF_THRESHOLD) || (to->conf < AI_MPE_YOLOV8_PP_CONF_THRESHOLD))
    {
        return;
    }

    app_convert_point(from->x, from->y, &x0, &y0);
    if (app_clamp_point(&x0, &y0) != 0)
    {
        return;
    }

    app_convert_point(to->x, to->y, &x1, &y1);
    if (app_clamp_point(&x1, &y1) != 0)
    {
        return;
    }

    UTIL_LCD_DrawLine(x0, y0, x1, y1, color);
    for (i = 1; i <= (BINDING_WIDTH - 1) / 2; i++)
    {
        if (abs(y1 - y0) > abs(x1 - x0))
        {
            app_display_binding_line(x0 + i, y0, x1 + i , y1, color);
            app_display_binding_line(x0 - i, y0, x1 - i , y1, color);
        }
        else
        {
            app_display_binding_line(x0, y0 + i, x1 , y1 + i, color);
            app_display_binding_line(x0, y0 - i, x1 , y1 - i, color);
        }
    }
}

static void app_display_keypoint(mpe_pp_keyPoints_t *key, uint32_t color)
{
    int32_t x;
    int32_t y;
    int32_t xc;
    int32_t yc;

    if (key->conf < AI_MPE_YOLOV8_PP_CONF_THRESHOLD)
    {
        return;
    }

    app_convert_point(key->x, key->y, &x, &y);

    xc = x - CIRCLE_RADIUS / 2;
    yc = y - CIRCLE_RADIUS / 2;
    if (app_clamp_point(&xc, &yc) != 0)
    {
        return;
    }

    xc = x + CIRCLE_RADIUS / 2;
    yc = y + CIRCLE_RADIUS / 2;
    if (app_clamp_point(&xc, &yc) != 0)
    {
        return;
    }

    UTIL_LCD_FillCircle(x, y, CIRCLE_RADIUS, color);
}

static void app_display_detection(mpe_pp_outBuffer_t *detect)
{
    int32_t xc;
    int32_t yc;
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    int32_t w;
    int32_t h;
    uint8_t i;

    app_convert_point(detect->x_center, detect->y_center, &xc, &yc);
    app_convert_length(detect->width, detect->height, &w, &h);

    x0 = xc - (w + 1) / 2;
    y0 = yc - (h + 1) / 2;
    x1 = xc + (w + 1) / 2;
    y1 = yc + (h + 1) / 2;

    app_clamp_point(&x0, &y0);
    app_clamp_point(&x1, &y1);

    UTIL_LCD_DrawRect(x0, y0, x1 - x0, y1 - y0, UTIL_LCD_COLOR_GREEN);
    UTIL_LCDEx_PrintfAt(x0, y0, LEFT_MODE, nn_classes_table[detect->class_index]);

    for (i = 0; i < ARRAY_NB(bindings); i++)
    {
        app_display_binding(&detect->pKeyPoints[bindings[i][0]], &detect->pKeyPoints[bindings[i][1]], bindings[i][2]);
    }

    for (i = 0; i < AI_POSE_PP_POSE_KEYPOINTS_NB; i++)
    {
        app_display_keypoint(&detect->pKeyPoints[i], kp_color[i]);
    }
}

static void app_display_network_output(app_display_info_t *display_info)
{
    float cpuload_one_second;
    uint8_t line_nb = 0;
    int32_t i;

    app_lcd_draw_area_update();

    UTIL_LCD_FillRect(0, 0, LCD_FG_WIDTH, LCD_FG_HEIGHT, 0x00000000);

    app_cpuload_update(&cpuload);
    app_cpuload_get_info(&cpuload, NULL, &cpuload_one_second, NULL);

    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "CPU load");
    line_nb += 1;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "%.1f%%", cpuload_one_second);
    line_nb += 2;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Inference");
    line_nb += 1;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "%ums", display_info->inf_ms);
    line_nb += 2;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "FPS");
    line_nb += 1;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "%.2f", 1000.0 / display_info->nn_period_ms);
    line_nb += 2;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "Peoples");
    line_nb += 1;
    UTIL_LCDEx_PrintfAt(0, LINE(line_nb), RIGHT_MODE, "%u", display_info->nb_detect);

    for (i = 0; i < display_info->nb_detect; i++)
    {
        app_display_detection(&display_info->detects[i]);
    }

    app_lcd_draw_area_commit();
}
