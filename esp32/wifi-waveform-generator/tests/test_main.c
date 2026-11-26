/**
 * Host tests for the ESP32 waveform generator core: tuning words, DAC
 * scaling, every waveform, the sync output, sweeps, the page preview, the
 * web settings parser and the settings JSON.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../wavegen/src/core/dds.h"
#include "../wavegen/src/core/gen_config.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

#define N 2000u
static uint8_t buf[2u * N];

static dds_settings_t settings(dds_wave_t w, uint32_t freq_mhz)
{
    dds_settings_t s;
    dds_settings_default(&s);
    s.wave = w;
    s.freq_mhz = freq_mhz;
    s.antialias = false;
    return s;
}

static void render(dds_t *d, uint32_t n)
{
    dds_render(d, buf, n);
}

static void minmax(uint32_t n, int *lo, int *hi, long *sum)
{
    *lo = 255;
    *hi = 0;
    *sum = 0;
    for (uint32_t k = 0; k < n; k++) {
        const int v = buf[2u * k];
        *lo = v < *lo ? v : *lo;
        *hi = v > *hi ? v : *hi;
        *sum += v;
    }
}

static void test_scaling(void)
{
    CHECK(dds_tuning_word(1000000u) == 42949673u);      /* 1 kHz at 100 kHz: 2^32 / 100 */
    CHECK(dds_freq_mhz(42949673u) == 1000000u);
    CHECK(dds_freq_mhz(dds_tuning_word(12345678u)) == 12345678u);
    CHECK(dds_mv_to_counts(3300u) == 255 && dds_mv_to_counts(1650u) == 128 && dds_mv_to_counts(0u) == 0);
}

static void test_sine_and_sync(void)
{
    dds_settings_t s = settings(DDS_SINE, 1000000u);   /* 1 kHz: 100 samples per cycle */
    s.amp_mv = 3000u;
    dds_t d;
    dds_init(&d, &s);
    render(&d, N);
    int lo, hi;
    long sum;
    minmax(N, &lo, &hi, &sum);
    CHECK(hi >= 243 && hi <= 245);                    /* 128 + 116 */
    CHECK(lo >= 11 && lo <= 13);
    CHECK(labs(sum / (long)N - 128) <= 1);
    CHECK(buf[0] == 128 && buf[2u * 25u] == hi);       /* starts at the centre, peak a quarter later */
    CHECK(buf[2u * 100u] == buf[0]);                   /* period of 100 samples */
    int high = 0;
    for (uint32_t k = 0; k < 100; k++) {
        high += buf[2u * k + 1u] == 255;
    }
    CHECK(high == 50 && buf[1] == 255 && buf[2u * 50u + 1u] == 0);   /* sync: first half high */
}

static void test_shapes(void)
{
    dds_t d;
    dds_settings_t s = settings(DDS_SQUARE, 1000000u);
    s.duty_pct = 25u;
    dds_init(&d, &s);
    render(&d, 100);
    int hi_count = 0;
    for (uint32_t k = 0; k < 100; k++) {
        hi_count += buf[2u * k] > 128;
    }
    CHECK(hi_count == 25);

    s = settings(DDS_TRIANGLE, 1000000u);
    s.amp_mv = 3300u;
    dds_init(&d, &s);
    render(&d, 100);
    CHECK(buf[0] <= 1 && buf[2u * 50u] >= 254);        /* bottom at 0, top half a cycle later */
    CHECK(abs((int)buf[2u * 25u] - 128) <= 1);

    s = settings(DDS_SAW, 1000000u);
    dds_init(&d, &s);
    render(&d, 100);
    int rising = 1;
    for (uint32_t k = 1; k < 100; k++) {
        rising &= buf[2u * k] >= buf[2u * (k - 1u)];
    }
    CHECK(rising);

    s = settings(DDS_NOISE, 1000000u);
    dds_init(&d, &s);
    render(&d, N);
    int lo, hi;
    long sum;
    minmax(N, &lo, &hi, &sum);
    CHECK(hi - lo > 120 && labs(sum / (long)N - 128) < 6);

    s = settings(DDS_SINE, 1000000u);
    s.output_on = false;
    dds_init(&d, &s);
    render(&d, 100);
    minmax(100, &lo, &hi, &sum);
    CHECK(hi == 0 && buf[1] == 0);
}

static void test_antialias(void)
{
    /* With PolyBLEP the samples next to a square edge sit between the two levels. */
    dds_settings_t s = settings(DDS_SQUARE, 3330000u);   /* 3.33 kHz: edges fall between samples */
    s.antialias = true;
    dds_t d;
    dds_init(&d, &s);
    render(&d, 300);
    int between = 0;
    for (uint32_t k = 0; k < 300; k++) {
        const int v = buf[2u * k];
        between += v > 40 && v < 215;
    }
    CHECK(between > 10);
}

static void test_phase_continuity(void)
{
    dds_settings_t s = settings(DDS_SINE, 1000000u);
    dds_t d;
    dds_init(&d, &s);
    render(&d, 37);
    const uint32_t before = d.phase;
    s.freq_mhz = 2000000u;
    dds_apply(&d, &s);
    CHECK(d.phase == before && d.tw == dds_tuning_word(2000000u));
}

static void test_sweep(void)
{
    dds_settings_t s = settings(DDS_SINE, 1000000u);
    s.sweep = DDS_SWEEP_LIN;
    s.sweep_start_mhz = 100000u;
    s.sweep_stop_mhz = 10000000u;
    s.sweep_ms = 100u;                                 /* 10000 samples */
    dds_t d;
    dds_init(&d, &s);
    CHECK(d.sweep_len == 10000u && d.tw == dds_tuning_word(100000u));
    for (int i = 0; i < 10; i++) {
        render(&d, 500);
    }
    const uint32_t mid = d.tw;                         /* set at 4500 samples */
    const uint32_t expect = dds_tuning_word(100000u) + (uint32_t)(((uint64_t)dds_tuning_word(10000000u) -
                                                                   dds_tuning_word(100000u)) * 4500u / 10000u);
    CHECK(mid == expect);
    for (int i = 0; i < 10; i++) {
        render(&d, 500);
    }
    CHECK(d.sweep_pos == 0u);                          /* restarted after one sweep */
}

static void test_preview(void)
{
    uint8_t p[256];
    dds_settings_t s = settings(DDS_SINE, 1000000u);
    CHECK(dds_preview(&s, p, 256) == 200u);            /* two cycles = 200 real samples */
    CHECK(p[0] == 128 && p[100] == 128);
    s.freq_mhz = 20000000u;
    CHECK(dds_preview(&s, p, 256) == 10u);             /* 20 kHz: only 5 samples per cycle */
    s.freq_mhz = 10000u;
    CHECK(dds_preview(&s, p, 256) == 256u);            /* 10 Hz: decimated */
    s.freq_mhz = 100u;                                 /* 0.1 Hz: limited to 200 ms */
    CHECK(dds_preview(&s, p, 256) == 256u);
    s.sweep = DDS_SWEEP_LOG;                           /* sweeps preview the start frequency */
    s.sweep_start_mhz = 5000000u;
    CHECK(dds_preview(&s, p, 256) == 40u);
}

static void test_set(void)
{
    dds_settings_t s;
    dds_settings_default(&s);
    CHECK(dds_set(&s, "freq", "1234.5") && s.freq_mhz == 1234500u);
    CHECK(dds_set(&s, "freq", "0.01") && s.freq_mhz == 10u);
    CHECK(!dds_set(&s, "freq", "0.001") && !dds_set(&s, "freq", "20000.1") && !dds_set(&s, "freq", "1k"));
    CHECK(dds_set(&s, "freq", "20000") && s.freq_mhz == 20000000u);
    CHECK(dds_set(&s, "wave", "triangle") && s.wave == DDS_TRIANGLE && !dds_set(&s, "wave", "pulse"));
    CHECK(dds_set(&s, "amp", "3300") && !dds_set(&s, "amp", "3301") && !dds_set(&s, "amp", "-1"));
    CHECK(dds_set(&s, "duty", "10") && s.duty_pct == 10u && !dds_set(&s, "duty", "0"));
    CHECK(dds_set(&s, "output", "0") && !s.output_on && !dds_set(&s, "output", "2"));
    CHECK(dds_set(&s, "antialias", "0") && !s.antialias);
    CHECK(dds_set(&s, "sweep", "log") && s.sweep == DDS_SWEEP_LOG && !dds_set(&s, "sweep", "fast"));
    CHECK(dds_set(&s, "sweep_ms", "500") && !dds_set(&s, "sweep_ms", "5"));
    CHECK(!dds_set(&s, "volume", "1") && !dds_set(&s, NULL, "1"));
}

static void test_json(void)
{
    dds_settings_t s;
    dds_settings_default(&s);
    s.freq_mhz = 1234567u;
    char out[512];
    const size_t n = dds_settings_json(&s, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strcmp(out, "{\"wave\":\"sine\",\"freq\":1234.567,\"actual\":1234.567,\"amp\":2000,\"offset\":1650,"
                      "\"duty\":50,\"output\":1,\"antialias\":1,\"sweep\":\"off\",\"sweep_start\":100.000,"
                      "\"sweep_stop\":10000.000,\"sweep_ms\":2000,\"fs\":100000}") == 0);
    CHECK(dds_settings_json(&s, out, 40) == 0 && out[0] == '\0');
}

int main(void)
{
    test_scaling();
    test_sine_and_sync();
    test_shapes();
    test_antialias();
    test_phase_continuity();
    test_sweep();
    test_preview();
    test_set();
    test_json();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
