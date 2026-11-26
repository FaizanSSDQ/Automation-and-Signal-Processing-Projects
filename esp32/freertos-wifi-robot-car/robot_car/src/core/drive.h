/**
 * @file    drive.h
 * @brief   Differential-drive mixing, ramping and PWM mapping (no hardware dependency).
 *
 * Speeds are floats in -1 .. +1 (full reverse .. full forward). The four motors
 * of the car are driven as two sides: the left pair and the right pair.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef DRIVE_H
#define DRIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float throttle;  /* forward (+) / backward (-) */
    float steer;     /* right (+) / left (-) */
} drive_cmd_t;

typedef struct {
    float left;
    float right;
} wheels_t;

/** Clamp to -1 .. +1. */
float drive_clamp(float v);

/**
 * Arcade mixing: left = throttle + steer, right = throttle - steer, scaled
 * down together when either exceeds 1 so the turn ratio is kept.
 */
wheels_t drive_mix(drive_cmd_t cmd);

/** Move @p current towards @p target by at most @p max_step (slew-rate limit). */
float drive_ramp(float current, float target, float max_step);

/**
 * PWM duty for a speed magnitude. Small DC motors do not turn below a minimum
 * duty, so the range 0 < |speed| <= 1 is mapped to min_start .. 1 of @p max_duty;
 * speeds below @p deadband give 0.
 */
uint32_t drive_duty(float speed, uint32_t max_duty, float min_start, float deadband);

/** -1, 0 or +1 for a speed, using the same deadband. */
int drive_dir(float speed, float deadband);

#ifdef __cplusplus
}
#endif

#endif /* DRIVE_H */
