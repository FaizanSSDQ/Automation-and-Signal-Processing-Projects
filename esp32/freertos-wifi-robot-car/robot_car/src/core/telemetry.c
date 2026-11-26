/**
 * @file    telemetry.c
 * @brief   Command parsing and telemetry JSON for the web interface.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "telemetry.h"

#include <stdio.h>
#include <stdlib.h>

static bool parse_int(const char *text, long lo, long hi, long *out)
{
    if (text == NULL || *text == '\0') {
        return false;
    }
    char *end;
    const long v = strtol(text, &end, 10);
    if (*end != '\0' || v < lo || v > hi) {
        return false;
    }
    *out = v;
    return true;
}

bool telemetry_parse_axis(const char *text, float *out)
{
    long v;
    if (!parse_int(text, -100, 100, &v)) {
        return false;
    }
    *out = (float)v / 100.0f;
    return true;
}

bool telemetry_parse_limit(const char *text, float *out)
{
    long v;
    if (!parse_int(text, 10, 100, &v)) {
        return false;
    }
    *out = (float)v / 100.0f;
    return true;
}

static int pct(float v)
{
    return (int)(v * 100.0f + (v >= 0.0f ? 0.5f : -0.5f));
}

size_t telemetry_json(const telemetry_t *t, char *out, size_t cap)
{
    const sensors_t *s = &t->sensors;
    const int n = snprintf(out, cap,
                           "{\"mode\":\"%s\",\"stop\":\"%s\",\"l\":%d,\"r\":%d,\"d\":%.1f,\"dok\":%d,"
                           "\"irl\":%d,\"irr\":%d,\"bat\":%.2f,\"rssi\":%d,\"up\":%lu}",
                           autopilot_mode_name(t->mode), autopilot_stop_name(t->stop), pct(t->left),
                           pct(t->right), (double)s->distance_cm, s->distance_ok ? 1 : 0, s->ir_left ? 1 : 0,
                           s->ir_right ? 1 : 0, (double)s->battery_v, t->rssi, (unsigned long)t->uptime_s);
    if (n < 0 || (size_t)n >= cap) {
        if (cap) {
            out[0] = '\0';
        }
        return 0;
    }
    return (size_t)n;
}
