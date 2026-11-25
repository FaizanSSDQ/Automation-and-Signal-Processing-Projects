/**
 * @file    app_sampler.c
 * @brief   Dual-channel ADC acquisition with ping-pong DMA, block statistics and
 *          non-blocking UART telemetry.
 *
 * Data path
 *   TIM3 update (2 kHz) --TRGO--> ADC scan (IN1, IN4) --DMA1 Ch1, circular--> dma_buf
 *   DMA half-transfer IRQ  -> block 0 ready      DMA transfer-complete IRQ -> block 1 ready
 *   main loop: statistics on the ready block -> frames -> USART2 TX via DMA1 Ch7
 *
 * Timing budget (defaults): one block = 256 frames / 2 kHz = 128 ms.
 * Worst-case UART load = 37 B stats + 1033 B samples = 1070 B at 230400 baud ~ 46 ms.
 *
 * STATS payload (28 bytes, little-endian):
 *   u16 sample_rate_hz, u16 frames,
 *   2 x { u16 min, u16 max, u16 mean, u16 ac_rms }   (raw ADC counts, 12-bit)
 *   u16 overruns, u16 tx_drops, u32 cpu_cycles (processing time of this block)
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#include "app_sampler.h"

#include <stdbool.h>
#include <string.h>

#include "frame.h"
#include "main.h"
#include "signal_stats.h"

extern ADC_HandleTypeDef hadc;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;

#define BLOCK_SAMPLES (SAMPLER_BLOCK_FRAMES * SAMPLER_CHANNELS)
#define DMA_BUF_SAMPLES (2u * BLOCK_SAMPLES)
#define SAMPLES_PAYLOAD (BLOCK_SAMPLES * 2u)
#define TX_BUF_SIZE ((FRAME_OVERHEAD + SAMPLER_STATS_PAYLOAD) + (FRAME_OVERHEAD + SAMPLES_PAYLOAD))
#define BUTTON_DEBOUNCE_MS 200u

_Static_assert(SAMPLES_PAYLOAD <= UINT16_MAX, "sample payload must fit the 16-bit length field");

static uint16_t dma_buf[DMA_BUF_SAMPLES];
static uint8_t tx_buf[TX_BUF_SIZE];

static volatile uint8_t ready_mask;   /* bit0: first half ready, bit1: second half ready */
static volatile uint16_t overruns;    /* a half completed again before it was processed */
static volatile bool streaming;       /* send raw samples in addition to statistics */
static volatile uint32_t last_press_ms;
static uint16_t tx_drops;             /* blocks not sent because the UART was still busy */
static uint16_t seq;

static void cycle_counter_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void mark_ready(uint8_t half)
{
    const uint8_t bit = (uint8_t)(1u << half);
    if (ready_mask & bit) {
        overruns++; /* main loop fell behind: this block is overwritten */
    }
    ready_mask |= bit;
}

void sampler_start(void)
{
    cycle_counter_init();
    ready_mask = 0;
    /* Arm the ADC first so the very first TRGO edge is captured. */
    if (HAL_ADC_Start_DMA(&hadc, (uint32_t *)dma_buf, DMA_BUF_SAMPLES) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_Base_Start(&htim3) != HAL_OK) {
        Error_Handler();
    }
}

void sampler_toggle_streaming(void)
{
    const uint32_t now = HAL_GetTick();
    if (now - last_press_ms >= BUTTON_DEBOUNCE_MS) {
        last_press_ms = now;
        streaming = !streaming;
    }
}

static void process_block(const uint16_t *block)
{
    const uint32_t t0 = DWT->CYCCNT;
    HAL_GPIO_WritePin(DBG_PROC_GPIO_Port, DBG_PROC_Pin, GPIO_PIN_SET);

    channel_stats_t st[SAMPLER_CHANNELS];
    for (size_t ch = 0; ch < SAMPLER_CHANNELS; ch++) {
        stats_compute(block, SAMPLER_BLOCK_FRAMES, SAMPLER_CHANNELS, ch, &st[ch]);
    }

    if (huart2.gState != HAL_UART_STATE_READY) {
        tx_drops++; /* previous transmission still running: skip this block */
    } else {
        size_t used = 0;
        const bool send_samples = streaming;

        /* Raw samples are copied into tx_buf now, before DMA overwrites this half. */
        if (send_samples) {
            used += frame_encode(FRAME_TYPE_SAMPLES, seq, (const uint8_t *)block,
                                 (uint16_t)SAMPLES_PAYLOAD, tx_buf + used, sizeof(tx_buf) - used);
        }

        const uint32_t cycles = DWT->CYCCNT - t0;
        uint8_t payload[SAMPLER_STATS_PAYLOAD];
        uint8_t *p = payload;
        p = put_u16(p, (uint16_t)SAMPLER_SAMPLE_RATE_HZ);
        p = put_u16(p, (uint16_t)SAMPLER_BLOCK_FRAMES);
        for (size_t ch = 0; ch < SAMPLER_CHANNELS; ch++) {
            p = put_u16(p, st[ch].min);
            p = put_u16(p, st[ch].max);
            p = put_u16(p, st[ch].mean);
            p = put_u16(p, st[ch].ac_rms);
        }
        p = put_u16(p, overruns);
        p = put_u16(p, tx_drops);
        (void)put_u32(p, cycles);
        used += frame_encode(FRAME_TYPE_STATS, seq, payload, sizeof(payload), tx_buf + used,
                             sizeof(tx_buf) - used);

        if (HAL_UART_Transmit_DMA(&huart2, tx_buf, (uint16_t)used) != HAL_OK) {
            tx_drops++;
        }
    }

    seq++;
    HAL_GPIO_WritePin(DBG_PROC_GPIO_Port, DBG_PROC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); /* heartbeat: toggles every block */
}

void sampler_poll(void)
{
    for (uint8_t half = 0; half < 2u; half++) {
        const uint8_t bit = (uint8_t)(1u << half);
        if (ready_mask & bit) {
            process_block(&dma_buf[half * BLOCK_SAMPLES]);
            __disable_irq();
            ready_mask &= (uint8_t)~bit;
            __enable_irq();
        }
    }
}

/* ---- HAL callbacks (run in interrupt context: keep them minimal) -------------------------- */

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h)
{
    if (h->Instance == ADC1) {
        mark_ready(0);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    if (h->Instance == ADC1) {
        mark_ready(1);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == B1_Pin) {
        sampler_toggle_streaming();
    }
}
