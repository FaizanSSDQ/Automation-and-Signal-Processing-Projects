/**
 * Host tests for the logic analyzer core: trigger matching, the pre-trigger
 * capture engine running against a simulated circular DMA, the DMA write
 * counter, and the SUMP protocol parser.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "la_capture.h"
#include "la_config.h"
#include "la_trigger.h"
#include "sump.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

/* ---- simulated input signal and DMA ----------------------------------------------------- */

/* Channels 0..6 carry a counter so every sample is identifiable; channel 7 is a
 * test line that the scenarios drive. */
typedef struct {
    uint64_t rise_at[4]; /* channel 7 goes high at these sample indices ... */
    uint64_t fall_at[4]; /* ... and low again at these */
    int n;
} line_t;

static uint8_t sample_at(const line_t *l, uint64_t i)
{
    uint8_t ch7 = 0;
    for (int k = 0; k < l->n; k++) {
        if (i >= l->rise_at[k] && i < l->fall_at[k]) {
            ch7 = 1;
        }
    }
    return (uint8_t)((i & 0x7Fu) | (uint8_t)(ch7 << 7));
}

typedef struct {
    la_state_t state;
    uint8_t out[LA_MAX_SAMPLES];
    uint64_t trig_at;
} result_t;

static uint8_t ring[LA_RING_SIZE];

/**
 * Run a capture: the "DMA" stores 'chunk' samples between polls; after the
 * engine reports LA_DONE, 'extra' more samples arrive before the timer stops.
 */
static void run(result_t *r, const line_t *line, uint32_t depth, uint32_t pre,
                const la_trigger_t *trig, uint32_t chunk, uint32_t extra, uint64_t limit)
{
    la_capture_t c;
    uint64_t written = 0;
    memset(r, 0, sizeof(*r));
    memset(ring, 0, sizeof(ring));

    if (!la_capture_start(&c, LA_RING_SIZE, depth, pre, trig)) {
        r->state = LA_IDLE;
        return;
    }
    la_state_t st = LA_PRE_TRIGGER;
    while (st != LA_DONE && st != LA_OVERRUN && written < limit) {
        for (uint32_t k = 0; k < chunk; k++, written++) {
            ring[written % LA_RING_SIZE] = sample_at(line, written);
        }
        st = la_capture_poll(&c, ring, written);
    }
    if (st == LA_DONE) {
        for (uint32_t k = 0; k < extra; k++, written++) {
            ring[written % LA_RING_SIZE] = sample_at(line, written);
        }
        st = la_capture_finish(&c, written);
    }
    r->state = st;
    r->trig_at = c.trig_at;
    if (st == LA_DONE) {
        uint32_t idx = la_capture_first_index(&c);
        for (uint32_t k = 0; k < depth; k++) {
            r->out[k] = ring[idx];
            idx = (idx + 1 == LA_RING_SIZE) ? 0 : idx + 1;
        }
    }
}

/* True if the captured window holds exactly the samples around the trigger. */
static int window_ok(const result_t *r, const line_t *line, uint32_t depth, uint32_t pre)
{
    for (uint32_t k = 0; k < depth; k++) {
        if (r->out[k] != sample_at(line, r->trig_at - pre + k)) {
            return 0;
        }
    }
    return 1;
}

/* ---- trigger ---------------------------------------------------------------------------- */

static void test_trigger_match(void)
{
    la_trigger_t t = {0};
    CHECK(la_trigger_is_immediate(&t));
    CHECK(la_trigger_match(&t, 0x00, 0x5A));

    CHECK(la_trigger_set_channel(&t, 2, '1'));
    CHECK(la_trigger_set_channel(&t, 3, '0'));
    CHECK(!la_trigger_is_immediate(&t) && !la_trigger_uses_edges(&t));
    CHECK(la_trigger_match(&t, 0x00, 0x04));
    CHECK(!la_trigger_match(&t, 0x00, 0x0C));
    CHECK(!la_trigger_match(&t, 0x00, 0x00));

    la_trigger_t e = {0};
    CHECK(la_trigger_set_channel(&e, 0, 'r'));
    CHECK(la_trigger_uses_edges(&e));
    CHECK(la_trigger_match(&e, 0x00, 0x01));
    CHECK(!la_trigger_match(&e, 0x01, 0x01)); /* high but not rising */
    CHECK(la_trigger_set_channel(&e, 1, 'f'));
    CHECK(la_trigger_match(&e, 0x02, 0x01));  /* ch0 rises while ch1 falls */
    CHECK(!la_trigger_match(&e, 0x00, 0x01));

    /* A new condition replaces the old one on that channel. */
    CHECK(la_trigger_set_channel(&e, 0, 'x'));
    CHECK(e.rise_mask == 0 && e.fall_mask == 0x02);
    CHECK(!la_trigger_set_channel(&e, 8, '1'));
    CHECK(!la_trigger_set_channel(&e, 0, 'q'));
}

/* ---- capture engine --------------------------------------------------------------------- */

static void test_immediate_capture(void)
{
    line_t line = {.n = 0};
    la_trigger_t none = {0};
    result_t r;
    run(&r, &line, 4096, 0, &none, 64, 10, 100000);
    CHECK(r.state == LA_DONE);
    CHECK(r.trig_at == 0);
    CHECK(window_ok(&r, &line, 4096, 0));

    /* Immediate trigger with pre-trigger: window starts at sample 0. */
    run(&r, &line, 4096, 1000, &none, 64, 10, 100000);
    CHECK(r.state == LA_DONE && r.trig_at == 1000);
    CHECK(window_ok(&r, &line, 4096, 1000));
}

static void test_level_trigger_any_poll_size(void)
{
    line_t line = {.rise_at = {20000}, .fall_at = {20500}, .n = 1};
    la_trigger_t t = {0};
    la_trigger_set_channel(&t, 7, '1');
    const uint32_t chunks[] = {1, 37, 1000, 4096};
    for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
        result_t r;
        run(&r, &line, 2048, 512, &t, chunks[i], 0, 100000);
        CHECK(r.state == LA_DONE);
        CHECK(r.trig_at == 20000);
        CHECK(window_ok(&r, &line, 2048, 512));
        CHECK((r.out[511] & 0x80) == 0 && (r.out[512] & 0x80) != 0); /* trigger at index pre */
    }
}

static void test_edge_triggers(void)
{
    line_t line = {.rise_at = {3000}, .fall_at = {7000}, .n = 1};
    la_trigger_t rise = {0}, fall = {0};
    la_trigger_set_channel(&rise, 7, 'r');
    la_trigger_set_channel(&fall, 7, 'f');
    result_t r;

    run(&r, &line, 1024, 100, &rise, 50, 0, 100000);
    CHECK(r.state == LA_DONE && r.trig_at == 3000);
    CHECK(window_ok(&r, &line, 1024, 100));

    run(&r, &line, 1024, 100, &fall, 50, 0, 100000);
    CHECK(r.state == LA_DONE && r.trig_at == 7000);
    CHECK(window_ok(&r, &line, 1024, 100));
}

static void test_no_trigger_inside_pretrigger(void)
{
    /* The line is high from the start: a level trigger may only fire once the
     * pre-trigger samples exist, i.e. at sample 'pre'. */
    line_t high = {.rise_at = {0}, .fall_at = {1000000}, .n = 1};
    la_trigger_t lvl = {0};
    la_trigger_set_channel(&lvl, 7, '1');
    result_t r;
    run(&r, &high, 2048, 600, &lvl, 16, 0, 100000);
    CHECK(r.state == LA_DONE && r.trig_at == 600);
    CHECK(window_ok(&r, &high, 2048, 600));

    /* An edge before the pre-trigger is filled is ignored; the next one counts. */
    line_t two = {.rise_at = {50, 5000}, .fall_at = {80, 5100}, .n = 2};
    la_trigger_t rise = {0};
    la_trigger_set_channel(&rise, 7, 'r');
    run(&r, &two, 2048, 600, &rise, 16, 0, 100000);
    CHECK(r.state == LA_DONE && r.trig_at == 5000);

    /* An edge exactly on the first eligible sample is seen, because the
     * sample before it seeds the edge detector. */
    line_t at_pre = {.rise_at = {600}, .fall_at = {700}, .n = 1};
    run(&r, &at_pre, 2048, 600, &rise, 1000, 0, 100000);
    CHECK(r.state == LA_DONE && r.trig_at == 600);
    CHECK(window_ok(&r, &at_pre, 2048, 600));
}

static void test_trigger_after_many_ring_laps(void)
{
    line_t line = {.rise_at = {1000003}, .fall_at = {1000010}, .n = 1};
    la_trigger_t rise = {0};
    la_trigger_set_channel(&rise, 7, 'r');
    result_t r;
    run(&r, &line, LA_MAX_SAMPLES, LA_MAX_SAMPLES / 2, &rise, 333, 100, 2000000);
    CHECK(r.state == LA_DONE && r.trig_at == 1000003);
    CHECK(window_ok(&r, &line, LA_MAX_SAMPLES, LA_MAX_SAMPLES / 2));
}

static void test_overrun_detection(void)
{
    line_t line = {.rise_at = {50000}, .fall_at = {50001}, .n = 1};
    la_trigger_t rise = {0};
    la_trigger_set_channel(&rise, 7, 'r');
    result_t r;

    /* Scanner polled less than once per ring: unscanned samples were lost. */
    run(&r, &line, 1024, 0, &rise, LA_RING_SIZE + 1, 0, 100000);
    CHECK(r.state == LA_OVERRUN);

    /* Exactly one ring between polls is still safe. */
    run(&r, &line, 1024, 0, &rise, LA_RING_SIZE, 0, 100000);
    CHECK(r.state == LA_DONE);

    /* Timer stopped late: the ring head room is depth .. ring size. */
    const uint32_t depth = LA_MAX_SAMPLES, pre = 1000, room = LA_RING_SIZE - LA_MAX_SAMPLES;
    run(&r, &line, depth, pre, &rise, 1, room, 100000);
    CHECK(r.state == LA_DONE);
    CHECK(window_ok(&r, &line, depth, pre));
    run(&r, &line, depth, pre, &rise, 1, room + 1, 100000);
    CHECK(r.state == LA_OVERRUN);
}

static void test_invalid_configs(void)
{
    la_capture_t c;
    la_trigger_t t = {0};
    CHECK(!la_capture_start(&c, LA_RING_SIZE, 0, 0, &t));
    CHECK(!la_capture_start(&c, LA_RING_SIZE, LA_RING_SIZE, 0, &t));
    CHECK(!la_capture_start(&c, LA_RING_SIZE, 100, 101, &t));
    CHECK(c.state == LA_IDLE);
    CHECK(la_capture_start(&c, LA_RING_SIZE, 100, 100, &t));
}

static void test_ring_written(void)
{
    const uint32_t n = 1000;
    CHECK(la_ring_written(0, n, false, n) == 0);
    CHECK(la_ring_written(0, 1, false, n) == 999);
    CHECK(la_ring_written(3, 600, false, n) == 3400);
    /* Wrapped, interrupt not yet run: the small position is in the next lap. */
    CHECK(la_ring_written(3, n - 5, true, n) == 4005);
    /* Flag set after CNDTR was read near the end of the lap: no extra lap. */
    CHECK(la_ring_written(3, 2, true, n) == 3998);
}

/* ---- SUMP protocol ---------------------------------------------------------------------- */

static sump_event_t feed(sump_parser_t *p, sump_config_t *cfg, const uint8_t *b, size_t n)
{
    sump_event_t ev = SUMP_EV_NONE;
    for (size_t i = 0; i < n; i++) {
        ev = sump_feed(p, cfg, b[i]);
    }
    return ev;
}

static void test_sump_short_commands(void)
{
    sump_parser_t p;
    sump_config_t cfg;
    sump_init(&p, &cfg);
    CHECK(sump_feed(&p, &cfg, 0x00) == SUMP_EV_RESET);
    CHECK(sump_feed(&p, &cfg, 0x02) == SUMP_EV_ID);
    CHECK(sump_feed(&p, &cfg, 0x04) == SUMP_EV_METADATA);
    CHECK(sump_feed(&p, &cfg, 0x01) == SUMP_EV_RUN);
    CHECK(sump_feed(&p, &cfg, 0x11) == SUMP_EV_IGNORED);

    /* Five resets always recover, even from inside a long command. */
    CHECK(sump_feed(&p, &cfg, 0x80) == SUMP_EV_NONE);
    const uint8_t resets[5] = {0, 0, 0, 0, 0};
    CHECK(feed(&p, &cfg, resets, 5) == SUMP_EV_RESET);
}

static void test_sump_long_commands(void)
{
    sump_parser_t p;
    sump_config_t cfg;
    sump_init(&p, &cfg);
    CHECK(cfg.divider == 99 && cfg.read_count == 4096);

    const uint8_t div[] = {0x80, 0x0F, 0x27, 0x00, 0x00}; /* 9999 -> 10 kS/s */
    CHECK(feed(&p, &cfg, div, 5) == SUMP_EV_CONFIG);
    CHECK(cfg.divider == 9999);

    /* 2048 samples, 1536 after the trigger: (2048/4 - 1), (1536/4 - 1). */
    const uint8_t counts[] = {0x81, 0xFF, 0x01, 0x7F, 0x01};
    CHECK(feed(&p, &cfg, counts, 5) == SUMP_EV_CONFIG);
    CHECK(cfg.read_count == 2048 && cfg.delay_count == 1536);

    const uint8_t mask[] = {0xC0, 0x81, 0, 0, 0}, value[] = {0xC1, 0xFF, 0, 0, 0};
    feed(&p, &cfg, mask, 5);
    feed(&p, &cfg, value, 5);
    const uint8_t edge[] = {0xC3, 0x04, 0x08, 0, 0};
    CHECK(feed(&p, &cfg, edge, 5) == SUMP_EV_CONFIG);
    const uint8_t stage1[] = {0xC4, 1, 2, 3, 4};
    CHECK(feed(&p, &cfg, stage1, 5) == SUMP_EV_IGNORED);

    uint32_t depth, pre;
    la_trigger_t t;
    sump_capture_plan(&cfg, &depth, &pre, &t);
    CHECK(depth == 2048 && pre == 512);
    CHECK(t.level_mask == 0x81 && t.level_value == 0x81); /* value limited to the mask */
    CHECK(t.rise_mask == 0x04 && t.fall_mask == 0x08);

    /* A reset drops the edge extension so standard clients get plain levels. */
    sump_feed(&p, &cfg, 0x00);
    CHECK(cfg.rise_mask == 0 && cfg.fall_mask == 0);
}

static void test_sump_plan_limits(void)
{
    sump_parser_t p;
    sump_config_t cfg;
    sump_init(&p, &cfg);
    cfg.read_count = 65536;
    cfg.delay_count = 70000;
    uint32_t depth, pre;
    la_trigger_t t;
    sump_capture_plan(&cfg, &depth, &pre, &t);
    CHECK(depth == LA_MAX_SAMPLES && pre == 0);
    CHECK(la_trigger_is_immediate(&t));
}

static void test_sump_metadata(void)
{
    uint8_t buf[96];
    size_t n = sump_metadata(buf, sizeof(buf));
    CHECK(n > 0 && buf[0] == 0x01);
    CHECK(strcmp((const char *)&buf[1], LA_DEVICE_NAME) == 0);
    CHECK(buf[n - 1] == 0x00);

    /* Find the sample-memory key and check its big-endian value. */
    const uint8_t mem[5] = {0x21, 0x00, 0x00, 0x20, 0x00}; /* 8192 */
    const uint8_t rate[5] = {0x23, 0x00, 0x3D, 0x09, 0x00}; /* 4 000 000 */
    int found_mem = 0, found_rate = 0;
    for (size_t i = 0; i + 5 <= n; i++) {
        found_mem |= memcmp(&buf[i], mem, 5) == 0;
        found_rate |= memcmp(&buf[i], rate, 5) == 0;
    }
    CHECK(found_mem && found_rate);
    CHECK(sump_metadata(buf, 10) == 0);
}

static void test_sample_rate_conversion(void)
{
    CHECK(la_divider_to_ticks(99) == 64);       /* 1 MS/s */
    CHECK(la_divider_to_ticks(24) == 16);       /* 4 MS/s */
    CHECK(la_divider_to_ticks(0) == 16);        /* 100 MS/s request -> clamped */
    CHECK(la_divider_to_ticks(99999) == 64000); /* 1 kS/s */

    uint16_t psc, arr;
    CHECK(la_timer_split(64, &psc, &arr) == 64 && psc == 0 && arr == 63);
    CHECK(la_timer_split(65536, &psc, &arr) == 65536 && psc == 0 && arr == 65535);
    /* 10 S/s needs a prescaler; the rate error stays tiny. */
    const uint32_t want = la_divider_to_ticks(9999999);
    const uint32_t got = la_timer_split(want, &psc, &arr);
    CHECK(want == 6400000);
    CHECK(psc > 0 && llabs((long long)got - (long long)want) * 10000 < (long long)want);
}

int main(void)
{
    test_trigger_match();
    test_immediate_capture();
    test_level_trigger_any_poll_size();
    test_edge_triggers();
    test_no_trigger_inside_pretrigger();
    test_trigger_after_many_ring_laps();
    test_overrun_detection();
    test_invalid_configs();
    test_ring_written();
    test_sump_short_commands();
    test_sump_long_commands();
    test_sump_plan_limits();
    test_sump_metadata();
    test_sample_rate_conversion();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
