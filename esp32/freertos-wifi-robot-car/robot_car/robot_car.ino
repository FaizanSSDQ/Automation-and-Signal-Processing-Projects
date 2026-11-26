/**
 * @file    robot_car.ino
 * @brief   FreeRTOS Wi-Fi robot car on ESP32: dual-core tasks, browser joystick,
 *          obstacle avoidance, line following and safety stops.
 *
 * Tasks
 *   control  core 1, priority 3, 50 Hz  autopilot + safety, mixing, ramping, motors
 *   sensors  core 1, priority 2, 20 Hz  ultrasonic + Sharp IR (fused), line sensors, battery
 *   web      core 0, priority 1         HTTP server for the control page and /cmd
 * The Wi-Fi stack itself runs on core 0, so motor control on core 1 is never
 * delayed by network traffic. Shared data is guarded by one mutex.
 *
 * Connect a phone to the "RobotCar-XXXX" network and open http://192.168.4.1
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <WebServer.h>
#include <esp_wifi.h>
#include <WiFi.h>

#include "config.h"
#include "motors.h"
#include "sensors.h"
#include "src/core/autopilot.h"
#include "src/core/drive.h"
#include "src/core/telemetry.h"
#include "web_ui.h"

/* ---- shared state (guarded by state_lock) ------------------------------------------------ */
static SemaphoreHandle_t state_lock;
static struct {
    drive_cmd_t joystick;      /* latest command from the browser */
    float speed_max;           /* speed limit slider, 0.1 .. 1 */
    uint32_t last_cmd_ms;      /* when it arrived (link watchdog) */
    car_mode_t mode_request;
    sensors_t sensors;         /* latest sensor reading */
    telemetry_t telemetry;     /* latest control output, for the browser */
} shared;

static WebServer server(80);

/* ---- control task ------------------------------------------------------------------------ */
static void control_task(void *)
{
    autopilot_cfg_t cfg;
    autopilot_defaults(&cfg);
    autopilot_t ap;
    autopilot_init(&ap, &cfg);

    const TickType_t period = pdMS_TO_TICKS(1000 / CONTROL_HZ);
    const float step = RAMP_PER_S / CONTROL_HZ;
    float left = 0.0f, right = 0.0f;
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        xSemaphoreTake(state_lock, portMAX_DELAY);
        const drive_cmd_t joy = shared.joystick;
        const float limit = shared.speed_max;
        const uint32_t last = shared.last_cmd_ms;
        const car_mode_t mode = shared.mode_request;
        const sensors_t s = shared.sensors;
        xSemaphoreGive(state_lock);

        autopilot_set_mode(&ap, mode);
        const drive_cmd_t cmd = autopilot_step(&ap, &s, joy, millis(), last);
        const wheels_t w = drive_mix(cmd);
        left = drive_ramp(left, w.left * limit, step);
        right = drive_ramp(right, w.right * limit, step);
        motors_set(left, right);

        /* LED: steady while driving normally, blinking when a safety rule stops the car. */
        digitalWrite(PIN_LED, ap.stop == STOP_NONE ? HIGH : ((millis() / 150) & 1));

        xSemaphoreTake(state_lock, portMAX_DELAY);
        shared.telemetry.mode = ap.mode;
        shared.telemetry.stop = ap.stop;
        shared.telemetry.left = left;
        shared.telemetry.right = right;
        shared.telemetry.sensors = s;
        xSemaphoreGive(state_lock);

        vTaskDelayUntil(&wake, period);
    }
}

/* ---- sensor task ------------------------------------------------------------------------- */
static void sensor_task(void *)
{
    const TickType_t period = pdMS_TO_TICKS(1000 / SENSOR_HZ);
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        const sensors_t s = sensors_read();   /* blocking parts happen outside the lock */
        xSemaphoreTake(state_lock, portMAX_DELAY);
        shared.sensors = s;
        xSemaphoreGive(state_lock);
        vTaskDelayUntil(&wake, period);
    }
}

/* ---- web task ---------------------------------------------------------------------------- */
static int station_rssi()
{
    wifi_sta_list_t list;
    if (esp_wifi_ap_get_sta_list(&list) == ESP_OK && list.num > 0) {
        return list.sta[0].rssi;
    }
    return 0;
}

static void send_telemetry()
{
    char json[256];
    xSemaphoreTake(state_lock, portMAX_DELAY);
    telemetry_t t = shared.telemetry;
    xSemaphoreGive(state_lock);
    t.rssi = station_rssi();
    t.uptime_s = millis() / 1000u;
    const size_t n = telemetry_json(&t, json, sizeof(json));
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", n ? json : "{}");
}

static void handle_cmd()
{
    float x = 0.0f, y = 0.0f, max = 0.7f;
    const bool ok = telemetry_parse_axis(server.arg("x").c_str(), &x) &&
                    telemetry_parse_axis(server.arg("y").c_str(), &y);
    (void)telemetry_parse_limit(server.arg("max").c_str(), &max);
    const car_mode_t mode = autopilot_mode_from_name(server.arg("mode").c_str());

    xSemaphoreTake(state_lock, portMAX_DELAY);
    if (ok) {
        shared.joystick.throttle = y;
        shared.joystick.steer = x;
        shared.speed_max = max;
        shared.last_cmd_ms = millis();    /* only valid commands feed the watchdog */
    }
    if (mode != MODE_COUNT) {
        shared.mode_request = mode;
    }
    xSemaphoreGive(state_lock);
    send_telemetry();
}

static void web_task(void *)
{
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

/* ---- setup ------------------------------------------------------------------------------- */
void setup()
{
    Serial.begin(115200);
    pinMode(PIN_LED, OUTPUT);
    state_lock = xSemaphoreCreateMutex();
    shared.speed_max = 0.7f;
    shared.mode_request = MODE_MANUAL;

    if (!motors_begin()) {
        Serial.println("PWM setup failed");
    }
    sensors_begin();

    char ssid[24];
    snprintf(ssid, sizeof(ssid), "%s%04X", AP_SSID_PREFIX, (unsigned)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, AP_PASSWORD);
    Serial.printf("Wi-Fi \"%s\", open http://%s\n", ssid, WiFi.softAPIP().toString().c_str());

    server.on("/", [] { server.send_P(200, "text/html", WEB_UI_HTML); });
    server.on("/cmd", handle_cmd);
    server.on("/status", send_telemetry);
    server.onNotFound([] { server.send(404, "text/plain", "not found"); });
    server.begin();

    xTaskCreatePinnedToCore(control_task, "control", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(sensor_task, "sensors", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(web_task, "web", 6144, nullptr, 1, nullptr, 0);
}

void loop()
{
    vTaskDelete(nullptr);   /* all work happens in the tasks above */
}
