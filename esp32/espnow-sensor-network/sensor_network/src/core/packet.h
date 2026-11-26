/**
 * @file    packet.h
 * @brief   Sensor report packet sent from a node to the gateway over ESP-NOW
 *          (no hardware dependency).
 *
 * 22 bytes, little-endian, with a CRC-16/CCITT-FALSE over bytes 0..19:
 *
 *   0 magic 0x5A   1 version   2 node id   3 flags
 *   4..5   sequence number (counts up across deep sleep, restarts at power-on)
 *   6..7   temperature, DHT22, 0.01 degC (signed)
 *   8..9   relative humidity, 0.01 %
 *  10..13  pressure, BMP280, Pa
 *  14..15  temperature, BMP280, 0.01 degC (signed)
 *  16..17  battery, mV
 *  18      alarm inputs (bit 0 flame, bit 1 vibration, bit 2 motion)
 *  19      wake-up reason (0 timer, 1 alarm, 2 power-on)
 *  20..21  CRC-16
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef PACKET_H
#define PACKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKT_MAGIC    0x5Au
#define PKT_VERSION  1u
#define PKT_LEN      22u

/* flags */
#define PKT_F_DHT_OK  0x01u
#define PKT_F_BMP_OK  0x02u

/* alarm bits */
#define ALARM_FLAME      0x01u
#define ALARM_VIBRATION  0x02u
#define ALARM_MOTION     0x04u

typedef enum { WAKE_TIMER = 0, WAKE_ALARM = 1, WAKE_POWER_ON = 2 } wake_reason_t;

typedef struct {
    uint8_t node_id;
    uint8_t flags;
    uint16_t seq;
    int16_t temp_c100;       /* DHT22 */
    uint16_t humidity_x100;
    uint32_t pressure_pa;    /* BMP280 */
    int16_t bmp_temp_c100;
    uint16_t battery_mv;
    uint8_t alarms;
    uint8_t wake;
} reading_t;

typedef enum { PKT_OK = 0, PKT_ERR_LENGTH, PKT_ERR_MAGIC, PKT_ERR_VERSION, PKT_ERR_CRC } pkt_status_t;

uint16_t pkt_crc16(const uint8_t *data, size_t len);

/** Serialise into exactly PKT_LEN bytes. */
void pkt_encode(const reading_t *r, uint8_t out[PKT_LEN]);

/** Validate and parse a received frame. */
pkt_status_t pkt_decode(const uint8_t *data, size_t len, reading_t *r);

const char *pkt_status_name(pkt_status_t s);

#ifdef __cplusplus
}
#endif

#endif /* PACKET_H */
