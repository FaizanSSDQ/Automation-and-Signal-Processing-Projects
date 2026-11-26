/**
 * @file    sump.h
 * @brief   SUMP / Openbench Logic Sniffer protocol: command parser, metadata
 *          and sample-rate conversion.
 *
 * SUMP is the serial protocol of the open-source SUMP and Openbench Logic
 * Sniffer analyzers. sigrok / PulseView talk to it through their "ols"
 * driver, so a device that speaks it works with those tools unchanged.
 *
 * Commands are either one byte (short) or one byte plus a 32-bit little-endian
 * argument (long, command byte >= 0x80):
 *
 *   0x00 reset         0x01 run           0x02 ID ("1ALS")   0x04 metadata
 *   0x80 divider       0x81 read/delay    0x82 flags
 *   0xC0 trigger mask  0xC1 trigger value 0xC2 trigger config (stage 0)
 *   0xC3 edge trigger  (extension of this project: rising mask, falling mask)
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef SUMP_H
#define SUMP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "la_trigger.h"

enum {
    SUMP_CMD_RESET = 0x00,
    SUMP_CMD_RUN = 0x01,
    SUMP_CMD_ID = 0x02,
    SUMP_CMD_METADATA = 0x04,
    SUMP_CMD_XON = 0x11,
    SUMP_CMD_XOFF = 0x13,
    SUMP_CMD_DIVIDER = 0x80,
    SUMP_CMD_READ_DELAY = 0x81,
    SUMP_CMD_FLAGS = 0x82,
    SUMP_CMD_TRIG_MASK = 0xC0,
    SUMP_CMD_TRIG_VALUE = 0xC1,
    SUMP_CMD_TRIG_CONFIG = 0xC2,
    SUMP_CMD_TRIG_EDGE = 0xC3,
};

typedef enum {
    SUMP_EV_NONE = 0,   /* byte consumed, command not complete yet */
    SUMP_EV_RESET,
    SUMP_EV_RUN,
    SUMP_EV_ID,
    SUMP_EV_METADATA,
    SUMP_EV_CONFIG,     /* a long command updated the configuration */
    SUMP_EV_IGNORED     /* a valid command this device does not act on */
} sump_event_t;

typedef struct {
    uint32_t divider;      /* sample rate = 100 MHz / (divider + 1) */
    uint32_t read_count;   /* samples requested */
    uint32_t delay_count;  /* of which after the trigger */
    uint32_t flags;
    uint8_t trig_mask;     /* stage-0 level trigger */
    uint8_t trig_value;
    uint8_t rise_mask;     /* edge extension (0xC3) */
    uint8_t fall_mask;
} sump_config_t;

typedef struct {
    uint8_t cmd;
    uint8_t arg[4];
    uint8_t n_arg;
    bool in_long;
} sump_parser_t;

/** Reset the parser and load the power-on configuration. */
void sump_init(sump_parser_t *p, sump_config_t *cfg);

/** Feed one received byte. */
sump_event_t sump_feed(sump_parser_t *p, sump_config_t *cfg, uint8_t byte);

/** Build the reply to the metadata command. @return bytes written (0 if @p cap is too small). */
size_t sump_metadata(uint8_t *buf, size_t cap);

/** Capture size and trigger derived from a configuration, clamped to the device limits. */
void sump_capture_plan(const sump_config_t *cfg, uint32_t *depth, uint32_t *pre, la_trigger_t *trig);

/** Timer ticks per sample for a SUMP divider, rounded and limited to the fastest rate. */
uint32_t la_divider_to_ticks(uint32_t divider);

/**
 * Split a tick count into a 16-bit prescaler and auto-reload value.
 * @return the tick count actually achieved, (psc + 1) * (arr + 1).
 */
uint32_t la_timer_split(uint32_t ticks, uint16_t *psc, uint16_t *arr);

#endif /* SUMP_H */
