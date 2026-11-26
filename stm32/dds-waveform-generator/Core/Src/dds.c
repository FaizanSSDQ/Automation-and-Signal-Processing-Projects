/**
 * @file    dds.c
 * @brief   Direct digital synthesis core.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "dds.h"

#include <math.h>
#include <string.h>

#include "dds_tables.h"
#include "gen_config.h"

#define Q15_ONE 32768

void dds_settings_default(dds_settings_t *s)
{
    s->wave = DDS_SINE;
    s->freq_mhz = 1000000u;  /* 1 kHz */
    s->amp_mv = 2000u;
    s->offset_mv = 1650u;
    s->duty_pct = 50u;
    s->output_on = true;
    s->antialias = true;
    s->sweep = DDS_SWEEP_OFF;
    s->sweep_start_mhz = 100000u;     /* 100 Hz */
    s->sweep_stop_mhz = 10000000u;    /* 10 kHz */
    s->sweep_ms = 2000u;
}

uint32_t dds_tuning_word(uint32_t freq_mhz)
{
    const uint64_t den = (uint64_t)GEN_SAMPLE_RATE_HZ * 1000u;
    return (uint32_t)((((uint64_t)freq_mhz << 32) + den / 2u) / den);
}

uint32_t dds_freq_mhz(uint32_t tw)
{
    const uint64_t num = (uint64_t)tw * GEN_SAMPLE_RATE_HZ * 1000u;
    return (uint32_t)((num + (1ull << 31)) >> 32);
}

int32_t dds_mv_to_counts(uint32_t mv)
{
    return (int32_t)((mv * GEN_FULL_SCALE + GEN_VREF_MV / 2u) / GEN_VREF_MV);
}

static uint32_t clamp_freq(uint32_t mhz)
{
    if (mhz < GEN_FREQ_MIN_MHZ) {
        return GEN_FREQ_MIN_MHZ;
    }
    return (mhz > GEN_FREQ_MAX_MHZ) ? GEN_FREQ_MAX_MHZ : mhz;
}

void dds_init(dds_t *d, const dds_settings_t *s)
{
    memset(d, 0, sizeof(*d));
    d->phase = 0u;
    d->noise = 0x12345678u;
    d->sweep_pos = 0u;
    dds_apply(d, s);
}

void dds_apply(dds_t *d, const dds_settings_t *s)
{
    const bool sweep_changed = (s->sweep != d->s.sweep) ||
                               (s->sweep_start_mhz != d->s.sweep_start_mhz) ||
                               (s->sweep_stop_mhz != d->s.sweep_stop_mhz) ||
                               (s->sweep_ms != d->s.sweep_ms);
    d->s = *s;

    uint32_t duty = s->duty_pct;
    if (duty < 1u) {
        duty = 1u;
    } else if (duty > 99u) {
        duty = 99u;
    }
    d->duty_phase = (uint32_t)(((uint64_t)duty << 32) / 100u);
    d->amp_pp = dds_mv_to_counts(s->amp_mv);
    d->offset = dds_mv_to_counts(s->offset_mv);
    d->tw = dds_tuning_word(clamp_freq(s->freq_mhz));

    if (s->sweep != DDS_SWEEP_OFF) {
        d->tw_start = dds_tuning_word(clamp_freq(s->sweep_start_mhz));
        d->tw_stop = dds_tuning_word(clamp_freq(s->sweep_stop_mhz));
        uint32_t ms = s->sweep_ms;
        if (ms < GEN_SWEEP_MIN_MS) {
            ms = GEN_SWEEP_MIN_MS;
        } else if (ms > GEN_SWEEP_MAX_MS) {
            ms = GEN_SWEEP_MAX_MS;
        }
        d->sweep_len = (uint32_t)((uint64_t)ms * GEN_SAMPLE_RATE_HZ / 1000u);
        d->log_ratio = logf((float)d->tw_stop / (float)d->tw_start);
        if (sweep_changed) {
            d->sweep_pos = 0u;
        }
        d->tw = d->tw_start;
    }
}

/* Tuning word at the current sweep position. Called once per block, so the
 * floating-point exp() of the log sweep costs nothing noticeable. */
static uint32_t sweep_tw(const dds_t *d)
{
    if (d->s.sweep == DDS_SWEEP_LIN) {
        const int64_t span = (int64_t)d->tw_stop - (int64_t)d->tw_start;
        return (uint32_t)((int64_t)d->tw_start + span * (int64_t)d->sweep_pos / (int64_t)d->sweep_len);
    }
    const float frac = (float)d->sweep_pos / (float)d->sweep_len;
    return (uint32_t)((float)d->tw_start * expf(d->log_ratio * frac) + 0.5f);
}

/*
 * PolyBLEP: a two-sample polynomial correction around each jump of a naive
 * square or saw. It replaces the infinitely sharp step (whose harmonics fold
 * back below fs/2 as aliases) with a band-limited approximation.
 *
 * t is the phase since the jump, dt the phase step per sample (both as
 * fractions of 2^32). Returns, in Q15, the correction for a jump of height 2:
 *   just after the jump  (t < dt):       -(1 - t/dt)^2
 *   just before the jump (t > 1 - dt):   +(1 - (1-t)/dt)^2
 */
static int32_t poly_blep(uint32_t t, uint32_t dt)
{
    if (dt == 0u) {
        return 0;
    }
    if (t < dt) {
        const int32_t x = (int32_t)(((uint64_t)t << 15) / dt);    /* Q15, 0..1 */
        const int32_t r = Q15_ONE - x;
        return -((r * r) >> 15);
    }
    const uint32_t before = (uint32_t)(0u - t);                   /* 1 - t */
    if (before < dt) {
        const int32_t y = (int32_t)(((uint64_t)before << 15) / dt);
        const int32_t r = Q15_ONE - y;
        return (r * r) >> 15;
    }
    return 0;
}

int32_t dds_shape(const dds_t *d, uint32_t phase)
{
    int32_t w;
    switch (d->s.wave) {
    case DDS_SINE: {
        /* 256-entry table, linear interpolation on the next 16 phase bits. */
        const uint32_t idx = phase >> (32u - DDS_SINE_BITS);
        const int32_t frac = (int32_t)((phase >> (16u - DDS_SINE_BITS)) & 0xFFFFu);
        const int32_t a = dds_sine_table[idx];
        const int32_t b = dds_sine_table[idx + 1u];
        w = a + (((b - a) * frac) >> 16);
        break;
    }
    case DDS_SQUARE:
        w = (phase < d->duty_phase) ? 32767 : -32767;
        if (d->s.antialias) {
            w += poly_blep(phase, d->tw);                      /* rising edge at 0 */
            w -= poly_blep(phase - d->duty_phase, d->tw);      /* falling edge at duty */
        }
        break;
    case DDS_TRIANGLE: {
        /* Rises from -1 at phase 0 to +1 at half a cycle, then falls. */
        const int32_t u = (int32_t)(phase >> 15);              /* 0 .. 131071 */
        w = (u < 65536) ? u - 32768 : 98304 - u;
        break;
    }
    case DDS_SAW:
        w = (int32_t)(phase >> 16) - 32768;                     /* -1 .. +1, rising */
        if (d->s.antialias) {
            w -= poly_blep(phase, d->tw);                       /* falling jump at 0 */
        }
        break;
    case DDS_NOISE:
    default:
        w = (int32_t)(d->noise >> 16) - 32768;
        break;
    }
    if (w > 32767) {
        w = 32767;
    } else if (w < -32767) {
        w = -32767;
    }
    return w;
}

void dds_render(dds_t *d, uint16_t *out, uint32_t n)
{
    if (d->s.sweep != DDS_SWEEP_OFF && d->sweep_len > 0u) {
        d->tw = sweep_tw(d);
        d->sweep_pos += n;
        if (d->sweep_pos >= d->sweep_len) {
            d->sweep_pos = 0u;
        }
    }

    if (!d->s.output_on) {
        for (uint32_t k = 0; k < n; k++) {
            out[2u * k] = 0u;
            out[2u * k + 1u] = 0u;
        }
        return;
    }

    const int32_t amp = d->amp_pp;
    const int32_t off = d->offset;
    uint32_t phase = d->phase;
    for (uint32_t k = 0; k < n; k++) {
        if (d->s.wave == DDS_NOISE) {
            uint32_t x = d->noise;      /* xorshift32: period 2^32 - 1 */
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            d->noise = x;
        }
        const int32_t w = dds_shape(d, phase);
        int32_t code = off + ((w * amp + Q15_ONE) >> 16);
        if (code < 0) {
            code = 0;
        } else if (code > (int32_t)GEN_FULL_SCALE) {
            code = (int32_t)GEN_FULL_SCALE;
        }
        out[2u * k] = (uint16_t)code;
        out[2u * k + 1u] = (phase < 0x80000000u) ? (uint16_t)GEN_FULL_SCALE : 0u;
        phase += d->tw;
    }
    d->phase = phase;
}
