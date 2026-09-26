/* ems04_kftable.h — EMS04-S2 B1：kf 航次表（路线 A 预计算表）板侧缓冲 + 精确查表。
 *
 * 契约：FEATURE_PANEL_CONTRACT.md §3（路线 A / 精确 & fail-closed 查表）、
 *       §2.2（CMD_KF_CHUNK 分块上传 + 整体 SHA-256）；PROTOCOL_FP.md §2。
 *
 * 缓冲：静态 512 行 × 728 B（f64 issue_time_s + 720 B kf），最大 372,736 B。
 * 行内 kf = (30,6) f32 LE C-order（k-major），板上内容 = host 打包字节本身。
 *
 * 查表公式（§3，禁就近取行/插值/缺行回退）：
 *   d = t0 − issue_first ≥ 0；fmod(d, issue_step) == 0.0（精确）；
 *   row = d / issue_step < n_rows；行内 issue_time_s 与 t0 f64 位级相等 → 命中。
 */
#ifndef EMS04_KFTABLE_H
#define EMS04_KFTABLE_H

#include <stddef.h>
#include <stdint.h>

#include "ems04_fp_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_KF_MAX_ROWS     512u
#define EMS04_KF_PAYLOAD_BYTES 720u                                  /* 30×6 f32 */
#define EMS04_KF_ROW_BYTES    (8u + EMS04_KF_PAYLOAD_BYTES)          /* 728 */
#define EMS04_KF_BUF_BYTES    (EMS04_KF_MAX_ROWS * EMS04_KF_ROW_BYTES) /* 372736 */

typedef struct {
    uint8_t  buf[EMS04_KF_BUF_BYTES];
    uint8_t  declared_sha256[32];
    double   issue_first;
    double   issue_step;
    uint32_t n_rows;        /* VOYAGE_INIT 声明行数 */
    uint32_t total_chunks;  /* 首个 chunk 声明值（其后必须一致） */
    uint32_t next_chunk;    /* 期望的下一 chunk_idx（连续性） */
    uint32_t used_bytes;    /* 已重组字节数 */
    uint8_t  started;
    uint8_t  ready;         /* 收满 + sha 校验通过 */
    uint64_t chunks_ok;
    uint64_t lookups_ok;
} ems04_kftable_t;

void ems04_kftable_reset(ems04_kftable_t *t);

/* VOYAGE_INIT 侧：开表（清空缓冲，落声明值）。返回 0 / err 码。 */
int ems04_kftable_begin(ems04_kftable_t *t, uint32_t n_rows, double issue_first,
                        double issue_step, const uint8_t declared_sha256[32]);

/* CMD_KF_CHUNK：连续性 + 越界检查 → 追加；收满后 sha256 校验。返回 0 / err 码。 */
int ems04_kftable_chunk(ems04_kftable_t *t, const ems04_fp_kf_chunk_t *c);

/* 精确查表：命中 → *row720 指向行内 720 B（表内生命周期）；失败 → err 码。 */
int ems04_kftable_lookup(ems04_kftable_t *t, double t0, const uint8_t **row720);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_KFTABLE_H */
