/* ems04_beat_svc.c — S2 board-level beat replay service (EMS04, RTT ch0).
 * See ems04_beat_svc.h for the wire contract. Per-beat computation is the
 * unmodified S1 decision-layer port (ems04_dp_tick); the board holds no
 * inter-beat state. All multi-byte fields are little-endian (AArch32 LE,
 * plain memcpy of wire bytes into typed locals). */
#include "ems04_beat_svc.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ems04_law6_c2.h"
#include "ems04_rtt_min.h"

#define FLAG_DP_OK      (1u << 0)
#define FLAG_FALLBACK   (1u << 1)
#define FLAG_OOB        (1u << 2)
#define FLAG_TABLES     (1u << 3)

/* parser states */
enum { BS_SYNC = 0, BS_PAYLOAD };

typedef struct {
    int state;
    uint32_t got;          /* bytes accumulated in frame[] */
    uint8_t frame[EMS04_BEAT_FRAME_LEN];
    ems04_tables tbl;
    int tables_ok;
} ems04_beat_ctx;

static ems04_beat_ctx m_beat;

/* read a little-endian scalar out of the frame buffer (no alignment
 * assumptions: memcpy, and AArch32 LE makes the wire layout native) */
static uint32_t rd_u32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static double   rd_f64(const uint8_t *p) { double v;   memcpy(&v, p, 8); return v; }

static void beat_respond(uint32_t seq, double d1, uint32_t flags)
{
    uint8_t out[16];
    memcpy(&out[0], &seq, 4);      /* LE native */
    memcpy(&out[4], &d1, 8);
    memcpy(&out[12], &flags, 4);
    (void)ems04_rtt_write_up(EMS04_RTT_CH0, out, sizeof out);
}

static void beat_process(void)
{
    const uint8_t *f = m_beat.frame;
    uint32_t seq = rd_u32(f + 4);
    uint32_t flags = 0u;

    if (!m_beat.tables_ok) {
        beat_respond(seq, 0.0, FLAG_TABLES);
        return;
    }

    /* contract §4.1: Pbar[K], SOC0, N0, gen0 (t_dwell forced [0,0], A-02);
     * SOC_target fixed 0.60 (run_cstar hard-codes it, inside ems04_dp_tick). */
    double Pbar[EMS04_BEAT_K];
    for (uint32_t k = 0; k < EMS04_BEAT_K; k++) {
        Pbar[k] = rd_f64(f + 72u + 8u * k);
    }
    double SOC0 = rd_f64(f + 8);
    double N0   = rd_f64(f + 16);

    ems04_gen_state gen0;
    gen0.st[0]      = rd_f64(f + 24);
    gen0.st[1]      = rd_f64(f + 32);
    gen0.P_ramp[0]  = rd_f64(f + 40);
    gen0.P_ramp[1]  = rd_f64(f + 48);
    gen0.P_lag[0]   = rd_f64(f + 56);
    gen0.P_lag[1]   = rd_f64(f + 64);
    gen0.t_dwell[0] = 0.0;   /* gen0_from_ds convention (A-02) */
    gen0.t_dwell[1] = 0.0;

    ems04_tick_result res;
    int rc = ems04_dp_tick(&m_beat.tbl, Pbar, (int)EMS04_BEAT_K,
                           SOC0, N0, &gen0, &res);
    if (rc != 0) {
        beat_respond(seq, 0.0, 0u);   /* dp_ok = 0 */
        return;
    }
    flags |= FLAG_DP_OK;
    if (res.infeasible_fallback_n > 0) flags |= FLAG_FALLBACK;
    if (res.oob_query_count > 0)       flags |= FLAG_OOB;
    beat_respond(seq, res.dlambda_first, flags);
}

void ems04_beat_init(void)
{
    memset(&m_beat, 0, sizeof m_beat);
    m_beat.state = BS_SYNC;
    m_beat.tables_ok = (ems04_tables_board(&m_beat.tbl) == 0);
    ems04_rtt_init();
}

void ems04_beat_poll(void)
{
    /* bounded work per poll: process at most a few frames' worth of bytes;
     * the S2 host paces one beat at a time, so this never backs up */
    uint8_t tmp[256];
    for (int iter = 0; iter < 4; iter++) {
        unsigned n = ems04_rtt_read_down(EMS04_RTT_CH0, tmp, sizeof tmp);
        if (n == 0u) break;

        for (unsigned i = 0; i < n; i++) {
            uint8_t c = tmp[i];
            if (m_beat.state == BS_SYNC) {
                /* slide a 4-byte window until the magic matches; remaining
                 * bytes of a valid header continue in BS_PAYLOAD */
                m_beat.frame[m_beat.got++] = c;
                if (m_beat.got == 4u) {
                    if (rd_u32(m_beat.frame) == EMS04_BEAT_MAGIC) {
                        m_beat.state = BS_PAYLOAD;
                    } else {
                        /* keep last 3 bytes for the next magic test */
                        m_beat.frame[0] = m_beat.frame[1];
                        m_beat.frame[1] = m_beat.frame[2];
                        m_beat.frame[2] = m_beat.frame[3];
                        m_beat.got = 3u;
                    }
                }
            } else {
                m_beat.frame[m_beat.got++] = c;
                if (m_beat.got == EMS04_BEAT_FRAME_LEN) {
                    m_beat.got = 0u;
                    m_beat.state = BS_SYNC;
                    beat_process();
                }
            }
        }
    }
}
