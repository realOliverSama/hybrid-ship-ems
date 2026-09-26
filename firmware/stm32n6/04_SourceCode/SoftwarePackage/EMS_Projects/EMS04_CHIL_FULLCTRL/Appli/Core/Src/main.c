/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* EMS_N1_LADDER（SRAM 直载 RAM-Release）同 DEBUG：绕过 FSBL，需自配时钟与
 * HyperRAM/LCD fb（缺后者 = LCD 首帧写 0x90000000 BusFault→lockstop，板上实锤） */
#if defined(DEBUG) || (defined(EMS_N1_LADDER) && !EMS_EXTMEM_PREINIT_BY_FSBL)
#include "../../../../../Drivers/BSP/SYS/sys.h"
#include "../../../../../Drivers/BSP/HyperRAM/hyperram.h"
#endif
#include "../../../../../Drivers/BSP/LED/led.h"
#include "../../../../../Drivers/BSP/UART/uart.h"
#include "ems_config.h"
#include "ems_app.h"
#include "ems04_beat_svc.h" /* EMS04 S2：拍回放服务（RTT ch0，ems04_beat_svc.c） */
#include "ems_lcd.h" /* phase4-g NPU2：RGBLCD 竖装可视化 */
#if (EMS_FW_MODE == EMS_FW_MODE_NPU)
#include "npu_cache.h" /* Middlewares/STEdgeAI/Npu/Devices/STM32N6XX（CMake EMS01 AI 块） */
/* LL EnableClockLowPower 三族（LL_BUS/LL_MEM/LL_AHB/LL_APB/LL_MISC，同文件内定义），
 * npu_low_power_clock_config()（991 low_power_clock_config 移植，T-507 根因②）依赖 */
#include "stm32n6xx_ll_bus.h"
#if defined(DEBUG) || (defined(EMS_N1_LADDER) && !EMS_EXTMEM_PREINIT_BY_FSBL)
/* bare 名 include：有意不用相对路径，避免 prepare 误检 NORFlash 组件并
 * 把 ExtMem loader 胶水（extmem_user_driver.c）混入 AUTO BSP 块；NORFlash BSP
 * 源文件与包含路径由 Appli/CMakeLists.txt 的 EMS01 AI 块显式接线（991 同款 4 文件）。
 * EMS_N1_LADDER（SRAM 直载 RAM-Release）同需 NOR 权重区（0x70B/0x70C/0x70D）。 */
#include "norflash.h"
#endif
#endif

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

UART_HandleTypeDef huart1;

XSPI_HandleTypeDef hxspi1;
XSPI_HandleTypeDef hxspi2;

/* USER CODE BEGIN PV */
#if defined(DEBUG) || (defined(EMS_N1_LADDER) && !EMS_EXTMEM_PREINIT_BY_FSBL)
static HyperRAM_ObjectTypeDef HyperRAMObject = {0};
#if (EMS_FW_MODE == EMS_FW_MODE_NPU)
static NORFlash_ObjectTypeDef NORFlashObject = {0};
/* hxspi2 / MX_XSPI2_Init 由 CubeMX 依已授权 .ioc 生成（非 static，见生成代码），
 * USER CODE 不再手写同款定义。 */
#endif
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */
void PeriphCommonClock_Config(void); /* phase4-g：定义在本文件后部，显式原型（CubeMX 世系丢失的调用补回，见 SysInit 注） */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if (EMS_FW_MODE == EMS_FW_MODE_NPU)
/* 991 main.c:849-852 + 2.2 官方 N6 示例 NPU_Config 的合并：NPU 时钟 + 复位释放；
 * CACHEAXI 时钟 + 复位释放（2.2 NPU_Config 同段执行；991 由 FSBL/既有 RCC 配置
 * 代做，EMS01 .ioc 无 CACHEAXI/RAMCFG 配置，须显式使能，npu_cache_init 前置条件）。 */
static void npu_config(void)
{
  /* NPU 域 IC2 频率实验（2026-09-02 深夜，用户指令"拉到最大试试"）：IC2DIV 3→2
   * （400→600MHz）板上实测 **NO-GO**——LL_ATON_RT_Main 挂死（调试器停机定位
   * ll_aton_rt_main.c:67），且 NOR memory-mapped 权重区读回与烧录数据失配
   * （XSPI2 kernel 时钟随 IC2/AXI 域同比例抬升 200→300MHz，超出
   * MX25UM25645G 200MHz 规格 → 读失真）。回退至原 IC2 频率域；"NPU 频率提升"
   * 维持 v1 design 既定结论：列入后续硬件方向（需 RM 时钟树全查 + XSPI 重定时
   * 验证），Phase 3 不做。原始 .ioc IC2Div=3/400MHz 恢复。 */

  __HAL_RCC_NPU_CLK_ENABLE();
  __HAL_RCC_NPU_FORCE_RESET();
  __HAL_RCC_NPU_RELEASE_RESET();
  __HAL_RCC_CACHEAXI_CLK_ENABLE();
  __HAL_RCC_CACHEAXI_FORCE_RESET();
  __HAL_RCC_CACHEAXI_RELEASE_RESET();
}

/* 2.2 官方 N6 示例 system_init_post 的 NPU 必需子集（CMSIS 直写；本项目 .ioc 无
 * RAMCFG 外设，991 的 MX_RAMCFG_Init/HAL_RAMCFG_EnableAXISRAM 不可用，等价移植）：
 *  - RCC->MEMENR：AXISRAM3..6 + CACHEAXIRAM 存贮体供电/时钟
 *    （Model 各子目录 my_mpools/stm32n6.mpool 的激活/内部缓冲落在 AXISRAM3-6
 *    0x34200000..，NPU cache tag/data 落 CACHEAXIRAM；FSBL 不代做）；
 *  - RAMCFG_SRAMx_AXI->CR 清 RAMCFG_CR_SRAMSD（SRAM3-6 shutdown 位，
 *    复位后置位，2.2 同款；SRAM2 非 NPU 池不动）。
 * 时钟树零改动（本函数不触碰 PLL/ICx，只开存储体供电），RIF 配置不触碰。 */
static void npu_ram_init(void)
{
  /* RAMCFG 外设时钟必须先行：否则下面对 RAMCFG_SRAMx_AXI->CR 的 SRAMSD 清零写
   * 入无效（寄存器无时钟 = 静默丢弃），AXISRAM3-6 保持下电，NPU 访问 activation
   * 缓冲即 AXI 总线挂死（T-507 板上实测：首帧推理后整机死锁+debug AP 失联）。
   * 对照 991 stm32n6xx_hal_msp.c:411 __HAL_RCC_RAMCFG_CLK_ENABLE()。 */
  __HAL_RCC_RAMCFG_CLK_ENABLE();
  /* vendor LL_MEM_EnableClock 同款（stm32n6xx_ll_bus.h:678-685）：经置位寄存器
   * MEMENSR（RM0486 14.10.146）原子置位 + 读回空转建立时序；MEMENR 本体 RMW 写
   * 对 RIF 保护位不可靠（T-507 根因①：AXISRAM3-6EN/CACHEAXIRAMEN 复位=0） */
  RCC->MEMENSR = RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN |
                 RCC_MEMENR_AXISRAM5EN | RCC_MEMENR_AXISRAM6EN |
                 RCC_MEMENR_CACHEAXIRAMEN;
  (void)RCC->MEMENR; /* LL 同款使能后读回空转 */
  RAMCFG_SRAM3_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM4_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM5_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM6_AXI->CR &= ~RAMCFG_CR_SRAMSD;
}

/* 991 main.c:863-868：NPU cache 使能（npu_cache.c 为 vendor 原路径编译）。
 * 4.0.1 语义变化（stai 迁移，T-816c）：npu_cache_init 改为 npu_cache.c 内
 * static inline，其 HAL_CACHEAXI_Init 语义已并入 npu_cache_enable()（内部
 * 先 init 再使能，含 HAL_BUSY 重试）；__weak npu_cache_enable_clocks_and_
 * reset() 默认空实现，CACHEAXI 时钟/复位仍由上方 npu_config() 负责。 */
static void npu_cache_config(void)
{
  npu_cache_enable();
}

/* 991 main.c:869-885 同款（去 ThreadX 语义，保留寄存器效果）：全部总线/存储体
 * sleep 时钟使能。MEMLPENR 复位=0（RM0486 14.10.93），WFE 等待 NPU 完成期间
 * 存储体时钟门控会死锁 NPU AXI 事务（T-507 根因②）。 */
static void npu_low_power_clock_config(void)
{
  LL_BUS_EnableClockLowPower(~0);
  LL_MEM_EnableClockLowPower(~0);
  LL_AHB1_GRP1_EnableClockLowPower(~0);
  LL_AHB2_GRP1_EnableClockLowPower(~0);
  LL_AHB3_GRP1_EnableClockLowPower(~0);
  LL_AHB4_GRP1_EnableClockLowPower(~0);
  LL_AHB5_GRP1_EnableClockLowPower(~0);
  LL_APB1_GRP1_EnableClockLowPower(~0);
  LL_APB1_GRP2_EnableClockLowPower(~0);
  LL_APB2_GRP1_EnableClockLowPower(~0);
  LL_APB4_GRP1_EnableClockLowPower(~0);
  LL_APB4_GRP2_EnableClockLowPower(~0);
  LL_APB5_GRP1_EnableClockLowPower(~0);
  LL_MISC_EnableClockLowPower(~0);
}
#endif /* EMS_FW_MODE == EMS_FW_MODE_NPU */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* 991 main.c:105-106 同款：使能 I/D-Cache 前先激活 cache RAM（ARM ARM 上 CCR
   * 使能而未激活 = UNPREDICTABLE；05_Serial 模板缺此两行，T-507 NPU 挂死排查补齐） */
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_ICACTIVE_Msk;
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk;

  /* USER CODE END 1 */

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* EMS_N1_LADDER=1 = SRAM 直载的 RAM-Release 服务形态：绕过 FSBL，时钟与
   * HyperRAM 必须由应用自配（同 Debug；Flash-boot Release 形态由 FSBL 代做，
   * 保持原 DEBUG 守卫不变）。缺 HyperRAM memory-mapped 时 LCD 帧缓冲
   * (0x90000000) 首帧写即 BusFault→lockstop（2026-09-25 板上实锤）。 */
#if defined(DEBUG) || (defined(EMS_N1_LADDER) && !EMS_EXTMEM_PREINIT_BY_FSBL)
  sys_clock_config_debug();
#endif

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */
  /* phase4-g 修复（板上 bring-up 2026-09-22 定位）：CubeMX 生成的
   * PeriphCommonClock_Config()（CKPER=HSI，.ioc USART1Freq=64M）在本工程
   * main() 调用序列中缺失（ATK 模板世系丢失）——Dev boot 下 CKPER 复位默认
   * 恰为 HSI(64MHz) 故从未暴露；Flash boot（FSBL 置 PLL/SYSCLK 后）USART1
   * 有效波特率随 CLKP 漂移（实测 115200 配置全线乱码）。此处显式补调，
   * 任何 boot 模式下内核时钟确定 = HSI 64MHz。 */
  PeriphCommonClock_Config();
#if defined(DEBUG) || (defined(EMS_N1_LADDER) && !EMS_EXTMEM_PREINIT_BY_FSBL)
  MX_XSPI1_Init();
  HyperRAM_Init(&HyperRAMObject, &hxspi1);
  HyperRAM_EnableMemoryMappedMode(&HyperRAMObject);
  /* NPU 模式的 XSPI2 初始化 + NOR memory-mapped（991 main.c:701-724 序列的
   * NOR 部分）在 USER CODE 2 段执行：MX_XSPI2_Init 为 CubeMX 生成版、
   * 由生成序列调用，NOR 只需在其后使能（生成代码见 "Initialize all
   * configured peripherals" 一节）。 */
#endif

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */
  led_init();         /* 初始化LED */
  uart_init(115200);  /* 初始化串口 */

#if defined(EMS_REATT_UART_BAUD) && (EMS_REATT_UART_BAUD != 115200)
  /* phase4-g（D6）：USART1 运行时波特率覆盖。CMD_REATT 请求帧 13924 B，
   * 115200 下仅传输即 1.21 s/锚点 ×~350 锚点/航次不可接受；升至
   * EMS_REATT_UART_BAUD（ems_config.h，默认 921600）。不修改 .ioc
   * （CubeMX 生成态保持 115200，此覆盖为工程自有代码、单点、可回退）。
   * 时序前提：ems_app RX 面在 ems_app_init() 才武装，此处重初始化无竞争。 */
  huart1.Init.BaudRate = EMS_REATT_UART_BAUD;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
#endif

  /* DWT CYCCNT 使能（FR-505 infer_us/ctrl_us 测量；代码使能，不涉 .ioc/时钟树） */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

#if defined(DEBUG) || (defined(EMS_N1_LADDER) && !EMS_EXTMEM_PREINIT_BY_FSBL)
#if (EMS_FW_MODE == EMS_FW_MODE_NPU)
  /* 991 main.c:701-723 序列的 NOR 部分（SystemIsolation_Config 之后）：
   * MX_XSPI2_Init（CubeMX 生成版，调用不落自动区，与 991 main.c:701 同由应用显式调用）
   * → NOR 读权重（0x70200000/0x70400000）前提的 memory-mapped 使能。
   * Release（ROMxspi2）由 boot 链完成 XSPI2 memory-mapped，不入此分支。 */
  MX_XSPI2_Init();
  if (NORFlash_Init(&NORFlashObject, &hxspi2,
                    HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_XSPI2)) != NORFlash_OK)
  {
    Error_Handler();
  }
  if (NORFlash_EnableMemoryMappedMode(&NORFlashObject) != NORFlash_OK)
  {
    Error_Handler();
  }
#endif
#endif

#if (EMS_FW_MODE == EMS_FW_MODE_NPU)
  /* 供电先行（2.2 system_init_post 等价，npu_ram_init），随后 991 main.c:730-735
   * 序列（去 ThreadX/iac 附属段）：NPU/CACHEAXI 时钟复位 → NPU cache 使能 →
   * npu_low_power_clock_config()（991 附属段实为 NPU 必需：MEMLPENR 复位=0 时
   * WFE 等待 NPU 完成期间存储体时钟门控会死锁 NPU AXI 事务，T-507 根因②）。
   * RIF 隔离不在此处调用：生成的 SystemIsolation_Config() 已在上方外设初始化区
   * 执行（先于 NPU 时钟，顺序同 991:704-705/730 单次调用），NPU 从端 RISUP +
   * NPU 主端 RIMC（USER CODE RIF_Init 1）均为 991 同款属性。 */
  npu_ram_init();
  npu_config();
  npu_cache_config();
  npu_low_power_clock_config();
#endif

  ems_app_init(); /* 控制律状态复位 + 协议解析复位（ems_app.c，T-505） */
  ems04_beat_init(); /* EMS04 S2：绑定 law6 冻结表 + RTT ch0（ems04_beat_svc.c） */
#if (EMS_FW_MODE == EMS_FW_MODE_NPU)
  printf("\r\nEMS01 CHIL ECMS firmware v1 (NPU backend, weights @0x70200000/0x70400000)\r\n");
#else
  printf("\r\nEMS01 CHIL ECMS firmware v1 (REF backend)\r\n");
#endif
  /* NPU2 调试：LCD init 挪到 banner 后（串口先活，RS-TRACE 定位挂点） */
  printf("RS-TRACE main: before lcd_init\\r\\n");
  ems_lcd_init(); /* RGBLCD 竖装可视化（NPU2 线；IC16/LTDC/DMA2D/RIF + 首帧） */
  printf("RS-TRACE main: after lcd_init\\r\\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t heartbeat = 0;
  while (1)
  {
    ems_app_poll(); /* UART 帧 → INFER_TEST/PING 分发（ems_app.c，T-505） */
    ems04_beat_poll(); /* EMS04 S2：RTT 拍帧 → law6 决策 → 16B 回帧（ems04_beat_svc.c） */
    ems_lcd_poll(HAL_GetTick()); /* RGBLCD 1 Hz 重绘节流（NPU2 线） */
    heartbeat++;
    if ((heartbeat & 0x000FFFFFu) == 0u)
    {
      LED0_TOGGLE(); /* 心跳指示（EMS 应用替换 05_Serial 按键演示） */
      (void)0; /* phase4-g bring-up 诊断全部移除（三探针/眨眼/回显）；心跳 LED 保留 */
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CKPER;
  PeriphClkInitStruct.CkperClockSelection = RCC_CLKPCLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RIF Initialization Function
  * @param None
  * @retval None
  */
  static void SystemIsolation_Config(void)
{

  /* USER CODE BEGIN RIF_Init 0 */

  /* USER CODE END RIF_Init 0 */

  /* set all required IPs as secure privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /*RISUP configuration*/
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  /* RIF-Aware IPs Config */

  /* set up PWR configuration */
  HAL_PWR_ConfigAttributes(PWR_ITEM_0,PWR_SEC_NPRIV);

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_8,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPION,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOO,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_0,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_2,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_4,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOP,GPIO_PIN_7,GPIO_PIN_SEC|GPIO_PIN_NPRIV);

  /* USER CODE BEGIN RIF_Init 1 */
#if (EMS_FW_MODE == EMS_FW_MODE_NPU)
  /* NPU 主端 RIMC 授权（991 main.c:392 同款）：RIMC_master={CID1,SEC|PRIV}。
   * CubeMX 仅为静态 CID≠0 的主设备生成 RIMC 块（991 因 DCMIPP/DMA2D/LTDC 存在）。
   * EMS01 无相机/LCD 主设备且 .ioc 只含 NPU_CID_RIMU-1=1，生成区仅产出 NPU
   * 从端 RISUP 行（上方 244 行），故 NPU 主端 RIMC 保留在 USER CODE。
   * 不弱化不扩大授权；991 的 DCMIPP/DMA2D/LTDC 行不移植。 */
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &RIMC_master);
#endif
  /* USER CODE END RIF_Init 1 */
  /* USER CODE BEGIN RIF_Init 2 */

  /* USER CODE END RIF_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief XSPI1 Initialization Function
  * @param None
  * @retval None
  */
void MX_XSPI1_Init(void)
{

  /* USER CODE BEGIN XSPI1_Init 0 */

  /* USER CODE END XSPI1_Init 0 */

  XSPIM_CfgTypeDef sXspiManagerCfg = {0};
  XSPI_HyperbusCfgTypeDef sHyperBusCfg = {0};

  /* USER CODE BEGIN XSPI1_Init 1 */

  /* USER CODE END XSPI1_Init 1 */
  /* XSPI1 parameter configuration*/
  hxspi1.Instance = XSPI1;
  hxspi1.Init.FifoThresholdByte = 4;
  hxspi1.Init.MemoryMode = HAL_XSPI_SINGLE_MEM;
  hxspi1.Init.MemoryType = HAL_XSPI_MEMTYPE_HYPERBUS;
  hxspi1.Init.MemorySize = HAL_XSPI_SIZE_256MB;
  hxspi1.Init.ChipSelectHighTimeCycle = 2;
  hxspi1.Init.FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE;
  hxspi1.Init.ClockMode = HAL_XSPI_CLOCK_MODE_0;
  hxspi1.Init.WrapSize = HAL_XSPI_WRAP_32_BYTES;
  hxspi1.Init.ClockPrescaler = 1 - 1;
  hxspi1.Init.SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE;
  hxspi1.Init.DelayHoldQuarterCycle = HAL_XSPI_DHQC_DISABLE;
  hxspi1.Init.ChipSelectBoundary = HAL_XSPI_BONDARYOF_NONE;
  hxspi1.Init.MaxTran = 0;
  hxspi1.Init.Refresh = 0;
  hxspi1.Init.MemorySelect = HAL_XSPI_CSSEL_NCS1;
  if (HAL_XSPI_Init(&hxspi1) != HAL_OK)
  {
    Error_Handler();
  }
  sXspiManagerCfg.nCSOverride = HAL_XSPI_CSSEL_OVR_NCS1;
  sXspiManagerCfg.IOPort = HAL_XSPIM_IOPORT_1;
  sXspiManagerCfg.Req2AckTime = 1;
  if (HAL_XSPIM_Config(&hxspi1, &sXspiManagerCfg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
  sHyperBusCfg.RWRecoveryTimeCycle = 7;
  sHyperBusCfg.AccessTimeCycle = 7;
  sHyperBusCfg.WriteZeroLatency = HAL_XSPI_LATENCY_ON_WRITE;
  sHyperBusCfg.LatencyMode = HAL_XSPI_FIXED_LATENCY;
  if (HAL_XSPI_HyperbusCfg(&hxspi1, &sHyperBusCfg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN XSPI1_Init 2 */

  /* USER CODE END XSPI1_Init 2 */

}

/**
  * @brief XSPI2 Initialization Function
  * @param None
  * @retval None
  */
void MX_XSPI2_Init(void)
{

  /* USER CODE BEGIN XSPI2_Init 0 */

  /* USER CODE END XSPI2_Init 0 */

  XSPIM_CfgTypeDef sXspiManagerCfg = {0};

  /* USER CODE BEGIN XSPI2_Init 1 */

  /* USER CODE END XSPI2_Init 1 */
  /* XSPI2 parameter configuration*/
  hxspi2.Instance = XSPI2;
  hxspi2.Init.FifoThresholdByte = 4;
  hxspi2.Init.MemoryMode = HAL_XSPI_SINGLE_MEM;
  hxspi2.Init.MemoryType = HAL_XSPI_MEMTYPE_MACRONIX;
  hxspi2.Init.MemorySize = HAL_XSPI_SIZE_256MB;
  hxspi2.Init.ChipSelectHighTimeCycle = 1;
  hxspi2.Init.FreeRunningClock = HAL_XSPI_FREERUNCLK_DISABLE;
  hxspi2.Init.ClockMode = HAL_XSPI_CLOCK_MODE_0;
  hxspi2.Init.WrapSize = HAL_XSPI_WRAP_NOT_SUPPORTED;
  hxspi2.Init.ClockPrescaler = 1 - 1;
  hxspi2.Init.SampleShifting = HAL_XSPI_SAMPLE_SHIFT_NONE;
  hxspi2.Init.DelayHoldQuarterCycle = HAL_XSPI_DHQC_DISABLE;
  hxspi2.Init.ChipSelectBoundary = HAL_XSPI_BONDARYOF_NONE;
  hxspi2.Init.MaxTran = 0;
  hxspi2.Init.Refresh = 0;
  hxspi2.Init.MemorySelect = HAL_XSPI_CSSEL_NCS1;
  if (HAL_XSPI_Init(&hxspi2) != HAL_OK)
  {
    Error_Handler();
  }
  sXspiManagerCfg.nCSOverride = HAL_XSPI_CSSEL_OVR_NCS1;
  sXspiManagerCfg.IOPort = HAL_XSPIM_IOPORT_2;
  sXspiManagerCfg.Req2AckTime = 1;
  if (HAL_XSPIM_Config(&hxspi2, &sXspiManagerCfg, HAL_XSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN XSPI2_Init 2 */

  /* USER CODE END XSPI2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOP_CLK_ENABLE();
  __HAL_RCC_GPIOO_CLK_ENABLE();
  __HAL_RCC_GPION_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);

  /*Configure GPIO pin : PE10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PG10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* 按键引脚配置：KEY0(PC6)、KEY1(PD1)、KEY2(PG11) 上拉输入，WKUP(PC13) 下拉输入 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
