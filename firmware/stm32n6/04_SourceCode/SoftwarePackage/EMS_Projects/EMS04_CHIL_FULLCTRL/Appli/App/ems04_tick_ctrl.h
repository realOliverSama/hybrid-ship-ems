/* ems04_tick_ctrl.h — S4-G4 full-controller TICK_CTRL service (UART). */
#ifndef EMS04_TICK_CTRL_H
#define EMS04_TICK_CTRL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_TICK_CTRL_CMD   0x0Eu
#define EMS04_TICK_CTRL_RSP   0x0Fu
#define EMS04_TICK_CTRL_RSP_LEN 94u  /* 4+80+8+1 = seq+20×f32+2×u32+flags */

/* reset controller state (CMD_RESET path) */
void ems04_tick_ctrl_reset(void);

/* handle one CMD_TICK_CTRL payload; returns RSP payload length (94),
 * or 0 → caller sends NACK. */
uint16_t ems04_tick_ctrl_handle(const uint8_t *payload, uint16_t len,
                                uint8_t out[EMS04_TICK_CTRL_RSP_LEN],
                                uint32_t *out_len_cap);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_TICK_CTRL_H */
