/**
 * @file    app_sched.h
 * @brief   Scheduling demo application: console, worker and blink tasks.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef APP_SCHED_H
#define APP_SCHED_H

#include "cmsis_os.h"
#include "main.h"

/** Create RTOS objects used by the tasks (before the threads are created). */
void app_init(void);

/** Attach trace pins and names to the created threads. */
void app_register_tasks(osThreadId_t console, osThreadId_t worker, osThreadId_t blink_fast,
                        osThreadId_t blink_slow);

void app_console_task(void *argument);
void app_worker_task(void *argument);
void app_blink_task(void *argument, GPIO_TypeDef *port, uint16_t pin, uint32_t half_period_ms);

#endif /* APP_SCHED_H */
