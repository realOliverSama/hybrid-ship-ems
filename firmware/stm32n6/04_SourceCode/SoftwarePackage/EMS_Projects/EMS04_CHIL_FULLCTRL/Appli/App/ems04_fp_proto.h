/* ems04_fp_proto.h — EMS04-S2 B1：FPC-1.0 特征面板线格式编解码（PROTOCOL_FP.md）。
 *
 * 线格式（冻结，逐字来自 PROTOCOL_FP.md §0）：
 *   [0xAA 0x55][TYPE u8][LEN u16 LE][PAYLOAD LEN B][CRC16 u16 LE]
 *   CRC16-CCITT-FALSE（poly 0x1021，init 0xFFFF，不反射，无输出异或），作用域 = TYPE..PAYLOAD。
 *
 * 命令/响应码（PROTOCOL_FP.md §1–§5，② 新段 0x20..0x26；既有段 0x01..0x0B 不动）：
 *   0x20 CMD_VOYAGE_INIT (LEN=116) → ACK
 *   0x21 CMD_KF_CHUNK    (LEN=10+chunk_len) → ACK
 *   0x22 CMD_TICK2       (LEN=92) → ACK
 *   0x23 CMD_FP_QUERY    (LEN=8) → 0x24 RSP_FP_QUERY (LEN=76)
 *   0x25 CMD_FP_DUMP     (LEN=8) → 0x26 RSP_FP_DUMP  (LEN=13932)
 *   0x7F ACK = [TYPE u8]（沿用既有 ems_proto 机制）
 *   0x7E NACK = [TYPE u8][err u8]（沿用既有 ems_proto 机制）
 *
 * 纪律：**禁止 struct 强转上线**；所有字段逐字节显式小端读写。
 * 纯 C99；无动态内存；无机器字节序假设。
 */
#ifndef EMS04_FP_PROTO_H
#define EMS04_FP_PROTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- §0 帧容器 ---- */
#define EMS04_FP_SYNC0 0xAAu
#define EMS04_FP_SYNC1 0x55u
#define EMS04_FP_FRAME_OVERHEAD 7u /* 2 sync + 1 type + 2 len + 2 crc */

/* ---- 命令/响应码 ---- */
#define EMS04_FP_CMD_VOYAGE_INIT 0x20u
#define EMS04_FP_CMD_KF_CHUNK    0x21u
#define EMS04_FP_CMD_TICK2       0x22u
#define EMS04_FP_CMD_FP_QUERY    0x23u
#define EMS04_FP_RSP_FP_QUERY    0x24u
#define EMS04_FP_CMD_FP_DUMP     0x25u
#define EMS04_FP_RSP_FP_DUMP     0x26u
#define EMS04_FP_ACK             0x7Fu
#define EMS04_FP_NACK            0x7Eu

/* ---- §6 err 码（0x01..0x03 为既有段，0x40..0x48 为 ② 新段） ---- */
#define EMS04_FP_ERR_CRC       0x01u
#define EMS04_FP_ERR_TYPE      0x02u
#define EMS04_FP_ERR_LEN       0x03u
#define EMS04_FP_ERR_STATE     0x40u
#define EMS04_FP_ERR_POLICY    0x41u
#define EMS04_FP_ERR_ORDER     0x42u
#define EMS04_FP_ERR_TICK_T    0x43u
#define EMS04_FP_ERR_NONFINITE 0x44u
#define EMS04_FP_ERR_NOT_READY 0x45u
#define EMS04_FP_ERR_KF_HASH   0x46u
#define EMS04_FP_ERR_KF_ROW    0x47u
#define EMS04_FP_ERR_BOUNDS    0x48u

/* ---- 载荷与长度契约（PROTOCOL_FP.md §1–§5 逐项） ---- */
#define EMS04_FP_VOYAGE_INIT_LEN  116u
#define EMS04_FP_TICK2_LEN        92u
#define EMS04_FP_KF_CHUNK_HDR_LEN 10u  /* [u32 idx][u32 total][u16 len] */
#define EMS04_FP_T0_LEN           8u
#define EMS04_FP_RSP_QUERY_LEN    76u
#define EMS04_FP_HIST_BYTES       13200u /* 300×11 f32 LE，C-order t-major */
#define EMS04_FP_KF_BYTES         720u   /* 30×6 f32 LE，C-order k-major */
#define EMS04_FP_RSP_DUMP_LEN     (8u + 4u + EMS04_FP_HIST_BYTES + EMS04_FP_KF_BYTES) /* 13932 */
#define EMS04_FP_N_TICK_F64       11u
#define EMS04_FP_TICK2_PAYLOAD_BYTES ((4u) + (11u * 8u)) /* 92，与 TICK2_LEN 同值 */

/* RX 载荷上限：LEN 为 u16，上限即 65535（PROTOCOL_FP §0/TASK LEN 上限） */
#define EMS04_FP_MAX_PAYLOAD 65535u
#define EMS04_FP_FRAME_MAX   (EMS04_FP_MAX_PAYLOAD + EMS04_FP_FRAME_OVERHEAD)

/* 版本常量（PROTOCOL_FP §1 声明值） */
#define EMS04_FP_PROTOCOL_VERSION          1u
#define EMS04_FP_FEATURE_CONTRACT_VERSION  0x00010000u /* FPC-1.0 */
#define EMS04_FP_POLICY_AUTO               0u
#define EMS04_FP_MANUAL_RULE_ZERO_FILL     0u

/* ---- CRC16-CCITT-FALSE（CCITT-FALSE：poly 0x1021 / init 0xFFFF / 不反射） ---- */
uint16_t ems04_fp_crc16(const uint8_t *data, uint32_t len);
uint16_t ems04_fp_crc16_chain(uint16_t crc_in, const uint8_t *data, uint32_t len);

/* ---- 小端标量读写（显式，无对齐/字节序假设） ---- */
uint16_t ems04_le_u16(const uint8_t *p);
uint32_t ems04_le_u32(const uint8_t *p);
double   ems04_le_f64(const uint8_t *p);
void     ems04_st_u16(uint8_t *p, uint16_t v);
void     ems04_st_u32(uint8_t *p, uint32_t v);
void     ems04_st_f64(uint8_t *p, double v);

/* ---- 解码视图（线下结构体，字段由显式 LE 解码填充） ---- */
typedef struct {
    uint32_t protocol_version;
    uint32_t feature_contract_version;
    char     voyage_id[33];   /* NUL 结尾 ASCII 副本（线上 32 B，NUL 填充） */
    double   water_depth_m;
    uint8_t  ctrl_policy;
    uint8_t  manual_power_rule;
    uint16_t reserved;
    uint32_t tick_first;
    uint32_t tick_last;
    uint32_t n_ticks;
    uint32_t kf_n_rows;
    double   kf_issue_first;
    double   kf_issue_step;
    uint8_t  kf_payload_sha256[32];
} ems04_fp_voyage_init_t;

typedef struct {
    uint32_t       chunk_idx;
    uint32_t       total_chunks;
    uint16_t       chunk_len;
    const uint8_t *payload; /* 指向帧缓冲内，生命周期 = 帧有效期内 */
} ems04_fp_kf_chunk_t;

typedef struct {
    uint32_t tick_idx;
    double   v[EMS04_FP_N_TICK_F64]; /* 契约 §2.1 字段序（t_s 在 [0]） */
} ems04_fp_tick2_t;

/* ---- 解码器：返回 0 = OK，>0 = err 码（EMS04_FP_ERR_*），<0 保留 ---- */
int ems04_fp_dec_voyage_init(const uint8_t *p, uint32_t len, ems04_fp_voyage_init_t *out);
int ems04_fp_dec_kf_chunk(const uint8_t *p, uint32_t len, ems04_fp_kf_chunk_t *out);
int ems04_fp_dec_tick2(const uint8_t *p, uint32_t len, ems04_fp_tick2_t *out);
int ems04_fp_dec_t0(const uint8_t *p, uint32_t len, double *t0_out);

/* 类型 → 固定载荷长度；非固定长度类型返回 EMS04_FP_LEN_VARIABLE(0xFFFFFFFF) */
#define EMS04_FP_LEN_VARIABLE 0xFFFFFFFFu
uint32_t ems04_fp_fixed_len(uint8_t type);

/* ---- 编码器：返回写出字节数；缓冲不足或参数非法返回 0 ---- */
size_t ems04_fp_encode_frame(uint8_t type, const uint8_t *payload, uint32_t payload_len,
                             uint8_t *out, size_t cap);
size_t ems04_fp_encode_ack(uint8_t cmd_type, uint8_t *out, size_t cap);
size_t ems04_fp_encode_nack(uint8_t cmd_type, uint8_t err, uint8_t *out, size_t cap);
size_t ems04_fp_encode_rsp_query(double t0, const uint8_t hist_sha256[32],
                                 const uint8_t kf_sha256[32], uint8_t *out, size_t cap);
size_t ems04_fp_encode_rsp_dump(double t0, const uint8_t *hist, const uint8_t *kf,
                                uint8_t *out, size_t cap);
/* 命令编码（H→F 侧复用；C 阶段 RTT 上行、单元台构造向量共用同一实现） */
size_t ems04_fp_encode_voyage_init(const ems04_fp_voyage_init_t *vi, uint8_t *out, size_t cap);
size_t ems04_fp_encode_kf_chunk(uint32_t chunk_idx, uint32_t total_chunks,
                                const uint8_t *chunk, uint16_t chunk_len,
                                uint8_t *out, size_t cap);
size_t ems04_fp_encode_tick2(const ems04_fp_tick2_t *tk, uint8_t *out, size_t cap);
size_t ems04_fp_encode_t0(uint8_t type, double t0, uint8_t *out, size_t cap);

/* ---- 字节流 RX 状态机 ---- */
typedef struct {
    uint8_t  state;
    uint8_t  type;
    uint8_t  len_bytes[2];
    uint32_t len_got;
    uint32_t len;
    uint32_t got;
    uint16_t crc;      /* 递增 CRC（TYPE..PAYLOAD） */
    uint8_t  crc_bytes[2];
    uint32_t crc_got;
    uint16_t crc_rx;
    uint32_t junk_bytes;   /* 同步搜索期丢弃的字节数（重同步证据） */
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint8_t  err;          /* EMS04_FP_RX_ERR 时的 err 码（ERR_CRC / ERR_LEN） */
    uint8_t  payload[EMS04_FP_MAX_PAYLOAD];
} ems04_fp_rx_t;

typedef enum {
    EMS04_FP_RX_IDLE  = 0, /* 无完整事件 */
    EMS04_FP_RX_FRAME = 1, /* rx->type/rx->payload/rx->len 有效 */
    EMS04_FP_RX_ERR   = 2  /* rx->err 有效（CRC/LEN）；rx->type 为已解析 TYPE */
} ems04_fp_rx_result_t;

void ems04_fp_rx_init(ems04_fp_rx_t *rx);
/* 仅复位解析状态与载荷簿记，保留 frames_ok/crc_errors/junk_bytes（链路诊断累计） */
void ems04_fp_rx_reset_state(ems04_fp_rx_t *rx);
/* 喂 1 字节；返回 EMS04_FP_RX_*（err 码经 rx->err） */
int ems04_fp_rx_push(ems04_fp_rx_t *rx, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_FP_PROTO_H */
