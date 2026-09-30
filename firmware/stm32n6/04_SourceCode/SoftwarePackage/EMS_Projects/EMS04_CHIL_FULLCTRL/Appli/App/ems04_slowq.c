/* ems04_slowq.c — A 类实时升级：重命令双缓冲队列实现（PLAN §3.3，2026-10-01）
 * 双缓冲 + osSemaphore 计数（生产者 release，消费者 acquire）+ osMutex 保护
 * 空闲缓冲选择。命令体在慢执行任务内执行（低于排水任务与快命令任务优先级），
 * DP/推理长算期间被高优先级任务抢占——窗内 EXEC_REFRESH <10ms 的结构性前提。 */
#include "ems04_slowq.h"
#include "cmsis_os2.h"

#include <string.h>

typedef struct {
    uint32_t seq;
    uint16_t len;
    uint8_t  type;
    uint8_t  payload[EMS04_SLOWQ_PAYLOAD_MAX];
} ems04_slowq_slot_t;

static ems04_slowq_slot_t s_slots[2];
static osMutexId_t        s_mtx;
static osSemaphoreId_t    s_sem;      /* 已填充缓冲计数 */
static int                s_busy[2];  /* 消费者持有标志 */
static uint32_t           s_wr;

void ems04_slowq_init(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_wr = 0;
    s_busy[0] = s_busy[1] = 0;
    s_mtx = osMutexNew(NULL);
    s_sem = osSemaphoreNew(2, 0, NULL);
}

int ems04_slowq_push(uint8_t type, const uint8_t *payload, uint16_t len,
                     uint32_t seq)
{
    if (s_mtx == NULL || s_sem == NULL || len > EMS04_SLOWQ_PAYLOAD_MAX) return -1;
    osMutexAcquire(s_mtx, osWaitForever);
    /* 选一个空闲缓冲（非 busy 且非"已填充待消费"——后者由信号量计数体现，
     * 简化：只在两个 slot 里找 !busy 的；填充后立即置 busy（消费者 release 才清）。 */
    int idx = -1;
    for (int i = 0; i < 2; i++) {
        if (!s_busy[i]) { idx = i; break; }
    }
    if (idx < 0) { osMutexRelease(s_mtx); return -1; }
    ems04_slowq_slot_t *s = &s_slots[idx];
    s->seq = seq;
    s->len = len;
    s->type = type;
    memcpy(s->payload, payload, len);
    s_busy[idx] = 1;
    osMutexRelease(s_mtx);
    osSemaphoreRelease(s_sem);
    return 0;
}

int ems04_slowq_pop(uint8_t *type, const uint8_t **payload, uint16_t *len,
                    uint32_t *seq)
{
    if (s_mtx == NULL || s_sem == NULL) return -1;
    (void)osSemaphoreAcquire(s_sem, osWaitForever);
    osMutexAcquire(s_mtx, osWaitForever);
    int idx = -1;
    for (int i = 0; i < 2; i++) {
        if (s_busy[i] == 1) { idx = i; break; }
    }
    if (idx < 0) { osMutexRelease(s_mtx); return -1; }
    ems04_slowq_slot_t *s = &s_slots[idx];
    *seq = s->seq;
    *len = s->len;
    *type = s->type;
    *payload = s->payload;
    osMutexRelease(s_mtx);
    return 0; /* busy 保持 1（持有中），release() 归还 */
}

void ems04_slowq_release(void)
{
    osMutexAcquire(s_mtx, osWaitForever);
    for (int i = 0; i < 2; i++) {
        if (s_busy[i] == 1) { s_busy[i] = 0; break; }
    }
    osMutexRelease(s_mtx);
}
