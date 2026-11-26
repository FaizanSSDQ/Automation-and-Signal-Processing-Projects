/**
 * @file    radar_core.c
 * @brief   Sweep, servo timing, range conversion, filtering and object detection.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "radar_core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void sweep_init(sweep_t *s)
{
    s->angle = 0;
    s->dir = 1;
}

int16_t sweep_next(sweep_t *s)
{
    int next = s->angle + s->dir * RADAR_STEP_DEG;
    if (next > 180 || next < 0) {
        s->dir = (int8_t)-s->dir;
        next = s->angle + s->dir * RADAR_STEP_DEG;
    }
    s->angle = (int16_t)next;
    return s->angle;
}

int radar_index(int angle)
{
    if (angle < 0) {
        angle = 0;
    }
    if (angle > 180) {
        angle = 180;
    }
    return angle / RADAR_STEP_DEG;
}

uint32_t servo_pulse_us(int angle, uint32_t min_us, uint32_t max_us)
{
    if (angle < 0) {
        angle = 0;
    }
    if (angle > 180) {
        angle = 180;
    }
    return min_us + (uint32_t)(((uint64_t)(max_us - min_us) * (uint32_t)angle + 90u) / 180u);
}

uint32_t servo_duty(uint32_t pulse_us, uint32_t period_us, uint8_t bits)
{
    const uint64_t full = (1ull << bits) - 1u;
    return (uint32_t)((pulse_us * full + period_us / 2u) / period_us);
}

float sound_cm_per_us(float temp_c)
{
    return (331.3f + 0.606f * temp_c) / 10000.0f;
}

float echo_to_cm(uint32_t echo_us, float temp_c, bool *ok)
{
    const float cm = (float)echo_us * sound_cm_per_us(temp_c) / 2.0f;
    *ok = echo_us > 0u && cm >= 2.0f && cm <= RADAR_MAX_CM;
    return cm;
}

float sharp_to_cm(float volts, bool *ok)
{
    if (volts <= 0.05f) {
        *ok = false;
        return SHARP_MAX_CM;
    }
    const float cm = 27.73f * powf(volts, -1.205f);
    *ok = cm >= SHARP_MIN_CM && cm <= SHARP_MAX_CM;
    return cm;
}

float median3(float a, float b, float c)
{
    if (a > b) {
        const float t = a;
        a = b;
        b = t;
    }
    if (b > c) {
        b = c;
    }
    return a > b ? a : b;
}

void scan_init(scan_t *s)
{
    memset(s, 0, sizeof(*s));
}

size_t radar_find_objects(const float *cm, const bool *ok, size_t n, float max_cm, float gap_cm,
                          object_t *out, size_t max_out)
{
    size_t count = 0;
    size_t i = 0;
    while (i < n && count < max_out) {
        if (!ok[i] || cm[i] > max_cm) {
            i++;
            continue;
        }
        size_t j = i;
        float nearest = cm[i];
        while (j + 1 < n && ok[j + 1] && cm[j + 1] <= max_cm && fabsf(cm[j + 1] - cm[j]) < gap_cm) {
            j++;
            if (cm[j] < nearest) {
                nearest = cm[j];
            }
        }
        object_t *o = &out[count++];
        o->start_deg = (int16_t)(i * RADAR_STEP_DEG);
        o->end_deg = (int16_t)(j * RADAR_STEP_DEG);
        o->bearing_deg = (float)(o->start_deg + o->end_deg) / 2.0f;
        o->range_cm = nearest;
        i = j + 1;
    }
    return count;
}

static size_t put_trace(char *out, size_t cap, const char *name, const float *cm, const bool *ok)
{
    size_t len = 0;
    int w = snprintf(out, cap, "\"%s\":[", name);
    if (w < 0 || (size_t)w >= cap) {
        return 0;
    }
    len = (size_t)w;
    for (int i = 0; i < RADAR_POINTS; i++) {
        w = ok[i] ? snprintf(out + len, cap - len, "%s%.0f", i ? "," : "", (double)cm[i])
                  : snprintf(out + len, cap - len, "%s-1", i ? "," : "");
        if (w < 0 || (size_t)w >= cap - len) {
            return 0;
        }
        len += (size_t)w;
    }
    if (len + 1 >= cap) {
        return 0;
    }
    out[len++] = ']';
    out[len] = '\0';
    return len;
}

size_t radar_json(const scan_t *s, int angle, float temp_c, const object_t *obj, size_t n_obj,
                  char *out, size_t cap)
{
    size_t len, n;
    int w = snprintf(out, cap, "{\"angle\":%d,\"step\":%d,\"temp\":%.1f,\"sweeps\":%lu,", angle, RADAR_STEP_DEG,
                     (double)temp_c, (unsigned long)s->sweeps);
    if (w < 0 || (size_t)w >= cap) {
        goto overflow;
    }
    len = (size_t)w;
    if ((n = put_trace(out + len, cap - len, "us", s->us_cm, s->us_ok)) == 0) {
        goto overflow;
    }
    len += n;
    if (len + 1 >= cap) {
        goto overflow;
    }
    out[len++] = ',';
    if ((n = put_trace(out + len, cap - len, "ir", s->ir_cm, s->ir_ok)) == 0) {
        goto overflow;
    }
    len += n;
    w = snprintf(out + len, cap - len, ",\"objects\":[");
    if (w < 0 || (size_t)w >= cap - len) {
        goto overflow;
    }
    len += (size_t)w;
    for (size_t k = 0; k < n_obj; k++) {
        w = snprintf(out + len, cap - len, "%s{\"from\":%d,\"to\":%d,\"bearing\":%.0f,\"range\":%.0f}",
                     k ? "," : "", obj[k].start_deg, obj[k].end_deg, (double)obj[k].bearing_deg,
                     (double)obj[k].range_cm);
        if (w < 0 || (size_t)w >= cap - len) {
            goto overflow;
        }
        len += (size_t)w;
    }
    w = snprintf(out + len, cap - len, "]}");
    if (w < 0 || (size_t)w >= cap - len) {
        goto overflow;
    }
    return len + (size_t)w;

overflow:
    if (cap) {
        out[0] = '\0';
    }
    return 0;
}
