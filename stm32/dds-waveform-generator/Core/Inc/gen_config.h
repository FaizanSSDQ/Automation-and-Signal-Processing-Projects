/**
 * @file    gen_config.h
 * @brief   DDS waveform generator constants (shared by firmware and host tests).
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef GEN_CONFIG_H
#define GEN_CONFIG_H

/* TIM3 runs from the 64 MHz timer clock. One PWM period is one output sample. */
#define GEN_TIMER_CLOCK_HZ   64000000u
#define GEN_PWM_PERIOD       512u                 /* ARR = 511: 9-bit resolution */
#define GEN_SAMPLE_RATE_HZ   (GEN_TIMER_CLOCK_HZ / GEN_PWM_PERIOD)   /* 125 kHz */

/* Compare value for 100 % duty (CCR > ARR keeps the output high). */
#define GEN_FULL_SCALE       GEN_PWM_PERIOD

/* PWM high level = supply voltage. */
#define GEN_VREF_MV          3300u

/* Samples per half of the ping-pong buffer: 2.048 ms at 125 kHz. */
#define GEN_BLOCK            256u

/* Frequency limits in milli-hertz. Above ~10 kHz there are fewer than 12
 * samples per cycle; the reconstruction filter then dominates the shape. */
#define GEN_FREQ_MIN_MHZ     10u                  /* 0.01 Hz */
#define GEN_FREQ_MAX_MHZ     20000000u            /* 20 kHz */

#define GEN_SWEEP_MIN_MS     10u
#define GEN_SWEEP_MAX_MS     600000u              /* 10 minutes */

#define GEN_FW_VERSION       "1.0.0"

#endif /* GEN_CONFIG_H */
