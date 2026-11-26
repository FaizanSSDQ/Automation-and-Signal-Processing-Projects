/**
 * @file    drive.c
 * @brief   Differential-drive mixing, ramping and PWM mapping.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "drive.h"

#include <math.h>

float drive_clamp(float v)
{
    if (v > 1.0f) {
        return 1.0f;
    }
    if (v < -1.0f) {
        return -1.0f;
    }
    return v;
}

wheels_t drive_mix(drive_cmd_t cmd)
{
    const float t = drive_clamp(cmd.throttle);
    const float s = drive_clamp(cmd.steer);
    wheels_t w = {t + s, t - s};
    const float peak = fmaxf(fabsf(w.left), fabsf(w.right));
    if (peak > 1.0f) {
        w.left /= peak;
        w.right /= peak;
    }
    return w;
}

float drive_ramp(float current, float target, float max_step)
{
    const float d = target - current;
    if (d > max_step) {
        return current + max_step;
    }
    if (d < -max_step) {
        return current - max_step;
    }
    return target;
}

uint32_t drive_duty(float speed, uint32_t max_duty, float min_start, float deadband)
{
    const float m = fminf(fabsf(speed), 1.0f);
    if (m < deadband) {
        return 0u;
    }
    const float frac = min_start + (1.0f - min_start) * m;
    return (uint32_t)(frac * (float)max_duty + 0.5f);
}

int drive_dir(float speed, float deadband)
{
    if (speed >= deadband) {
        return 1;
    }
    if (speed <= -deadband) {
        return -1;
    }
    return 0;
}
