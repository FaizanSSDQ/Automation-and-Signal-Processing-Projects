/**
 * Host tests for the ESP32-CAM server core: base64 and Basic authentication,
 * the lockout after wrong passwords, MJPEG framing, frame-rate measurement,
 * pacing, settings validation and the status JSON.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../camera_server/src/core/cam_core.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)
#define NEAR(a, b, tol) (fabsf((float)(a) - (float)(b)) <= (tol))

static void test_base64(void)
{
    char out[64];
    CHECK(b64_decode("TWFu", out, sizeof(out)) == 3 && strcmp(out, "Man") == 0);
    CHECK(b64_decode("TWE=", out, sizeof(out)) == 2 && strcmp(out, "Ma") == 0);
    CHECK(b64_decode("TQ==", out, sizeof(out)) == 1 && strcmp(out, "M") == 0);
    CHECK(b64_decode("", out, sizeof(out)) == 0 && out[0] == '\0');
    CHECK(b64_decode("TWF", out, sizeof(out)) == -1);       /* length not a multiple of 4 */
    CHECK(b64_decode("TW=u", out, sizeof(out)) == -1);      /* data after padding */
    CHECK(b64_decode("=WFu", out, sizeof(out)) == -1);
    CHECK(b64_decode("TWFu TWFu", out, sizeof(out)) == -1);
    CHECK(b64_decode("TWFuTWFu", out, 4) == -1);            /* does not fit */
}

static void test_auth(void)
{
    /* "admin:cam-pass" */
    CHECK(auth_check_basic("Basic YWRtaW46Y2FtLXBhc3M=", "admin", "cam-pass"));
    CHECK(!auth_check_basic("Basic YWRtaW46Y2FtLXBhc3M=", "admin", "cam-pasS"));
    CHECK(!auth_check_basic("Basic YWRtaW46Y2FtLXBhc3M=", "admin", "cam-pas"));     /* prefix only */
    CHECK(!auth_check_basic("Basic YWRtaW46Y2FtLXBhc3M=", "admin", "cam-pass1"));   /* longer */
    CHECK(!auth_check_basic("Bearer YWRtaW46Y2FtLXBhc3M=", "admin", "cam-pass"));
    CHECK(!auth_check_basic("Basic !!!!", "admin", "cam-pass"));
    CHECK(!auth_check_basic(NULL, "admin", "cam-pass"));
    CHECK(auth_check_token("3f9a0c71", "3f9a0c71"));
    CHECK(!auth_check_token("3f9a0c7", "3f9a0c71") && !auth_check_token("", "3f9a0c71"));
    CHECK(!auth_check_token("", "") && !auth_check_token(NULL, "3f9a0c71"));   /* no token set: refuse */
}

static void test_limiter(void)
{
    auth_limiter_t l;
    auth_limiter_init(&l);
    uint32_t t = 1000;
    for (int i = 0; i < AUTH_MAX_FAILS - 1; i++) {
        CHECK(!auth_limiter_blocked(&l, t));
        auth_limiter_record(&l, false, t);
        t += 1000;
    }
    CHECK(!auth_limiter_blocked(&l, t));
    auth_limiter_record(&l, true, t);                  /* a correct login resets the count */
    for (int i = 0; i < AUTH_MAX_FAILS - 1; i++) {
        auth_limiter_record(&l, false, t);
    }
    CHECK(!auth_limiter_blocked(&l, t));
    auth_limiter_record(&l, false, t);                 /* fifth wrong attempt */
    CHECK(auth_limiter_blocked(&l, t));
    CHECK(auth_limiter_blocked(&l, t + AUTH_LOCKOUT_MS - 1));
    CHECK(!auth_limiter_blocked(&l, t + AUTH_LOCKOUT_MS));

    /* Failures spread over more than the window never lock. */
    auth_limiter_init(&l);
    t = 0;
    for (int i = 0; i < 20; i++) {
        auth_limiter_record(&l, false, t);
        t += AUTH_WINDOW_MS / 4 + 1;
    }
    CHECK(!auth_limiter_blocked(&l, t));

    /* Lockout timing survives the 32-bit millisecond counter wrapping. */
    auth_limiter_init(&l);
    t = 0xFFFFFF00u;
    for (int i = 0; i < AUTH_MAX_FAILS; i++) {
        auth_limiter_record(&l, false, t);
    }
    CHECK(auth_limiter_blocked(&l, t + 1000));
    CHECK(!auth_limiter_blocked(&l, t + AUTH_LOCKOUT_MS));
}

static void test_mjpeg(void)
{
    char h[160];
    const size_t n = mjpeg_part_header(h, sizeof(h), 23456, 61234);
    CHECK(n == strlen(h));
    CHECK(strcmp(h, "\r\n--esp32camframe\r\nContent-Type: image/jpeg\r\nContent-Length: 23456\r\n"
                    "X-Timestamp: 61.234\r\n\r\n") == 0);
    CHECK(mjpeg_part_header(h, 20, 1, 0) == 0);
    CHECK(strcmp(mjpeg_content_type(), "multipart/x-mixed-replace;boundary=esp32camframe") == 0);
}

static void test_fps(void)
{
    fps_meter_t m;
    fps_init(&m);
    CHECK(fps_value(&m) == 0.0f);
    uint32_t t = 5000;
    for (int i = 0; i < 100; i++) {
        fps_frame(&m, t, 20000);
        t += 50;                                      /* 20 frames/s */
    }
    CHECK(NEAR(fps_value(&m), 20.0f, 0.01f) && m.frames == 100);
    CHECK(NEAR(fps_kbps(&m), 3200.0f, 1.0f));          /* 20 * 20000 bytes * 8 bits */
    for (int i = 0; i < 40; i++) {                    /* slows to 10 frames/s */
        fps_frame(&m, t, 20000);
        t += 100;
    }
    CHECK(NEAR(fps_value(&m), 10.0f, 0.1f));

    CHECK(pace_delay_ms(1000, 1010, 20) == 40);
    CHECK(pace_delay_ms(1000, 1060, 20) == 0);        /* already late */
    CHECK(pace_delay_ms(1000, 1010, 0) == 0);         /* no limit */
    CHECK(pace_delay_ms(0xFFFFFFF0u, 5, 10) == 79);   /* across the counter wrap */
}

static void test_settings(void)
{
    setting_id_t id;
    int v;
    CHECK(setting_parse("quality", "20", &id, &v) && id == SET_QUALITY && v == 20);
    CHECK(setting_parse("brightness", "-2", &id, &v) && id == SET_BRIGHTNESS && v == -2);
    CHECK(setting_parse("framesize", "8", &id, &v) && v == 8);
    CHECK(!setting_parse("framesize", "9", &id, &v));
    CHECK(!setting_parse("quality", "9", &id, &v));
    CHECK(!setting_parse("quality", "12x", &id, &v));
    CHECK(!setting_parse("quality", "", &id, &v));
    CHECK(!setting_parse("exposure", "1", &id, &v));
    CHECK(!setting_parse(NULL, "1", &id, &v));
    CHECK(frame_size(3) && strcmp(frame_size(3)->name, "VGA") == 0 && frame_size(3)->w == 640);
    CHECK(frame_size(-1) == NULL && frame_size(FRAME_SIZE_COUNT) == NULL);
    for (int i = 0; i < SET_COUNT; i++) {             /* every default is inside its range */
        const setting_info_t *s = setting_info((setting_id_t)i);
        CHECK(s->def >= s->min && s->def <= s->max);
    }
}

static void test_status(void)
{
    cam_status_t s;
    memset(&s, 0, sizeof(s));
    for (int i = 0; i < SET_COUNT; i++) {
        s.settings[i] = setting_info((setting_id_t)i)->def;
    }
    s.fps = 14.96f;
    s.kbps = 2390.4f;
    s.frames = 1234;
    s.clients = 1;
    s.uptime_s = 3600;
    s.heap_free = 180000;
    s.psram_free = 4000000;
    s.rssi = -61;
    char out[512];
    const size_t n = status_json(&s, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strcmp(out, "{\"framesize\":3,\"quality\":12,\"brightness\":0,\"contrast\":0,\"saturation\":0,"
                      "\"vflip\":0,\"hmirror\":0,\"flash\":0,\"fps\":15,\"resolution\":\"VGA 640x480\","
                      "\"fps_now\":15.0,\"kbps\":2390,\"frames\":1234,\"clients\":1,\"uptime\":3600,"
                      "\"heap\":180000,\"psram\":4000000,\"rssi\":-61}") == 0);
    CHECK(status_json(&s, out, 60) == 0 && out[0] == '\0');
}

int main(void)
{
    test_base64();
    test_auth();
    test_limiter();
    test_mjpeg();
    test_fps();
    test_settings();
    test_status();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
