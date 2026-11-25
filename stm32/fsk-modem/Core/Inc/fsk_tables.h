/**
 * @file    fsk_tables.h
 * @brief   Lookup tables used by the FSK modulator and demodulator (see tools/gen_tables.py).
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef FSK_TABLES_H
#define FSK_TABLES_H

#include <stdint.h>

extern const int16_t fsk_sine_q15[256];
extern const int16_t fsk_ref_mark_cos[16];
extern const int16_t fsk_ref_mark_sin[16];
extern const int16_t fsk_ref_space_cos[16];
extern const int16_t fsk_ref_space_sin[16];

#endif /* FSK_TABLES_H */
