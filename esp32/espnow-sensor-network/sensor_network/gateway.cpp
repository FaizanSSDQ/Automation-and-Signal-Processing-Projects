/**
 * @file    gateway.cpp
 * @brief   Gateway: receive ESP-NOW reports, keep the node table, serve the
 *          dashboard and upload to ThingSpeak.
 *
 * The ESP-NOW receive callback runs in the Wi-Fi task, so it only copies the
 * frame into a queue. A processing task validates and decodes it and updates
 * the node table (guarded by a mutex), which the web server and the upload
 * task read.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "config.h"

#if NET_ROLE == ROLE_GATEWAY

#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "dashboard.h"
#include "gateway.h"
#include "src/core/node_table.h"
#include "src/core/packet.h"

typedef struct {
    uint8_t mac[6];
    int8_t rssi;
    uint8_t len;
    uint8_t data[32];
} rx_frame_t;

static QueueHandle_t rx_queue;
static SemaphoreHandle_t table_lock;
static node_table_t table;
static volatile uint32_t dropped;
static WebServer server(80);

/* ---- ESP-NOW receive (Wi-Fi task context: keep it short) ------------------------------- */
static void on_receive(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    rx_frame_t f;
    memcpy(f.mac, info->src_addr, 6);
    f.rssi = (int8_t)info->rx_ctrl->rssi;
    f.len = (uint8_t)(len > (int)sizeof(f.data) ? sizeof(f.data) : len);
    memcpy(f.data, data, f.len);
    if (xQueueSend(rx_queue, &f, 0) != pdTRUE) {
        dropped = dropped + 1u;   /* queue full: frame lost at the gateway */
    }
}

static void process_task(void *)
{
    rx_frame_t f;
    for (;;) {
        if (xQueueReceive(rx_queue, &f, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        reading_t r;
        const pkt_status_t st = pkt_decode(f.data, f.len, &r);
        xSemaphoreTake(table_lock, portMAX_DELAY);
        if (st == PKT_OK) {
            nt_update(&table, f.mac, &r, f.rssi, millis());
        } else {
            table.bad_frames++;
        }
        xSemaphoreGive(table_lock);
        if (st == PKT_OK) {
            Serial.printf("node %u seq %u: %.2f C %.2f %% %lu Pa bat %u mV alarms %u rssi %d\n", r.node_id, r.seq,
                          r.temp_c100 / 100.0, r.humidity_x100 / 100.0, (unsigned long)r.pressure_pa,
                          r.battery_mv, r.alarms, f.rssi);
        } else {
            Serial.printf("bad frame (%s) from %02X:%02X:.., queue drops %lu\n", pkt_status_name(st), f.mac[0],
                          f.mac[1], (unsigned long)dropped);
        }
    }
}

/* ---- ThingSpeak upload: one node per period, in turn ------------------------------------- */
static void upload_task(void *)
{
    int next = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(UPLOAD_PERIOD_MS));
        if (strlen(THINGSPEAK_API_KEY) == 0 || WiFi.status() != WL_CONNECTED) {
            continue;
        }
        node_entry_t e = {};
        xSemaphoreTake(table_lock, portMAX_DELAY);
        for (int k = 0; k < NT_MAX_NODES; k++) {
            const int i = (next + k) % NT_MAX_NODES;
            if (nt_online(&table.node[i], millis(), NODE_TIMEOUT_MS)) {
                e = table.node[i];
                next = i + 1;
                break;
            }
        }
        xSemaphoreGive(table_lock);
        if (!e.used) {
            continue;
        }
        char url[256];
        snprintf(url, sizeof(url),
                 "http://api.thingspeak.com/update?api_key=%s&field1=%.2f&field2=%.2f&field3=%.2f&field4=%.3f"
                 "&field5=%u&field6=%u",
                 THINGSPEAK_API_KEY, e.last.temp_c100 / 100.0, e.last.humidity_x100 / 100.0,
                 e.last.pressure_pa / 100.0, e.last.battery_mv / 1000.0, e.last.alarms, e.last.node_id);
        HTTPClient http;
        http.begin(url);
        const int code = http.GET();
        http.end();
        Serial.printf("ThingSpeak node %u: HTTP %d\n", e.last.node_id, code);
    }
}

/* ---- web --------------------------------------------------------------------------------- */
static void send_nodes()
{
    static char json[2048];
    xSemaphoreTake(table_lock, portMAX_DELAY);
    const size_t n = nt_json(&table, millis(), NODE_TIMEOUT_MS, json, sizeof(json));
    xSemaphoreGive(table_lock);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", n ? json : "{\"nodes\":[]}");
}

void gateway_setup()
{
    Serial.begin(115200);
    rx_queue = xQueueCreate(16, sizeof(rx_frame_t));
    table_lock = xSemaphoreCreateMutex();
    nt_init(&table);

    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s%04X", GATEWAY_SSID_PREFIX, (unsigned)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.mode(WIFI_AP_STA);
    if (strlen(WIFI_SSID) > 0) {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
            delay(250);
        }
    }
    /* When joined to a router the radio must stay on the router's channel,
     * so the access point (and with it ESP-NOW) follows that channel. */
    const int channel = WiFi.status() == WL_CONNECTED ? WiFi.channel() : GATEWAY_AP_CHANNEL;
    WiFi.softAP(ssid, GATEWAY_AP_PASSWORD, channel);
    Serial.printf("gateway \"%s\" on channel %d, dashboard http://%s", ssid, channel,
                  WiFi.softAPIP().toString().c_str());
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(" and http://%s", WiFi.localIP().toString().c_str());
    }
    Serial.println();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
    }
    esp_now_register_recv_cb(on_receive);

    server.on("/", [] { server.send_P(200, "text/html", DASHBOARD_HTML); });
    server.on("/api/nodes", send_nodes);
    server.onNotFound([] { server.send(404, "text/plain", "not found"); });
    server.begin();

    xTaskCreatePinnedToCore(process_task, "process", 4096, nullptr, 3, nullptr, 1);
    xTaskCreatePinnedToCore(upload_task, "upload", 6144, nullptr, 1, nullptr, 1);
}

void gateway_loop()
{
    server.handleClient();
    delay(2);
}

#endif /* NET_ROLE == ROLE_GATEWAY */
