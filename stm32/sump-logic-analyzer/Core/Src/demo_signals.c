/**
 * @file    demo_signals.c
 * @brief   On-board test signals for trying the analyzer with jumper wires only.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "demo_signals.h"

#include <stdint.h>

#include "main.h"

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart1;

static uint8_t message[] = "LA demo 0000\r\n";
#define COUNTER_POS 8u /* first digit of the counter in message[] */

static uint32_t next_ms;
static uint16_t counter;

void demo_signals_start(void)
{
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    next_ms = HAL_GetTick();
}

void demo_signals_poll(void)
{
    if ((int32_t)(HAL_GetTick() - next_ms) < 0 || huart1.gState != HAL_UART_STATE_READY) {
        return;
    }
    next_ms += DEMO_UART_PERIOD_MS;

    uint16_t v = counter;
    counter = (uint16_t)((counter + 1u) % 10000u);
    for (int i = 3; i >= 0; i--) {
        message[COUNTER_POS + (unsigned)i] = (uint8_t)('0' + v % 10u);
        v /= 10u;
    }
    (void)HAL_UART_Transmit_IT(&huart1, message, sizeof(message) - 1u);
}
