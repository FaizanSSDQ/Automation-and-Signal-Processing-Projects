/**
 * @file    config.h
 * @brief   Role, pins and timing of the ESP-NOW sensor network.
 *
 * One sketch, two roles: flash ROLE_NODE onto each sensor board (with its
 * own NODE_ID) and ROLE_GATEWAY onto one board. The Makefile builds both by
 * passing -DNET_ROLE=... ; in the Arduino IDE, edit the default below.
 *
 * Wi-Fi credentials and the ThingSpeak key go in an optional secrets.h next
 * to this file (ignored by git), see secrets.example.h.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CONFIG_H
#define CONFIG_H

#define ROLE_NODE     0
#define ROLE_GATEWAY  1
#ifndef NET_ROLE
#define NET_ROLE      ROLE_NODE
#endif
#ifndef NODE_ID
#define NODE_ID       1            /* 1..254, unique per node */
#endif

/* ---- network ---------------------------------------------------------------------------- */
#define GATEWAY_SSID_PREFIX "SensorGW-"   /* nodes find the gateway (and its channel) by this name */
#define GATEWAY_AP_PASSWORD "sensornet123"
#define GATEWAY_AP_CHANNEL  6             /* used when the gateway is not joined to a router */

#if defined(__has_include)
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#endif
#ifndef WIFI_SSID
#define WIFI_SSID           ""            /* router for internet upload; empty = gateway works offline */
#define WIFI_PASSWORD       ""
#endif
#ifndef THINGSPEAK_API_KEY
#define THINGSPEAK_API_KEY  ""            /* write key of a ThingSpeak channel; empty = no upload */
#endif

/* ---- node timing ------------------------------------------------------------------------ */
#define REPORT_INTERVAL_S   60u           /* deep sleep between reports */
#define RETRY_INTERVAL_S    30u           /* when no gateway was found */
#define SEND_TIMEOUT_MS     100u
#define MAX_SEND_FAILURES   3u            /* then search for the gateway again */

/* ---- gateway timing --------------------------------------------------------------------- */
#define NODE_TIMEOUT_MS     (3u * REPORT_INTERVAL_S * 1000u)   /* offline after 3 missed reports */
#define UPLOAD_PERIOD_MS    20000u        /* ThingSpeak free tier: at most one update per 15 s */

/* ---- node pins -------------------------------------------------------------------------- */
#define PIN_DHT             4             /* DHT22 data, 10 k pull-up to 3.3 V */
#define PIN_SDA             21            /* BMP280 */
#define PIN_SCL             22
#define BMP280_ADDR         0x76          /* 0x77 if SDO is tied high */
#define PIN_BATTERY         35            /* battery through a 100k/100k divider (ADC1) */
#define BATTERY_RATIO_X100  200u

/* Alarm inputs (RTC-capable GPIOs so they can wake the node from deep sleep).
 * Typical modules: flame sensor DO is low on flame, SW-420 vibration and PIR
 * motion outputs are high on an event. */
#define PIN_FLAME           27
#define FLAME_ACTIVE_LEVEL  0
#define PIN_VIBRATION       26
#define PIN_MOTION          25            /* PIR module or IR obstacle sensor (active high) */

#define PIN_LED             2

#endif /* CONFIG_H */
