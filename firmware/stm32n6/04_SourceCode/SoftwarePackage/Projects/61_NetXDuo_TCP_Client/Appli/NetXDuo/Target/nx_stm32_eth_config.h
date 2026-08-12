/**
 ****************************************************************************************************
 * @file        nx_stm32_eth_config.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       nx_stm32_eth_config.h文件
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#ifndef __NX_STM32_ETH_CONFIG_H
#define __NX_STM32_ETH_CONFIG_H

#include "stm32n6xx_hal.h"

#define MULTI_QUEUE_FEATURE

extern ETH_HandleTypeDef heth1;
#ifdef NX_DRIVER_ETH_HW_IP_INIT
extern void MX_ETH1_Init(void);
#endif

#define eth_handle heth1
#ifdef NX_DRIVER_ETH_HW_IP_INIT
#define nx_eth_init MX_ETH1_Init
#endif

#endif
