/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : FreeRTOS scheduling demo on NUCLEO-F103RB
  ******************************************************************************
  * Four application tasks share the CPU under the FreeRTOS fixed-priority
  * preemptive scheduler:
  *   console    (high)    serial shell, run-time statistics ("top")
  *   worker     (below)   adjustable CPU load, 100 ms period
  *   blink_fast (low)     LD2, 100 ms
  *   blink_slow (low)     external LED on PA4, 500 ms
  * Priorities, load and suspension are changed live from the terminal; every
  * context switch drives a trace pin (PB12..PB15) for a logic analyzer.
  *
  * Peripheral and RTOS objects below follow STM32CubeMX (RTOS_Scheduling.ioc);
  * application code lives in app_sched.c and USER CODE blocks.
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
#include "app_sched.h"
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
/* Definitions for WorkerTask */
osThreadId_t WorkerTaskHandle;
const osThreadAttr_t WorkerTask_attributes = {
  .name = "worker",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for BlinkFastTask */
osThreadId_t BlinkFastTaskHandle;
const osThreadAttr_t BlinkFastTask_attributes = {
  .name = "blink_fast",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for BlinkSlowTask */
osThreadId_t BlinkSlowTaskHandle;
const osThreadAttr_t BlinkSlowTask_attributes = {
  .name = "blink_slow",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
void ConsoleTask_Entry(void *argument);
void WorkerTask_Entry(void *argument);
void BlinkFastTask_Entry(void *argument);
void BlinkSlowTask_Entry(void *argument);

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

  /* creation of WorkerTask */
  WorkerTaskHandle = osThreadNew(WorkerTask_Entry, NULL, &WorkerTask_attributes);

  /* creation of BlinkFastTask */
  BlinkFastTaskHandle = osThreadNew(BlinkFastTask_Entry, NULL, &BlinkFastTask_attributes);

  /* creation of BlinkSlowTask */
  BlinkSlowTaskHandle = osThreadNew(BlinkSlowTask_Entry, NULL, &BlinkSlowTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  app_register_tasks(ConsoleTaskHandle, WorkerTaskHandle, BlinkFastTaskHandle, BlinkSlowTaskHandle);
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
  HAL_GPIO_WritePin(GPIOB, TRACE_CONSOLE_Pin|TRACE_WORKER_Pin|TRACE_BLINK_FAST_Pin|TRACE_BLINK_SLOW_Pin, GPIO_PIN_RESET);

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

  /*Configure GPIO pins : TRACE_CONSOLE_Pin TRACE_WORKER_Pin TRACE_BLINK_FAST_Pin TRACE_BLINK_SLOW_Pin */
  GPIO_InitStruct.Pin = TRACE_CONSOLE_Pin|TRACE_WORKER_Pin|TRACE_BLINK_FAST_Pin|TRACE_BLINK_SLOW_Pin;
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

/* USER CODE BEGIN Header_WorkerTask_Entry */
/**
* @brief Function implementing the WorkerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_WorkerTask_Entry */
void WorkerTask_Entry(void *argument)
{
  /* USER CODE BEGIN WorkerTask_Entry */
  app_worker_task(argument);
  /* USER CODE END WorkerTask_Entry */
}

/* USER CODE BEGIN Header_BlinkFastTask_Entry */
/**
* @brief Function implementing the BlinkFastTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_BlinkFastTask_Entry */
void BlinkFastTask_Entry(void *argument)
{
  /* USER CODE BEGIN BlinkFastTask_Entry */
  app_blink_task(argument, LD2_GPIO_Port, LD2_Pin, 100u);
  /* USER CODE END BlinkFastTask_Entry */
}

/* USER CODE BEGIN Header_BlinkSlowTask_Entry */
/**
* @brief Function implementing the BlinkSlowTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_BlinkSlowTask_Entry */
void BlinkSlowTask_Entry(void *argument)
{
  /* USER CODE BEGIN BlinkSlowTask_Entry */
  app_blink_task(argument, LED2_GPIO_Port, LED2_Pin, 500u);
  /* USER CODE END BlinkSlowTask_Entry */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM3 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3) {
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
