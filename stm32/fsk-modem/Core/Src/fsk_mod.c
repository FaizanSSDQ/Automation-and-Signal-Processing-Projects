/**
 * @file    fsk_mod.c
 * @brief   Character framer + phase-continuous FSK modulator (NCO).
 *
 * Tuning word = f / fs * 2^32. With fs = 16 kHz: mark 1 kHz -> 2^28, space 2 kHz -> 2^29.
 * The top 8 bits of the phase accumulator index a 256-entry Q15 sine table.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#include "fsk_mod.h"

#include "fsk_config.h"
#include "fsk_tables.h"

#define QUEUE_MASK (FSK_TX_QUEUE_SIZE - 1u)
#define FRAME_BITS 10u /* start + 8 data + stop */

_Static_assert((FSK_TX_QUEUE_SIZE & QUEUE_MASK) == 0, "queue size must be a power of two");

static uint32_t tuning_word(uint32_t freq_hz)
{
    return (uint32_t)(((uint64_t)freq_hz << 32) / FSK_SAMPLE_RATE_HZ);
}

void fsk_mod_init(fsk_mod_t *m)
{
    m->head = 0;
    m->tail = 0;
    m->shift = 0;
    m->bits_left = 0;
    m->current_bit = 1; /* idle line = mark */
    m->sample_in_bit = 0;
    m->phase = 0;
    m->inc_mark = tuning_word(FSK_MARK_HZ);
    m->inc_space = tuning_word(FSK_SPACE_HZ);
    m->amplitude = FSK_DAC_AMPLITUDE;
    m->mid = FSK_DAC_MID;
}

bool fsk_mod_put(fsk_mod_t *m, uint8_t c)
{
    const uint16_t head = m->head;
    const uint16_t next = (uint16_t)((head + 1u) & QUEUE_MASK);
    if (next == m->tail) {
        return false; /* full */
    }
    m->queue[head] = c;
    m->head = next; /* publish after the data is written */
    return true;
}

size_t fsk_mod_puts(fsk_mod_t *m, const char *s)
{
    size_t n = 0;
    while (s[n] != '\0' && fsk_mod_put(m, (uint8_t)s[n])) {
        n++;
    }
    return n;
}

bool fsk_mod_busy(const fsk_mod_t *m)
{
    return m->bits_left != 0 || m->head != m->tail;
}

/* Select the bit for the next bit period. */
static void next_bit(fsk_mod_t *m)
{
    if (m->bits_left == 0 && m->head != m->tail) {
        const uint8_t c = m->queue[m->tail];
        m->tail = (uint16_t)((m->tail + 1u) & QUEUE_MASK);
        /* LSB first: start(0), d0..d7, stop(1) */
        m->shift = (uint16_t)((1u << 9) | ((uint16_t)c << 1));
        m->bits_left = FRAME_BITS;
    }
    if (m->bits_left != 0) {
        m->current_bit = (uint8_t)(m->shift & 1u);
        m->shift >>= 1;
        m->bits_left--;
    } else {
        m->current_bit = 1; /* idle: continuous mark tone */
    }
}

void fsk_mod_fill(fsk_mod_t *m, uint16_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (m->sample_in_bit == 0) {
            next_bit(m);
        }
        const int32_t s = fsk_sine_q15[m->phase >> 24];
        out[i] = (uint16_t)(m->mid + ((m->amplitude * s) >> 15));
        m->phase += m->current_bit ? m->inc_mark : m->inc_space;
        m->sample_in_bit = (uint8_t)((m->sample_in_bit + 1u) % FSK_SAMPLES_PER_BIT);
    }
}
