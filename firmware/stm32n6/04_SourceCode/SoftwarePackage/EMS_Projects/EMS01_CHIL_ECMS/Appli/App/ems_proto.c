/* ems_proto.c — UART 帧协议实现（T-506，spec ems01-chil-ecms-v1）。
 *
 * design.md §UART 协议（FR-510）全量实现：
 *   帧 [AA 55][TYPE u8][LEN u16 LE][PAYLOAD][CRC16-CCITT LE over TYPE..PAYLOAD]，
 *   CRC = CCITT-FALSE（poly 0x1021，init 0xFFFF），作用域 TYPE..PAYLOAD。
 *   实现与 T-505 ems_app.c 内联 ems_crc16_ccitt/ems_crc16_ccitt_chain 逐位一致
 *   （迁移契约；环回测试以独立复算 + 已知检验值锁定）。
 *
 * 职责（本文件不含控制律/推理等应用语义）：
 *   1. 字节流状态机解析（byte-fed）。CRC 错 → NACK(ERR_CRC) + 事件环；LEN 超限/
 *      帧外垃圾（含文本日志行）→ 静默跳过重回同步（分流契约的固件侧收向）；
 *   2. 帧编码器（显式小端，不依赖机器字节序）+ TX 经 write 回调注入（固件接
 *      HAL_UART_Transmit，host 测试接内存/pty）；
 *   3. 注入会话（design §UART 超时）：合法 CMD_TICK 间隔 >2 s → 置 STALE + 事件。
 *      "保持 Pheld、不重跑控制律"的执行属 ems_app.c（应用编排）——本模块只维护
 *      会话状态与 STALE 标志；
 *   4. 故障事件环（定长 32，满覆盖最旧）+ CMD_DUMP_EVENTS 文本行导出。
 *
 * TYPE 分发（CMD_RESET/TICK/PING/DUMP/INFER_TEST → RSP/ACK/NACK）属应用层，
 * 在 ems_app.c 挂 on_frame 回调实现。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ems_proto.h"

/* ---- CRC16-CCITT（CCITT-FALSE）：与 T-505 ems_app.c ems_crc16_ccitt 逐位一致 ---- */
uint16_t ems_proto_crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFu; /* init 0xFFFF（CCITT-FALSE，design 帧表迁移契约） */
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int b = 0; b < 8; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc; /* 不反射、无异或输出 */
}

uint16_t ems_proto_crc16_chain(uint16_t crc_in, const uint8_t *data, uint32_t len)
{
    uint16_t crc = crc_in;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int b = 0; b < 8; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ---- 小端原语（显式逐字节组合，不依赖机器字节序） ---- */
uint16_t ems_proto_rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint32_t ems_proto_rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

float ems_proto_rd_f32(const uint8_t *p)
{
    /* f32 比特模型 = u32（IEEE-754；字节序已按线上 u32 组合归一，载体经 memcpy） */
    const uint32_t bits = ems_proto_rd_u32(p);
    float f;
    (void)memcpy(&f, &bits, 4u);
    return f;
}

void ems_proto_wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((uint32_t)v >> 8);
}

void ems_proto_wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

void ems_proto_wr_f32(uint8_t *p, float f)
{
    uint32_t bits;
    (void)memcpy(&bits, &f, 4u);
    ems_proto_wr_u32(p, bits);
}

/* ---- 生命周期 ---- */
void ems_proto_init(ems_proto_t *p,
                    void (*write)(void *user, const uint8_t *buf, uint32_t len),
                    void *write_user,
                    uint32_t (*now_ms)(void *user), void *now_user,
                    void (*on_frame)(void *user, uint8_t type,
                                     const uint8_t *payload, uint16_t len,
                                     uint32_t seq),
                    void *frame_user)
{
    (void)memset(p, 0, sizeof *p);
    p->write = write;
    p->write_user = write_user;
    p->now_ms = now_ms;
    p->now_user = now_user;
    p->on_frame = on_frame;
    p->frame_user = frame_user;
    p->rx_state = EMS_PROTO_RX_SYNC0;
}

static void ems_rx_reset(ems_proto_t *p)
{
    p->rx_state = EMS_PROTO_RX_SYNC0;
    p->rx_len = 0u;
    p->rx_got = 0u;
    p->rx_crc = 0u;
}

/* CMD_TICK 合法帧的会话登记（design §UART：注入会话 + 2 s 间隔守卫）：
 * 在 CRC 通过后、on_frame 分发前执行，使应用层读到的是本拍判定结果。
 * 实现见文件后部 ems_session_on_valid_tick()（前向声明于此，feed 先用）。 */
static void ems_session_on_valid_tick(ems_proto_t *p, const uint8_t *payload);

void ems_proto_feed(ems_proto_t *p, uint8_t b)
{
    switch (p->rx_state) {
    case EMS_PROTO_RX_SYNC0:
        if (b == EMS_PROTO_SYNC0) {
            p->rx_state = EMS_PROTO_RX_SYNC1;
        }
        break;
    case EMS_PROTO_RX_SYNC1:
        if (b == EMS_PROTO_SYNC1) {
            p->rx_state = EMS_PROTO_RX_TYPE;
        } else if (b == EMS_PROTO_SYNC0) {
            /* 连续 0xAA 容错（T-505 同语义迁移） */
        } else {
            ems_rx_reset(p); /* 文本日志行/垃圾字节：静默跳过（分流契约） */
        }
        break;
    case EMS_PROTO_RX_TYPE:
        p->rx_frame[0] = b;
        p->rx_state = EMS_PROTO_RX_LEN0;
        break;
    case EMS_PROTO_RX_LEN0:
        p->rx_frame[1] = b;
        p->rx_state = EMS_PROTO_RX_LEN1;
        break;
    case EMS_PROTO_RX_LEN1:
        p->rx_frame[2] = b;
        p->rx_len = ems_proto_rd_u16(&p->rx_frame[1]);
        /* 长度守卫：垃圾流/超长声明直接丢弃重回同步（host 环回用例覆盖）。
         * 不回 NACK：TYPE..LEN 在噪声流中不可信（无帧界）。 */
        if (p->rx_len > EMS_PROTO_MAX_PAYLOAD) {
            ems_rx_reset(p);
        } else {
            p->rx_got = 0u;
            p->rx_state = (p->rx_len > 0u) ? EMS_PROTO_RX_PAYLOAD : EMS_PROTO_RX_CRC0;
        }
        break;
    case EMS_PROTO_RX_PAYLOAD:
        p->rx_frame[3u + (uint32_t)p->rx_got] = b;
        p->rx_got++;
        if (p->rx_got >= p->rx_len) {
            p->rx_state = EMS_PROTO_RX_CRC0;
        }
        break;
    case EMS_PROTO_RX_CRC0:
        p->rx_crc = b;
        p->rx_state = EMS_PROTO_RX_CRC1;
        break;
    case EMS_PROTO_RX_CRC1:
        p->rx_crc |= (uint16_t)((uint16_t)b << 8); /* 线上 CRC 小端拼装 */
        if (ems_proto_crc16(p->rx_frame, 3u + (uint32_t)p->rx_len) == p->rx_crc) {
            p->stat_frames_ok++;
            if (p->rx_frame[0] == EMS_PROTO_CMD_TICK &&
                p->rx_len == EMS_PROTO_TICK_LEN) {
                /* 会话时基更新（合法 CMD_TICK 才驱动，间隔 >2 s → STALE+事件） */
                ems_session_on_valid_tick(p, &p->rx_frame[3]);
            }
            if (p->on_frame != NULL) {
                /* 载荷级 seq（2026-09-01）：载荷 ≥4B 时头 4B 按 u32 LE 视为序号
                 * （CMD_TICK=tick_idx；CMD_INFER/WC_TEST=测试 seq，应用层原样回显）；
                 * <4B 帧无序号，传 0。与 rd_u32 同款小端原语，与载荷解码一致。 */
                const uint32_t seq = (p->rx_len >= 4u)
                                         ? ems_proto_rd_u32(&p->rx_frame[3])
                                         : 0u;
                p->on_frame(p->frame_user, p->rx_frame[0],
                            (p->rx_len > 0u) ? &p->rx_frame[3] : NULL,
                            p->rx_len, seq);
            }
        } else {
            p->stat_crc_err++;
            /* design Error Handling：CRC 错 → NACK(ERR_CRC) + 事件（tick=最近一次） */
            ems_proto_event_push(p, EMS_EVT_CRC_ERR, p->rx_frame[0],
                                 ems_proto_last_tick_idx(p));
            (void)ems_proto_send_nack(p, p->rx_frame[0], EMS_PROTO_ERR_CRC);
        }
        ems_rx_reset(p);
        break;
    default:
        ems_rx_reset(p);
        break;
    }
}

void ems_proto_feed_buf(ems_proto_t *p, const uint8_t *buf, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        ems_proto_feed(p, buf[i]);
    }
}

/* ---- 事件环 ---- */
void ems_proto_event_push(ems_proto_t *p, uint8_t type, uint8_t aux, uint32_t tick)
{
    ems_proto_event_t *e = &p->ev[p->ev_head];
    e->type = type;
    e->aux = aux;
    e->tick = tick;
    e->ms = (p->now_ms != NULL) ? p->now_ms(p->now_user) : 0u;
    p->ev_head = (p->ev_head + 1u) % EMS_PROTO_EVENT_N;
    if (p->ev_count < EMS_PROTO_EVENT_N) {
        p->ev_count++;
    }
    p->ev_total++;
}

uint32_t ems_proto_event_count(const ems_proto_t *p)
{
    return p->ev_count;
}

const ems_proto_event_t *ems_proto_event_at(const ems_proto_t *p, uint32_t i)
{
    if (i >= p->ev_count) {
        return NULL;
    }
    /* 0 = 最旧：head 指向下一写入位，最旧位于 (head + N - count) % N */
    const uint32_t n = EMS_PROTO_EVENT_N;
    const uint32_t idx = (p->ev_head + n - p->ev_count + i) % n;
    return &p->ev[idx];
}

/* 最近一次合法 CMD_TICK 序号（事件环时戳源；无会话 → 哨兵） */
uint32_t ems_proto_last_tick_idx(const ems_proto_t *p)
{
    return p->session_active ? p->last_tick_idx : EMS_PROTO_TICK_NONE;
}

/* CMD_DUMP_EVENTS 文本行导出（首字节 'E' ≠ 0xAA，可打印 ASCII + \r\n）。 */
void ems_proto_dump_events(ems_proto_t *p)
{
    static const char *const names[] = {
        "RES0", "CRC", "UNKNOWN_TYPE", "STALE", "INFER_FAIL", "CLS_INVALID", "LEN",
    };
    char line[96];
    int n;

    for (uint32_t i = 0; i < p->ev_count; i++) {
        const ems_proto_event_t *e = ems_proto_event_at(p, i);
        const char *nm = (e->type < (sizeof names / sizeof names[0]))
                             ? names[e->type] : "?";
        /* seq = 全序号（含被环覆盖者）；1 基 */
        n = snprintf(line, sizeof line,
                     "EVT seq=%lu tick=%lu ms=%lu type=%s aux=%u\r\n",
                     (unsigned long)(p->ev_total - p->ev_count + 1u + i),
                     (unsigned long)e->tick, (unsigned long)e->ms, nm,
                     (unsigned int)e->aux);
        if ((n > 0) && (p->write != NULL)) {
            p->write(p->write_user, (const uint8_t *)line, (uint32_t)n);
        }
    }
    n = snprintf(line, sizeof line, "EVT total=%lu kept=%lu cap=%u\r\n",
                 (unsigned long)p->ev_total, (unsigned long)p->ev_count,
                 (unsigned)EMS_PROTO_EVENT_N);
    if ((n > 0) && (p->write != NULL)) {
        p->write(p->write_user, (const uint8_t *)line, (uint32_t)n);
    }
}

/* ---- 发送（组帧 + CRC + write 回调；len 超限/无 write 拒发返回 0，不半写） ---- */
uint8_t ems_proto_send_frame(ems_proto_t *p, uint8_t type,
                             const uint8_t *payload, uint16_t len)
{
    /* 静态组帧缓冲：≤1.3 KB，避免在主循环栈上放 INFER_TEST 级别的帧。
     * 单实例 + 请求-响应协议（无 ISR 重入、无并发 send），静态安全（design
     * §UART 会话单线串行）。 */
    static uint8_t buf[EMS_PROTO_FRAME_MAX];
    uint8_t hdr[3];
    uint16_t crc;
    uint32_t pos = 0u;

    if ((len > EMS_PROTO_MAX_PAYLOAD) || (p->write == NULL)) {
        return 0u;
    }

    hdr[0] = type;
    hdr[1] = (uint8_t)(len & 0xFFu);        /* LEN 低字节先发（小端线上序） */
    hdr[2] = (uint8_t)((uint32_t)len >> 8);
    crc = ems_proto_crc16(hdr, 3u);         /* CRC 作用域 TYPE..PAYLOAD */
    if ((payload != NULL) && (len > 0u)) {
        crc = ems_proto_crc16_chain(crc, payload, len);
    }

    buf[pos++] = EMS_PROTO_SYNC0;
    buf[pos++] = EMS_PROTO_SYNC1;
    buf[pos++] = hdr[0];
    buf[pos++] = hdr[1];
    buf[pos++] = hdr[2];
    if ((payload != NULL) && (len > 0u)) {
        (void)memcpy(&buf[pos], payload, len);
        pos += len;
    }
    buf[pos++] = (uint8_t)(crc & 0xFFu);    /* CRC 线上小端 */
    buf[pos++] = (uint8_t)((uint32_t)crc >> 8);
    p->write(p->write_user, buf, pos);
    return 1u;
}

uint8_t ems_proto_send_ack(ems_proto_t *p, uint8_t echo_type)
{
    return ems_proto_send_frame(p, EMS_PROTO_ACK, &echo_type, EMS_PROTO_ACK_LEN);
}

uint8_t ems_proto_send_nack(ems_proto_t *p, uint8_t echo_type, uint8_t err)
{
    uint8_t payload[2] = { echo_type, err }; /* design 帧表：echo TYPE + err */
    return ems_proto_send_frame(p, EMS_PROTO_NACK, payload, EMS_PROTO_NACK_LEN);
}

/* ---- 合法 CMD_TICK 的注入会话时基（间隔 >2 s → STALE + 事件） ---- */
static void ems_session_on_valid_tick(ems_proto_t *p, const uint8_t *payload)
{
    const uint32_t now = (p->now_ms != NULL) ? p->now_ms(p->now_user) : 0u;
    const uint32_t seq = ems_proto_rd_u32(&payload[0]);

    if (p->session_active && p->now_ms != NULL &&
        ((uint32_t)(now - p->last_tick_ms) > EMS_PROTO_INJECT_GAP_MS)) {
        p->stale = 1u;
        ems_proto_event_push(p, EMS_EVT_STALE, 0u, p->last_tick_idx);
    } else {
        p->stale = 0u; /* 正常拍（间隔 ≤2 s）清除 STALE（design：下一拍恢复） */
    }
    p->session_active = 1u;
    p->last_tick_ms = now;
    p->last_tick_idx = seq;
}

/* CMD_RESET 语义：清 STALE/会话/事件环（控制状态复位由 ems_ctrl_init 承担） */
void ems_proto_session_reset(ems_proto_t *p)
{
    p->session_active = 0u;
    p->stale = 0u;
    p->last_tick_ms = 0u;
    p->last_tick_idx = EMS_PROTO_TICK_NONE;
    p->ev_head = 0u;
    p->ev_count = 0u;
    p->ev_total = 0u;
}

uint8_t ems_proto_session_stale(const ems_proto_t *p)
{
    return p->stale;
}

/* ---- RSP_TICK / RSP_INFER_TEST 编解码 ---- */
void ems_proto_enc_rsp_tick(uint8_t out[EMS_PROTO_RSP_TICK_LEN],
                            const ems_proto_rsp_tick_t *r)
{
    ems_proto_wr_u32(&out[0], r->tick_idx);
    ems_proto_wr_f32(&out[4], r->P_gen_ref);
    ems_proto_wr_f32(&out[8], r->e_hat);
    ems_proto_wr_f32(&out[12], r->K);
    out[16] = r->cls_eff;
    ems_proto_wr_u32(&out[17], r->infer_us);
    ems_proto_wr_u32(&out[21], r->ctrl_us);
    out[25] = r->flags;
}

void ems_proto_enc_rsp_infer(uint8_t out[EMS_PROTO_RSP_INFER_LEN], uint32_t seq,
                             const float q_kwh[3], uint32_t infer_us)
{
    ems_proto_wr_u32(&out[0], seq); /* 载荷级 seq 回显（2026-09-01，帧头 +4B） */
    ems_proto_wr_f32(&out[4], q_kwh[0]);
    ems_proto_wr_f32(&out[8], q_kwh[1]);
    ems_proto_wr_f32(&out[12], q_kwh[2]);
    ems_proto_wr_u32(&out[16], infer_us); /* [16..19]；RSP_INFER_LEN=20 契约 */
}

void ems_proto_enc_rsp_wc(uint8_t out[EMS_PROTO_RSP_WC_LEN], uint32_t seq,
                          uint8_t cls, const float logits[7], uint32_t us)
{
    ems_proto_wr_u32(&out[0], seq);
    out[4] = cls;
    out[5] = 0u; /* 3B 保留填 0（design 帧表 0x09） */
    out[6] = 0u;
    out[7] = 0u;
    for (int i = 0; i < 7; i++) {
        ems_proto_wr_f32(&out[8u + 4u * (uint32_t)i], logits[i]);
    }
    ems_proto_wr_u32(&out[36], us);
}

int ems_proto_dec_cmd_tick(const uint8_t *payload, uint16_t len,
                           ems_proto_cmd_tick_t *out)
{
    if (len != EMS_PROTO_TICK_LEN) {
        return -1;
    }
    out->tick_idx = ems_proto_rd_u32(&payload[0]);
    for (int i = 0; i < (int)EMS_PROTO_SIG_N; i++) {
        out->sig[i] = ems_proto_rd_f32(&payload[4u + 4u * (uint32_t)i]);
    }
    return 0;
}

int ems_proto_dec_rsp_tick(const uint8_t *payload, uint16_t len,
                           ems_proto_rsp_tick_t *out)
{
    if (len != EMS_PROTO_RSP_TICK_LEN) {
        return -1;
    }
    out->tick_idx = ems_proto_rd_u32(&payload[0]);
    out->P_gen_ref = ems_proto_rd_f32(&payload[4]);
    out->e_hat = ems_proto_rd_f32(&payload[8]);
    out->K = ems_proto_rd_f32(&payload[12]);
    out->cls_eff = payload[16];
    out->infer_us = ems_proto_rd_u32(&payload[17]);
    out->ctrl_us = ems_proto_rd_u32(&payload[21]);
    out->flags = payload[25];
    return 0;
}