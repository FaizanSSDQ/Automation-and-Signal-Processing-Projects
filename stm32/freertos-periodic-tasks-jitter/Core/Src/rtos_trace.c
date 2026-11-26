/**
 * @file    rtos_trace.c
 * @brief   Task-switch trace on GPIO pins.
 *
 * FreeRTOS calls traceTASK_SWITCHED_IN() (see FreeRTOSConfig.h) inside the
 * context switch. Each traced task carries its own GPIO mask as application
 * tag, so one write to the port's set/reset register raises the incoming
 * task's pin and lowers all the others. Tasks without a tag (idle, timer
 * service) leave every pin low. A logic analyzer on PB12..PB15 therefore
 * shows exactly which task owns the CPU, with sub-microsecond resolution.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdint.h>

#include "main.h"

void rtos_trace_switched_in(uint32_t tag)
{
    const uint32_t on = tag & TRACE_ALL_PINS;
    GPIOB->BSRR = on | ((uint32_t)(TRACE_ALL_PINS & ~on) << 16);
}
