/**
 * @file    motors.cpp
 * @brief   Two L298N drivers, four DC motors, driven as left and right sides.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <Arduino.h>

#include "config.h"
#include "motors.h"
#include "src/core/drive.h"

namespace {

struct Motor {
    uint8_t en, in1, in2;
};

const Motor M_FL = {PIN_FL_EN, PIN_FL_IN1, PIN_FL_IN2};
const Motor M_FR = {PIN_FR_EN, PIN_FR_IN1, PIN_FR_IN2};
const Motor M_BL = {PIN_BL_EN, PIN_BL_IN1, PIN_BL_IN2};
const Motor M_BR = {PIN_BR_EN, PIN_BR_IN1, PIN_BR_IN2};
const Motor *const ALL[] = {&M_FL, &M_FR, &M_BL, &M_BR};

void drive_motor(const Motor &m, float speed)
{
    const int dir = drive_dir(speed, MOTOR_DEADBAND);
    digitalWrite(m.in1, dir > 0 ? HIGH : LOW);   /* both low with EN = 0: coast */
    digitalWrite(m.in2, dir < 0 ? HIGH : LOW);
    ledcWrite(m.en, drive_duty(speed, PWM_MAX_DUTY, MOTOR_MIN_START, MOTOR_DEADBAND));
}

}  // namespace

bool motors_begin()
{
    bool ok = true;
    for (const Motor *m : ALL) {
        pinMode(m->in1, OUTPUT);
        pinMode(m->in2, OUTPUT);
        digitalWrite(m->in1, LOW);
        digitalWrite(m->in2, LOW);
        ok = ledcAttach(m->en, PWM_FREQ_HZ, PWM_BITS) && ok;
        ledcWrite(m->en, 0);
    }
    return ok;
}

void motors_set(float left, float right)
{
    if (LEFT_REVERSED) {
        left = -left;
    }
    if (RIGHT_REVERSED) {
        right = -right;
    }
    drive_motor(M_FL, left);
    drive_motor(M_BL, left);
    drive_motor(M_FR, right);
    drive_motor(M_BR, right);
}

void motors_stop()
{
    motors_set(0.0f, 0.0f);
}
