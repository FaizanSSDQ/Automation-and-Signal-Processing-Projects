/**
 * Host tests for the scheduling demo's portable modules: the console command
 * parser and the per-task CPU statistics behind the "top" table.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <string.h>

#include "cmd.h"
#include "sched_stats.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

static const char *const names[] = {"worker", "blink_fast", "blink_slow"};

static cmd_t parse(const char *text)
{
    char line[64];
    snprintf(line, sizeof(line), "%s", text);
    return cmd_parse(line, names, 3);
}

/* ---- commands --------------------------------------------------------------------------- */

static void test_commands(void)
{
    CHECK(parse("").type == CMD_EMPTY);
    CHECK(parse("   ").type == CMD_EMPTY);
    CHECK(parse("help").type == CMD_HELP);
    CHECK(parse("TOP").type == CMD_TOP);

    cmd_t c = parse("watch on");
    CHECK(c.type == CMD_WATCH && c.value == 1);
    CHECK(parse("watch off").value == 0);
    CHECK(parse("watch maybe").type == CMD_ERROR);

    c = parse("load 75");
    CHECK(c.type == CMD_LOAD && c.value == 75);
    CHECK(parse("load 101").type == CMD_ERROR);
    CHECK(parse("load -1").type == CMD_ERROR);
    CHECK(parse("load 5x").type == CMD_ERROR);
    CHECK(parse("load").type == CMD_ERROR);

    c = parse("prio worker above");
    CHECK(c.type == CMD_PRIO && c.task == 0 && c.value == PRIO_ABOVE_NORMAL);
    c = parse("prio Blink_Slow low");
    CHECK(c.type == CMD_PRIO && c.task == 2 && c.value == PRIO_LOW);
    CHECK(parse("prio worker high").type == CMD_ERROR);     /* above the console: not allowed */
    CHECK(parse("prio console low").type == CMD_ERROR);     /* console is not addressable */
    CHECK(parse("prio worker").type == CMD_ERROR);

    c = parse("suspend blink_fast");
    CHECK(c.type == CMD_SUSPEND && c.task == 1);
    c = parse("resume blink_fast");
    CHECK(c.type == CMD_RESUME && c.task == 1);
    CHECK(parse("suspend nobody").type == CMD_ERROR);
    CHECK(parse("reboot").type == CMD_ERROR);
}

static void test_priority_names(void)
{
    CHECK(cmd_prio_from_name("low") == PRIO_LOW);
    CHECK(cmd_prio_from_name("below") == PRIO_BELOW_NORMAL);
    CHECK(cmd_prio_from_name("normal") == PRIO_NORMAL);
    CHECK(cmd_prio_from_name("above") == PRIO_ABOVE_NORMAL);
    CHECK(cmd_prio_from_name("realtime") == -1);
    CHECK(strcmp(cmd_prio_name(PRIO_HIGH), "high") == 0);
    CHECK(strcmp(cmd_prio_name(PRIO_IDLE), "idle") == 0);
    CHECK(strcmp(cmd_prio_name(2), "p2") == 0);
}

/* ---- statistics ------------------------------------------------------------------------- */

static ss_sample_t sample(uint32_t id, const char *name, uint8_t prio, char state, uint32_t runtime)
{
    ss_sample_t s;
    memset(&s, 0, sizeof(s));
    s.id = id;
    snprintf(s.name, sizeof(s.name), "%s", name);
    s.prio = prio;
    s.state = state;
    s.runtime = runtime;
    s.stack_free = 50;
    return s;
}

static void test_cpu_shares_and_sorting(void)
{
    /* 64 M cycles in one second: worker 30 %, console 1 %, idle the rest. */
    const ss_sample_t prev[] = {
        sample(1, "console", PRIO_HIGH, 'R', 1000000),
        sample(2, "worker", PRIO_BELOW_NORMAL, 'B', 5000000),
        sample(3, "IDLE", PRIO_IDLE, 'r', 90000000),
    };
    const ss_sample_t cur[] = {
        sample(1, "console", PRIO_HIGH, 'R', 1000000 + 640000),
        sample(2, "worker", PRIO_BELOW_NORMAL, 'B', 5000000 + 19200000),
        sample(3, "IDLE", PRIO_IDLE, 'r', 90000000 + 44160000),
    };
    ss_row_t rows[3];
    CHECK(ss_compute(prev, 3, cur, 3, rows) == 3);
    CHECK(strcmp(rows[0].task->name, "IDLE") == 0 && rows[0].permille == 690);
    CHECK(strcmp(rows[1].task->name, "worker") == 0 && rows[1].permille == 300);
    CHECK(strcmp(rows[2].task->name, "console") == 0 && rows[2].permille == 10);
    CHECK(ss_busy_permille(rows, 3) == 310);
}

static void test_counter_wrap_and_new_tasks(void)
{
    /* The worker's cycle counter wraps past 2^32 within the window. */
    const ss_sample_t prev[] = {
        sample(2, "worker", PRIO_NORMAL, 'B', 0xFFFFF000u),
        sample(3, "IDLE", PRIO_IDLE, 'r', 100),
    };
    const ss_sample_t cur[] = {
        sample(2, "worker", PRIO_NORMAL, 'B', 0x00001000u),  /* +0x2000 cycles */
        sample(3, "IDLE", PRIO_IDLE, 'r', 100 + 0x2000),
        sample(9, "new", PRIO_LOW, 'B', 0x4000),             /* created in this window */
    };
    ss_row_t rows[3];
    ss_compute(prev, 2, cur, 3, rows);
    CHECK(rows[0].cycles == 0x4000 && strcmp(rows[0].task->name, "new") == 0);
    CHECK(rows[1].cycles == 0x2000 && rows[2].cycles == 0x2000);
    CHECK(rows[1].task->prio > rows[2].task->prio); /* tie broken by priority */
    CHECK(rows[0].permille == 500 && rows[1].permille == 250);
}

static void test_empty_window(void)
{
    const ss_sample_t s[] = {sample(1, "IDLE", PRIO_IDLE, 'R', 5)};
    ss_row_t rows[1];
    ss_compute(s, 1, s, 1, rows);
    CHECK(rows[0].permille == 0);
}

static void test_table_format(void)
{
    const ss_sample_t prev[] = {sample(1, "worker", PRIO_ABOVE_NORMAL, 'R', 0),
                                sample(2, "IDLE", PRIO_IDLE, 'r', 0)};
    const ss_sample_t cur[] = {sample(1, "worker", PRIO_ABOVE_NORMAL, 'R', 999),
                               sample(2, "IDLE", PRIO_IDLE, 'r', 1)};
    ss_row_t rows[2];
    char out[512];
    ss_compute(prev, 2, cur, 2, rows);
    const size_t n = ss_format(rows, 2, 1000, 3000, 2800, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strstr(out, "task") == out);
    CHECK(strstr(out, "worker       above    R      99.9 %    50 words") != NULL);
    CHECK(strstr(out, "cpu busy 99.9 % over 1000 ms | heap 3000 free, 2800 minimum") != NULL);

    char small[40];
    const size_t m = ss_format(rows, 2, 1000, 3000, 2800, small, sizeof(small));
    CHECK(m < sizeof(small) && small[m] == '\0');
}

int main(void)
{
    test_commands();
    test_priority_names();
    test_cpu_shares_and_sorting();
    test_counter_wrap_and_new_tasks();
    test_empty_window();
    test_table_format();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
