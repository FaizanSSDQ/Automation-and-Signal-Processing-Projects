/**
 * @file    node_table.c
 * @brief   The gateway's view of the network.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "node_table.h"

#include <stdio.h>
#include <string.h>

void nt_init(node_table_t *t)
{
    memset(t, 0, sizeof(*t));
}

static node_entry_t *find(node_table_t *t, const uint8_t mac[6])
{
    for (int i = 0; i < NT_MAX_NODES; i++) {
        if (t->node[i].used && memcmp(t->node[i].mac, mac, 6) == 0) {
            return &t->node[i];
        }
    }
    return NULL;
}

nt_result_t nt_update(node_table_t *t, const uint8_t mac[6], const reading_t *r, int rssi, uint32_t now_ms)
{
    node_entry_t *e = find(t, mac);
    nt_result_t result = NT_UPDATED;

    if (e == NULL) {
        for (int i = 0; i < NT_MAX_NODES && e == NULL; i++) {
            if (!t->node[i].used) {
                e = &t->node[i];
            }
        }
        if (e == NULL) {
            return NT_FULL;
        }
        memset(e, 0, sizeof(*e));
        e->used = true;
        memcpy(e->mac, mac, 6);
        e->rssi_avg_x4 = (int16_t)(rssi * 4);
        result = NT_NEW;
    } else {
        const uint16_t gap = (uint16_t)(r->seq - e->last.seq);
        if (gap == 0u) {
            e->duplicates++;
            e->last_seen_ms = now_ms;
            return NT_DUPLICATE;
        }
        if (r->wake != WAKE_POWER_ON && gap < NT_MAX_GAP) {
            e->lost += gap - 1u;
        }
        /* RSSI average: avg += (new - avg) / 4, in units of 1/4 dB */
        e->rssi_avg_x4 = (int16_t)(e->rssi_avg_x4 + (rssi * 4 - e->rssi_avg_x4) / 4);
    }
    e->last = *r;
    e->received++;
    e->last_seen_ms = now_ms;
    return result;
}

uint32_t nt_loss_permille(const node_entry_t *e)
{
    const uint32_t total = e->received + e->lost;
    return total ? (e->lost * 1000u + total / 2u) / total : 0u;
}

bool nt_online(const node_entry_t *e, uint32_t now_ms, uint32_t timeout_ms)
{
    return e->used && (now_ms - e->last_seen_ms) <= timeout_ms;
}

static int fixed2(char *out, size_t cap, int32_t v100)
{
    const char *sign = v100 < 0 ? "-" : "";
    const int32_t a = v100 < 0 ? -v100 : v100;
    return snprintf(out, cap, "%s%ld.%02ld", sign, (long)(a / 100), (long)(a % 100));
}

size_t nt_json(const node_table_t *t, uint32_t now_ms, uint32_t timeout_ms, char *out, size_t cap)
{
    size_t len = 0;
    int w = snprintf(out, cap, "{\"nodes\":[");
    if (w < 0 || (size_t)w >= cap) {
        goto overflow;
    }
    len = (size_t)w;
    bool first = true;
    for (int i = 0; i < NT_MAX_NODES; i++) {
        const node_entry_t *e = &t->node[i];
        if (!e->used) {
            continue;
        }
        const reading_t *r = &e->last;
        char temp[16], hum[16], bt[16];
        fixed2(temp, sizeof(temp), r->temp_c100);
        fixed2(hum, sizeof(hum), r->humidity_x100);
        fixed2(bt, sizeof(bt), r->bmp_temp_c100);
        const uint32_t loss = nt_loss_permille(e);
        w = snprintf(out + len, cap - len,
                     "%s{\"id\":%u,\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"dht\":%d,\"bmp\":%d,"
                     "\"t\":%s,\"h\":%s,\"p\":%lu,\"bt\":%s,\"bat\":%u,\"alarms\":%u,\"wake\":%u,"
                     "\"seq\":%u,\"rssi\":%d,\"rx\":%lu,\"lost\":%lu,\"loss\":%lu.%lu,\"age\":%lu,\"online\":%s}",
                     first ? "" : ",", (unsigned)r->node_id, e->mac[0], e->mac[1], e->mac[2], e->mac[3],
                     e->mac[4], e->mac[5], (r->flags & PKT_F_DHT_OK) ? 1 : 0, (r->flags & PKT_F_BMP_OK) ? 1 : 0,
                     temp, hum, (unsigned long)r->pressure_pa, bt, (unsigned)r->battery_mv, (unsigned)r->alarms,
                     (unsigned)r->wake, (unsigned)r->seq, e->rssi_avg_x4 / 4, (unsigned long)e->received,
                     (unsigned long)e->lost, (unsigned long)(loss / 10u), (unsigned long)(loss % 10u),
                     (unsigned long)((now_ms - e->last_seen_ms) / 1000u),
                     nt_online(e, now_ms, timeout_ms) ? "true" : "false");
        if (w < 0 || (size_t)w >= cap - len) {
            goto overflow;
        }
        len += (size_t)w;
        first = false;
    }
    w = snprintf(out + len, cap - len, "],\"bad_frames\":%lu}", (unsigned long)t->bad_frames);
    if (w < 0 || (size_t)w >= cap - len) {
        goto overflow;
    }
    return len + (size_t)w;

overflow:
    if (cap) {
        out[0] = '\0';
    }
    return 0;
}
