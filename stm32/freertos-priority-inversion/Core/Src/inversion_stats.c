/**
 * @file    inversion_stats.c
 * @brief   Blocking-time statistics of the high-priority task.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "inversion_stats.h"

#include <stdio.h>
#include <string.h>

void inv_init(inversion_stats_t *s)
{
    memset(s, 0, sizeof(*s));
    for (int k = 0; k < LOCK_KINDS; k++) {
        s->kind[k].min_us = UINT32_MAX;
    }
}

void inv_record(inversion_stats_t *s, lock_kind_t kind, uint32_t wait_us, uint8_t holder_prio)
{
    if (kind >= LOCK_KINDS) {
        return;
    }
    block_stats_t *b = &s->kind[kind];
    b->n++;
    b->sum_us += wait_us;
    b->last_us = wait_us;
    b->holder_prio_last = holder_prio;
    if (wait_us < b->min_us) {
        b->min_us = wait_us;
    }
    if (wait_us > b->max_us) {
        b->max_us = wait_us;
    }
}

uint32_t inv_mean_us(const block_stats_t *b)
{
    return b->n ? (uint32_t)((b->sum_us + b->n / 2u) / b->n) : 0u;
}

const char *inv_kind_name(lock_kind_t kind)
{
    return kind == LOCK_MUTEX ? "mutex" : "semaphore";
}

/* "38.012" from microseconds */
static void ms3(char *out, size_t cap, uint32_t us)
{
    snprintf(out, cap, "%lu.%03lu", (unsigned long)(us / 1000u), (unsigned long)(us % 1000u));
}

size_t inv_format(const inversion_stats_t *s, lock_kind_t active, char *out, size_t cap)
{
    int w = snprintf(out, cap, "%-11s %7s %10s %10s %10s %13s\r\n", "lock", "cycles", "min ms", "mean ms",
                     "max ms", "holder prio");
    if (w < 0) {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)w >= cap) {
        return cap - 1;
    }
    size_t len = (size_t)w;
    for (int k = 0; k < LOCK_KINDS; k++) {
        const block_stats_t *b = &s->kind[k];
        char mn[16], me[16], mx[16];
        ms3(mn, sizeof(mn), b->n ? b->min_us : 0u);
        ms3(me, sizeof(me), inv_mean_us(b));
        ms3(mx, sizeof(mx), b->max_us);
        w = snprintf(out + len, cap - len, "%c%-10s %7lu %10s %10s %10s %13u\r\n", k == (int)active ? '*' : ' ',
                     inv_kind_name((lock_kind_t)k), (unsigned long)b->n, mn, me, mx, (unsigned)b->holder_prio_last);
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

size_t inv_format_cycle(lock_kind_t kind, uint32_t wait_us, uint8_t holder_prio, char *out, size_t cap)
{
    char t[16];
    ms3(t, sizeof(t), wait_us);
    const int w = snprintf(out, cap, "high waited %s ms for the %s (low ran at priority %u)\r\n", t,
                           inv_kind_name(kind), (unsigned)holder_prio);
    if (w < 0) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)w >= cap ? cap - 1 : (size_t)w;
}
