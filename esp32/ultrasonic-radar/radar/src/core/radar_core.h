/**
 * @file    radar_core.h
 * @brief   Sweep, servo timing, range conversion, filtering and object
 *          detection for the 180-degree radar (no hardware dependency).
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef RADAR_CORE_H
#define RADAR_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RADAR_STEP_DEG    2
#define RADAR_POINTS      (180 / RADAR_STEP_DEG + 1)   /* 0, 2, ... 180 degrees */
#define RADAR_MAX_CM      200.0f
#define SHARP_MIN_CM      10.0f
#define SHARP_MAX_CM      80.0f
#define RADAR_MAX_OBJECTS 8

/* ---- sweep ---------------------------------------------------------------------------- */

typedef struct {
    int16_t angle;   /* current angle, degrees */
    int8_t dir;      /* +1 or -1 */
} sweep_t;

void sweep_init(sweep_t *s);

/** Advance one step and return the new angle; turns around at 0 and 180. */
int16_t sweep_next(sweep_t *s);

/** Index of an angle in the scan arrays (angle / step). */
int radar_index(int angle);

/* ---- servo ---------------------------------------------------------------------------- */

/** Pulse width for an angle (0..180), linear between min_us and max_us. */
uint32_t servo_pulse_us(int angle, uint32_t min_us, uint32_t max_us);

/** PWM duty for a pulse width at the given period and resolution. */
uint32_t servo_duty(uint32_t pulse_us, uint32_t period_us, uint8_t bits);

/* ---- ranging -------------------------------------------------------------------------- */

/** Speed of sound in cm/us at an air temperature: (331.3 + 0.606 T) m/s. */
float sound_cm_per_us(float temp_c);

/** Echo time to distance, half the round trip; ok false outside 2..RADAR_MAX_CM. */
float echo_to_cm(uint32_t echo_us, float temp_c, bool *ok);

/** Sharp GP2Y0A21: d = 27.73 * V^-1.205 cm; ok false outside 10..80 cm. */
float sharp_to_cm(float volts, bool *ok);

/** Median of three values (rejects one outlier). */
float median3(float a, float b, float c);

/* ---- scan and objects ----------------------------------------------------------------- */

typedef struct {
    float us_cm[RADAR_POINTS];     /* ultrasonic range per angle */
    bool us_ok[RADAR_POINTS];
    float ir_cm[RADAR_POINTS];     /* infrared range per angle */
    bool ir_ok[RADAR_POINTS];
    uint32_t sweeps;               /* completed half sweeps */
} scan_t;

typedef struct {
    int16_t start_deg, end_deg;    /* angular extent */
    float bearing_deg;             /* centre */
    float range_cm;                /* nearest point */
} object_t;

void scan_init(scan_t *s);

/**
 * Group neighbouring valid points closer than @p max_cm whose ranges differ by
 * less than @p gap_cm into objects. @return number of objects written.
 */
size_t radar_find_objects(const float *cm, const bool *ok, size_t n, float max_cm, float gap_cm,
                          object_t *out, size_t max_out);

/** JSON with the current angle, both traces and the objects. 0 if it does not fit. */
size_t radar_json(const scan_t *s, int angle, float temp_c, const object_t *obj, size_t n_obj,
                  char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* RADAR_CORE_H */
