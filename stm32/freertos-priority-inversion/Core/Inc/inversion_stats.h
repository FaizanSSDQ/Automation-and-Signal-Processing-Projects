/**
 * @file    inversion_stats.h
 * @brief   Blocking-time statistics of the high-priority task (no RTOS dependency).
 *
 * Every cycle the high-priority task records how long it waited for the shared
 * lock (from the request to the moment it owned it) and the priority the
 * low-priority holder was running at during that time. Statistics are kept
 * separately for the binary semaphore and for the mutex, so the two can be
 * compared side by side.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef INVERSION_STATS_H
#define INVERSION_STATS_H

#include <stddef.h>
#include <stdint.h>

typedef enum { LOCK_SEMAPHORE = 0, LOCK_MUTEX = 1, LOCK_KINDS } lock_kind_t;

typedef struct {
    uint32_t n;
    uint64_t sum_us;
    uint32_t min_us, max_us, last_us;
    uint8_t holder_prio_last;     /* low task's priority while it held the lock */
} block_stats_t;

typedef struct {
    block_stats_t kind[LOCK_KINDS];
} inversion_stats_t;

void inv_init(inversion_stats_t *s);

/** Record one cycle: the high task waited @p wait_us for a lock of type @p kind. */
void inv_record(inversion_stats_t *s, lock_kind_t kind, uint32_t wait_us, uint8_t holder_prio);

uint32_t inv_mean_us(const block_stats_t *b);

const char *inv_kind_name(lock_kind_t kind);

/** Two-line comparison table. @return characters written. */
size_t inv_format(const inversion_stats_t *s, lock_kind_t active, char *out, size_t cap);

/** One line for a single cycle ("cycle: high waited 38.012 ms (semaphore) ..."). */
size_t inv_format_cycle(lock_kind_t kind, uint32_t wait_us, uint8_t holder_prio, char *out, size_t cap);

#endif /* INVERSION_STATS_H */
