/**
 * @file    la_capture.h
 * @brief   Pre-trigger capture engine on top of a circular DMA sample ring.
 *
 * DMA writes samples into a ring of ring_len bytes forever. The engine is
 * told how many samples have been written in total (an absolute count that
 * never wraps) and scans the new ones for the trigger. Once the trigger is
 * found it waits for the post-trigger samples, after which the caller stops
 * the sampling timer. The captured window is
 *
 *     [trig_at - pre, trig_at - pre + depth)
 *
 * so the trigger sample sits at position @c pre of the returned data.
 *
 * The engine has no hardware dependencies; the firmware feeds it from the DMA
 * counter and the host tests feed it from a simulated DMA.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef LA_CAPTURE_H
#define LA_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>
#include "la_trigger.h"

typedef enum {
    LA_IDLE = 0,
    LA_PRE_TRIGGER,   /* collecting the samples that precede the trigger */
    LA_WAIT_TRIGGER,  /* scanning new samples for the trigger condition */
    LA_POST_TRIGGER,  /* trigger found, collecting the remaining samples */
    LA_DONE,          /* window complete: stop the timer and read it out */
    LA_OVERRUN        /* the scanner fell a full ring behind the DMA */
} la_state_t;

typedef struct {
    /* configuration */
    uint32_t ring_len;
    uint32_t depth;      /* samples returned to the host */
    uint32_t pre;        /* of which before the trigger */
    la_trigger_t trig;

    /* state */
    la_state_t state;
    uint64_t scanned;    /* absolute index of the next sample to examine */
    uint32_t scan_idx;   /* scanned % ring_len */
    uint8_t prev;        /* sample before 'scanned', for edge triggers */
    bool have_prev;
    uint64_t trig_at;    /* absolute index of the trigger sample */
    uint64_t end;        /* absolute index one past the last sample needed */
} la_capture_t;

/**
 * Prepare a capture. @p pre must not exceed @p depth, and @p depth must leave
 * head room in the ring (depth < ring_len).
 * @return false for an invalid configuration (state stays LA_IDLE).
 */
bool la_capture_start(la_capture_t *c, uint32_t ring_len, uint32_t depth,
                      uint32_t pre, const la_trigger_t *trig);

/**
 * Advance the engine. @p written is the total number of samples DMA has
 * stored since the capture started. Call often: at least once per ring_len
 * samples while waiting for the trigger.
 */
la_state_t la_capture_poll(la_capture_t *c, const uint8_t *ring, uint64_t written);

/**
 * Call after the timer is stopped, with the final write count. Confirms that
 * no sample of the window was overwritten (otherwise the state becomes
 * LA_OVERRUN).
 */
la_state_t la_capture_finish(la_capture_t *c, uint64_t written_at_stop);

/** Ring index of the first sample of the captured window (state LA_DONE). */
uint32_t la_capture_first_index(const la_capture_t *c);

/**
 * Total samples written by a circular DMA, from the number of completed laps
 * counted in the transfer-complete interrupt and the channel's remaining
 * count register.
 *
 * Read CNDTR first and the transfer-complete flag second, with interrupts
 * disabled. If the flag is pending the ring has wrapped but the interrupt
 * has not counted the lap yet; a small position then belongs to the new lap.
 */
uint64_t la_ring_written(uint32_t laps, uint32_t cndtr, bool tc_pending, uint32_t ring_len);

#endif /* LA_CAPTURE_H */
