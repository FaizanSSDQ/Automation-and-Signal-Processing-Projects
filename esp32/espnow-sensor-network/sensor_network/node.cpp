/**
 * @file    node.cpp
 * @brief   Sensor node: wake, measure, send one ESP-NOW report, deep sleep.
 *
 * Every wake-up runs setup() from the start; nothing runs in loop(). State
 * that must survive deep sleep (sequence number, gateway channel and MAC)
 * lives in RTC memory. The node wakes on its timer, or at once when an alarm
 * input becomes active (flame on ext0, vibration and motion on ext1).
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "config.h"

#if NET_ROLE == ROLE_NODE

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>

#include "drivers.h"
#include "node.h"
#include "src/core/packet.h"
#include "src/core/sensor_math.h"

RTC_DATA_ATTR static uint16_t rtc_seq;
RTC_DATA_ATTR static uint8_t rtc_channel;         /* 0 = gateway not known yet */
RTC_DATA_ATTR static uint8_t rtc_gateway[6];
RTC_DATA_ATTR static uint8_t rtc_failures;

static volatile int send_status = -1;             /* -1 pending, 0 delivered, 1 failed */

static void on_sent(const wifi_tx_info_t *, esp_now_send_status_t status)
{
    send_status = (status == ESP_NOW_SEND_SUCCESS) ? 0 : 1;
}

static wake_reason_t wake_reason()
{
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER: return WAKE_TIMER;
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:  return WAKE_ALARM;
    default:                     return WAKE_POWER_ON;
    }
}

static uint8_t read_alarms()
{
    pinMode(PIN_FLAME, INPUT);
    pinMode(PIN_VIBRATION, INPUT);
    pinMode(PIN_MOTION, INPUT);
    uint8_t a = 0;
    if (digitalRead(PIN_FLAME) == FLAME_ACTIVE_LEVEL) a |= ALARM_FLAME;
    if (digitalRead(PIN_VIBRATION) == HIGH) a |= ALARM_VIBRATION;
    if (digitalRead(PIN_MOTION) == HIGH) a |= ALARM_MOTION;
    return a;
}

/* Find the gateway's access point: its channel is the channel ESP-NOW must use. */
static bool find_gateway()
{
    WiFi.mode(WIFI_STA);
    const int n = WiFi.scanNetworks(false, false, false, 120);
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i).startsWith(GATEWAY_SSID_PREFIX) && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) {
            best = i;
        }
    }
    if (best >= 0) {
        rtc_channel = (uint8_t)WiFi.channel(best);
        memcpy(rtc_gateway, WiFi.BSSID(best), 6);   /* the AP's MAC accepts ESP-NOW frames */
    }
    WiFi.scanDelete();
    return best >= 0;
}

static void sleep_until_next(uint32_t seconds, uint8_t active_alarms)
{
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ull);
    /* Arm only inputs that are currently idle, or an active alarm would wake us at once. */
    if (!(active_alarms & ALARM_FLAME)) {
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_FLAME, FLAME_ACTIVE_LEVEL);
    }
    uint64_t mask = 0;
    if (!(active_alarms & ALARM_VIBRATION)) mask |= 1ull << PIN_VIBRATION;
    if (!(active_alarms & ALARM_MOTION)) mask |= 1ull << PIN_MOTION;
    if (mask) {
        esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
    }
    digitalWrite(PIN_LED, LOW);
    esp_deep_sleep_start();
}

void node_run()
{
    const uint32_t t_start = millis();
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
    Serial.begin(115200);

    reading_t r = {};
    r.node_id = NODE_ID;
    r.wake = wake_reason();
    if (r.wake == WAKE_POWER_ON) {
        rtc_seq = 0;
        rtc_channel = 0;
        rtc_failures = 0;
        delay(1200);                   /* DHT22 needs ~1 s after power-up */
    }
    r.seq = ++rtc_seq;
    r.alarms = read_alarms();

    if (dht22_read(PIN_DHT, &r.temp_c100, &r.humidity_x100)) {
        r.flags |= PKT_F_DHT_OK;
    }
    int32_t bt;
    if (bmp280_begin(PIN_SDA, PIN_SCL, BMP280_ADDR) && bmp280_measure(&bt, &r.pressure_pa)) {
        r.bmp_temp_c100 = (int16_t)bt;
        r.flags |= PKT_F_BMP_OK;
    }
    analogSetPinAttenuation(PIN_BATTERY, ADC_11db);
    r.battery_mv = battery_mv(analogReadMilliVolts(PIN_BATTERY), BATTERY_RATIO_X100);

    if (rtc_channel == 0 && !find_gateway()) {
        Serial.println("gateway not found");
        sleep_until_next(RETRY_INTERVAL_S, r.alarms);
    }

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(rtc_channel, WIFI_SECOND_CHAN_NONE);
    bool delivered = false;
    if (esp_now_init() == ESP_OK) {
        esp_now_register_send_cb(on_sent);
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, rtc_gateway, 6);
        peer.channel = rtc_channel;
        peer.ifidx = WIFI_IF_STA;
        esp_now_add_peer(&peer);

        uint8_t frame[PKT_LEN];
        pkt_encode(&r, frame);
        send_status = -1;
        if (esp_now_send(rtc_gateway, frame, sizeof(frame)) == ESP_OK) {
            const uint32_t t0 = millis();
            while (send_status < 0 && millis() - t0 < SEND_TIMEOUT_MS) {
                delay(1);
            }
            delivered = send_status == 0;   /* unicast: the gateway's radio acknowledged it */
        }
    }
    if (delivered) {
        rtc_failures = 0;
    } else if (++rtc_failures >= MAX_SEND_FAILURES) {
        rtc_channel = 0;                    /* gateway moved or changed channel: search again */
        rtc_failures = 0;
    }
    Serial.printf("node %u seq %u %s in %lu ms\n", NODE_ID, r.seq, delivered ? "delivered" : "not delivered",
                  (unsigned long)(millis() - t_start));
    Serial.flush();
    sleep_until_next(REPORT_INTERVAL_S, r.alarms);
}

#endif /* NET_ROLE == ROLE_NODE */
