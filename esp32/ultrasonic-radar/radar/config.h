/**
 * @file    config.h
 * @brief   Pins and tuning of the 180-degree radar (ESP32 DevKit, WROOM module).
 *
 * An SG90 servo turns a mount that carries an HC-SR04 ultrasonic sensor and a
 * Sharp GP2Y0A21 infrared sensor side by side. Pins avoid the strapping pins
 * 0, 5, 12 and 15, the UART0 pins 1 and 3 and the flash pins 6..11 (the old
 * Uno sketch used 10, 11 and 12, which on an ESP32 would stop it booting).
 * The Sharp output goes to ADC1, which keeps working while Wi-Fi is on.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- pins ------------------------------------------------------------------------------- */
#define PIN_SERVO       18      /* SG90 signal (servo powered from 5 V, grounds joined) */
#define PIN_US_TRIG     21      /* HC-SR04 trigger (3.3 V is enough) */
#define PIN_US_ECHO     34      /* HC-SR04 echo through a 1k/2k divider (5 V -> 3.3 V) */
#define PIN_SHARP       33      /* Sharp GP2Y0A21 analog output (ADC1) */
#define PIN_LED         2       /* on-board LED, blinks once per sweep */

/* ---- servo ------------------------------------------------------------------------------ */
#define SERVO_PERIOD_US 20000   /* 50 Hz frame */
#define SERVO_BITS      16      /* LEDC resolution: 0.3 us per count */
#define SERVO_MIN_US    500     /* pulse at 0 degrees: trim for your servo */
#define SERVO_MAX_US    2400    /* pulse at 180 degrees */
#define SETTLE_MS       25      /* wait after each 2-degree step before measuring */

/* ---- ranging ---------------------------------------------------------------------------- */
#define PINGS_PER_STEP  3       /* median of three rejects a single bad echo */
#define PING_GAP_MS     15      /* a target at 200 cm echoes after 58 us/cm * 200 = 11.7 ms */
#define ECHO_TIMEOUT_MS 15      /* no echo by then: nothing within range */
#define SHARP_SAMPLES   16      /* ADC readings averaged per step */
#define DEFAULT_TEMP_C  25.0f   /* air temperature, adjustable from the browser */

/* ---- object detection ------------------------------------------------------------------- */
#define OBJECT_MAX_CM   150.0f  /* only report things closer than this */
#define OBJECT_GAP_CM   15.0f   /* a jump in range larger than this starts a new object */

/* ---- Wi-Fi access point ----------------------------------------------------------------- */
#define AP_SSID_PREFIX  "Radar-"        /* followed by the last 4 hex digits of the MAC */
#define AP_PASSWORD     "radar1234"     /* at least 8 characters; change before use */

#endif /* CONFIG_H */
