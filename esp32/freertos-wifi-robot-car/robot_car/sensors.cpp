/**
 * @file    sensors.cpp
 * @brief   Reading the car's sensors: ultrasonic, Sharp infrared, line sensors, battery.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <Arduino.h>

#include "config.h"
#include "sensors.h"
#include "src/core/sensing.h"

#define ECHO_TIMEOUT_US 25000u   /* ~4.3 m round trip; beyond that: no obstacle */

void sensors_begin()
{
    pinMode(PIN_US_TRIG, OUTPUT);
    digitalWrite(PIN_US_TRIG, LOW);
    pinMode(PIN_US_ECHO, INPUT);
    pinMode(PIN_IR_LEFT, INPUT);
    pinMode(PIN_IR_RIGHT, INPUT);
    analogSetPinAttenuation(PIN_SHARP, ADC_11db);     /* 0 .. ~3.1 V */
    analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
}

static uint32_t echo_us()
{
    digitalWrite(PIN_US_TRIG, LOW);
    delayMicroseconds(3);
    digitalWrite(PIN_US_TRIG, HIGH);      /* 10 us trigger pulse */
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);
    return pulseIn(PIN_US_ECHO, HIGH, ECHO_TIMEOUT_US);
}

sensors_t sensors_read()
{
    sensors_t s = {};

    bool us_ok;
    const float us_cm = sense_ultrasonic_cm(echo_us(), &us_ok);

    float v[5];
    for (int i = 0; i < 5; i++) {         /* the Sharp output is noisy: median of 5 */
        v[i] = analogReadMilliVolts(PIN_SHARP) / 1000.0f;
    }
    bool ir_ok;
    const float ir_cm = sense_sharp_cm(sense_median5(v), &ir_ok);

    s.distance_cm = sense_fuse(us_cm, us_ok, ir_cm, ir_ok, &s.distance_ok);
    s.ir_left = digitalRead(PIN_IR_LEFT) == IR_LINE_LEVEL;
    s.ir_right = digitalRead(PIN_IR_RIGHT) == IR_LINE_LEVEL;
    s.battery_v = sense_battery_v(analogReadMilliVolts(PIN_BATTERY), BATTERY_RATIO);
    return s;
}
