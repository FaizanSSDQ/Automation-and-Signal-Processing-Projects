/**
 * @file    config.h
 * @brief   Settings of the Wi-Fi logic analyzer (ESP32 DevKit, WROOM module).
 *
 * Inputs: channels 0..7 on GPIO 13, 14, 16, 17, 18, 19, 21, 22 (LA_PINS in
 * src/core/la_core.h), 3.3 V logic only. The board also outputs three test
 * signals on spare pins, so it can be tried with jumper wires alone.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- built-in test signals ------------------------------------------------------------- */
#define TEST_SIGNALS        1       /* 0 to free the pins below */
#define PIN_TEST_UART_TX    25      /* "Hello from ESP32 <n>" at 115200 8N1 every 20 ms */
#define TEST_UART_BAUD      115200
#define PIN_TEST_PWM        26      /* 10 kHz, 30 % duty */
#define PIN_TEST_SDA        33      /* I2C at 100 kHz: writes to address 0x3C every 20 ms */
#define PIN_TEST_SCL        32

#define PIN_LED             2       /* on while a capture runs */

/* ---- capture ---------------------------------------------------------------------------- */
#define TRIGGER_TIMEOUT_MS  3000    /* default wait for the trigger, then capture anyway */
#define YIELD_EVERY_US      200000  /* while waiting, let other core-1 tasks run this often */
#define MAX_DECODED         512     /* frames / events returned by a decoder */

/* ---- Wi-Fi access point ----------------------------------------------------------------- */
#define AP_SSID_PREFIX      "LogicAnalyzer-"
#define AP_PASSWORD         "analyzer123"   /* at least 8 characters; change before use */

#endif /* CONFIG_H */
