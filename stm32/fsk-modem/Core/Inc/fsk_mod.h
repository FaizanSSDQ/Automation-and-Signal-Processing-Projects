/**
 * @file    fsk_mod.h
 * @brief   Asynchronous character framer and phase-continuous FSK modulator.
 *
 * Characters are sent like a UART: idle = mark, 1 start bit (space), 8 data bits
 * LSB first, 1 stop bit (mark). The modulator is a numerically controlled oscillator
 * (32-bit phase accumulator + sine table), so switching tones never causes a phase jump.
 *
 * Hardware independent: compiled for the STM32 and for host unit tests.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef FSK_MOD_H
#define FSK_MOD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FSK_TX_QUEUE_SIZE 128u /**< power of two */

typedef struct {
    /* character queue (single producer, single consumer) */
    volatile uint8_t queue[FSK_TX_QUEUE_SIZE];
    volatile uint16_t head; /**< written by the producer */
    volatile uint16_t tail; /**< written by the consumer */
    /* framing state */
    uint16_t shift;     /**< remaining bits of the current character, LSB first */
    uint8_t bits_left;  /**< 0 = idle */
    uint8_t current_bit;
    uint8_t sample_in_bit;
    /* oscillator */
    uint32_t phase;
    uint32_t inc_mark;
    uint32_t inc_space;
    int32_t amplitude;
    int32_t mid;
} fsk_mod_t;

/** Initialise with the default tones and amplitude from fsk_config.h. */
void fsk_mod_init(fsk_mod_t *m);

/** Queue one character. Returns false if the queue is full. Safe to call from an ISR. */
bool fsk_mod_put(fsk_mod_t *m, uint8_t c);

/** Queue a NUL-terminated string; returns the number of characters accepted. */
size_t fsk_mod_puts(fsk_mod_t *m, const char *s);

/** True while characters are queued or being transmitted. */
bool fsk_mod_busy(const fsk_mod_t *m);

/** Produce @p n output samples as 12-bit DAC codes (mid +/- amplitude). */
void fsk_mod_fill(fsk_mod_t *m, uint16_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* FSK_MOD_H */
