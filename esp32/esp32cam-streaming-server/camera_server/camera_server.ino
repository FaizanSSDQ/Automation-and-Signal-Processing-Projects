/**
 * @file    camera_server.ino
 * @brief   Password-protected MJPEG streaming server for the AI-Thinker ESP32-CAM.
 *
 * Two HTTP servers (ESP-IDF esp_http_server), each in its own task:
 *   port 80   page, /status, /control, /capture, /token   (Basic authentication)
 *   port 81   /stream: MJPEG, one JPEG after another in a multipart response
 * The stream runs on its own server because a stream request never ends;
 * on the same server it would block every other request.
 *
 * Security: every request needs the login, except the stream, which accepts
 * a random per-boot token that only a logged-in page can read. Five wrong
 * logins within a minute lock the login for a minute.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include <esp_random.h>

#include "config.h"
#include "src/core/cam_core.h"
#include "web_ui.h"

/* Index in the core's resolution list -> camera driver frame size. */
static const framesize_t FRAME_SIZES[FRAME_SIZE_COUNT] = {
    FRAMESIZE_QQVGA, FRAMESIZE_QVGA, FRAMESIZE_CIF,  FRAMESIZE_VGA,  FRAMESIZE_SVGA,
    FRAMESIZE_XGA,   FRAMESIZE_HD,   FRAMESIZE_SXGA, FRAMESIZE_UXGA,
};

/* ---- shared state (guarded by lock) ------------------------------------------------------ */
static SemaphoreHandle_t lock;
static int settings[SET_COUNT];
static fps_meter_t meter;
static auth_limiter_t limiter;
static uint32_t stream_clients;
static char token[17];
static bool station_mode;

static httpd_handle_t web_server, stream_server;

/* ---- camera ------------------------------------------------------------------------------ */
static bool camera_begin()
{
    camera_config_t c = {};
    c.pin_pwdn = CAM_PIN_PWDN;
    c.pin_reset = CAM_PIN_RESET;
    c.pin_xclk = CAM_PIN_XCLK;
    c.pin_sccb_sda = CAM_PIN_SIOD;
    c.pin_sccb_scl = CAM_PIN_SIOC;
    c.pin_d7 = CAM_PIN_D7;
    c.pin_d6 = CAM_PIN_D6;
    c.pin_d5 = CAM_PIN_D5;
    c.pin_d4 = CAM_PIN_D4;
    c.pin_d3 = CAM_PIN_D3;
    c.pin_d2 = CAM_PIN_D2;
    c.pin_d1 = CAM_PIN_D1;
    c.pin_d0 = CAM_PIN_D0;
    c.pin_vsync = CAM_PIN_VSYNC;
    c.pin_href = CAM_PIN_HREF;
    c.pin_pclk = CAM_PIN_PCLK;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_3;     /* away from the timer Arduino gives the flash LED */
    c.ledc_channel = LEDC_CHANNEL_7;
    c.pixel_format = PIXFORMAT_JPEG;   /* the OV2640 compresses in hardware */
    if (psramFound()) {
        /* Two frame buffers in PSRAM: the sensor fills one while the other is sent. */
        c.frame_size = FRAMESIZE_UXGA;   /* allocate for the largest size */
        c.jpeg_quality = 12;
        c.fb_count = 2;
        c.fb_location = CAMERA_FB_IN_PSRAM;
        c.grab_mode = CAMERA_GRAB_LATEST;
    } else {
        c.frame_size = FRAMESIZE_SVGA;
        c.jpeg_quality = 14;
        c.fb_count = 1;
        c.fb_location = CAMERA_FB_IN_DRAM;
        c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }
    const esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        Serial.printf("camera init failed: 0x%x (check the ribbon cable)\n", err);
        return false;
    }
    return true;
}

/* Apply one setting to the hardware. Settings are validated by setting_parse() first. */
static void apply_setting(setting_id_t id, int v)
{
    sensor_t *s = esp_camera_sensor_get();
    switch (id) {
    case SET_FRAMESIZE:
        if (!psramFound() && v > 4) {
            v = 4;   /* without PSRAM, SVGA is the largest frame that fits */
        }
        s->set_framesize(s, FRAME_SIZES[v]);
        break;
    case SET_QUALITY: s->set_quality(s, v); break;
    case SET_BRIGHTNESS: s->set_brightness(s, v); break;
    case SET_CONTRAST: s->set_contrast(s, v); break;
    case SET_SATURATION: s->set_saturation(s, v); break;
    case SET_VFLIP: s->set_vflip(s, v); break;
    case SET_HMIRROR: s->set_hmirror(s, v); break;
    case SET_FLASH: ledcWrite(PIN_FLASH_LED, (uint32_t)v * 255u / 100u); break;
    case SET_FPS:
    case SET_COUNT: break;   /* the frame-rate limit is read by the stream loop */
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    settings[id] = v;
    xSemaphoreGive(lock);
}

/* ---- authentication ---------------------------------------------------------------------- */
static bool authorized(httpd_req_t *req, bool allow_token)
{
    const uint32_t now = millis();
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool blocked = auth_limiter_blocked(&limiter, now);
    xSemaphoreGive(lock);
    if (blocked) {
        httpd_resp_set_status(req, "429 Too Many Requests");
        httpd_resp_sendstr(req, "too many wrong logins, try again in a minute");
        return false;
    }

    if (allow_token) {
        char query[64], given[24];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
            httpd_query_key_value(query, "token", given, sizeof(given)) == ESP_OK && auth_check_token(given, token)) {
            return true;
        }
    }

    char header[160];
    const bool has_header =
        httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header)) == ESP_OK;
    const bool ok = has_header && auth_check_basic(header, CAM_USER, CAM_PASSWORD);
    if (has_header) {   /* the browser's first request has no header: not a failed attempt */
        xSemaphoreTake(lock, portMAX_DELAY);
        auth_limiter_record(&limiter, ok, now);
        xSemaphoreGive(lock);
    }
    if (!ok) {
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32-CAM\"");
        httpd_resp_sendstr(req, "login required");
    }
    return ok;
}

/* ---- handlers on port 80 ----------------------------------------------------------------- */
static esp_err_t index_handler(httpd_req_t *req)
{
    if (!authorized(req, false)) {
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, WEB_UI_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t token_handler(httpd_req_t *req)
{
    if (!authorized(req, false)) {
        return ESP_OK;
    }
    char body[40];
    snprintf(body, sizeof(body), "{\"token\":\"%s\",\"port\":%d}", token, STREAM_PORT);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    if (!authorized(req, false)) {
        return ESP_OK;
    }
    cam_status_t st = {};
    xSemaphoreTake(lock, portMAX_DELAY);
    memcpy(st.settings, settings, sizeof(settings));
    st.fps = stream_clients ? fps_value(&meter) : 0.0f;
    st.kbps = stream_clients ? fps_kbps(&meter) : 0.0f;
    st.frames = meter.frames;
    st.clients = stream_clients;
    xSemaphoreGive(lock);
    st.uptime_s = millis() / 1000u;
    st.heap_free = ESP.getFreeHeap();
    st.psram_free = ESP.getFreePsram();
    st.rssi = station_mode ? WiFi.RSSI() : 0;

    char json[512];
    const size_t n = status_json(&st, json, sizeof(json));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return n ? httpd_resp_send(req, json, (ssize_t)n) : httpd_resp_send_500(req);
}

/* /control?var=quality&val=20 */
static esp_err_t control_handler(httpd_req_t *req)
{
    if (!authorized(req, false)) {
        return ESP_OK;
    }
    char query[64], var[16], val[8];
    setting_id_t id;
    int value;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "var", var, sizeof(var)) != ESP_OK ||
        httpd_query_key_value(query, "val", val, sizeof(val)) != ESP_OK || !setting_parse(var, val, &id, &value)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown setting or value out of range");
        return ESP_OK;
    }
    apply_setting(id, value);
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    if (!authorized(req, false)) {
        return ESP_OK;
    }
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, (ssize_t)fb->len);
    esp_camera_fb_return(fb);
    return res;
}

/* ---- stream on port 81 ------------------------------------------------------------------- */
static esp_err_t stream_handler(httpd_req_t *req)
{
    if (!authorized(req, true)) {
        return ESP_OK;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    const bool full = stream_clients >= MAX_STREAM_CLIENTS;
    if (!full) {
        stream_clients++;
    }
    xSemaphoreGive(lock);
    if (full) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "too many viewers");
    }

    httpd_resp_set_type(req, mjpeg_content_type());
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    digitalWrite(PIN_STATUS_LED, LOW);   /* on while streaming */

    esp_err_t res = ESP_OK;
    char part[128];
    while (res == ESP_OK) {
        const uint32_t start = millis();
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == nullptr) {
            res = ESP_FAIL;
            break;
        }
        const size_t n = mjpeg_part_header(part, sizeof(part), fb->len, start);
        res = httpd_resp_send_chunk(req, part, (ssize_t)n);
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, (ssize_t)fb->len);
        }
        const size_t bytes = fb->len;
        esp_camera_fb_return(fb);

        xSemaphoreTake(lock, portMAX_DELAY);
        fps_frame(&meter, millis(), bytes);
        const int fps_limit = settings[SET_FPS];
        xSemaphoreGive(lock);
        const uint32_t wait = pace_delay_ms(start, millis(), (uint8_t)fps_limit);
        if (wait) {
            vTaskDelay(pdMS_TO_TICKS(wait));
        }
    }

    xSemaphoreTake(lock, portMAX_DELAY);
    stream_clients--;
    if (stream_clients == 0) {
        fps_init(&meter);
    }
    const bool none = stream_clients == 0;
    xSemaphoreGive(lock);
    if (none) {
        digitalWrite(PIN_STATUS_LED, HIGH);
    }
    return res;   /* the viewer closed the page: the error ends the request */
}

static void start_servers()
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 8;
    cfg.server_port = 80;
    cfg.ctrl_port = 32768;
    cfg.core_id = 0;
    const httpd_uri_t pages[] = {
        {"/", HTTP_GET, index_handler, nullptr},
        {"/token", HTTP_GET, token_handler, nullptr},
        {"/status", HTTP_GET, status_handler, nullptr},
        {"/control", HTTP_GET, control_handler, nullptr},
        {"/capture", HTTP_GET, capture_handler, nullptr},
    };
    if (httpd_start(&web_server, &cfg) == ESP_OK) {
        for (const httpd_uri_t &u : pages) {
            httpd_register_uri_handler(web_server, &u);
        }
    }

    cfg.server_port = STREAM_PORT;
    cfg.ctrl_port = 32769;
    cfg.core_id = 1;   /* sending frames gets the other core */
    cfg.max_open_sockets = MAX_STREAM_CLIENTS + 1;
    const httpd_uri_t stream = {"/stream", HTTP_GET, stream_handler, nullptr};
    if (httpd_start(&stream_server, &cfg) == ESP_OK) {
        httpd_register_uri_handler(stream_server, &stream);
    }
}

/* ---- network ----------------------------------------------------------------------------- */
static void network_begin()
{
    WiFi.setSleep(false);   /* modem sleep adds latency and drops frames */
    if (strlen(WIFI_SSID) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.printf("joining \"%s\"", WIFI_SSID);
        for (int i = 0; i < 60 && WiFi.status() != WL_CONNECTED; i++) {
            delay(250);
            Serial.print('.');
        }
        Serial.println();
        station_mode = WiFi.status() == WL_CONNECTED;
    }
    if (station_mode) {
        Serial.printf("open http://%s (signal %d dBm)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        return;
    }
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "%s%04X", AP_SSID_PREFIX, (unsigned)(ESP.getEfuseMac() & 0xFFFF));
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, AP_PASSWORD);
    Serial.printf("access point \"%s\", open http://%s\n", ssid, WiFi.softAPIP().toString().c_str());
}

void setup()
{
    Serial.begin(115200);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);
    pinMode(PIN_FLASH_LED, OUTPUT);
    digitalWrite(PIN_FLASH_LED, LOW);

    lock = xSemaphoreCreateMutex();
    fps_init(&meter);
    auth_limiter_init(&limiter);

    if (!HAVE_SECRETS || strcmp(CAM_PASSWORD, "change-me-now") == 0) {
        Serial.println("WARNING: using the example login; copy secrets.example.h to secrets.h and change it");
    }
    if (!camera_begin()) {
        for (;;) {   /* blink the red LED: camera missing */
            digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
            delay(200);
        }
    }
    /* The camera clock (XCLK) runs on LEDC timer 3 / channel 7, which Arduino does not track;
     * the flash takes channel 8 (low-speed group, timer 0), so the two never share a timer. */
    ledcAttachChannel(PIN_FLASH_LED, FLASH_PWM_HZ, 8, FLASH_LEDC_CHANNEL);
    for (int i = 0; i < SET_COUNT; i++) {
        apply_setting((setting_id_t)i, setting_info((setting_id_t)i)->def);
    }
    Serial.printf("camera ready, PSRAM %s\n", psramFound() ? "found" : "missing (SVGA max)");

    network_begin();
    /* After Wi-Fi is up: the radio feeds the hardware random generator. */
    snprintf(token, sizeof(token), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    start_servers();
}

void loop()
{
    vTaskDelete(nullptr);   /* the servers run in their own tasks */
}
