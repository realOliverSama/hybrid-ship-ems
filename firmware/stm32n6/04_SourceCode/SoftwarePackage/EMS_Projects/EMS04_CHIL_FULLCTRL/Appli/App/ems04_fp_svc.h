/* ems04_fp_svc.h — EMS04-S2 B1：特征面板服务状态机（板侧入口）。
 *
 * 契约：FEATURE_PANEL_CONTRACT.md §2/§3/§6.1；PROTOCOL_FP.md §1–§7。
 *
 * 状态机：COLD →（VOYAGE_INIT）→ TABLE →（收满并校验 kf 表）→ RUN
 *         RUN 内可 QUERY（构造并锁存最近特征帧）/ DUMP（回锁存字节）。
 * 错误锁存：任何 fail-closed → NACK + latched=1；latched 时除 VOYAGE_INIT（冷启动语义，
 *           先隐式复位再接受）外一律 NACK ERR_FP_STATE；锁存期间不产出任何特征字节。
 *
 * 本文件同时是 C 阶段（板级 RTT 服务）的复用件：只依赖 ems04_* 四个模块 + stdio-free。
 */
#ifndef EMS04_FP_SVC_H
#define EMS04_FP_SVC_H

#include <stddef.h>
#include <stdint.h>

#include "ems04_fp_proto.h"
#include "ems04_h1win.h"
#include "ems04_kftable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EMS04_FP_SVC_COLD  = 0u, /* 无会话 */
    EMS04_FP_SVC_TABLE = 1u, /* VOYAGE_INIT 已接受，等 kf 表 */
    EMS04_FP_SVC_RUN   = 2u  /* 表就绪，收 tick / 可 QUERY / DUMP */
} ems04_fp_svc_state_t;

/* 诊断计数 id（报告/单测读数；不参与语义） */
enum {
    EMS04_FP_CNT_RSP_TOTAL = 0,
    EMS04_FP_CNT_ACK,
    EMS04_FP_CNT_NACK,
    EMS04_FP_CNT_RSP_QUERY,
    EMS04_FP_CNT_RSP_DUMP,
    EMS04_FP_CNT_RX_FRAMES,
    EMS04_FP_CNT_RX_CRC_ERR,
    EMS04_FP_CNT_RX_JUNK_BYTES,
    EMS04_FP_CNT_TICKS_OK,
    EMS04_FP_CNT_CHUNKS_OK,
    EMS04_FP_CNT_CONSTRUCTS_OK,
    EMS04_FP_CNT_KF_LOOKUPS_OK,
    EMS04_FP_CNT_COUNT
};

typedef void (*ems04_fp_svc_sink_fn)(void *ctx, const uint8_t *bytes, size_t len);

typedef struct {
    /* --- 会话声明（VOYAGE_INIT） --- */
    char     voyage_id[33];
    uint32_t tick_first;
    uint32_t tick_last;
    uint32_t n_ticks;
    uint32_t kf_n_rows;
    double   kf_issue_first;
    double   kf_issue_step;
    double   water_depth_m;
    uint8_t  ctrl_policy;
    uint8_t  manual_power_rule;

    ems04_fp_svc_state_t state;
    uint8_t  latched;

    /* --- 最近特征帧锁存（QUERY 产出，DUMP 回读） --- */
    uint8_t  hist_latch[EMS04_H1_BYTES];
    uint8_t  kf_latch[EMS04_FP_KF_BYTES];
    double   last_query_t0;
    uint8_t  has_query;

    /* --- 子模块（大缓冲，实例必须为静态/全局，勿置于栈） --- */
    ems04_fp_rx_t      rx;
    ems04_h1win_t      win;
    ems04_kftable_t    kf;

    /* --- 响应暂存 + 出口 --- */
    uint8_t  rsp[EMS04_FP_RSP_DUMP_LEN + EMS04_FP_FRAME_OVERHEAD];
    ems04_fp_svc_sink_fn sink;
    void    *sink_ctx;

    /* --- 计数（诊断/报告只读） --- */
    uint64_t cnt[EMS04_FP_CNT_COUNT];
    uint32_t err_counts[12]; /* 0x01..0x03 → [0..2]；0x40..0x48 → [3..11] */
} ems04_fp_svc_t;

void ems04_fp_svc_init(ems04_fp_svc_t *s, ems04_fp_svc_sink_fn sink, void *ctx);
void ems04_fp_svc_reset(ems04_fp_svc_t *s); /* 冷启动：清会话/环/表/锁存与计数 */

/* 字节流入口：喂入任意切片；内部逐字节驱动 RX 状态机并分发。 */
void ems04_fp_svc_feed(ems04_fp_svc_t *s, const uint8_t *bytes, size_t len);

uint32_t ems04_fp_svc_err_count(const ems04_fp_svc_t *s, uint8_t code);
uint64_t ems04_fp_svc_counter(const ems04_fp_svc_t *s, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_FP_SVC_H */
