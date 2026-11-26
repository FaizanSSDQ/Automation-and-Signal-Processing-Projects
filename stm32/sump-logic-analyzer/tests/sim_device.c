/**
 * Host simulator of the logic analyzer: the firmware's SUMP parser, trigger
 * and capture engine, speaking SUMP over stdin/stdout. Sampling is simulated:
 * a modeled "DMA" stores samples of the on-board demo signals into the ring
 * between polls, exactly as app_la.c does with the real DMA.
 *
 * Inputs (as if wired to the demo outputs, see demo_signals.h):
 *   CH0 <- PA6  10 kHz, 50 %      CH2 <- PB6  1 kHz, 10 %
 *   CH1 <- PA7  10 kHz, 25 %      CH3 <- PA9  9600 baud "LA demo NNNN\r\n" every 50 ms
 *   CH4..CH7 unconnected (pull-down, read 0)
 *
 * Build: make sim      Use: python tools/sump_capture.py sim ...
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "la_capture.h"
#include "la_config.h"
#include "sump.h"

#define SIM_CHUNK        97u          /* samples the "DMA" stores between polls */
#define SIM_MAX_SAMPLES  200000000ull /* give up waiting for a trigger after this */
#define UART_BAUD        9600.0
#define UART_START_S     0.003        /* first message starts 3 ms after arming */
#define UART_PERIOD_S    0.050

static uint8_t ring[LA_RING_SIZE];

static double frac(double x) { return x - (double)(long long)x; }

static int uart_line(double t)
{
    if (t < UART_START_S) {
        return 1;
    }
    const long long msg = (long long)((t - UART_START_S) / UART_PERIOD_S);
    const double tm = t - UART_START_S - (double)msg * UART_PERIOD_S;
    const long long bit_no = (long long)(tm * UART_BAUD);
    const long long ch_no = bit_no / 10;

    char text[16];
    snprintf(text, sizeof(text), "LA demo %04lld\r\n", msg % 10000);
    if (ch_no >= (long long)strlen(text)) {
        return 1; /* idle */
    }
    const int bit = (int)(bit_no % 10);
    if (bit == 0) {
        return 0; /* start */
    }
    if (bit == 9) {
        return 1; /* stop */
    }
    return ((unsigned char)text[ch_no] >> (bit - 1)) & 1;
}

/* Phase in cycles of a tone of frequency f at sample n. Computed as n*f/rate
 * (exact for whole-number results) plus a small guard, so edges never shift
 * by a sample because of floating-point rounding. */
static double cycles(uint64_t n, double f, double rate)
{
    return (double)n * f / rate + 1e-9;
}

static uint8_t sample_at(uint64_t n, double rate)
{
    uint8_t s = 0;
    s |= (uint8_t)(frac(cycles(n, 10e3, rate)) < 0.50) << 0;
    s |= (uint8_t)(frac(cycles(n, 10e3, rate)) < 0.25) << 1;
    s |= (uint8_t)(frac(cycles(n, 1e3, rate)) < 0.10) << 2;
    s |= (uint8_t)uart_line(cycles(n, 1.0, rate)) << 3;
    return s;
}

static void send(const void *p, size_t n)
{
    fwrite(p, 1, n, stdout);
    fflush(stdout);
}

static void run_capture(const sump_config_t *cfg)
{
    uint32_t depth, pre;
    la_trigger_t trig;
    uint16_t psc, arr;
    sump_capture_plan(cfg, &depth, &pre, &trig);
    const double rate = (double)LA_TIMER_CLOCK_HZ /
                        (double)la_timer_split(la_divider_to_ticks(cfg->divider), &psc, &arr);

    la_capture_t c;
    if (!la_capture_start(&c, LA_RING_SIZE, depth, pre, &trig)) {
        return;
    }
    uint64_t written = 0;
    la_state_t st = LA_PRE_TRIGGER;
    while (st != LA_DONE && st != LA_OVERRUN) {
        if (written > SIM_MAX_SAMPLES) {
            fprintf(stderr, "sim: trigger not found\n");
            return; /* like the board: keep waiting, send nothing */
        }
        for (uint32_t k = 0; k < SIM_CHUNK; k++, written++) {
            ring[written % LA_RING_SIZE] = sample_at(written, rate);
        }
        st = la_capture_poll(&c, ring, written);
    }
    st = la_capture_finish(&c, written);
    uint32_t first = (st == LA_DONE) ? la_capture_first_index(&c)
                                     : (uint32_t)((written - depth) % LA_RING_SIZE);

    /* Newest sample first, as SUMP requires. */
    uint32_t idx = (first + depth - 1u) % LA_RING_SIZE;
    for (uint32_t k = 0; k < depth; k++) {
        putchar(ring[idx]);
        idx = (idx == 0u) ? LA_RING_SIZE - 1u : idx - 1u;
    }
    fflush(stdout);
    fprintf(stderr, "sim: %s, %u samples at %.0f S/s, trigger at sample %llu\n",
            st == LA_DONE ? "done" : "overrun", (unsigned)depth, rate,
            (unsigned long long)c.trig_at);
}

int main(void)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    sump_parser_t parser;
    sump_config_t cfg;
    sump_init(&parser, &cfg);

    int ch;
    while ((ch = getchar()) != EOF) {
        switch (sump_feed(&parser, &cfg, (uint8_t)ch)) {
        case SUMP_EV_ID:
            send("1ALS", 4);
            break;
        case SUMP_EV_METADATA: {
            uint8_t meta[96];
            send(meta, sump_metadata(meta, sizeof(meta)));
            break;
        }
        case SUMP_EV_RUN:
            run_capture(&cfg);
            break;
        default:
            break;
        }
    }
    return 0;
}
