/**
 * @file    node_table.h
 * @brief   The gateway's view of the network: one entry per sensor node with its
 *          latest reading, signal strength, packet loss and online state
 *          (no hardware dependency).
 *
 * Packet loss is counted from the sequence numbers: a jump of k means k - 1
 * reports were lost; the same number again is a duplicate (ESP-NOW retry); a
 * report marked "power-on" restarts the count without counting losses.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef NODE_TABLE_H
#define NODE_TABLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "packet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NT_MAX_NODES 8
#define NT_MAX_GAP   1000u   /* larger sequence jumps are treated as a restart */

typedef struct {
    bool used;
    uint8_t mac[6];
    reading_t last;
    int16_t rssi_avg_x4;      /* exponential average of RSSI, scaled by 4 */
    uint32_t received;
    uint32_t lost;
    uint32_t duplicates;
    uint32_t last_seen_ms;
} node_entry_t;

typedef struct {
    node_entry_t node[NT_MAX_NODES];
    uint32_t bad_frames;      /* failed length / magic / version / CRC checks */
} node_table_t;

typedef enum { NT_NEW = 0, NT_UPDATED, NT_DUPLICATE, NT_FULL } nt_result_t;

void nt_init(node_table_t *t);

/** Record a valid report from @p mac. */
nt_result_t nt_update(node_table_t *t, const uint8_t mac[6], const reading_t *r, int rssi, uint32_t now_ms);

/** Loss in permille: lost / (received + lost). */
uint32_t nt_loss_permille(const node_entry_t *e);

bool nt_online(const node_entry_t *e, uint32_t now_ms, uint32_t timeout_ms);

/** JSON for the dashboard. @return characters written, 0 if it does not fit. */
size_t nt_json(const node_table_t *t, uint32_t now_ms, uint32_t timeout_ms, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* NODE_TABLE_H */
