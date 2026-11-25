/**
 * @file    fsk_demod.h
 * @brief   Non-coherent FSK demodulator (sliding correlators) with an asynchronous
 *          character receiver.
 *
 * Hardware independent: compiled for the STM32 and for host unit tests.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef FSK_DEMOD_H
#define FSK_DEMOD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fsk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FSK_RX_HUNT = 0, /**< waiting for a mark -> space edge (start bit) */
    FSK_RX_START,    /**< confirming the start bit at its centre */
    FSK_RX_DATA,     /**< sampling 8 data bits */
    FSK_RX_STOP,     /**< checking the stop bit */
} fsk_rx_state_t;

typedef struct {
    uint32_t bytes_ok;
    uint32_t framing_errors; /**< stop bit was not a mark */
    uint32_t false_starts;   /**< start edge not confirmed half a bit later */
} fsk_rx_stats_t;

typedef void (*fsk_byte_cb_t)(void *ctx, uint8_t byte);

typedef struct {
    /* sliding correlators */
    int16_t history[FSK_SAMPLES_PER_BIT];
    uint8_t index;
    int32_t mark_i, mark_q, space_i, space_q;
    int64_t squelch_energy;
    /* last decision (exposed for debugging / plotting) */
    int64_t mark_energy, space_energy;
    uint8_t bit;
    bool signal;
    /* async receiver */
    fsk_rx_state_t state;
    uint8_t countdown;
    uint8_t bit_count;
    uint8_t shift;
    uint8_t prev_bit;
    fsk_rx_stats_t stats;
    fsk_byte_cb_t on_byte;
    void *ctx;
} fsk_demod_t;

/** Initialise; @p on_byte is called for every correctly framed character. */
void fsk_demod_init(fsk_demod_t *d, fsk_byte_cb_t on_byte, void *ctx);

/** Feed raw 12-bit ADC samples (offset binary, mid-scale = 2048). */
void fsk_demod_process(fsk_demod_t *d, const uint16_t *samples, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* FSK_DEMOD_H */
