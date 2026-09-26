/* ems04_fp_proto.c — EMS04-S2 B1：FPC-1.0 线格式编解码 + CRC16 + RX 状态机。
 *
 * 逐字段显式小端读写（禁止 struct 强转上线）；错误码见 PROTOCOL_FP.md §6。
 */
#include "ems04_fp_proto.h"

#include <string.h>

/* ------------------------------------------------------------------ CRC16 */
uint16_t ems04_fp_crc16(const uint8_t *data, uint32_t len)
{
    return ems04_fp_crc16_chain(0xFFFFu, data, len);
}

uint16_t ems04_fp_crc16_chain(uint16_t crc_in, const uint8_t *data, uint32_t len)
{
    uint16_t crc = crc_in;
    uint32_t i;
    uint32_t b;

    for (i = 0; i < len; ++i) {
        crc = (uint16_t)(crc ^ (uint16_t)((uint16_t)data[i] << 8));
        for (b = 0; b < 8u; ++b) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc; /* 不反射、无异或输出 */
}

/* ------------------------------------------------------------- LE 标量读写 */
uint16_t ems04_le_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

uint32_t ems04_le_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

double ems04_le_f64(const uint8_t *p)
{
    uint64_t bits = 0u;
    double   v;
    uint32_t i;

    for (i = 0; i < 8u; ++i) {
        bits |= ((uint64_t)p[i]) << (8u * i);
    }
    memcpy(&v, &bits, sizeof(v));
    return v;
}

void ems04_st_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

void ems04_st_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

void ems04_st_f64(uint8_t *p, double v)
{
    uint64_t bits = 0u;
    uint32_t i;

    memcpy(&bits, &v, sizeof(bits));
    for (i = 0; i < 8u; ++i) {
        p[i] = (uint8_t)((bits >> (8u * i)) & 0xFFu);
    }
}

/* ------------------------------------------------------------- 长度契约表 */
uint32_t ems04_fp_fixed_len(uint8_t type)
{
    switch (type) {
    case EMS04_FP_CMD_VOYAGE_INIT: return EMS04_FP_VOYAGE_INIT_LEN;
    case EMS04_FP_CMD_TICK2:       return EMS04_FP_TICK2_LEN;
    case EMS04_FP_CMD_FP_QUERY:    return EMS04_FP_T0_LEN;
    case EMS04_FP_CMD_FP_DUMP:     return EMS04_FP_T0_LEN;
    case EMS04_FP_RSP_FP_QUERY:    return EMS04_FP_RSP_QUERY_LEN;
    case EMS04_FP_RSP_FP_DUMP:     return EMS04_FP_RSP_DUMP_LEN;
    case EMS04_FP_ACK:             return 1u;
    case EMS04_FP_NACK:            return 2u;
    case EMS04_FP_CMD_KF_CHUNK:    return EMS04_FP_LEN_VARIABLE;
    default:                       return EMS04_FP_LEN_VARIABLE;
    }
}

/* ------------------------------------------------------------------ 解码器 */
int ems04_fp_dec_voyage_init(const uint8_t *p, uint32_t len, ems04_fp_voyage_init_t *out)
{
    uint32_t i;

    if (len != EMS04_FP_VOYAGE_INIT_LEN) {
        return (int)EMS04_FP_ERR_LEN;
    }
    memset(out, 0, sizeof(*out));
    out->protocol_version         = ems04_le_u32(p + 0);
    out->feature_contract_version = ems04_le_u32(p + 4);
    for (i = 0; i < 32u; ++i) {
        out->voyage_id[i] = (char)p[8u + i];
    }
    out->voyage_id[32]      = '\0';
    out->water_depth_m      = ems04_le_f64(p + 40);
    out->ctrl_policy        = p[48];
    out->manual_power_rule  = p[49];
    out->reserved           = ems04_le_u16(p + 50);
    out->tick_first         = ems04_le_u32(p + 52);
    out->tick_last          = ems04_le_u32(p + 56);
    out->n_ticks            = ems04_le_u32(p + 60);
    out->kf_n_rows          = ems04_le_u32(p + 64);
    out->kf_issue_first     = ems04_le_f64(p + 68);
    out->kf_issue_step      = ems04_le_f64(p + 76);
    memcpy(out->kf_payload_sha256, p + 84, 32u);
    return 0;
}

int ems04_fp_dec_kf_chunk(const uint8_t *p, uint32_t len, ems04_fp_kf_chunk_t *out)
{
    if (len < EMS04_FP_KF_CHUNK_HDR_LEN) {
        return (int)EMS04_FP_ERR_LEN;
    }
    out->chunk_idx    = ems04_le_u32(p + 0);
    out->total_chunks = ems04_le_u32(p + 4);
    out->chunk_len    = ems04_le_u16(p + 8);
    if ((uint32_t)out->chunk_len != (len - EMS04_FP_KF_CHUNK_HDR_LEN)) {
        return (int)EMS04_FP_ERR_LEN; /* 声明长度与实到长度不符 → fail-closed */
    }
    out->payload = p + EMS04_FP_KF_CHUNK_HDR_LEN;
    return 0;
}

int ems04_fp_dec_tick2(const uint8_t *p, uint32_t len, ems04_fp_tick2_t *out)
{
    uint32_t i;

    if (len != EMS04_FP_TICK2_LEN) {
        return (int)EMS04_FP_ERR_LEN;
    }
    out->tick_idx = ems04_le_u32(p + 0);
    for (i = 0; i < EMS04_FP_N_TICK_F64; ++i) {
        out->v[i] = ems04_le_f64(p + 4u + 8u * i);
    }
    return 0;
}

int ems04_fp_dec_t0(const uint8_t *p, uint32_t len, double *t0_out)
{
    if (len != EMS04_FP_T0_LEN) {
        return (int)EMS04_FP_ERR_LEN;
    }
    *t0_out = ems04_le_f64(p);
    return 0;
}

/* ------------------------------------------------------------------ 编码器 */
size_t ems04_fp_encode_frame(uint8_t type, const uint8_t *payload, uint32_t payload_len,
                             uint8_t *out, size_t cap)
{
    uint32_t total = payload_len + EMS04_FP_FRAME_OVERHEAD;
    uint16_t crc;

    if (out == NULL || total > cap) {
        return 0u;
    }
    if (payload_len > EMS04_FP_MAX_PAYLOAD) {
        return 0u; /* LEN 为 u16，超限拒绝 */
    }
    out[0] = EMS04_FP_SYNC0;
    out[1] = EMS04_FP_SYNC1;
    out[2] = type;
    ems04_st_u16(out + 3, (uint16_t)payload_len);
    if (payload_len > 0u) {
        if (payload == NULL) {
            return 0u;
        }
        memcpy(out + 5, payload, payload_len);
    }
    /* CRC 作用域 = TYPE..PAYLOAD → 从 out[2] 起 total-4 字节 */
    crc = ems04_fp_crc16(out + 2, (uint32_t)(total - 4u));
    ems04_st_u16(out + 5u + payload_len, crc);
    return (size_t)total;
}

size_t ems04_fp_encode_ack(uint8_t cmd_type, uint8_t *out, size_t cap)
{
    uint8_t payload[1];
    payload[0] = cmd_type;
    return ems04_fp_encode_frame(EMS04_FP_ACK, payload, 1u, out, cap);
}

size_t ems04_fp_encode_nack(uint8_t cmd_type, uint8_t err, uint8_t *out, size_t cap)
{
    uint8_t payload[2];
    payload[0] = cmd_type;
    payload[1] = err;
    return ems04_fp_encode_frame(EMS04_FP_NACK, payload, 2u, out, cap);
}

size_t ems04_fp_encode_rsp_query(double t0, const uint8_t hist_sha256[32],
                                 const uint8_t kf_sha256[32], uint8_t *out, size_t cap)
{
    uint8_t payload[EMS04_FP_RSP_QUERY_LEN];

    ems04_st_f64(payload + 0, t0);
    payload[8] = 0u;             /* status = 0 */
    payload[9] = 0u;             /* rsv */
    ems04_st_u16(payload + 10, 0u); /* rsv */
    memcpy(payload + 12, hist_sha256, 32u);
    memcpy(payload + 44, kf_sha256, 32u);
    return ems04_fp_encode_frame(EMS04_FP_RSP_FP_QUERY, payload,
                                 EMS04_FP_RSP_QUERY_LEN, out, cap);
}

size_t ems04_fp_encode_rsp_dump(double t0, const uint8_t *hist, const uint8_t *kf,
                                uint8_t *out, size_t cap)
{
    /* 直接写入目标缓冲：避开 13932 B 的临时栈帧（板侧 SRAM 形态友好） */
    const uint32_t plen = EMS04_FP_RSP_DUMP_LEN;
    uint32_t total = plen + EMS04_FP_FRAME_OVERHEAD;
    uint16_t crc;

    if (out == NULL || hist == NULL || kf == NULL || (size_t)total > cap) {
        return 0u;
    }
    out[0] = EMS04_FP_SYNC0;
    out[1] = EMS04_FP_SYNC1;
    out[2] = (uint8_t)EMS04_FP_RSP_FP_DUMP;
    ems04_st_u16(out + 3, (uint16_t)plen);
    ems04_st_f64(out + 5, t0);
    out[13] = 0u; /* status = 0 */
    out[14] = 0u; /* rsv */
    ems04_st_u16(out + 15, 0u);
    memcpy(out + 17, hist, EMS04_FP_HIST_BYTES);
    memcpy(out + 17u + EMS04_FP_HIST_BYTES, kf, EMS04_FP_KF_BYTES);
    crc = ems04_fp_crc16(out + 2, total - 4u);
    ems04_st_u16(out + 5u + plen, crc);
    return (size_t)total;
}

size_t ems04_fp_encode_voyage_init(const ems04_fp_voyage_init_t *vi, uint8_t *out, size_t cap)
{
    uint8_t payload[EMS04_FP_VOYAGE_INIT_LEN];
    uint32_t i;

    memset(payload, 0, sizeof(payload));
    ems04_st_u32(payload + 0, vi->protocol_version);
    ems04_st_u32(payload + 4, vi->feature_contract_version);
    for (i = 0; i < 32u && vi->voyage_id[i] != '\0'; ++i) {
        payload[8u + i] = (uint8_t)vi->voyage_id[i];
    }
    ems04_st_f64(payload + 40, vi->water_depth_m);
    payload[48] = vi->ctrl_policy;
    payload[49] = vi->manual_power_rule;
    ems04_st_u16(payload + 50, vi->reserved);
    ems04_st_u32(payload + 52, vi->tick_first);
    ems04_st_u32(payload + 56, vi->tick_last);
    ems04_st_u32(payload + 60, vi->n_ticks);
    ems04_st_u32(payload + 64, vi->kf_n_rows);
    ems04_st_f64(payload + 68, vi->kf_issue_first);
    ems04_st_f64(payload + 76, vi->kf_issue_step);
    memcpy(payload + 84, vi->kf_payload_sha256, 32u);
    return ems04_fp_encode_frame(EMS04_FP_CMD_VOYAGE_INIT, payload,
                                 EMS04_FP_VOYAGE_INIT_LEN, out, cap);
}

size_t ems04_fp_encode_kf_chunk(uint32_t chunk_idx, uint32_t total_chunks,
                                const uint8_t *chunk, uint16_t chunk_len,
                                uint8_t *out, size_t cap)
{
    /* 直接写入目标缓冲：避免 65545 B 的临时栈帧；chunk 不得与 out 重叠 */
    uint32_t plen = (uint32_t)chunk_len + EMS04_FP_KF_CHUNK_HDR_LEN;
    uint32_t total = plen + EMS04_FP_FRAME_OVERHEAD;
    uint16_t crc;

    if (plen > EMS04_FP_MAX_PAYLOAD || out == NULL || (size_t)total > cap) {
        return 0u;
    }
    if (chunk_len > 0u && chunk == NULL) {
        return 0u;
    }
    out[0] = EMS04_FP_SYNC0;
    out[1] = EMS04_FP_SYNC1;
    out[2] = (uint8_t)EMS04_FP_CMD_KF_CHUNK;
    ems04_st_u16(out + 3, (uint16_t)plen);
    ems04_st_u32(out + 5, chunk_idx);
    ems04_st_u32(out + 9, total_chunks);
    ems04_st_u16(out + 13, chunk_len);
    if (chunk_len > 0u) {
        memcpy(out + 15, chunk, chunk_len);
    }
    crc = ems04_fp_crc16(out + 2, total - 4u);
    ems04_st_u16(out + 5u + plen, crc);
    return (size_t)total;
}

size_t ems04_fp_encode_tick2(const ems04_fp_tick2_t *tk, uint8_t *out, size_t cap)
{
    uint8_t payload[EMS04_FP_TICK2_LEN];
    uint32_t i;

    ems04_st_u32(payload + 0, tk->tick_idx);
    for (i = 0; i < EMS04_FP_N_TICK_F64; ++i) {
        ems04_st_f64(payload + 4u + 8u * i, tk->v[i]);
    }
    return ems04_fp_encode_frame(EMS04_FP_CMD_TICK2, payload, EMS04_FP_TICK2_LEN, out, cap);
}

size_t ems04_fp_encode_t0(uint8_t type, double t0, uint8_t *out, size_t cap)
{
    uint8_t payload[EMS04_FP_T0_LEN];

    ems04_st_f64(payload, t0);
    return ems04_fp_encode_frame(type, payload, EMS04_FP_T0_LEN, out, cap);
}

/* --------------------------------------------------------- RX 字节流状态机 */
enum {
    EMS04_FP_RXST_SYNC0 = 0,
    EMS04_FP_RXST_SYNC1,
    EMS04_FP_RXST_TYPE,
    EMS04_FP_RXST_LEN,
    EMS04_FP_RXST_PAYLOAD,
    EMS04_FP_RXST_CRC
};

void ems04_fp_rx_init(ems04_fp_rx_t *rx)
{
    memset(rx, 0, sizeof(*rx));
    rx->state = (uint8_t)EMS04_FP_RXST_SYNC0;
    rx->crc   = 0xFFFFu; /* CCITT-FALSE init（首帧亦须从 init 值起算） */
}

/* 仅复位解析状态与载荷簿记；frames_ok/crc_errors/junk_bytes 作为链路诊断累计保留 */
void ems04_fp_rx_reset_state(ems04_fp_rx_t *rx)
{
    uint32_t frames_ok = rx->frames_ok;
    uint32_t crc_errors = rx->crc_errors;
    uint32_t junk_bytes = rx->junk_bytes;

    ems04_fp_rx_init(rx);
    rx->frames_ok  = frames_ok;
    rx->crc_errors = crc_errors;
    rx->junk_bytes = junk_bytes;
}

static void rx_begin_frame(ems04_fp_rx_t *rx)
{
    rx->state   = (uint8_t)EMS04_FP_RXST_SYNC0;
    rx->type    = 0u;
    rx->len     = 0u;
    rx->got     = 0u;
    rx->len_got = 0u;
    rx->crc_got = 0u;
    rx->crc     = 0xFFFFu;
    rx->crc_rx  = 0u;
    rx->err     = 0u;
}

int ems04_fp_rx_push(ems04_fp_rx_t *rx, uint8_t byte)
{
    switch (rx->state) {
    case EMS04_FP_RXST_SYNC0:
        if (byte == EMS04_FP_SYNC0) {
            rx->state = (uint8_t)EMS04_FP_RXST_SYNC1;
        } else {
            rx->junk_bytes++;
        }
        return (int)EMS04_FP_RX_IDLE;

    case EMS04_FP_RXST_SYNC1:
        if (byte == EMS04_FP_SYNC1) {
            rx->state = (uint8_t)EMS04_FP_RXST_TYPE;
            rx->crc   = 0xFFFFu; /* 帧起始：CRC16-CCITT-FALSE init（每帧重置） */
            rx->len_got = 0u;
            rx->got     = 0u;
        } else if (byte == EMS04_FP_SYNC0) {
            rx->junk_bytes++; /* "AA AA 55" 容错：保留当前 AA 作为新同步首字节 */
        } else {
            rx->junk_bytes += 2u;
            rx->state = (uint8_t)EMS04_FP_RXST_SYNC0;
        }
        return (int)EMS04_FP_RX_IDLE;

    case EMS04_FP_RXST_TYPE:
        rx->type = byte;
        rx->crc = ems04_fp_crc16_chain(rx->crc, &byte, 1u);
        rx->state = (uint8_t)EMS04_FP_RXST_LEN;
        rx->len_got = 0u;
        return (int)EMS04_FP_RX_IDLE;

    case EMS04_FP_RXST_LEN:
        rx->len_bytes[rx->len_got++] = byte;
        rx->crc = ems04_fp_crc16_chain(rx->crc, &byte, 1u);
        if (rx->len_got == 2u) {
            rx->len = ems04_le_u16(rx->len_bytes);
            rx->got = 0u;
            if (rx->len > EMS04_FP_MAX_PAYLOAD) {
                rx_begin_frame(rx);
                rx->err = EMS04_FP_ERR_LEN;
                rx->crc_errors++; /* LEN 超限：与 CRC 错同类（帧级丢弃） */
                return (int)EMS04_FP_RX_ERR;
            }
            rx->state = (uint8_t)((rx->len == 0u) ? EMS04_FP_RXST_CRC : EMS04_FP_RXST_PAYLOAD);
        }
        return (int)EMS04_FP_RX_IDLE;

    case EMS04_FP_RXST_PAYLOAD:
        rx->payload[rx->got++] = byte;
        rx->crc = ems04_fp_crc16_chain(rx->crc, &byte, 1u);
        if (rx->got == rx->len) {
            rx->state = (uint8_t)EMS04_FP_RXST_CRC;
            rx->crc_got = 0u;
        }
        return (int)EMS04_FP_RX_IDLE;

    case EMS04_FP_RXST_CRC:
        rx->crc_bytes[rx->crc_got++] = byte;
        if (rx->crc_got == 2u) {
            uint8_t ty = rx->type;
            rx->crc_rx = ems04_le_u16(rx->crc_bytes);
            if (rx->crc_rx != rx->crc) {
                rx->crc_errors++;
                rx_begin_frame(rx);
                rx->type = ty; /* 保留已解析 TYPE 供 NACK 回显 */
                rx->len  = 0u;
                rx->err  = EMS04_FP_ERR_CRC;
                return (int)EMS04_FP_RX_ERR;
            }
            rx->frames_ok++;
            rx->state = (uint8_t)EMS04_FP_RXST_SYNC0;
            return (int)EMS04_FP_RX_FRAME;
        }
        return (int)EMS04_FP_RX_IDLE;

    default:
        rx_begin_frame(rx);
        return (int)EMS04_FP_RX_IDLE;
    }
}
