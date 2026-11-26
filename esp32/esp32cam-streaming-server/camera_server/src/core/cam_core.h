/**
 * @file    cam_core.h
 * @brief   Hardware-independent parts of the ESP32-CAM server: HTTP Basic
 *          authentication with a lockout, MJPEG framing, frame-rate and
 *          bandwidth measurement, frame pacing, camera setting validation
 *          and the status JSON.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CAM_CORE_H
#define CAM_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- authentication ------------------------------------------------------------------- */

/** Decode standard base64. @return decoded length, or -1 on bad input or overflow. Output is NUL-terminated. */
int b64_decode(const char *in, char *out, size_t cap);

/**
 * Check an "Authorization" header value ("Basic <base64 user:pass>").
 * The comparison takes the same time whatever the position of the first
 * wrong character, so the password cannot be guessed one character at a time.
 */
bool auth_check_basic(const char *header, const char *user, const char *pass);

/**
 * Check a stream token from the URL. The stream runs on its own port, and
 * browsers do not reuse a password across ports, so the page passes a random
 * per-boot token instead. Constant time, like the password check.
 */
bool auth_check_token(const char *given, const char *token);

#define AUTH_MAX_FAILS   5        /* wrong attempts allowed ... */
#define AUTH_WINDOW_MS   60000u   /* ... within this time, then ... */
#define AUTH_LOCKOUT_MS  60000u   /* ... every request is refused for this long */

typedef struct {
    uint32_t window_start;
    uint32_t locked_until;
    uint8_t fails;
    bool locked;
} auth_limiter_t;

void auth_limiter_init(auth_limiter_t *l);
/** True while requests must be refused (HTTP 429). */
bool auth_limiter_blocked(auth_limiter_t *l, uint32_t now_ms);
/** Record the result of one authentication attempt. */
void auth_limiter_record(auth_limiter_t *l, bool ok, uint32_t now_ms);

/* ---- MJPEG -------------------------------------------------------------------------- */

#define MJPEG_BOUNDARY "esp32camframe"

/** Content-Type value of the stream response. */
const char *mjpeg_content_type(void);

/** Part header written before each JPEG. @return length, 0 if it does not fit. */
size_t mjpeg_part_header(char *out, size_t cap, size_t jpeg_len, uint32_t ts_ms);

/* ---- frame rate, bandwidth, pacing -------------------------------------------------- */

typedef struct {
    uint32_t last_ms;       /* time of the previous frame */
    float frame_ms;         /* smoothed frame interval */
    float frame_bytes;      /* smoothed frame size */
    uint32_t frames;        /* total frames */
    bool started;
} fps_meter_t;

void fps_init(fps_meter_t *m);
/** Record one frame of @p bytes sent at @p now_ms (exponential smoothing, alpha 1/8). */
void fps_frame(fps_meter_t *m, uint32_t now_ms, size_t bytes);
float fps_value(const fps_meter_t *m);
/** Smoothed bandwidth in kbit/s. */
float fps_kbps(const fps_meter_t *m);

/**
 * How long to wait before the next frame to hold @p target_fps, given when the
 * previous frame started and the time now. 0 = no limit or already late.
 */
uint32_t pace_delay_ms(uint32_t frame_start_ms, uint32_t now_ms, uint8_t target_fps);

/* ---- camera settings ------------------------------------------------------------------ */

typedef enum {
    SET_FRAMESIZE, SET_QUALITY, SET_BRIGHTNESS, SET_CONTRAST, SET_SATURATION,
    SET_VFLIP, SET_HMIRROR, SET_FLASH, SET_FPS, SET_COUNT
} setting_id_t;

typedef struct {
    const char *name;
    int16_t min, max, def;
} setting_info_t;

const setting_info_t *setting_info(setting_id_t id);

/** Look up and range-check "var=val". @return false for an unknown name, bad number or out of range. */
bool setting_parse(const char *var, const char *val, setting_id_t *id, int *value);

/** Resolutions offered, smallest first; SET_FRAMESIZE is an index into this list. */
typedef struct {
    const char *name;
    uint16_t w, h;
} frame_size_t;

#define FRAME_SIZE_COUNT 9
const frame_size_t *frame_size(int index);

/* ---- status --------------------------------------------------------------------------- */

typedef struct {
    int settings[SET_COUNT];
    float fps, kbps;
    uint32_t frames, clients, uptime_s, heap_free, psram_free;
    int rssi;   /* 0 in access-point mode */
} cam_status_t;

/** Status JSON for the page. @return length, 0 if it does not fit. */
size_t status_json(const cam_status_t *s, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* CAM_CORE_H */
