/* ems04_tick_ctrl.h — S4-G4 full-controller TICK_CTRL service (UART). */
#ifndef EMS04_TICK_CTRL_H
#define EMS04_TICK_CTRL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_TICK_CTRL_CMD   0x0Eu
#define EMS04_TICK_CTRL_RSP   0x0Fu
#define EMS04_TICK_CTRL_RSP_LEN 94u  /* 4+80+8+1 = seq+20×f32+2×u32+flags（缓冲尺寸；线上载荷 93B） */

/* S4-P2 分账调试通道（附加命令，不动 TICK_CTRL 93B 线上契约）：
 * CMD_PROF(0x10, 空载荷) → RSP_PROF(0x11, 24B = seq+tick+reatt/dp/grid/pms
 * 五段 u32 µs)，回放上一次 TICK_CTRL 的段级计时；无历史时全 0。 */
#define EMS04_TICK_CTRL_PROF_CMD 0x10u
#define EMS04_TICK_CTRL_PROF_RSP 0x11u
#define EMS04_TICK_CTRL_PROF_RSP_LEN 24u

/* reset controller state (CMD_RESET path) */
void ems04_tick_ctrl_reset(void);

/* handle one CMD_TICK_CTRL payload; returns RSP payload length (94),
 * or 0 → caller sends NACK. */
uint16_t ems04_tick_ctrl_handle(const uint8_t *payload, uint16_t len,
                                uint8_t out[EMS04_TICK_CTRL_RSP_LEN],
                                uint32_t *out_len_cap);

/* 回放最近一次 TICK_CTRL 的段级分账（µs；CYCCNT 周期域差值、回绕安全）。 */
uint16_t ems04_tick_ctrl_prof(uint8_t out[EMS04_TICK_CTRL_PROF_RSP_LEN]);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_TICK_CTRL_H */
