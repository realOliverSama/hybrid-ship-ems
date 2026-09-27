/* ems04_fp_dumpchunk.c — EMS04-S3-A2：RSP_FP_DUMP 分块诊断传输（TYPE 0x27）。
 *
 * 载荷/帧格式、seq 语义与红线（冻结六模块零改动、DUMP 查询行为不变）见
 * 同目录 ems04_fp_dumpchunk.h 文件头注释。本文件实现：
 *   - 切块：DUMP 载荷按 494 B/帧切分，块头 18 B 显式小端编码（禁 struct 强转
 *     上线，同 ems04_fp_proto 纪律）；
 *   - 帧封装：复用冻结模块 ems04_fp_proto.c 的 ems04_fp_encode_frame
 *     （AA55/TYPE/LEN/CRC16-CCITT-FALSE 单一实现面，零协议重实现）；
 *   - 产出：逐帧经 sink 回调交出（ems_app.c 挂载点用 HAL_UART_Transmit 直发）。
 *
 * 诊断通道语义：顺序直发、无重传；丢帧由主机按 chunk_idx 缺口检测
 * （Host/tools/s3a2_dump_reasm.py fail-closed 报告）。
 */
#include "ems04_fp_dumpchunk.h"

#include <string.h>

#include "ems04_fp_proto.h" /* 冻结编码器/CRC 原语（只调用，不修改） */
#include "main.h"           /* HAL_UART_Transmit / UART_HandleTypeDef */

extern UART_HandleTypeDef huart1; /* CubeMX 生成（main.c），ems_app.c 同款引用 */

int ems04_fp_dumpchunk_emit(const uint8_t *dump_payload, uint32_t len,
                            uint32_t seq, ems04_fp_dumpchunk_sink sink,
                            void *user)
{
    uint8_t chunk[EMS04_DUMPCHUNK_FRAME_PAYLOAD_MAX];
    uint8_t frame[EMS04_DUMPCHUNK_FRAME_MAX];
    uint32_t total;
    uint32_t idx;

    if ((dump_payload == NULL && len > 0u) || sink == NULL) {
        return -1; /* badarg：不写出 */
    }

    /* total_chunks = ceil(len / 494)；空载荷仍发 1 个 last_flag=1 空数据帧，
     * 使主机侧重组总能闭合（fail-closed 判据可判定）。 */
    total = (len + EMS04_DUMPCHUNK_DATA_MAX - 1u) / EMS04_DUMPCHUNK_DATA_MAX;
    if (total == 0u) {
        total = 1u;
    }

    for (idx = 0u; idx < total; idx++) {
        uint32_t off      = idx * EMS04_DUMPCHUNK_DATA_MAX;
        uint32_t data_len = len - off;
        uint32_t chunk_len;
        size_t frame_len;

        if (data_len > EMS04_DUMPCHUNK_DATA_MAX) {
            data_len = EMS04_DUMPCHUNK_DATA_MAX;
        }
        chunk_len = EMS04_DUMPCHUNK_HDR_LEN + data_len;

        /* 块头 18 B：[u32 seq][u32 chunk_idx][u32 total_chunks][u32 payload_len]
         * [u8 last_flag][u8 rsv] —— 逐字节显式小端 */
        ems04_st_u32(&chunk[0], seq);
        ems04_st_u32(&chunk[4], idx);
        ems04_st_u32(&chunk[8], total);
        ems04_st_u32(&chunk[12], data_len);
        chunk[16] = (idx == (total - 1u)) ? 1u : 0u; /* last_flag：仅末帧为 1 */
        chunk[17] = 0u;                              /* rsv：保留，恒 0 */

        if (data_len > 0u) {
            (void)memcpy(&chunk[EMS04_DUMPCHUNK_HDR_LEN], &dump_payload[off],
                         data_len);
        }

        /* 帧封装复用冻结编码器：[AA 55][0x27][LEN LE16][载荷][CRC16 LE]，
         * CRC16-CCITT-FALSE 作用域 TYPE..PAYLOAD（与既有帧逐字节同构）。
         * 512+7 < 缓冲容量，返回 0 理论不可达；fail-closed 不写半帧。 */
        frame_len = ems04_fp_encode_frame(EMS04_FP_RSP_DUMP_CHUNK, chunk,
                                          chunk_len, frame, sizeof frame);
        if (frame_len == 0u || frame_len > 0xFFFFu) {
            return -2;
        }
        sink(frame, (uint16_t)frame_len, user);
    }
    return (int)total;
}

/* HAL_UART_Transmit 阻塞直发 sink：与 ems_app.c 既有 FP 响应直发路径同款语义 */
static void ems04_fp_dumpchunk_uart_sink(const uint8_t *frame, uint16_t len,
                                         void *user)
{
    (void)user;
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)frame, len, 0xFFFFu);
}

int ems04_fp_dumpchunk_uart_dump(const uint8_t *dump_payload, uint32_t len,
                                 uint32_t seq)
{
    return ems04_fp_dumpchunk_emit(dump_payload, len, seq,
                                   ems04_fp_dumpchunk_uart_sink, NULL);
}
