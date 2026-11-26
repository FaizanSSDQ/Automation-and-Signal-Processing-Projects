/**
 * Host tests for the periodic-task demo's portable modules: period and jitter
 * statistics from release timestamps, and the console command parser.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "period_stats.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

#define CPU 64u          /* cycles per microsecond */
#define NOMINAL 10000u   /* 10 ms */

/* ---- statistics ------------------------------------------------------------------------- */

static void test_exact_period(void)
{
    period_stats_t s;
    ps_init(&s, NOMINAL, CPU);
    uint32_t t = 0xFFF00000u; /* start close to the counter wrap */
    for (int i = 0; i <= 1000; i++) {
        ps_release(&s, t);
        t += NOMINAL * CPU;
    }
    CHECK(s.n == 1000);
    CHECK(ps_mean_us(&s) == NOMINAL);
    CHECK(s.min_us == NOMINAL && s.max_us == NOMINAL);
    CHECK(ps_stddev_us(&s) == 0);
    CHECK(s.late_us == 0 && s.late_max_us == 0);
    CHECK(ps_rate_x100(&s) == 10000);               /* 100.00 per second */
    CHECK(s.hist[ps_bin(&s, NOMINAL)] == 1000);
}

static void test_relative_delay_drifts(void)
{
    /* vTaskDelay(10) after 2 ms of work: every period is 12 ms. */
    period_stats_t s;
    ps_init(&s, NOMINAL, CPU);
    uint32_t t = 0;
    for (int i = 0; i <= 500; i++) {
        ps_release(&s, t);
        t += 12000u * CPU;
    }
    CHECK(ps_mean_us(&s) == 12000);
    CHECK(s.late_us == 500 * 2000);                  /* 1 s behind after 500 periods */
    CHECK(ps_rate_x100(&s) == 8333);                 /* 83.33 per second instead of 100 */
    CHECK(s.hist[ps_bin(&s, 12000)] == 500);
}

static void test_jitter_with_bounded_lateness(void)
{
    /* Fixed-rate releases, every third one delayed by 3 ms (then on time again). */
    period_stats_t s;
    ps_init(&s, NOMINAL, CPU);
    for (uint32_t k = 0; k <= 300; k++) {
        const uint32_t ideal = k * NOMINAL * CPU;
        ps_release(&s, ideal + ((k % 3u == 1u) ? 3000u * CPU : 0u));
    }
    CHECK(s.n == 300);
    CHECK(ps_mean_us(&s) == NOMINAL);                /* no drift */
    CHECK(s.min_us == 7000 && s.max_us == 13000);
    CHECK(s.late_max_us == 3000);                    /* bounded lateness */
    /* Periods: 13 ms, 7 ms, 10 ms repeating -> sd = sqrt(6) ms = 2449 us */
    CHECK(ps_stddev_us(&s) >= 2448 && ps_stddev_us(&s) <= 2450);
}

static void test_histogram_bins(void)
{
    period_stats_t s;
    ps_init(&s, NOMINAL, CPU);
    CHECK(ps_bin(&s, 0) == 0);                       /* far below: first bin */
    CHECK(ps_bin(&s, 8000) == 0);                    /* -2000 us */
    CHECK(ps_bin(&s, 9999) == 3);                    /* -1 us: [-500, 0) */
    CHECK(ps_bin(&s, 10000) == 4);                   /* 0: [0, 500) */
    CHECK(ps_bin(&s, 10499) == 4);
    CHECK(ps_bin(&s, 10500) == 5);
    CHECK(ps_bin(&s, 100000) == PS_HIST_BINS - 1);   /* far above: last bin */
}

static void test_isqrt(void)
{
    CHECK(ps_isqrt(0) == 0 && ps_isqrt(1) == 1 && ps_isqrt(15) == 3 && ps_isqrt(16) == 4);
    CHECK(ps_isqrt(6000000ull) == 2449);
    CHECK(ps_isqrt(0xFFFFFFFFFFFFFFFFull) == 0xFFFFFFFFu);
}

static void test_report_format(void)
{
    period_stats_t s;
    char out[1200];
    ps_init(&s, NOMINAL, CPU);
    CHECK(ps_format(&s, "empty", out, sizeof(out)) > 0 && strstr(out, "0 periods") != NULL);

    uint32_t t = 0;
    for (int i = 0; i <= 100; i++) {
        ps_release(&s, t);
        t += (i % 2 ? 11000u : 10000u) * CPU;
    }
    const size_t n = ps_format(&s, "absolute", out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strstr(out, "absolute: 100 periods, mean 10500 us (nominal 10000), min 10000, max 11000") != NULL);
    CHECK(strstr(out, "period error +5.0 %") != NULL);
    CHECK(strstr(out, "      +0 us |     50 ##############################") != NULL);
    CHECK(strstr(out, "  +1000 us |     50 ##############################") != NULL);

    char small[60];
    const size_t m = ps_format(&s, "absolute", small, sizeof(small));
    CHECK(m == sizeof(small) - 1 && small[m] == '\0');
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
    CHECK(parse("Report").type == CMD_REPORT);
    CHECK(parse("r").type == CMD_REPORT);
    CHECK(parse("reset").type == CMD_RESET);
    CHECK(parse("watch on").type == CMD_WATCH && parse("watch on").value == 1);
    CHECK(parse("watch off").value == 0);
    CHECK(parse("watch").type == CMD_ERROR);
    CHECK(parse("work 4").type == CMD_WORK && parse("work 4").value == 4);
    CHECK(parse("work 9").type == CMD_ERROR);
    CHECK(parse("burst 0").type == CMD_BURST && parse("burst 0").value == 0);
    CHECK(parse("burst 9").value == 9);
    CHECK(parse("burst 10").type == CMD_ERROR);
    CHECK(parse("burst x").type == CMD_ERROR);
    CHECK(parse("jump").type == CMD_ERROR);
}

int main(void)
{
    test_exact_period();
    test_relative_delay_drifts();
    test_jitter_with_bounded_lateness();
    test_histogram_bins();
    test_isqrt();
    test_report_format();
    test_commands();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
