/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void HAL_DMA_XferCpltCallback(DMA_HandleTypeDef *hdma);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define Screen_RST_Pin GPIO_PIN_3
#define Screen_RST_GPIO_Port GPIOF
#define Screen_DC_Pin GPIO_PIN_5
#define Screen_DC_GPIO_Port GPIOF
#define Gimbal_UART_TX_Pin GPIO_PIN_2
#define Gimbal_UART_TX_GPIO_Port GPIOA
#define HBridge_PWM_Pin GPIO_PIN_5
#define HBridge_PWM_GPIO_Port GPIOA
#define HBridge_DIR_Pin GPIO_PIN_9
#define HBridge_DIR_GPIO_Port GPIOE
#define SD_SPI_SCK_Pin GPIO_PIN_13
#define SD_SPI_SCK_GPIO_Port GPIOE
#define SD_SPI_MISO_Pin GPIO_PIN_14
#define SD_SPI_MISO_GPIO_Port GPIOE
#define SD_SPI_MOSI_Pin GPIO_PIN_15
#define SD_SPI_MOSI_GPIO_Port GPIOE
#define Gimbal_TIM_v_Pin GPIO_PIN_12
#define Gimbal_TIM_v_GPIO_Port GPIOD
#define Gimbal_TIM_h_Pin GPIO_PIN_13
#define Gimbal_TIM_h_GPIO_Port GPIOD
#define SD_SPI_CS_Pin GPIO_PIN_14
#define SD_SPI_CS_GPIO_Port GPIOD
#define Gimbal_UART_RX_Pin GPIO_PIN_15
#define Gimbal_UART_RX_GPIO_Port GPIOA
#define Screen_SCK_Pin GPIO_PIN_10
#define Screen_SCK_GPIO_Port GPIOC
#define Screen_SDO_Pin GPIO_PIN_11
#define Screen_SDO_GPIO_Port GPIOC
#define Screen_SDI_Pin GPIO_PIN_12
#define Screen_SDI_GPIO_Port GPIOC
#define Screen_TCS_Pin GPIO_PIN_0
#define Screen_TCS_GPIO_Port GPIOD
#define Screen_TCK_Pin GPIO_PIN_1
#define Screen_TCK_GPIO_Port GPIOD
#define Screen_CS_Pin GPIO_PIN_2
#define Screen_CS_GPIO_Port GPIOD
#define Screen_TDO_Pin GPIO_PIN_3
#define Screen_TDO_GPIO_Port GPIOD
#define Screen_TDI_Pin GPIO_PIN_4
#define Screen_TDI_GPIO_Port GPIOD
#define Screen_PEN_Pin GPIO_PIN_7
#define Screen_PEN_GPIO_Port GPIOD
#define Screen_PEN_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
