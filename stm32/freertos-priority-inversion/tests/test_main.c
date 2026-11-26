/**
 * Host tests for the priority-inversion demo's portable modules: blocking-time
 * statistics per lock type, their report, and the console command parser.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "inversion_stats.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

/* ---- statistics ------------------------------------------------------------------------- */

static void test_separate_statistics_per_lock(void)
{
    inversion_stats_t s;
    inv_init(&s);
    inv_record(&s, LOCK_SEMAPHORE, 38000, 16);
    inv_record(&s, LOCK_SEMAPHORE, 38010, 16);
    inv_record(&s, LOCK_SEMAPHORE, 37990, 16);
    inv_record(&s, LOCK_MUTEX, 8000, 32);
    inv_record(&s, LOCK_MUTEX, 8004, 32);

    const block_stats_t *sem = &s.kind[LOCK_SEMAPHORE];
    const block_stats_t *mtx = &s.kind[LOCK_MUTEX];
    CHECK(sem->n == 3 && mtx->n == 2);
    CHECK(inv_mean_us(sem) == 38000 && sem->min_us == 37990 && sem->max_us == 38010);
    CHECK(inv_mean_us(mtx) == 8002 && mtx->last_us == 8004);
    CHECK(sem->holder_prio_last == 16 && mtx->holder_prio_last == 32);

    inv_record(&s, LOCK_KINDS, 1, 1);   /* invalid kind is ignored */
    CHECK(sem->n == 3 && mtx->n == 2);
}

static void test_empty_statistics(void)
{
    inversion_stats_t s;
    inv_init(&s);
    CHECK(inv_mean_us(&s.kind[LOCK_MUTEX]) == 0);
    char out[300];
    inv_format(&s, LOCK_MUTEX, out, sizeof(out));
    CHECK(strstr(out, "*mutex            0      0.000      0.000      0.000") != NULL);
}

static void test_report_format(void)
{
    inversion_stats_t s;
    inv_init(&s);
    inv_record(&s, LOCK_SEMAPHORE, 38012, 16);
    inv_record(&s, LOCK_MUTEX, 8001, 32);
    char out[300];
    const size_t n = inv_format(&s, LOCK_SEMAPHORE, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strstr(out, "lock         cycles     min ms    mean ms     max ms   holder prio") == out);
    CHECK(strstr(out, "*semaphore        1     38.012     38.012     38.012            16") != NULL);
    CHECK(strstr(out, " mutex            1      8.001      8.001      8.001            32") != NULL);

    char small[30];
    const size_t m = inv_format(&s, LOCK_MUTEX, small, sizeof(small));
    CHECK(m == sizeof(small) - 1 && small[m] == '\0');

    char line[120];
    inv_format_cycle(LOCK_MUTEX, 8001, 32, line, sizeof(line));
    CHECK(strcmp(line, "high waited 8.001 ms for the mutex (low ran at priority 32)\r\n") == 0);
    CHECK(strcmp(inv_kind_name(LOCK_SEMAPHORE), "semaphore") == 0);
}

/* ---- commands --------------------------------------------------------------------------- */

static cmd_t parse(const char *text)
{
    char line[64];
    snprintf(line, sizeof(line), "%s", text);
    return cmd_parse(line);
}

static void test_commands(void)
{
    CHECK(parse("").type == CMD_EMPTY);
    CHECK(parse("help").type == CMD_HELP);
    CHECK(parse("report").type == CMD_REPORT && parse("r").type == CMD_REPORT);
    CHECK(parse("reset").type == CMD_RESET);

    cmd_t c = parse("mode mutex");
    CHECK(c.type == CMD_MODE && c.value == 1);
    c = parse("MODE Semaphore");
    CHECK(c.type == CMD_MODE && c.value == 0);
    CHECK(parse("mode sem").value == 0 && parse("mode m").value == 1);
    CHECK(parse("mode spinlock").type == CMD_ERROR);
    CHECK(parse("mode").type == CMD_ERROR);

    c = parse("medium 45");
    CHECK(c.type == CMD_MEDIUM && c.value == 45);
    CHECK(parse("medium 0").type == CMD_MEDIUM);
    CHECK(parse("medium 61").type == CMD_ERROR);

    c = parse("hold 20");
    CHECK(c.type == CMD_HOLD && c.value == 20);
    CHECK(parse("hold 0").type == CMD_ERROR);
    CHECK(parse("hold 21").type == CMD_ERROR);

    CHECK(parse("watch on").type == CMD_WATCH && parse("watch on").value == 1);
    CHECK(parse("watch").type == CMD_ERROR);
    CHECK(parse("lock").type == CMD_ERROR);
}

int main(void)
{
    test_separate_statistics_per_lock();
    test_empty_statistics();
    test_report_format();
    test_commands();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
