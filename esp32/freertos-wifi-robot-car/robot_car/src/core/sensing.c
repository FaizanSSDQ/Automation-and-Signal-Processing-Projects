/**
 * @file    sensing.c
 * @brief   Sensor conversions and filtering for the robot car.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "sensing.h"

#include <math.h>

float sense_ultrasonic_cm(uint32_t echo_us, bool *ok)
{
    const float cm = (float)echo_us * 0.0343f / 2.0f;
    *ok = echo_us > 0u && cm >= US_MIN_CM && cm <= US_MAX_CM;
    return cm;
}

float sense_sharp_cm(float volts, bool *ok)
{
    if (volts <= 0.05f) {
        *ok = false;
        return SHARP_MAX_CM;
    }
    const float cm = 27.73f * powf(volts, -1.205f);
    *ok = cm >= SHARP_MIN_CM && cm <= SHARP_MAX_CM;
    return cm;
}

float sense_battery_v(uint32_t adc_mv, float ratio)
{
    return (float)adc_mv * ratio / 1000.0f;
}

float sense_median5(const float v[5])
{
    float a[5];
    for (int i = 0; i < 5; i++) {
        a[i] = v[i];
    }
    for (int i = 1; i < 5; i++) {        /* insertion sort, 5 elements */
        const float x = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > x) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = x;
    }
    return a[2];
}

float sense_fuse(float a_cm, bool a_ok, float b_cm, bool b_ok, bool *ok)
{
    *ok = a_ok || b_ok;
    if (a_ok && b_ok) {
        return fminf(a_cm, b_cm);
    }
    if (a_ok) {
        return a_cm;
    }
    if (b_ok) {
        return b_cm;
    }
    return US_MAX_CM;
}
