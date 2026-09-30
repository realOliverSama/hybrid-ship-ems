/* ems04_slowq.h — A 类实时升级：重命令双缓冲队列（PLAN §3.3，2026-10-01）
 *
 * 重命令（TICK_CTRL 13960B / REATT 13924B / DPDBG 260B / INFER_TEST 等）的
 * 载荷远超快队列槽位，且排水任务必须立即继续装配下一帧——故以**双缓冲拷贝**
 * 语义移交慢执行任务：生产者写入空闲缓冲，消费者（慢执行任务）独占持有直到
 * ems04_slowq_release()。容量 2 = 宿主 runner 同步语义（单 outstanding 重命令）
 * 下的一写一读全覆盖；第 3 条重命令到达且无空闲缓冲 → push 返回 -1，调用方
 * 内联回退（单线程语义保底，R-4 契约：重命令不与拍窗并发仍由 host 保证）。 */
#ifndef EMS04_SLOWQ_H
#define EMS04_SLOWQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_SLOWQ_PAYLOAD_MAX 13960u  /* TICK_CTRL 13960B 为最大重命令载荷 */

void ems04_slowq_init(void);
int  ems04_slowq_push(uint8_t type, const uint8_t *payload, uint16_t len,
                      uint32_t seq);
/* pop 成功后 payload 指向队列内部缓冲，处理完成必须调用 release() 归还。 */
int  ems04_slowq_pop(uint8_t *type, const uint8_t **payload, uint16_t *len,
                     uint32_t *seq);
void ems04_slowq_release(void);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_SLOWQ_H */
