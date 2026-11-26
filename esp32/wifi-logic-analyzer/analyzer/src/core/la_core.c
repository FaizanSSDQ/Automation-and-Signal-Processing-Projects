/**
 * @file    la_core.c
 * @brief   Capture planning, recording loop, measurements, decoders and JSON.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "la_core.h"

#include <stdio.h>
#include <string.h>

/* On the ESP32 the recording loop runs from IRAM: a flash-cache miss in the
 * middle of a capture would delay samples. Elsewhere the attribute is empty. */
#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#define LA_IRAM IRAM_ATTR
#else
#define LA_IRAM
#endif

#define BIT(buf, i, ch) (((buf)[i] >> (ch)) & 1u)

/* ---- plan ----------------------------------------------------------------------------- */

static const uint32_t RATES[LA_RATE_COUNT] = {5000u,   10000u,  20000u,   50000u,  100000u,
                                              200000u, 500000u, 1000000u, 2000000u};

uint32_t la_rate(int index)
{
    return index >= 0 && index < LA_RATE_COUNT ? RATES[index] : 0u;
}

bool la_plan(uint32_t rate_hz, uint32_t samples, uint8_t pre_pct, la_plan_t *p)
{
    bool offered = false;
    for (int i = 0; i < LA_RATE_COUNT; i++) {
        offered |= RATES[i] == rate_hz;
    }
    if (!offered || samples < LA_MIN_SAMPLES || samples > LA_MAX_SAMPLES || pre_pct > 99u) {
        return false;
    }
    p->rate_hz = rate_hz;
    p->period_cycles = LA_CPU_HZ / rate_hz;
    p->actual_hz = LA_CPU_HZ / p->period_cycles;
    p->samples = samples;
    p->pre = (uint32_t)((uint64_t)samples * pre_pct / 100u);
    if (p->pre == 0u) {
        p->pre = 1u;   /* the trigger sample itself */
    }
    p->post = samples - p->pre;
    const uint64_t post_us = (uint64_t)p->post * 1000000u / rate_hz;
    p->irq_off = rate_hz >= LA_IRQ_OFF_RATE && post_us <= LA_IRQ_OFF_MAX_US;
    return true;
}

bool la_trigger_parse(const char *s, la_trigger_t *t)
{
    memset(t, 0, sizeof(*t));
    if (s == NULL || strlen(s) != LA_NUM_CHANNELS) {
        return false;
    }
    for (unsigned ch = 0; ch < LA_NUM_CHANNELS; ch++) {
        if (!la_trigger_set_channel(t, ch, s[ch])) {
            return false;
        }
    }
    return true;
}

/* ---- recording ------------------------------------------------------------------------ */

LA_IRAM static void reverse(uint8_t *b, uint32_t lo, uint32_t hi)   /* [lo, hi) */
{
    while (lo + 1u < hi) {
        const uint8_t t = b[lo];
        b[lo] = b[hi - 1u];
        b[hi - 1u] = t;
        lo++;
        hi--;
    }
}

LA_IRAM void la_rotate(uint8_t *buf, uint32_t n, uint32_t first)
{
    if (n == 0u || (first %= n) == 0u) {
        return;
    }
    reverse(buf, 0, first);
    reverse(buf, first, n);
    reverse(buf, 0, n);
}

LA_IRAM void la_record(const la_plan_t *plan, const la_trigger_t *trig, const la_hw_t *hw, uint8_t *buf,
               la_result_t *res)
{
    const uint32_t n = plan->samples;
    const uint32_t period = plan->period_cycles;
    uint32_t pos = 0;          /* next slot in the ring */
    uint32_t filled = 0;       /* samples since the last (re)start, capped at n */
    uint32_t max_late = 0;
    uint32_t chunk = 0;
    bool force = false;
    res->triggered = false;

    uint8_t prev = hw->read();
    uint32_t next = hw->now() + period;

    /* Pre-trigger: fill the ring, then look for the trigger on every sample. */
    for (;;) {
        uint32_t t;
        while ((int32_t)((t = hw->now()) - next) < 0) {
        }
        const uint8_t s = hw->read();
        const uint32_t late = t - next;
        max_late = late > max_late ? late : max_late;
        next += period;
        buf[pos] = s;
        if (++pos == n) {
            pos = 0;
        }
        if (filled < n) {
            filled++;
        }
        if (filled >= plan->pre && (force || la_trigger_match(trig, prev, s))) {
            res->triggered = !force;
            break;
        }
        prev = s;
        if (++chunk == 4096u) {
            chunk = 0;
            const la_wait_t w = hw->waiting();
            if (w == LA_RESTART) {         /* the task yielded: the ring has a gap, refill it */
                filled = 0;
                prev = hw->read();
                next = hw->now() + period;
            } else if (w == LA_STOP_WAITING) {
                force = true;
            }
        }
    }

    /* Post-trigger: optionally with interrupts off, so no sample is delayed. */
    if (plan->irq_off) {
        hw->irq(true);
    }
    for (uint32_t k = 0; k < plan->post; k++) {
        uint32_t t;
        while ((int32_t)((t = hw->now()) - next) < 0) {
        }
        buf[pos] = hw->read();
        const uint32_t late = t - next;
        max_late = late > max_late ? late : max_late;
        next += period;
        if (++pos == n) {
            pos = 0;
        }
    }
    if (plan->irq_off) {
        hw->irq(false);
    }

    la_rotate(buf, n, pos);   /* pos is now the oldest sample */
    res->trigger_index = plan->pre - 1u;
    res->max_late = max_late;
}

/* ---- measurements --------------------------------------------------------------------- */

void la_measure(const uint8_t *buf, uint32_t n, unsigned ch, uint32_t rate_hz, la_measure_t *m)
{
    memset(m, 0, sizeof(*m));
    if (n == 0u) {
        return;
    }
    uint32_t first_rise = 0, last_rise = 0, rises = 0, last_edge = 0;
    bool seen_edge = false;
    m->high_samples = BIT(buf, 0, ch);
    for (uint32_t i = 1; i < n; i++) {
        const unsigned a = BIT(buf, i - 1u, ch), b = BIT(buf, i, ch);
        m->high_samples += b;
        if (a == b) {
            continue;
        }
        m->edges++;
        if (seen_edge) {   /* a complete run between two edges */
            const uint32_t run = i - last_edge;
            if (m->min_pulse == 0u || run < m->min_pulse) {
                m->min_pulse = run;
            }
        }
        seen_edge = true;
        last_edge = i;
        if (b) {
            if (rises++ == 0u) {
                first_rise = i;
            }
            last_rise = i;
        }
    }
    m->duty_pct = 100.0f * (float)m->high_samples / (float)n;
    if (rises >= 2u) {
        m->freq_hz = (float)(rises - 1u) * (float)rate_hz / (float)(last_rise - first_rise);
    }
}

/* ---- UART ----------------------------------------------------------------------------- */

size_t la_uart_decode(const uint8_t *buf, uint32_t n, unsigned ch, uint32_t rate_hz, uint32_t baud,
                      la_uart_frame_t *out, size_t max)
{
    if (baud == 0u || rate_hz < 3u * baud) {
        return 0;
    }
    const float spb = (float)rate_hz / (float)baud;   /* samples per bit */
    size_t count = 0;
    uint32_t i = 1;
    while (i < n && count < max) {
        if (!(BIT(buf, i - 1u, ch) && !BIT(buf, i, ch))) {   /* falling edge = start bit */
            i++;
            continue;
        }
        const uint32_t start = i;
        const uint32_t stop = start + (uint32_t)(9.5f * spb);
        if (stop >= n) {
            break;   /* frame runs past the end of the capture */
        }
        if (BIT(buf, start + (uint32_t)(0.5f * spb), ch)) {
            i++;     /* glitch: not low in the middle of the start bit */
            continue;
        }
        uint8_t v = 0;
        for (unsigned k = 0; k < 8u; k++) {
            v |= (uint8_t)(BIT(buf, start + (uint32_t)((1.5f + (float)k) * spb), ch) << k);
        }
        la_uart_frame_t *f = &out[count++];
        f->start = start;
        f->end = stop;
        f->value = v;
        f->frame_error = !BIT(buf, stop, ch);
        i = stop + 1u;
    }
    return count;
}

/* ---- I2C ------------------------------------------------------------------------------ */

size_t la_i2c_decode(const uint8_t *buf, uint32_t n, unsigned sda, unsigned scl, la_i2c_event_t *out,
                     size_t max)
{
    size_t count = 0;
    bool active = false, first = false;
    unsigned bits = 0;
    uint8_t byte = 0;
    uint32_t byte_start = 0;
#define EMIT(k, s, e)                                                                      \
    do {                                                                                   \
        if (count == max) {                                                                \
            return count;                                                                  \
        }                                                                                  \
        out[count] = (la_i2c_event_t){(k), (s), (e), 0, false, false};                     \
        count++;                                                                           \
    } while (0)

    for (uint32_t i = 1; i < n; i++) {
        const unsigned d0 = BIT(buf, i - 1u, sda), d1 = BIT(buf, i, sda);
        const unsigned c0 = BIT(buf, i - 1u, scl), c1 = BIT(buf, i, scl);
        if (c0 && c1 && d0 != d1) {            /* SDA changes while SCL is high */
            if (!d1) {
                EMIT(active ? I2C_RESTART : I2C_START, i, i);
                active = true;
                first = true;
                bits = 0;
                byte = 0;
            } else {
                EMIT(I2C_STOP, i, i);
                active = false;
            }
            continue;
        }
        if (!active || c0 || !c1) {
            continue;                          /* only rising SCL edges carry bits */
        }
        if (bits == 0u) {
            byte_start = i;
        }
        if (bits < 8u) {
            byte = (uint8_t)((byte << 1) | d1);
            bits++;
            continue;
        }
        /* ninth clock: acknowledge, low = ACK */
        EMIT(first ? I2C_ADDRESS : I2C_DATA, byte_start, i);
        out[count - 1u].value = first ? (uint8_t)(byte >> 1) : byte;
        out[count - 1u].read = first && (byte & 1u);
        out[count - 1u].ack = !d1;
        first = false;
        bits = 0;
        byte = 0;
    }
#undef EMIT
    return count;
}

/* ---- JSON ----------------------------------------------------------------------------- */

#define APPEND(...)                                                                        \
    do {                                                                                   \
        const int w_ = snprintf(out + len, cap - len, __VA_ARGS__);                        \
        if (w_ < 0 || (size_t)w_ >= cap - len) {                                           \
            goto overflow;                                                                 \
        }                                                                                  \
        len += (size_t)w_;                                                                 \
    } while (0)

size_t la_result_json(const la_plan_t *p, const la_result_t *r, char *out, size_t cap)
{
    size_t len = 0;
    if (cap == 0u) {
        return 0;
    }
    APPEND("{\"rate\":%lu,\"actual_rate\":%lu,\"samples\":%lu,\"pre\":%lu,\"trigger_index\":%lu,"
           "\"triggered\":%s,\"irq_off\":%s,\"max_late_ns\":%lu}",
           (unsigned long)p->rate_hz, (unsigned long)p->actual_hz, (unsigned long)p->samples,
           (unsigned long)p->pre, (unsigned long)r->trigger_index, r->triggered ? "true" : "false",
           p->irq_off ? "true" : "false", (unsigned long)((uint64_t)r->max_late * 1000000000u / LA_CPU_HZ));
    return len;
overflow:
    out[0] = '\0';
    return 0;
}

size_t la_uart_json(const la_uart_frame_t *f, size_t n, char *out, size_t cap)
{
    size_t len = 0;
    if (cap == 0u) {
        return 0;
    }
    APPEND("[");
    for (size_t i = 0; i < n; i++) {
        APPEND("%s{\"s\":%lu,\"e\":%lu,\"v\":%u%s}", i ? "," : "", (unsigned long)f[i].start,
               (unsigned long)f[i].end, f[i].value, f[i].frame_error ? ",\"err\":1" : "");
    }
    APPEND("]");
    return len;
overflow:
    out[0] = '\0';
    return 0;
}

size_t la_i2c_json(const la_i2c_event_t *e, size_t n, char *out, size_t cap)
{
    static const char *const KIND[] = {"start", "restart", "stop", "addr", "data"};
    size_t len = 0;
    if (cap == 0u) {
        return 0;
    }
    APPEND("[");
    for (size_t i = 0; i < n; i++) {
        APPEND("%s{\"k\":\"%s\",\"s\":%lu,\"e\":%lu", i ? "," : "", KIND[e[i].kind], (unsigned long)e[i].start,
               (unsigned long)e[i].end);
        if (e[i].kind == I2C_ADDRESS || e[i].kind == I2C_DATA) {
            APPEND(",\"v\":%u,\"ack\":%d", e[i].value, e[i].ack ? 1 : 0);
        }
        if (e[i].kind == I2C_ADDRESS) {
            APPEND(",\"rd\":%d", e[i].read ? 1 : 0);
        }
        APPEND("}");
    }
    APPEND("]");
    return len;
overflow:
    out[0] = '\0';
    return 0;
}

size_t la_measure_json(const la_measure_t *m, size_t channels, char *out, size_t cap)
{
    size_t len = 0;
    if (cap == 0u) {
        return 0;
    }
    APPEND("[");
    for (size_t i = 0; i < channels; i++) {
        APPEND("%s{\"edges\":%lu,\"freq\":%.1f,\"duty\":%.1f,\"min_pulse\":%lu}", i ? "," : "",
               (unsigned long)m[i].edges, (double)m[i].freq_hz, (double)m[i].duty_pct,
               (unsigned long)m[i].min_pulse);
    }
    APPEND("]");
    return len;
overflow:
    out[0] = '\0';
    return 0;
}
