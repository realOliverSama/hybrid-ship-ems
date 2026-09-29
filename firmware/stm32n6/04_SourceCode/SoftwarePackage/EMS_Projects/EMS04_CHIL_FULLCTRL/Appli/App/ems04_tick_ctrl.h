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

/* S4-P4 执行层 1 s 门控 + 航速展示通道（附加命令，冻结契约零改动；用户裁决
 * 2026-09-29 补齐契约 §4.3 "1 s 门控" 口径 v2）：
 * CMD_EXEC_REFRESH(0x12, 16B = seq+SOC+v_ship+P_load f32)
 *   → RSP_EXEC_REFRESH(0x13, 12B = seq+P_gen_ref+lam_eff f32)。
 * host 每 1 s（植物子步 100 边界）下发植物实测 SOC/P_load 与航速；板以
 * (SOC, P_load, 上拍 d1) 重算执行层 grid → P_gen_ref（0.04 ms 量级），同帧
 * 刷新 LCD（功率流/SOC/航速 1 Hz）。 */
#define EMS04_EXEC_REFRESH_CMD 0x12u
#define EMS04_EXEC_REFRESH_RSP 0x13u
#define EMS04_EXEC_REFRESH_CMD_LEN 16u
#define EMS04_EXEC_REFRESH_RSP_LEN 12u

/* S4 偏差④遗留①取证通道（附加命令，debug 用途，不动任何既有契约）：
 * CMD_DPDBG(0x14, 260B = seq+SOC f64+N0 f64+q50 30×f64) → 板直调 ems04_dp_tick
 * （跳过推理），RSP_DPDBG(0x15) 全量回传 TickResult（800B：rc+d1/J/soc_end/
 * fuel/oob/fallback_n/n_fb + dlambda_all/soc_path/N_path 全量 f64）。 */
#define EMS04_DPDBG_CMD 0x14u
#define EMS04_DPDBG_RSP 0x15u
#define EMS04_DPDBG_CMD_LEN 260u
#define EMS04_DPDBG_RSP_LEN 800u

/* reset controller state (CMD_RESET path) */
void ems04_tick_ctrl_reset(void);

/* handle one CMD_TICK_CTRL payload; returns RSP payload length (94),
 * or 0 → caller sends NACK. */
uint16_t ems04_tick_ctrl_handle(const uint8_t *payload, uint16_t len,
                                uint8_t out[EMS04_TICK_CTRL_RSP_LEN],
                                uint32_t *out_len_cap);

/* 回放最近一次 TICK_CTRL 的段级分账（µs；CYCCNT 周期域差值、回绕安全）。 */
uint16_t ems04_tick_ctrl_prof(uint8_t out[EMS04_TICK_CTRL_PROF_RSP_LEN]);

/* S4 偏差④遗留①取证（CMD_DPDBG 0x14 → RSP_DPDBG 0x15）：q50 注入式
 * 板 DP 直调 + TickResult 全量回传；len 非契约返回 0 → 调用方 NACK。 */
uint16_t ems04_dpdbg_handle(const uint8_t *payload, uint16_t len,
                            uint8_t out[EMS04_DPDBG_RSP_LEN]);

/* 执行层 1 s 门控（口径 v2）：以 (SOC, P_load, 上拍 d1) 重算 grid → P_gen_ref，
 * 同帧刷新 LCD。返回 RSP 载荷长度（12）；无 TICK_CTRL 历史（d1 未初始化）时
 * NACK（len=0）。 */
uint16_t ems04_exec_refresh(const uint8_t *payload, uint16_t len,
                            uint8_t out[EMS04_EXEC_REFRESH_RSP_LEN]);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_TICK_CTRL_H */
