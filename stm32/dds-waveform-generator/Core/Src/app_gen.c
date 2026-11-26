/**
 * @file    app_gen.c
 * @brief   Waveform generator application: PWM/DMA output, command line, button.
 *
 * Output path (no CPU per sample):
 *   TIM3 update (125 kHz) -> DMA1 Ch3 burst of 2 -> TIM3->DMAR -> CCR1, CCR2
 *
 * TIM3's DMA burst mode (DCR: base CCR1, length 2) turns every update event
 * into two DMA transfers, written to CCR1 (waveform) and CCR2 (sync). The DMA
 * runs in circular mode over a buffer of two halves; its half-transfer and
 * transfer-complete interrupts refill the half that was just sent, so the
 * DDS core always works 256 samples (2.05 ms) ahead of the output.
 *
 * Settings from the command line are handed to the interrupt through a
 * "pending" copy and a flag, so the renderer never sees half-written settings.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "app_gen.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "dds.h"
#include "gen_config.h"
#include "main.h"

extern TIM_HandleTypeDef htim3;
extern DMA_HandleTypeDef hdma_tim3_up;
extern UART_HandleTypeDef huart2;

#define RX_QUEUE_SIZE    64u   /* power of two */
#define BUTTON_DEBOUNCE  250u  /* ms */
#define CYCLES_PER_BLOCK (GEN_BLOCK * GEN_PWM_PERIOD) /* CPU and timer both run at 64 MHz */

/* Two halves of GEN_BLOCK samples, each sample = {CCR1, CCR2}. */
static uint16_t dma_buf[2u * 2u * GEN_BLOCK];

static dds_t dds;                       /* owned by the DMA interrupt */
static dds_settings_t settings;         /* owned by the main loop */
static dds_settings_t pending;
static volatile bool pending_flag;

static volatile uint32_t load_last, load_max; /* render cycles per block */
static volatile uint32_t blocks;

static uint8_t rx_queue[RX_QUEUE_SIZE];
static volatile uint32_t rx_head, rx_tail;
static uint8_t rx_byte;
static cli_line_t line;

static volatile bool button_pressed;
static uint32_t button_last_ms;

/* ---- output: DMA refill ----------------------------------------------------------------- */

static void refill(uint32_t half)
{
    const uint32_t t0 = DWT->CYCCNT;
    if (pending_flag) {
        dds_apply(&dds, &pending);
        __DMB();
        pending_flag = false;
    }
    dds_render(&dds, &dma_buf[half * 2u * GEN_BLOCK], GEN_BLOCK);

    const uint32_t cycles = DWT->CYCCNT - t0;
    load_last = cycles;
    if (cycles > load_max) {
        load_max = cycles;
    }
    blocks++;
}

static void on_dma_half(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    refill(0u); /* first half has just been sent */
}

static void on_dma_full(DMA_HandleTypeDef *hdma)
{
    (void)hdma;
    refill(1u);
}

static void output_start(void)
{
    /* Cycle counter for measuring the render load. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    dds_init(&dds, &settings);
    dds_render(&dds, dma_buf, 2u * GEN_BLOCK); /* prefill both halves */

    htim3.Instance->DCR = TIM_DMABASE_CCR1 | TIM_DMABURSTLENGTH_2TRANSFERS;
    hdma_tim3_up.XferHalfCpltCallback = on_dma_half;
    hdma_tim3_up.XferCpltCallback = on_dma_full;
    if (HAL_DMA_Start_IT(&hdma_tim3_up, (uint32_t)dma_buf, (uint32_t)&htim3.Instance->DMAR,
                         2u * 2u * GEN_BLOCK) != HAL_OK) {
        Error_Handler();
    }
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_UPDATE);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    (void)HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

/* Hand new settings to the interrupt; waits at most one block (2 ms). */
static void settings_commit(void)
{
    while (pending_flag) {
    }
    pending = settings;
    __DMB();
    pending_flag = true;
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, settings.output_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ---- serial terminal -------------------------------------------------------------------- */

static void send(const char *s)
{
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 1000u);
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
        }
        rx_start();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2) {
        rx_start();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == B1_Pin) {
        button_pressed = true;
    }
}

static void print_load(void)
{
    char out[96];
    const uint32_t last = load_last * 1000u / CYCLES_PER_BLOCK;
    const uint32_t max = load_max * 1000u / CYCLES_PER_BLOCK;
    snprintf(out, sizeof(out), "render load %lu.%lu %% (max %lu.%lu %%), %lu blocks\r\n",
             (unsigned long)(last / 10u), (unsigned long)(last % 10u),
             (unsigned long)(max / 10u), (unsigned long)(max % 10u), (unsigned long)blocks);
    send(out);
}

static void handle_line(void)
{
    char reply[512];
    send("\r\n");
    if (line.overflow) {
        send("error: line too long\r\n");
    } else {
        const bool is_status = strncmp(line.buf, "status", 6) == 0;
        const cli_result_t r = cli_execute(line.buf, &settings, reply, sizeof(reply));
        if (r == CLI_CHANGED) {
            settings_commit();
        }
        if (r != CLI_EMPTY) {
            send(reply);
            send("\r\n");
        }
        if (is_status) {
            print_load();
        }
    }
    cli_line_reset(&line);
    send("> ");
}

/* ---- public ----------------------------------------------------------------------------- */

void app_gen_init(void)
{
    char status[256];
    dds_settings_default(&settings);
    cli_line_reset(&line);
    output_start();
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    rx_start();

    send("\r\nDDS waveform generator " GEN_FW_VERSION ": 125 kS/s, 9-bit PWM on PA6, sync on PA7\r\n");
    cli_format_status(&settings, status, sizeof(status));
    send(status);
    send("\r\ntype 'help' for commands, B1 cycles the waveform\r\n> ");
}

void app_gen_run(void)
{
    static char prev;
    while (rx_tail != rx_head) {
        const char c = (char)rx_queue[rx_tail & (RX_QUEUE_SIZE - 1u)];
        rx_tail++;
        const bool crlf = (c == '\n' && prev == '\r'); /* terminals sending CR LF */
        prev = c;
        if (crlf) {
            continue;
        }
        if (cli_line_feed(&line, c)) {
            handle_line();
        } else if (c == '\b' || c == 0x7F) {
            send("\b \b");
        } else {
            const char echo[2] = {c, '\0'};
            send(echo);
        }
    }

    if (button_pressed) {
        button_pressed = false;
        const uint32_t now = HAL_GetTick();
        if (now - button_last_ms >= BUTTON_DEBOUNCE) {
            button_last_ms = now;
            settings.wave = (dds_wave_t)((settings.wave + 1u) % DDS_WAVE_COUNT);
            settings_commit();
            char out[64];
            snprintf(out, sizeof(out), "\r\n[B1] wave %s\r\n> ", cli_wave_name(settings.wave));
            send(out);
        }
    }
}
