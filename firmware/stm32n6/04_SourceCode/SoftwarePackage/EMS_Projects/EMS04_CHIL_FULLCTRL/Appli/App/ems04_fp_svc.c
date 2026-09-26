/* ems04_fp_svc.c — EMS04-S2 B1：特征面板服务状态机实现。
 *
 * 分发（PROTOCOL_FP §1–§5）：
 *   0x20 VOYAGE_INIT → ACK                （先隐式复位，冷启动语义）
 *   0x21 KF_CHUNK    → ACK                （TABLE 态；连续性/越界/hash 逐条 fail-closed）
 *   0x22 TICK2       → ACK                （RUN 态；§3 逐条校验）
 *   0x23 FP_QUERY    → 0x24 RSP_FP_QUERY  （构造 + 查表 + 锁存）
 *   0x25 FP_DUMP     → 0x26 RSP_FP_DUMP   （仅回最近 QUERY 锁存字节）
 *   其他 TYPE        → NACK ERR_TYPE
 *
 * 检查次序固化（PROTOCOL_FP §4 列表序）：状态 → t0 整数性 → 未满 301 拍 → t0 网格区间
 * → 锚点样本齐备（t0 == last_tick，环深 301）→ kf 行查找。
 */
#include "ems04_fp_svc.h"

#include <math.h>
#include <string.h>

#include "ems04_sha256.h"

/* --------------------------------------------------------------- 内部工具 */
static int err_index(uint8_t code)
{
    if (code >= EMS04_FP_ERR_STATE && code <= EMS04_FP_ERR_BOUNDS) {
        return (int)(code - EMS04_FP_ERR_STATE) + 3;
    }
    if (code >= EMS04_FP_ERR_CRC && code <= EMS04_FP_ERR_LEN) {
        return (int)(code - EMS04_FP_ERR_CRC);
    }
    return -1;
}

static void svc_emit(ems04_fp_svc_t *s, size_t n)
{
    s->cnt[EMS04_FP_CNT_RSP_TOTAL]++;
    if (s->sink != NULL && n > 0u) {
        s->sink(s->sink_ctx, s->rsp, n);
    }
}

static void svc_emit_ack(ems04_fp_svc_t *s, uint8_t cmd_type)
{
    size_t n = ems04_fp_encode_ack(cmd_type, s->rsp, sizeof(s->rsp));
    s->cnt[EMS04_FP_CNT_ACK]++;
    svc_emit(s, n);
}

/* fail-closed：NACK + 会话级错误锁存（仅 VOYAGE_INIT 可清） */
static void svc_emit_nack(ems04_fp_svc_t *s, uint8_t cmd_type, uint8_t err)
{
    size_t n = ems04_fp_encode_nack(cmd_type, err, s->rsp, sizeof(s->rsp));
    int    idx = err_index(err);

    s->cnt[EMS04_FP_CNT_NACK]++;
    if (idx >= 0) {
        s->err_counts[idx]++;
    }
    s->latched = 1u;
    svc_emit(s, n);
}

/* ------------------------------------------------------------ 会话子状态 */
static void svc_clear_session(ems04_fp_svc_t *s)
{
    s->voyage_id[0] = '\0';
    s->tick_first = 0u;
    s->tick_last = 0u;
    s->n_ticks = 0u;
    s->kf_n_rows = 0u;
    s->kf_issue_first = 0.0;
    s->kf_issue_step = 0.0;
    s->water_depth_m = 0.0;
    s->ctrl_policy = 0u;
    s->manual_power_rule = 0u;
    s->state = EMS04_FP_SVC_COLD;
    s->latched = 0u;
    s->has_query = 0u;
    s->last_query_t0 = 0.0;
    memset(s->hist_latch, 0, sizeof(s->hist_latch));
    memset(s->kf_latch, 0, sizeof(s->kf_latch));
    /* RX 解析状态复位；frames_ok/crc_errors/junk_bytes 作为链路诊断累计保留 */
    ems04_fp_rx_reset_state(&s->rx);
    ems04_h1win_reset(&s->win);
    ems04_kftable_reset(&s->kf);
}

void ems04_fp_svc_init(ems04_fp_svc_t *s, ems04_fp_svc_sink_fn sink, void *ctx)
{
    memset(s, 0, sizeof(*s));
    s->sink = sink;
    s->sink_ctx = ctx;
    svc_clear_session(s);
}

void ems04_fp_svc_reset(ems04_fp_svc_t *s)
{
    ems04_fp_svc_sink_fn sink = s->sink;
    void *ctx = s->sink_ctx;
    memset(s, 0, sizeof(*s));
    s->sink = sink;
    s->sink_ctx = ctx;
    svc_clear_session(s);
}

uint32_t ems04_fp_svc_err_count(const ems04_fp_svc_t *s, uint8_t code)
{
    int idx = err_index(code);
    return (idx < 0) ? 0u : s->err_counts[idx];
}

uint64_t ems04_fp_svc_counter(const ems04_fp_svc_t *s, uint32_t id)
{
    /* 5..11 直接从子模块实时取值（直调路径不经过 feed，缓存值会陈旧） */
    switch (id) {
    case EMS04_FP_CNT_RX_FRAMES:       return (uint64_t)s->rx.frames_ok;
    case EMS04_FP_CNT_RX_CRC_ERR:      return (uint64_t)s->rx.crc_errors;
    case EMS04_FP_CNT_RX_JUNK_BYTES:   return (uint64_t)s->rx.junk_bytes;
    case EMS04_FP_CNT_TICKS_OK:        return s->win.ticks_ok;
    case EMS04_FP_CNT_CHUNKS_OK:       return s->kf.chunks_ok;
    case EMS04_FP_CNT_CONSTRUCTS_OK:   return s->win.constructs_ok;
    case EMS04_FP_CNT_KF_LOOKUPS_OK:   return s->kf.lookups_ok;
    default:                           break;
    }
    return (id < (uint32_t)EMS04_FP_CNT_COUNT) ? s->cnt[id] : 0u;
}

/* ------------------------------------------------------------ 命令处理体 */
static int svc_voyage_init(ems04_fp_svc_t *s, const uint8_t *payload, uint32_t len)
{
    ems04_fp_voyage_init_t vi;
    int rc = ems04_fp_dec_voyage_init(payload, len, &vi);

    if (rc != 0) {
        return rc;
    }
    if (vi.protocol_version != EMS04_FP_PROTOCOL_VERSION ||
        vi.feature_contract_version != EMS04_FP_FEATURE_CONTRACT_VERSION) {
        return (int)EMS04_FP_ERR_STATE; /* 版本会话级不符 */
    }
    if (vi.ctrl_policy != EMS04_FP_POLICY_AUTO) {
        return (int)EMS04_FP_ERR_POLICY; /* 仅 AUTO 声明合法（O-9 收口） */
    }
    if (vi.manual_power_rule != EMS04_FP_MANUAL_RULE_ZERO_FILL) {
        return (int)EMS04_FP_ERR_POLICY; /* 仅 ZERO_FILL 规则合法（DGN-06） */
    }
    if (vi.n_ticks != (vi.tick_last - vi.tick_first) + 1u ||
        vi.tick_last < vi.tick_first) {
        return (int)EMS04_FP_ERR_STATE; /* tick 网格声明不一致 */
    }
    if (!isfinite(vi.water_depth_m) || !isfinite(vi.kf_issue_first) ||
        !isfinite(vi.kf_issue_step) || !(vi.kf_issue_step > 0.0)) {
        return (int)EMS04_FP_ERR_NONFINITE;
    }
    if (vi.kf_n_rows == 0u || vi.kf_n_rows > EMS04_KF_MAX_ROWS) {
        return (int)EMS04_FP_ERR_BOUNDS; /* 表缓冲越界 */
    }

    /* 重复 INIT = 先隐式复位②状态机（清空环/表/锁存）再接受（冷启动语义） */
    svc_clear_session(s);
    memcpy(s->voyage_id, vi.voyage_id, sizeof(s->voyage_id));
    s->tick_first = vi.tick_first;
    s->tick_last = vi.tick_last;
    s->n_ticks = vi.n_ticks;
    s->kf_n_rows = vi.kf_n_rows;
    s->kf_issue_first = vi.kf_issue_first;
    s->kf_issue_step = vi.kf_issue_step;
    s->water_depth_m = vi.water_depth_m;
    s->ctrl_policy = vi.ctrl_policy;
    s->manual_power_rule = vi.manual_power_rule;

    rc = ems04_kftable_begin(&s->kf, vi.kf_n_rows, vi.kf_issue_first, vi.kf_issue_step,
                             vi.kf_payload_sha256);
    if (rc != 0) {
        return rc;
    }
    rc = ems04_h1win_begin(&s->win, vi.tick_first, vi.water_depth_m,
                           (double)vi.ctrl_policy, vi.manual_power_rule);
    if (rc != 0) {
        return rc;
    }
    s->state = EMS04_FP_SVC_TABLE;
    return 0;
}

static int svc_kf_chunk(ems04_fp_svc_t *s, const uint8_t *payload, uint32_t len)
{
    ems04_fp_kf_chunk_t c;
    int rc;

    if (s->state != EMS04_FP_SVC_TABLE) {
        return (int)EMS04_FP_ERR_STATE; /* 未 INIT / 表已就绪 */
    }
    rc = ems04_fp_dec_kf_chunk(payload, len, &c);
    if (rc != 0) {
        return rc;
    }
    rc = ems04_kftable_chunk(&s->kf, &c);
    if (rc != 0) {
        return rc;
    }
    if (s->kf.ready != 0u) {
        s->state = EMS04_FP_SVC_RUN;
    }
    return 0;
}

static int svc_tick2(ems04_fp_svc_t *s, const uint8_t *payload, uint32_t len)
{
    ems04_fp_tick2_t tk;
    int rc;

    if (s->state != EMS04_FP_SVC_RUN) {
        return (int)EMS04_FP_ERR_STATE; /* 未 INIT 或 kf 表未就绪 */
    }
    rc = ems04_fp_dec_tick2(payload, len, &tk);
    if (rc != 0) {
        return rc;
    }
    return ems04_h1win_tick(&s->win, tk.tick_idx, tk.v);
}

static int svc_fp_query(ems04_fp_svc_t *s, const uint8_t *payload, uint32_t len)
{
    double   t0;
    int      rc;
    const uint8_t *row = NULL;
    uint8_t  hist_sha[32];
    uint8_t  kf_sha[32];
    size_t   n;

    if (s->state != EMS04_FP_SVC_RUN) {
        return (int)EMS04_FP_ERR_STATE;
    }
    rc = ems04_fp_dec_t0(payload, len, &t0);
    if (rc != 0) {
        return rc;
    }
    if (!isfinite(t0) || !(t0 == floor(t0))) {
        return (int)EMS04_FP_ERR_TICK_T; /* 非整数锚点 */
    }
    if (s->win.has_tick == 0u || s->win.last_tick < EMS04_H1_ROWS) {
        return (int)EMS04_FP_ERR_NOT_READY; /* 未收满 301 拍 */
    }
    if (t0 < (double)EMS04_H1_ROWS || t0 > (double)s->tick_last) {
        return (int)EMS04_FP_ERR_TICK_T; /* 不在 [300, tick_last] */
    }
    if (t0 > (double)s->win.last_tick) {
        return (int)EMS04_FP_ERR_NOT_READY; /* 锚点样本尚未到齐 */
    }
    rc = ems04_h1win_construct(&s->win, (uint32_t)t0, s->hist_latch);
    if (rc != 0) {
        return rc;
    }
    rc = ems04_kftable_lookup(&s->kf, t0, &row);
    if (rc != 0) {
        return rc;
    }
    memcpy(s->kf_latch, row, EMS04_FP_KF_BYTES);
    s->last_query_t0 = t0;
    s->has_query = 1u;

    ems04_sha256(s->hist_latch, EMS04_H1_BYTES, hist_sha);
    ems04_sha256(s->kf_latch, EMS04_FP_KF_BYTES, kf_sha);
    n = ems04_fp_encode_rsp_query(t0, hist_sha, kf_sha, s->rsp, sizeof(s->rsp));
    s->cnt[EMS04_FP_CNT_RSP_QUERY]++;
    svc_emit(s, n);
    return 0;
}

static int svc_fp_dump(ems04_fp_svc_t *s, const uint8_t *payload, uint32_t len)
{
    double t0;
    int    rc;
    size_t n;

    if (s->state != EMS04_FP_SVC_RUN) {
        return (int)EMS04_FP_ERR_STATE;
    }
    rc = ems04_fp_dec_t0(payload, len, &t0);
    if (rc != 0) {
        return rc;
    }
    if (s->has_query == 0u) {
        return (int)EMS04_FP_ERR_STATE; /* DUMP 前未 QUERY */
    }
    if (!(t0 == s->last_query_t0)) {
        return (int)EMS04_FP_ERR_STATE; /* t0 与最近 QUERY 不符 */
    }
    n = ems04_fp_encode_rsp_dump(t0, s->hist_latch, s->kf_latch, s->rsp, sizeof(s->rsp));
    s->cnt[EMS04_FP_CNT_RSP_DUMP]++;
    svc_emit(s, n);
    return 0;
}

/* --------------------------------------------------------------- 帧分发 */
static void svc_dispatch(ems04_fp_svc_t *s, uint8_t type, const uint8_t *payload, uint32_t len)
{
    int rc = 0;

    /* 锁存态：仅 VOYAGE_INIT（冷启动）可继续，其余一律 ERR_STATE */
    if (s->latched != 0u && type != EMS04_FP_CMD_VOYAGE_INIT) {
        svc_emit_nack(s, type, (uint8_t)EMS04_FP_ERR_STATE);
        return;
    }

    switch (type) {
    case EMS04_FP_CMD_VOYAGE_INIT:
        rc = svc_voyage_init(s, payload, len);
        if (rc == 0) {
            svc_emit_ack(s, type);
        } else {
            svc_emit_nack(s, type, (uint8_t)rc);
        }
        return;

    case EMS04_FP_CMD_KF_CHUNK:
        rc = svc_kf_chunk(s, payload, len);
        if (rc == 0) {
            s->cnt[EMS04_FP_CNT_CHUNKS_OK] = s->kf.chunks_ok;
            svc_emit_ack(s, type);
        } else {
            svc_emit_nack(s, type, (uint8_t)rc);
        }
        return;

    case EMS04_FP_CMD_TICK2:
        rc = svc_tick2(s, payload, len);
        if (rc == 0) {
            s->cnt[EMS04_FP_CNT_TICKS_OK] = s->win.ticks_ok;
            svc_emit_ack(s, type);
        } else {
            svc_emit_nack(s, type, (uint8_t)rc);
        }
        return;

    case EMS04_FP_CMD_FP_QUERY:
        rc = svc_fp_query(s, payload, len);
        if (rc != 0) {
            svc_emit_nack(s, type, (uint8_t)rc);
        }
        return;

    case EMS04_FP_CMD_FP_DUMP:
        rc = svc_fp_dump(s, payload, len);
        if (rc != 0) {
            svc_emit_nack(s, type, (uint8_t)rc);
        }
        return;

    default:
        svc_emit_nack(s, type, (uint8_t)EMS04_FP_ERR_TYPE);
        return;
    }
}

void ems04_fp_svc_feed(ems04_fp_svc_t *s, const uint8_t *bytes, size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        int r = ems04_fp_rx_push(&s->rx, bytes[i]);

        if (r == (int)EMS04_FP_RX_FRAME) {
            uint8_t  type = s->rx.type;
            uint32_t plen = s->rx.len;

            s->cnt[EMS04_FP_CNT_RX_FRAMES] = s->rx.frames_ok;
            svc_dispatch(s, type, s->rx.payload, plen);
        } else if (r == (int)EMS04_FP_RX_ERR) {
            uint8_t err = s->rx.err;
            uint8_t type = s->rx.type;

            s->cnt[EMS04_FP_CNT_RX_CRC_ERR] = s->rx.crc_errors;
            s->cnt[EMS04_FP_CNT_RX_JUNK_BYTES] = s->rx.junk_bytes;
            svc_emit_nack(s, type, err);
        } else {
            s->cnt[EMS04_FP_CNT_RX_JUNK_BYTES] = s->rx.junk_bytes;
        }
    }
    s->cnt[EMS04_FP_CNT_RX_FRAMES] = s->rx.frames_ok;
    s->cnt[EMS04_FP_CNT_RX_CRC_ERR] = s->rx.crc_errors;
    s->cnt[EMS04_FP_CNT_RX_JUNK_BYTES] = s->rx.junk_bytes;
    s->cnt[EMS04_FP_CNT_TICKS_OK] = s->win.ticks_ok;
    s->cnt[EMS04_FP_CNT_CHUNKS_OK] = s->kf.chunks_ok;
    s->cnt[EMS04_FP_CNT_CONSTRUCTS_OK] = s->win.constructs_ok;
    s->cnt[EMS04_FP_CNT_KF_LOOKUPS_OK] = s->kf.lookups_ok;
}
