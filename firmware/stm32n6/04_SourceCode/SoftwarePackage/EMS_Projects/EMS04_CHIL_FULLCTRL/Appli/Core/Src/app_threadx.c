/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
    * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"
#include "cmsis_os2.h"
#include "main.h"
#include "led.h"
#include "ems_app.h"
#include "ems_lcd.h"
#include "ems04_fastq.h"
#include "ems04_slowq.h"
#include "ems04_beat_svc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
/* A 类实时升级（PLAN §3.3，2026-10-01）：单线程主循环三分结构 → 三任务。
 * 优先级（CMSIS-RTOS2 数小者高）：快命令 3 > 协议 5 > LCD 10。
 * 位级红线：任务化只改变"何时处理"，不改变"算什么"——
 *   - 协议任务 = 原 while(1) 的 ems_app_poll + ems04_beat_poll（重命令同步执行语义不变）；
 *   - 快命令任务 = EXEC_REFRESH/PING/PROF（经 ems04_fastq 移交，DP 窗内可抢占）；
 *   - LCD 任务 = ems_lcd_poll（内部 1 Hz 节流语义原样迁移，绘制不再阻塞他任务）。
 * m_last_d1 跨任务访问以 seqlock 快照治理（ems04_tick_ctrl.c §3.4/R-2）；
 * UART 发送路径以 osMutex 串行化（ems_app.c ems_tx_sync_init）。 */
static osThreadId_t s_tid_prot;
static osThreadId_t s_tid_fast;
static osThreadId_t s_tid_slow;
static osThreadId_t s_tid_lcd;
static osThreadAttr_t s_thr_attr[3];

static void EmsProtThread(void *arg)
{
    (void)arg;
    for (;;) {
        ems_app_poll();     /* UART 帧装配 + 分发（全部入队：排水永不阻塞，§3.3） */
        ems04_beat_poll();  /* EMS04 S2：RTT 拍帧 → law6 决策 → 16B 回帧 */
        osThreadYield();
    }
}

/* 慢执行任务：重命令体（TICK_CTRL/REATT/DPDBG/…）在此同步执行——DP/推理
 * 长算期间被快命令/排水任务抢占（低于二者优先级），执行序 FIFO 与单线程等价。 */
static void EmsSlowThread(void *arg)
{
    (void)arg;
    for (;;) {
        uint8_t type; const uint8_t *payload; uint16_t len; uint32_t seq;
        if (ems04_slowq_pop(&type, &payload, &len, &seq) == 0) {
            ems_dispatch_slow_exec(type, payload, len, seq);
            ems04_slowq_release();
        }
    }
}

static void EmsFastThread(void *arg)
{
    (void)arg;
    for (;;) {
        uint8_t type; uint8_t payload[EMS04_FASTQ_PAYLOAD_MAX];
        uint16_t len; uint32_t seq;
        if (ems04_fastq_pop(&type, payload, &len, &seq) == 0) {
            ems_dispatch_fast(type, payload, len, seq);
        }
    }
}

static void EmsLcdThread(void *arg)
{
    (void)arg;
    uint32_t heartbeat = 0;
    for (;;) {
        ems_lcd_poll(HAL_GetTick()); /* 1 Hz 节流（内部语义原样迁移） */
        heartbeat++;
        if ((heartbeat & 0x000FFFFFu) == 0u) {
            LED0_TOGGLE(); /* 心跳指示（自 main.c while(1) 迁入） */
        }
        osDelay(10);
    }
}

/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */
  /* A 类 bring-up 修复（2026-10-01，p5q 板锤）：CMSIS-RTOS2 对象创建前必须
   * osKernelInitialize()——它驱动 MemInit() 创建 Stack/Heap 内核字节池；
   * 缺失时 osThreadNew/osMutexNew/osSemaphoreNew 全部静默返回 NULL
   * （tx_byte_allocate 对未创建池失败），且 KernelState 停在 osKernelInactive。
   * CubeMX 生成模板只示范 tx_* 原生 API，不含本调用。 */
  if (osKernelInitialize() != osOK)
  {
    /* USER CODE BEGIN App_ThreadX_Init_KernelErr */
    /* USER CODE END App_ThreadX_Init_KernelErr */
  }
  /* 对象/互斥初始化（调度器启动前；cmsis_os2 对象允许 pre-kernel 创建） */
  ems_tx_sync_init();      /* UART 发送互斥（ems_app.c §3.4） */
  ems04_fastq_init();      /* 快命令队列 */
  ems04_slowq_init();      /* 重命令双缓冲队列 */

  static const char *names[4] = {"EmsProt", "EmsFast", "EmsSlow", "EmsLcd"};
  static const osPriority_t prios_os[4] = {osPriorityNormal, osPriorityAboveNormal, osPriorityBelowNormal, osPriorityLow};
  static const uint32_t stacks[4] = {4096u, 2048u, 12288u, 2048u}; /* 慢任务扛 DP/推理调用深栈 */
  void (*bodies[4])(void *) = {EmsProtThread, EmsFastThread, EmsSlowThread, EmsLcdThread};
  osThreadId_t *ids[4] = {&s_tid_prot, &s_tid_fast, &s_tid_slow, &s_tid_lcd};
  for (int i = 0; i < 4; i++) {
      s_thr_attr[i] = (osThreadAttr_t){0};
      s_thr_attr[i].name = names[i];
      s_thr_attr[i].priority = prios_os[i];
      s_thr_attr[i].stack_size = stacks[i];
      *ids[i] = osThreadNew(bodies[i], NULL, &s_thr_attr[i]);
      if (*ids[i] == NULL) {
          /* fail-fast：线程创建失败时在此自旋（SWD halt 可见 PC），不再静默
           * 继续——p5q 教训：四线程 NULL 静默通过曾把根因掩埋数小时。 */
          volatile int n = i; (void)n;
          for (;;) { }
      }
  }
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN Before_Kernel_Start */

  /* USER CODE END Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN Kernel_Start_Error */

  /* USER CODE END Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
