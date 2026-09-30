/* ems_app.h — 应用编排与 UART 帧入口（T-505/T-506，spec ems01-chil-ecms-v1）。
 *
 * 裸机主循环形态：main.c USER CODE 调 ems_app_init() 一次，随后在 while(1)
 * 内调 ems_app_poll()。
 *
 * T-506 起，design §UART 帧表全部 TYPE 由 ems_proto.c（协议层）+ 本文件
 * （应用分发）接管：
 *   CMD_RESET(0x01)  → ems_ctrl_init（下一 tick 平 fill）+ 会话复位 → ACK
 *   CMD_TICK(0x02)   → ems_ctrl_step（间隔 >2 s 置 STALE 并保持 Pheld）→ RSP_TICK
 *   CMD_INFER_TEST(0x04) → 归一化 → ems_infer 后端 → 反归一化 → RSP_INFER_TEST
 *                      （T-505 交付，行为逐字节不变，回归保护）
 *   CMD_PING(0x06)   → ACK
 *   CMD_DUMP_EVENTS(0x07) → 事件环文本行导出（design §UART 分流契约）
 * 帧编解码/CRC/字节流状态机/注入会话/事件环在 ems_proto.c（T-506 接管，
 * T-505 的内联实现迁移于此；CRC 取 CCITT-FALSE：poly 0x1021 / init 0xFFFF /
 * 不反射 / 无异或输出，TYPE..PAYLOAD 作用域，线上小端，数值契约不变）。
 *
 * 帧格式（design.md §UART 协议，FR-510；单线 USART1 @115200 8N1）：
 *   [0xAA 0x55] [TYPE u8] [LEN u16] [PAYLOAD LEN B] [CRC16-CCITT u16 over TYPE..PAYLOAD]
 * 文本日志行以可打印 ASCII 开头、\r\n 结尾，首字节 ≠ 0xAA（主机按 0xAA 分流）。
 */
#ifndef EMS_APP_H
#define EMS_APP_H

#include <stdint.h>

#include "ems_proto.h" /* TYPE/ERR/载荷长度常量与帧协议 API（T-506 迁移，数值不变） */

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化应用：控制律状态复位（EML 首调 = 平 fill 语义）、协议会话/解析器复位。 */
void ems_app_init(void);

/* 主循环轮询：拉取 UART 字节流 → ems_proto 解析 → 帧分发（阻塞面仅单帧处理）。
 * 接收方案保持 T-505 的 HAL_UART_Receive timeout=0 逐字节轮询 + BSP_UART_RX_DISABLE
 * （请求-响应协议下主机等 RSP 再发，单帧处理期间不收帧，无溢出路径）。 */
void ems_app_poll(void);

/* A 类 RTOS 化（PLAN §3.3/§3.4）：
 *  UART 发送互斥初始化（协议/快命令任务共用 HAL 发送路径的串行化）。
 *  须在调度器启动前调用一次（app_threadx.c）。 */
void ems_tx_sync_init(void);
/* 快命令任务入口：执行一条出队快命令（EXEC_REFRESH/PING/PROF 白名单）。 */
void ems_dispatch_fast(uint8_t type, const uint8_t *payload,
                       uint16_t len, uint32_t seq);
/* 慢执行任务入口：执行一条出队重命令（TICK_CTRL/REATT/DPDBG/…）。 */
void ems_dispatch_slow_exec(uint8_t type, const uint8_t *payload,
                            uint16_t len, uint32_t seq);

#ifdef __cplusplus
}
#endif

#endif /* EMS_APP_H */