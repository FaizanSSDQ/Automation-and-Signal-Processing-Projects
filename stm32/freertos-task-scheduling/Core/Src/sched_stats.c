/**
 * @file    sched_stats.c
 * @brief   CPU usage per task from FreeRTOS run-time counters, "top" table.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "sched_stats.h"

#include <stdio.h>
#include <string.h>

#include "cmd.h"

size_t ss_compute(const ss_sample_t *prev, size_t n_prev, const ss_sample_t *cur, size_t n_cur,
                  ss_row_t *rows)
{
    uint64_t total = 0;
    for (size_t i = 0; i < n_cur; i++) {
        uint32_t before = 0;
        for (size_t j = 0; j < n_prev; j++) {
            if (prev[j].id == cur[i].id) {
                before = prev[j].runtime;
                break;
            }
        }
        rows[i].task = &cur[i];
        rows[i].cycles = cur[i].runtime - before; /* wraps correctly */
        total += rows[i].cycles;
    }
    for (size_t i = 0; i < n_cur; i++) {
        rows[i].permille = total ? (uint16_t)((rows[i].cycles * 1000ull + total / 2u) / total) : 0u;
    }

    /* Insertion sort: CPU share descending, then priority descending. */
    for (size_t i = 1; i < n_cur; i++) {
        const ss_row_t r = rows[i];
        size_t j = i;
        while (j > 0 && (rows[j - 1].cycles < r.cycles ||
                         (rows[j - 1].cycles == r.cycles && rows[j - 1].task->prio < r.task->prio))) {
            rows[j] = rows[j - 1];
            j--;
        }
        rows[j] = r;
    }
    return n_cur;
}

uint16_t ss_busy_permille(const ss_row_t *rows, size_t n)
{
    uint32_t idle = 0;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(rows[i].task->name, "IDLE") == 0) {
            idle += rows[i].permille;
        }
    }
    return (uint16_t)(idle > 1000u ? 0u : 1000u - idle);
}

size_t ss_format(const ss_row_t *rows, size_t n, uint32_t window_ms, uint32_t heap_free,
                 uint32_t heap_min, char *out, size_t cap)
{
    size_t len = 0;
    int w = snprintf(out, cap, "%-12s %-8s %-5s %7s %11s\r\n", "task", "prio", "state", "cpu", "stack free");
    if (w < 0) {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)w >= cap) {
        return cap - 1; /* truncated, still NUL-terminated */
    }
    len = (size_t)w;
    for (size_t i = 0; i < n && len < cap; i++) {
        const ss_sample_t *t = rows[i].task;
        w = snprintf(out + len, cap - len, "%-12s %-8s %-5c %3u.%u %% %5u words\r\n", t->name,
                     cmd_prio_name(t->prio), t->state, rows[i].permille / 10u, rows[i].permille % 10u,
                     (unsigned)t->stack_free);
        if (w < 0) {
            return len;
        }
        len += (size_t)w;
    }
    if (len < cap) {
        const uint16_t busy = ss_busy_permille(rows, n);
        w = snprintf(out + len, cap - len, "cpu busy %u.%u %% over %lu ms | heap %lu free, %lu minimum\r\n",
                     busy / 10u, busy % 10u, (unsigned long)window_ms, (unsigned long)heap_free,
                     (unsigned long)heap_min);
        if (w > 0) {
            len += (size_t)w;
        }
    }
    return len < cap ? len : cap - 1;
}
