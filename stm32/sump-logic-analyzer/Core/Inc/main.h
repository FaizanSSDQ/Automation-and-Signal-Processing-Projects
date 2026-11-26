/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LA_CH0_Pin GPIO_PIN_0
#define LA_CH0_GPIO_Port GPIOC
#define LA_CH1_Pin GPIO_PIN_1
#define LA_CH1_GPIO_Port GPIOC
#define LA_CH2_Pin GPIO_PIN_2
#define LA_CH2_GPIO_Port GPIOC
#define LA_CH3_Pin GPIO_PIN_3
#define LA_CH3_GPIO_Port GPIOC
#define LA_CH4_Pin GPIO_PIN_4
#define LA_CH4_GPIO_Port GPIOC
#define LA_CH5_Pin GPIO_PIN_5
#define LA_CH5_GPIO_Port GPIOC
#define LA_CH6_Pin GPIO_PIN_6
#define LA_CH6_GPIO_Port GPIOC
#define LA_CH7_Pin GPIO_PIN_7
#define LA_CH7_GPIO_Port GPIOC
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define DEMO_PWM_10K_50_Pin GPIO_PIN_6
#define DEMO_PWM_10K_50_GPIO_Port GPIOA
#define DEMO_PWM_10K_25_Pin GPIO_PIN_7
#define DEMO_PWM_10K_25_GPIO_Port GPIOA
#define DEMO_UART_TX_Pin GPIO_PIN_9
#define DEMO_UART_TX_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define DEMO_PWM_1K_10_Pin GPIO_PIN_6
#define DEMO_PWM_1K_10_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
