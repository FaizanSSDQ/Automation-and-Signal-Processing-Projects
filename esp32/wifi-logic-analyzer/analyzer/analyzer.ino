/**
 * @file    analyzer.ino
 * @brief   8-channel Wi-Fi logic analyzer on ESP32: up to 2 M samples/s with
 *          triggers, UART and I2C decoding and a waveform viewer in the browser.
 *
 * Tasks
 *   capture  core 1, high priority  runs la_record(): reads GPIO_IN_REG once per
 *                                   sample, paced by the CPU cycle counter; the
 *                                   post-trigger part runs with interrupts off
 *                                   at high rates
 *   web      core 0                 page, capture requests, data, decoders
 *   signals  core 0                 optional test signals (UART, PWM, I2C)
 * Core 0 also runs the Wi-Fi stack, so nothing on core 0 can delay a sample.
 *
 * Connect to "LogicAnalyzer-XXXX" and open http://192.168.4.1
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_cpu.h>
#include <esp_timer.h>
#include <soc/gpio_reg.h>

#include "config.h"
#include "src/core/la_core.h"
#include "web_ui.h"

static uint8_t *samples;                  /* LA_MAX_SAMPLES bytes from the heap */
static char *json;                        /* decoder output */
static const size_t JSON_CAP = 24576;

static TaskHandle_t capture_handle;
static volatile bool busy, abort_wait, have_data;
static la_plan_t plan;
static la_trigger_t trigger;
static la_result_t result;
static int64_t wait_until_us, last_yield_us;

static WebServer server(80);

/* ---- hardware hooks for la_record() (run on core 1, from IRAM) ---------------------------- */
static uint32_t IRAM_ATTR hw_now(void)
{
    return (uint32_t)esp_cpu_get_cycle_count();
}
static uint8_t IRAM_ATTR hw_read(void)
{
    return la_pack(REG_READ(GPIO_IN_REG));
}
static void IRAM_ATTR hw_irq(bool off)
{
    if (off) {
        portDISABLE_INTERRUPTS();
    } else {
        portENABLE_INTERRUPTS();
    }
}
static la_wait_t hw_waiting(void)
{
    const int64_t now = esp_timer_get_time();
    if (abort_wait || now >= wait_until_us) {
        return LA_STOP_WAITING;
    }
    if (now - last_yield_us >= YIELD_EVERY_US) {
        vTaskDelay(1);                    /* lets the idle task run; leaves a gap in the ring */
        last_yield_us = esp_timer_get_time();
        return LA_RESTART;
    }
    return LA_GO_ON;
}
static const la_hw_t HW = {hw_now, hw_read, hw_irq, hw_waiting};

static void capture_task(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        digitalWrite(PIN_LED, HIGH);
        last_yield_us = esp_timer_get_time();
        la_record(&plan, &trigger, &HW, samples, &result);
        digitalWrite(PIN_LED, LOW);
        have_data = true;
        busy = false;   /* last: the web task reads plan, result and samples only when not busy */
    }
}

/* ---- test signals ------------------------------------------------------------------------ */
static void signals_task(void *)
{
    uint32_t n = 0;
    for (;;) {
        Serial2.printf("Hello from ESP32 %lu\n", (unsigned long)n++);
        Wire.beginTransmission(0x3C);     /* nobody answers: the decoder shows a NACK */
        Wire.write(0xA5);
        Wire.write((uint8_t)n);
        Wire.endTransmission();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ---- web --------------------------------------------------------------------------------- */
static uint32_t arg_u32(const char *name, uint32_t def)
{
    return server.hasArg(name) ? (uint32_t)strtoul(server.arg(name).c_str(), nullptr, 10) : def;
}

/* /api/capture?rate=1000000&samples=16384&pre=10&trigger=rxxxxxxx&timeout=3000
 * Starts a capture and answers at once; the page then polls /api/status. */
static void handle_capture()
{
    if (busy) {
        server.send(409, "text/plain", "a capture is already running");
        return;
    }
    la_plan_t p;
    la_trigger_t t;
    const String trig = server.hasArg("trigger") ? server.arg("trigger") : String("xxxxxxxx");
    if (!la_plan(arg_u32("rate", 1000000u), arg_u32("samples", 16384u), (uint8_t)arg_u32("pre", 10u), &p) ||
        !la_trigger_parse(trig.c_str(), &t)) {
        server.send(400, "text/plain", "bad rate, samples, pre or trigger");
        return;
    }
    const uint32_t timeout_ms = arg_u32("timeout", TRIGGER_TIMEOUT_MS);
    plan = p;
    trigger = t;
    abort_wait = false;
    have_data = false;
    busy = true;
    wait_until_us = esp_timer_get_time() + (int64_t)(timeout_ms > 60000u ? 60000u : timeout_ms) * 1000;
    xTaskNotifyGive(capture_handle);
    server.send(202, "application/json", "{\"busy\":true}");
}

/* {"busy":true} while capturing, then the capture result. */
static void handle_status()
{
    server.sendHeader("Cache-Control", "no-store");
    if (busy || !have_data) {
        server.send(200, "application/json", busy ? "{\"busy\":true}" : "{\"busy\":false}");
        return;
    }
    char out[256];
    la_result_json(&plan, &result, out, sizeof(out));
    server.send(200, "application/json", out);
}

static void handle_data()
{
    if (!have_data || busy) {
        server.send(404, "text/plain", "no capture");
        return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(plan.samples);
    server.send(200, "application/octet-stream", "");
    server.sendContent((const char *)samples, plan.samples);
}

/* /api/decode?type=uart&ch=0&baud=115200   or   ?type=i2c&sda=2&scl=3 */
static void handle_decode()
{
    if (!have_data || busy) {
        server.send(404, "text/plain", "no capture");
        return;
    }
    size_t n = 0;
    if (server.arg("type") == "uart") {
        static la_uart_frame_t frames[MAX_DECODED];
        const size_t count = la_uart_decode(samples, plan.samples, arg_u32("ch", 0) & 7u, plan.actual_hz,
                                            arg_u32("baud", 115200u), frames, MAX_DECODED);
        n = la_uart_json(frames, count, json, JSON_CAP);
    } else if (server.arg("type") == "i2c") {
        static la_i2c_event_t events[MAX_DECODED];
        const size_t count = la_i2c_decode(samples, plan.samples, arg_u32("sda", 2) & 7u, arg_u32("scl", 3) & 7u,
                                           events, MAX_DECODED);
        n = la_i2c_json(events, count, json, JSON_CAP);
    } else {
        server.send(400, "text/plain", "type must be uart or i2c");
        return;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", n ? json : "[]");
}

static void handle_measure()
{
    if (!have_data || busy) {
        server.send(404, "text/plain", "no capture");
        return;
    }
    la_measure_t m[LA_NUM_CHANNELS];
    for (unsigned ch = 0; ch < LA_NUM_CHANNELS; ch++) {
        la_measure(samples, plan.samples, ch, plan.actual_hz, &m[ch]);
    }
    char out[640];
    la_measure_json(m, LA_NUM_CHANNELS, out, sizeof(out));
    server.send(200, "application/json", out);
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
    const int pins[LA_NUM_CHANNELS] = LA_PINS;
    for (int pin : pins) {
        pinMode(pin, INPUT);
    }
    samples = (uint8_t *)heap_caps_malloc(LA_MAX_SAMPLES, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    json = (char *)heap_caps_malloc(JSON_CAP, MALLOC_CAP_8BIT);
    if (samples == nullptr || json == nullptr) {
        Serial.println("not enough memory for the sample buffer");
        for (;;) {
            delay(1000);
        }
    }

#if TEST_SIGNALS
    Serial2.begin(TEST_UART_BAUD, SERIAL_8N1, -1, PIN_TEST_UART_TX);
    ledcAttach(PIN_TEST_PWM, 10000, 10);
    ledcWrite(PIN_TEST_PWM, 307);         /* 30 % of 1023 */
    Wire.begin(PIN_TEST_SDA, PIN_TEST_SCL, 100000);
    xTaskCreatePinnedToCore(signals_task, "signals", 3072, nullptr, 1, nullptr, 0);
#endif

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s%04X", AP_SSID_PREFIX, (unsigned)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, AP_PASSWORD);
    Serial.printf("logic analyzer \"%s\", open http://%s\n", ssid, WiFi.softAPIP().toString().c_str());

    server.on("/", [] { server.send_P(200, "text/html", WEB_UI_HTML); });
    server.on("/api/capture", handle_capture);
    server.on("/api/status", handle_status);
    server.on("/api/abort", [] {   /* stop waiting for the trigger and capture now */
        abort_wait = true;
        server.send(200, "text/plain", "ok");
    });
    server.on("/api/data", handle_data);
    server.on("/api/decode", handle_decode);
    server.on("/api/measure", handle_measure);
    server.onNotFound([] { server.send(404, "text/plain", "not found"); });
    server.begin();

    xTaskCreatePinnedToCore(capture_task, "capture", 4096, nullptr, configMAX_PRIORITIES - 2, &capture_handle, 1);
    xTaskCreatePinnedToCore(web_task, "web", 6144, nullptr, 1, nullptr, 0);
}

void loop()
{
    vTaskDelete(nullptr);   /* all work happens in the tasks */
}
