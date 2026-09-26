/* ems04_rtt_min.c — minimal SEGGER-RTT-wire-compatible transport (EMS04).
 *
 * Layout is the STANDARD SEGGER RTT control block so any RTT-aware debugger
 * (probe-rs included) can auto-detect it by scanning RAM for the 16-byte ID
 * "SEGGER RTT":
 *
 *   char  acID[16];                 "SEGGER RTT\0" padded with zeros
 *   int32 NumUpBuffers;             1
 *   int32 NumDownBuffers;           1
 *   struct { const char* sName;         (NULL — debugger assigns names)
 *            char*      pBuffer;        (RAM ring)
 *            uint32 SizeOfBuffer;
 *            uint32 WrOff;           (volatile, wraps at SizeOfBuffer)
 *            uint32 RdOff;           (volatile)
 *            uint32 Flags;           (0 = MODE_NO_BLOCK_SKIP)
 *          } aUp[NumUp], aDown[NumDown];
 *
 * All words are 32-bit (AArch32), little-endian, natural alignment. The
 * control block and rings are statically initialized so they exist in
 * .data/.bss from load time (SRAM debug flow scans RAM after load_elf).
 * Single-core, single-writer-per-ring: no cache maintenance is involved
 * (rings live in normal cached RAM; the debugger access is coherent from
 * the debug perspective, matching how SEGGER RTT is used everywhere).
 */
#include "ems04_rtt_min.h"

#include <stddef.h>

#include "main.h"

/* probe-rs 的 STM32N647 芯片定义只把 AXISRAM1234 别名区 [0x24000000,0x243E0000)
 * 记为 !Ram，0x34xxxxxx 别名不在其 RAM 表内 → RTT 缓冲指针必须以 0x24 别名呈现
 * （同一物理 SRAM，CPU 经别名访问与 0x34 直访等价）。 */
#define EMS04_RTT_AXI_ALIAS_BASE 0x24000000u
#define EMS04_RTT_AXI_SRC_BASE   0x34000000u

static uint32_t ems04_rtt_alias(uint32_t addr)
{
    return addr - EMS04_RTT_AXI_SRC_BASE + EMS04_RTT_AXI_ALIAS_BASE;
}

#define EMS04_RTT_ID      "SEGGER RTT"
#define EMS04_RTT_UP_N    1u
#define EMS04_RTT_DOWN_N  1u
#define EMS04_RTT_UP_SZ   1024u   /* beat response = 16 B */
#define EMS04_RTT_DOWN_SZ 2048u   /* beat request  = 312 B */
#define EMS04_RTT_MODE_SKIP 0u

typedef struct {
    const char *name;
    uint8_t    *buf;
    uint32_t    size;
    volatile uint32_t wroff;
    volatile uint32_t rdoff;
    uint32_t    flags;
} ems04_rtt_buf;

typedef struct {
    char         acID[16];
    int32_t      num_up;
    int32_t      num_down;
    ems04_rtt_buf aUp[EMS04_RTT_UP_N];
    ems04_rtt_buf aDown[EMS04_RTT_DOWN_N];
} ems04_rtt_cb;

/* rings carry explicit alignment: uint8_t arrays have alignment 1, and the
 * linker may place them at odd addresses (observed 0x340C4307 on first bring-up)
 * which RTT hosts reject as a corrupted control block. */
static uint8_t m_up_ring[EMS04_RTT_UP_SZ]   __attribute__((aligned(8)));
static uint8_t m_down_ring[EMS04_RTT_DOWN_SZ] __attribute__((aligned(8)));

/* Constant-initialized => lands in .data, detectable immediately. */
static ems04_rtt_cb m_cb = {
    .acID = EMS04_RTT_ID,   /* remaining bytes zero-initialized */
    .num_up = (int32_t)EMS04_RTT_UP_N,
    .num_down = (int32_t)EMS04_RTT_DOWN_N,
    .aUp = {{
        .name = NULL, .buf = m_up_ring, .size = EMS04_RTT_UP_SZ,
        .wroff = 0u, .rdoff = 0u, .flags = EMS04_RTT_MODE_SKIP,
    }},
    .aDown = {{
        .name = NULL, .buf = m_down_ring, .size = EMS04_RTT_DOWN_SZ,
        .wroff = 0u, .rdoff = 0u, .flags = EMS04_RTT_MODE_SKIP,
    }},
};

void ems04_rtt_init(void)
{
    /* 静态初始化的 .buf 是 0x34 直访地址；改写为 0x24 别名（probe-rs RAM 表）。
     * 此后 CPU 的环数据访问全部经 b->buf（=别名）进行，物理存储不变。 */
    m_cb.aUp[0].buf   = (uint8_t *)ems04_rtt_alias((uint32_t)m_up_ring);
    m_cb.aDown[0].buf = (uint8_t *)ems04_rtt_alias((uint32_t)m_down_ring);
}

/* 环被 CPU（D-cache 开启，CCRR 实证）与调试器（DRAM 直读写）双侧访问：
 * 每次操作前把 CB/环上调试器新写的内容 CleanInvalidate 进来，操作后把 CPU
 * 新写的内容 Clean 出去——EMS03 NPU cache race 的同款纪律（SCB by-addr）。 */
static void ems04_rtt_cache_ci(void *addr, uint32_t len)
{
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)addr, (int32_t)len);
}

static void ems04_rtt_cache_clean(void *addr, uint32_t len)
{
    SCB_CleanDCache_by_Addr((uint32_t *)addr, (int32_t)len);
}

static ems04_rtt_buf *buf_of(uint8_t ch, int up)
{
    if (ch != EMS04_RTT_CH0) return NULL;
    if (up) { if (EMS04_RTT_UP_N == 0u) return NULL; }
    else    { if (EMS04_RTT_DOWN_N == 0u) return NULL; }
    return up ? &m_cb.aUp[0] : &m_cb.aDown[0];
}

unsigned ems04_rtt_write_up(uint8_t ch, const uint8_t *buf, unsigned n)
{
    ems04_rtt_buf *b = buf_of(ch, 1);
    uint32_t rd, wr, free_;
    unsigned head;

    if (!b || n == 0u) return 0u;
    ems04_rtt_cache_ci(&m_cb.aUp[0], sizeof(m_cb.aUp[0])); /* 看 host 的 rdoff */
    rd = b->rdoff;
    wr = b->wroff;
    free_ = (rd >= wr) ? (b->size - 1u - (wr - rd)) : (rd - wr - 1u);
    if (n > free_) return 0u;               /* SKIP: drop the whole write */
    head = b->size - wr;
    if (head >= n) {
        for (unsigned i = 0u; i < n; i++) b->buf[wr + i] = buf[i];
        ems04_rtt_cache_clean(b->buf + wr, n);
    } else {
        for (unsigned i = 0u; i < head; i++) b->buf[wr + i] = buf[i];
        for (unsigned i = head; i < n; i++) b->buf[i - head] = buf[i];
        ems04_rtt_cache_clean(b->buf + wr, head);
        ems04_rtt_cache_clean(b->buf, (uint32_t)n - head);
    }
    b->wroff = (wr + (uint32_t)n) % b->size;
    ems04_rtt_cache_clean(&b->wroff, sizeof(b->wroff));
    return n;
}

unsigned ems04_rtt_read_down(uint8_t ch, uint8_t *buf, unsigned n)
{
    ems04_rtt_buf *b = buf_of(ch, 0);
    uint32_t rd, wr, avail;
    unsigned take, head;

    if (!b || n == 0u) return 0u;
    ems04_rtt_cache_ci(&m_cb.aDown[0], sizeof(m_cb.aDown[0])); /* 看 host 的 wroff */
    rd = b->rdoff;
    wr = b->wroff;
    avail = (wr >= rd) ? (wr - rd) : (b->size - rd + wr);
    take = (avail < (uint32_t)n) ? (unsigned)avail : n;
    if (take == 0u) return 0u;
    head = b->size - rd;
    if ((uint32_t)head >= (uint32_t)take) {
        ems04_rtt_cache_ci(b->buf + rd, take);
        for (unsigned i = 0u; i < take; i++) buf[i] = b->buf[rd + i];
    } else {
        ems04_rtt_cache_ci(b->buf + rd, head);
        ems04_rtt_cache_ci(b->buf, (uint32_t)take - head);
        for (unsigned i = 0u; i < head; i++) buf[i] = b->buf[rd + i];
        for (unsigned i = head; i < take; i++) buf[i - head] = b->buf[i - head];
    }
    b->rdoff = (rd + (uint32_t)take) % b->size;
    ems04_rtt_cache_clean(&b->rdoff, sizeof(b->rdoff));
    return take;
}

unsigned ems04_rtt_down_pending(void)
{
    ems04_rtt_buf *b = &m_cb.aDown[0];
    uint32_t rd = b->rdoff, wr = b->wroff;
    return (wr >= rd) ? (unsigned)(wr - rd)
                      : (unsigned)(b->size - rd + wr);
}
