/* ems04_kftable.c — EMS04-S2 B1：kf 航次表缓冲 / 分块重组 / 精确查表。
 *
 * fail-closed 面：未开表 / chunk 乱序 / 越界 / 长度与声明不符 / sha 不符 /
 * 网格不合 / 行越界 / 行内 issue_time_s 不符 —— 全部返回 err 码，绝不产出字节。
 */
#include "ems04_kftable.h"

#include <math.h>
#include <string.h>

#include "ems04_sha256.h"

void ems04_kftable_reset(ems04_kftable_t *t)
{
    memset(t, 0, sizeof(*t));
}

int ems04_kftable_begin(ems04_kftable_t *t, uint32_t n_rows, double issue_first,
                        double issue_step, const uint8_t declared_sha256[32])
{
    if (n_rows == 0u || n_rows > EMS04_KF_MAX_ROWS) {
        return (int)EMS04_FP_ERR_BOUNDS;   /* 行数越界（板上限 512） */
    }
    if (!isfinite(issue_first) || !isfinite(issue_step) || !(issue_step > 0.0)) {
        return (int)EMS04_FP_ERR_BOUNDS;   /* 网格非法 */
    }
    ems04_kftable_reset(t);
    t->n_rows       = n_rows;
    t->issue_first  = issue_first;
    t->issue_step   = issue_step;
    t->next_chunk   = 0u;
    t->used_bytes   = 0u;
    t->started      = 1u;
    t->ready        = 0u;
    memcpy(t->declared_sha256, declared_sha256, 32u);
    return 0;
}

int ems04_kftable_chunk(ems04_kftable_t *t, const ems04_fp_kf_chunk_t *c)
{
    uint32_t off;
    uint32_t len;

    if (t->started == 0u) {
        return (int)EMS04_FP_ERR_STATE;    /* 未 INIT / 未开表 */
    }
    if (t->ready != 0u) {
        return (int)EMS04_FP_ERR_STATE;    /* 表已收满，重复上传 */
    }
    if (c->total_chunks == 0u || c->chunk_idx >= c->total_chunks) {
        return (int)EMS04_FP_ERR_ORDER;
    }
    if (t->next_chunk == 0u) {
        t->total_chunks = c->total_chunks;
    } else if (c->total_chunks != t->total_chunks) {
        return (int)EMS04_FP_ERR_ORDER;    /* total_chunks 前后不一致 */
    }
    if (c->chunk_idx != t->next_chunk) {
        return (int)EMS04_FP_ERR_ORDER;    /* 乱序 / 跳号 / 重复 */
    }

    len = (uint32_t)c->chunk_len;
    if (len > EMS04_KF_BUF_BYTES - t->used_bytes) {
        return (int)EMS04_FP_ERR_BOUNDS;   /* 缓冲越界（512×728 上限） */
    }
    off = t->used_bytes;
    if (len > 0u) {
        memcpy(t->buf + off, c->payload, len);
    }
    t->used_bytes += len;
    t->next_chunk  = c->chunk_idx + 1u;
    t->chunks_ok++;

    if (t->next_chunk == t->total_chunks) {
        uint8_t sha[32];
        uint32_t expect_bytes;

        if (t->n_rows > EMS04_KF_MAX_ROWS) {
            return (int)EMS04_FP_ERR_BOUNDS;
        }
        expect_bytes = t->n_rows * EMS04_KF_ROW_BYTES;
        if (t->used_bytes != expect_bytes) {
            return (int)EMS04_FP_ERR_BOUNDS; /* 重组长度与声明行数不符 */
        }
        ems04_sha256(t->buf, (size_t)t->used_bytes, sha);
        if (memcmp(sha, t->declared_sha256, 32u) != 0) {
            return (int)EMS04_FP_ERR_KF_HASH; /* 与 VOYAGE_INIT 声明值不符 */
        }
        t->ready = 1u;
    }
    return 0;
}

int ems04_kftable_lookup(ems04_kftable_t *t, double t0, const uint8_t **row720)
{
    double   d;
    double   q;
    uint32_t row;
    double   issue_time_s;

    if (t->ready == 0u) {
        return (int)EMS04_FP_ERR_STATE;    /* 表未就绪 */
    }
    d = t0 - t->issue_first;
    if (!(d >= 0.0)) {
        return (int)EMS04_FP_ERR_KF_ROW;   /* 锚点早于 issue_first（含 NaN） */
    }
    if (!(fmod(d, t->issue_step) == 0.0)) {
        return (int)EMS04_FP_ERR_KF_ROW;   /* 不在 issue 网格上（精确判等） */
    }
    q = d / t->issue_step;
    if (!(q >= 0.0) || !(q < (double)t->n_rows)) {
        return (int)EMS04_FP_ERR_KF_ROW;   /* 行越界 */
    }
    row = (uint32_t)q;
    issue_time_s = ems04_le_f64(t->buf + (size_t)row * EMS04_KF_ROW_BYTES);
    if (!(issue_time_s == t0)) {
        return (int)EMS04_FP_ERR_KF_ROW;   /* 行时标不符（禁就近取行） */
    }
    *row720 = t->buf + (size_t)row * EMS04_KF_ROW_BYTES + 8u;
    t->lookups_ok++;
    return 0;
}
