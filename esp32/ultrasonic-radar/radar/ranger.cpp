/**
 * @file    ranger.cpp
 * @brief   HC-SR04 echo timing by interrupt, Sharp IR reading, servo output.
 *
 * pulseIn() busy-waits, up to a full second when nothing answers. Here the
 * echo pin raises an interrupt on both edges; the handler stamps them with the
 * microsecond timer and, on the falling edge, wakes the waiting task with a
 * task notification. The task sleeps in the meantime, so the CPU is free for
 * the other tasks, and a missing echo costs at most ECHO_TIMEOUT_MS (plus
 * the sensor's own recovery time before the next ping).
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "ranger.h"

#include <Arduino.h>

#include "config.h"
#include "src/core/radar_core.h"

static volatile int64_t rise_us;
static volatile uint32_t width_us;
static volatile TaskHandle_t waiter;

static void IRAM_ATTR on_echo_edge()
{
    const int64_t now = esp_timer_get_time();
    if (digitalRead(PIN_US_ECHO)) {
        rise_us = now;
        return;
    }
    if (rise_us == 0 || waiter == nullptr) {
        return;   /* falling edge without a rising one, or nobody waiting */
    }
    width_us = (uint32_t)(now - rise_us);
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(waiter, &woken);
    portYIELD_FROM_ISR(woken);
}

void ranger_begin()
{
    pinMode(PIN_US_TRIG, OUTPUT);
    digitalWrite(PIN_US_TRIG, LOW);
    pinMode(PIN_US_ECHO, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_US_ECHO), on_echo_edge, CHANGE);

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_SHARP, ADC_11db);   /* full 0..3.1 V range */

    ledcAttach(PIN_SERVO, 1000000 / SERVO_PERIOD_US, SERVO_BITS);
}

uint32_t ranger_ping_us()
{
    /* After a lost echo the HC-SR04 holds its output high for about 38 ms and
     * ignores triggers until it drops; wait for that instead of losing this ping too. */
    for (int t = 0; t < 50 && digitalRead(PIN_US_ECHO); t++) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    rise_us = 0;
    width_us = 0;
    waiter = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);   /* clear a stale notification */

    digitalWrite(PIN_US_TRIG, HIGH);   /* 10 us trigger pulse */
    delayMicroseconds(10);
    digitalWrite(PIN_US_TRIG, LOW);

    const bool got = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ECHO_TIMEOUT_MS)) > 0;
    waiter = nullptr;
    return got ? width_us : 0u;
}

float ranger_sharp_volts()
{
    uint32_t mv = 0;
    for (int i = 0; i < SHARP_SAMPLES; i++) {
        mv += analogReadMilliVolts(PIN_SHARP);   /* uses the factory ADC calibration */
    }
    return (float)mv / SHARP_SAMPLES / 1000.0f;
}

void servo_write(int angle)
{
    const uint32_t pulse = servo_pulse_us(angle, SERVO_MIN_US, SERVO_MAX_US);
    ledcWrite(PIN_SERVO, servo_duty(pulse, SERVO_PERIOD_US, SERVO_BITS));
}
