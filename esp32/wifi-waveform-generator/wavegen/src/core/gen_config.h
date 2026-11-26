/**
 * @file    gen_config.h
 * @brief   Waveform generator constants (shared by firmware and host tests).
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef GEN_CONFIG_H
#define GEN_CONFIG_H

/* Samples per second on each DAC channel. The DAC DMA alternates between the
 * two channels, so the converter itself runs at twice this rate. */
#define GEN_SAMPLE_RATE_HZ   100000u

/* 8-bit DAC: code 255 is (about) the 3.3 V supply. */
#define GEN_FULL_SCALE       255u
#define GEN_VREF_MV          3300u

/* Samples rendered per DMA write: 5.12 ms at 100 kHz. */
#define GEN_BLOCK            512u

/* Frequency limits in milli-hertz. Above ~10 kHz there are fewer than ten
 * samples per cycle; the reconstruction filter then dominates the shape. */
#define GEN_FREQ_MIN_MHZ     10u                  /* 0.01 Hz */
#define GEN_FREQ_MAX_MHZ     20000000u            /* 20 kHz */

#define GEN_SWEEP_MIN_MS     10u
#define GEN_SWEEP_MAX_MS     600000u              /* 10 minutes */

#define GEN_PREVIEW_MAX_MS   200u                 /* longest stretch shown on the page */

#endif /* GEN_CONFIG_H */
