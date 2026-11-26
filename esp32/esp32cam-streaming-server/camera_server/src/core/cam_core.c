/**
 * @file    cam_core.c
 * @brief   Authentication, MJPEG framing, measurement, pacing, settings and status.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "cam_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- base64 and Basic authentication -------------------------------------------------- */

static int b64_value(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

int b64_decode(const char *in, char *out, size_t cap)
{
    const size_t n = strlen(in);
    if (n % 4 != 0 || cap == 0) {
        return -1;
    }
    size_t len = 0;
    for (size_t i = 0; i < n; i += 4) {
        int v[4];
        int pad = 0;
        for (int k = 0; k < 4; k++) {
            const char c = in[i + k];
            if (c == '=') {
                /* padding only at the very end, in the last one or two places */
                if (i + 4 != n || k < 2) {
                    return -1;
                }
                pad++;
                v[k] = 0;
            } else {
                if (pad) {
                    return -1;   /* data after padding */
                }
                v[k] = b64_value(c);
                if (v[k] < 0) {
                    return -1;
                }
            }
        }
        const uint32_t triple = ((uint32_t)v[0] << 18) | ((uint32_t)v[1] << 12) | ((uint32_t)v[2] << 6) | (uint32_t)v[3];
        const int bytes = 3 - pad;
        for (int k = 0; k < bytes; k++) {
            if (len + 1 >= cap) {
                return -1;
            }
            out[len++] = (char)((triple >> (16 - 8 * k)) & 0xFFu);
        }
    }
    out[len] = '\0';
    return (int)len;
}

/* Compare without an early exit: the time does not depend on where the strings differ. */
static bool equal_const_time(const char *a, size_t na, const char *b, size_t nb)
{
    unsigned diff = (unsigned)(na ^ nb);
    const size_t n = na > nb ? na : nb;
    for (size_t i = 0; i < n; i++) {
        const unsigned ca = i < na ? (unsigned char)a[i] : 0u;
        const unsigned cb = i < nb ? (unsigned char)b[i] : 0u;
        diff |= ca ^ cb;
    }
    return diff == 0;
}

bool auth_check_basic(const char *header, const char *user, const char *pass)
{
    if (header == NULL || strncmp(header, "Basic ", 6) != 0) {
        return false;
    }
    char decoded[128];
    const int n = b64_decode(header + 6, decoded, sizeof(decoded));
    if (n < 0) {
        return false;
    }
    char expected[128];
    const int m = snprintf(expected, sizeof(expected), "%s:%s", user, pass);
    if (m < 0 || (size_t)m >= sizeof(expected)) {
        return false;
    }
    return equal_const_time(decoded, (size_t)n, expected, (size_t)m);
}

bool auth_check_token(const char *given, const char *token)
{
    if (given == NULL || token == NULL || token[0] == 0) {
        return false;
    }
    return equal_const_time(given, strlen(given), token, strlen(token));
}

void auth_limiter_init(auth_limiter_t *l)
{
    memset(l, 0, sizeof(*l));
}

bool auth_limiter_blocked(auth_limiter_t *l, uint32_t now_ms)
{
    if (l->locked && (int32_t)(now_ms - l->locked_until) >= 0) {
        l->locked = false;   /* lockout over: start counting afresh */
        l->fails = 0;
    }
    return l->locked;
}

void auth_limiter_record(auth_limiter_t *l, bool ok, uint32_t now_ms)
{
    if (ok) {
        l->fails = 0;
        return;
    }
    if (l->fails == 0 || now_ms - l->window_start >= AUTH_WINDOW_MS) {
        l->window_start = now_ms;
        l->fails = 0;
    }
    if (++l->fails >= AUTH_MAX_FAILS) {
        l->locked = true;
        l->locked_until = now_ms + AUTH_LOCKOUT_MS;
    }
}

/* ---- MJPEG ---------------------------------------------------------------------------- */

const char *mjpeg_content_type(void)
{
    return "multipart/x-mixed-replace;boundary=" MJPEG_BOUNDARY;
}

size_t mjpeg_part_header(char *out, size_t cap, size_t jpeg_len, uint32_t ts_ms)
{
    const int w = snprintf(out, cap,
                           "\r\n--" MJPEG_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\n"
                           "X-Timestamp: %lu.%03lu\r\n\r\n",
                           (unsigned long)jpeg_len, (unsigned long)(ts_ms / 1000u), (unsigned long)(ts_ms % 1000u));
    return (w < 0 || (size_t)w >= cap) ? 0u : (size_t)w;
}

/* ---- measurement and pacing ----------------------------------------------------------- */

void fps_init(fps_meter_t *m)
{
    memset(m, 0, sizeof(*m));
}

void fps_frame(fps_meter_t *m, uint32_t now_ms, size_t bytes)
{
    m->frames++;
    if (!m->started) {
        m->started = true;
        m->last_ms = now_ms;
        m->frame_bytes = (float)bytes;
        return;
    }
    const float dt = (float)(now_ms - m->last_ms);
    m->last_ms = now_ms;
    if (m->frame_ms == 0.0f) {
        m->frame_ms = dt;   /* first interval: no history to smooth with */
    } else {
        m->frame_ms += (dt - m->frame_ms) / 8.0f;
    }
    m->frame_bytes += ((float)bytes - m->frame_bytes) / 8.0f;
}

float fps_value(const fps_meter_t *m)
{
    return m->frame_ms > 0.0f ? 1000.0f / m->frame_ms : 0.0f;
}

float fps_kbps(const fps_meter_t *m)
{
    return fps_value(m) * m->frame_bytes * 8.0f / 1000.0f;
}

uint32_t pace_delay_ms(uint32_t frame_start_ms, uint32_t now_ms, uint8_t target_fps)
{
    if (target_fps == 0) {
        return 0;
    }
    const uint32_t period = 1000u / target_fps;
    const uint32_t spent = now_ms - frame_start_ms;
    return spent >= period ? 0u : period - spent;
}

/* ---- settings ------------------------------------------------------------------------- */

static const setting_info_t SETTINGS[SET_COUNT] = {
    [SET_FRAMESIZE] = {"framesize", 0, FRAME_SIZE_COUNT - 1, 3},   /* VGA */
    [SET_QUALITY] = {"quality", 10, 63, 12},                       /* JPEG: lower = better, bigger */
    [SET_BRIGHTNESS] = {"brightness", -2, 2, 0},
    [SET_CONTRAST] = {"contrast", -2, 2, 0},
    [SET_SATURATION] = {"saturation", -2, 2, 0},
    [SET_VFLIP] = {"vflip", 0, 1, 0},
    [SET_HMIRROR] = {"hmirror", 0, 1, 0},
    [SET_FLASH] = {"flash", 0, 100, 0},                            /* LED brightness in percent */
    [SET_FPS] = {"fps", 0, 30, 15},                                /* frame-rate limit, 0 = none */
};

static const frame_size_t FRAME_SIZES[FRAME_SIZE_COUNT] = {
    {"QQVGA", 160, 120}, {"QVGA", 320, 240}, {"CIF", 400, 296}, {"VGA", 640, 480}, {"SVGA", 800, 600},
    {"XGA", 1024, 768},  {"HD", 1280, 720},  {"SXGA", 1280, 1024}, {"UXGA", 1600, 1200},
};

const setting_info_t *setting_info(setting_id_t id)
{
    return (unsigned)id < SET_COUNT ? &SETTINGS[id] : NULL;
}

const frame_size_t *frame_size(int index)
{
    return index >= 0 && index < FRAME_SIZE_COUNT ? &FRAME_SIZES[index] : NULL;
}

bool setting_parse(const char *var, const char *val, setting_id_t *id, int *value)
{
    if (var == NULL || val == NULL || *val == '\0') {
        return false;
    }
    char *end;
    const long v = strtol(val, &end, 10);
    if (*end != '\0') {
        return false;
    }
    for (int i = 0; i < SET_COUNT; i++) {
        if (strcmp(var, SETTINGS[i].name) == 0) {
            if (v < SETTINGS[i].min || v > SETTINGS[i].max) {
                return false;
            }
            *id = (setting_id_t)i;
            *value = (int)v;
            return true;
        }
    }
    return false;
}

/* ---- status --------------------------------------------------------------------------- */

size_t status_json(const cam_status_t *s, char *out, size_t cap)
{
    size_t len = 0;
    int w = snprintf(out, cap, "{");
    for (int i = 0; i < SET_COUNT && w >= 0 && (size_t)w < cap - len; i++) {
        len += (size_t)w;
        w = snprintf(out + len, cap - len, "\"%s\":%d,", SETTINGS[i].name, s->settings[i]);
    }
    if (w < 0 || (size_t)w >= cap - len) {
        goto overflow;
    }
    len += (size_t)w;
    const frame_size_t *fs = frame_size(s->settings[SET_FRAMESIZE]);
    w = snprintf(out + len, cap - len,
                 "\"resolution\":\"%s %ux%u\",\"fps_now\":%.1f,\"kbps\":%.0f,\"frames\":%lu,\"clients\":%lu,"
                 "\"uptime\":%lu,\"heap\":%lu,\"psram\":%lu,\"rssi\":%d}",
                 fs ? fs->name : "?", fs ? fs->w : 0u, fs ? fs->h : 0u, (double)s->fps, (double)s->kbps,
                 (unsigned long)s->frames, (unsigned long)s->clients, (unsigned long)s->uptime_s,
                 (unsigned long)s->heap_free, (unsigned long)s->psram_free, s->rssi);
    if (w < 0 || (size_t)w >= cap - len) {
        goto overflow;
    }
    return len + (size_t)w;

overflow:
    if (cap) {
        out[0] = '\0';
    }
    return 0;
}
