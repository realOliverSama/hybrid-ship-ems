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
#ifdef DEBUG
#include "./SYS/sys.h"
#include "./HyperRAM/hyperram.h"
#endif
#include "./LED/led.h"
#include "./KEY/key.h"
#include "./UART/uart.h"
#include "./RGBLCD/rgblcd.h"
#include "./QMI8658A/qmi8658.h"
#include "./QMI8658A/imu.h"
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

DMA2D_HandleTypeDef hdma2d;

I2C_HandleTypeDef hi2c4;

LTDC_HandleTypeDef hltdc;

UART_HandleTypeDef huart1;

XSPI_HandleTypeDef hxspi1;

/* USER CODE BEGIN PV */
#ifdef DEBUG
static HyperRAM_ObjectTypeDef HyperRAMObject = {0};
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_DMA2D_Init(void);
static void MX_LTDC_Init(void);
static void MX_I2C4_Init(void);
static void MX_USART1_UART_Init(void);
static void SystemIsolation_Config(void);
/* USER CODE BEGIN PFP */
static void show_data(uint16_t x, uint16_t y, float data);
void usart1_send_char(uint8_t c);
void usart1_niming_report(uint8_t fun, uint8_t *data, uint8_t len);
void qmi8658_send_data(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz);
void usart1_report_imu(short roll, short pitch, short yaw, int prs, uint8_t fly_mode, uint8_t armed);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint8_t t = 0;
  uint8_t key;
  uint8_t report = 0;
  float gyro[3];
  float accel[3];
  float euler_angle[3] = {0, 0, 0};
  /* USER CODE END 1 */

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Update SystemCoreClock variable according to RCC registers values. */
  SystemCoreClockUpdate();
  HAL_Init();

  /* USER CODE BEGIN Init */
#ifdef DEBUG
  sys_clock_config_debug();
#endif

  /* USER CODE END Init */

  /* Update SystemCoreClock variable */
  SystemCoreClockUpdate();

  /* USER CODE BEGIN SysInit */
#ifdef DEBUG
  MX_XSPI1_Init();
  HyperRAM_Init(&HyperRAMObject, &hxspi1);
  HyperRAM_EnableMemoryMappedMode(&HyperRAMObject);
#endif

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA2D_Init();
  MX_LTDC_Init();
  MX_I2C4_Init();
  MX_USART1_UART_Init();
  SystemIsolation_Config();
  /* USER CODE BEGIN 2 */
  led_init();                               /* 初始化LED */
  key_init();     							/* 初始化按键 */
  uart_init(115200);                        /* 初始化串口 */
  rgblcd_init();  							/* 初始化RGB LCD */
  rgblcd_show_string(30, 50, 200, 16, 16, "STM32", RED);
  rgblcd_show_string(30, 70, 200, 16, 16, "QMI8658A TEST", RED);
  rgblcd_show_string(30, 90, 200, 16, 16, "ATOM@ALIENTEK", RED);
  rgblcd_show_string(30, 110, 200, 16, 16, "KEY0:UPLOAD ON/OFF", RED);

  while (qmi8658_init() != 0)        /* 初始化QMI8658A */
  {
      rgblcd_show_string(30, 130, 200, 16, 16, "QMI8658A Error!", RED);
      HAL_Delay(500);
      rgblcd_show_string(30, 130, 200, 16, 16, "Please Check!  ", RED);
      HAL_Delay(500);
      LED0_TOGGLE();
  }

  rgblcd_show_string(30, 130, 200, 16, 16, "QMI8658A Ready!", RED);
  rgblcd_show_string(30, 150, 200, 16, 16, "UPLOAD OFF", BLUE);
  rgblcd_show_string(30, 170, 200, 16, 16, "Temp :    . C", BLUE);
  rgblcd_show_string(30, 190, 200, 16, 16, "Pitch:    . C", BLUE);
  rgblcd_show_string(30, 210, 200, 16, 16, "Roll :    . C", BLUE);
  rgblcd_show_string(30, 230, 200, 16, 16, "Yaw  :    . C", BLUE);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    qmi8658_read_xyz(accel, gyro);
    key = key_scan(0);
    if (key == KEY0_PRES)
    {
      /* 切换匿名数据上报开关 */
      report = !report;
      if (report == 0)
      {
    	rgblcd_show_string(30, 150, 200, 16, 16, "UPLOAD OFF", BLUE);
      }
      else
      {
        rgblcd_show_string(30, 150, 200, 16, 16, "UPLOAD ON ", BLUE);
      }
    }

    /* 获取并显示温度 */
    show_data(30, 170, qmi8658_get_temperature());

    /* 获取并显示欧拉角 */
    if (g_imu_init)
    {
      imu_get_eulerian_angles(accel, gyro, euler_angle, IMU_DELTA_T);
      show_data(30, 190, euler_angle[0]);
      show_data(30, 210, euler_angle[1]);
      show_data(30, 230, euler_angle[2]);

      if (report != 0)
      {
        /* 上报匿名状态帧 */
        qmi8658_send_data(accel[0], accel[1], accel[2], gyro[0], gyro[1], gyro[2]);  /* 发送加速度+陀螺仪原始数据 */
        usart1_report_imu((int)(euler_angle[1] * 100), (int)(euler_angle[0] * 100), (int)(euler_angle[2] * 100), 0, 0, 0); /* Pitch和Roll角位置调换 */
      }
    }
    if (++t == 100)
    {
      t = 0;
      LED0_TOGGLE();
    }

    HAL_Delay(10);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M_PFC;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 255;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief I2C4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C4_Init(void)
{

  /* USER CODE BEGIN I2C4_Init 0 */

  /* USER CODE END I2C4_Init 0 */

  /* USER CODE BEGIN I2C4_Init 1 */

  /* USER CODE END I2C4_Init 1 */
  hi2c4.Instance = I2C4;
  hi2c4.Init.Timing = 0x10707DBC;
  hi2c4.Init.OwnAddress1 = 0;
  hi2c4.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c4.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c4.Init.OwnAddress2 = 0;
  hi2c4.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c4.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c4.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c4, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C4_Init 2 */

  /* USER CODE END I2C4_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 0;
  hltdc.Init.VerticalSync = 0;
  hltdc.Init.AccumulatedHBP = 40;
  hltdc.Init.AccumulatedVBP = 8;
  hltdc.Init.AccumulatedActiveW = 520;
  hltdc.Init.AccumulatedActiveH = 280;
  hltdc.Init.TotalWidth = 525;
  hltdc.Init.TotalHeigh = 288;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 480;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 272;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0;
  pLayerCfg.ImageWidth = 480;
  pLayerCfg.ImageHeight = 272;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */

  /* USER CODE END LTDC_Init 2 */

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
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;

  /*RIMC configuration*/
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1, &RIMC_master);

  /*RISUP configuration*/
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  /* RIF-Aware IPs Config */

  /* set up PWR configuration */
  HAL_PWR_ConfigAttributes(PWR_ITEM_0,PWR_SEC_NPRIV);

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOA,GPIO_PIN_3,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOD,GPIO_PIN_1,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_5,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_6,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_9,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_13,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE,GPIO_PIN_14,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_10,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG,GPIO_PIN_11,GPIO_PIN_SEC|GPIO_PIN_NPRIV);
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOP_CLK_ENABLE();
  __HAL_RCC_GPIOO_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_9, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);

  /*Configure GPIO pin : PD1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PE9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PG10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : PG11 */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
 * @brief   显示数据
 * @param   x: 标题起始X坐标
 * @param   y: 标题起始Y坐标
 * @param   data: 温度或俯仰角或横滚角或航向角数据
 * @retval  无
 */
static void show_data(uint16_t x, uint16_t y, float data)
{
    int16_t temp;

    temp = data * 10;
    if (temp < 0)
    {
        rgblcd_show_char(x + 6 * 8, y, '-', 16, 0, BLUE);
        temp = -temp;
    }
    else
    {
        rgblcd_show_char(x + 6 * 8, y, ' ', 16, 0, BLUE);
    }
    rgblcd_show_num(x + 7 * 8, y, temp / 10, 3, 16, BLUE);
    rgblcd_show_num(x + 11 * 8, y, temp % 10, 1, 16, BLUE);
}

/**
 * @brief       串口1发送1个字符
 * @param       c  : 要发送的字符
 * @retval      无
 */
void usart1_send_char(uint8_t c)
{
//	HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, 0xFFFF);
    while ((USART1->ISR & 0X40) == 0);            /* 等待上一个字符发送完成 */
    USART1->TDR = (uint8_t)c;                     /* 将要发送的字符 ch 写入到DR寄存器 */
}

/**
 * @brief       传送数据给 ANO_TC匿名科创地面站v4.exe
 * @param       fun  : 功能字. 0XA0~0XAF
 * @param       data : 数据缓存区,最多28字节!!
 * @param       len  : data区有效数据个数
 * @retval      无
 */
void usart1_niming_report(uint8_t fun, uint8_t *data, uint8_t len)
{
    uint8_t send_buf[32];
    uint8_t i;

    if (len > 28)
    {
        return;    /* 最多28字节数据 */
    }

    send_buf[len + 4] = 0;  /* 校验数置零 */
    send_buf[0] = 0XAA;     /* 帧头 */
    send_buf[1] = 0XAA;     /* 帧头 */
    send_buf[2] = fun;      /* 功能字 */
    send_buf[3] = len;      /* 数据长度 */

    for (i = 0; i < len; i++)
    {
        send_buf[4 + i] = data[i];             /* 复制数据 */
    }

    for (i = 0; i < len + 4; i++)
    {
        send_buf[len + 4] += send_buf[i];      /* 计算校验和 */
    }

    for (i = 0; i < len + 5; i++)
    {
        usart1_send_char(send_buf[i]);         /* 发送数据到串口1 */
    }
}

/**
 * @brief       发送加速度传感器数据和陀螺仪数据
 * @param       aacx,aacy,aacz    : x,y,z三个方向上面的加速度值
 * @param       gyrox,gyroy,gyroz : x,y,z三个方向上面的陀螺仪值
 * @retval      无
 */
void qmi8658_send_data(short aacx, short aacy, short aacz, short gyrox, short gyroy, short gyroz)
{
    uint8_t tbuf[18];
    tbuf[0] = (aacx >> 8) & 0XFF;
    tbuf[1] = aacx & 0XFF;
    tbuf[2] = (aacy >> 8) & 0XFF;
    tbuf[3] = aacy & 0XFF;
    tbuf[4] = (aacz >> 8) & 0XFF;
    tbuf[5] = aacz & 0XFF;
    tbuf[6] = (gyrox >> 8) & 0XFF;
    tbuf[7] = gyrox & 0XFF;
    tbuf[8] = (gyroy >> 8) & 0XFF;
    tbuf[9] = gyroy & 0XFF;
    tbuf[10] = (gyroz >> 8) & 0XFF;
    tbuf[11] = gyroz & 0XFF;
    /* 无磁力计数据,所以这里直接屏蔽掉.用0替代. */
    tbuf[12]=0;
    tbuf[13]=0;
    tbuf[14]=0;
    tbuf[15]=0;
    tbuf[16]=0;
    tbuf[17]=0;
    usart1_niming_report(0X02, tbuf, 18);       /* 自定义帧,0X02 */
}

/**
 * @brief       通过串口1上报结算后的姿态数据给电脑
 * @param       roll     : 横滚角.单位0.1度。 -9000 -> 9000 对应 -90.00  ->  90.00度
 * @param       pitch    : 俯仰角.单位 0.1度。-18000 -> 18000 对应 -180.00 -> 180.00 度
 * @param       yaw      : 航向角.单位为0.1度 -18000 -> 18000  对应 -180.00 -> 180.00 度
 * @param       prs      : 气压计高度,单位:cm
 * @param       fly_mode : 飞行模式
 * @param       armed    : 锁定状态
 * @retval      无
 */
void usart1_report_imu(short roll, short pitch, short yaw, int prs, uint8_t fly_mode, uint8_t armed)
{
    uint8_t tbuf[12];

    tbuf[0] = (roll >> 8) & 0XFF;
    tbuf[1] = roll & 0XFF;
    tbuf[2] = (pitch >> 8) & 0XFF;
    tbuf[3] = pitch & 0XFF;
    tbuf[4] = (yaw >> 8) & 0XFF;
    tbuf[5] = yaw & 0XFF;
    tbuf[6] = (prs >> 24) & 0XFF;
    tbuf[7] = (prs >> 16) & 0XFF;
    tbuf[8] = (prs >> 8) & 0XFF;
    tbuf[9] = prs & 0XFF;
    tbuf[10] = fly_mode;
    tbuf[11] = armed;
    usart1_niming_report(0X01, tbuf, 12);   /* 状态帧,0X01 */
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
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

#ifdef  USE_FULL_ASSERT
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
