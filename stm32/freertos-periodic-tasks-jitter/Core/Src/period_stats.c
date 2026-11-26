/**
 * @file    period_stats.c
 * @brief   Timing statistics of a periodic task from its release timestamps.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "period_stats.h"

#include <stdio.h>
#include <string.h>

#define BAR_WIDTH 30u

void ps_init(period_stats_t *s, uint32_t nominal_us, uint32_t cycles_per_us)
{
    memset(s, 0, sizeof(*s));
    s->nominal_us = nominal_us;
    s->cycles_per_us = cycles_per_us ? cycles_per_us : 1u;
    s->min_us = UINT32_MAX;
}

uint32_t ps_isqrt(uint64_t v)
{
    uint64_t r = 0, bit = 1ull << 62;
    while (bit > v) {
        bit >>= 2;
    }
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)r;
}

int ps_bin(const period_stats_t *s, uint32_t period_us)
{
    const int32_t dev = (int32_t)period_us - (int32_t)s->nominal_us;
    if (dev < PS_HIST_FIRST_US) {
        return 0;
    }
    const int32_t bin = (dev - PS_HIST_FIRST_US) / PS_HIST_BIN_US;
    return bin >= PS_HIST_BINS ? PS_HIST_BINS - 1 : (int)bin;
}

void ps_release(period_stats_t *s, uint32_t now)
{
    if (!s->started) {
        s->started = true;
        s->first = now;
        s->last = now;
        return;
    }
    const uint32_t cycles = now - s->last;   /* exact across counter wrap */
    const uint32_t period_us = cycles / s->cycles_per_us;
    s->last = now;
    s->n++;
    s->sum_cycles += cycles;
    s->sum_us += period_us;
    s->sum_sq_us += (uint64_t)period_us * period_us;
    if (period_us < s->min_us) {
        s->min_us = period_us;
    }
    if (period_us > s->max_us) {
        s->max_us = period_us;
    }
    s->hist[ps_bin(s, period_us)]++;

    /* Time since the first release, accumulated in exact cycles (immune to
     * counter wrap and to rounding), against the ideal n x nominal. */
    const int64_t ideal = (int64_t)s->n * s->nominal_us * s->cycles_per_us;
    s->late_us = ((int64_t)s->sum_cycles - ideal) / (int64_t)s->cycles_per_us;
    if (s->late_us > s->late_max_us) {
        s->late_max_us = s->late_us;
    }
}

uint32_t ps_mean_us(const period_stats_t *s)
{
    if (s->n == 0u) {
        return 0u;
    }
    const uint64_t div = (uint64_t)s->n * s->cycles_per_us;
    return (uint32_t)((s->sum_cycles + div / 2u) / div);
}

uint32_t ps_stddev_us(const period_stats_t *s)
{
    if (s->n < 2u) {
        return 0u;
    }
    /* Variance = E[x^2] - E[x]^2, computed as (n*sum(x^2) - sum(x)^2) / n^2. */
    const uint64_t n = s->n;
    const uint64_t a = s->sum_sq_us * n;
    const uint64_t b = s->sum_us * s->sum_us;
    return a > b ? ps_isqrt((a - b) / (n * n)) : 0u;
}

uint32_t ps_rate_x100(const period_stats_t *s)
{
    return s->n && s->sum_cycles
               ? (uint32_t)((uint64_t)s->n * s->cycles_per_us * 100000000u / s->sum_cycles)
               : 0u;
}

size_t ps_format(const period_stats_t *s, const char *name, char *out, size_t cap)
{
    size_t len = 0;
    int w;
    const uint32_t mean = ps_mean_us(s);
    const uint32_t rate = ps_rate_x100(s);
    const int64_t err_permille = s->nominal_us && mean
                                     ? ((int64_t)mean - s->nominal_us) * 1000 / s->nominal_us
                                     : 0;
    const char sign = err_permille < 0 ? '-' : '+';
    const int64_t eabs = err_permille < 0 ? -err_permille : err_permille;

    w = snprintf(out, cap,
                 "%s: %lu periods, mean %lu us (nominal %lu), min %lu, max %lu, jitter (sd) %lu us\r\n"
                 "  rate %lu.%02lu /s, period error %c%ld.%ld %%, lateness now %ld us, worst %ld us\r\n",
                 name, (unsigned long)s->n, (unsigned long)mean, (unsigned long)s->nominal_us,
                 (unsigned long)(s->n ? s->min_us : 0u), (unsigned long)s->max_us,
                 (unsigned long)ps_stddev_us(s), (unsigned long)(rate / 100u), (unsigned long)(rate % 100u),
                 sign, (long)(eabs / 10), (long)(eabs % 10), (long)s->late_us, (long)s->late_max_us);
    if (w < 0) {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)w >= cap) {
        return cap - 1;
    }
    len = (size_t)w;

    uint32_t peak = 0;
    for (int i = 0; i < PS_HIST_BINS; i++) {
        peak = s->hist[i] > peak ? s->hist[i] : peak;
    }
    for (int i = 0; i < PS_HIST_BINS && peak; i++) {
        if (s->hist[i] == 0u) {
            continue;
        }
        /* Bin i covers [lo, lo + width) of period deviation; the outer bins also
         * take everything beyond them. */
        const int32_t lo = PS_HIST_FIRST_US + i * PS_HIST_BIN_US;
        const char *rel = (i == 0) ? "< " : (i == PS_HIST_BINS - 1 ? ">=" : "  ");
        const long edge = (i == 0) ? (long)(lo + PS_HIST_BIN_US) : (long)lo;
        char bar[BAR_WIDTH + 1];
        const uint32_t nbar = (s->hist[i] * BAR_WIDTH + peak - 1u) / peak;
        memset(bar, '#', nbar);
        bar[nbar] = '\0';
        w = snprintf(out + len, cap - len, "  %s%+6ld us | %6lu %s\r\n", rel, edge,
                     (unsigned long)s->hist[i], bar);
        if (w < 0) {
            break;
        }
        if ((size_t)w >= cap - len) {
            return cap - 1;
        }
        len += (size_t)w;
    }
    return len;
}
