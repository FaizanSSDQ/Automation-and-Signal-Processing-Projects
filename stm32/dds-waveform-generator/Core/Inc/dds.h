/**
 * @file    dds.h
 * @brief   Direct digital synthesis core: phase accumulator, waveform shapes,
 *          PolyBLEP anti-aliasing, frequency sweeps and output scaling.
 *
 * Every output sample:
 *   phase += tuning_word                 (32-bit, wraps once per cycle)
 *   w      = shape(phase)                (Q15, -1 .. +1)
 *   code   = offset + w * amplitude      (PWM compare value, 0 .. GEN_FULL_SCALE)
 *
 * tuning_word = f * 2^32 / fs, so the frequency resolution is
 * fs / 2^32 = 125 kHz / 4.29e9 = 29 uHz.
 *
 * The core has no hardware dependencies; host tests and the sample-dump tool
 * run the same code as the firmware.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef DDS_H
#define DDS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DDS_SINE = 0,
    DDS_SQUARE,
    DDS_TRIANGLE,
    DDS_SAW,
    DDS_NOISE,
    DDS_WAVE_COUNT
} dds_wave_t;

typedef enum {
    DDS_SWEEP_OFF = 0,
    DDS_SWEEP_LIN,
    DDS_SWEEP_LOG
} dds_sweep_t;

/** User settings, in human units. */
typedef struct {
    dds_wave_t wave;
    uint32_t freq_mhz;        /* frequency in milli-hertz */
    uint16_t amp_mv;          /* peak-to-peak amplitude */
    uint16_t offset_mv;       /* centre voltage */
    uint8_t duty_pct;         /* square wave high time, 1..99 */
    bool output_on;
    bool antialias;           /* PolyBLEP on square and saw edges */
    dds_sweep_t sweep;
    uint32_t sweep_start_mhz;
    uint32_t sweep_stop_mhz;
    uint32_t sweep_ms;        /* duration of one sweep, then it restarts */
} dds_settings_t;

/** Engine state, derived from the settings. */
typedef struct {
    dds_settings_t s;
    uint32_t phase;
    uint32_t tw;              /* current tuning word */
    uint32_t duty_phase;      /* phase at which the square wave falls */
    int32_t amp_pp;           /* peak-to-peak amplitude in PWM counts */
    int32_t offset;           /* centre in PWM counts */
    uint32_t noise;           /* xorshift32 state */
    /* sweep */
    uint32_t tw_start, tw_stop;
    uint32_t sweep_len;       /* samples per sweep */
    uint32_t sweep_pos;       /* samples into the current sweep */
    float log_ratio;          /* ln(stop / start) for log sweeps */
} dds_t;

/** Power-on settings: 1 kHz sine, 2 Vpp around 1.65 V, output on. */
void dds_settings_default(dds_settings_t *s);

/** Start the engine from phase 0. */
void dds_init(dds_t *d, const dds_settings_t *s);

/** Change settings without resetting the phase (no glitch on frequency changes). */
void dds_apply(dds_t *d, const dds_settings_t *s);

/**
 * Render @p n samples as (main, sync) pairs of PWM compare values:
 * out[2k] drives the waveform output, out[2k+1] the sync output, which is
 * high for the first half of every cycle.
 */
void dds_render(dds_t *d, uint16_t *out, uint32_t n);

/** Q15 sample of a waveform at a phase (tw is needed for anti-aliasing). */
int32_t dds_shape(const dds_t *d, uint32_t phase);

/** Tuning word for a frequency in milli-hertz, rounded. */
uint32_t dds_tuning_word(uint32_t freq_mhz);

/** Exact output frequency of a tuning word, in milli-hertz (rounded). */
uint32_t dds_freq_mhz(uint32_t tw);

/** Millivolts to PWM counts, rounded. */
int32_t dds_mv_to_counts(uint32_t mv);

#endif /* DDS_H */
