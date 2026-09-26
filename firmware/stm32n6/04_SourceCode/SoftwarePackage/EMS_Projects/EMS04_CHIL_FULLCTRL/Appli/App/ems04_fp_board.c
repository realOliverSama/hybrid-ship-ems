/* ems04_fp_board.c — EMS04-S2 C-fw：FP 服务 ↔ ems_proto UART 层胶水实现。
 *
 * 见 ems04_fp_board.h 的接线说明。本文件只做三件事：
 *   1) 组装 PROTOCOL_FP §0 帧容器（复用既有 ems_proto CRC 原语）并喂 FP 服务；
 *   2) 把服务产出的响应帧字节原样交回调用方（零重编码）；
 *   3) 维护诊断计数（命令数/响应数/err 码累计）。
 * 不改 FP 服务语义、不改既有命令行为。
 */
#include "ems04_fp_board.h"

#include <string.h>

#include "ems_proto.h" /* 既有 UART 帧层：CRC 原语复用（CCITT-FALSE 单一实现面） */
#include "ems04_fp_svc.h"

/* ---------------------------------------------------------------- 静态实例 */
static ems04_fp_svc_t s_svc; /* ≈486 KB BSS：RX 载荷 64 KB + kf 表 372.7 KB + 环/锁存 */
static ems04_fp_board_stats_t s_stat;

/* 响应捕获面（一次 handle 调用期间有效；单线程串行，无重入） */
static uint8_t *s_out;   /* 调用方缓冲 */
static uint32_t s_out_len;
static uint32_t s_out_cap;
static uint32_t s_out_ovf;

static void board_sink(void *ctx, const uint8_t *bytes, size_t len)
{
    (void)ctx;
    if (s_out == NULL || len > (size_t)(s_out_cap - s_out_len)) {
        s_out_ovf++; /* fail-closed：不写半帧，handle 返回 OVERFLOW */
        return;
    }
    memcpy(&s_out[s_out_len], bytes, len);
    s_out_len += (uint32_t)len;
}

static void board_stat_refresh(void)
{
    s_stat.svc_rx_frames = ems04_fp_svc_counter(&s_svc, EMS04_FP_CNT_RX_FRAMES);
    s_stat.ack = ems04_fp_svc_counter(&s_svc, EMS04_FP_CNT_ACK);
    s_stat.nack = ems04_fp_svc_counter(&s_svc, EMS04_FP_CNT_NACK);
    s_stat.rsp_query = ems04_fp_svc_counter(&s_svc, EMS04_FP_CNT_RSP_QUERY);
    s_stat.rsp_dump = ems04_fp_svc_counter(&s_svc, EMS04_FP_CNT_RSP_DUMP);
    {
        static const uint8_t codes[12] = {
            EMS04_FP_ERR_CRC,   EMS04_FP_ERR_TYPE,     EMS04_FP_ERR_LEN,
            EMS04_FP_ERR_STATE, EMS04_FP_ERR_POLICY,   EMS04_FP_ERR_ORDER,
            EMS04_FP_ERR_TICK_T, EMS04_FP_ERR_NONFINITE, EMS04_FP_ERR_NOT_READY,
            EMS04_FP_ERR_KF_HASH, EMS04_FP_ERR_KF_ROW, EMS04_FP_ERR_BOUNDS
        };
        for (uint32_t idx = 0u; idx < 12u; idx++) {
            s_stat.err[idx] = ems04_fp_svc_err_count(&s_svc, codes[idx]);
        }
    }
}

void ems04_fp_board_init(void)
{
    memset(&s_stat, 0, sizeof s_stat);
    s_out = NULL;
    s_out_len = 0u;
    s_out_cap = 0u;
    s_out_ovf = 0u;
    ems04_fp_svc_init(&s_svc, board_sink, NULL);
}

void ems04_fp_board_reset(void)
{
    ems04_fp_svc_reset(&s_svc);
}

int ems04_fp_board_is_fp_type(uint8_t type)
{
    switch (type) {
    case EMS04_FP_CMD_VOYAGE_INIT:
    case EMS04_FP_CMD_KF_CHUNK:
    case EMS04_FP_CMD_TICK2:
    case EMS04_FP_CMD_FP_QUERY:
    case EMS04_FP_CMD_FP_DUMP:
        return 1;
    default:
        return 0;
    }
}

int ems04_fp_board_handle(uint8_t type, const uint8_t *payload, uint16_t len,
                          uint8_t *rsp, uint16_t *rsp_len)
{
    uint8_t  hdr[5];
    uint8_t  crc_bytes[2];
    uint16_t crc;

    if (rsp_len != NULL) {
        *rsp_len = 0u;
    }
    if (ems04_fp_board_is_fp_type(type) == 0) {
        s_stat.unhandled++;
        return EMS04_FP_BOARD_UNHANDLED;
    }
    if (rsp == NULL || rsp_len == NULL || (payload == NULL && len > 0u)) {
        s_stat.bad_args++;
        return EMS04_FP_BOARD_BADARG;
    }

    /* 帧容器（PROTOCOL_FP §0）：[AA 55][TYPE][LEN LE16][PAYLOAD][CRC16 LE]
     * CRC 作用域 = TYPE..PAYLOAD；CRC 原语复用既有 ems_proto 层（同一 CCITT-FALSE
     * 实现的单一点），不另造 CRC 例程。 */
    hdr[0] = EMS04_FP_SYNC0;
    hdr[1] = EMS04_FP_SYNC1;
    hdr[2] = type;
    hdr[3] = (uint8_t)(len & 0xFFu);
    hdr[4] = (uint8_t)(len >> 8);
    crc = ems_proto_crc16(&hdr[2], 3u);
    if (len > 0u) {
        crc = ems_proto_crc16_chain(crc, payload, (uint32_t)len);
    }
    crc_bytes[0] = (uint8_t)(crc & 0xFFu);
    crc_bytes[1] = (uint8_t)(crc >> 8);

    s_out = rsp;
    s_out_cap = (uint32_t)EMS04_FP_BOARD_RSP_MAX; /* rsp 容量契约见头文件 */
    s_out_len = 0u;
    s_out_ovf = 0u;

    s_stat.commands++;
    /* 三段喂入 = 一条完整帧；载荷段零拷贝直传（svc RX 状态机按字节推进） */
    ems04_fp_svc_feed(&s_svc, hdr, sizeof hdr);
    if (len > 0u) {
        ems04_fp_svc_feed(&s_svc, payload, (size_t)len);
    }
    ems04_fp_svc_feed(&s_svc, crc_bytes, sizeof crc_bytes);

    s_out = NULL;
    if (s_out_ovf != 0u) {
        s_stat.overflow++;
        board_stat_refresh();
        return EMS04_FP_BOARD_OVERFLOW;
    }
    if (s_out_len == 0u) {
        /* 不可达（每条②命令恰好一条响应）；计数面留痕，调用方按零响应处理 */
        s_stat.no_response++;
        board_stat_refresh();
        return EMS04_FP_BOARD_NORSP;
    }

    *rsp_len = (uint16_t)s_out_len;
    s_stat.responses++;
    s_stat.response_bytes += s_out_len;
    board_stat_refresh();
    return EMS04_FP_BOARD_OK;
}

void ems04_fp_board_stats(ems04_fp_board_stats_t *out)
{
    board_stat_refresh();
    if (out != NULL) {
        *out = s_stat;
    }
}

uint32_t ems04_fp_board_err_count(uint8_t code)
{
    return ems04_fp_svc_err_count(&s_svc, code);
}
