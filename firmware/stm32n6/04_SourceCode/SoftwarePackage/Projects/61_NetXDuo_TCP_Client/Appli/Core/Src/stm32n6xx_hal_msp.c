/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32n6xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External functions --------------------------------------------------------*/
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{

  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  /* System interrupt init*/

  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO2,PWR_VDDIO_RANGE_1V8);

  HAL_PWREx_EnableVddIO4();
  HAL_PWREx_ConfigVddIORange(PWR_VDDIO4,PWR_VDDIO_RANGE_3V3);

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

/**
* @brief ETH MSP Initialization
* This function configures the hardware resources used in this example
* @param heth: ETH handle pointer
* @retval None
*/
void HAL_ETH_MspInit(ETH_HandleTypeDef* heth)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(heth->Instance==ETH1)
  {
  /* USER CODE BEGIN ETH1_MspInit 0 */

  /* USER CODE END ETH1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ETH1;
    PeriphClkInitStruct.Eth1ClockSelection = RCC_ETH1CLKSOURCE_HCLK;

  /* USER CODE BEGIN MACADDRESS */

  /* USER CODE END MACADDRESS */

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* Peripheral clock enable */
    __HAL_RCC_ETH1_CLK_ENABLE();

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    /**ETH1 GPIO Configuration
    PH5     ------> ETH1_MDC
    PF4     ------> ETH1_MDIO
    PF10     ------> ETH1_RMII_CRS_DV
    PF7     ------> ETH1_RMII_REF_CLK
    PF5     ------> ETH1_CLK
    PF15     ------> ETH1_RMII_RXD1
    PF14     ------> ETH1_RMII_RXD0
    PF11     ------> ETH1_RMII_TX_EN
    PF13     ------> ETH1_RMII_TXD1
    PF12     ------> ETH1_RMII_TXD0
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH1;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_10|GPIO_PIN_7|GPIO_PIN_5
                          |GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_11|GPIO_PIN_13
                          |GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH1;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* ETH1 interrupt Init */
    HAL_NVIC_SetPriority(ETH1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(ETH1_IRQn);
  /* USER CODE BEGIN ETH1_MspInit 1 */
    /* Peripheral clock enable */
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /**ETH1 GPIO Configuration
    PF5     ------> YT8512C_RESET
    */
    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_5);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_5, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /* USER CODE END ETH1_MspInit 1 */

  }

}

/**
* @brief ETH MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param heth: ETH handle pointer
* @retval None
*/
void HAL_ETH_MspDeInit(ETH_HandleTypeDef* heth)
{
  if(heth->Instance==ETH1)
  {
  /* USER CODE BEGIN ETH1_MspDeInit 0 */

  /* USER CODE END ETH1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ETH1_CLK_DISABLE();

    /**ETH1 GPIO Configuration
    PH5     ------> ETH1_MDC
    PF4     ------> ETH1_MDIO
    PF10     ------> ETH1_RMII_CRS_DV
    PF7     ------> ETH1_RMII_REF_CLK
    PF5     ------> ETH1_CLK
    PF15     ------> ETH1_RMII_RXD1
    PF14     ------> ETH1_RMII_RXD0
    PF11     ------> ETH1_RMII_TX_EN
    PF13     ------> ETH1_RMII_TXD1
    PF12     ------> ETH1_RMII_TXD0
    */
    HAL_GPIO_DeInit(GPIOH, GPIO_PIN_5);

    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_4|GPIO_PIN_10|GPIO_PIN_7|GPIO_PIN_5
                          |GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_11|GPIO_PIN_13
                          |GPIO_PIN_12);

    /* ETH1 interrupt DeInit */
    HAL_NVIC_DisableIRQ(ETH1_IRQn);
  /* USER CODE BEGIN ETH1_MspDeInit 1 */
    /* Peripheral clock disable */
    __HAL_RCC_ETH1MAC_CLK_DISABLE();
    __HAL_RCC_ETH1TX_CLK_DISABLE();
    __HAL_RCC_ETH1RX_CLK_DISABLE();

    /**ETH1 GPIO Configuration
    PF5     ------> YT8512C_RESET
    */
    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_5);

  /* USER CODE END ETH1_MspDeInit 1 */
  }

}

/**
* @brief UART MSP Initialization
* This function configures the hardware resources used in this example
* @param huart: UART handle pointer
* @retval None
*/
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(huart->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_CLKP;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOE_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PE5     ------> USART1_TX
    PE6     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */

  }

}

/**
* @brief UART MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param huart: UART handle pointer
* @retval None
*/
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
  if(huart->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PE5     ------> USART1_TX
    PE6     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_5|GPIO_PIN_6);

    /* USART1 interrupt DeInit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }

}

/**
* @brief XSPI MSP Initialization
* This function configures the hardware resources used in this example
* @param hxspi: XSPI handle pointer
* @retval None
*/
void HAL_XSPI_MspInit(XSPI_HandleTypeDef* hxspi)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(hxspi->Instance==XSPI1)
  {
  /* USER CODE BEGIN XSPI1_MspInit 0 */

  /* USER CODE END XSPI1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_XSPI1;
    PeriphClkInitStruct.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_IC4;
    PeriphClkInitStruct.ICSelection[RCC_IC4].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    PeriphClkInitStruct.ICSelection[RCC_IC4].ClockDivider = 6;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* Peripheral clock enable */
    __HAL_RCC_XSPIM_CLK_ENABLE();
    __HAL_RCC_XSPI1_CLK_ENABLE();

    __HAL_RCC_GPIOP_CLK_ENABLE();
    __HAL_RCC_GPIOO_CLK_ENABLE();
    /**XSPI1 GPIO Configuration
    PP7     ------> XSPIM_P1_IO7
    PP6     ------> XSPIM_P1_IO6
    PP0     ------> XSPIM_P1_IO0
    PP4     ------> XSPIM_P1_IO4
    PP1     ------> XSPIM_P1_IO1
    PP5     ------> XSPIM_P1_IO5
    PP3     ------> XSPIM_P1_IO3
    PP2     ------> XSPIM_P1_IO2
    PO5     ------> XSPIM_P1_NCLK
    PO2     ------> XSPIM_P1_DQS0
    PO0     ------> XSPIM_P1_NCS1
    PO4     ------> XSPIM_P1_CLK
    */
    GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_0|GPIO_PIN_4
                          |GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_3|GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_XSPIM_P1;
    HAL_GPIO_Init(GPIOP, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_2|GPIO_PIN_0|GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_XSPIM_P1;
    HAL_GPIO_Init(GPIOO, &GPIO_InitStruct);

  /* USER CODE BEGIN XSPI1_MspInit 1 */

  /* USER CODE END XSPI1_MspInit 1 */

  }

}

/**
* @brief XSPI MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param hxspi: XSPI handle pointer
* @retval None
*/
void HAL_XSPI_MspDeInit(XSPI_HandleTypeDef* hxspi)
{
  if(hxspi->Instance==XSPI1)
  {
  /* USER CODE BEGIN XSPI1_MspDeInit 0 */

  /* USER CODE END XSPI1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_XSPIM_CLK_DISABLE();
    __HAL_RCC_XSPI1_CLK_DISABLE();

    /**XSPI1 GPIO Configuration
    PP7     ------> XSPIM_P1_IO7
    PP6     ------> XSPIM_P1_IO6
    PP0     ------> XSPIM_P1_IO0
    PP4     ------> XSPIM_P1_IO4
    PP1     ------> XSPIM_P1_IO1
    PP5     ------> XSPIM_P1_IO5
    PP3     ------> XSPIM_P1_IO3
    PP2     ------> XSPIM_P1_IO2
    PO5     ------> XSPIM_P1_NCLK
    PO2     ------> XSPIM_P1_DQS0
    PO0     ------> XSPIM_P1_NCS1
    PO4     ------> XSPIM_P1_CLK
    */
    HAL_GPIO_DeInit(GPIOP, GPIO_PIN_7|GPIO_PIN_6|GPIO_PIN_0|GPIO_PIN_4
                          |GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_3|GPIO_PIN_2);

    HAL_GPIO_DeInit(GPIOO, GPIO_PIN_5|GPIO_PIN_2|GPIO_PIN_0|GPIO_PIN_4);

  /* USER CODE BEGIN XSPI1_MspDeInit 1 */

  /* USER CODE END XSPI1_MspDeInit 1 */
  }

}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
