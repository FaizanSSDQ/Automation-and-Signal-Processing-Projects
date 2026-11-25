/**
 * @file    app_sampler.h
 * @brief   Dual-channel, timer-triggered ADC sampler with ping-pong DMA buffers.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef APP_SAMPLER_H
#define APP_SAMPLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Configuration ---------------------------------------------------------------------- */
#define SAMPLER_SAMPLE_RATE_HZ 2000u /**< per channel; must match TIM3 (32 MHz / 32 / 500) */
#define SAMPLER_CHANNELS 2u          /**< ADC scan length: IN1 (PA1/A1), IN4 (PA4/A2) */
#define SAMPLER_BLOCK_FRAMES 256u    /**< frames per half buffer -> 128 ms blocks */
#define SAMPLER_STATS_PAYLOAD 28u    /**< bytes in a FRAME_TYPE_STATS payload */

/** Start TIM3-triggered conversions into the circular DMA buffer. */
void sampler_start(void);

/** Process any completed half buffer. Call continuously from the main loop. */
void sampler_poll(void);

/** Toggle raw-sample streaming (bound to the user button). */
void sampler_toggle_streaming(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SAMPLER_H */
