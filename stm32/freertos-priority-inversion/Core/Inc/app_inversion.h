/**
 * @file    app_inversion.h
 * @brief   Priority inversion with a binary semaphore, and its cure with a mutex.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef APP_INVERSION_H
#define APP_INVERSION_H

#include "cmsis_os.h"

#define CYCLE_MS          100u  /* the scenario repeats every 100 ms */
#define HIGH_OFFSET_MS    2u    /* high wants the lock 2 ms after low took it */
#define MEDIUM_OFFSET_MS  3u    /* medium becomes ready 1 ms later */

void app_init(void);
void app_register_tasks(osThreadId_t console, osThreadId_t high, osThreadId_t medium, osThreadId_t low);

void app_console_task(void *argument);
void app_high_task(void *argument);
void app_medium_task(void *argument);
void app_low_task(void *argument);

#endif /* APP_INVERSION_H */
