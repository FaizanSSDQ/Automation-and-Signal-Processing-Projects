/**
 * @file    frame.c
 * @brief   Binary frame encoder with CRC-16/CCITT-FALSE.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#include "frame.h"

#include <string.h>

uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

size_t frame_encode(uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t payload_len,
                    uint8_t *out, size_t out_size)
{
    const size_t total = (size_t)FRAME_OVERHEAD + payload_len;
    if (out == NULL || out_size < total || (payload_len > 0 && payload == NULL)) {
        return 0;
    }

    uint8_t *p = out;
    *p++ = FRAME_SYNC0;
    *p++ = FRAME_SYNC1;
    *p++ = type;
    p = put_u16(p, seq);
    p = put_u16(p, payload_len);
    if (payload_len > 0) {
        memcpy(p, payload, payload_len);
        p += payload_len;
    }

    /* CRC covers type, seq, len and payload (not the sync bytes). */
    const uint16_t crc = crc16_ccitt(out + 2, (size_t)(p - (out + 2)));
    (void)put_u16(p, crc);
    return total;
}
