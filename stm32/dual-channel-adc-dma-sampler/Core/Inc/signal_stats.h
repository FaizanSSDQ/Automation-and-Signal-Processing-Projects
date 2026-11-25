/**
 * @file    signal_stats.h
 * @brief   Integer-only block statistics for interleaved multi-channel ADC data.
 *
 * Hardware independent: compiled for the STM32 and for host unit tests.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef SIGNAL_STATS_H
#define SIGNAL_STATS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Statistics of one channel over one block, in raw ADC counts. */
typedef struct {
    uint16_t min;    /**< smallest sample */
    uint16_t max;    /**< largest sample */
    uint16_t mean;   /**< arithmetic mean (DC level), rounded */
    uint16_t ac_rms; /**< RMS of the signal with the mean removed (AC content) */
} channel_stats_t;

/**
 * Compute statistics for one channel of an interleaved buffer
 * (ch0, ch1, ..., chN-1, ch0, ch1, ...).
 *
 * @param data      interleaved samples
 * @param frames    number of frames (samples per channel)
 * @param channels  number of interleaved channels (>= 1)
 * @param channel   channel index to analyse (< channels)
 * @param out       result; zeroed when frames == 0
 */
void stats_compute(const uint16_t *data, size_t frames, size_t channels, size_t channel,
                   channel_stats_t *out);

/** Integer square root: floor(sqrt(value)). */
uint32_t isqrt64(uint64_t value);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_STATS_H */
