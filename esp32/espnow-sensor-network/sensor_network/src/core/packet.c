/**
 * @file    packet.c
 * @brief   Sensor report packet: encoding, decoding, CRC.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "packet.h"

uint16_t pkt_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;   /* CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF */
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, (uint16_t)v);
    put16(p + 2, (uint16_t)(v >> 16));
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16);
}

void pkt_encode(const reading_t *r, uint8_t out[PKT_LEN])
{
    out[0] = PKT_MAGIC;
    out[1] = PKT_VERSION;
    out[2] = r->node_id;
    out[3] = r->flags;
    put16(&out[4], r->seq);
    put16(&out[6], (uint16_t)r->temp_c100);
    put16(&out[8], r->humidity_x100);
    put32(&out[10], r->pressure_pa);
    put16(&out[14], (uint16_t)r->bmp_temp_c100);
    put16(&out[16], r->battery_mv);
    out[18] = r->alarms;
    out[19] = r->wake;
    put16(&out[20], pkt_crc16(out, PKT_LEN - 2u));
}

pkt_status_t pkt_decode(const uint8_t *d, size_t len, reading_t *r)
{
    if (len != PKT_LEN) {
        return PKT_ERR_LENGTH;
    }
    if (d[0] != PKT_MAGIC) {
        return PKT_ERR_MAGIC;
    }
    if (d[1] != PKT_VERSION) {
        return PKT_ERR_VERSION;
    }
    if (get16(&d[20]) != pkt_crc16(d, PKT_LEN - 2u)) {
        return PKT_ERR_CRC;
    }
    r->node_id = d[2];
    r->flags = d[3];
    r->seq = get16(&d[4]);
    r->temp_c100 = (int16_t)get16(&d[6]);
    r->humidity_x100 = get16(&d[8]);
    r->pressure_pa = get32(&d[10]);
    r->bmp_temp_c100 = (int16_t)get16(&d[14]);
    r->battery_mv = get16(&d[16]);
    r->alarms = d[18];
    r->wake = d[19];
    return PKT_OK;
}

const char *pkt_status_name(pkt_status_t s)
{
    switch (s) {
    case PKT_OK:          return "ok";
    case PKT_ERR_LENGTH:  return "length";
    case PKT_ERR_MAGIC:   return "magic";
    case PKT_ERR_VERSION: return "version";
    case PKT_ERR_CRC:     return "crc";
    default:              return "?";
    }
}
