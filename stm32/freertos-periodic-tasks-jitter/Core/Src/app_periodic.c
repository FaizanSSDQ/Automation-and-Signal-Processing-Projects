/**
 * @file    app_periodic.c
 * @brief   Periodic-task demo: vTaskDelay versus vTaskDelayUntil under interference.
 *
 * Two tasks at the same priority both try to run every 10 ms and do the same
 * amount of work per cycle. They differ only in how they wait:
 *
 *   relative   work, then vTaskDelay(10):          period = work + 10 ms + delays
 *   absolute   work, then vTaskDelayUntil(&t, 10): period = 10 ms on average
 *
 * A higher-priority interference task steals a burst of CPU time every 37 ms,
 * which delays the releases of both (jitter). Every release is stamped with
 * the DWT cycle counter and fed to period_stats; the console prints the
 * resulting statistics and histograms.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "app_periodic.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "cmd.h"
#include "main.h"
#include "period_stats.h"
#include "task.h"

extern UART_HandleTypeDef huart2;

#define RX_QUEUE_LEN     32u
#define BUTTON_EVENT     0x01u
#define FLAG_TX_DONE     0x0001u
#define WATCH_MS         2000u
#define LINE_MAX         32u

static osThreadId_t console_handle;
static osMessageQueueId_t rx_queue;
static uint8_t rx_byte;

static volatile uint32_t work_ms = 2u;
static volatile uint32_t burst_ms = 3u;
static period_stats_t stats_relative, stats_absolute;

static const char help_text[] =
    "commands:\r\n"
    "  report          statistics of both periodic tasks\r\n"
    "  watch on|off    print the report every 2 s\r\n"
    "  work <0..8>     busy ms per cycle of each periodic task\r\n"
    "  burst <0..9>    busy ms of the interference task every 37 ms\r\n"
    "  reset           clear the statistics\r\n"
    "  B1 button       interference on/off\r\n";

/* ---- helpers ---------------------------------------------------------------------------- */

static void busy_ms(uint32_t ms)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t cycles = ms * (SystemCoreClock / 1000u);
    while (DWT->CYCCNT - start < cycles) {
    }
}

static void stats_reset(void)
{
    const uint32_t cpu = SystemCoreClock / 1000000u;
    taskENTER_CRITICAL();
    ps_init(&stats_relative, PERIOD_MS * 1000u, cpu);
    ps_init(&stats_absolute, PERIOD_MS * 1000u, cpu);
    taskEXIT_CRITICAL();
}

static void con_write(const char *s, size_t len)
{
    if (len && HAL_UART_Transmit_IT(&huart2, (uint8_t *)s, (uint16_t)len) == HAL_OK) {
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
        (void)osMessageQueuePut(rx_queue, &rx_byte, 0u, 0u);
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

static void print_report(void)
{
    static char out[1600];
    period_stats_t rel, abs_;
    taskENTER_CRITICAL();  /* consistent copies; the tasks update them every 10 ms */
    rel = stats_relative;
    abs_ = stats_absolute;
    taskEXIT_CRITICAL();

    size_t len = (size_t)snprintf(out, sizeof(out), "work %lu ms, interference %lu ms every %lu ms\r\n",
                                  (unsigned long)work_ms, (unsigned long)burst_ms,
                                  (unsigned long)INTERFERENCE_MS);
    len += ps_format(&rel, "relative (vTaskDelay)", out + len, sizeof(out) - len);
    len += ps_format(&abs_, "absolute (vTaskDelayUntil)", out + len, sizeof(out) - len);
    con_write(out, len);
}

static void execute(char *line, bool *watch)
{
    char reply[64];
    const cmd_t c = cmd_parse(line);
    reply[0] = '\0';
    switch (c.type) {
    case CMD_EMPTY:
        return;
    case CMD_HELP:
        con_print(help_text);
        return;
    case CMD_REPORT:
        print_report();
        return;
    case CMD_WATCH:
        *watch = c.value != 0;
        snprintf(reply, sizeof(reply), "watch %s\r\n", *watch ? "on" : "off");
        break;
    case CMD_WORK:
        work_ms = (uint32_t)c.value;
        stats_reset();
        snprintf(reply, sizeof(reply), "work %d ms (statistics reset)\r\n", c.value);
        break;
    case CMD_BURST:
        burst_ms = (uint32_t)c.value;
        stats_reset();
        snprintf(reply, sizeof(reply), "burst %d ms (statistics reset)\r\n", c.value);
        break;
    case CMD_RESET:
        stats_reset();
        snprintf(reply, sizeof(reply), "statistics reset\r\n");
        break;
    case CMD_ERROR:
    default:
        snprintf(reply, sizeof(reply), "error: %s\r\n", c.error);
        break;
    }
    con_print(reply);
}

/* ---- tasks ------------------------------------------------------------------------------ */

void app_init(void)
{
    rx_queue = osMessageQueueNew(RX_QUEUE_LEN, sizeof(uint8_t), NULL);
}

void app_register_tasks(osThreadId_t console, osThreadId_t relative, osThreadId_t absolute,
                        osThreadId_t interference)
{
    console_handle = console;
    vTaskSetApplicationTaskTag((TaskHandle_t)console, (TaskHookFunction_t)(uintptr_t)TRACE_CONSOLE_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)relative, (TaskHookFunction_t)(uintptr_t)TRACE_RELATIVE_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)absolute, (TaskHookFunction_t)(uintptr_t)TRACE_ABSOLUTE_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)interference, (TaskHookFunction_t)(uintptr_t)TRACE_INTERFERENCE_Pin);
}

void app_console_task(void *argument)
{
    (void)argument;
    char line[LINE_MAX];
    size_t len = 0;
    bool watch = false;
    TickType_t last = xTaskGetTickCount();

    stats_reset();
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1u);
    con_print("\r\nFreeRTOS periodic tasks: vTaskDelay vs vTaskDelayUntil - type 'help'\r\n> ");

    for (;;) {
        const TickType_t since = xTaskGetTickCount() - last;
        const uint32_t wait = (since >= WATCH_MS) ? 0u : WATCH_MS - since;
        uint8_t c;

        if (osMessageQueueGet(rx_queue, &c, NULL, wait) == osOK) {
            if (c == BUTTON_EVENT) {
                static uint32_t saved = 3u;
                if (burst_ms) {
                    saved = burst_ms;
                    burst_ms = 0u;
                } else {
                    burst_ms = saved;
                }
                stats_reset();
                char msg[64];
                snprintf(msg, sizeof(msg), "\r\n[B1] interference %lu ms\r\n> ", (unsigned long)burst_ms);
                con_print(msg);
            } else if (c == '\r' || c == '\n') {
                if (c == '\n' && len == 0u) {
                    continue;
                }
                con_print("\r\n");
                line[len] = '\0';
                execute(line, &watch);
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

        if (xTaskGetTickCount() - last >= WATCH_MS) {
            last = xTaskGetTickCount();
            if (watch) {
                con_print("\r\n");
                print_report();
                con_print("> ");
            }
        }
    }
}

void app_relative_task(void *argument)
{
    (void)argument;
    for (;;) {
        const uint32_t now = DWT->CYCCNT;   /* release time */
        taskENTER_CRITICAL();              /* the console may reset the stats */
        ps_release(&stats_relative, now);
        taskEXIT_CRITICAL();
        HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
        busy_ms(work_ms);
        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));           /* relative: counts from now */
    }
}

void app_absolute_task(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        const uint32_t now = DWT->CYCCNT;   /* release time */
        taskENTER_CRITICAL();              /* the console may reset the stats */
        ps_release(&stats_absolute, now);
        taskEXIT_CRITICAL();
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        busy_ms(work_ms);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(PERIOD_MS)); /* absolute: fixed grid */
    }
}

void app_interference_task(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        busy_ms(burst_ms);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(INTERFERENCE_MS));
    }
}
