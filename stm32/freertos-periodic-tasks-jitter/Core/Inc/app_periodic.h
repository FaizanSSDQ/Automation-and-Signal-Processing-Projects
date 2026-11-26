/**
 * @file    app_periodic.h
 * @brief   Periodic-task demo: vTaskDelay versus vTaskDelayUntil under interference.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef APP_PERIODIC_H
#define APP_PERIODIC_H

#include "cmsis_os.h"

#define PERIOD_MS        10u   /* nominal period of both periodic tasks */
#define INTERFERENCE_MS  37u   /* period of the interference task (not a multiple of 10) */

void app_init(void);
void app_register_tasks(osThreadId_t console, osThreadId_t relative, osThreadId_t absolute,
                        osThreadId_t interference);

void app_console_task(void *argument);
void app_relative_task(void *argument);
void app_absolute_task(void *argument);
void app_interference_task(void *argument);

#endif /* APP_PERIODIC_H */
