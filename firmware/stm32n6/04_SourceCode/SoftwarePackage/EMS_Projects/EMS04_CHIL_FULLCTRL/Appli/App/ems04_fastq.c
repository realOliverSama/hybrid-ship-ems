/* ems04_fastq.c — A 类实时升级：快命令队列实现（PLAN §3.3/§3.4，2026-10-01）
 * SPSC 环 + osMutex（生产者互斥兜底）+ osSemaphore 计数唤醒。载荷 ≤16B
 * （EXEC_REFRESH），数值语义零改动：仅"在哪执行"变化（快命令任务），
 * "算什么"与回帧字节与单线程版逐位一致。 */
#include "ems04_fastq.h"
#include "cmsis_os2.h"

typedef struct {
    uint32_t seq;
    uint16_t len;
    uint8_t  type;
    uint8_t  payload[EMS04_FASTQ_PAYLOAD_MAX];
} ems04_fastq_slot_t;

static ems04_fastq_slot_t s_ring[EMS04_FASTQ_DEPTH];
static osMutexId_t        s_mtx;
static osSemaphoreId_t    s_sem;
static uint32_t           s_head; /* 生产者写 */
static uint32_t           s_tail; /* 消费者写 */

void ems04_fastq_init(void)
{
    s_head = s_tail = 0;
    s_mtx = osMutexNew(NULL);
    s_sem = osSemaphoreNew(EMS04_FASTQ_DEPTH, 0, NULL);
    /* 对象创建失败属集成错误： bring-up 门（PING）会当场暴露。 */
}

int ems04_fastq_push(uint8_t type, const uint8_t *payload, uint16_t len,
                     uint32_t seq)
{
    if (s_mtx == NULL || s_sem == NULL || len > EMS04_FASTQ_PAYLOAD_MAX) return -1;
    osMutexAcquire(s_mtx, osWaitForever);
    uint32_t next = (s_head + 1u) % EMS04_FASTQ_DEPTH;
    if (next == s_tail) {
        osMutexRelease(s_mtx);
        return -1; /* 满：调用方内联执行（退化为单线程语义，不丢帧） */
    }
    ems04_fastq_slot_t *s = &s_ring[s_head];
    s->seq = seq;
    s->len = len;
    s->type = type;
    for (uint16_t i = 0; i < len; i++) s->payload[i] = payload[i];
    s_head = next;
    osMutexRelease(s_mtx);
    osSemaphoreRelease(s_sem);
    return 0;
}

int ems04_fastq_pop(uint8_t *type, uint8_t payload[EMS04_FASTQ_PAYLOAD_MAX],
                    uint16_t *len, uint32_t *seq)
{
    if (s_mtx == NULL || s_sem == NULL) return -1;
    (void)osSemaphoreAcquire(s_sem, osWaitForever);
    osMutexAcquire(s_mtx, osWaitForever);
    ems04_fastq_slot_t *s = &s_ring[s_tail];
    *seq = s->seq;
    *len = s->len;
    *type = s->type;
    for (uint16_t i = 0; i < s->len; i++) payload[i] = s->payload[i];
    s_tail = (s_tail + 1u) % EMS04_FASTQ_DEPTH;
    osMutexRelease(s_mtx);
    return 0;
}
