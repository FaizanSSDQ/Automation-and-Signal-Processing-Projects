/**
 * @file    telemetry.h
 * @brief   Command parsing and telemetry JSON for the web interface (no hardware dependency).
 *
 * The browser sends   /cmd?x=<-100..100>&y=<-100..100>&max=<10..100>
 * and receives        {"mode":"manual","stop":"","l":42,"r":40,"d":87.5,"dok":1,
 *                      "irl":0,"irr":0,"bat":7.62,"rssi":-48,"up":123}
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "autopilot.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    car_mode_t mode;
    stop_reason_t stop;
    float left, right;      /* applied wheel speeds, -1 .. 1 */
    sensors_t sensors;
    int rssi;               /* dBm of the connected phone, 0 if unknown */
    uint32_t uptime_s;
} telemetry_t;

/** Parse an integer joystick axis (-100..100) into -1..1; false if missing or invalid. */
bool telemetry_parse_axis(const char *text, float *out);

/** Parse the speed limit (10..100 %) into 0.1..1; false if missing or invalid. */
bool telemetry_parse_limit(const char *text, float *out);

/** JSON object for the browser. @return characters written (0 if it does not fit). */
size_t telemetry_json(const telemetry_t *t, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_H */
