/**
 * @file    app_la.c
 * @brief   Logic analyzer application: SUMP host link and capture control.
 *
 * Sampling path (no CPU involvement):
 *   TIM2 update event -> DMA1 Channel 2 request -> GPIOC->IDR low byte -> ring[]
 *
 * The DMA runs in circular mode over a 12 KB ring. Its transfer-complete
 * interrupt counts laps, so the main loop always knows the absolute number of
 * samples written. The main loop scans new samples for the trigger
 * (la_capture.c); when the post-trigger samples are in, it stops TIM2 and
 * uploads the window to the host, newest sample first as SUMP requires.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "app_la.h"

#include <stdbool.h>

#include "demo_signals.h"
#include "la_capture.h"
#include "la_config.h"
#include "main.h"
#include "sump.h"

extern TIM_HandleTypeDef htim2;
extern DMA_HandleTypeDef hdma_tim2_up;
extern UART_HandleTypeDef huart2;

#define RX_QUEUE_SIZE   64u   /* power of two */
#define UPLOAD_CHUNK    64u
#define ERROR_BLINK_MS  1500u

static uint8_t ring[LA_RING_SIZE];
static volatile uint32_t dma_laps;

/* Host bytes: written by the USART2 interrupt, read by the main loop. */
static uint8_t rx_queue[RX_QUEUE_SIZE];
static volatile uint32_t rx_head, rx_tail, rx_dropped;
static uint8_t rx_byte;

static sump_parser_t parser;
static sump_config_t config;
static la_capture_t capture;
static bool capturing;
static uint32_t error_until_ms;

/* Counters, readable in the debugger. */
static volatile uint32_t stat_captures, stat_overruns;

/* ---- DMA write position ----------------------------------------------------------------- */

static void on_dma_lap(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    dma_laps++;
}

static uint64_t samples_written(void)
{
    /* CNDTR first, then the transfer-complete flag; see la_ring_written(). */
    __disable_irq();
    const uint32_t cndtr = hdma_tim2_up.Instance->CNDTR;
    const bool tc_pending = (DMA1->ISR & DMA_ISR_TCIF2) != 0u;
    const uint32_t laps = dma_laps;
    __enable_irq();
    return la_ring_written(laps, cndtr, tc_pending, LA_RING_SIZE);
}

/* ---- host link -------------------------------------------------------------------------- */

static void host_send(const uint8_t *data, uint16_t len)
{
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 1000u);
}

static void rx_start(void)
{
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte, 1u);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        const uint32_t head = rx_head;
        if (head - rx_tail < RX_QUEUE_SIZE) {
            rx_queue[head & (RX_QUEUE_SIZE - 1u)] = rx_byte;
            rx_head = head + 1u;
        } else {
            rx_dropped++;
        }
        rx_start();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        rx_start(); /* an overrun or noise error stops reception: re-arm */
    }
}

/* ---- capture control -------------------------------------------------------------------- */

static void sampling_stop(void)
{
    __HAL_TIM_DISABLE(&htim2);
    __HAL_TIM_DISABLE_DMA(&htim2, TIM_DMA_UPDATE);
}

static void capture_arm(void)
{
    uint32_t depth, pre;
    la_trigger_t trig;
    uint16_t psc, arr;

    sump_capture_plan(&config, &depth, &pre, &trig);
    (void)la_timer_split(la_divider_to_ticks(config.divider), &psc, &arr);
    if (!la_capture_start(&capture, LA_RING_SIZE, depth, pre, &trig)) {
        return;
    }

    /* Load the sample period while the DMA request is still disabled, so the
     * forced update event does not store a sample. */
    __HAL_TIM_DISABLE(&htim2);
    htim2.Instance->PSC = psc;
    htim2.Instance->ARR = arr;
    htim2.Instance->CNT = 0u;
    htim2.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);

    dma_laps = 0u;
    hdma_tim2_up.XferCpltCallback = on_dma_lap;
    hdma_tim2_up.XferHalfCpltCallback = NULL; /* only the lap interrupt is needed */
    if (HAL_DMA_Start_IT(&hdma_tim2_up, (uint32_t)&GPIOC->IDR, (uint32_t)ring, LA_RING_SIZE) != HAL_OK) {
        return;
    }
    __HAL_TIM_ENABLE_DMA(&htim2, TIM_DMA_UPDATE);
    __HAL_TIM_ENABLE(&htim2);

    capturing = true;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
}

static void capture_abort(void)
{
    sampling_stop();
    (void)HAL_DMA_Abort(&hdma_tim2_up);
    capturing = false;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}

/* SUMP sends the newest sample first. */
static void upload(uint32_t first, uint32_t count)
{
    uint8_t chunk[UPLOAD_CHUNK];
    uint32_t idx = (first + count - 1u) % LA_RING_SIZE;
    while (count > 0u) {
        const uint32_t n = (count < UPLOAD_CHUNK) ? count : UPLOAD_CHUNK;
        for (uint32_t k = 0; k < n; k++) {
            chunk[k] = ring[idx];
            idx = (idx == 0u) ? LA_RING_SIZE - 1u : idx - 1u;
        }
        host_send(chunk, (uint16_t)n);
        count -= n;
    }
}

static void capture_service(void)
{
    la_state_t st = la_capture_poll(&capture, ring, samples_written());
    if (st != LA_DONE && st != LA_OVERRUN) {
        return;
    }

    sampling_stop();
    const uint64_t written = samples_written();
    (void)HAL_DMA_Abort(&hdma_tim2_up);
    st = la_capture_finish(&capture, written);
    capturing = false;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    uint32_t first;
    if (st == LA_DONE) {
        stat_captures++;
        first = la_capture_first_index(&capture);
    } else {
        /* The trigger scan fell behind. The host still expects data, so send
         * the newest samples and flash LD2 to report the problem. */
        stat_overruns++;
        error_until_ms = HAL_GetTick() + ERROR_BLINK_MS;
        first = (uint32_t)((written - capture.depth) % LA_RING_SIZE);
    }
    upload(first, capture.depth);
}

static void handle_host_byte(uint8_t b)
{
    switch (sump_feed(&parser, &config, b)) {
    case SUMP_EV_RESET:
        if (capturing) {
            capture_abort();
        }
        break;
    case SUMP_EV_ID: {
        static const uint8_t id[4] = {'1', 'A', 'L', 'S'};
        host_send(id, sizeof(id));
        break;
    }
    case SUMP_EV_METADATA: {
        uint8_t meta[96];
        const size_t n = sump_metadata(meta, sizeof(meta));
        host_send(meta, (uint16_t)n);
        break;
    }
    case SUMP_EV_RUN:
        if (!capturing) {
            capture_arm();
        }
        break;
    default:
        break;
    }
}

static void led_service(void)
{
    if (capturing) {
        return; /* LD2 stays on while armed */
    }
    GPIO_PinState on = GPIO_PIN_RESET;
    if ((int32_t)(error_until_ms - HAL_GetTick()) > 0) {
        on = ((HAL_GetTick() / 100u) & 1u) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, on);
}

/* ---- public ----------------------------------------------------------------------------- */

void app_la_init(void)
{
    sump_init(&parser, &config);
    error_until_ms = HAL_GetTick();
    rx_start();
    demo_signals_start();
}

void app_la_run(void)
{
    while (rx_tail != rx_head) {
        const uint8_t b = rx_queue[rx_tail & (RX_QUEUE_SIZE - 1u)];
        rx_tail++;
        handle_host_byte(b);
    }
    if (capturing) {
        capture_service();
    }
    demo_signals_poll();
    led_service();
}
