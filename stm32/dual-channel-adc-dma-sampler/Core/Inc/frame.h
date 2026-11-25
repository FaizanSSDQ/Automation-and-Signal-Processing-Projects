/**
 * @file    frame.h
 * @brief   Binary framing for the UART telemetry stream.
 *
 * Frame layout (all multi-byte fields little-endian):
 *
 *   offset  size  field
 *   0       1     sync0 = 0xA5
 *   1       1     sync1 = 0x5A
 *   2       1     type  (FRAME_TYPE_*)
 *   3       2     seq   (block counter, wraps at 65535)
 *   5       2     len   (payload length in bytes)
 *   7       len   payload
 *   7+len   2     crc   CRC-16/CCITT-FALSE over bytes 2 .. 6+len
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 * @license MIT
 */
#ifndef FRAME_H
#define FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FRAME_SYNC0 0xA5u
#define FRAME_SYNC1 0x5Au
#define FRAME_HEADER_SIZE 7u
#define FRAME_CRC_SIZE 2u
#define FRAME_OVERHEAD (FRAME_HEADER_SIZE + FRAME_CRC_SIZE)

enum {
    FRAME_TYPE_STATS = 0x01,   /**< per-block statistics (see app_sampler.c) */
    FRAME_TYPE_SAMPLES = 0x02, /**< raw interleaved uint16 samples */
};

/** CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR. */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

/**
 * Encode one frame into @p out.
 * @return total frame length, or 0 if @p out_size is too small.
 */
size_t frame_encode(uint8_t type, uint16_t seq, const uint8_t *payload, uint16_t payload_len,
                    uint8_t *out, size_t out_size);

/** Little-endian helpers used to build payloads without packed structs. */
static inline uint8_t *put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
    return p + 2;
}

static inline uint8_t *put_u32(uint8_t *p, uint32_t v)
{
    p = put_u16(p, (uint16_t)(v & 0xFFFFu));
    return put_u16(p, (uint16_t)(v >> 16));
}

#ifdef __cplusplus
}
#endif

#endif /* FRAME_H */
