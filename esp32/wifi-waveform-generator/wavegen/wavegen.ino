/**
 * @file    wavegen.ino
 * @brief   Wi-Fi waveform generator on ESP32: DDS into both DACs through DMA,
 *          controlled from a phone or PC browser.
 *
 * Tasks
 *   generator  core 1, priority 5  renders 512-sample blocks with the DDS core
 *                                  and queues them to the DAC DMA (blocks while
 *                                  the DMA queue is full, so it runs exactly at
 *                                  the output rate)
 *   web        core 0, priority 1  page, settings, preview
 * The DAC DMA alternates between the two channels: even bytes to GPIO 25
 * (waveform), odd bytes to GPIO 26 (sync).
 *
 * Connect to the "WaveGen-XXXX" network and open http://192.168.4.1
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <WebServer.h>
#include <WiFi.h>
#include <driver/dac_continuous.h>

#include "config.h"
#include "src/core/dds.h"
#include "src/core/gen_config.h"
#include "web_ui.h"

/* ---- shared settings (guarded by lock) --------------------------------------------------- */
static SemaphoreHandle_t lock;
static dds_settings_t settings;
static bool settings_changed;
static uint32_t write_errors;

static dac_continuous_handle_t dac;
static WebServer server(80);

/* ---- generator --------------------------------------------------------------------------- */
static void dac_begin()
{
    dac_continuous_config_t cfg = {};
    cfg.chan_mask = DAC_CHANNEL_MASK_ALL;
    cfg.desc_num = GEN_DMA_BUFFERS;
    cfg.buf_size = GEN_DMA_BUF_BYTES;
    cfg.freq_hz = 2u * GEN_SAMPLE_RATE_HZ;   /* alternate mode: two conversions per sample pair */
    cfg.offset = 0;
    cfg.clk_src = DAC_DIGI_CLK_SRC_DEFAULT;
    cfg.chan_mode = DAC_CHANNEL_MODE_ALTER;
    ESP_ERROR_CHECK(dac_continuous_new_channels(&cfg, &dac));
    ESP_ERROR_CHECK(dac_continuous_enable(dac));
}

static void generator_task(void *)
{
    static uint8_t block[2u * GEN_BLOCK];   /* (wave, sync) pairs */
    dds_t dds;
    xSemaphoreTake(lock, portMAX_DELAY);
    dds_init(&dds, &settings);
    xSemaphoreGive(lock);

    for (;;) {
        xSemaphoreTake(lock, portMAX_DELAY);
        if (settings_changed) {
            dds_apply(&dds, &settings);   /* keeps the phase: no glitch on a frequency change */
            settings_changed = false;
        }
        xSemaphoreGive(lock);

        dds_render(&dds, block, GEN_BLOCK);
        size_t loaded = 0;
        /* Blocks until the DMA has room (normally one block time); 100 ms means the DAC stalled. */
        if (dac_continuous_write(dac, block, sizeof(block), &loaded, 100) != ESP_OK || loaded != sizeof(block)) {
            write_errors++;
        }
    }
}

/* ---- web --------------------------------------------------------------------------------- */
static void send_settings()
{
    char json[512];
    xSemaphoreTake(lock, portMAX_DELAY);
    const size_t n = dds_settings_json(&settings, json, sizeof(json));
    xSemaphoreGive(lock);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", n ? json : "{}");
}

/* /api/set?freq=1000&wave=square ... : all arguments are checked before any is applied. */
static void handle_set()
{
    xSemaphoreTake(lock, portMAX_DELAY);
    dds_settings_t next = settings;
    xSemaphoreGive(lock);
    for (int i = 0; i < server.args(); i++) {
        if (!dds_set(&next, server.argName(i).c_str(), server.arg(i).c_str())) {
            server.send(400, "text/plain", "bad setting: " + server.argName(i) + "=" + server.arg(i));
            return;
        }
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    settings = next;
    settings_changed = true;
    xSemaphoreGive(lock);
    digitalWrite(PIN_LED, next.output_on);
    send_settings();
}

static void send_preview()
{
    static uint8_t pts[256];
    static char json[1400];
    xSemaphoreTake(lock, portMAX_DELAY);
    const dds_settings_t s = settings;
    xSemaphoreGive(lock);
    const size_t n = dds_preview(&s, pts, sizeof(pts));
    size_t len = (size_t)snprintf(json, sizeof(json), "{\"write_errors\":%lu,\"codes\":[", (unsigned long)write_errors);
    for (size_t i = 0; i < n && len + 6 < sizeof(json); i++) {
        len += (size_t)snprintf(json + len, sizeof(json) - len, "%s%u", i ? "," : "", pts[i]);
    }
    snprintf(json + len, sizeof(json) - len, "]}");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
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
    dds_settings_default(&settings);
    digitalWrite(PIN_LED, settings.output_on);

    dac_begin();

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s%04X", AP_SSID_PREFIX, (unsigned)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, AP_PASSWORD);
    Serial.printf("waveform generator \"%s\", open http://%s\n", ssid, WiFi.softAPIP().toString().c_str());

    server.on("/", [] { server.send_P(200, "text/html", WEB_UI_HTML); });
    server.on("/api/settings", send_settings);
    server.on("/api/set", handle_set);
    server.on("/api/preview", send_preview);
    server.onNotFound([] { server.send(404, "text/plain", "not found"); });
    server.begin();

    xTaskCreatePinnedToCore(generator_task, "generator", 4096, nullptr, 5, nullptr, 1);
    xTaskCreatePinnedToCore(web_task, "web", 6144, nullptr, 1, nullptr, 0);
}

void loop()
{
    vTaskDelete(nullptr);   /* all work happens in the tasks */
}
