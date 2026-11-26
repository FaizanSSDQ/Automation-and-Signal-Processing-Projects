/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : FreeRTOS priority inversion: binary semaphore vs mutex
  ******************************************************************************
  *   console  (high)          serial shell, blocking-time reports
  *   high     (above normal)  needs the shared lock 2 ms into every 100 ms cycle
  *   medium   (normal)        CPU-bound for `medium` ms, needs no lock
  *   low      (below normal)  holds the shared lock for `hold` ms each cycle
  * The lock is a binary semaphore (priority inversion) or a mutex (priority
  * inheritance), switchable at run time. Every context switch drives a trace
  * pin (PB12..PB15) for a logic analyzer.
  *
  * Peripheral and RTOS objects below follow STM32CubeMX (RTOS_Priority_Inversion.ioc);
  * application code lives in app_inversion.c and USER CODE blocks.
  *
  * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_inversion.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* Definitions for ConsoleTask */
osThreadId_t ConsoleTaskHandle;
const osThreadAttr_t ConsoleTask_attributes = {
  .name = "console",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for HighTask */
osThreadId_t HighTaskHandle;
const osThreadAttr_t HighTask_attributes = {
  .name = "high",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for MediumTask */
osThreadId_t MediumTaskHandle;
const osThreadAttr_t MediumTask_attributes = {
  .name = "medium",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LowTask */
osThreadId_t LowTaskHandle;
const osThreadAttr_t LowTask_attributes = {
  .name = "low",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
void ConsoleTask_Entry(void *argument);
void HighTask_Entry(void *argument);
void MediumTask_Entry(void *argument);
void LowTask_Entry(void *argument);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_QUEUES */
  app_init();
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of ConsoleTask */
  ConsoleTaskHandle = osThreadNew(ConsoleTask_Entry, NULL, &ConsoleTask_attributes);

  /* creation of HighTask */
  HighTaskHandle = osThreadNew(HighTask_Entry, NULL, &HighTask_attributes);

  /* creation of MediumTask */
  MediumTaskHandle = osThreadNew(MediumTask_Entry, NULL, &MediumTask_attributes);

  /* creation of LowTask */
  LowTaskHandle = osThreadNew(LowTask_Entry, NULL, &LowTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  app_register_tasks(ConsoleTaskHandle, HighTaskHandle, MediumTaskHandle, LowTaskHandle);
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  *        HSI 8 MHz / 2 x 16 = 64 MHz SYSCLK; APB1 32 MHz, APB2 64 MHz.
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function (console via ST-LINK virtual COM port)
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED2_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TRACE_CONSOLE_Pin|TRACE_HIGH_Pin|TRACE_MEDIUM_Pin|TRACE_LOW_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin (pressed = low) */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED2_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LED2_Pin|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : TRACE_CONSOLE_Pin TRACE_HIGH_Pin TRACE_MEDIUM_Pin TRACE_LOW_Pin */
  GPIO_InitStruct.Pin = TRACE_CONSOLE_Pin|TRACE_HIGH_Pin|TRACE_MEDIUM_Pin|TRACE_LOW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init (priority 6: may call FreeRTOS ISR API) */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN Header_ConsoleTask_Entry */
/**
  * @brief  Function implementing the ConsoleTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_ConsoleTask_Entry */
void ConsoleTask_Entry(void *argument)
{
  /* USER CODE BEGIN ConsoleTask_Entry */
  app_console_task(argument);
  /* USER CODE END ConsoleTask_Entry */
}

/* USER CODE BEGIN Header_HighTask_Entry */
/**
* @brief Function implementing the HighTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_HighTask_Entry */
void HighTask_Entry(void *argument)
{
  /* USER CODE BEGIN HighTask_Entry */
  app_high_task(argument);
  /* USER CODE END HighTask_Entry */
}

/* USER CODE BEGIN Header_MediumTask_Entry */
/**
* @brief Function implementing the MediumTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_MediumTask_Entry */
void MediumTask_Entry(void *argument)
{
  /* USER CODE BEGIN MediumTask_Entry */
  app_medium_task(argument);
  /* USER CODE END MediumTask_Entry */
}

/* USER CODE BEGIN Header_LowTask_Entry */
/**
* @brief Function implementing the LowTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LowTask_Entry */
void LowTask_Entry(void *argument)
{
  /* USER CODE BEGIN LowTask_Entry */
  app_low_task(argument);
  /* USER CODE END LowTask_Entry */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM4) {
    HAL_IncTick();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
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
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
