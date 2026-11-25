/**
 * @file    signal_stats.c
 * @brief   Integer-only block statistics (no FPU required on Cortex-M3).
 *
 * Variance uses the exact integer identity
 *     var = (n * sum(x^2) - sum(x)^2) / n^2
 * which fits in 64 bits for 12-bit samples and n <= 65535.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#include "signal_stats.h"

uint32_t isqrt64(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = (uint64_t)1 << 62; /* highest power of four <= 2^63 */

    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)result;
}

void stats_compute(const uint16_t *data, size_t frames, size_t channels, size_t channel,
                   channel_stats_t *out)
{
    out->min = 0;
    out->max = 0;
    out->mean = 0;
    out->ac_rms = 0;
    if (data == NULL || frames == 0 || channels == 0 || channel >= channels) {
        return;
    }

    uint16_t lo = UINT16_MAX;
    uint16_t hi = 0;
    uint64_t sum = 0;
    uint64_t sum_sq = 0;

    for (size_t i = 0; i < frames; i++) {
        const uint16_t x = data[i * channels + channel];
        if (x < lo) {
            lo = x;
        }
        if (x > hi) {
            hi = x;
        }
        sum += x;
        sum_sq += (uint64_t)x * x;
    }

    const uint64_t n = frames;
    const uint64_t num = n * sum_sq - sum * sum; /* >= 0 by Cauchy-Schwarz */
    const uint64_t den = n * n;

    out->min = lo;
    out->max = hi;
    out->mean = (uint16_t)((sum + n / 2) / n);
    out->ac_rms = (uint16_t)isqrt64((num + den / 2) / den);
}
