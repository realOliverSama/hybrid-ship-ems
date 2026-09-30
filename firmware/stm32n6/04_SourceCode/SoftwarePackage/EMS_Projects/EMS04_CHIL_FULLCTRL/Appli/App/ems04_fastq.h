/* ems04_fastq.h — A 类实时升级：快命令队列（PLAN §3.3/§3.4，2026-10-01）
 *
 * 协议任务（主轮询）解析出的"快命令"（EXEC_REFRESH 0x12 / PING 0x06 /
 * PROF 0x10，载荷 ≤16B）经本队列移交高优先级快命令任务执行，使 DP
 * 阻塞窗内的执行层刷新不再排队（端到端 <10 ms 验收门 §5.1）。
 * SPSC 单生产者（协议任务）单消费者（快命令任务）；cmsis_os2 对象由
 * ems04_fastq_init() 在调度器启动前创建（app_threadx.c）。 */
#ifndef EMS04_FASTQ_H
#define EMS04_FASTQ_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_FASTQ_PAYLOAD_MAX 16u   /* EXEC_REFRESH 16B 为最大快命令载荷 */
#define EMS04_FASTQ_DEPTH       16u

/* 调度器启动前调用一次（创建 mutex/semaphore）。 */
void ems04_fastq_init(void);

/* 生产者（协议任务上下文）：入队快命令；满则返回 -1（调用方回退为内联执行，
 * 位级语义与单线程版一致——不丢帧）。 */
int ems04_fastq_push(uint8_t type, const uint8_t *payload, uint16_t len,
                     uint32_t seq);

/* 消费者（快命令任务上下文）：阻塞等待一条快命令。返回 0=OK。 */
int ems04_fastq_pop(uint8_t *type, uint8_t payload[EMS04_FASTQ_PAYLOAD_MAX],
                    uint16_t *len, uint32_t *seq);

#ifdef __cplusplus
}
#endif
#endif /* EMS04_FASTQ_H */
