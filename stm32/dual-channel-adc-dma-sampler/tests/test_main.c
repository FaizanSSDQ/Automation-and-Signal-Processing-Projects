/**
 * Host unit tests for the hardware-independent modules (signal_stats, frame).
 * Build & run:  make test        (uses the host C compiler, not the ARM toolchain)
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "frame.h"
#include "signal_stats.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures;
static int checks;

#define CHECK(cond)                                                              \
    do {                                                                         \
        checks++;                                                                \
        if (!(cond)) {                                                           \
            failures++;                                                          \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        checks++;                                                                \
        long long va_ = (long long)(a), vb_ = (long long)(b);                    \
        if (va_ != vb_) {                                                        \
            failures++;                                                          \
            printf("  FAIL %s:%d  %s == %s  (%lld != %lld)\n", __FILE__, __LINE__, \
                   #a, #b, va_, vb_);                                            \
        }                                                                        \
    } while (0)

/* ---- CRC & framing ------------------------------------------------------------------------ */

static void test_crc_check_value(void)
{
    const char *msg = "123456789"; /* standard check string */
    CHECK_EQ(crc16_ccitt((const uint8_t *)msg, strlen(msg)), 0x29B1);
    CHECK_EQ(crc16_ccitt(NULL, 0), 0xFFFF);
}

static void test_frame_layout(void)
{
    const uint8_t payload[3] = {0x10, 0x20, 0x30};
    uint8_t out[32];
    const size_t n = frame_encode(FRAME_TYPE_STATS, 0x1234, payload, 3, out, sizeof(out));

    CHECK_EQ(n, FRAME_OVERHEAD + 3);
    CHECK_EQ(out[0], 0xA5);
    CHECK_EQ(out[1], 0x5A);
    CHECK_EQ(out[2], FRAME_TYPE_STATS);
    CHECK_EQ(out[3], 0x34); /* seq little-endian */
    CHECK_EQ(out[4], 0x12);
    CHECK_EQ(out[5], 3); /* len */
    CHECK_EQ(out[6], 0);
    CHECK(memcmp(&out[7], payload, 3) == 0);

    const uint16_t crc = crc16_ccitt(&out[2], 5 + 3);
    CHECK_EQ(out[10], crc & 0xFF);
    CHECK_EQ(out[11], crc >> 8);
}

static void test_frame_rejects_small_buffer(void)
{
    const uint8_t payload[8] = {0};
    uint8_t out[10];
    CHECK_EQ(frame_encode(FRAME_TYPE_SAMPLES, 0, payload, 8, out, sizeof(out)), 0);
    CHECK_EQ(frame_encode(FRAME_TYPE_SAMPLES, 0, NULL, 4, out, sizeof(out)), 0);
    CHECK_EQ(frame_encode(FRAME_TYPE_SAMPLES, 0, NULL, 0, out, sizeof(out)), FRAME_OVERHEAD);
}

static void test_put_helpers(void)
{
    uint8_t buf[6];
    uint8_t *p = put_u16(buf, 0xBEEF);
    p = put_u32(p, 0x01020304u);
    CHECK_EQ(p - buf, 6);
    CHECK_EQ(buf[0], 0xEF);
    CHECK_EQ(buf[1], 0xBE);
    CHECK_EQ(buf[2], 0x04);
    CHECK_EQ(buf[5], 0x01);
}

/* ---- Statistics --------------------------------------------------------------------------- */

static void test_isqrt(void)
{
    CHECK_EQ(isqrt64(0), 0);
    CHECK_EQ(isqrt64(1), 1);
    CHECK_EQ(isqrt64(15), 3);
    CHECK_EQ(isqrt64(16), 4);
    CHECK_EQ(isqrt64(4294967296ull), 65536);
    CHECK_EQ(isqrt64(18446744073709551615ull), 4294967295u);
}

static void test_constant_signal(void)
{
    uint16_t data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = 1234;
    }
    channel_stats_t s;
    stats_compute(data, 64, 1, 0, &s);
    CHECK_EQ(s.min, 1234);
    CHECK_EQ(s.max, 1234);
    CHECK_EQ(s.mean, 1234);
    CHECK_EQ(s.ac_rms, 0);
}

static void test_square_wave(void)
{
    uint16_t data[100];
    for (int i = 0; i < 100; i++) {
        data[i] = (i % 2) ? 3000 : 1000;
    }
    channel_stats_t s;
    stats_compute(data, 100, 1, 0, &s);
    CHECK_EQ(s.mean, 2000);
    CHECK_EQ(s.ac_rms, 1000); /* square wave: RMS equals amplitude */
}

static void test_interleaved_channels(void)
{
    /* ch0 = full-scale 12-bit square wave, ch1 = sine of amplitude 1000 around mid-scale */
    enum { FRAMES = 1000 };
    uint16_t data[FRAMES * 2];
    for (int i = 0; i < FRAMES; i++) {
        data[2 * i] = (i % 2) ? 4095 : 0;
        data[2 * i + 1] = (uint16_t)lround(2048.0 + 1000.0 * sin(2.0 * M_PI * 10.0 * i / FRAMES));
    }
    channel_stats_t s0, s1;
    stats_compute(data, FRAMES, 2, 0, &s0);
    stats_compute(data, FRAMES, 2, 1, &s1);

    CHECK_EQ(s0.min, 0);
    CHECK_EQ(s0.max, 4095);
    CHECK_EQ(s0.mean, 2048); /* 2047.5 rounds up */

    CHECK_EQ(s1.mean, 2048);
    CHECK(s1.ac_rms >= 706 && s1.ac_rms <= 708); /* 1000 / sqrt(2) = 707.1 */
    CHECK(s1.min <= 1049 && s1.max >= 3047);
}

static void test_invalid_arguments(void)
{
    uint16_t data[4] = {1, 2, 3, 4};
    channel_stats_t s = {9, 9, 9, 9};
    stats_compute(data, 0, 1, 0, &s);
    CHECK_EQ(s.max, 0);
    s.max = 9;
    stats_compute(data, 2, 2, 2, &s); /* channel out of range */
    CHECK_EQ(s.max, 0);
}

int main(void)
{
    test_crc_check_value();
    test_frame_layout();
    test_frame_rejects_small_buffer();
    test_put_helpers();
    test_isqrt();
    test_constant_signal();
    test_square_wave();
    test_interleaved_channels();
    test_invalid_arguments();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
