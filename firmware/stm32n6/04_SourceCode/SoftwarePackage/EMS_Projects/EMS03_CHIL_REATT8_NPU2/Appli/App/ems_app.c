/* ems_app.c — 应用编排与帧分发（T-505/T-506，spec ems01-chil-ecms-v1）。
 *
 * 职责（T-506 起）：
 *   1. 持有 ems_proto_t（协议层，帧编解码/CRC/注入会话/事件环）与 ems_ctrl_t
 *      （控制律状态），经回调（write→HAL_UART_Transmit，now_ms→HAL_GetTick，
 *      on_frame→ems_dispatch）把 HAL 平面接到协议层（ems_proto 零 HAL）；
 *   2. design §UART 帧表全量分发：
 *        CMD_RESET(0x01)       复位 ems_ctrl 状态（下一 tick 平 fill）+ 会话 → ACK
 *        CMD_TICK(0x02)        组 ems_ctrl_in_t → ems_ctrl_step → RSP_TICK
 *                              （注入间隔 >2 s：STALE，保持 Pheld，不重跑控制律）
 *        CMD_INFER_TEST(0x04)  [seq u32][302×f32] → seq 回显 + 归一化（EML:64 同式）→
 *                              ems_infer_mlpq → 反归一化（EML:68）→ RSP_INFER_TEST
 *                              （T-505 链路；2026-09-01 起载荷级 seq 回显）
 *        CMD_WC_TEST(0x08)     [seq u32][302×f32] → Pdec30（步长 10）+[v,a] 旁路 →
 *                              ems_infer_workclf → RSP_WC_TEST（T-507 类别一致率）
 *        CMD_PING(0x06)        → ACK；CMD_DUMP_EVENTS(0x07) → 事件环文本行 → ACK
 *      CRC 错（→NACK+事件）/已知未接管/未知 TYPE（→NACK ERR_TYPE）由 ems_proto
 *      统一处理后进本分发。
 *   3. FR-505 遥测：DWT CYCCNT 测 ctrl_us（控制律整体），infer_us 经
 *      ems_infer_last_us()（REF 后端恒 0，NPU 后端为 DWT 实测，见 ems_infer.h）。
 *
 * T-506 迁移说明：T-505 的内联字节流解析器/TYPE 常量/ACK-NACK 发送/
 * ems_crc16_ccitt 已迁入 ems_proto.c/.h（数值契约不变，见 ems_proto.h 文件头）；
 * CMD_INFER_TEST 处理逻辑不变（仅组帧与 LE 序列化改走 ems_proto 等价原语，
 * 板上小端机器逐字节等价，环回测试锁定）。
 *
 * 归一化/反归一化说明（design R8）：INFER_TEST 的预处理（M1 与 Python int8
 * reference 逐向量对齐用）与 ems_ctrl.c 内 EML:64/68 为同式异位实现，不构成
 * 后端漂移；EMS_CTRL 白盒缝合面（"生产代码勿另行调用"）不反用。
 *
 * 接收面说明（保持 T-505 方案）：BSP uart.c 以 -DBSP_UART_RX_DISABLE 编译
 * （CMake 用户块），停用其行缓冲中断接收；本文件以 HAL_UART_Receive 超时 0
 * 逐字节轮询（请求-响应协议下无连续背靠背帧，主机等 RSP 再发；单帧处理期间
 * 不收帧，无溢出路径）。USART1 FIFO 关闭（CubeMX 生成态），115200 下字节间隔
 * ≈87 μs。协议状态（STALE/事件环/帧缓冲）都在 ems_proto_t 单实例内，
 * 主循环单线程消费，无 ISR 重入路径。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include <string.h>

#include "main.h"
#include "uart.h"

#include "ems_app.h"
#include "ems_config.h"
#include "ems_ctrl.h"
#include "ems_infer.h"
#include "ems_lcd.h" /* phase4-g NPU2：RGBLCD 可视化注入面 */
#include "ems_params.h"
#include "ems_proto.h"
#include "ems_reatt.h" /* phase4-g：CMD_REATT REAT-T 板上预测服务 */

extern UART_HandleTypeDef huart1; /* CubeMX 生成（main.c），BSP/it.c 同款引用 */

static ems_proto_t ems_proto_st; /* 协议会话（单实例：解析/会话/事件环） */
static ems_ctrl_t ems_ctrl_st;   /* 控制律状态（T-506 CMD_TICK/CMD_RESET 驱动） */
static void ems_handle_cmd_tick(const uint8_t *payload, uint16_t len);

/* 上一非保持控制拍遥测（STALE 保持拍/HELD 拍的 RSP_TICK 时序不变性来源；FR-509） */
static struct {
    uint32_t tick_idx;
    float    P;      /* Pheld/最近 P_gen_ref [kW] */
    float    e_hat;
    float    K;
    uint32_t infer_us;
    uint32_t ctrl_us;
    uint8_t  cls_wire; /* 线上值：1..7 或 EMS_PROTO_CLS_NONE */
    uint8_t  soft_flags; /* FALLBACK/CLS_BAD（非保持拍结算；HELD 位不持久） */
} ems_last;

/* ---- 回调：TX（HAL 阻塞发送；len ≤ EMS_PROTO_FRAME_MAX 单帧上限） ---- */
static void ems_uart_write(void *user, const uint8_t *buf, uint32_t len)
{
    (void)user;
    if (len <= EMS_PROTO_FRAME_MAX) {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 0xFFFFu);
    }
}

/* ---- 回调：毫秒时基（HAL_GetTick，1 ms） ---- */
static uint32_t ems_now_ms(void *user)
{
    (void)user;
    return HAL_GetTick();
}

/* ---- CMD_INFER_TEST（0x04）：M1 一致性自测（FR-504；T-505 链路 + 2026-09-01
 * 载荷级 seq 前缀：[seq u32][302×f32]，RSP 原样回显 seq，级联错位免疫） ---- */
static void ems_handle_infer_test(const uint8_t *payload, uint16_t len)
{
    double x[EMS_PROTO_FEAT_N];
    double xnorm[EMS_PROTO_FEAT_N];
    double q3_norm[3];
    uint32_t seq;
    uint32_t us = 0;
    int rc;

    if (len < EMS_PROTO_CMD_INFER_LEN) {
        ems_proto_event_push(&ems_proto_st, EMS_EVT_LEN_ERR,
                             EMS_PROTO_CMD_INFER_TEST,
                             ems_proto_last_tick_idx(&ems_proto_st));
        ems_proto_send_nack(&ems_proto_st, EMS_PROTO_CMD_INFER_TEST,
                            EMS_PROTO_ERR_LEN);
        return;
    }

    /* 载荷头 4B = seq u32 LE（原样回显）；302×f32 特征块起点 +4 */
    seq = ems_proto_rd_u32(&payload[0]);
    for (int i = 0; i < (int)EMS_PROTO_FEAT_N; i++) {
        x[i] = (double)ems_proto_rd_f32(&payload[4u + 4u * (uint32_t)i]);
    }

    /* EML:64 同式：xnorm=(x-XMean)./XStd（物理量 → 归一化域，design R8） */
    for (int i = 0; i < (int)EMS_PROTO_FEAT_N; i++) {
        xnorm[i] = (x[i] - EMS_NORM_X_MEAN[i]) / EMS_NORM_X_STD[i];
    }

    rc = ems_infer_mlpq(xnorm, q3_norm, &us); /* 后端由 EMS_FW_MODE 编译期选定 */
    if (rc != 0) {
        ems_proto_send_nack(&ems_proto_st, EMS_PROTO_CMD_INFER_TEST,
                            EMS_PROTO_ERR_INFER);
        return;
    }

    /* EML:68 同式反归一化 → kWh；q10/q50/q90 顺序 = 分位数通道序 */
    uint8_t rsp[EMS_PROTO_RSP_INFER_LEN];
    float q_kwh[3];
    for (int i = 0; i < 3; i++) {
        q_kwh[i] = (float)(q3_norm[i] * EMS_NORM_Y_STD + EMS_NORM_Y_MEAN);
    }
    ems_proto_enc_rsp_infer(rsp, seq, q_kwh, us);
    (void)ems_proto_send_frame(&ems_proto_st, EMS_PROTO_RSP_INFER_TEST, rsp,
                               EMS_PROTO_RSP_INFER_LEN);

    /* 文本日志行（可打印 ASCII 开头、\r\n 结尾，design §UART 分流契约） */
    printf("EMS01 INFER_TEST seq=%lu q10=%.6f q50=%.6f q90=%.6f infer_us=%lu\r\n",
           (unsigned long)seq, (double)q_kwh[0], (double)q_kwh[1],
           (double)q_kwh[2], (unsigned long)us);
}

/* ---- CMD_WC_TEST（0x08）：workclf 类别一致率测试（T-507；2026-09-01 新增） ----
 * 载荷 [seq u32][302×f32]（与 CMD_INFER_TEST 对称）；输入抽取与训练导出口径
 * 一致：Pdec30 = f32[0]、[10]、…、[290]（步长 10 取 30 点，EML:82 同式），
 * 旁路 [v,a] = f32[300]、[301]（Xr32[:, 0:300:10], Xr32[:, 300:] 同口径）。
 * 回 RSP_WC_TEST：[seq u32][cls u8][3B 保留][logits 7×f32][wc_us u32]。 */
static void ems_handle_wc_test(const uint8_t *payload, uint16_t len)
{
    double pdec30[30];
    float logits[7];
    uint32_t seq;
    uint32_t us = 0;
    int cls = 0;
    int rc;

    if (len < EMS_PROTO_CMD_WC_LEN) {
        ems_proto_event_push(&ems_proto_st, EMS_EVT_LEN_ERR, EMS_PROTO_CMD_WC_TEST,
                             ems_proto_last_tick_idx(&ems_proto_st));
        ems_proto_send_nack(&ems_proto_st, EMS_PROTO_CMD_WC_TEST,
                            EMS_PROTO_ERR_LEN);
        return;
    }

    /* 载荷头 4B = seq（原样回显）；302×f32 特征块自 +4 起（同 INFER_TEST） */
    seq = ems_proto_rd_u32(&payload[0]);
    for (int j = 0; j < 30; j++) {
        pdec30[j] = (double)ems_proto_rd_f32(&payload[4u + 40u * (uint32_t)j]);
    }
    const double v_bypass = (double)ems_proto_rd_f32(&payload[4u + 300u * 4u]);
    const double a_bypass = (double)ems_proto_rd_f32(&payload[4u + 301u * 4u]);

    rc = ems_infer_workclf(pdec30, v_bypass, a_bypass, &cls, logits, &us);
    if (rc != 0) {
        ems_proto_send_nack(&ems_proto_st, EMS_PROTO_CMD_WC_TEST,
                            EMS_PROTO_ERR_INFER);
        return;
    }

    uint8_t rsp[EMS_PROTO_RSP_WC_LEN];
    ems_proto_enc_rsp_wc(rsp, seq, (uint8_t)cls, logits, us);
    (void)ems_proto_send_frame(&ems_proto_st, EMS_PROTO_RSP_WC_TEST, rsp,
                               EMS_PROTO_RSP_WC_LEN);

    printf("EMS01 WC_TEST seq=%lu cls=%d wc_us=%lu\r\n", (unsigned long)seq,
           cls, (unsigned long)us);
}

/* ---- CMD_REATT（0x0A）：REAT-T 板上预测查询（phase4-g，T-005） ----
 * 载荷 [seq u32][hist 3300×f32（t 主序 × 11 ch，原始域）][kf 180×f32（k 主序 ×
 * 6 ch，原始域）]；固件内归一化（ems_reatt_params 冻结常数）→ int8 CPU 推理
 * → 单调分位变换；回 RSP_REATT [seq u32][q 90×f32 kW][infer_us u32]。
 * 输入域 = 原始物理量（三口径契约：归一化在固件内、与训练/标定同变换）。 */
static void ems_handle_cmd_reatt(const uint8_t *payload, uint16_t len)
{
    static float hist_raw[EMS_REATT_HIST_N];
    static float kf_raw[EMS_REATT_KF_N];
    float q_kW[EMS_PROTO_REATT_Q_FLT];
    uint32_t seq;
    uint32_t us = 0u;
    uint8_t rsp[EMS_PROTO_RSP_REATT_LEN];

#if defined(EMS_N1_LADDER) && EMS_N1_LADDER && !(defined(EMS_N1_SEG_ONLY) && EMS_N1_SEG_ONLY)
    /* N1 阶梯 bisection 探针（2026-09-24 夜，仅 Debug 探针形态）：
     * 每次 CMD_REATT 执行一轮 FULL/L0/ZA0/DC0/LN0/L1 对拍并经 UART 倒 RS- 判决行。 */
    {
        extern void ems_n1_ladder_run(void);
        ems_n1_ladder_run();
    }
#endif

    if (len < EMS_PROTO_CMD_REATT_LEN) {
        ems_proto_event_push(&ems_proto_st, EMS_EVT_LEN_ERR, EMS_PROTO_CMD_REATT,
                             ems_proto_last_tick_idx(&ems_proto_st));
        ems_proto_send_nack(&ems_proto_st, EMS_PROTO_CMD_REATT,
                            EMS_PROTO_ERR_LEN);
        return;
    }

    seq = ems_proto_rd_u32(&payload[0]);
    for (uint32_t i = 0u; i < EMS_REATT_HIST_N; i++) {
        hist_raw[i] = ems_proto_rd_f32(&payload[4u + 4u * i]);
    }
    for (uint32_t i = 0u; i < EMS_REATT_KF_N; i++) {
        kf_raw[i] = ems_proto_rd_f32(
            &payload[4u + 4u * EMS_REATT_HIST_N + 4u * i]);
    }

    if (ems_reatt_query(hist_raw, kf_raw, q_kW, &us) != 0) {
        ems_proto_send_nack(&ems_proto_st, EMS_PROTO_CMD_REATT,
                            EMS_PROTO_ERR_INFER);
        return;
    }

    { /* LCD 预测曲线（NPU2 线）：q_out = [q10;q50;q90] x 30 lead，取 q50 列 */
        float q50_lcd[30];
        for (uint32_t i = 0u; i < 30u; i++) q50_lcd[i] = q_kW[3u * i + 1u];
        ems_lcd_notify_q(q50_lcd);
    }

    ems_proto_enc_rsp_reatt(rsp, seq, q_kW, us);
    (void)ems_proto_send_frame(&ems_proto_st, EMS_PROTO_RSP_REATT, rsp,
                               EMS_PROTO_RSP_REATT_LEN);
}

/* ---- CMD_TICK（0x02）：一个控制拍（T-506 接管；STALE 拍保持 Pheld） ----
 * 会话/STALE 判定已在 ems_proto 解析面完成（合法 CMD_TICK 间隔 >2 s →
 * 置位 + 事件，见 ems_proto.c），此处读会话标志决定"保持"路径。 */
static void ems_handle_cmd_tick(const uint8_t *payload, uint16_t len)
{
    ems_proto_cmd_tick_t tk;
    ems_proto_rsp_tick_t r;
    uint8_t rsp[EMS_PROTO_RSP_TICK_LEN];
    uint32_t ctrl_us = 0u;
    uint32_t infer_us = 0u;

    if (len != EMS_PROTO_TICK_LEN) {
        ems_proto_event_push(&ems_proto_st, EMS_EVT_LEN_ERR, EMS_PROTO_CMD_TICK,
                             ems_proto_last_tick_idx(&ems_proto_st));
        ems_proto_send_nack(&ems_proto_st, EMS_PROTO_CMD_TICK, EMS_PROTO_ERR_LEN);
        return;
    }
    (void)ems_proto_dec_cmd_tick(payload, len, &tk);

    if (ems_proto_session_stale(&ems_proto_st) != 0u) {
        /* STALE 保持拍：不重新跑控制律、输出不突变（design §UART 超时语义）。
         * 返回 Pheld 与上一拍 ê/K/cls（ems_last），infer/ctrl 计时器不动。 */
        r.tick_idx = tk.tick_idx;
        r.P_gen_ref = ems_last.P;
        r.e_hat = ems_last.e_hat;
        r.K = ems_last.K;
        r.cls_eff = ems_last.cls_wire;
        r.infer_us = ems_last.infer_us;
        r.ctrl_us = ems_last.ctrl_us;
        r.flags = (uint8_t)(EMS_PROTO_FLAGS_STALE | ems_last.soft_flags);
        ems_proto_enc_rsp_tick(rsp, &r);
        (void)ems_proto_send_frame(&ems_proto_st, EMS_PROTO_RSP_TICK, rsp,
                                   EMS_PROTO_RSP_TICK_LEN);
        return;
    }

    /* 单位映射（design §UART CMD_TICK 信号序）：固件 v1 控制律消费
     * t/P_load/SOC/v_ship（EML 接口）；v_cmd/accel/x_ship 原样入帧，控制侧
     * accel 由 ems_ctrl 内部 vprev 差分（EML:41，勿复写） */
    ems_ctrl_in_t in;
    ems_ctrl_out_t out;
    in.t = (double)tk.sig[0];
    in.P_load = (double)tk.sig[1];
    in.SOC = (double)tk.sig[2];
    in.v_ship = (double)tk.sig[4];
    in.nn_ok = 1; /* 完整性/回退状态机由 ems_ctrl 内部经 ems_infer 失败路径驱动 */

    /* FR-505 计时：DWT CYCCNT（main.c USER CODE 使能，600 MHz → us 换算）
     * ctrl_us = 控制律整体（窗口+归一化+推理+门控+网格搜索单拍）；infer_us 为
     * 其中 mlpq+workclf 合计（ems_infer_last_us，调用方先复位）。 */
    ems_infer_last_us_reset();
    const uint32_t c0 = DWT->CYCCNT;
    (void)ems_ctrl_step(&ems_ctrl_st, &in, &out);
    ctrl_us = (uint32_t)((DWT->CYCCNT - c0) / (SystemCoreClock / 1000000u));
    infer_us = ems_infer_last_us(); /* 上一控制拍推理合计（FR-505 遥测） */

    /* LCD 实时量注入（NPU2 线可视化；BAT 支路由 P_load-P_gen 推导） */
    ems_lcd_notify((float)in.v_ship, (float)in.P_load, (float)out.P,
                   (float)in.SOC, out.ok ? 0 : 1);

    /* 遥测持久化（FR-509 逐拍遥测；held 拍其余字段无效 → 保留上一非保持拍值） */
    if (!out.held) {
        ems_last.P = (float)out.P;
        ems_last.e_hat = (float)out.e_hat;
        ems_last.K = (float)out.K;
        /* cls_eff ∈ {−1(无状态), 1..7}；else（理论不可达）按无状态处理 */
        ems_last.cls_wire = (out.cls_eff >= 1)
                                ? (uint8_t)out.cls_eff
                                : EMS_PROTO_CLS_NONE;
        ems_last.soft_flags = 0u;
        if (out.ok == 0) {
            /* 推理失败回退（ê=0，EML:166-169）→ 事件 + FALLBACK 位（FR-512） */
            ems_last.soft_flags |= EMS_PROTO_FLAGS_FALLBACK;
            ems_proto_event_push(&ems_proto_st, EMS_EVT_INFER_FAIL,
                                 EMS_PROTO_ERR_INFER, tk.tick_idx);
        }
        if (out.cls_invalid != 0) {
            /* 类别越界按非 upstream 处理（不误关预测）→ 事件 + CLS_BAD 位 */
            ems_last.soft_flags |= EMS_PROTO_FLAGS_CLS_BAD;
            ems_proto_event_push(&ems_proto_st, EMS_EVT_CLS_INVALID,
                                 (uint8_t)(out.cls_raw & 0xFFu),
                                 tk.tick_idx);
        }
        ems_last.infer_us = infer_us;
        ems_last.ctrl_us = ctrl_us;
    }
    ems_last.tick_idx = tk.tick_idx;

    /* RSP_TICK（design 帧表 0x03，每拍必答；STALE/保持拍输出不突变） */
    r.tick_idx = tk.tick_idx;
    r.P_gen_ref = ems_last.P;
    r.e_hat = ems_last.e_hat;
    r.K = ems_last.K;
    r.cls_eff = ems_last.cls_wire;
    r.infer_us = ems_last.infer_us;
    r.ctrl_us = ems_last.ctrl_us;
    r.flags = (uint8_t)(ems_last.soft_flags |
                        (out.held ? EMS_PROTO_FLAGS_HELD : 0u));
    ems_proto_enc_rsp_tick(rsp, &r);
    (void)ems_proto_send_frame(&ems_proto_st, EMS_PROTO_RSP_TICK, rsp,
                               EMS_PROTO_RSP_TICK_LEN);
}

/* ---- 完整帧分发（CRC 已过；协议层语义见 ems_proto.h） ---- */
static void ems_dispatch(void *user, uint8_t type, const uint8_t *payload,
                         uint16_t len, uint32_t seq)
{
    (void)user;
    (void)seq; /* 载荷级 seq：测试命令处理函数自载荷头读取（同一字节，双路径一致） */
    switch (type) {
    case EMS_PROTO_CMD_RESET:
        ems_ctrl_init(&ems_ctrl_st);            /* 下一 tick 平 fill（EML:27-35） */
        ems_proto_session_reset(&ems_proto_st); /* 会话/STALE/事件环清零 */
        (void)ems_proto_send_ack(&ems_proto_st, type);
        break;
    case EMS_PROTO_CMD_TICK:
        ems_handle_cmd_tick(payload, len);
        break;
    case EMS_PROTO_CMD_INFER_TEST:
        ems_handle_infer_test(payload, len);
        break;
    case EMS_PROTO_CMD_WC_TEST:
        ems_handle_wc_test(payload, len);
        break;
    case EMS_PROTO_CMD_REATT:
        ems_handle_cmd_reatt(payload, len);
        break;
    case EMS_PROTO_CMD_PING:
        (void)ems_proto_send_ack(&ems_proto_st, type);
        break;
    case EMS_PROTO_CMD_DUMP:
        /* 事件环文本行导出（首字节 'E' ≠ 0xAA）；随后 ACK 封尾（主机以
         * "EVT total=" 汇总行定界，ACK 仅确认会话活性） */
        ems_proto_dump_events(&ems_proto_st);
        (void)ems_proto_send_ack(&ems_proto_st, type);
        break;
    default:
        /* 未知/未接管 TYPE（design Error Handling）：入环 + NACK ERR_TYPE */
        ems_proto_event_push(&ems_proto_st, EMS_EVT_UNKNOWN_TYPE, type,
                             ems_proto_last_tick_idx(&ems_proto_st));
        ems_proto_send_nack(&ems_proto_st, type, EMS_PROTO_ERR_TYPE);
        break;
    }
}

/* ---- RX 中断驱动环形缓冲（T-507 bug#4 修复） ----
 * 板上实证：主循环被 TX（HAL_UART_Transmit 轮询，CHK/结果打印 ≈55ms）阻塞期间，
 * 1 字节 RDR 必然过写（ORE 置位、后续字节全部丢失、ORE 粘滞进尔压制 RXNE），
 * 帧请求前 ~600 字节蒸发 → 交替帧失败。改为每字节 RX 中断入环：主循环任意
 * 阻塞不再丢字节；错误由 ErrorCallback 重挂。 */
#define EMS_RX_RING_SIZE 1024u /* >最长帧单次突发 + TX 窗口余量（phase4-g：256→1024，
                                * CMD_REATT 13924B 长帧 + 921600 波特下接收期主循环
                                * 连续排水的裕量） */
static uint8_t ems_rx_ring[EMS_RX_RING_SIZE];
static volatile uint16_t ems_rx_head;
static volatile uint16_t ems_rx_tail;
static uint8_t ems_rx_byte; /* HAL 单字节 IT 接收位 */
volatile uint32_t ems_rx_diag_count; /* phase4-g bring-up 诊断：RX 中断累计字节数 */
const uint32_t ems_rodata_probe = 0x5A0FACE5u; /* phase4-g bring-up 诊断：已知值落 .rodata，板上报读校验 XIP 完整性 */

static void ems_rx_arm(void)
{
    (void)HAL_UART_Receive_IT(&huart1, &ems_rx_byte, 1u);
}

void ems_app_init(void)
{
    (void)memset(&ems_last, 0, sizeof ems_last);
    ems_last.cls_wire = EMS_PROTO_CLS_NONE;
    ems_ctrl_init(&ems_ctrl_st); /* EML 首调 = 平 fill 语义（ems_ctrl.h） */
    ems_proto_init(&ems_proto_st, ems_uart_write, NULL, ems_now_ms, NULL,
                   ems_dispatch, NULL);
    ems_rx_head = 0u;
    ems_rx_tail = 0u;
    ems_rx_arm(); /* RX 中断驱动（bug#4）：主循环被 TX 阻塞不再丢字节 */
}

/* 回调（环状态定义在 ems_app_init 之前） */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        uint16_t next = (uint16_t)((ems_rx_head + 1u) & (EMS_RX_RING_SIZE - 1u));
        if (next != ems_rx_tail) { /* 满则丢最新（AA 55 同步可恢复） */
            ems_rx_ring[ems_rx_head] = ems_rx_byte;
            ems_rx_head = next;
        }
        ems_rx_diag_count++;
        ems_rx_arm();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* ORE/FE/NE/PE 会终止 IT 接收（UART_EndRxTransfer）：清标志并重挂 */
        huart->Instance->ICR = USART_ICR_ORECF | USART_ICR_FECF |
                               USART_ICR_NECF | USART_ICR_PECF;
        ems_rx_arm();
    }
}

void ems_app_poll(void)
{
    while (ems_rx_tail != ems_rx_head) {
        uint8_t b = ems_rx_ring[ems_rx_tail];
        ems_rx_tail = (uint16_t)((ems_rx_tail + 1u) & (EMS_RX_RING_SIZE - 1u));
        ems_proto_feed(&ems_proto_st, b);
    }
}