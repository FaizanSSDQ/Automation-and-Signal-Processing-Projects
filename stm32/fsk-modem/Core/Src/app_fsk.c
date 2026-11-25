/**
 * @file    app_fsk.c
 * @brief   FSK modem application on the NUCLEO-L152RE.
 *
 * Transmit path
 *   PC terminal -> USART2 RX interrupt -> fsk_mod queue
 *   TIM6 (16 kHz) -> DAC1 CH1 (PA4) <- DMA1 Ch2 circular <- dac_buf
 *   DMA half/full callbacks refill the half that was just played with fsk_mod_fill().
 *
 * Receive path
 *   PA1 -> ADC_IN1 <- TIM3 (16 kHz) -> DMA1 Ch1 circular -> adc_buf (ping-pong)
 *   main loop: fsk_demod_process() on each completed half -> decoded characters
 *   -> output ring -> USART2 TX interrupt -> PC terminal
 *
 * With a jumper from PA4 to PA1 the board demodulates its own transmission, so text
 * typed in the terminal only appears if the whole analog chain works.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#include "app_fsk.h"

#include <stdbool.h>
#include <stdio.h>

#include "fsk_config.h"
#include "fsk_demod.h"
#include "fsk_mod.h"
#include "main.h"

extern ADC_HandleTypeDef hadc;
extern DAC_HandleTypeDef hdac;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart2;

#define BLOCK 64u /* samples per DMA half buffer = 4 ms at 16 kHz */
#define OUT_RING_SIZE 512u
#define OUT_MASK (OUT_RING_SIZE - 1u)
#define TX_CHUNK 64u
#define BUTTON_DEBOUNCE_MS 250u

_Static_assert((OUT_RING_SIZE & OUT_MASK) == 0, "ring size must be a power of two");

static const char banner[] =
    "\r\nSTM32 FSK modem ready: 1000 baud, mark 1000 Hz, space 2000 Hz, fs 16 kHz.\r\n"
    "Connect PA4 (A2) to PA1 (A1). Type to transmit; press B1 for a test message.\r\n\r\n";
static const char test_message[] =
    "The quick brown fox jumps over the lazy dog 0123456789\r\n";

static uint16_t dac_buf[2u * BLOCK];
static uint16_t adc_buf[2u * BLOCK];

static fsk_mod_t mod;
static fsk_demod_t demod;

static volatile uint8_t adc_ready;    /* bit0: first half, bit1: second half */
static volatile uint32_t adc_overruns;
static volatile uint32_t rx_queue_drops; /* terminal characters lost: TX queue full */
static volatile bool test_requested;
static volatile uint32_t last_press_ms;
static uint8_t uart_rx_byte;

static uint8_t out_ring[OUT_RING_SIZE];
static uint16_t out_head, out_tail;
static uint8_t tx_chunk[TX_CHUNK];
static uint32_t reported_framing_errors;
static uint32_t reported_queue_drops;
static const char *test_cursor;

/* ---- terminal output (main-loop context only) ------------------------------------------- */

static void out_put(uint8_t c)
{
    const uint16_t next = (uint16_t)((out_head + 1u) & OUT_MASK);
    if (next != out_tail) {
        out_ring[out_head] = c;
        out_head = next;
    }
}

static void out_puts(const char *s)
{
    while (*s != '\0') {
        out_put((uint8_t)*s++);
    }
}

static void out_flush(void)
{
    if (out_head == out_tail || huart2.gState != HAL_UART_STATE_READY) {
        return;
    }
    uint16_t n = 0;
    while (out_tail != out_head && n < TX_CHUNK) {
        tx_chunk[n++] = out_ring[out_tail];
        out_tail = (uint16_t)((out_tail + 1u) & OUT_MASK);
    }
    (void)HAL_UART_Transmit_IT(&huart2, tx_chunk, n);
}

/* Demodulator callback: a correctly framed character arrived. */
static void on_rx_byte(void *ctx, uint8_t byte)
{
    (void)ctx;
    out_put(byte);
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}

/* The modulator queue also receives characters from the UART ISR, so the main loop
 * writes to it inside a short critical section. */
static bool mod_put_locked(uint8_t c)
{
    __disable_irq();
    const bool ok = fsk_mod_put(&mod, c);
    __enable_irq();
    return ok;
}

/* ---- start / poll ------------------------------------------------------------------------- */

void app_fsk_start(void)
{
    fsk_mod_init(&mod);
    fsk_demod_init(&demod, on_rx_byte, NULL);
    fsk_mod_fill(&mod, dac_buf, 2u * BLOCK); /* pre-fill with idle mark tone */

    if (HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)dac_buf, 2u * BLOCK,
                          DAC_ALIGN_12B_R) != HAL_OK ||
        HAL_TIM_Base_Start(&htim6) != HAL_OK ||
        HAL_ADC_Start_DMA(&hadc, (uint32_t *)adc_buf, 2u * BLOCK) != HAL_OK ||
        HAL_TIM_Base_Start(&htim3) != HAL_OK ||
        HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1) != HAL_OK) {
        Error_Handler();
    }
    out_puts(banner);
}

void app_fsk_poll(void)
{
    for (uint8_t half = 0; half < 2u; half++) {
        const uint8_t bit = (uint8_t)(1u << half);
        if (adc_ready & bit) {
            fsk_demod_process(&demod, &adc_buf[half * BLOCK], BLOCK);
            __disable_irq();
            adc_ready &= (uint8_t)~bit;
            __enable_irq();
        }
    }

    if (test_requested && test_cursor == NULL) {
        test_requested = false;
        test_cursor = test_message;
    }
    while (test_cursor != NULL && *test_cursor != '\0' && mod_put_locked((uint8_t)*test_cursor)) {
        test_cursor++;
    }
    if (test_cursor != NULL && *test_cursor == '\0') {
        test_cursor = NULL;
    }

    if (demod.stats.framing_errors != reported_framing_errors) {
        reported_framing_errors = demod.stats.framing_errors;
        char msg[64];
        snprintf(msg, sizeof msg, "\r\n[fsk] framing errors: %lu\r\n",
                 (unsigned long)reported_framing_errors);
        out_puts(msg);
    }

    if (rx_queue_drops != reported_queue_drops) {
        reported_queue_drops = rx_queue_drops;
        char msg[72];
        snprintf(msg, sizeof msg, "\r\n[fsk] TX queue full, characters dropped: %lu\r\n",
                 (unsigned long)reported_queue_drops);
        out_puts(msg);
    }

    out_flush();
}

/* ---- interrupt callbacks (keep short) ----------------------------------------------------- */

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *h)
{
    (void)h;
    fsk_mod_fill(&mod, &dac_buf[0], BLOCK); /* first half finished playing */
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *h)
{
    (void)h;
    fsk_mod_fill(&mod, &dac_buf[BLOCK], BLOCK); /* second half finished playing */
}

static void adc_mark_ready(uint8_t half)
{
    const uint8_t bit = (uint8_t)(1u << half);
    if (adc_ready & bit) {
        adc_overruns++;
    }
    adc_ready |= bit;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h)
{
    (void)h;
    adc_mark_ready(0);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
{
    (void)h;
    adc_mark_ready(1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h->Instance == USART2) {
        if (!fsk_mod_put(&mod, uart_rx_byte)) {
            rx_queue_drops++;
        }
        (void)HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    /* e.g. overrun while pasting text: clear and keep receiving */
    if (h->Instance == USART2) {
        (void)HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    const uint32_t now = HAL_GetTick();
    if (pin == B1_Pin && now - last_press_ms >= BUTTON_DEBOUNCE_MS) {
        last_press_ms = now;
        test_requested = true;
    }
}
