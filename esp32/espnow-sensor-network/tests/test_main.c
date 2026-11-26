/**
 * Host tests for the sensor network's portable core: the radio packet, DHT22
 * decoding, BMP280 compensation (datasheet example) and the gateway's node
 * table with loss counting and JSON output.
 * Build & run:  make test
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <string.h>

#include "../sensor_network/src/core/node_table.h"
#include "../sensor_network/src/core/packet.h"
#include "../sensor_network/src/core/sensor_math.h"

static int failures, checks;
#define CHECK(c)                                                                           \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(c)) {                                                                        \
            failures++;                                                                    \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);                          \
        }                                                                                  \
    } while (0)

static reading_t sample(uint8_t id, uint16_t seq)
{
    reading_t r;
    memset(&r, 0, sizeof(r));
    r.node_id = id;
    r.flags = PKT_F_DHT_OK | PKT_F_BMP_OK;
    r.seq = seq;
    r.temp_c100 = -1015;           /* -10.15 degC */
    r.humidity_x100 = 6520;
    r.pressure_pa = 100653;
    r.bmp_temp_c100 = 2508;
    r.battery_mv = 3915;
    r.alarms = ALARM_FLAME | ALARM_MOTION;
    r.wake = WAKE_ALARM;
    return r;
}

/* ---- packet ----------------------------------------------------------------------------- */

static void test_crc(void)
{
    CHECK(pkt_crc16((const uint8_t *)"123456789", 9) == 0x29B1);   /* CRC-16/CCITT-FALSE check value */
}

static void test_packet_round_trip(void)
{
    const reading_t in = sample(3, 0xBEEF);
    uint8_t buf[PKT_LEN];
    pkt_encode(&in, buf);
    CHECK(buf[0] == PKT_MAGIC && buf[2] == 3 && buf[4] == 0xEF && buf[5] == 0xBE);   /* little-endian */

    reading_t out;
    memset(&out, 0, sizeof(out));   /* compare without padding bytes */
    CHECK(pkt_decode(buf, PKT_LEN, &out) == PKT_OK);
    CHECK(memcmp(&in, &out, sizeof(in)) == 0);
    CHECK(out.temp_c100 == -1015 && out.pressure_pa == 100653);

    CHECK(pkt_decode(buf, PKT_LEN - 1, &out) == PKT_ERR_LENGTH);
    uint8_t bad[PKT_LEN];
    memcpy(bad, buf, PKT_LEN);
    bad[0] = 0;
    CHECK(pkt_decode(bad, PKT_LEN, &out) == PKT_ERR_MAGIC);
    memcpy(bad, buf, PKT_LEN);
    bad[1] = 9;
    CHECK(pkt_decode(bad, PKT_LEN, &out) == PKT_ERR_VERSION);
    /* Every single-bit error in the payload is caught by the CRC. */
    int caught = 0;
    for (int bit = 16; bit < (int)(PKT_LEN - 2) * 8; bit++) {
        memcpy(bad, buf, PKT_LEN);
        bad[bit / 8] ^= (uint8_t)(1u << (bit % 8));
        caught += pkt_decode(bad, PKT_LEN, &out) == PKT_ERR_CRC;
    }
    CHECK(caught == (int)(PKT_LEN - 2) * 8 - 16);
    CHECK(strcmp(pkt_status_name(PKT_ERR_CRC), "crc") == 0);
}

/* ---- DHT22 ------------------------------------------------------------------------------ */

static void bits_for(const uint8_t bytes[5], uint8_t us[40])
{
    for (int i = 0; i < 40; i++) {
        us[i] = (bytes[i / 8] >> (7 - i % 8)) & 1u ? 70 : 26;   /* nominal pulse widths */
    }
}

static void test_dht22(void)
{
    /* 65.2 %RH, 35.1 degC: 0x028C, 0x015F, checksum 0xEE */
    const uint8_t frame[5] = {0x02, 0x8C, 0x01, 0x5F, 0xEE};
    uint8_t us[40], bytes[5];
    bits_for(frame, us);
    dht_bits_to_bytes(us, bytes);
    CHECK(memcmp(bytes, frame, 5) == 0);

    int16_t t;
    uint16_t h;
    CHECK(dht22_convert(bytes, &t, &h) && t == 3510 && h == 6520);

    const uint8_t neg[5] = {0x01, 0x90, 0x80, 0x65, 0x76};      /* 40.0 %RH, -10.1 degC */
    CHECK(dht22_convert(neg, &t, &h) && t == -1010 && h == 4000);

    uint8_t broken[5] = {0x02, 0x8C, 0x01, 0x5F, 0xEF};
    CHECK(!dht22_convert(broken, &t, &h));                       /* checksum error */
    const uint8_t over[5] = {0x03, 0xE9, 0x00, 0x00, 0xEC};      /* 100.1 %: rejected */
    CHECK(!dht22_convert(over, &t, &h));
}

/* ---- BMP280 ----------------------------------------------------------------------------- */

static void test_bmp280_datasheet_example(void)
{
    /* Bosch BMP280 datasheet, section 8.2: calibration and raw values of the example. */
    const int16_t cal[12] = {27504, 26435, -1000, (int16_t)36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000};
    uint8_t raw[24];
    for (int i = 0; i < 12; i++) {
        raw[2 * i] = (uint8_t)((uint16_t)cal[i] & 0xFF);
        raw[2 * i + 1] = (uint8_t)((uint16_t)cal[i] >> 8);
    }
    bmp280_calib_t c;
    bmp280_parse_calib(raw, &c);
    CHECK(c.T1 == 27504 && c.P1 == 36477 && c.P9 == 6000 && c.T3 == -1000);

    int32_t t;
    uint32_t p;
    CHECK(bmp280_compensate(&c, 519888, 415148, &t, &p));
    CHECK(t == 2508);           /* 25.08 degC */
    CHECK(p == 100653);         /* 100653.27 Pa */

    CHECK(bmp280_raw20(0x65, 0x5A, 0xC0) == 415148);   /* 0x655AC >> 0, xlsb upper nibble */
    bmp280_calib_t zero;
    memset(&zero, 0, sizeof(zero));
    CHECK(!bmp280_compensate(&zero, 519888, 415148, &t, &p));   /* P1 = 0: division guard */
}

static void test_battery(void)
{
    CHECK(battery_mv(1950, 200) == 3900);       /* 1:1 divider (ratio 2.00) */
    CHECK(battery_mv(1000, 133) == 1330);
}

/* ---- node table ------------------------------------------------------------------------- */

static const uint8_t MAC_A[6] = {0x24, 0x6F, 0x28, 0x01, 0x02, 0x03};
static const uint8_t MAC_B[6] = {0x24, 0x6F, 0x28, 0x0A, 0x0B, 0x0C};

static void test_node_table_loss_and_duplicates(void)
{
    node_table_t t;
    nt_init(&t);
    reading_t r = sample(1, 100);
    CHECK(nt_update(&t, MAC_A, &r, -60, 1000) == NT_NEW);
    r.seq = 101;
    CHECK(nt_update(&t, MAC_A, &r, -64, 2000) == NT_UPDATED);
    r.seq = 101;
    CHECK(nt_update(&t, MAC_A, &r, -64, 2100) == NT_DUPLICATE);
    r.seq = 105;                                               /* 102..104 lost */
    nt_update(&t, MAC_A, &r, -64, 3000);
    const node_entry_t *e = &t.node[0];
    CHECK(e->received == 3 && e->lost == 3 && e->duplicates == 1);
    CHECK(nt_loss_permille(e) == 500);

    r.seq = 1;                                                 /* node rebooted */
    r.wake = WAKE_POWER_ON;
    nt_update(&t, MAC_A, &r, -64, 4000);
    CHECK(e->lost == 3);
    r.seq = 0xFFFF;
    r.wake = WAKE_TIMER;
    nt_update(&t, MAC_A, &r, -64, 5000);                       /* huge jump: treated as restart */
    CHECK(e->lost == 3);
    r.seq = 0x0001;                                            /* 0xFFFF -> 0x0001: wraps, 1 lost */
    nt_update(&t, MAC_A, &r, -64, 6000);
    CHECK(e->lost == 4);

    CHECK(e->rssi_avg_x4 / 4 >= -64 && e->rssi_avg_x4 / 4 <= -60);
    CHECK(nt_online(e, 6000 + 30000, 30000) && !nt_online(e, 6000 + 30001, 30000));
}

static void test_node_table_capacity(void)
{
    node_table_t t;
    nt_init(&t);
    uint8_t mac[6] = {1, 2, 3, 4, 5, 0};
    reading_t r = sample(1, 1);
    for (int i = 0; i < NT_MAX_NODES; i++) {
        mac[5] = (uint8_t)i;
        CHECK(nt_update(&t, mac, &r, -50, 0) == NT_NEW);
    }
    mac[5] = 99;
    CHECK(nt_update(&t, mac, &r, -50, 0) == NT_FULL);
}

static void test_node_table_json(void)
{
    node_table_t t;
    nt_init(&t);
    reading_t r = sample(2, 7);
    nt_update(&t, MAC_B, &r, -58, 1000);
    t.bad_frames = 2;
    char out[600];
    const size_t n = nt_json(&t, 5500, 30000, out, sizeof(out));
    CHECK(n == strlen(out));
    CHECK(strcmp(out,
                 "{\"nodes\":[{\"id\":2,\"mac\":\"24:6F:28:0A:0B:0C\",\"dht\":1,\"bmp\":1,\"t\":-10.15,\"h\":65.20,"
                 "\"p\":100653,\"bt\":25.08,\"bat\":3915,\"alarms\":5,\"wake\":1,\"seq\":7,\"rssi\":-58,\"rx\":1,"
                 "\"lost\":0,\"loss\":0.0,\"age\":4,\"online\":true}],\"bad_frames\":2}") == 0);

    node_table_t empty;
    nt_init(&empty);
    CHECK(nt_json(&empty, 0, 1000, out, sizeof(out)) > 0 && strcmp(out, "{\"nodes\":[],\"bad_frames\":0}") == 0);
    char small[40];
    CHECK(nt_json(&t, 0, 1000, small, sizeof(small)) == 0 && small[0] == '\0');
}

int main(void)
{
    test_crc();
    test_packet_round_trip();
    test_dht22();
    test_bmp280_datasheet_example();
    test_battery();
    test_node_table_loss_and_duplicates();
    test_node_table_capacity();
    test_node_table_json();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
