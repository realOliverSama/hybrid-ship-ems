/* ems04_rtt_min.h — minimal SEGGER-RTT-wire-compatible transport (EMS04).
 *
 * The vendor tree carries no SEGGER RTT middleware, so EMS04 ships this
 * self-owned minimal implementation of the STANDARD RTT control-block wire
 * format ("SEGGER RTT" ID + up/down ring descriptors). It is exclusively a
 * debug-transport for the S2 board-level beat replay (ems04_beat_svc):
 * host <-> board via the debug probe (channel 0 both directions). It is NOT
 * a production link and shares nothing with the CMD_REATT UART service.
 *
 * Channel map (both fixed by convention, see README):
 *   up   ch0: board -> host  (beat response frames, 16 B each)
 *   down ch0: host  -> board (beat request frames, 312 B each)
 *
 * Modes: NO_BLOCK_SKIP (drop the whole write when the ring lacks space).
 * The host correlates via the echoed seq and detects drops as seq gaps —
 * the S2 host paces one beat at a time, so steady-state never overflows.
 */
#ifndef EMS04_RTT_MIN_H
#define EMS04_RTT_MIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_RTT_CH0 0u

/* Prepare the control block (constant-initialized, so polling loops work
 * even without calling this; provided for explicitness). Idempotent. */
void ems04_rtt_init(void);

/* board -> host. Returns bytes accepted (SKIP mode: 0 if no room). */
unsigned ems04_rtt_write_up(uint8_t ch, const uint8_t *buf, unsigned n);

/* host -> board. Returns bytes consumed from the ring. */
unsigned ems04_rtt_read_down(uint8_t ch, uint8_t *buf, unsigned n);

/* Bytes currently pending in the down ring (diagnostics). */
unsigned ems04_rtt_down_pending(void);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_RTT_MIN_H */
