/**
 * Host tests for the DDS waveform generator: tuning words, every waveform
 * shape, PolyBLEP anti-aliasing, output scaling and clipping, sync output,
 * sweeps, and the command-line parser.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "dds.h"
#include "dds_tables.h"
#include "gen_config.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

#define PI 3.14159265358979323846
#define N_BUF 125000u /* one second of samples */

static uint16_t buf[2u * N_BUF];

static dds_settings_t settings(dds_wave_t w, uint32_t freq_mhz)
{
    dds_settings_t s;
    dds_settings_default(&s);
    s.wave = w;
    s.freq_mhz = freq_mhz;
    return s;
}

static void render(dds_t *d, uint32_t n)
{
    /* Render in firmware-sized blocks, as the DMA callbacks do. */
    for (uint32_t done = 0; done < n; done += GEN_BLOCK) {
        const uint32_t k = (n - done < GEN_BLOCK) ? n - done : GEN_BLOCK;
        dds_render(d, &buf[2u * done], k);
    }
}

/* ---- tuning word and tables ------------------------------------------------------------- */

static void test_tuning_word(void)
{
    CHECK(dds_tuning_word(1000000) == 34359738u);        /* 1 kHz: 2^32 / 125 */
    CHECK(dds_freq_mhz(34359738u) == 1000000u);
    CHECK(dds_tuning_word(10) == 344u);                  /* 0.01 Hz is still representable */
    CHECK(dds_tuning_word(GEN_FREQ_MAX_MHZ) < 0x80000000u);
    /* Round trip within one LSB of resolution (29 uHz) for awkward values. */
    const uint32_t f[] = {10, 1234567, 3141593, 19999999};
    for (size_t i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        CHECK(labs((long)dds_freq_mhz(dds_tuning_word(f[i])) - (long)f[i]) <= 1);
    }
    CHECK(dds_mv_to_counts(3300) == 512 && dds_mv_to_counts(1650) == 256 && dds_mv_to_counts(0) == 0);
}

static void test_sine_table_and_interpolation(void)
{
    CHECK(dds_sine_table[0] == 0 && dds_sine_table[64] == 32767);
    CHECK(dds_sine_table[128] == 0 && dds_sine_table[192] == -32767 && dds_sine_table[256] == 0);

    dds_settings_t s = settings(DDS_SINE, 1000000);
    dds_t d;
    dds_init(&d, &s);
    double worst = 0;
    for (uint32_t k = 0; k < 65536; k++) {
        const uint32_t phase = k * 65536u + 12345u;
        const double ideal = 32767.0 * sin(2 * PI * (double)phase / 4294967296.0);
        const double err = fabs(dds_shape(&d, phase) - ideal);
        worst = err > worst ? err : worst;
    }
    /* Chord error of a 256-point table is 32767 * (2*pi/256)^2 / 8 = 2.5 LSB, plus
     * rounding: below 4 LSB of Q15 (-79 dB), far under the 9-bit output step. */
    CHECK(worst < 4.0);
}

/* ---- shapes ----------------------------------------------------------------------------- */

static void test_square_duty_and_levels(void)
{
    /* 1234.567 Hz: not a whole number of samples per cycle, so the edges fall
     * at varying sample positions and the average duty converges. */
    dds_settings_t s = settings(DDS_SQUARE, 1234567);
    s.duty_pct = 25;
    s.antialias = false;
    dds_t d;
    dds_init(&d, &s);
    render(&d, N_BUF);
    uint32_t high = 0, levels_other = 0;
    const uint16_t hi = (uint16_t)(256 + 155), lo = (uint16_t)(256 - 155); /* 2 Vpp = 310 counts */
    for (uint32_t k = 0; k < N_BUF; k++) {
        high += buf[2u * k] == hi;
        levels_other += buf[2u * k] != hi && buf[2u * k] != lo;
    }
    CHECK(levels_other == 0);                       /* naive square: two levels only */
    CHECK(labs((long)high - 31250) <= 156);         /* 25 % of 125000 samples, within 0.5 % */

    s.antialias = true;
    dds_apply(&d, &s);
    render(&d, N_BUF);
    levels_other = 0;
    for (uint32_t k = 0; k < N_BUF; k++) {
        levels_other += buf[2u * k] != hi && buf[2u * k] != lo;
    }
    CHECK(levels_other > 1000 && levels_other < 5000); /* ~2 transition samples per edge */
}

static int32_t max_step(const dds_t *d, uint32_t phase0, int n)
{
    int32_t worst = 0, prev = dds_shape(d, phase0);
    for (int k = 1; k < n; k++) {
        const int32_t w = dds_shape(d, phase0 + (uint32_t)k * d->tw);
        worst = abs(w - prev) > worst ? abs(w - prev) : worst;
        prev = w;
    }
    return worst;
}

static void test_polyblep_softens_jumps(void)
{
    /* Saw at 5 kHz (25 samples per cycle), straddling the wrap at phase 0. */
    dds_settings_t s = settings(DDS_SAW, 5000000);
    dds_t d;
    s.antialias = false;
    dds_init(&d, &s);
    const uint32_t start = 0u - 3u * d.tw - 12345u;
    const int32_t naive = max_step(&d, start, 6);
    s.antialias = true;
    dds_apply(&d, &s);
    const int32_t blep = max_step(&d, start, 6);
    CHECK(naive > 60000);              /* full -2 jump in one sample */
    CHECK(blep < naive * 6 / 10);      /* spread over two samples */

    /* Away from the edge the correction is exactly zero. */
    CHECK(dds_shape(&d, 0x40000000u) == (int32_t)(0x40000000u >> 16) - 32768);
}

static void test_triangle_and_saw_shape(void)
{
    dds_settings_t s = settings(DDS_TRIANGLE, 1000000);
    dds_t d;
    dds_init(&d, &s);
    CHECK(dds_shape(&d, 0) == -32767);
    CHECK(abs(dds_shape(&d, 0x40000000u)) <= 1);
    CHECK(dds_shape(&d, 0x80000000u) == 32767);
    CHECK(dds_shape(&d, 0x20000000u) == dds_shape(&d, 0xE0000000u)); /* symmetric */

    s.wave = DDS_SAW;
    s.antialias = false;
    dds_apply(&d, &s);
    CHECK(abs(dds_shape(&d, 0x80000000u)) <= 1);
    CHECK(dds_shape(&d, 0xFFFF0000u) > 32700);
}

static void test_noise_statistics(void)
{
    dds_settings_t s = settings(DDS_NOISE, 1000000);
    dds_t d;
    dds_init(&d, &s);
    render(&d, N_BUF);
    double sum = 0, sum2 = 0;
    uint16_t lo = 0xFFFF, hi = 0;
    for (uint32_t k = 0; k < N_BUF; k++) {
        const double v = buf[2u * k] - 256.0;
        sum += v;
        sum2 += v * v;
        lo = buf[2u * k] < lo ? buf[2u * k] : lo;
        hi = buf[2u * k] > hi ? buf[2u * k] : hi;
    }
    const double mean = sum / N_BUF, rms = sqrt(sum2 / N_BUF);
    CHECK(fabs(mean) < 1.0);
    CHECK(fabs(rms - 155.0 / sqrt(3.0)) < 2.0); /* uniform over +-155 counts */
    CHECK(lo == 256 - 155 && hi == 256 + 155);
}

/* ---- output stage ----------------------------------------------------------------------- */

static void test_scaling_clipping_and_off(void)
{
    dds_settings_t s = settings(DDS_SINE, 1000000);
    s.amp_mv = 3300;
    dds_t d;
    dds_init(&d, &s);
    render(&d, 1250);
    uint16_t lo = 0xFFFF, hi = 0;
    for (uint32_t k = 0; k < 1250; k++) {
        lo = buf[2u * k] < lo ? buf[2u * k] : lo;
        hi = buf[2u * k] > hi ? buf[2u * k] : hi;
    }
    CHECK(lo <= 1 && hi >= 511 && hi <= GEN_FULL_SCALE); /* full 0 .. 3.3 V swing */

    s.offset_mv = 500; /* centre too low: bottom half clips at 0 */
    dds_apply(&d, &s);
    render(&d, 1250);
    uint32_t zeros = 0;
    for (uint32_t k = 0; k < 1250; k++) {
        zeros += buf[2u * k] == 0;
    }
    CHECK(zeros > 300);

    s.output_on = false;
    dds_apply(&d, &s);
    render(&d, 1000);
    uint32_t nonzero = 0;
    for (uint32_t k = 0; k < 2000; k++) {
        nonzero += buf[k] != 0;
    }
    CHECK(nonzero == 0);
}

static void test_frequency_exactness_and_sync(void)
{
    /* Count sync rising edges over exactly one second. */
    const uint32_t freqs[] = {1000000, 1234567, 50000, 10000000};
    for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); i++) {
        dds_settings_t s = settings(DDS_SINE, freqs[i]);
        dds_t d;
        dds_init(&d, &s);
        render(&d, N_BUF);
        uint32_t edges = 0;
        for (uint32_t k = 1; k < N_BUF; k++) {
            edges += buf[2u * k + 1u] && !buf[2u * (k - 1u) + 1u];
        }
        const double expected = freqs[i] / 1000.0;
        CHECK(fabs((double)edges - expected) <= 1.0);
    }

    /* Sync is high for the first half of each cycle, in step with the sine. */
    dds_settings_t s = settings(DDS_SINE, 1000000);
    dds_t d;
    dds_init(&d, &s);
    render(&d, 125);
    CHECK(buf[1] == GEN_FULL_SCALE && buf[2u * 70u + 1u] == 0);
    CHECK(buf[2u * 31u] > 256 && buf[2u * 94u] < 256);
}

static void test_phase_continuity_on_change(void)
{
    dds_settings_t s = settings(DDS_SINE, 1000000);
    dds_t d;
    dds_init(&d, &s);
    render(&d, 100);
    const uint32_t phase = d.phase;
    s.freq_mhz = 2000000;
    dds_apply(&d, &s);
    CHECK(d.phase == phase);
    CHECK(d.tw == dds_tuning_word(2000000));
}

static void test_sweeps(void)
{
    dds_settings_t s = settings(DDS_SINE, 1000000);
    s.sweep = DDS_SWEEP_LIN;
    s.sweep_start_mhz = 100000;
    s.sweep_stop_mhz = 10100000;
    s.sweep_ms = 1000;
    dds_t d;
    dds_init(&d, &s);
    CHECK(d.sweep_len == 125000u);
    render(&d, 62500); /* half way */
    dds_render(&d, buf, 1);
    CHECK(labs((long)dds_freq_mhz(d.tw) - 5100000) < 50000);
    render(&d, 62500); /* wraps back to the start */
    dds_render(&d, buf, 1);
    CHECK(labs((long)dds_freq_mhz(d.tw) - 100000) < 50000);

    s.sweep = DDS_SWEEP_LOG;
    s.sweep_stop_mhz = 10000000;
    dds_apply(&d, &s);
    CHECK(d.sweep_pos == 0);
    render(&d, 62464); /* 244 blocks: 0.4997 of the sweep */
    dds_render(&d, buf, 1);
    /* Geometric middle of 100 Hz .. 10 kHz is 1 kHz. */
    CHECK(fabs(dds_freq_mhz(d.tw) / 1000000.0 - 1.0) < 0.01);
}

/* ---- command line ----------------------------------------------------------------------- */

static void test_number_parsing(void)
{
    uint64_t v;
    CHECK(cli_parse_milli("12", &v) && v == 12000);
    CHECK(cli_parse_milli("2.5", &v) && v == 2500);
    CHECK(cli_parse_milli("1.2k", &v) && v == 1200000);
    CHECK(cli_parse_milli("1.2345k", &v) && v == 1234500);
    CHECK(cli_parse_milli("0.01", &v) && v == 10);
    CHECK(cli_parse_milli(".5", &v) && v == 500);
    CHECK(cli_parse_milli("3.3", &v) && v == 3300);
    CHECK(!cli_parse_milli("", &v));
    CHECK(!cli_parse_milli("abc", &v));
    CHECK(!cli_parse_milli("1e3", &v));
    CHECK(!cli_parse_milli("12x", &v));

    char out[24];
    cli_format_milli(out, sizeof(out), 1234500, 3);
    CHECK(strcmp(out, "1234.500") == 0);
    cli_format_milli(out, sizeof(out), 1234567, 2);
    CHECK(strcmp(out, "1234.57") == 0);
    cli_format_milli(out, sizeof(out), 50, 3);
    CHECK(strcmp(out, "0.050") == 0);
}

static cli_result_t run(const char *cmd, dds_settings_t *s, char *reply)
{
    char line[CLI_LINE_MAX];
    snprintf(line, sizeof(line), "%s", cmd);
    return cli_execute(line, s, reply, 256);
}

static void test_commands(void)
{
    dds_settings_t s;
    char reply[256];
    dds_settings_default(&s);

    CHECK(run("freq 1.5k", &s, reply) == CLI_CHANGED && s.freq_mhz == 1500000);
    CHECK(strncmp(reply, "ok: sine 1500.000 Hz", 20) == 0);
    CHECK(run("FREQ 2kHz", &s, reply) == CLI_CHANGED && s.freq_mhz == 2000000);
    CHECK(run("freq 30k", &s, reply) == CLI_ERROR && s.freq_mhz == 2000000);
    CHECK(run("wave tri", &s, reply) == CLI_CHANGED && s.wave == DDS_TRIANGLE);
    CHECK(run("wave square", &s, reply) == CLI_CHANGED && s.wave == DDS_SQUARE);
    CHECK(run("wave xyz", &s, reply) == CLI_ERROR && s.wave == DDS_SQUARE);
    CHECK(run("amp 3.3", &s, reply) == CLI_CHANGED && s.amp_mv == 3300);
    CHECK(run("amp 4", &s, reply) == CLI_ERROR);
    CHECK(run("offset 1v", &s, reply) == CLI_CHANGED && s.offset_mv == 1000);
    CHECK(strstr(reply, "clipping") != NULL);
    CHECK(run("duty 25%", &s, reply) == CLI_CHANGED && s.duty_pct == 25);
    CHECK(run("duty 100", &s, reply) == CLI_ERROR);
    CHECK(run("aa off", &s, reply) == CLI_CHANGED && !s.antialias);
    CHECK(run("off", &s, reply) == CLI_CHANGED && !s.output_on);
    CHECK(run("on", &s, reply) == CLI_CHANGED && s.output_on);
    CHECK(run("sweep 100 10k 2 log", &s, reply) == CLI_CHANGED && s.sweep == DDS_SWEEP_LOG);
    CHECK(s.sweep_start_mhz == 100000 && s.sweep_stop_mhz == 10000000 && s.sweep_ms == 2000);
    CHECK(strstr(reply, "log sweep") != NULL);
    CHECK(run("sweep 100 10k", &s, reply) == CLI_ERROR);
    CHECK(run("freq 440", &s, reply) == CLI_CHANGED && s.sweep == DDS_SWEEP_OFF);
    CHECK(run("sweep 1k 2k 0.5s", &s, reply) == CLI_CHANGED && s.sweep == DDS_SWEEP_LIN && s.sweep_ms == 500);
    CHECK(run("sweep off", &s, reply) == CLI_CHANGED && s.sweep == DDS_SWEEP_OFF);
    CHECK(run("status", &s, reply) == CLI_INFO && strstr(reply, "square 440.000 Hz") != NULL);
    CHECK(run("help", &s, reply) == CLI_INFO && strstr(reply, "sweep") != NULL);
    CHECK(run("   ", &s, reply) == CLI_EMPTY);
    CHECK(run("bogus", &s, reply) == CLI_ERROR);
}

static void test_line_editing(void)
{
    cli_line_t l;
    cli_line_reset(&l);
    const char *typed = "frex\bq 1k\r";
    bool done = false;
    for (const char *p = typed; *p; p++) {
        done = cli_line_feed(&l, *p);
    }
    CHECK(done && strcmp(l.buf, "freq 1k") == 0);

    cli_line_reset(&l);
    for (int i = 0; i < 100; i++) {
        cli_line_feed(&l, 'a');
    }
    CHECK(l.overflow && l.len == CLI_LINE_MAX - 1u);
}

int main(void)
{
    test_tuning_word();
    test_sine_table_and_interpolation();
    test_square_duty_and_levels();
    test_polyblep_softens_jumps();
    test_triangle_and_saw_shape();
    test_noise_statistics();
    test_scaling_clipping_and_off();
    test_frequency_exactness_and_sync();
    test_phase_continuity_on_change();
    test_sweeps();
    test_number_parsing();
    test_commands();
    test_line_editing();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
