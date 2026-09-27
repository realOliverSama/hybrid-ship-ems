/* ems04_fp_dumpchunk.h — EMS04-S3-A2：RSP_FP_DUMP 分块诊断传输（TYPE 0x27）。
 *
 * 背景（用户裁决 2026-09-26 §14 APPROVED："Repair only the diagnostic transport"）：
 * RSP_FP_DUMP(0x26) 单帧 13939 B（载荷 13932 B），在 115200 UART 长流中丢帧头
 * （板上 rsp_dump=1 证明已产出、主机 capture 丢帧）。本模块把该 DUMP 载荷切成
 * 小帧分块直发，仅修诊断传输，不改特征/预测语义：
 *   - DUMP 查询（0x25）行为不变（服务侧冻结六模块零改动）；
 *   - 客户端收齐 total_chunks 后自行重组为完整 DUMP 载荷（重组工具
 *     Host/tools/s3a2_dump_reasm.py）。
 *
 * 分块响应（新增诊断响应类型 0x27 = RSP_FP_DUMP_CHUNK，PROTOCOL_FP 0x20..0x26
 * 之外未占用，ems04_fp_proto.h/ems_proto.h grep 确认 0x27 空闲）：
 *
 *   帧容器（沿用 PROTOCOL_FP §0，与既有帧逐字节同构）：
 *     [AA 55][TYPE=0x27][LEN u16 LE][PAYLOAD][CRC16 u16 LE]
 *     CRC16-CCITT-FALSE（poly 0x1021 / init 0xFFFF），作用域 = TYPE..PAYLOAD。
 *
 *   载荷（LEN ≤ EMS04_DUMPCHUNK_FRAME_PAYLOAD_MAX = 512）：
 *     [u32 seq][u32 chunk_idx][u32 total_chunks][u32 payload_len]
 *     [u8 last_flag][u8 rsv][payload payload_len B]
 *     - 块头固定 18 B（4×u32 + last_flag + rsv 保留位）→ 数据 ≤ 494 B/帧；
 *     - payload_len = 本帧数据字节数（不含 18 B 块头）；末帧 last_flag=1；
 *     - total_chunks = ceil(DUMP 载荷 / 494)（13932 B → 29 帧：28×494 + 100）；
 *     - chunk_idx 从 0 连续递增，顺序直发、无重传（诊断通道，丢帧由
 *       主机按 chunk_idx 缺口检测，fail-closed 报告）。
 *
 * seq 来源：CMD_FP_DUMP(0x25) 载荷为 [f64 t0]（PROTOCOL_FP §5），无 CMD_REATT
 * 式载荷头 seq，故由调用方传入自增计数器（ems_app.c 挂载点静态计数，见该处
 * 注释）；同一 DUMP 的全部分块帧共用同一 seq，主机按 seq 分组重组。
 *
 * 纯 C99；无动态内存；帧编码复用冻结模块 ems04_fp_proto.c 的
 * ems04_fp_encode_frame（本文件零协议重实现）。仅随 CMake 开关
 * EMS04_DUMP_CHUNK=1（且 FP 服务形态）编入，默认 0 零体积零影响。
 */
#ifndef EMS04_FP_DUMPCHUNK_H
#define EMS04_FP_DUMPCHUNK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 0x27 载荷契约 ---- */
#define EMS04_FP_RSP_DUMP_CHUNK 0x27u /* 新增诊断响应 TYPE（0x20..0x26 之外） */
#define EMS04_DUMPCHUNK_HDR_LEN 18u   /* [4×u32][u8 last_flag][u8 rsv] */
#define EMS04_DUMPCHUNK_FRAME_PAYLOAD_MAX \
    512u /* 单帧载荷上限（块头 18 + 数据 494） */
#define EMS04_DUMPCHUNK_DATA_MAX \
    (EMS04_DUMPCHUNK_FRAME_PAYLOAD_MAX - EMS04_DUMPCHUNK_HDR_LEN) /* 494 */

/* chunk 帧整帧字节数上限（供调用方直发缓冲定容；主机侧不缓冲整帧） */
#define EMS04_DUMPCHUNK_FRAME_MAX \
    (EMS04_DUMPCHUNK_FRAME_PAYLOAD_MAX + 7u) /* 7 = AA55/TYPE/LEN/CRC16 */

/* ---- writer 回调：每产出一个完整 chunk 帧调用一次（frame/len 为整帧字节，
 * 含 AA55/TYPE/LEN/CRC16；user 透传）。回调应在返回后即视为 frame 已消费
 * （本模块逐帧复用同一内部缓冲，回调不得长期持有 frame 指针）。 ---- */
typedef void (*ems04_fp_dumpchunk_sink)(const uint8_t *frame, uint16_t len,
                                        void *user);

/* 把 DUMP 载荷切成 0x27 chunk 帧并逐帧写 sink。
 * dump_payload/len = RSP_FP_DUMP 载荷（如 [f64 t0][status/rsv][hist][kf]，本模块
 * 不解释内容、纯透传）；seq = 本次 DUMP 的会话序号（全部分块帧共用）。
 * 返回 = 产出的 chunk 帧数（>0）；参数非法或 sink 为空返回负值（不写出）。 */
int ems04_fp_dumpchunk_emit(const uint8_t *dump_payload, uint32_t len,
                            uint32_t seq, ems04_fp_dumpchunk_sink sink,
                            void *user);

/* 板上便捷入口：emit 到「HAL_UART_Transmit(&huart1)」阻塞直发 sink。
 * 与 ems_app.c 既有 FP 响应直发路径同款语义（115200 轮询 TX）。返回值同 emit。 */
int ems04_fp_dumpchunk_uart_dump(const uint8_t *dump_payload, uint32_t len,
                                 uint32_t seq);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_FP_DUMPCHUNK_H */
