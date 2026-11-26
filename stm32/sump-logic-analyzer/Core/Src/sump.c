/**
 * @file    sump.c
 * @brief   SUMP / Openbench Logic Sniffer protocol: command parser, metadata
 *          and sample-rate conversion.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "sump.h"
#include "la_config.h"

#include <string.h>

/* Power-on defaults: 1 MS/s, 4096 samples, no trigger. */
#define DEFAULT_DIVIDER  (SUMP_CLOCK_HZ / 1000000u - 1u)
#define DEFAULT_SAMPLES  4096u

void sump_init(sump_parser_t *p, sump_config_t *cfg)
{
    memset(p, 0, sizeof(*p));
    memset(cfg, 0, sizeof(*cfg));
    cfg->divider = DEFAULT_DIVIDER;
    cfg->read_count = DEFAULT_SAMPLES;
    cfg->delay_count = DEFAULT_SAMPLES;
}

static sump_event_t apply_long(sump_config_t *cfg, uint8_t cmd, const uint8_t a[4])
{
    const uint32_t value = (uint32_t)a[0] | ((uint32_t)a[1] << 8) |
                           ((uint32_t)a[2] << 16) | ((uint32_t)a[3] << 24);
    switch (cmd) {
    case SUMP_CMD_DIVIDER:
        cfg->divider = value & 0x00FFFFFFu;
        return SUMP_EV_CONFIG;
    case SUMP_CMD_READ_DELAY:
        /* Both counts are sent in units of 4 samples, minus one. */
        cfg->read_count = (((uint32_t)a[0] | ((uint32_t)a[1] << 8)) + 1u) * 4u;
        cfg->delay_count = (((uint32_t)a[2] | ((uint32_t)a[3] << 8)) + 1u) * 4u;
        return SUMP_EV_CONFIG;
    case SUMP_CMD_FLAGS:
        cfg->flags = value;
        return SUMP_EV_CONFIG;
    case SUMP_CMD_TRIG_MASK:
        cfg->trig_mask = a[0];
        return SUMP_EV_CONFIG;
    case SUMP_CMD_TRIG_VALUE:
        cfg->trig_value = a[0];
        return SUMP_EV_CONFIG;
    case SUMP_CMD_TRIG_EDGE:
        cfg->rise_mask = a[0];
        cfg->fall_mask = a[1];
        return SUMP_EV_CONFIG;
    default:
        /* Trigger config and stages 1..3: accepted, not used (one stage only). */
        return SUMP_EV_IGNORED;
    }
}

sump_event_t sump_feed(sump_parser_t *p, sump_config_t *cfg, uint8_t byte)
{
    if (p->in_long) {
        p->arg[p->n_arg++] = byte;
        if (p->n_arg < 4u) {
            return SUMP_EV_NONE;
        }
        p->in_long = false;
        return apply_long(cfg, p->cmd, p->arg);
    }

    if (byte & 0x80u) {
        p->cmd = byte;
        p->n_arg = 0u;
        p->in_long = true;
        return SUMP_EV_NONE;
    }

    switch (byte) {
    case SUMP_CMD_RESET:
        /* A reset also clears this project's edge-trigger extension, so a
         * standard client always starts from a plain level trigger. */
        cfg->rise_mask = 0u;
        cfg->fall_mask = 0u;
        return SUMP_EV_RESET;
    case SUMP_CMD_RUN:      return SUMP_EV_RUN;
    case SUMP_CMD_ID:       return SUMP_EV_ID;
    case SUMP_CMD_METADATA: return SUMP_EV_METADATA;
    default:                return SUMP_EV_IGNORED;
    }
}

static size_t put_u32_be(uint8_t *buf, uint8_t key, uint32_t v)
{
    buf[0] = key;
    buf[1] = (uint8_t)(v >> 24);
    buf[2] = (uint8_t)(v >> 16);
    buf[3] = (uint8_t)(v >> 8);
    buf[4] = (uint8_t)v;
    return 5u;
}

static size_t put_str(uint8_t *buf, uint8_t key, const char *s)
{
    const size_t n = strlen(s);
    buf[0] = key;
    memcpy(&buf[1], s, n);
    buf[1u + n] = 0u;
    return n + 2u;
}

size_t sump_metadata(uint8_t *buf, size_t cap)
{
    const size_t need = (sizeof(LA_DEVICE_NAME) + 1u) + (sizeof(LA_FW_VERSION) + 1u) + 4u * 5u + 1u;
    if (cap < need) {
        return 0u;
    }
    size_t n = 0u;
    n += put_str(&buf[n], 0x01, LA_DEVICE_NAME);     /* device name */
    n += put_str(&buf[n], 0x02, LA_FW_VERSION);      /* firmware version */
    n += put_u32_be(&buf[n], 0x20, LA_NUM_CHANNELS); /* usable probes */
    n += put_u32_be(&buf[n], 0x21, LA_MAX_SAMPLES);  /* sample memory (bytes) */
    n += put_u32_be(&buf[n], 0x23, LA_MAX_RATE_HZ);  /* max sample rate */
    n += put_u32_be(&buf[n], 0x24, 2u);              /* protocol version */
    buf[n++] = 0x00;                                 /* end of metadata */
    return n;
}

void sump_capture_plan(const sump_config_t *cfg, uint32_t *depth, uint32_t *pre, la_trigger_t *trig)
{
    uint32_t read = cfg->read_count;
    if (read > LA_MAX_SAMPLES) {
        read = LA_MAX_SAMPLES;
    }
    if (read == 0u) {
        read = 4u;
    }
    uint32_t delay = cfg->delay_count;
    if (delay > read) {
        delay = read;
    }
    *depth = read;
    *pre = read - delay;

    trig->level_mask = cfg->trig_mask;
    trig->level_value = cfg->trig_value & cfg->trig_mask;
    trig->rise_mask = cfg->rise_mask;
    trig->fall_mask = cfg->fall_mask;
}

uint32_t la_divider_to_ticks(uint32_t divider)
{
    /* ticks = timer_clock * (divider + 1) / 100 MHz, rounded to nearest. */
    const uint64_t num = (uint64_t)LA_TIMER_CLOCK_HZ * ((uint64_t)(divider & 0x00FFFFFFu) + 1u);
    uint32_t ticks = (uint32_t)((num + SUMP_CLOCK_HZ / 2u) / SUMP_CLOCK_HZ);
    if (ticks < LA_MIN_TICKS) {
        ticks = LA_MIN_TICKS;
    }
    return ticks;
}

uint32_t la_timer_split(uint32_t ticks, uint16_t *psc, uint16_t *arr)
{
    if (ticks < 1u) {
        ticks = 1u;
    }
    uint32_t div = (ticks + 65535u) / 65536u;  /* smallest prescaler that fits */
    if (div > 65536u) {
        div = 65536u;
    }
    uint32_t period = (ticks + div / 2u) / div;
    if (period < 1u) {
        period = 1u;
    }
    if (period > 65536u) {
        period = 65536u;
    }
    *psc = (uint16_t)(div - 1u);
    *arr = (uint16_t)(period - 1u);
    return div * period;
}
