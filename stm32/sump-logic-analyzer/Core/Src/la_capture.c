/**
 * @file    la_capture.c
 * @brief   Pre-trigger capture engine on top of a circular DMA sample ring.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "la_capture.h"

bool la_capture_start(la_capture_t *c, uint32_t ring_len, uint32_t depth,
                      uint32_t pre, const la_trigger_t *trig)
{
    c->state = LA_IDLE;
    if (ring_len == 0u || depth == 0u || depth >= ring_len || pre > depth) {
        return false;
    }
    c->ring_len = ring_len;
    c->depth = depth;
    c->pre = pre;
    c->trig = *trig;
    c->scanned = 0u;
    c->scan_idx = 0u;
    c->prev = 0u;
    c->have_prev = false;
    c->trig_at = 0u;
    c->end = 0u;
    c->state = LA_PRE_TRIGGER;
    return true;
}

static void fire(la_capture_t *c, uint64_t at)
{
    c->trig_at = at;
    c->end = at + (c->depth - c->pre);
    c->state = LA_POST_TRIGGER;
}

/* Scan [scanned, written) for the trigger. Kept tight: at 4 MS/s the CPU has
 * 16 clock cycles per sample, shared with the DMA. The ring is scanned in
 * contiguous segments so the inner loop is a plain pointer walk with no
 * wrap-around test and no 64-bit arithmetic. The caller guarantees
 * written - scanned <= ring_len. */
static void scan(la_capture_t *c, const uint8_t *ring, uint64_t written)
{
    const la_trigger_t t = c->trig;
    const uint32_t len = c->ring_len;
    uint32_t todo = (uint32_t)(written - c->scanned);
    uint32_t idx = c->scan_idx;

    if (todo == 0u) {
        return;
    }
    if (!c->have_prev) {
        /* First sample of the capture has no predecessor: no edge on it. */
        c->prev = ring[idx];
        c->have_prev = true;
    }

    uint8_t prev = c->prev;
    while (todo > 0u) {
        const uint32_t seg = (todo < len - idx) ? todo : len - idx;
        const uint8_t *p = &ring[idx];
        const uint8_t *const end = p + seg;

        while (p != end) {
            const uint8_t cur = *p;
            if (la_trigger_match(&t, prev, cur)) {
                break;
            }
            prev = cur;
            p++;
        }

        const uint32_t used = (uint32_t)(p - &ring[idx]);
        c->scanned += used;
        if (p != end) {
            /* Trigger on this sample. */
            c->scan_idx = idx + used;
            c->prev = prev;
            fire(c, c->scanned);
            return;
        }
        todo -= seg;
        idx += seg;
        if (idx == len) {
            idx = 0u;
        }
    }
    c->prev = prev;
    c->scan_idx = idx;
}

la_state_t la_capture_poll(la_capture_t *c, const uint8_t *ring, uint64_t written)
{
    if (c->state == LA_PRE_TRIGGER) {
        if (written < c->pre) {
            return c->state;
        }
        if (la_trigger_is_immediate(&c->trig)) {
            fire(c, c->pre);
        } else {
            /* The trigger may only fire once 'pre' samples exist before it.
             * Skip straight there; the sample before it seeds edge detection. */
            if (c->pre > 0u && written - c->pre >= c->ring_len) {
                /* The seed sample (pre - 1) is already overwritten. */
                c->state = LA_OVERRUN;
                return c->state;
            }
            c->scanned = c->pre;
            c->scan_idx = (uint32_t)(c->pre % c->ring_len);
            if (c->pre > 0u) {
                c->prev = ring[(c->pre - 1u) % c->ring_len];
                c->have_prev = true;
            }
            c->state = LA_WAIT_TRIGGER;
        }
    }

    if (c->state == LA_WAIT_TRIGGER) {
        if (written - c->scanned > c->ring_len) {
            /* Unscanned samples were already overwritten by the DMA. */
            c->state = LA_OVERRUN;
            return c->state;
        }
        scan(c, ring, written);
    }

    if (c->state == LA_POST_TRIGGER && written >= c->end) {
        c->state = LA_DONE;
    }
    return c->state;
}

la_state_t la_capture_finish(la_capture_t *c, uint64_t written_at_stop)
{
    if (c->state == LA_DONE) {
        const uint64_t start = c->trig_at - c->pre;
        if (written_at_stop - start > c->ring_len) {
            c->state = LA_OVERRUN;
        }
    }
    return c->state;
}

uint32_t la_capture_first_index(const la_capture_t *c)
{
    return (uint32_t)((c->trig_at - c->pre) % c->ring_len);
}

uint64_t la_ring_written(uint32_t laps, uint32_t cndtr, bool tc_pending, uint32_t ring_len)
{
    if (cndtr > ring_len) {
        cndtr = ring_len;
    }
    const uint32_t pos = ring_len - cndtr;
    if (tc_pending && pos < ring_len / 2u) {
        laps++;
    }
    return (uint64_t)laps * ring_len + pos;
}
