/**
 * @file    config.h
 * @brief   Pins and tuning of the Wi-Fi robot car (ESP32 DevKit, WROOM module).
 *
 * Four DC motors on two L298N drivers: driver 1 runs the front pair, driver 2
 * the back pair. The firmware drives them as two sides (left / right).
 * All pins avoid the boot-strapping pins 0, 2, 5, 12 and 15 (except the
 * on-board LED on GPIO 2, an output that is set only after boot) and the
 * UART0 pins 1 and 3. Analog inputs use ADC1, which still works while Wi-Fi
 * is on (ADC2 does not).
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- motors: EN = PWM speed, IN = direction -------------------------------------------- */
#define PIN_FL_EN   25   /* driver 1, motor A: front left  */
#define PIN_FL_IN1  26
#define PIN_FL_IN2  27
#define PIN_FR_EN   4    /* driver 1, motor B: front right */
#define PIN_FR_IN1  14
#define PIN_FR_IN2  13
#define PIN_BL_EN   23   /* driver 2, motor A: back left   */
#define PIN_BL_IN1  16
#define PIN_BL_IN2  17
#define PIN_BR_EN   22   /* driver 2, motor B: back right  */
#define PIN_BR_IN1  18
#define PIN_BR_IN2  19

/* Flip a side if its motors run backwards after wiring. */
#define LEFT_REVERSED   0
#define RIGHT_REVERSED  0

#define PWM_FREQ_HZ     20000   /* above hearing, fine for L298N */
#define PWM_BITS        10
#define PWM_MAX_DUTY    ((1u << PWM_BITS) - 1u)
#define MOTOR_MIN_START 0.35f   /* duty needed to overcome friction */
#define MOTOR_DEADBAND  0.03f

/* ---- sensors ---------------------------------------------------------------------------- */
#define PIN_US_TRIG     21      /* HC-SR04 trigger (3.3 V is enough) */
#define PIN_US_ECHO     34      /* HC-SR04 echo through a 1k/2k divider (5 V -> 3.3 V) */
#define PIN_IR_LEFT     36      /* line sensors (TCRT5000 modules), input only */
#define PIN_IR_RIGHT    39
#define IR_LINE_LEVEL   HIGH    /* level the module outputs over a black line */
#define PIN_SHARP       33      /* Sharp GP2Y0A21 analog output (ADC1) */
#define PIN_BATTERY     32      /* battery through 20k/10k divider (ADC1) */
#define BATTERY_RATIO   3.0f
#define PIN_LED         2       /* on-board LED */

/* ---- Wi-Fi access point ----------------------------------------------------------------- */
#define AP_SSID_PREFIX  "RobotCar-"     /* followed by the last 4 hex digits of the MAC */
#define AP_PASSWORD     "robotcar123"   /* at least 8 characters; change before use */

/* ---- timing ----------------------------------------------------------------------------- */
#define CONTROL_HZ      50              /* motor control loop */
#define SENSOR_HZ       20              /* sensor sampling */
#define RAMP_PER_S      4.0f            /* max wheel speed change per second (0 -> full in 250 ms) */

#endif /* CONFIG_H */
