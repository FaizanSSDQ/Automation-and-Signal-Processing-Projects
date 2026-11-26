/**
 * @file    config.h
 * @brief   Board pins, limits and network settings of the ESP32-CAM server
 *          (AI-Thinker ESP32-CAM with OV2640 camera and 4 MB PSRAM).
 *
 * Wi-Fi and login credentials go in secrets.h next to this file (ignored by
 * git); copy secrets.example.h to start. Without secrets.h the camera opens
 * its own access point and uses the example login, and says so on the
 * serial port.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- AI-Thinker camera pins (fixed by the board) --------------------------------------- */
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

#define PIN_FLASH_LED   4       /* bright white LED; PWM-dimmed, never full on for long */
#define PIN_STATUS_LED  33      /* small red LED on the back, active low */
#define FLASH_PWM_HZ    5000
#define FLASH_LEDC_CHANNEL 8   /* low-speed group; the camera clock uses timer 3 / channel 7 */

/* ---- server ----------------------------------------------------------------------------- */
#define STREAM_PORT         81  /* stream on its own server so the controls stay responsive */
#define MAX_STREAM_CLIENTS  2   /* more would divide the frame rate between viewers */
#define AP_SSID_PREFIX      "ESP32CAM-"

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#define HAVE_SECRETS 1
#endif
#endif
#ifndef HAVE_SECRETS
#include "secrets.example.h"
#define HAVE_SECRETS 0
#endif

#endif /* CONFIG_H */
