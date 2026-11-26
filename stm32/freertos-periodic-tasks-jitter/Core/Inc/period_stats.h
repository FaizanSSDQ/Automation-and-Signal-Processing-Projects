/**
 * @file    period_stats.h
 * @brief   Timing statistics of a periodic task from its release timestamps
 *          (no RTOS dependency).
 *
 * Each release (the moment the task wakes for a new cycle) is stamped with the
 * DWT cycle counter. From consecutive stamps the module keeps:
 *   - period count, mean, minimum, maximum and standard deviation (jitter)
 *   - a histogram of the period's deviation from nominal
 *   - lateness against an ideal fixed-rate schedule (first release + k x nominal),
 *     which stays bounded for vTaskDelayUntil and grows without bound (drift)
 *     for vTaskDelay.
 * The 32-bit cycle counter wraps every 67 s; only differences are used, so the
 * statistics stay exact as long as one period is shorter than that.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef PERIOD_STATS_H
#define PERIOD_STATS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PS_HIST_BINS      16
#define PS_HIST_FIRST_US  (-2000)   /* lower edge of the first bin (deviation from nominal) */
#define PS_HIST_BIN_US    500

typedef struct {
    uint32_t nominal_us;
    uint32_t cycles_per_us;
    bool started;
    uint32_t first;          /* cycle stamp of the first release */
    uint32_t last;           /* cycle stamp of the previous release */
    uint32_t n;              /* number of periods measured */
    uint64_t sum_cycles;     /* exact total of all periods */
    uint64_t sum_us;
    uint64_t sum_sq_us;
    uint32_t min_us, max_us;
    int64_t late_us;         /* lateness of the latest release vs the ideal schedule */
    int64_t late_max_us;
    uint32_t hist[PS_HIST_BINS];
} period_stats_t;

void ps_init(period_stats_t *s, uint32_t nominal_us, uint32_t cycles_per_us);

/** Record a release at cycle counter value @p now. */
void ps_release(period_stats_t *s, uint32_t now);

uint32_t ps_mean_us(const period_stats_t *s);

/** Standard deviation of the period (the jitter), in microseconds. */
uint32_t ps_stddev_us(const period_stats_t *s);

/** Releases per second times 100 (e.g. 9990 = 99.90 /s). */
uint32_t ps_rate_x100(const period_stats_t *s);

/** Index of the histogram bin for a period, clamped to the outer bins. */
int ps_bin(const period_stats_t *s, uint32_t period_us);

/** Integer square root (floor). */
uint32_t ps_isqrt(uint64_t v);

/** Multi-line report with an ASCII histogram. @return characters written. */
size_t ps_format(const period_stats_t *s, const char *name, char *out, size_t cap);

#endif /* PERIOD_STATS_H */
