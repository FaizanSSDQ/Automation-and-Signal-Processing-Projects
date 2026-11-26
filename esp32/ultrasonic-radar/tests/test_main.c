/**
 * Host tests for the radar core: sweep stepping, servo timing, temperature-
 * corrected ranging, filtering, object detection and the JSON for the display.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../radar/src/core/radar_core.h"

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

static void test_sweep(void)
{
    sweep_t s;
    sweep_init(&s);
    int up = 0, last = 0;
    for (int i = 0; i < 90; i++) {
        last = sweep_next(&s);
        up++;
    }
    CHECK(last == 180 && up == 90);
    CHECK(sweep_next(&s) == 178);            /* turns around without repeating 180 */
    for (int i = 0; i < 88; i++) {
        sweep_next(&s);
    }
    CHECK(s.angle == 2 && sweep_next(&s) == 0 && sweep_next(&s) == 2);
    CHECK(radar_index(0) == 0 && radar_index(180) == RADAR_POINTS - 1 && radar_index(91) == 45);
    CHECK(radar_index(-5) == 0 && radar_index(999) == RADAR_POINTS - 1);
}

static void test_servo(void)
{
    CHECK(servo_pulse_us(0, 500, 2500) == 500);
    CHECK(servo_pulse_us(90, 500, 2500) == 1500);
    CHECK(servo_pulse_us(180, 500, 2500) == 2500);
    CHECK(servo_pulse_us(200, 500, 2500) == 2500);
    /* 1.5 ms of a 20 ms period at 16 bits: 7.5 % of 65535 */
    CHECK(servo_duty(1500, 20000, 16) == 4915);
}

static void test_ranging(void)
{
    CHECK(NEAR(sound_cm_per_us(20.0f) * 10000.0f, 343.42f, 0.01f));
    bool ok;
    /* 100 cm there and back at 20 degC: 5824 us */
    CHECK(NEAR(echo_to_cm(5824, 20.0f, &ok), 100.00f, 0.05f) && ok);
    /* The same echo read with the 0 degC speed of sound: 3.5 cm short. */
    CHECK(NEAR(echo_to_cm(5824, 0.0f, &ok), 96.47f, 0.05f) && ok);
    echo_to_cm(0, 20.0f, &ok);
    CHECK(!ok);
    echo_to_cm(12000, 20.0f, &ok);
    CHECK(!ok);                               /* 206 cm: beyond the display range */

    CHECK(NEAR(sharp_to_cm(1.0f, &ok), 27.73f, 0.01f) && ok);
    sharp_to_cm(0.2f, &ok);
    CHECK(!ok);

    CHECK(median3(5, 1, 3) == 3 && median3(1, 9, 2) == 2 && median3(7, 7, 100) == 7);
}

static void test_objects(void)
{
    float cm[RADAR_POINTS];
    bool ok[RADAR_POINTS];
    for (int i = 0; i < RADAR_POINTS; i++) {
        cm[i] = 180.0f;                       /* background wall beyond the 150 cm limit */
        ok[i] = true;
    }
    for (int i = 10; i <= 14; i++) {          /* box at 20..28 degrees, 60 cm */
        cm[i] = 60.0f + (float)(i - 12);
    }
    for (int i = 40; i <= 41; i++) {          /* post at 80..82 degrees, 35 cm */
        cm[i] = 35.0f;
    }
    ok[60] = false;                           /* a lost echo is skipped */
    for (int i = 70; i <= 72; i++) {          /* two objects side by side, 40 cm apart in range */
        cm[i] = 50.0f;
    }
    for (int i = 73; i <= 75; i++) {
        cm[i] = 90.0f;
    }

    object_t obj[RADAR_MAX_OBJECTS];
    const size_t n = radar_find_objects(cm, ok, RADAR_POINTS, 150.0f, 15.0f, obj, RADAR_MAX_OBJECTS);
    CHECK(n == 4);
    CHECK(obj[0].start_deg == 20 && obj[0].end_deg == 28 && NEAR(obj[0].bearing_deg, 24, 0) &&
          NEAR(obj[0].range_cm, 58, 0));
    CHECK(obj[1].start_deg == 80 && obj[1].end_deg == 82 && NEAR(obj[1].range_cm, 35, 0));
    CHECK(obj[2].start_deg == 140 && obj[2].end_deg == 144 && NEAR(obj[2].range_cm, 50, 0));
    CHECK(obj[3].start_deg == 146 && obj[3].end_deg == 150 && NEAR(obj[3].range_cm, 90, 0));

    CHECK(radar_find_objects(cm, ok, RADAR_POINTS, 150.0f, 15.0f, obj, 2) == 2);   /* capacity */
}

static void test_json(void)
{
    scan_t s;
    scan_init(&s);
    s.us_cm[0] = 120.4f;
    s.us_ok[0] = true;
    s.ir_cm[1] = 33.6f;
    s.ir_ok[1] = true;
    s.sweeps = 7;
    const object_t o = {20, 28, 24.0f, 58.0f};
    static char out[4096];
    const size_t n = radar_json(&s, 46, 21.5f, &o, 1, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strncmp(out, "{\"angle\":46,\"step\":2,\"temp\":21.5,\"sweeps\":7,\"us\":[120,-1,-1,", 58) == 0);
    CHECK(strstr(out, "\"ir\":[-1,34,-1,") != NULL);
    CHECK(strstr(out, "\"objects\":[{\"from\":20,\"to\":28,\"bearing\":24,\"range\":58}]}") != NULL);
    char small[100];
    CHECK(radar_json(&s, 0, 20, &o, 1, small, sizeof(small)) == 0 && small[0] == '\0');
}

int main(void)
{
    test_sweep();
    test_servo();
    test_ranging();
    test_objects();
    test_json();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
