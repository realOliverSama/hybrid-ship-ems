/* ems04_beat_svc.h — S2 board-level beat replay service (EMS04, RTT ch0).
 *
 * Minimal host-driven interface for the law6 (C2 arm) board-level parity
 * check. The BOARD IS STATELESS BETWEEN BEATS: per-tick state (SOC/N/gen)
 * is reconstructed on the host with the S1 conventions (A-02 t_dwell=[0,0],
 * 1 Hz right-endpoint traj sampling) and fed in frame by frame. The 10 s
 * SOC/N/gen0 advance itself is deliberately NOT implemented on board at
 * this stage (host-side state reconstruction, same scope as S1).
 *
 * Wire format (little-endian, RTT channel 0 both ways):
 *
 *   DOWN (host -> board), EMS04_BEAT_FRAME_LEN = 312 B:
 *     [magic u32 = 0x34534D45 ("EMS4" LE)]
 *     [seq u32]                       host-side sequence, echoed verbatim
 *     [SOC0 f64][N f64]               beat initial state
 *     [gen0 6xf64]                    st1, st2, P_ramp1, P_ramp2,
 *                                     P_lag1, P_lag2   (t_dwell forced [0,0])
 *     [Pbar 30xf64]                   bus-side kW, 10 s leads, K fixed 30
 *
 *   UP (board -> host), 16 B:
 *     [seq u32][d1 f64][flags u32]
 *     flags bit0 = dp_ok           (frame consumed, ems04_dp_tick returned 0)
 *            bit1 = fallback       (infeasible_fallback_n > 0 in this beat)
 *            bit2 = oob            (oob_query_count > 0 in this beat)
 *            bit3 = tables_fault   (board table binding failed at init)
 *            bits[31:5] reserved 0
 *
 * gen0 is carried as 6 f64 (the S1 replay input encoding). NOTE for the
 * coordinator: the S2 task text wrote "[gen0 f64]" as a single scalar; a
 * single double cannot reconstruct the S1 gen0 tuple, which bit-level
 * parity REQUIRES (fuel/N2 depend on it), so the field was implemented as
 * 6xf64 — same slot position in the frame, expanded in place.
 *
 * Interop with EMS03 services: the CMD_REATT UART service (ems_app.c) is
 * untouched and keeps running; this service only touches RTT ch0.
 */
#ifndef EMS04_BEAT_SVC_H
#define EMS04_BEAT_SVC_H

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_BEAT_MAGIC      0x34534D45u   /* "EMS4" little-endian */
#define EMS04_BEAT_K          30u
#define EMS04_BEAT_FRAME_LEN  (4u + 4u + 8u + 8u + 6u * 8u + 8u * 30u)  /* 312 */

/* Bind board tables + arm RTT. Call once from USER CODE 2 (after
 * ems_app_init()). Never blocks. */
void ems04_beat_init(void);

/* Non-blocking pump: drains RTT down ch0, re-assembles frames, runs one
 * ems04_dp_tick per complete frame, emits one 16 B response per frame.
 * Call from the main loop (alongside ems_app_poll()). */
void ems04_beat_poll(void);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_BEAT_SVC_H */
