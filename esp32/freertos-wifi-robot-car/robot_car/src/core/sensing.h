/**
 * @file    sensing.h
 * @brief   Sensor conversions and filtering for the robot car (no hardware dependency).
 *
 * Front distance comes from two sensors that fail in different ways:
 *   - HC-SR04 ultrasonic: 2 .. 400 cm, but soft or angled surfaces may not echo
 *   - Sharp GP2Y0A21 infrared: 10 .. 80 cm, unaffected by those surfaces
 * The fused distance is the nearer of the valid readings, so an obstacle seen
 * by either sensor is respected.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef SENSING_H
#define SENSING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define US_MIN_CM     2.0f
#define US_MAX_CM     400.0f
#define SHARP_MIN_CM  10.0f
#define SHARP_MAX_CM  80.0f

/** Echo pulse width (us) to distance (cm): sound travels 0.0343 cm/us, there and back. */
float sense_ultrasonic_cm(uint32_t echo_us, bool *ok);

/** Sharp GP2Y0A21 output voltage to distance (cm), power-law fit d = 27.73 * V^-1.205. */
float sense_sharp_cm(float volts, bool *ok);

/** Battery voltage from the ADC voltage behind a divider of @p ratio (e.g. 3.0 for 20k/10k). */
float sense_battery_v(uint32_t adc_mv, float ratio);

/** Median of five values (does not modify the input). */
float sense_median5(const float v[5]);

/** Nearer of two optional distances; @p ok false if neither is valid. */
float sense_fuse(float a_cm, bool a_ok, float b_cm, bool b_ok, bool *ok);

#ifdef __cplusplus
}
#endif

#endif /* SENSING_H */
