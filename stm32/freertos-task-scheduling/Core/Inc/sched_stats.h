/**
 * @file    sched_stats.h
 * @brief   CPU usage per task from two snapshots of FreeRTOS run-time counters,
 *          and the "top"-style table that shows it (no RTOS dependency).
 *
 * FreeRTOS keeps a run-time counter per task, advanced while the task runs.
 * Here it counts CPU cycles (DWT->CYCCNT), so it wraps every 67 s at 64 MHz.
 * Differences between two snapshots taken less than 67 s apart are still
 * exact with unsigned arithmetic, so CPU usage is computed per window.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef SCHED_STATS_H
#define SCHED_STATS_H

#include <stddef.h>
#include <stdint.h>

#define SS_NAME_LEN   16
#define SS_MAX_TASKS  12

typedef struct {
    uint32_t id;               /* FreeRTOS task number, unique per task */
    char name[SS_NAME_LEN];
    uint8_t prio;
    char state;                /* R running, r ready, B blocked, S suspended, D deleted */
    uint32_t runtime;          /* run-time counter (cycles, wrapping) */
    uint16_t stack_free;       /* minimum free stack ever, in words */
} ss_sample_t;

typedef struct {
    const ss_sample_t *task;
    uint32_t cycles;           /* run time within the window */
    uint16_t permille;         /* share of the window, 0..1000 */
} ss_row_t;

/**
 * Per-task CPU share between snapshots @p prev and @p cur (matched by id).
 * A task missing from @p prev is counted from zero. Rows are sorted by CPU
 * share, then by priority. @return the number of rows (= @p n_cur).
 */
size_t ss_compute(const ss_sample_t *prev, size_t n_prev, const ss_sample_t *cur, size_t n_cur,
                  ss_row_t *rows);

/** CPU busy share in permille: everything except the task named "IDLE". */
uint16_t ss_busy_permille(const ss_row_t *rows, size_t n);

/** Format the table plus a summary line. @return characters written. */
size_t ss_format(const ss_row_t *rows, size_t n, uint32_t window_ms, uint32_t heap_free,
                 uint32_t heap_min, char *out, size_t cap);

#endif /* SCHED_STATS_H */
