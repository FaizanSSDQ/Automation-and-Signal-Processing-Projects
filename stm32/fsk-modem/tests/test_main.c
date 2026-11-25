/**
 * Host tests for the FSK modem: the real modulator output is passed through a simulated
 * channel (delay, attenuation, DC offset, noise) into the real demodulator.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsk_config.h"
#include "fsk_demod.h"
#include "fsk_mod.h"
#include "fsk_tables.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

/* ---- helpers ---------------------------------------------------------------------------- */

typedef struct {
    char text[512];
    size_t len;
} sink_t;

static void on_byte(void *ctx, uint8_t b)
{
    sink_t *s = ctx;
    if (s->len < sizeof(s->text) - 1) {
        s->text[s->len++] = (char)b;
        s->text[s->len] = '\0';
    }
}

static uint32_t rng_state = 12345;
static double uniform(void)
{
    rng_state = rng_state * 1664525u + 1013904223u; /* LCG, deterministic */
    return ((rng_state >> 8) + 0.5) / 16777216.0;
}
static double gaussian(void)
{
    return sqrt(-2.0 * log(uniform())) * cos(6.283185307179586 * uniform());
}

typedef struct {
    int delay;        /* leading idle samples (arbitrary bit alignment) */
    double gain;      /* channel attenuation */
    double dc;        /* offset in ADC counts */
    double noise_rms; /* additive white Gaussian noise, ADC counts */
} channel_t;

/* Modulate text (with idle before/after), pass through the channel, demodulate. */
static fsk_demod_t run(const char *text, channel_t ch, sink_t *out)
{
    enum { MAX = 200000 };
    static uint16_t tx[MAX], rx[MAX];
    fsk_mod_t mod;
    fsk_demod_t dem;
    fsk_mod_init(&mod);
    fsk_demod_init(&dem, on_byte, out);
    memset(out, 0, sizeof(*out));

    size_t n = 0;
    fsk_mod_fill(&mod, tx, (size_t)ch.delay + 64); /* idle mark */
    n += (size_t)ch.delay + 64;
    /* Feed the queue as it drains, like the firmware does with UART input. */
    const char *p = text;
    while ((*p != '\0' || fsk_mod_busy(&mod)) && n + 16 < MAX) {
        p += fsk_mod_puts(&mod, p);
        fsk_mod_fill(&mod, &tx[n], 16);
        n += 16;
    }
    fsk_mod_fill(&mod, &tx[n], 64); /* trailing idle */
    n += 64;

    for (size_t i = 0; i < n; i++) {
        double v = FSK_DAC_MID + ch.gain * ((double)tx[i] - FSK_DAC_MID) + ch.dc
                   + ch.noise_rms * gaussian();
        v = v < 0 ? 0 : (v > 4095 ? 4095 : v);
        rx[i] = (uint16_t)lround(v);
    }
    fsk_demod_process(&dem, rx, n);
    return dem;
}

/* ---- tests -------------------------------------------------------------------------------- */

static void test_tables(void)
{
    CHECK(fsk_sine_q15[0] == 0);
    CHECK(fsk_sine_q15[64] == 32767);
    CHECK(fsk_ref_mark_cos[0] == 16384 && fsk_ref_space_cos[4] == -16384);
}

static void test_mod_output_range_and_idle_tone(void)
{
    fsk_mod_t m;
    fsk_mod_init(&m);
    uint16_t buf[64];
    fsk_mod_fill(&m, buf, 64); /* idle = mark = 1 kHz = 16 samples per cycle */
    int lo = 4095, hi = 0;
    for (int i = 0; i < 64; i++) {
        lo = buf[i] < lo ? buf[i] : lo;
        hi = buf[i] > hi ? buf[i] : hi;
    }
    CHECK(hi <= FSK_DAC_MID + FSK_DAC_AMPLITUDE && lo >= FSK_DAC_MID - FSK_DAC_AMPLITUDE);
    CHECK(hi - lo > 2 * FSK_DAC_AMPLITUDE - 10);
    CHECK(buf[0] == buf[16] && buf[5] == buf[21]); /* period of 16 samples */
}

static void test_queue_limits(void)
{
    fsk_mod_t m;
    fsk_mod_init(&m);
    CHECK(!fsk_mod_busy(&m));
    size_t accepted = 0;
    for (int i = 0; i < 200; i++) {
        accepted += fsk_mod_put(&m, 'x');
    }
    CHECK(accepted == FSK_TX_QUEUE_SIZE - 1);
    CHECK(fsk_mod_busy(&m));
}

static void test_clean_loopback(void)
{
    const char *msg = "Hello from the STM32 FSK modem! 0123456789 ~!@#$%^&*()";
    sink_t out;
    fsk_demod_t d = run(msg, (channel_t){.delay = 0, .gain = 1.0}, &out);
    CHECK(strcmp(out.text, msg) == 0);
    CHECK(d.stats.framing_errors == 0);
    CHECK(d.stats.bytes_ok == strlen(msg));
}

static void test_all_byte_values(void)
{
    char msg[256];
    for (int i = 0; i < 255; i++) {
        msg[i] = (char)(i + 1); /* 0x01..0xFF */
    }
    msg[255] = '\0';
    sink_t out;
    run(msg, (channel_t){.delay = 3, .gain = 1.0}, &out);
    CHECK(out.len == 255 && memcmp(out.text, msg, 255) == 0);
}

static void test_any_alignment(void)
{
    /* The receiver must lock regardless of where the bits fall relative to the window. */
    for (int delay = 0; delay < 32; delay++) {
        sink_t out;
        run("Align", (channel_t){.delay = delay, .gain = 1.0}, &out);
        CHECK(strcmp(out.text, "Align") == 0);
    }
}

static void test_attenuation_and_dc_offset(void)
{
    sink_t out;
    fsk_demod_t d = run("weak signal", (channel_t){.delay = 7, .gain = 0.15, .dc = -300}, &out);
    CHECK(strcmp(out.text, "weak signal") == 0);
    CHECK(d.stats.framing_errors == 0);
}

static void test_noise(void)
{
    /* Signal peak 1240 counts; noise 250 counts RMS (SNR ~ 11 dB). */
    const char *msg = "The quick brown fox jumps over the lazy dog.";
    sink_t out;
    rng_state = 2024;
    fsk_demod_t d = run(msg, (channel_t){.delay = 5, .gain = 1.0, .noise_rms = 250}, &out);
    CHECK(strcmp(out.text, msg) == 0);
    CHECK(d.stats.framing_errors == 0);
}

static void test_squelch_rejects_silence(void)
{
    uint16_t quiet[4000];
    rng_state = 7;
    for (int i = 0; i < 4000; i++) {
        quiet[i] = (uint16_t)lround(FSK_DAC_MID + 20.0 * gaussian()); /* noise only */
    }
    sink_t out = {0};
    fsk_demod_t d;
    fsk_demod_init(&d, on_byte, &out);
    fsk_demod_process(&d, quiet, 4000);
    CHECK(out.len == 0);
    CHECK(!d.signal);
}

int main(void)
{
    test_tables();
    test_mod_output_range_and_idle_tone();
    test_queue_limits();
    test_clean_loopback();
    test_all_byte_values();
    test_any_alignment();
    test_attenuation_and_dc_offset();
    test_noise();
    test_squelch_rejects_silence();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
