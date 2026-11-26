/**
 * @file    radar.ino
 * @brief   180-degree ultrasonic + infrared radar on ESP32 with a live browser display.
 *
 * Tasks
 *   scan  core 1, priority 2  step the servo 0..180..0 in 2-degree steps; at each
 *                             step take three pings (median) and the Sharp IR
 *                             average, convert with the air temperature, store
 *   web   core 0, priority 1  serve the radar page and /api/scan (JSON)
 * The scan buffer is shared under one mutex. Objects are found from a copy of
 * the scan when a browser asks, so the scan task never waits on the network.
 *
 * Connect to the "Radar-XXXX" network and open http://192.168.4.1
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "ranger.h"
#include "src/core/radar_core.h"
#include "web_ui.h"

/* ---- shared state (guarded by lock) ------------------------------------------------------ */
static SemaphoreHandle_t lock;
static scan_t scan;
static int current_angle;
static float air_temp_c = DEFAULT_TEMP_C;
static bool running = true;

static WebServer server(80);

static_assert(PINGS_PER_STEP == 3, "the scan task takes the median of three pings");

/* ---- scan task --------------------------------------------------------------------------- */
static void scan_task(void *)
{
    sweep_t sweep;
    sweep_init(&sweep);
    servo_write(0);
    vTaskDelay(pdMS_TO_TICKS(500));   /* let the servo reach the start */

    for (;;) {
        xSemaphoreTake(lock, portMAX_DELAY);
        const bool run = running;
        const float temp = air_temp_c;
        xSemaphoreGive(lock);
        if (!run) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        const int angle = sweep_next(&sweep);
        servo_write(angle);
        vTaskDelay(pdMS_TO_TICKS(SETTLE_MS));

        float cm[PINGS_PER_STEP];
        int valid = 0;
        for (int p = 0; p < PINGS_PER_STEP; p++) {
            bool ok;
            const float d = echo_to_cm(ranger_ping_us(), temp, &ok);
            cm[p] = ok ? d : RADAR_MAX_CM + 1.0f;   /* a lost echo counts as "far" in the median */
            valid += ok;
            vTaskDelay(pdMS_TO_TICKS(PING_GAP_MS));
        }
        const float us = median3(cm[0], cm[1], cm[2]);
        bool ir_ok;
        const float ir = sharp_to_cm(ranger_sharp_volts(), &ir_ok);

        const int i = radar_index(angle);
        xSemaphoreTake(lock, portMAX_DELAY);
        current_angle = angle;
        scan.us_cm[i] = us;
        scan.us_ok[i] = valid >= 2 && us <= RADAR_MAX_CM;
        scan.ir_cm[i] = ir;
        scan.ir_ok[i] = ir_ok;
        if (angle == 0 || angle == 180) {
            scan.sweeps++;
        }
        xSemaphoreGive(lock);

        digitalWrite(PIN_LED, angle == 0 || angle == 180);
    }
}

/* ---- web --------------------------------------------------------------------------------- */
static void send_scan()
{
    static scan_t copy;   /* static: too big for the web task's stack to hold twice */
    static char json[3072];
    int angle;
    float temp;
    xSemaphoreTake(lock, portMAX_DELAY);
    copy = scan;
    angle = current_angle;
    temp = air_temp_c;
    xSemaphoreGive(lock);

    object_t obj[RADAR_MAX_OBJECTS];
    const size_t n_obj = radar_find_objects(copy.us_cm, copy.us_ok, RADAR_POINTS, OBJECT_MAX_CM, OBJECT_GAP_CM,
                                            obj, RADAR_MAX_OBJECTS);
    const size_t n = radar_json(&copy, angle, temp, obj, n_obj, json, sizeof(json));
    server.sendHeader("Cache-Control", "no-store");
    if (n) {
        server.send(200, "application/json", json);
    } else {
        server.send(500, "text/plain", "scan does not fit the buffer");
    }
}

/* /api/config?temp=22.5&run=0 : either argument may be left out */
static void handle_config()
{
    xSemaphoreTake(lock, portMAX_DELAY);
    if (server.hasArg("temp")) {
        const float t = server.arg("temp").toFloat();
        if (t >= -20.0f && t <= 50.0f) {
            air_temp_c = t;
        }
    }
    if (server.hasArg("run")) {
        running = server.arg("run") != "0";
    }
    xSemaphoreGive(lock);
    server.send(200, "text/plain", "ok");
}

static void web_task(void *)
{
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_LED, OUTPUT);
    lock = xSemaphoreCreateMutex();
    scan_init(&scan);
    ranger_begin();

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s%04X", AP_SSID_PREFIX, (unsigned)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, AP_PASSWORD);
    Serial.printf("radar \"%s\", open http://%s\n", ssid, WiFi.softAPIP().toString().c_str());

    server.on("/", [] { server.send_P(200, "text/html", WEB_UI_HTML); });
    server.on("/api/scan", send_scan);
    server.on("/api/config", handle_config);
    server.onNotFound([] { server.send(404, "text/plain", "not found"); });
    server.begin();

    xTaskCreatePinnedToCore(scan_task, "scan", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(web_task, "web", 6144, nullptr, 1, nullptr, 0);
}

void loop()
{
    vTaskDelete(nullptr);   /* all work happens in the tasks */
}
