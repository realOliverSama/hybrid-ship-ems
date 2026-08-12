#include "usmart.h"
#include "usmart_str.h"

/******************************************************************************************/
/* 用户配置区
 * 这下面要包含所用到的函数所申明的头文件(用户自己添加)
 */

#include "main.h"
#include "./RGBLCD/rgblcd.h"

extern void led_set(uint8_t sta);
extern void test_fun(void(*ledset)(uint8_t), uint8_t sta);

/* 函数名列表初始化(用户自己添加)
 * 用户直接在这里输入要执行的函数名及其查找串
 */
struct _m_usmart_nametab usmart_nametab[] =
{
#if USMART_USE_WRFUNS == 1 /* 如果使能了读写操作 */
    {(void *)read_addr, "uint32_t read_addr(uint32_t addr)"},
    {(void *)write_addr, "void write_addr(uint32_t addr, uint32_t val)"},
#endif
    {(void *)rgblcd_init, "void rgblcd_init(void)"},
    {(void *)rgblcd_display_dir, "void rgblcd_display_dir(uint8_t dir)"},
    {(void *)rgblcd_fill, "void rgblcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)"},
    {(void *)rgblcd_color_fill, "void rgblcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)"},
    {(void *)rgblcd_draw_point, "void rgblcd_draw_point(uint16_t x, uint16_t y, uint16_t color)"},
    {(uint16_t *)rgblcd_read_point, "uint16_t rgblcd_read_point(uint16_t x, uint16_t y)"},
    {(void *)rgblcd_clear, "void rgblcd_clear(uint16_t color)"},
    {(void *)rgblcd_draw_line, "void rgblcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)"},
    {(void *)rgblcd_draw_hline, "void rgblcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)"},
    {(void *)rgblcd_draw_rectangle, "void rgblcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)"},
    {(void *)rgblcd_draw_circle, "void rgblcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)"},
    {(void *)rgblcd_fill_circle, "void rgblcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)"},
    {(void *)rgblcd_show_char, "void rgblcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color)"},
    {(void *)rgblcd_show_num, "void rgblcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)"},
    {(void *)rgblcd_show_xnum, "void rgblcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)"},
    {(void *)rgblcd_show_string, "void rgblcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)"},

    {(void *)led_set, "void led_set(uint8_t sta)"},
    {(void *)test_fun, "void test_fun(void(*ledset)(uint8_t), uint8_t sta)"},
};

/******************************************************************************************/

/* 函数控制管理器初始化
 * 得到各个受控函数的名字
 * 得到函数总数量
 */
struct _m_usmart_dev usmart_dev =
{
    usmart_nametab,
    usmart_init,
    usmart_cmd_rec,
    usmart_exe,
    usmart_scan,
    sizeof(usmart_nametab) / sizeof(struct _m_usmart_nametab),  /* 函数数量 */
    0,                                                          /* 参数数量 */
    0,                                                          /* 函数ID */
    1,                                                          /* 参数显示类型,0,10进制;1,16进制 */
    0,                                                          /* 参数类型.bitx:,0,数字;1,字符串 */
    {0},                                                        /* 每个参数的长度暂存表,需要MAX_PARM个0初始化 */
    {0},                                                        /* 函数的参数,需要PARM_LEN个0初始化 */
};
