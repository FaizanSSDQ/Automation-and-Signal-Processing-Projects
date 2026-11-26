/**
 * @file    app_sched.c
 * @brief   Scheduling demo application: console, worker and blink tasks.
 *
 * console    High priority. Owns the UART: bytes arrive through a message
 *            queue filled by the RX interrupt, text leaves through
 *            interrupt-driven transmission while the task sleeps on a thread
 *            flag. Every second it samples the FreeRTOS run-time counters.
 * worker     Burns a configurable share of each 100 ms period in a busy loop,
 *            then waits with vTaskDelayUntil (fixed rate, no drift).
 * blink_*    Toggle an LED and sleep: tiny CPU users that make starvation
 *            visible when a higher-priority task never blocks.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "app_sched.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "cmd.h"
#include "sched_stats.h"
#include "task.h"

extern UART_HandleTypeDef huart2;

#define RX_QUEUE_LEN      32u
#define BUTTON_EVENT      0x01u     /* pseudo character queued by the B1 interrupt */
#define FLAG_TX_DONE      0x0001u
#define WINDOW_MS         1000u
#define WORKER_PERIOD_MS  100u
#define LINE_MAX          48u

/* Tasks the console may address (the console itself is excluded). */
enum { T_WORKER, T_BLINK_FAST, T_BLINK_SLOW, T_COUNT };
static const char *const task_names[T_COUNT] = {"worker", "blink_fast", "blink_slow"};
static osThreadId_t task_handles[T_COUNT];
static osThreadId_t console_handle;

static osMessageQueueId_t rx_queue;
static uint8_t rx_byte;
static volatile uint32_t worker_load = 30u;      /* percent */

static const char help_text[] =
    "commands:\r\n"
    "  top                    task table for the last second\r\n"
    "  watch on|off           print the table every second\r\n"
    "  load <0..100>          worker CPU load in percent\r\n"
    "  prio <task> <level>    level low|below|normal|above; tasks worker, blink_fast, blink_slow\r\n"
    "  suspend <task> | resume <task>\r\n"
    "  B1 button              starvation demo on/off (worker 100 % at 'above')\r\n";

/* ---- console I/O ------------------------------------------------------------------------ */

static void con_write(const char *s, size_t len)
{
    if (len == 0u) {
        return;
    }
    if (HAL_UART_Transmit_IT(&huart2, (uint8_t *)s, (uint16_t)len) == HAL_OK) {
        (void)osThreadFlagsWait(FLAG_TX_DONE, osFlagsWaitAny, osWaitForever);
    }
}

static void con_print(const char *s)
{
    con_write(s, strlen(s));
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        (void)osThreadFlagsSet(console_handle, FLAG_TX_DONE);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        (void)osMessageQueuePut(rx_queue, &rx_byte, 0u, 0u); /* drop if full */
        (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1u);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1u);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == B1_Pin) {
        const uint8_t ev = BUTTON_EVENT;
        (void)osMessageQueuePut(rx_queue, &ev, 0u, 0u);
    }
}

/* ---- statistics ------------------------------------------------------------------------- */

static ss_sample_t samples[2][SS_MAX_TASKS];
static size_t n_samples[2];
static int cur_set;
static ss_row_t rows[SS_MAX_TASKS];
static size_t n_rows;
static uint32_t window_ms;

static char state_char(eTaskState s)
{
    switch (s) {
    case eRunning:   return 'R';
    case eReady:     return 'r';
    case eBlocked:   return 'B';
    case eSuspended: return 'S';
    default:         return 'D';
    }
}

static void take_snapshot(uint32_t elapsed_ms)
{
    static TaskStatus_t status[SS_MAX_TASKS];
    const int next = cur_set ^ 1;
    const UBaseType_t n = uxTaskGetSystemState(status, SS_MAX_TASKS, NULL);
    for (UBaseType_t i = 0; i < n; i++) {
        ss_sample_t *s = &samples[next][i];
        s->id = status[i].xTaskNumber;
        strncpy(s->name, status[i].pcTaskName, SS_NAME_LEN - 1u);
        s->name[SS_NAME_LEN - 1u] = '\0';
        s->prio = (uint8_t)status[i].uxCurrentPriority;
        s->state = state_char(status[i].eCurrentState);
        s->runtime = status[i].ulRunTimeCounter;
        s->stack_free = (uint16_t)status[i].usStackHighWaterMark;
    }
    n_samples[next] = n;
    n_rows = ss_compute(samples[cur_set], n_samples[cur_set], samples[next], n, rows);
    window_ms = elapsed_ms;
    cur_set = next;
}

static void print_top(void)
{
    static char table[1024];
    const size_t len = ss_format(rows, n_rows, window_ms, (uint32_t)xPortGetFreeHeapSize(),
                                 (uint32_t)xPortGetMinimumEverFreeHeapSize(), table, sizeof(table));
    con_write(table, len);
}

/* ---- commands --------------------------------------------------------------------------- */

static void execute(char *line)
{
    char reply[96];
    const cmd_t c = cmd_parse(line, task_names, T_COUNT);
    reply[0] = '\0';

    switch (c.type) {
    case CMD_EMPTY:
        return;
    case CMD_HELP:
        con_print(help_text);
        return;
    case CMD_TOP:
        print_top();
        return;
    case CMD_WATCH:
        return; /* handled by the caller */
    case CMD_LOAD:
        worker_load = (uint32_t)c.value;
        snprintf(reply, sizeof(reply), "worker load %d %%\r\n", c.value);
        break;
    case CMD_PRIO:
        (void)osThreadSetPriority(task_handles[c.task], (osPriority_t)c.value);
        snprintf(reply, sizeof(reply), "%s priority %s\r\n", task_names[c.task], cmd_prio_name(c.value));
        break;
    case CMD_SUSPEND:
        (void)osThreadSuspend(task_handles[c.task]);
        snprintf(reply, sizeof(reply), "%s suspended\r\n", task_names[c.task]);
        break;
    case CMD_RESUME:
        (void)osThreadResume(task_handles[c.task]);
        snprintf(reply, sizeof(reply), "%s resumed\r\n", task_names[c.task]);
        break;
    case CMD_ERROR:
    default:
        snprintf(reply, sizeof(reply), "error: %s\r\n", c.error);
        break;
    }
    con_print(reply);
}

static void starvation_toggle(void)
{
    static bool starving;
    starving = !starving;
    if (starving) {
        worker_load = 100u;
        (void)osThreadSetPriority(task_handles[T_WORKER], osPriorityAboveNormal);
        con_print("\r\n[B1] starvation: worker 100 % at 'above' - the LEDs stop\r\n> ");
    } else {
        worker_load = 30u;
        (void)osThreadSetPriority(task_handles[T_WORKER], osPriorityBelowNormal);
        con_print("\r\n[B1] normal: worker 30 % at 'below' - the LEDs blink again\r\n> ");
    }
}

/* ---- tasks ------------------------------------------------------------------------------ */

void app_init(void)
{
    rx_queue = osMessageQueueNew(RX_QUEUE_LEN, sizeof(uint8_t), NULL);
}

void app_register_tasks(osThreadId_t console, osThreadId_t worker, osThreadId_t blink_fast,
                        osThreadId_t blink_slow)
{
    console_handle = console;
    task_handles[T_WORKER] = worker;
    task_handles[T_BLINK_FAST] = blink_fast;
    task_handles[T_BLINK_SLOW] = blink_slow;
    vTaskSetApplicationTaskTag((TaskHandle_t)console, (TaskHookFunction_t)(uintptr_t)TRACE_CONSOLE_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)worker, (TaskHookFunction_t)(uintptr_t)TRACE_WORKER_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)blink_fast, (TaskHookFunction_t)(uintptr_t)TRACE_BLINK_FAST_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)blink_slow, (TaskHookFunction_t)(uintptr_t)TRACE_BLINK_SLOW_Pin);
}

void app_console_task(void *argument)
{
    (void)argument;
    char line[LINE_MAX];
    size_t len = 0;
    bool watch = false;
    TickType_t last = xTaskGetTickCount();

    take_snapshot(0u);
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1u);
    con_print("\r\nFreeRTOS scheduling demo (" tskKERNEL_VERSION_NUMBER ") - type 'help'\r\n> ");

    for (;;) {
        const TickType_t since = xTaskGetTickCount() - last;
        const uint32_t wait = (since >= WINDOW_MS) ? 0u : WINDOW_MS - since;
        uint8_t c;

        if (osMessageQueueGet(rx_queue, &c, NULL, wait) == osOK) {
            if (c == BUTTON_EVENT) {
                starvation_toggle();
            } else if (c == '\r' || c == '\n') {
                if (c == '\n' && len == 0u) {
                    continue; /* LF of a CR LF pair */
                }
                con_print("\r\n");
                line[len] = '\0';
                char copy[LINE_MAX];
                memcpy(copy, line, len + 1u);
                const cmd_t probe = cmd_parse(copy, task_names, T_COUNT);
                if (probe.type == CMD_WATCH) {
                    watch = probe.value != 0;
                    con_print(watch ? "watch on\r\n" : "watch off\r\n");
                } else {
                    execute(line);
                }
                len = 0u;
                con_print("> ");
            } else if (c == '\b' || c == 0x7Fu) {
                if (len > 0u) {
                    len--;
                    con_print("\b \b");
                }
            } else if (len < LINE_MAX - 1u && c >= ' ') {
                line[len++] = (char)c;
                con_write((const char *)&c, 1u);
            }
        }

        const TickType_t now = xTaskGetTickCount();
        if (now - last >= WINDOW_MS) {
            take_snapshot((uint32_t)(now - last));
            last = now;
            if (watch) {
                con_print("\r\n");
                print_top();
                con_print("> ");
            }
        }
    }
}

void app_worker_task(void *argument)
{
    (void)argument;
    const uint32_t cycles_per_ms = SystemCoreClock / 1000u;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        /* Busy for load % of the period (wall-clock time, so preemption by
         * higher-priority tasks is included), then sleep until the next period. */
        const uint32_t start = DWT->CYCCNT;
        const uint32_t busy = worker_load * WORKER_PERIOD_MS / 100u * cycles_per_ms;
        while (DWT->CYCCNT - start < busy) {
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(WORKER_PERIOD_MS));
    }
}

void app_blink_task(void *argument, GPIO_TypeDef *port, uint16_t pin, uint32_t half_period_ms)
{
    (void)argument;
    for (;;) {
        HAL_GPIO_TogglePin(port, pin);
        osDelay(half_period_ms);
    }
}
