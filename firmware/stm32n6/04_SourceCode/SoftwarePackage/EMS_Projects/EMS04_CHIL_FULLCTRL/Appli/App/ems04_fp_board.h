/* ems04_fp_board.h — EMS04-S2 C-fw：FP 服务 ↔ 既有 ems_proto UART 层的胶水。
 *
 * 契约来源（本文件不新增语义，只做接线）：
 *   - `../FEATURE_PANEL_CONTRACT.md` §5.3（C 阶段形态：UART 115200 + 复用 ems_proto 帧容器）
 *   - `b/PROTOCOL_FP.md`（线格式权威：帧容器 [AA 55][TYPE][LEN LE16][PAYLOAD][CRC16 LE]，
 *     命令码 0x20..0x26，CRC16-CCITT-FALSE 作用域 TYPE..PAYLOAD）
 *   - `b/ems04_fp_svc.h`（特征面板服务状态机；本文件逐字节复用该模块，不改其语义）
 *
 * 接线方式（一条命令 = 一次 handle 调用）：
 *   ems_proto 帧状态机已完成**线上帧的同步/LEN/CRC 校验**（既有层，不重复造），
 *   其 on_frame 回调把「TYPE + 载荷」交给本模块；本模块把该载荷重新装进
 *   PROTOCOL_FP §0 帧容器（同步字节 + TYPE/LEN + CRC 由既有 ems_proto CRC 原语
 *   ems_proto_crc16/_chain 现算）喂 `ems04_fp_svc_feed`，服务产出的响应帧
 *   （ACK 0x7F / NACK 0x7E / RSP_FP_QUERY 0x24 / RSP_FP_DUMP 0x26）由本模块原样
 *   交回调用方（字节 = 服务编码器输出，零重编码/零改写）。
 *
 * 为什么响应不走 ems_proto_send_frame()：RSP_FP_DUMP 载荷 13932 B 比既有
 * EMS_PROTO_MAX_PAYLOAD（13924 B，REATT 帧定义的既有上限）大 8 B，既有编码器会
 * 拒发。改既有上限会动到 EMS01/EMS03/EMS04 共用文件的常量与缓冲尺寸，故本形态
 * 的响应帧由 FP 编码器产出、调用方直发 UART（帧格式与 ems_proto 逐字节同构）。
 *
 * 内存：静态实例 ≈ 486 KB BSS（RX 载荷缓冲 64 KB + kf 表 372.7 KB + H1 环 19.3 KB
 * + 锁存/响应缓冲 ~28 KB），必须置于静态存储（勿置于栈）；本形态仅随
 * `EMS04_FP_PARITY=1`（Release + LRUN_RAMxspi1 精简形态）编入。
 */
#ifndef EMS04_FP_BOARD_H
#define EMS04_FP_BOARD_H

#include <stddef.h>
#include <stdint.h>

#include "ems04_fp_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 响应缓冲上限 = 最大响应帧（RSP_FP_DUMP）= 13932 + 7 = 13939 B。
 * `ems04_fp_board_handle` 的 `rsp` 必须至少这么大（调用方按本宏声明缓冲）。 */
#define EMS04_FP_BOARD_RSP_MAX \
    (EMS04_FP_RSP_DUMP_LEN + EMS04_FP_FRAME_OVERHEAD)

/* 返回码 */
#define EMS04_FP_BOARD_OK        0  /* 已接管：*rsp_len 为响应帧字节数 */
#define EMS04_FP_BOARD_UNHANDLED (-1) /* TYPE 不在 0x20..0x26（调用方按既有路径处理） */
#define EMS04_FP_BOARD_BADARG    (-2) /* 参数非法（rsp/rsp_len 为空等） */
#define EMS04_FP_BOARD_OVERFLOW  (-3) /* 响应超缓冲（fail-closed，应不可达） */
#define EMS04_FP_BOARD_NORSP     (-4) /* 无响应产出（不可达；计数留痕） */

typedef struct {
    uint64_t commands;      /* 交给 FP 服务的命令数（= UART 帧数，CRC 已过） */
    uint64_t responses;     /* 产出并交回的响应帧数 */
    uint64_t response_bytes;/* 交回的响应字节数 */
    uint64_t bad_args;      /* 参数非法计数 */
    uint64_t overflow;      /* 响应超缓冲计数（应为 0） */
    uint64_t no_response;   /* 无响应产出计数（应为 0） */
    uint64_t unhandled;     /* 非 0x20..0x26 的 TYPE 计数 */
    /* 下述五项 = 服务侧计数（透传，诊断用） */
    uint64_t svc_rx_frames; /* FP 服务 RX 状态机合法帧数 */
    uint64_t ack;           /* ACK 数 */
    uint64_t nack;          /* NACK 数（非零即链路/协议异常） */
    uint64_t rsp_query;     /* RSP_FP_QUERY 数 */
    uint64_t rsp_dump;      /* RSP_FP_DUMP 数 */
    uint32_t err[12];       /* err 码计数，索引同 ems04_fp_svc（0x01..0x03 → 0..2，
                             * 0x40..0x48 → 3..11） */
} ems04_fp_board_stats_t;

/* 冷启动初始化（静态实例 + sink 绑定 + 计数器清零）。
 * 幂等：可重复调用（等价 ems04_fp_svc_init 语义）。 */
void ems04_fp_board_init(void);

/* 会话复位透传（等价 ems04_fp_svc_reset：清会话/环/表/锁存与计数）。
 * 注意：线上会话的冷启动复位由 CMD_VOYAGE_INIT(0x20) 的内部语义承担
 * （PROTOCOL_FP §1「重复 INIT → 先隐式复位②状态机」），本入口仅供诊断/自检。 */
void ems04_fp_board_reset(void);

/* TYPE 是否属②命令段（0x20..0x26 中的 5 条命令码）。 */
int ems04_fp_board_is_fp_type(uint8_t type);

/* 处理一条完整命令（载荷已由 ems_proto 校验 CRC/LEN）。
 * 返回 EMS04_FP_BOARD_*；成功时 *rsp_len = 响应帧字节数（响应帧含 AA55/TYPE/LEN/CRC，
 * 由调用方直发 UART）。响应缓冲 rsp 容量必须 ≥ EMS04_FP_BOARD_RSP_MAX。 */
int ems04_fp_board_handle(uint8_t type, const uint8_t *payload, uint16_t len,
                          uint8_t *rsp, uint16_t *rsp_len);

/* 状态计数快照（诊断：rx 帧数/响应数/err 计数；out 为 NULL 时忽略）。 */
void ems04_fp_board_stats(ems04_fp_board_stats_t *out);

/* 单条 err 码累计（等价 ems04_fp_svc_err_count，便于板上单点查询）。 */
uint32_t ems04_fp_board_err_count(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_FP_BOARD_H */
