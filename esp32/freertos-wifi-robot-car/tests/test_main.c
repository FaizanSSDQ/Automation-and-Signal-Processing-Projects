/**
 * Host tests for the robot car's portable core: drive mixing and PWM mapping,
 * the autopilot modes and safety rules, sensor conversions and the web
 * command/telemetry format.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../robot_car/src/core/autopilot.h"
#include "../robot_car/src/core/drive.h"
#include "../robot_car/src/core/sensing.h"
#include "../robot_car/src/core/telemetry.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)
#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 1e-4f)

/* ---- drive ------------------------------------------------------------------------------ */

static void test_mixing(void)
{
    wheels_t w = drive_mix((drive_cmd_t){1.0f, 0.0f});
    CHECK(NEAR(w.left, 1) && NEAR(w.right, 1));             /* straight ahead */
    w = drive_mix((drive_cmd_t){0.0f, 1.0f});
    CHECK(NEAR(w.left, 1) && NEAR(w.right, -1));            /* spin right on the spot */
    w = drive_mix((drive_cmd_t){1.0f, 1.0f});
    CHECK(NEAR(w.left, 1) && NEAR(w.right, 0));             /* 2 : 0 scaled to 1 : 0 */
    w = drive_mix((drive_cmd_t){0.5f, 0.25f});
    CHECK(NEAR(w.left, 0.75f) && NEAR(w.right, 0.25f));     /* gentle right curve */
    w = drive_mix((drive_cmd_t){-3.0f, 0.0f});
    CHECK(NEAR(w.left, -1) && NEAR(w.right, -1));           /* inputs clamped */
}

static void test_ramp_duty_dir(void)
{
    CHECK(NEAR(drive_ramp(0.0f, 1.0f, 0.08f), 0.08f));
    CHECK(NEAR(drive_ramp(0.5f, 0.0f, 0.08f), 0.42f));
    CHECK(NEAR(drive_ramp(0.95f, 1.0f, 0.08f), 1.0f));      /* never overshoots */
    float v = 0.0f;
    int steps = 0;
    while (v < 1.0f && steps < 100) {
        v = drive_ramp(v, 1.0f, 4.0f / 50.0f);
        steps++;
    }
    CHECK(steps == 13);                                     /* 0 -> full in ~250 ms at 50 Hz */

    CHECK(drive_duty(0.0f, 1023, 0.35f, 0.03f) == 0);
    CHECK(drive_duty(0.02f, 1023, 0.35f, 0.03f) == 0);      /* inside the deadband */
    CHECK(drive_duty(1.0f, 1023, 0.35f, 0.03f) == 1023);
    CHECK(drive_duty(-0.5f, 1023, 0.35f, 0.03f) == 691);    /* (0.35 + 0.65 * 0.5) * 1023 */
    CHECK(drive_duty(0.03f, 1023, 0.35f, 0.03f) >= 358);    /* smallest speed still starts the motor */
    CHECK(drive_dir(0.5f, 0.03f) == 1 && drive_dir(-0.5f, 0.03f) == -1 && drive_dir(0.01f, 0.03f) == 0);
}

/* ---- autopilot -------------------------------------------------------------------------- */

static sensors_t clear_road(void)
{
    sensors_t s = {200.0f, true, false, false, 7.6f};
    return s;
}

static void test_manual_and_safety(void)
{
    autopilot_cfg_t cfg;
    autopilot_defaults(&cfg);
    autopilot_t ap;
    autopilot_init(&ap, &cfg);
    sensors_t s = clear_road();
    const drive_cmd_t joy = {0.8f, -0.2f};

    drive_cmd_t out = autopilot_step(&ap, &s, joy, 1000, 900);
    CHECK(NEAR(out.throttle, 0.8f) && NEAR(out.steer, -0.2f) && ap.stop == STOP_NONE);

    out = autopilot_step(&ap, &s, joy, 1400, 900);           /* exactly at the timeout: still fine */
    CHECK(NEAR(out.throttle, 0.8f));
    out = autopilot_step(&ap, &s, joy, 1401, 900);           /* link lost */
    CHECK(NEAR(out.throttle, 0) && NEAR(out.steer, 0) && ap.stop == STOP_LINK);
    out = autopilot_step(&ap, &s, joy, 5, 0xFFFFFFF0u);      /* millis() wrap still works */
    CHECK(ap.stop == STOP_NONE);

    s.battery_v = 6.3f;
    out = autopilot_step(&ap, &s, joy, 1000, 990);
    CHECK(NEAR(out.throttle, 0) && ap.stop == STOP_BATTERY);
    s.battery_v = 0.0f;                                      /* no battery reading: not a stop */
    out = autopilot_step(&ap, &s, joy, 1000, 990);
    CHECK(ap.stop == STOP_NONE);
}

static void test_obstacle_guard(void)
{
    autopilot_cfg_t cfg;
    autopilot_defaults(&cfg);
    autopilot_t ap;
    autopilot_init(&ap, &cfg);
    sensors_t s = clear_road();
    const drive_cmd_t fwd = {1.0f, 0.0f}, back = {-1.0f, 0.0f};

    s.distance_cm = 37.5f;                                   /* half way between 15 and 60 cm */
    CHECK(NEAR(autopilot_speed_limit(&cfg, &s), 0.5f));
    CHECK(NEAR(autopilot_step(&ap, &s, fwd, 10, 10).throttle, 0.5f));

    s.distance_cm = 12.0f;
    CHECK(NEAR(autopilot_step(&ap, &s, fwd, 10, 10).throttle, 0.0f) && ap.stop == STOP_OBSTACLE);
    CHECK(NEAR(autopilot_step(&ap, &s, back, 10, 10).throttle, -1.0f));   /* can always back away */

    s.distance_ok = false;                                   /* no echo: treated as a clear road */
    CHECK(NEAR(autopilot_step(&ap, &s, fwd, 10, 10).throttle, 1.0f));
}

static void test_avoidance_sequence(void)
{
    autopilot_cfg_t cfg;
    autopilot_defaults(&cfg);
    autopilot_t ap;
    autopilot_init(&ap, &cfg);
    autopilot_set_mode(&ap, MODE_AVOID);
    sensors_t s = clear_road();
    const drive_cmd_t idle = {0, 0};
    uint32_t t = 1000;

    drive_cmd_t out = autopilot_step(&ap, &s, idle, t, t);
    CHECK(NEAR(out.throttle, cfg.cruise) && ap.av_state == AV_CRUISE);

    s.distance_cm = 22.0f;                                   /* within stop + 10 cm: back up */
    out = autopilot_step(&ap, &s, idle, t, t);
    CHECK(out.throttle < 0 && ap.av_state == AV_BACKUP);
    out = autopilot_step(&ap, &s, idle, t + 399, t + 399);
    CHECK(out.throttle < 0 && ap.av_state == AV_BACKUP);
    out = autopilot_step(&ap, &s, idle, t + 400, t + 400);   /* then turn on the spot */
    CHECK(NEAR(out.throttle, 0) && out.steer > 0 && ap.av_state == AV_TURN);
    s.distance_cm = 200.0f;
    out = autopilot_step(&ap, &s, idle, t + 850, t + 850);   /* then cruise again */
    CHECK(ap.av_state == AV_CRUISE && NEAR(out.throttle, cfg.cruise));
    CHECK(ap.av_turn_dir == -1);                             /* next turn goes the other way */

    autopilot_set_mode(&ap, MODE_MANUAL);
    autopilot_set_mode(&ap, MODE_AVOID);
    CHECK(ap.av_state == AV_CRUISE);
}

static void test_line_following(void)
{
    autopilot_cfg_t cfg;
    autopilot_defaults(&cfg);
    autopilot_t ap;
    autopilot_init(&ap, &cfg);
    autopilot_set_mode(&ap, MODE_LINE);
    sensors_t s = clear_road();
    const drive_cmd_t idle = {0, 0};

    drive_cmd_t out = autopilot_step(&ap, &s, idle, 10, 10);
    CHECK(NEAR(out.throttle, cfg.cruise) && NEAR(out.steer, 0));
    s.ir_left = true;
    out = autopilot_step(&ap, &s, idle, 10, 10);
    CHECK(out.steer < 0 && out.throttle > 0);                /* line drifted left: steer left */
    s.ir_left = false;
    s.ir_right = true;
    out = autopilot_step(&ap, &s, idle, 10, 10);
    CHECK(out.steer > 0);
    s.ir_left = true;
    out = autopilot_step(&ap, &s, idle, 10, 10);
    CHECK(NEAR(out.throttle, 0) && ap.stop == STOP_LINE_END);
}

static void test_names(void)
{
    CHECK(strcmp(autopilot_mode_name(MODE_AVOID), "avoid") == 0);
    CHECK(autopilot_mode_from_name("line") == MODE_LINE);
    CHECK(autopilot_mode_from_name("turbo") == MODE_COUNT);
    CHECK(strcmp(autopilot_stop_name(STOP_LINK), "link lost") == 0);
    CHECK(strcmp(autopilot_stop_name(STOP_NONE), "") == 0);
}

/* ---- sensing ---------------------------------------------------------------------------- */

static void test_sensing(void)
{
    bool ok;
    CHECK(fabsf(sense_ultrasonic_cm(1166, &ok) - 20.0f) < 0.01f && ok);
    sense_ultrasonic_cm(0, &ok);
    CHECK(!ok);                                              /* timeout: no echo */
    sense_ultrasonic_cm(30000, &ok);
    CHECK(!ok);                                              /* 514 cm: beyond range */

    CHECK(fabsf(sense_sharp_cm(1.0f, &ok) - 27.73f) < 0.01f && ok);
    CHECK(sense_sharp_cm(2.5f, &ok) < 10.0f && !ok);         /* closer than 10 cm: ambiguous */
    sense_sharp_cm(0.3f, &ok);
    CHECK(!ok);                                              /* beyond 80 cm */
    sense_sharp_cm(0.0f, &ok);
    CHECK(!ok);

    const float v[5] = {5, 1, 4, 100, 3};
    CHECK(NEAR(sense_median5(v), 4));                        /* a single spike is rejected */
    CHECK(NEAR(sense_battery_v(2500, 3.0f), 7.5f));

    CHECK(NEAR(sense_fuse(40, true, 25, true, &ok), 25) && ok);   /* nearer one wins */
    CHECK(NEAR(sense_fuse(40, false, 25, true, &ok), 25) && ok);
    CHECK(NEAR(sense_fuse(40, true, 25, false, &ok), 40) && ok);
    sense_fuse(40, false, 25, false, &ok);
    CHECK(!ok);
}

/* ---- web format ------------------------------------------------------------------------- */

static void test_web_format(void)
{
    float v;
    CHECK(telemetry_parse_axis("50", &v) && NEAR(v, 0.5f));
    CHECK(telemetry_parse_axis("-100", &v) && NEAR(v, -1.0f));
    CHECK(!telemetry_parse_axis("101", &v));
    CHECK(!telemetry_parse_axis("", &v) && !telemetry_parse_axis(NULL, &v) && !telemetry_parse_axis("5a", &v));
    CHECK(telemetry_parse_limit("70", &v) && NEAR(v, 0.7f));
    CHECK(!telemetry_parse_limit("5", &v));

    telemetry_t t;
    memset(&t, 0, sizeof(t));
    t.mode = MODE_AVOID;
    t.stop = STOP_OBSTACLE;
    t.left = 0.42f;
    t.right = -0.405f;
    t.sensors = (sensors_t){87.5f, true, false, true, 7.62f};
    t.rssi = -48;
    t.uptime_s = 123;
    char out[256];
    const size_t n = telemetry_json(&t, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strcmp(out, "{\"mode\":\"avoid\",\"stop\":\"obstacle\",\"l\":42,\"r\":-41,\"d\":87.5,\"dok\":1,"
                      "\"irl\":0,\"irr\":1,\"bat\":7.62,\"rssi\":-48,\"up\":123}") == 0);
    char small[20];
    CHECK(telemetry_json(&t, small, sizeof(small)) == 0 && small[0] == '\0');
}

int main(void)
{
    test_mixing();
    test_ramp_duty_dir();
    test_manual_and_safety();
    test_obstacle_guard();
    test_avoidance_sequence();
    test_line_following();
    test_names();
    test_sensing();
    test_web_format();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
