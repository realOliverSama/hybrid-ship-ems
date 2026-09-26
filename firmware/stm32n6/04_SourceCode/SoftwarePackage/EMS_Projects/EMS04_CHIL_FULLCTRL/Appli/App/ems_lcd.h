/* ems_lcd.h — RGBLCD 480x272 竖装可视化（phase4-g NPU2 线，2026-09-24）。
 *
 * 逻辑画布 272x480（竖屏），物理帧缓冲 480x272 RGB565（绘图原语内部做
 * 90° 旋转变换）。内容 = 用户批准的预览图 /tmp/ems_ui_A.png：
 *   标题 / SPEED+MODE / 竖直 DC 母线功率流（GEN 橙、BAT 绿、PROP 黄；
 *   红箭头=输出、蓝箭头=输入）/ 预测 vs 实际滚动曲线（蓝虚线 q50、
 *   红实线 P_load，非负纵轴 0..900 kW）。
 *
 * 初始化代码移植自 vendor Projects/15_RGBLCD（LTDC/DMA2D/IC16/RIF/引脚，
 * demo 板级验证过的同款序列）；.ioc 已含 LTDC/DMA2D/RIF/IC16 配置
 * （2026-09-24 GUI 激活 + 保存，EMS 键零丢失）。
 *
 * 接口（ems_app.c / main.c 接线）：
 *   ems_lcd_init()                 主循环前一次（时钟/外设/RIF/首帧）
 *   ems_lcd_notify(...)            每控制 tick 注入实时量（v_ship/P_load/P_gen/SOC/回退态）
 *   ems_lcd_notify_q(q50)          REATT 预测成功后注入 30 lead q50
 *   ems_lcd_poll(ms)               主循环每次迭代调用；内部 1 Hz 节流重绘
 */
#ifndef EMS_LCD_H
#define EMS_LCD_H

#include <stdint.h>

void ems_lcd_init(void);
void ems_lcd_notify(float v_ship_kn, float p_load_kw, float p_gen_kw,
                    float soc, int fallback);
void ems_lcd_notify_q(const float q50[30]);
void ems_lcd_poll(uint32_t now_ms);

#endif /* EMS_LCD_H */
