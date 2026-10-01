/* ems_lcd.c — RGBLCD 竖装可视化实现（phase4-g NPU2 线）。
 *
 * 显示链路（2026-09-24 定案）：**vendor rgblcd BSP 驱动全套**
 * （App/rgblcd.c 本地化副本与官方库逐行一致，唯一改动：fb 从
 * [1280*800] section(".EXTRAM") 改为 HyperRAM 指针宏 0x90000000——
 * 内部 SRAM 域 LTDC 读取异常（寄存器 dump 实证），HyperRAM 由
 * FSBL+appli XSPI1 配为 memory-mapped）。初始化/画点/填充/文字
 * 全部调用官方库（rgblcd_init/draw_point/fill/show_string），
 * 本模块只保留 UI 布局组合 + 1 Hz 刷新。
 *
 * 竖装屏（ALIENTEK 4.3' ATK-MD0430R）：面板分辨率由驱动按 ID 自动检测
 * （0x4342=480x272 / 0x4384=800x480），dir=0 竖屏逻辑画布 =
 * rgblcddev.width x rgblcddev.height（272x480 或 480x800）。
 * UI 布局按 272x480 参考设计等比缩放（SX/SY），字号分档自适应。
 *
 * 已知限制（诚实标注）：固件无 V_dc 采样链（母排电压显示标称值，接口预留）；
 * MODE 仅 AUTO/FALLBACK 两态；BAT 支路功率 = P_load - P_gen（功率守恒推导）。
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stm32n6xx_hal.h"
#include "main.h"
#include "rgblcd.h"
#include "ems_lcd.h"

/* rgblcd 驱动的 DMA2D/LTDC 句柄（rgblcd.c extern 引用） */
DMA2D_HandleTypeDef hdma2d;        /* rgblcd 驱动 extern 引用 */
LTDC_HandleTypeDef hltdc;          /* rgblcd 驱动 extern 引用（LTDC 句柄） */

/* ---------- 几何/颜色 ---------- */
#define EMS_LCD_VBUS_NOMINAL_V  700.0f /* 板上无采样：标称值 */

#define RGB565(r, g, b)  ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))
#define C_BG      WHITE
#define C_BLACK   BLACK
#define C_GRAY    RGB565(150, 150, 150)
#define C_LGRAY   RGB565(210, 210, 214)
#define C_GRID    RGB565(228, 228, 232)
#define C_RED     RGB565(214, 39, 40)
#define C_BLUE    RGB565(31, 95, 191)
#define C_GEN     RGB565(242, 140, 40)
#define C_BAT     RGB565(46, 139, 87)
#define C_PROP    RGB565(230, 194, 41)
#define C_TEXTDIM RGB565(90, 90, 90)
#define C_BOXBG   RGB565(250, 250, 248)

/* ---------- 运行时画布（驱动 dir=0 竖屏逻辑尺寸）与坐标缩放 ---------- */
static uint16_t lcd_w;             /* = rgblcddev.width  (272 或 480) */
static uint16_t lcd_h;             /* = rgblcddev.height (480 或 800)  */
static uint8_t  lcd_fs_title;      /* 标题字号（官方档 12/16/24/32） */
static uint8_t  lcd_fs_head;       /* SPEED/MODE 档 */
static uint8_t  lcd_fs_body;       /* 标签/数值/坐标轴档 */

#define SX(x272)  ((uint16_t)((uint32_t)(x272) * lcd_w / 272u))   /* 参考横坐标缩放 */
#define SY(y480)  ((uint16_t)((uint32_t)(y480) * lcd_h / 480u))   /* 参考纵坐标缩放 */

/* ---------- 展示状态（tick/预测注入） ---------- */
static volatile float lcd_v_ship;
static volatile float lcd_p_load;
static volatile float lcd_p_gen;
static volatile int lcd_fallback;
static float lcd_vbus = EMS_LCD_VBUS_NOMINAL_V;
static float lcd_q50[30];               /* 最近一窗 q50（kW） */
static int lcd_q50_valid;
static float lcd_load_hist[30];         /* 实际负荷滚动（tick 注入） */
static uint32_t lcd_hist_head;
static float lcd_pred_hist[30];         /* p5s：每档"当时预测"（档首 q50[0]），与
 * lcd_load_hist 同槽对齐——红蓝线 x 轴同时间点（用户指令 2026-10-01） */
static uint32_t lcd_pred_head;
static uint8_t lcd_pair_arm;            /* notify（实际入环）置位，notify_q 消费 */
static uint32_t lcd_last_draw_ms;
static int lcd_ever_drawn;
static uint32_t lcd_frame_dbg_left;     /* 前 N 帧打印帧耗时（DWT 实测） */

/* ---------- 官方库原语封装（本层只做布局组合） ----------
 * lcd_fill 走 CPU 逐像素（官方 rgblcd_draw_point），不走路径 rgblcd_fill/DMA2D。
 * 注：2026-09-24 曾误判 DMA2D 填充 ~50ms×65次→5s/帧 阻塞主循环致 TICK STALE；
 * 后实锤 STALE 真凶是上位机 pyserial read(4096) 等到超时（响应延迟恒 =
 * 主机 timeout）。CPU 填充版实测帧耗 77ms（DWT），性能充裕，保持本实现。 */
static void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c)
{
    if (w == 0u || h == 0u) return;
    if (x >= lcd_w || y >= lcd_h) return;
    if ((uint32_t)x + w > lcd_w) w = (uint16_t)(lcd_w - x);
    if ((uint32_t)y + h > lcd_h) h = (uint16_t)(lcd_h - y);
    for (uint16_t j = 0u; j < h; j++) {
        for (uint16_t i = 0u; i < w; i++) {
            rgblcd_draw_point((uint16_t)(x + i), (uint16_t)(y + j), c);
        }
    }
}

static void lcd_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c)
{
    rgblcd_draw_rectangle(x, y, (uint16_t)(x + w - 1u), (uint16_t)(y + h - 1u), c);
}

/* 字符串（官方字体；mode=0 内建背景覆盖，1 Hz 刷新无残影） */
static void lcd_string(const char *s, uint16_t x, uint16_t y, uint16_t color, uint8_t size)
{
    rgblcd_show_string(x, y, (uint16_t)(lcd_w - x), size, size, (char *)s, color);
}

static void lcd_string_center(const char *s, uint16_t y, uint16_t color, uint8_t size)
{
    uint16_t w = (uint16_t)((size >> 1) * (uint16_t)strlen(s));
    lcd_string(s, (uint16_t)((lcd_w > w) ? (lcd_w - w) / 2u : 0u), y, color, size);
}

/* 垂直箭头（y0→y1，dy 决定方向；杆 t 粗 + 三角头） */
static void lcd_arrow_v(uint16_t x, uint16_t y0, uint16_t y1, uint16_t t, uint16_t c)
{
    int16_t dy = (int16_t)y1 - (int16_t)y0;
    uint16_t y_a = (dy >= 0) ? y0 : y1;
    uint16_t len = (uint16_t)((dy >= 0 ? (int16_t)y1 : (int16_t)y0) - (int16_t)y_a);
    uint16_t head = SY(10u);
    lcd_fill((uint16_t)(x - t / 2u), y_a, t, len, c);
    for (uint16_t i = 0u; i < head; i++) {
        uint16_t w = (uint16_t)(i + 1u);
        uint16_t yy = (dy >= 0) ? (uint16_t)(y1 - i) : (uint16_t)(y1 + i);
        lcd_fill((uint16_t)(x - w), yy, (uint16_t)(2u * w), 1u, c);
    }
}

/* ---------- UI 绘制（布局按 272x480 参考设计等比缩放） ----------
 * 分区刷新：静态元素（标题/框体/母线/图框/轴标）仅首帧画一次；
 * 每秒只刷动态元素（数值行/箭头/曲线区内层），消除整屏清屏闪烁。 */
static int lcd_static_drawn;

/* 部件框静态面：底 + 边框 + 名称（数值走 lcd_box_value 动态刷） */
static void lcd_box_frame(uint16_t cx, uint16_t cy, const char *name, uint16_t ec)
{
    uint16_t w = SX(120u), h = SY(42u);
    uint16_t x = (uint16_t)(cx - w / 2u), y = (uint16_t)(cy - h / 2u);
    lcd_fill(x, y, w, h, C_BOXBG);
    lcd_rect(x, y, w, h, ec);
    uint16_t lw = (uint16_t)((lcd_fs_body >> 1) * (uint16_t)strlen(name));
    lcd_string(name, (uint16_t)(cx - lw / 2u), (uint16_t)(y + SY(5u)), C_BLACK, lcd_fs_body);
}

/* 部件框数值（动态）：清数值行（框底色）后居中重写 */
static void lcd_box_value(uint16_t cx, uint16_t cy, const char *val)
{
    uint16_t w = SX(120u), h = SY(42u);
    uint16_t y = (uint16_t)(cy - h / 2u);
    uint16_t vy = (uint16_t)(y + h - SY(5u) - lcd_fs_body);
    g_back_color = C_BOXBG;
    lcd_fill((uint16_t)(cx - w / 2u + 2u), vy, (uint16_t)(w - 4u), lcd_fs_body, C_BOXBG);
    uint16_t lw = (uint16_t)((lcd_fs_body >> 1) * (uint16_t)strlen(val));
    lcd_string(val, (uint16_t)(cx - lw / 2u), vy, C_BLACK, lcd_fs_body);
    g_back_color = C_BG;
}

/* 静态元素：仅首帧绘制 */
static void lcd_draw_static(void)
{
    char buf[32];

    lcd_fill(0, 0, lcd_w, lcd_h, C_BG);

    lcd_string_center("Hybrid Ship EMS", SY(12u), C_BLACK, lcd_fs_title);
    lcd_fill(SX(8u), SY(108u), (uint16_t)(lcd_w - 2u * SX(8u)), SY(2u), C_LGRAY);

    lcd_box_frame(SX(68u), SY(140u), "GEN", C_GEN);
    lcd_box_frame(SX(204u), SY(140u), "BAT", C_BAT);
    lcd_box_frame(SX(136u), SY(280u), "PROP", C_PROP);

    /* 母排 + 电压（标称值，静态） */
    lcd_fill(SX(36u), SY(204u), (uint16_t)(lcd_w - 2u * SX(36u)), SY(4u), C_BLACK);
    (void)snprintf(buf, sizeof buf, "DC BUS  %.0f V", (double)lcd_vbus);
    lcd_string_center(buf, (uint16_t)(SY(204u) + SY(8u)), C_BLACK, lcd_fs_body);

    lcd_fill(SX(8u), SY(314u), (uint16_t)(lcd_w - 2u * SX(8u)), SY(2u), C_LGRAY);
    lcd_string("Load: forecast (blue) vs actual (red)", SX(8u), SY(322u), C_TEXTDIM, lcd_fs_body);

    /* 图框 + 纵轴标 */
    uint16_t px0 = SX(36u), px1 = (uint16_t)(lcd_w - SX(8u));
    uint16_t pyt = SY(350u), pyb = SY(472u);
    uint16_t span = (uint16_t)(px1 - px0), hgt = (uint16_t)(pyb - pyt);
    lcd_rect(px0, pyt, span, hgt, C_GRAY);
    /* 纵轴数字标签随自适应量程每秒重绘（见 lcd_draw_dynamic），骨架不再固定 */
}

/* 动态元素：每秒刷新（各区自带底色覆盖，无整屏清屏） */
static void lcd_draw_dynamic(void)
{
    char buf[32];

    /* SPEED / MODE（清行重写，字符串变长无残留） */
    lcd_fill(SX(10u), SY(52u), (uint16_t)(lcd_w - 2u * SX(10u)), lcd_fs_head, C_BG);
    (void)snprintf(buf, sizeof buf, "SPEED:%5.1f kn", (double)lcd_v_ship);
    lcd_string(buf, SX(10u), SY(52u), C_BLACK, lcd_fs_head);
    lcd_fill(SX(10u), SY(80u), (uint16_t)(lcd_w - 2u * SX(10u)), lcd_fs_head, C_BG);
    (void)snprintf(buf, sizeof buf, "MODE:%s", lcd_fallback ? "FALLBACK" : "AUTO");
    lcd_string(buf, SX(10u), SY(80u), lcd_fallback ? C_RED : C_BLACK, lcd_fs_head);

    /* ---- 功率流：GEN/BAT 上排 → DC BUS → PROP 下排 ---- */
    float p_gen = lcd_p_gen, p_load = lcd_p_load;
    float p_bat = p_load - p_gen;                    /* >0 放电(输出) <0 充电(输入) */
    int chg = (p_bat < 0.0f);

    (void)snprintf(buf, sizeof buf, "%.0f kW", (double)p_gen);
    lcd_box_value(SX(68u), SY(140u), buf);
    (void)snprintf(buf, sizeof buf, "%+.0f kW", (double)p_bat);
    lcd_box_value(SX(204u), SY(140u), buf);
    (void)snprintf(buf, sizeof buf, "%.0f kW", (double)p_load);
    lcd_box_value(SX(136u), SY(280u), buf);

    /* 箭头（充放电方向随工况变：清箭头区后重画） */
    uint16_t bus_y = SY(204u);
    uint16_t arr_t = (uint16_t)(SX(3u) > 2u ? SX(3u) : 2u);
    uint16_t am = (uint16_t)(SY(10u) + 2u);                 /* 箭头头半宽 + 余量 */
    lcd_fill((uint16_t)(SX(68u) - am), SY(164u), (uint16_t)(2u * am),
             (uint16_t)(bus_y - SY(8u) - SY(164u) + 1u), C_BG);
    lcd_arrow_v(SX(68u), SY(166u), (uint16_t)(bus_y - SY(8u)), arr_t, C_RED);   /* GEN→母线 */
    lcd_fill((uint16_t)(SX(204u) - am), SY(164u), (uint16_t)(2u * am),
             (uint16_t)(bus_y - SY(8u) - SY(164u) + 1u), C_BG);
    if (chg) lcd_arrow_v(SX(204u), (uint16_t)(bus_y - SY(8u)), SY(166u), arr_t, C_BLUE);
    else     lcd_arrow_v(SX(204u), SY(166u), (uint16_t)(bus_y - SY(8u)), arr_t, C_RED);

    /* 母线→PROP（负载输入蓝） */
    lcd_fill((uint16_t)(SX(136u) - am), (uint16_t)(bus_y + SY(22u)), (uint16_t)(2u * am),
             (uint16_t)(SY(250u) - bus_y - SY(22u) + 1u), C_BG);
    lcd_arrow_v(SX(136u), (uint16_t)(bus_y + SY(24u)), SY(250u), arr_t, C_BLUE);
    lcd_fill(SX(148u), (uint16_t)(bus_y + SY(28u)),
             (uint16_t)(lcd_w - SX(148u) - SX(8u)), lcd_fs_body, C_BG);
    (void)snprintf(buf, sizeof buf, "%.0f kW", (double)p_load);
    lcd_string(buf, SX(148u), (uint16_t)(bus_y + SY(28u)), C_BLUE, lcd_fs_body);

    /* ---- 预测区：曲线区内层清 + 网格 + 红实/蓝虚 + now/lead ---- */
    uint16_t px0 = SX(36u), px1 = (uint16_t)(lcd_w - SX(8u));
    uint16_t pyt = SY(350u), pyb = SY(472u);
    uint16_t span = (uint16_t)(px1 - px0);
    uint16_t hgt = (uint16_t)(pyb - pyt);
    lcd_fill((uint16_t)(px0 + 1u), (uint16_t)(pyt + 1u),
             (uint16_t)(span - 2u), (uint16_t)(hgt - 2u), C_BG);
    /* S4-P4 显示层修订（用户反馈 2026-09-29：负载曲线贴底直线/瞬态不可见）：
     * 量程自适应——取负荷历史与预测窗的窗内最大值向上取整到 300 kW 档
     * （下限 300），网格线三等分。pilot 航次低速段（≈128 kW）也能呈现形状。 */
    float rng_f = 300.0f;
    for (uint32_t i = 0u; i < 30u; i++) {
        if (lcd_load_hist[i] > rng_f) rng_f = lcd_load_hist[i];
        if (lcd_pred_hist[i] > rng_f) rng_f = lcd_pred_hist[i];
    }
    uint32_t rng = ((uint32_t)((rng_f + 299.0f) / 300.0f)) * 300u;
    for (uint32_t k = 0u; k <= 3u; k++) {
        uint32_t v = rng * k / 3u;
        uint16_t yy = (uint16_t)(pyb - (uint16_t)(v * hgt / rng));
        lcd_fill(px0, yy, span, 1u, C_GRID);
        lcd_fill((uint16_t)(px0 - SX(26u)), (uint16_t)(yy - lcd_fs_body / 2u),
                 SX(24u), (uint16_t)(lcd_fs_body + 1u), C_BG);
        (void)snprintf(buf, sizeof buf, "%u", (unsigned)v);
        lcd_string(buf, (uint16_t)(px0 - SX(24u)),
                   (uint16_t)(yy - lcd_fs_body / 2u), C_TEXTDIM, lcd_fs_body);
    }
    /* 实际（红实线）：滚动历史，全时间轴（p5s 对齐修订：与预测同 x 同时刻） */
    for (uint16_t i = 0u; i + 1u < 30u; i++) {
        float a = lcd_load_hist[(lcd_hist_head + i) % 30u];
        float b = lcd_load_hist[(lcd_hist_head + i + 1u) % 30u];
        if (a <= 0.0f || b <= 0.0f) continue;
        uint16_t x0 = (uint16_t)(px0 + (uint32_t)i * span / 29u);
        uint16_t x1 = (uint16_t)(px0 + (uint32_t)(i + 1u) * span / 29u);
        int16_t ya = (int16_t)(pyb - (int32_t)(a * (float)hgt / (float)rng));
        int16_t yb = (int16_t)(pyb - (int32_t)(b * (float)hgt / (float)rng));
        int16_t steps = (int16_t)(x1 - x0);
        if (steps < 1) steps = 1;
        for (int16_t s2 = 0; s2 <= steps; s2++) {
            int16_t yy = ya + (int16_t)(((int32_t)(yb - ya) * s2) / steps);
            rgblcd_draw_point((uint16_t)((int16_t)x0 + s2), (uint16_t)yy, C_RED);
            rgblcd_draw_point((uint16_t)((int16_t)x0 + s2), (uint16_t)(yy + 1), C_RED);
        }
    }
    /* 预测（蓝虚线）：对齐预测环——与红线同 x 同时刻（每档档首预测 vs 档首实际，
     * 用户指令 2026-10-01：红蓝 x 轴对应完全相同时间点） */
    for (uint16_t i = 0u; i + 1u < 30u; i++) {
        float a = lcd_pred_hist[(lcd_pred_head + i) % 30u];
        float b = lcd_pred_hist[(lcd_pred_head + i + 1u) % 30u];
        if (a <= 0.0f || b <= 0.0f) continue;
        uint16_t x0 = (uint16_t)(px0 + (uint32_t)i * span / 29u);
        uint16_t x1 = (uint16_t)(px0 + (uint32_t)(i + 1u) * span / 29u);
        int16_t ya = (int16_t)(pyb - (int32_t)(a * (float)hgt / (float)rng));
        int16_t yb = (int16_t)(pyb - (int32_t)(b * (float)hgt / (float)rng));
        int16_t steps = (int16_t)(x1 - x0);
        if (steps < 1) steps = 1;
        for (int16_t s2 = 0; s2 <= steps; s2++) {
            if (((uint16_t)s2 / 3u) % 2u != 0u) continue;      /* 虚线段 */
            int16_t yy = ya + (int16_t)(((int32_t)(yb - ya) * s2) / steps);
            rgblcd_draw_point((uint16_t)((int16_t)x0 + s2), (uint16_t)yy, C_BLUE);
        }
    }
    lcd_string("-300s", (uint16_t)(px0 + 2u), (uint16_t)(pyb - lcd_fs_body - SY(4u)), C_RED, lcd_fs_body);
    {
        const char *s = "now";
        uint16_t w = (uint16_t)((lcd_fs_body >> 1) * 8u);
        lcd_string(s, (uint16_t)(px1 - w), (uint16_t)(pyb - lcd_fs_body - SY(4u)), C_GRAY, lcd_fs_body);
    }
}

static void lcd_draw_frame(void)
{
    if (!lcd_static_drawn) {
        lcd_draw_static();
        lcd_static_drawn = 1;
    }
    lcd_draw_dynamic();
}

/* ---------- LTDC MSP 回调（demo stm32n6xx_hal_msp.c 同款移植） ----------
 * rgblcd_init 内 HAL_LTDC_Init 自动回调本函数：IC16 像素时钟（动态分频）
 * + LTDC 外设时钟使能 + 20 个 RGB 引脚 AF（PG0=AF10 特例）。 */
void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc_ptr)
{
    GPIO_InitTypeDef g = {0};
    RCC_PeriphCLKInitTypeDef clk = {0};

    clk.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    clk.LtdcClockSelection = RCC_LTDCCLKSOURCE_IC16;
    clk.ICSelection[RCC_IC16].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    clk.ICSelection[RCC_IC16].ClockDivider =
        (uint32_t)(HAL_RCCEx_GetPLL1CLKFreq() / 9000000u);   /* → 9 MHz 像素时钟 */
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK) Error_Handler();

    __HAL_RCC_LTDC_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    g.Pin = GPIO_PIN_15 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_4 | GPIO_PIN_10;
    g.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOB, &g);
    g.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOH, &g);
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    HAL_GPIO_Init(GPIOF, &g);
    g.Pin = GPIO_PIN_9 | GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_10 | GPIO_PIN_5 |
            GPIO_PIN_11 | GPIO_PIN_2 | GPIO_PIN_8 | GPIO_PIN_0;
    g.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_0;
    g.Alternate = GPIO_AF10_LCD;     /* PG0=LTDC_VSYNC 特例 AF10 */
    HAL_GPIO_Init(GPIOG, &g);
    g.Pin = GPIO_PIN_9 | GPIO_PIN_13;
    g.Alternate = GPIO_AF14_LCD;
    HAL_GPIO_Init(GPIOG, &g);
}

/* ---------- 公共接口 ---------- */
static void lcd_draw_frame_timed(void)
{
    const uint32_t c0 = DWT->CYCCNT;
    lcd_draw_frame();
    if (lcd_frame_dbg_left > 0u) {
        lcd_frame_dbg_left--;
        printf("RS-LCD frame=%lu ms\r\n",
               (unsigned long)((DWT->CYCCNT - c0) /
                               (SystemCoreClock / 1000u)));
    }
}

void ems_lcd_init(void)
{
    memset(lcd_q50, 0, sizeof lcd_q50);
    memset(lcd_load_hist, 0, sizeof lcd_load_hist);
    memset(lcd_pred_hist, 0, sizeof lcd_pred_hist);
    lcd_q50_valid = 0;
    lcd_hist_head = 0;
    lcd_pred_head = 0;
    lcd_pair_arm = 0u;
    lcd_last_draw_ms = 0u;
    lcd_ever_drawn = 0;
    lcd_static_drawn = 0;
    lcd_frame_dbg_left = 3u;

    /* RIF：DMA2D/LTDC master/slave 隔离面（EMS main.c 既有面之外补齐） */
    {
        RIMC_MasterConfig_t rimc = {0};
        rimc.MasterCID = RIF_CID_1;
        rimc.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
        HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D, &rimc);
        HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &rimc);
        HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D,
                                              RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
        HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1,
                                              RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    }

    /* DMA2D 首遍初始化（demo MX_DMA2D_Init 同款；rgblcd_fill 依赖已 init 句柄） */
    __HAL_RCC_DMA2D_CLK_ENABLE();   /* 缺此行使能：HAL_DMA2D_Init 读零寄存器报错 → Error_Handler 挂死 */
    hdma2d.Instance = DMA2D;
    hdma2d.Init.Mode = DMA2D_M2M_PFC;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0u;
    hdma2d.LayerCfg[1].InputOffset = 0u;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d.LayerCfg[1].InputAlpha = 255u;
    if (HAL_DMA2D_Init(&hdma2d) != HAL_OK) Error_Handler();
    if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK) Error_Handler();

    /* 预置 LTDC 句柄实例：rgblcd_init 内 HAL_LTDC_DeInit 需要非 NULL Instance
     * （demo 调用链 = MX_LTDC_Init 先行，此处等价）。 */
    hltdc.Instance = LTDC;

    /* GPIO 端口时钟全开（demo MX_GPIO_Init 同款）：rgblcd_panelid_read 在
     * rgblcd_init 首步直接读 R7/G7/B7 引脚，端口时钟未开 = 总线错误挂死。
     * （demo 另有 E 口 = EMS 应用自用引脚，EMS main.c 已自行使能。） */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* 背光脚 PA3（RGBLCD_BL）输出初始化：rgblcd 驱动假设 demo MX_GPIO_Init
     * 已配好 PA3，本工程引脚表无 PA3，须在此补配，末尾 BL(1) 方能拉亮。 */
    {
        GPIO_InitTypeDef bl = {0};
        bl.Pin = GPIO_PIN_3;
        bl.Mode = GPIO_MODE_OUTPUT_PP;
        bl.Pull = GPIO_PULLUP;
        bl.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &bl);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
    }

    /* vendor rgblcd 驱动全套：ID 检测/时序重配/fb 指针/背光（末尾 BL(1) 拉亮）；
     * LTDC 时钟与引脚由 HAL_LTDC_MspInit 回调提供（demo 同机制）。 */
    rgblcd_init();

    /* 运行时画布 = 驱动 dir=0 竖屏逻辑尺寸；字号分档自适应面板 */
    lcd_w = rgblcddev.width;
    lcd_h = rgblcddev.height;
    if (lcd_w >= 480u) { lcd_fs_title = 32u; lcd_fs_head = 24u; lcd_fs_body = 16u; }
    else               { lcd_fs_title = 16u; lcd_fs_head = 16u; lcd_fs_body = 12u; }
    g_back_color = C_BG;    /* show_char mode=0 的背景覆盖色 */

    printf("RS-LCD id=%04X panel=%ux%u canvas=%ux%u\r\n", rgblcddev.id,
           (unsigned)rgblcddev.pwidth, (unsigned)rgblcddev.pheight,
           (unsigned)lcd_w, (unsigned)lcd_h);

    lcd_draw_frame_timed();  /* 首帧（空数据骨架） */
    lcd_ever_drawn = 1;
    SCB_CleanDCache_by_Addr((uint32_t *)g_ltdc_lcd_framebuf,
                            (int32_t)(rgblcddev.pwidth * rgblcddev.pheight * 2u));
}

void ems_lcd_notify(float v_ship_kn, float p_load_kw, float p_gen_kw,
                    float soc, int fallback)
{
    (void)soc;
    lcd_v_ship = v_ship_kn;
    lcd_p_load = p_load_kw;
    lcd_p_gen = p_gen_kw;
    lcd_fallback = fallback;
    lcd_load_hist[lcd_hist_head] = p_load_kw;
    lcd_hist_head = (lcd_hist_head + 1u) % 30u;
    lcd_pair_arm = 1u;  /* 实际已入环：下一个 notify_q 的 q50[0] 与本档配对 */
}

void ems_lcd_notify_live(float v_ship_kn, float p_load_kw, float p_gen_kw,
                         float soc, int fallback)
{
    (void)soc;
    lcd_v_ship = v_ship_kn;
    lcd_p_load = p_load_kw;
    lcd_p_gen = p_gen_kw;
    lcd_fallback = fallback;
    /* 不写 lcd_load_hist：10 s 粒度历史由 TICK_CTRL 维持（轴语义 290 s 对齐） */
}

void ems_lcd_notify_q(const float q50[30])
{
    if (q50 == NULL) return;
    memcpy((void *)lcd_q50, q50, sizeof lcd_q50);
    lcd_q50_valid = 1;
    /* p5s 对齐环：仅当 notify 刚注入过同档实际（TICK_CTRL 顺序对）才入环；
     * REATT 单独查询（无前置 notify）不配对——不污染红蓝对齐轴。 */
    if (lcd_pair_arm) {
        uint32_t idx = (lcd_hist_head + 29u) % 30u;  /* 最新实际所在槽 */
        lcd_pred_hist[idx] = q50[0];                 /* 档首预测 vs 档首实际 */
        lcd_pred_head = lcd_hist_head;
        lcd_pair_arm = 0u;
    }
}

void ems_lcd_poll(uint32_t now_ms)
{
    if (lcd_ever_drawn && (now_ms - lcd_last_draw_ms) < 1000u) return;
    lcd_last_draw_ms = now_ms;
    lcd_draw_frame_timed();
    SCB_CleanDCache_by_Addr((uint32_t *)g_ltdc_lcd_framebuf,
                            (int32_t)(rgblcddev.pwidth * rgblcddev.pheight * 2u));
}
