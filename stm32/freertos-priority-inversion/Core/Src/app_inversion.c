/**
 * @file    app_inversion.c
 * @brief   Priority inversion with a binary semaphore, and its cure with a mutex.
 *
 * Every 100 ms the same scene plays out:
 *
 *   t = 0 ms   low (below normal) takes the shared lock and works `hold` ms
 *   t = 2 ms   high (above normal) wants the lock and has to wait for low
 *   t = 3 ms   medium (normal) becomes ready and wants `medium` ms of CPU
 *
 * Binary semaphore: medium outranks low, so it runs first, and high, the most
 * important task, waits for medium too: about (hold - 2) + medium ms.
 * Mutex: FreeRTOS raises low to high's priority while high waits (priority
 * inheritance), medium cannot preempt it, and high waits only (hold - 2) ms.
 *
 * The high task measures its waiting time with the DWT cycle counter; the low
 * task records the highest priority it ran at while holding the lock. LD2 is
 * lit while high is blocked.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "app_inversion.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "cmd.h"
#include "inversion_stats.h"
#include "main.h"
#include "task.h"

extern UART_HandleTypeDef huart2;

#define RX_QUEUE_LEN     32u
#define RESULT_QUEUE_LEN 4u
#define BUTTON_EVENT     0x01u
#define FLAG_TX_DONE     0x0001u
#define LINE_MAX         32u
#define HIGH_WORK_MS     1u

typedef struct {
    uint8_t kind;
    uint8_t holder_prio;
    uint32_t wait_us;
} cycle_result_t;

static osThreadId_t console_handle;
static osMessageQueueId_t rx_queue, result_queue;
static uint8_t rx_byte;

static osSemaphoreId_t lock_sem;
static osMutexId_t lock_mtx;

static volatile lock_kind_t requested_kind = LOCK_SEMAPHORE;
static volatile lock_kind_t cycle_kind = LOCK_SEMAPHORE;    /* chosen by low at the cycle start */
static volatile uint32_t medium_ms = 30u;
static volatile uint32_t hold_ms = 10u;
static volatile uint8_t holder_prio;                         /* highest priority low ran at */
static inversion_stats_t stats;

static const char help_text[] =
    "commands:\r\n"
    "  mode semaphore|mutex   lock shared by low and high\r\n"
    "  medium <0..60>         medium task CPU time per cycle (ms)\r\n"
    "  hold <1..20>           how long low holds the lock (ms)\r\n"
    "  report                 blocking time of high for both lock types\r\n"
    "  watch on|off           one line per 100 ms cycle\r\n"
    "  reset                  clear the statistics\r\n"
    "  B1 button              switch semaphore <-> mutex\r\n";

/* ---- helpers ---------------------------------------------------------------------------- */

static void lock_take(lock_kind_t kind)
{
    if (kind == LOCK_MUTEX) {
        (void)osMutexAcquire(lock_mtx, osWaitForever);
    } else {
        (void)osSemaphoreAcquire(lock_sem, osWaitForever);
    }
}

static void lock_give(lock_kind_t kind)
{
    if (kind == LOCK_MUTEX) {
        (void)osMutexRelease(lock_mtx);
    } else {
        (void)osSemaphoreRelease(lock_sem);
    }
}

static void busy_ms(uint32_t ms)
{
    const uint32_t start = DWT->CYCCNT;
    const uint32_t cycles = ms * (SystemCoreClock / 1000u);
    while (DWT->CYCCNT - start < cycles) {
    }
}

static void stats_reset(void)
{
    taskENTER_CRITICAL();
    inv_init(&stats);
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

/* ---- console ---------------------------------------------------------------------------- */

static void print_report(void)
{
    static char out[400];
    inversion_stats_t copy;
    taskENTER_CRITICAL();
    copy = stats;
    taskEXIT_CRITICAL();
    size_t len = (size_t)snprintf(out, sizeof(out), "hold %lu ms, medium %lu ms; high waits for the lock:\r\n",
                                  (unsigned long)hold_ms, (unsigned long)medium_ms);
    len += inv_format(&copy, requested_kind, out + len, sizeof(out) - len);
    con_write(out, len);
}

static void set_mode(lock_kind_t kind)
{
    char msg[64];
    requested_kind = kind;  /* low switches at its next cycle start, when nobody holds a lock */
    snprintf(msg, sizeof(msg), "lock: %s\r\n", inv_kind_name(kind));
    con_print(msg);
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
    case CMD_MODE:
        set_mode(c.value ? LOCK_MUTEX : LOCK_SEMAPHORE);
        return;
    case CMD_MEDIUM:
        medium_ms = (uint32_t)c.value;
        stats_reset();
        snprintf(reply, sizeof(reply), "medium %d ms (statistics reset)\r\n", c.value);
        break;
    case CMD_HOLD:
        hold_ms = (uint32_t)c.value;
        stats_reset();
        snprintf(reply, sizeof(reply), "hold %d ms (statistics reset)\r\n", c.value);
        break;
    case CMD_WATCH:
        *watch = c.value != 0;
        snprintf(reply, sizeof(reply), "watch %s\r\n", *watch ? "on" : "off");
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

void app_init(void)
{
    static const osMutexAttr_t mutex_attr = {
        .name = "lock_mutex",
        .attr_bits = osMutexPrioInherit,  /* FreeRTOS mutexes always inherit; stated for clarity */
    };
    rx_queue = osMessageQueueNew(RX_QUEUE_LEN, sizeof(uint8_t), NULL);
    result_queue = osMessageQueueNew(RESULT_QUEUE_LEN, sizeof(cycle_result_t), NULL);
    lock_sem = osSemaphoreNew(1u, 1u, NULL);   /* binary semaphore: no owner, no inheritance */
    lock_mtx = osMutexNew(&mutex_attr);
}

void app_register_tasks(osThreadId_t console, osThreadId_t high, osThreadId_t medium, osThreadId_t low)
{
    console_handle = console;
    vTaskSetApplicationTaskTag((TaskHandle_t)console, (TaskHookFunction_t)(uintptr_t)TRACE_CONSOLE_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)high, (TaskHookFunction_t)(uintptr_t)TRACE_HIGH_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)medium, (TaskHookFunction_t)(uintptr_t)TRACE_MEDIUM_Pin);
    vTaskSetApplicationTaskTag((TaskHandle_t)low, (TaskHookFunction_t)(uintptr_t)TRACE_LOW_Pin);
}

void app_console_task(void *argument)
{
    (void)argument;
    char line[LINE_MAX];
    size_t len = 0;
    bool watch = false;

    stats_reset();
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1u);
    con_print("\r\nFreeRTOS priority inversion: binary semaphore vs mutex - type 'help'\r\nlock: semaphore\r\n> ");

    for (;;) {
        cycle_result_t r;
        while (osMessageQueueGet(result_queue, &r, NULL, 0u) == osOK) {
            if (watch) {
                char out[96];
                const size_t n = inv_format_cycle((lock_kind_t)r.kind, r.wait_us, r.holder_prio, out, sizeof(out));
                con_print("\r");
                con_write(out, n);
                con_print("> ");
            }
        }

        uint8_t c;
        if (osMessageQueueGet(rx_queue, &c, NULL, 20u) != osOK) {
            continue;
        }
        if (c == BUTTON_EVENT) {
            con_print("\r\n[B1] ");
            set_mode(requested_kind == LOCK_MUTEX ? LOCK_SEMAPHORE : LOCK_MUTEX);
            con_print("> ");
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
}

/* ---- the three actors ------------------------------------------------------------------- */

void app_low_task(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        const lock_kind_t kind = requested_kind;  /* switch only while nobody holds a lock */
        cycle_kind = kind;
        lock_take(kind);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);   /* PA4: lock held */

        /* Work for `hold` ms of wall-clock time, sampling our own priority so the
         * effect of inheritance is visible. */
        UBaseType_t top = uxTaskPriorityGet(NULL);
        const uint32_t start = DWT->CYCCNT;
        const uint32_t cycles = hold_ms * (SystemCoreClock / 1000u);
        while (DWT->CYCCNT - start < cycles) {
            const UBaseType_t p = uxTaskPriorityGet(NULL);
            top = p > top ? p : top;
        }
        holder_prio = (uint8_t)top;

        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
        lock_give(kind);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CYCLE_MS));
    }
}

void app_high_task(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(HIGH_OFFSET_MS));
    for (;;) {
        const lock_kind_t kind = cycle_kind;
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);     /* LD2: high is waiting */
        const uint32_t t0 = DWT->CYCCNT;
        lock_take(kind);
        const uint32_t wait_us = (DWT->CYCCNT - t0) / (SystemCoreClock / 1000000u);
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

        busy_ms(HIGH_WORK_MS);
        lock_give(kind);

        const cycle_result_t r = {(uint8_t)kind, holder_prio, wait_us};
        taskENTER_CRITICAL();
        inv_record(&stats, kind, wait_us, holder_prio);
        taskEXIT_CRITICAL();
        (void)osMessageQueuePut(result_queue, &r, 0u, 0u);  /* drop if the console is behind */

        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CYCLE_MS));
    }
}

void app_medium_task(void *argument)
{
    (void)argument;
    TickType_t wake = xTaskGetTickCount();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(MEDIUM_OFFSET_MS));
    for (;;) {
        busy_ms(medium_ms);   /* needs no lock at all, yet it can delay high */
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CYCLE_MS));
    }
}
