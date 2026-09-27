/* ems_proto.h — UART 帧协议（编解码 + CRC16 + 字节流状态机 + 注入会话 + 事件环）。
 * T-506，spec ems01-chil-ecms-v1；design.md §UART 协议（FR-510）。
 *
 * 帧格式（单线 USART1 @115200 8N1，线上小端）：
 *   [0xAA 0x55] [TYPE u8] [LEN u16] [PAYLOAD LEN B] [CRC16-CCITT u16 over TYPE..PAYLOAD]
 *   CRC = CCITT-FALSE（poly 0x1021，init 0xFFFF，不反射，无异或输出）。
 *   与 T-505 ems_app.c 内联 ems_crc16_ccitt() 实现逐位一致（迁移契约，环回测试
 *   以已知检验值 + 分段续算等价用例锁定）。
 *
 * 2026-09-01 变更（用户裁决，级联错位免疫）：载荷级 seq 回显——CMD_INFER_TEST/
 * CMD_WC_TEST 载荷以 seq u32 LE 开头，对应 RSP 原样回显该 seq（主机按 seq 归因，
 * 响应迟到/错位不再连坐）；并新增 workclf 测试命令 0x08/0x09（T-507 类别一致率）。
 * 帧格式（AA 55/TYPE/LEN/CRC16 作用域）不变；RSP_TICK 自带 tick_idx 配对，不变。
 *
 * 分流契约（design §UART）：二进制帧以 0xAA 开头；文本日志行以可打印 ASCII 开头、
 * \r\n 结尾、首字节 ≠ 0xAA。解析器只把 0xAA 起始的合法帧上抛；帧外字节（含文本
 * 日志行）静默跳过（主机按首字节 0xAA 分流，固件保证日志行首字节 ≠ 0xAA）。
 *
 * 超时/会话（design §UART）：固件记录上次合法 CMD tick 的毫秒时基（now_ms 回调
 * 注入，固件接 HAL_GetTick）；注入中 CMD_TICK 间隔 >2 s → 置 STALE flags 位 +
 * 记事件 + 保持 Pheld（不重新跑控制律、输出不突变）；下一个间隔 ≤2 s 的合法
 * CMD_TICK 清 STALE。会话 = 已收到 CMD_TICK 起；CMD_RESET 复位会话与事件环。
 *
 * 便携性契约：纯 C99、无 HAL、无动态内存、无机器字节序假设（显式 LE 编解码），
 * 同一 .c 被固件与主机 gcc（Host/tests 环回测试）编译。
 */
#ifndef EMS_PROTO_H
#define EMS_PROTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- design §UART 帧表 TYPE 常量（迁移自 T-505 ems_app.h，数值不变） ---- */
#define EMS_PROTO_SYNC0          0xAAu
#define EMS_PROTO_SYNC1          0x55u
#define EMS_PROTO_CMD_RESET      0x01u  /* H→F 复位控制状态（下一 tick 平 fill）→ ACK */
#define EMS_PROTO_CMD_TICK       0x02u  /* H→F tick_idx u32 + 7×f32 → RSP_TICK */
#define EMS_PROTO_RSP_TICK       0x03u  /* F→H 控制遥测 */
#define EMS_PROTO_CMD_INFER_TEST 0x04u  /* H→F [seq u32][302×f32]（seq 原样回显） */
#define EMS_PROTO_RSP_INFER_TEST 0x05u  /* F→H [seq u32][q10/q50/q90 f32 + infer_us u32] */
#define EMS_PROTO_CMD_PING       0x06u  /* H→F 链路检查 → ACK */
#define EMS_PROTO_CMD_DUMP       0x07u  /* H→F 导出故障事件环（文本行） */
#define EMS_PROTO_CMD_WC_TEST    0x08u  /* H→F [seq u32][302×f32]（workclf 测试，同 04 对称） */
#define EMS_PROTO_RSP_WC_TEST    0x09u  /* F→H [seq u32][cls u8][3B 保留][logits 7×f32][wc_us u32] */
#define EMS_PROTO_CMD_REATT      0x0Au  /* H→F [seq u32][hist 300×11 f32][kf 30×6 f32]（REAT-T 查询，
                                           * 原始域，t/k 主序 × 通道序 = registry H1 / K+F；phase4-g） */
#define EMS_PROTO_RSP_REATT      0x0Bu  /* F→H [seq u32][q 30×3 f32 kW][infer_us u32]（失败 NACK ERR_INFER） */
#define EMS_PROTO_CMD_BDDIAG     0x0Cu  /* H→F [seq u32] → RSP_BDDIAG（S3-C BD 腿诊断，仅
                                           * -DEMS04_BDDIAG_UART=1 构建注册；默认构建不识别） */
#define EMS_PROTO_RSP_BDDIAG     0x0Du  /* F→H [seq u32][q27_crc32 u32][q27_valid u8+3R]
                                           * [hpast 96×f32][n2out 90×i8][n2kf 180×i8] = 666 B */
#define EMS_PROTO_ACK            0x7Fu  /* F→H echo TYPE u8 */
#define EMS_PROTO_NACK           0x7Eu  /* F→H echo TYPE u8 + err u8 */

/* NACK err 码（design 帧表：CRC 错/未知命令/状态非法；3/4 为固件侧细化，T-505 起） */
#define EMS_PROTO_ERR_CRC   0x01u
#define EMS_PROTO_ERR_TYPE  0x02u
#define EMS_PROTO_ERR_LEN   0x03u
#define EMS_PROTO_ERR_INFER 0x04u

/* ---- 载荷长度契约（design §UART 帧表逐项；2026-09-01 起测试命令载荷 seq 前缀） ---- */
#define EMS_PROTO_FEAT_N        302u                       /* INFER/WC_TEST 特征数 */
#define EMS_PROTO_INFER_LEN     (EMS_PROTO_FEAT_N * 4u)    /* 1208（302×f32 特征块） */
#define EMS_PROTO_CMD_INFER_LEN (4u + EMS_PROTO_INFER_LEN) /* 1212：[seq u32][302×f32] */
#define EMS_PROTO_CMD_WC_LEN    (4u + EMS_PROTO_INFER_LEN) /* 1212：与 INFER_TEST 对称 */
#define EMS_PROTO_SIG_N         7u                         /* CMD_TICK 信号数 */
#define EMS_PROTO_TICK_LEN      (4u + EMS_PROTO_SIG_N * 4u) /* 32 */
#define EMS_PROTO_RSP_TICK_LEN  26u /* 4+4+4+4+1+4+4+1（design 帧表逐项） */
#define EMS_PROTO_RSP_INFER_LEN 20u /* [seq u32] + 3×f32 + u32（2026-09-01 +4B seq） */
#define EMS_PROTO_RSP_WC_LEN    40u /* 4 seq + 1 cls + 3 保留 + 28 logits + 4 wc_us */
/* REATT 查询（phase4-g）：[seq u32][hist 3300×f32][kf 180×f32] = 13924 B；
 * RSP：[seq u32][q 90×f32][infer_us u32] = 368 B */
#define EMS_PROTO_REATT_HIST_FLT 3300u  /* 300 t × 11 ch（t 主序） */
#define EMS_PROTO_REATT_KF_FLT   180u   /* 30 lead × 6 ch（k 主序） */
#define EMS_PROTO_REATT_Q_FLT    90u    /* 30 lead × {q10,q50,q90} */
#define EMS_PROTO_CMD_REATT_LEN  (4u + (EMS_PROTO_REATT_HIST_FLT + EMS_PROTO_REATT_KF_FLT) * 4u)
#define EMS_PROTO_RSP_REATT_LEN  (4u + EMS_PROTO_REATT_Q_FLT * 4u + 4u)
/* BDDIAG（+S3-C 诊断，仅开关构建）：[seq u32][crc u32][valid u8+3R][96×f32][90×i8][180×i8] */
#define EMS_PROTO_CMD_BDDIAG_LEN 4u
#define EMS_PROTO_RSP_BDDIAG_LEN (4u + 4u + 4u + 96u * 4u + 90u + 180u) /* = 666 */
#define EMS_PROTO_ACK_LEN       1u
#define EMS_PROTO_NACK_LEN      2u
#if EMS_PROTO_CMD_REATT_LEN > EMS_PROTO_CMD_INFER_LEN
#define EMS_PROTO_MAX_PAYLOAD   EMS_PROTO_CMD_REATT_LEN   /* phase4-g：REATT 13924 B 为最大帧 */
#else
#define EMS_PROTO_MAX_PAYLOAD   EMS_PROTO_CMD_INFER_LEN
#endif
#define EMS_PROTO_FRAME_OVERHEAD 7u  /* 2 同步 + 1 TYPE + 2 LEN + 2 CRC */
#define EMS_PROTO_FRAME_MAX     (EMS_PROTO_MAX_PAYLOAD + EMS_PROTO_FRAME_OVERHEAD)

/* ---- RSP_TICK flags 位（0x03 payload；主机/固件两端契约） ----
 * STALE：注入中 CMD_TICK 间隔 >2 s 的"保持 Pheld"拍；
 * FALLBACK：上一非保持控制拍 ok=0（ê=0 回退，design Error Handling）；
 * CLS_BAD：上一非保持控制拍识别类别越界 [1,7]（按非 upstream K=1 处理）；
 * HELD：本拍为 tick 门保持拍（EML:46-49，t-lastT<dt）。 */
#define EMS_PROTO_FLAGS_STALE    0x01u
#define EMS_PROTO_FLAGS_FALLBACK 0x02u
#define EMS_PROTO_FLAGS_CLS_BAD  0x04u
#define EMS_PROTO_FLAGS_HELD     0x08u
#define EMS_PROTO_CLS_NONE       0xFFu /* cls_eff 无状态（EML 首判决前 -1 映射） */

/* ---- 事件环 type 码（design §Error Handling / §UART CMD_DUMP_EVENTS） ---- */
#define EMS_EVT_CRC_ERR      0x01u /* 帧 CRC 校验失败（aux=线上 TYPE） */
#define EMS_EVT_UNKNOWN_TYPE 0x02u /* 未知 TYPE（aux=TYPE） */
#define EMS_EVT_STALE        0x03u /* 注入超时置 STALE（aux=0） */
#define EMS_EVT_INFER_FAIL   0x04u /* 推理失败/输出非有限（aux=NACK err） */
#define EMS_EVT_CLS_INVALID  0x05u /* 识别类别越界 [1,7]（aux=原始类别低 8 位） */
#define EMS_EVT_LEN_ERR      0x06u /* LEN 与 TYPE 契约不符（aux=TYPE） */

/* ---- 事件条目（type+tick 序列；tick = 发生时最近一次 CMD_TICK 的 tick_idx） ---- */
#define EMS_PROTO_EVENT_N   32u
#define EMS_PROTO_TICK_NONE 0xFFFFFFFFu

typedef struct {
    uint8_t  type; /* EMS_EVT_* */
    uint8_t  aux;
    uint32_t tick;
    uint32_t ms;   /* 事件时戳（now_ms 回调值） */
} ems_proto_event_t;

/* ---- RSP_TICK（0x03）遥测视图（编码/解码共用；线下结构体字节序无关） ---- */
typedef struct {
    uint32_t tick_idx;
    float    P_gen_ref; /* kW */
    float    e_hat;     /* 门控后 ê [-] */
    float    K;         /* GATE_MODE 合成增益 [-] */
    uint8_t  cls_eff;   /* 1..7；0xFF = 无状态 */
    uint32_t infer_us;  /* 最近一个非保持控制拍推理合计 [μs]（DWT，FR-505） */
    uint32_t ctrl_us;   /* 最近一个非保持控制拍控制律整体 [μs] */
    uint8_t  flags;     /* EMS_PROTO_FLAGS_* */
} ems_proto_rsp_tick_t;

/* ---- CMD_TICK（0x02）解码视图（7 信号物理单位固定 design §UART） ---- */
typedef struct {
    uint32_t tick_idx;
    float    sig[7]; /* [t(s), P_load(kW), SOC(-), v_cmd(m/s), v_ship(m/s),
                      * accel(m/s²), x_ship(m)]；固件 v1 只消费 t/P_load/SOC/v_ship */
} ems_proto_cmd_tick_t;

/* ---- 注入会话阈值（design §UART 超时，禁改） ---- */
#define EMS_PROTO_INJECT_GAP_MS 2000u

/* ---- 协议会话（解析器 + 会话 + 事件环 + 计数；单实例静态分配，无动态内存）。
 * ~1.3 KB 帧缓冲 + 事件环 512 B。 */
typedef struct {
    /* TX / 时基 / 分发回调（依赖注入：固件接 HAL，host 测试接内存/pty/假时基） */
    void (*write)(void *user, const uint8_t *buf, uint32_t len);
    void *write_user;
    uint32_t (*now_ms)(void *user);
    void *now_user;
    void (*on_frame)(void *user, uint8_t type, const uint8_t *payload,
                     uint16_t len, uint32_t seq);
    void *frame_user;

    /* 字节流状态机（AA 55 | TYPE | LEN u16 | PAYLOAD | CRC16 u16） */
    uint8_t  rx_state; /* EMS_PROTO_RX_* */
    uint16_t rx_len;
    uint16_t rx_got;
    uint16_t rx_crc;
    uint8_t  rx_frame[EMS_PROTO_FRAME_MAX]; /* TYPE..PAYLOAD 连续存放（CRC 作用域） */

    /* 注入会话状态（design §UART 超时/STALE 契约） */
    uint8_t  session_active; /* 已收到 ≥1 个合法 CMD_TICK */
    uint8_t  stale;          /* 0/1（EMS_PROTO_FLAGS_STALE 语义在位） */
    uint32_t last_tick_ms;   /* 上次合法 CMD_TICK 的 now_ms */
    uint32_t last_tick_idx;  /* 上次合法 CMD_TICK 的 tick_idx（事件环时戳源） */

    /* 事件环（定长环形，满覆盖最旧；dump 按环序新→旧导出） */
    ems_proto_event_t ev[EMS_PROTO_EVENT_N];
    uint32_t ev_head;  /* 下一写入位 */
    uint32_t ev_count; /* 现存条数（≤ EMS_PROTO_EVENT_N） */
    uint32_t ev_total; /* 累计入环总数（含被覆盖者） */

    /* 机器可读计数（L2 诊断/环回测试观测面） */
    uint32_t stat_frames_ok;
    uint32_t stat_crc_err;
} ems_proto_t;

/* 解析器内部状态（ems_proto_t.rx_state 取值） */
enum {
    EMS_PROTO_RX_SYNC0 = 0, /* 等 0xAA */
    EMS_PROTO_RX_SYNC1,     /* 等 0x55（连续 0xAA 容错） */
    EMS_PROTO_RX_TYPE,
    EMS_PROTO_RX_LEN0,
    EMS_PROTO_RX_LEN1,
    EMS_PROTO_RX_PAYLOAD,
    EMS_PROTO_RX_CRC0,
    EMS_PROTO_RX_CRC1
};

/* ---- CRC16（design 迁移契约：CCITT-FALSE，poly 0x1021，init 0xFFFF） ----
 * ems_proto_crc16 与 T-505 ems_app.c ems_crc16_ccitt 实现逐位一致（迁移基）；
 * chain 版支持 TYPE..PAYLOAD 分段续算（结果与整段一致）。 */
uint16_t ems_proto_crc16(const uint8_t *data, uint32_t len);
uint16_t ems_proto_crc16_chain(uint16_t crc_in, const uint8_t *data, uint32_t len);

/* ---- 小端原语（显式逐字节组合，不依赖机器字节序；编解码共用） ---- */
uint16_t ems_proto_rd_u16(const uint8_t *p);
uint32_t ems_proto_rd_u32(const uint8_t *p);
float    ems_proto_rd_f32(const uint8_t *p);
void     ems_proto_wr_u16(uint8_t *p, uint16_t v);
void     ems_proto_wr_u32(uint8_t *p, uint32_t v);
void     ems_proto_wr_f32(uint8_t *p, float f);

/* ---- 编码器（返回写入字节数；out==NULL / cap 不足 / len 超限返回 0，不半写） ----
 * 帧布局 [AA 55][TYPE][LEN LE16][PAYLOAD][CRC LE16]，CRC 作用域 TYPE..PAYLOAD。 */
uint32_t ems_proto_enc_frame(uint8_t *out, uint32_t cap, uint8_t type,
                             const uint8_t *payload, uint16_t len);
/* RSP_INFER_TEST（0x05）payload 组装：[seq u32][q10/q50/q90 f32（kWh）][infer_us u32]；
 * seq = CMD_INFER_TEST 载荷头序号原样回显（2026-09-01 载荷级 seq 回显）。 */
void ems_proto_enc_rsp_infer(uint8_t out[EMS_PROTO_RSP_INFER_LEN], uint32_t seq,
                             const float q_kwh[3], uint32_t infer_us);
/* RSP_WC_TEST（0x09）payload 组装：[seq u32][cls u8][3B 保留=0][logits 7×f32][wc_us u32]；
 * seq = CMD_WC_TEST 载荷头序号原样回显（workclf 类别一致率测试，T-507）。 */
void ems_proto_enc_rsp_wc(uint8_t out[EMS_PROTO_RSP_WC_LEN], uint32_t seq,
                          uint8_t cls, const float logits[7], uint32_t us);
/* RSP_REATT（0x0B）payload 组装：[seq u32][q 90×f32（[k][q10,q50,q90] kW）]
 * [infer_us u32]；seq = CMD_REATT 载荷头序号原样回显（phase4-g）。 */
void ems_proto_enc_rsp_reatt(uint8_t out[EMS_PROTO_RSP_REATT_LEN], uint32_t seq,
                             const float q_kW[EMS_PROTO_REATT_Q_FLT], uint32_t us);
/* RSP_TICK（0x03）payload 组装（字段序按 design 帧表） */
void ems_proto_enc_rsp_tick(uint8_t out[EMS_PROTO_RSP_TICK_LEN],
                            const ems_proto_rsp_tick_t *r);

/* CMD_TICK（0x02）payload 解码：tick_idx + 7×f32。返回 0 = OK；-1 = len 不符。 */
int ems_proto_dec_cmd_tick(const uint8_t *payload, uint16_t len,
                           ems_proto_cmd_tick_t *out);
/* RSP_TICK（0x03）payload 解码（环回测试/主机对拍用）。返回 0 = OK；-1 = len 不符。 */
int ems_proto_dec_rsp_tick(const uint8_t *payload, uint16_t len,
                           ems_proto_rsp_tick_t *out);

/* ---- 会话生命周期与驱动（替代 T-505 的 ems_rx_* 内联状态） ---- */
/* 初始化：回调注入（write 必填；now_ms 为 NULL 时 STALE 会话计时禁用并恒不触发）；
 * 发送 CMD_RESET 后固件应再调 ems_proto_session_reset 同步会话/事件环。 */
void ems_proto_init(ems_proto_t *p,
                    void (*write)(void *user, const uint8_t *buf, uint32_t len),
                    void *write_user,
                    uint32_t (*now_ms)(void *user), void *now_user,
                    void (*on_frame)(void *user, uint8_t type,
                                     const uint8_t *payload, uint16_t len,
                                     uint32_t seq),
                    void *frame_user);

/* 字节流喂入：解析状态机推进；合法帧 → on_frame 回调；CRC 错 → NACK(ERR_CRC)
 * + 事件环 + stat_crc_err++；LEN 超限/帧间垃圾 → 静默丢弃并重回同步。 */
void ems_proto_feed(ems_proto_t *p, uint8_t b);
void ems_proto_feed_buf(ems_proto_t *p, const uint8_t *buf, uint32_t n);

/* 帧发送（组帧 + CRC + write 回调）。len > EMS_PROTO_MAX_PAYLOAD 拒发返回 0。
 * 返回 1 = 已完整交 write（含 0 字节 payload 帧）。 */
uint8_t ems_proto_send_frame(ems_proto_t *p, uint8_t type,
                             const uint8_t *payload, uint16_t len);
uint8_t ems_proto_send_ack(ems_proto_t *p, uint8_t echo_type);
uint8_t ems_proto_send_nack(ems_proto_t *p, uint8_t echo_type, uint8_t err);

/* ---- 注入会话（design §UART 超时语义；固件在收到 CMD_TICK 时调用） ----
 * 返回 1 = 距上次合法 CMD_TICK 已 >EMS_PROTO_INJECT_GAP_MS（本次应置 STALE 并
 * 保持 Pheld，不重跑控制律）；返回 0 = 正常拍。内部维护 session_active/
 * last_tick_ms/stale，STALE 时自动入事件环。now_ms==NULL 时恒返回 0。 */
uint8_t ems_proto_session_on_cmd_tick(ems_proto_t *p);
/* CMD_RESET 语义：清 STALE/会话与事件环（下一 tick 平 fill 由 ems_ctrl_init 承担） */
void ems_proto_session_reset(ems_proto_t *p);
uint8_t ems_proto_session_stale(const ems_proto_t *p);

/* ---- 事件环 ----
 * 入环（满覆盖最旧）。tick = 最近 CMD_TICK 序号（无会话时 EMS_PROTO_TICK_NONE），
 * ms = now_ms（无时基时 0）。 */
void ems_proto_event_push(ems_proto_t *p, uint8_t type, uint8_t aux, uint32_t tick);
/* 导出条数（现存）与按下标只读访问（0 = 最旧 … count-1 = 最新） */
uint32_t ems_proto_event_count(const ems_proto_t *p);
const ems_proto_event_t *ems_proto_event_at(const ems_proto_t *p, uint32_t i);
/* CMD_DUMP_EVENTS 文本导出（经 write 回调）：每事件一行
 * "EVT seq=<#> tick=<u> ms=<u> type=<名> aux=<u>\r\n"（首字节 'E' ≠ 0xAA，
 * 可打印 ASCII + CRLF，design 分流契约），末尾汇总行
 * "EVT total=<累计> kept=<现存> cap=<32>\r\n"。 */
void ems_proto_dump_events(ems_proto_t *p);

/* 最近一次合法 CMD_TICK 的 tick_idx（应用层入事件环时作 tick 时戳；
 * 无会话时返回 EMS_PROTO_TICK_NONE） */
uint32_t ems_proto_last_tick_idx(const ems_proto_t *p);

#ifdef __cplusplus
}
#endif

#endif /* EMS_PROTO_H */