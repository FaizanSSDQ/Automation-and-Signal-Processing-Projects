/**
 * Host tests for the ESP32 logic analyzer core. The recording loop runs
 * against a simulated cycle counter and input signal, so the same code that
 * samples the GPIO register on the ESP32 is checked here.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../analyzer/src/core/la_core.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

static uint8_t buf[LA_MAX_SAMPLES];

/* ---- simulated hardware ------------------------------------------------------------------ */
static uint32_t sim_cycles;           /* CPU cycle counter */
static uint32_t sim_edge_at;          /* channel 0 rises at this cycle (0 = never) */
static int sim_waits, sim_stop_after, sim_restart_at;
static int sim_irq_off_calls, sim_irq_on_calls;

static uint32_t sim_now(void)
{
    sim_cycles += 7u;                 /* reading the counter takes a few cycles */
    return sim_cycles;
}
static uint8_t sim_read(void)
{
    sim_cycles += 5u;
    const uint8_t ch0 = (uint8_t)(sim_edge_at && sim_cycles >= sim_edge_at);
    return (uint8_t)(ch0 | 0x40u);    /* channel 6 held high */
}
static void sim_irq(bool off)
{
    if (off) {
        sim_irq_off_calls++;
    } else {
        sim_irq_on_calls++;
    }
}
static la_wait_t sim_waiting(void)
{
    sim_waits++;
    if (sim_restart_at && sim_waits == sim_restart_at) {
        return LA_RESTART;
    }
    return (sim_stop_after && sim_waits >= sim_stop_after) ? LA_STOP_WAITING : LA_GO_ON;
}
static const la_hw_t SIM = {sim_now, sim_read, sim_irq, sim_waiting};

static void sim_reset(uint32_t edge_at)
{
    sim_cycles = 0xFFFF0000u;         /* start near the wrap: the loop must survive it */
    sim_edge_at = edge_at;
    sim_waits = sim_stop_after = sim_restart_at = 0;
    sim_irq_off_calls = sim_irq_on_calls = 0;
}

/* ---- tests ------------------------------------------------------------------------------- */
static void test_pack(void)
{
    const int pins[LA_NUM_CHANNELS] = LA_PINS;
    for (int ch = 0; ch < LA_NUM_CHANNELS; ch++) {
        CHECK(la_pack(1u << pins[ch]) == (1u << ch));
    }
    CHECK(la_pack((1u << 15) | (1u << 20) | (1u << 23) | (1u << 12) | 0xFFFu) == 0u);   /* not inputs */
    CHECK(la_pack(0xFFFFFFFFu) == 0xFFu);
}

static void test_plan(void)
{
    la_plan_t p;
    CHECK(la_plan(1000000u, 16384u, 10u, &p) && p.period_cycles == 240u && p.actual_hz == 1000000u);
    CHECK(p.pre == 1638u && p.post == 14746u && p.irq_off);          /* 14.7 ms with interrupts off */
    CHECK(la_plan(100000u, 16384u, 10u, &p) && !p.irq_off);          /* low rate: interrupts stay on */
    CHECK(la_plan(500000u, 65536u, 0u, &p) && p.pre == 1u && !p.irq_off);   /* 131 ms: too long */
    CHECK(la_plan(5000u, 256u, 50u, &p) && p.period_cycles == 48000u);
    CHECK(!la_plan(3000000u, 1024u, 10u, &p));                       /* rate not offered */
    CHECK(!la_plan(1000000u, 100u, 10u, &p) && !la_plan(1000000u, LA_MAX_SAMPLES + 1u, 10u, &p));
    CHECK(!la_plan(1000000u, 1024u, 100u, &p));
    CHECK(la_rate(0) == 5000u && la_rate(LA_RATE_COUNT - 1) == 2000000u && la_rate(LA_RATE_COUNT) == 0u);
}

static void test_trigger_parse(void)
{
    la_trigger_t t;
    CHECK(la_trigger_parse("xxxxxxxr", &t) && t.rise_mask == 0x80u && t.level_mask == 0u);
    CHECK(la_trigger_parse("1fxxxxx0", &t) && t.level_mask == 0x81u && t.level_value == 0x01u && t.fall_mask == 0x02u);
    CHECK(la_trigger_parse("xxxxxxxx", &t) && la_trigger_is_immediate(&t));
    CHECK(!la_trigger_parse("xxxxxxx", &t) && !la_trigger_parse("xxxxxxxq", &t) && !la_trigger_parse(NULL, &t));
}

static void test_rotate(void)
{
    uint8_t b[10];
    for (int i = 0; i < 10; i++) {
        b[i] = (uint8_t)i;
    }
    la_rotate(b, 10, 3);
    CHECK(b[0] == 3 && b[6] == 9 && b[7] == 0 && b[9] == 2);
    la_rotate(b, 10, 10);
    CHECK(b[0] == 3);
}

static void test_record_edge(void)
{
    la_plan_t p;
    la_trigger_t t;
    la_result_t r;
    CHECK(la_plan(1000000u, 1000u, 25u, &p) && la_trigger_parse("rxxxxxxx", &t));
    sim_reset(0xFFFF0000u + 3000000u);         /* edge after 12500 samples: the ring wraps 12 times */
    la_record(&p, &t, &SIM, buf, &r);
    CHECK(r.triggered && r.trigger_index == 249u);
    int ok = 1;
    for (uint32_t i = 0; i < 1000u; i++) {
        ok &= (buf[i] & 1u) == (i >= 249u) && (buf[i] & 0x40u);
    }
    CHECK(ok);                                  /* 249 lows, then the edge, then all high */
    CHECK(sim_irq_off_calls == 1 && sim_irq_on_calls == 1);
    CHECK(r.max_late < 24u);                    /* within one counter read + sample read */
}

static void test_record_timeout_and_restart(void)
{
    la_plan_t p;
    la_trigger_t t;
    la_result_t r;
    CHECK(la_plan(100000u, 2000u, 50u, &p) && la_trigger_parse("rxxxxxxx", &t));
    sim_reset(0);                               /* no edge ever */
    sim_stop_after = 3;
    la_record(&p, &t, &SIM, buf, &r);
    CHECK(!r.triggered && r.trigger_index == 999u && sim_waits == 3);
    CHECK(sim_irq_off_calls == 0);              /* 100 kHz: interrupts stay on */

    /* After a restart (the task yielded, leaving a gap) the pre-trigger part must be
     * refilled before the trigger may fire. Edge at sample ~4600: 500 samples after
     * the restart at 4096, fewer than the 1000 needed, so it is ignored. */
    sim_reset(0xFFFF0000u + 4600u * 2400u);
    sim_restart_at = 1;
    sim_stop_after = 3;
    la_record(&p, &t, &SIM, buf, &r);
    CHECK(!r.triggered && sim_waits == 3);

    /* The same edge without the restart is caught. */
    sim_reset(0xFFFF0000u + 4600u * 2400u);
    sim_stop_after = 3;
    la_record(&p, &t, &SIM, buf, &r);
    CHECK(r.triggered && (buf[r.trigger_index] & 1u) && !(buf[r.trigger_index - 1u] & 1u));
}

static void test_measure(void)
{
    for (uint32_t i = 0; i < 1000u; i++) {
        buf[i] = (uint8_t)(((i % 10u) < 3u) << 1);   /* channel 1: period 10, 30 % high */
    }
    la_measure_t m;
    la_measure(buf, 1000u, 1, 1000000u, &m);
    CHECK(fabsf(m.freq_hz - 100000.0f) < 1.0f && fabsf(m.duty_pct - 30.0f) < 0.01f);
    CHECK(m.edges == 199u && m.min_pulse == 3u);
    la_measure(buf, 1000u, 0, 1000000u, &m);
    CHECK(m.edges == 0u && m.freq_hz == 0.0f && m.min_pulse == 0u);
}

/* UART: 8N1, LSB first, idle high, on channel 2 at an exact number of samples per bit (spb). */
static uint32_t put_uart(uint32_t at, uint8_t v, float spb, bool good_stop)
{
    const int bits[10] = {0, v & 1, (v >> 1) & 1, (v >> 2) & 1, (v >> 3) & 1, (v >> 4) & 1,
                          (v >> 5) & 1, (v >> 6) & 1, (v >> 7) & 1, good_stop};
    for (int b = 0; b < 10; b++) {
        const uint32_t s = at + (uint32_t)(b * spb), e = at + (uint32_t)((b + 1) * spb);
        for (uint32_t i = s; i < e; i++) {
            buf[i] = (uint8_t)(bits[b] ? (buf[i] | 4u) : (buf[i] & ~4u));
        }
    }
    return at + (uint32_t)(10 * spb);
}

static void test_uart(void)
{
    memset(buf, 4, 3000);                       /* idle high */
    const float spb = 1000000.0f / 115200.0f;  /* 8.68 samples per bit at 1 MHz */
    uint32_t at = put_uart(100, 'H', spb, true);
    at = put_uart(at + 20, 'i', spb, true);
    put_uart(at + 5, 0x55, spb, false);
    la_uart_frame_t f[8];
    const size_t n = la_uart_decode(buf, 3000, 2, 1000000u, 115200u, f, 8);
    CHECK(n == 3 && f[0].value == 'H' && f[1].value == 'i' && f[2].value == 0x55);
    CHECK(f[0].start == 100u && !f[0].frame_error && !f[1].frame_error && f[2].frame_error);
    CHECK(la_uart_decode(buf, 3000, 2, 200000u, 115200u, f, 8) == 0);   /* under 3 samples per bit */
    CHECK(la_uart_decode(buf, 3000, 2, 1000000u, 115200u, f, 1) == 1);  /* capacity */
}

/* I2C on SDA = channel 3, SCL = channel 4, four samples per clock phase. */
static uint32_t i2c_pos;
static void i2c_set(int sda, int scl, int samples)
{
    for (int k = 0; k < samples; k++) {
        buf[i2c_pos++] = (uint8_t)((sda << 3) | (scl << 4));
    }
}
static void i2c_bit(int b)
{
    i2c_set(b, 0, 4);
    i2c_set(b, 1, 4);
    i2c_set(b, 0, 1);
}
static void i2c_byte(uint8_t v, int ack)
{
    for (int k = 7; k >= 0; k--) {
        i2c_bit((v >> k) & 1);
    }
    i2c_bit(!ack);
}

static void test_i2c(void)
{
    i2c_pos = 0;
    i2c_set(1, 1, 10);                          /* idle */
    i2c_set(0, 1, 4);                           /* START */
    i2c_byte(0x3Cu << 1, 1);                    /* address 0x3C, write, ACK */
    i2c_byte(0xA5u, 0);                         /* data, NACK */
    i2c_set(1, 1, 1);                           /* SDA high first, then a repeated START */
    i2c_set(0, 1, 4);
    i2c_byte((0x3Cu << 1) | 1u, 1);             /* address 0x3C, read */
    i2c_set(0, 0, 2);
    i2c_set(0, 1, 2);
    i2c_set(1, 1, 6);                           /* STOP */
    la_i2c_event_t e[16];
    const size_t n = la_i2c_decode(buf, i2c_pos, 3, 4, e, 16);
    CHECK(n == 6);
    CHECK(e[0].kind == I2C_START && e[1].kind == I2C_ADDRESS && e[1].value == 0x3C && !e[1].read && e[1].ack);
    CHECK(e[2].kind == I2C_DATA && e[2].value == 0xA5 && !e[2].ack);
    CHECK(e[3].kind == I2C_RESTART && e[4].kind == I2C_ADDRESS && e[4].read && e[5].kind == I2C_STOP);
    CHECK(la_i2c_decode(buf, i2c_pos, 3, 4, e, 2) == 2);
}

static void test_json(void)
{
    char out[512];
    la_plan_t p;
    la_plan(1000000u, 16384u, 10u, &p);
    const la_result_t r = {true, 1637u, 24u};
    CHECK(la_result_json(&p, &r, out, sizeof(out)) == strlen(out));
    CHECK(strcmp(out, "{\"rate\":1000000,\"actual_rate\":1000000,\"samples\":16384,\"pre\":1638,"
                      "\"trigger_index\":1637,\"triggered\":true,\"irq_off\":true,\"max_late_ns\":100}") == 0);
    const la_uart_frame_t f[2] = {{10, 90, 'H', false}, {100, 180, 0x55, true}};
    la_uart_json(f, 2, out, sizeof(out));
    CHECK(strcmp(out, "[{\"s\":10,\"e\":90,\"v\":72},{\"s\":100,\"e\":180,\"v\":85,\"err\":1}]") == 0);
    const la_i2c_event_t e[3] = {{I2C_START, 5, 5, 0, false, false}, {I2C_ADDRESS, 9, 90, 0x3C, true, true},
                                 {I2C_STOP, 99, 99, 0, false, false}};
    la_i2c_json(e, 3, out, sizeof(out));
    CHECK(strcmp(out, "[{\"k\":\"start\",\"s\":5,\"e\":5},{\"k\":\"addr\",\"s\":9,\"e\":90,\"v\":60,\"ack\":1,\"rd\":1},"
                      "{\"k\":\"stop\",\"s\":99,\"e\":99}]") == 0);
    const la_measure_t m = {199, 300, 100000.0f, 30.0f, 3};
    la_measure_json(&m, 1, out, sizeof(out));
    CHECK(strcmp(out, "[{\"edges\":199,\"freq\":100000.0,\"duty\":30.0,\"min_pulse\":3}]") == 0);
    CHECK(la_uart_json(f, 2, out, 20) == 0 && out[0] == '\0');
}

int main(void)
{
    test_pack();
    test_plan();
    test_trigger_parse();
    test_rotate();
    test_record_edge();
    test_record_timeout_and_restart();
    test_measure();
    test_uart();
    test_i2c();
    test_json();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
