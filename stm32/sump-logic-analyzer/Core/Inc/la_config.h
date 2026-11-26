/**
 * @file    la_config.h
 * @brief   Logic analyzer limits and clock constants (shared by firmware and host tests).
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef LA_CONFIG_H
#define LA_CONFIG_H

/* Sample ring that DMA writes continuously while a capture is armed.
 * One byte per sample = the 8 inputs PC0..PC7. */
#define LA_RING_SIZE        12288u

/* Largest capture returned to the host. The difference to LA_RING_SIZE
 * (4096 samples) is the head room the trigger scanner may fall behind the
 * DMA write position before samples are overwritten. */
#define LA_MAX_SAMPLES      8192u

#define LA_NUM_CHANNELS     8u

/* TIM2 kernel clock: APB1 = 32 MHz, timer clock doubled to 64 MHz. */
#define LA_TIMER_CLOCK_HZ   64000000u

/* Fastest sample rate: 64 MHz / 16 = 4 MS/s. */
#define LA_MIN_TICKS        16u
#define LA_MAX_RATE_HZ      (LA_TIMER_CLOCK_HZ / LA_MIN_TICKS)

/* SUMP clients express the sample rate as a divider of a 100 MHz clock. */
#define SUMP_CLOCK_HZ       100000000u

#define LA_DEVICE_NAME      "STM32F103 Logic Analyzer"
#define LA_FW_VERSION       "1.0.0"

#endif /* LA_CONFIG_H */
