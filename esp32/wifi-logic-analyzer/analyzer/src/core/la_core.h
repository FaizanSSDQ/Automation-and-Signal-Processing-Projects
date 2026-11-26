/**
 * @file    la_core.h
 * @brief   Hardware-independent core of the ESP32 Wi-Fi logic analyzer:
 *          pin packing, capture planning, the recording loop, UART and I2C
 *          decoders, signal measurements and JSON.
 *
 * The recording loop reaches the hardware only through the la_hw_t hooks, so
 * the host tests run exactly the loop the firmware runs.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef LA_CORE_H
#define LA_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "la_trigger.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LA_NUM_CHANNELS   8
#define LA_MAX_SAMPLES    65536u
#define LA_MIN_SAMPLES    256u
#define LA_CPU_HZ         240000000u
#define LA_IRQ_OFF_MAX_US 100000u     /* longest stretch with interrupts off (watchdog is 300 ms) */
#define LA_IRQ_OFF_RATE   250000u     /* below this rate interrupts stay on: their jitter is small */

/* Input pins, channel 0..7. All are in GPIO_IN_REG (GPIO 0..31) and none is a
 * strapping, flash or UART pin. */
#define LA_PINS {13, 14, 16, 17, 18, 19, 21, 22}

/** Gather the eight input bits from one read of GPIO_IN_REG (3 shifts, 3 masks). */
static inline uint8_t la_pack(uint32_t gpio_in)
{
    return (uint8_t)(((gpio_in >> 13) & 0x03u) |   /* GPIO 13, 14      -> bits 0, 1 */
                     ((gpio_in >> 14) & 0x3Cu) |   /* GPIO 16 .. 19    -> bits 2 .. 5 */
                     ((gpio_in >> 15) & 0xC0u));   /* GPIO 21, 22      -> bits 6, 7 */
}

/* ---- capture plan --------------------------------------------------------------------- */

typedef struct {
    uint32_t rate_hz;        /* requested */
    uint32_t period_cycles;  /* CPU cycles between samples */
    uint32_t actual_hz;      /* LA_CPU_HZ / period_cycles */
    uint32_t samples, pre, post;
    bool irq_off;            /* record the post-trigger part with interrupts off */
} la_plan_t;

/** Sample rates offered (Hz), ascending; LA_RATE_COUNT entries. */
#define LA_RATE_COUNT 9
uint32_t la_rate(int index);

/**
 * Check and plan a capture. @p pre_pct of the samples come before the
 * trigger (at least one: the trigger sample itself).
 * @return false for a rate not offered or a sample count out of range.
 */
bool la_plan(uint32_t rate_hz, uint32_t samples, uint8_t pre_pct, la_plan_t *p);

/** Parse a trigger string, one character per channel 0..7: x 0 1 r f. */
bool la_trigger_parse(const char *s, la_trigger_t *t);

/* ---- recording loop ------------------------------------------------------------------- */

typedef enum { LA_GO_ON, LA_RESTART, LA_STOP_WAITING } la_wait_t;

typedef struct {
    uint32_t (*now)(void);            /* cycle counter */
    uint8_t (*read)(void);            /* one packed sample */
    void (*irq)(bool off);            /* disable / re-enable interrupts */
    /* Called every 4096 samples while waiting for the trigger: may yield
     * (then returns LA_RESTART) or end the wait (LA_STOP_WAITING). */
    la_wait_t (*waiting)(void);
} la_hw_t;

typedef struct {
    bool triggered;          /* false: the wait ended and the capture was forced */
    uint32_t trigger_index;  /* position of the trigger sample in the buffer */
    uint32_t max_late;       /* worst lateness of a sample, CPU cycles */
} la_result_t;

/**
 * Record a capture into @p buf (plan->samples bytes), oldest sample first.
 * Before the trigger the buffer is a ring; once at least plan->pre samples
 * are in it, the trigger is checked on every sample. After the trigger
 * plan->post more samples are taken and the ring is rotated into order.
 */
void la_record(const la_plan_t *plan, const la_trigger_t *trig, const la_hw_t *hw, uint8_t *buf,
               la_result_t *res);

/** Rotate buf[0..n) left by @p first, in place (three reversals). */
void la_rotate(uint8_t *buf, uint32_t n, uint32_t first);

/* ---- measurements ---------------------------------------------------------------------- */

typedef struct {
    uint32_t edges;          /* rising + falling */
    uint32_t high_samples;
    float freq_hz;           /* from first to last rising edge; 0 if fewer than two */
    float duty_pct;          /* share of samples high */
    uint32_t min_pulse;      /* shortest high or low run between edges, samples; 0 if none */
} la_measure_t;

void la_measure(const uint8_t *buf, uint32_t n, unsigned ch, uint32_t rate_hz, la_measure_t *m);

/* ---- decoders -------------------------------------------------------------------------- */

typedef struct {
    uint32_t start, end;     /* sample positions: start bit edge .. middle of the stop bit */
    uint8_t value;
    bool frame_error;        /* stop bit was low */
} la_uart_frame_t;

/** 8N1 UART, idle high, LSB first. @return frames written; 0 if under 3 samples per bit. */
size_t la_uart_decode(const uint8_t *buf, uint32_t n, unsigned ch, uint32_t rate_hz, uint32_t baud,
                      la_uart_frame_t *out, size_t max);

typedef enum { I2C_START, I2C_RESTART, I2C_STOP, I2C_ADDRESS, I2C_DATA } la_i2c_kind_t;

typedef struct {
    la_i2c_kind_t kind;
    uint32_t start, end;     /* sample positions */
    uint8_t value;           /* address (7-bit) or data byte */
    bool read;               /* address: R/W bit */
    bool ack;                /* address and data: acknowledged */
} la_i2c_event_t;

/** I2C from SDA and SCL channels. @return events written. */
size_t la_i2c_decode(const uint8_t *buf, uint32_t n, unsigned sda, unsigned scl, la_i2c_event_t *out,
                     size_t max);

/* ---- JSON ------------------------------------------------------------------------------ */

size_t la_result_json(const la_plan_t *p, const la_result_t *r, char *out, size_t cap);
size_t la_uart_json(const la_uart_frame_t *f, size_t n, char *out, size_t cap);
size_t la_i2c_json(const la_i2c_event_t *e, size_t n, char *out, size_t cap);
size_t la_measure_json(const la_measure_t *m, size_t channels, char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* LA_CORE_H */
