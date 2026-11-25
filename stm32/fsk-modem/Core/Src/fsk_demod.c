/**
 * @file    fsk_demod.c
 * @brief   Sliding-correlator FSK demodulator + asynchronous character receiver.
 *
 * Detection
 *   For each tone k, I_k = sum x[m] cos(w_k m) and Q_k = sum x[m] sin(w_k m) over the
 *   last 16 samples (one bit). Because both tones repeat every 16 samples, the sliding
 *   update needs only the sample leaving the window:
 *       I_k += (x[n] - x[n-16]) * cos_k[n mod 16]
 *   Energy E_k = I_k^2 + Q_k^2 is independent of the signal's phase (non-coherent),
 *   and the bit is 1 when E_mark > E_space. All arithmetic is exact integer math.
 *
 * Timing
 *   The window ends at the current sample, so it is fully inside a bit exactly at the
 *   bit's last sample. A start edge is detected about half a bit after the real edge,
 *   so the receiver waits another half bit, then samples every 16 samples.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#include "fsk_demod.h"

#include <string.h>

#include "fsk_tables.h"

#define SPB FSK_SAMPLES_PER_BIT
#define HALF_BIT (SPB / 2u)

void fsk_demod_init(fsk_demod_t *d, fsk_byte_cb_t on_byte, void *ctx)
{
    memset(d, 0, sizeof(*d));
    d->on_byte = on_byte;
    d->ctx = ctx;
    d->bit = 1;
    d->prev_bit = 1;
    d->state = FSK_RX_HUNT;

    /* A tone of peak A gives |I + jQ| ~ A * 16384 * SPB / 2 for the matching reference. */
    const int64_t amp = (int64_t)FSK_SQUELCH_AMPLITUDE * 16384 * SPB / 2;
    d->squelch_energy = amp * amp;
}

static void deliver(fsk_demod_t *d)
{
    d->stats.bytes_ok++;
    if (d->on_byte != NULL) {
        d->on_byte(d->ctx, d->shift);
    }
}

/* Async receiver state machine, clocked once per sample with the current decision. */
static void receive(fsk_demod_t *d, uint8_t bit)
{
    switch (d->state) {
    case FSK_RX_HUNT:
        if (d->prev_bit == 1 && bit == 0) {
            d->state = FSK_RX_START;
            d->countdown = HALF_BIT;
        }
        break;

    case FSK_RX_START:
        if (--d->countdown == 0) {
            if (bit == 0) {
                d->state = FSK_RX_DATA;
                d->countdown = SPB;
                d->bit_count = 0;
                d->shift = 0;
            } else {
                d->stats.false_starts++; /* noise spike, not a start bit */
                d->state = FSK_RX_HUNT;
            }
        }
        break;

    case FSK_RX_DATA:
        if (--d->countdown == 0) {
            d->shift = (uint8_t)((d->shift >> 1) | (bit << 7)); /* LSB first */
            d->countdown = SPB;
            if (++d->bit_count == 8) {
                d->state = FSK_RX_STOP;
            }
        }
        break;

    case FSK_RX_STOP:
        if (--d->countdown == 0) {
            if (bit == 1) {
                deliver(d);
            } else {
                d->stats.framing_errors++;
            }
            d->state = FSK_RX_HUNT;
        }
        break;
    }
    d->prev_bit = bit;
}

void fsk_demod_process(fsk_demod_t *d, const uint16_t *samples, size_t n)
{
    for (size_t k = 0; k < n; k++) {
        const int16_t x = (int16_t)((int32_t)samples[k] - FSK_DAC_MID);
        const uint8_t i = d->index;
        const int32_t delta = (int32_t)x - d->history[i];
        d->history[i] = x;
        d->index = (uint8_t)((i + 1u) % SPB);

        d->mark_i += delta * fsk_ref_mark_cos[i];
        d->mark_q += delta * fsk_ref_mark_sin[i];
        d->space_i += delta * fsk_ref_space_cos[i];
        d->space_q += delta * fsk_ref_space_sin[i];

        d->mark_energy = (int64_t)d->mark_i * d->mark_i + (int64_t)d->mark_q * d->mark_q;
        d->space_energy = (int64_t)d->space_i * d->space_i + (int64_t)d->space_q * d->space_q;
        d->signal = (d->mark_energy + d->space_energy) > d->squelch_energy;

        /* No signal is treated as an idle (mark) line. */
        d->bit = (!d->signal || d->mark_energy >= d->space_energy) ? 1u : 0u;
        receive(d, d->bit);
    }
}
