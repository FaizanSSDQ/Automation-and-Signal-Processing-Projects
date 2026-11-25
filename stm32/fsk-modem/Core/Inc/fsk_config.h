/**
 * @file    fsk_config.h
 * @brief   Modem parameters shared by the modulator, demodulator and target glue.
 *
 * The two tones are chosen so that each bit holds a whole number of cycles of both
 * (1 and 2 cycles). Over one bit window they are orthogonal, which makes the
 * correlator demodulator reject the other tone (and any DC offset) exactly.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef FSK_CONFIG_H
#define FSK_CONFIG_H

#define FSK_SAMPLE_RATE_HZ 16000u /**< DAC and ADC sample rate (TIM6 / TIM3) */
#define FSK_BAUD 1000u            /**< bits per second */
#define FSK_SAMPLES_PER_BIT 16u   /**< FSK_SAMPLE_RATE_HZ / FSK_BAUD */
#define FSK_MARK_HZ 1000u         /**< logic 1 and idle line: 1 cycle per bit */
#define FSK_SPACE_HZ 2000u        /**< logic 0: 2 cycles per bit */

#define FSK_DAC_MID 2048           /**< 12-bit mid-scale (1.65 V) */
#define FSK_DAC_AMPLITUDE 1240     /**< peak deviation in DAC codes (~1.0 V) */
#define FSK_SQUELCH_AMPLITUDE 120  /**< minimum received peak (ADC counts) to accept bits */

#if (FSK_SAMPLE_RATE_HZ / FSK_BAUD) != FSK_SAMPLES_PER_BIT
#error "FSK_SAMPLES_PER_BIT must equal FSK_SAMPLE_RATE_HZ / FSK_BAUD"
#endif

#endif /* FSK_CONFIG_H */
