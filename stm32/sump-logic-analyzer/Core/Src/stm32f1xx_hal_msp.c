/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32f1xx_hal_msp.c
  * @brief        This file provides code for the MSP Initialization
  *               and de-Initialization codes.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"

extern DMA_HandleTypeDef hdma_tim2_up;

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* System interrupt init*/

  /** NOJTAG: JTAG-DP Disabled and SW-DP Enabled
  */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

/**
* @brief TIM_Base MSP Initialization
* @param htim_base: TIM_Base handle pointer
* @retval None
*/
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
  if(htim_base->Instance==TIM2)
  {
    /* Peripheral clock enable */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* TIM2 DMA Init */
    /* TIM2_UP Init: GPIOC->IDR (32-bit read) -> sample ring (8-bit write).
       The DMA keeps only the low byte, i.e. PC0..PC7. */
    hdma_tim2_up.Instance = DMA1_Channel2;
    hdma_tim2_up.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_tim2_up.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_tim2_up.Init.MemInc = DMA_MINC_ENABLE;
    hdma_tim2_up.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_tim2_up.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_tim2_up.Init.Mode = DMA_CIRCULAR;
    hdma_tim2_up.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    if (HAL_DMA_Init(&hdma_tim2_up) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(htim_base,hdma[TIM_DMA_ID_UPDATE],hdma_tim2_up);
  }
  else if(htim_base->Instance==TIM3)
  {
    /* Peripheral clock enable */
    __HAL_RCC_TIM3_CLK_ENABLE();
  }
  else if(htim_base->Instance==TIM4)
  {
    /* Peripheral clock enable */
    __HAL_RCC_TIM4_CLK_ENABLE();
  }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(htim->Instance==TIM3)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM3 GPIO Configuration
    PA6     ------> TIM3_CH1
    PA7     ------> TIM3_CH2
    */
    GPIO_InitStruct.Pin = DEMO_PWM_10K_50_Pin|DEMO_PWM_10K_25_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
  else if(htim->Instance==TIM4)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**TIM4 GPIO Configuration
    PB6     ------> TIM4_CH1
    */
    GPIO_InitStruct.Pin = DEMO_PWM_1K_10_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DEMO_PWM_1K_10_GPIO_Port, &GPIO_InitStruct);
  }
}

/**
* @brief TIM_Base MSP De-Initialization
* @param htim_base: TIM_Base handle pointer
* @retval None
*/
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
  if(htim_base->Instance==TIM2)
  {
    __HAL_RCC_TIM2_CLK_DISABLE();
    HAL_DMA_DeInit(htim_base->hdma[TIM_DMA_ID_UPDATE]);
  }
  else if(htim_base->Instance==TIM3)
  {
    __HAL_RCC_TIM3_CLK_DISABLE();
  }
  else if(htim_base->Instance==TIM4)
  {
    __HAL_RCC_TIM4_CLK_DISABLE();
  }
}

/**
* @brief UART MSP Initialization
* @param huart: UART handle pointer
* @retval None
*/
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(huart->Instance==USART1)
  {
    /* Peripheral clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX (reserved by CubeMX, unused: TX-only demo)
    */
    GPIO_InitStruct.Pin = DEMO_UART_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DEMO_UART_TX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
  else if(huart->Instance==USART2)
  {
    /* Peripheral clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = USART_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(USART_TX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = USART_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(USART_RX_GPIO_Port, &GPIO_InitStruct);

    /* USART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  }
}

/**
* @brief UART MSP De-Initialization
* @param huart: UART handle pointer
* @retval None
*/
void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
  if(huart->Instance==USART1)
  {
    __HAL_RCC_USART1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, DEMO_UART_TX_Pin|GPIO_PIN_10);
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  }
  else if(huart->Instance==USART2)
  {
    __HAL_RCC_USART2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, USART_TX_Pin|USART_RX_Pin);
    HAL_NVIC_DisableIRQ(USART2_IRQn);
  }
}
