/**
 * @file    la_trigger.h
 * @brief   Level and edge trigger matching for 8-channel samples.
 *
 * A trigger is a set of per-channel conditions that must all hold on the same
 * sample:
 *   - level: (sample & level_mask) == level_value
 *   - rising: every channel in rise_mask goes 0 -> 1 on this sample
 *   - falling: every channel in fall_mask goes 1 -> 0 on this sample
 * A trigger with no conditions fires on the first eligible sample.
 *
 * Shared with the STM32 SUMP logic analyzer in this repository.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef LA_TRIGGER_H
#define LA_TRIGGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t level_mask;
    uint8_t level_value;
    uint8_t rise_mask;
    uint8_t fall_mask;
} la_trigger_t;

/** True when the trigger has no conditions (capture starts immediately). */
bool la_trigger_is_immediate(const la_trigger_t *t);

/** True when the trigger needs the previous sample (edge conditions). */
bool la_trigger_uses_edges(const la_trigger_t *t);

/** Evaluate the trigger on sample @p cur, with @p prev the sample before it. */
static inline bool la_trigger_match(const la_trigger_t *t, uint8_t prev, uint8_t cur)
{
    const uint8_t rose = (uint8_t)(~prev & cur);
    const uint8_t fell = (uint8_t)(prev & ~cur);
    return ((cur & t->level_mask) == t->level_value) &&
           ((rose & t->rise_mask) == t->rise_mask) &&
           ((fell & t->fall_mask) == t->fall_mask);
}

/**
 * Set one channel's condition from a character:
 * 'x' don't care, '0' low, '1' high, 'r' rising, 'f' falling.
 * @return false for an invalid channel or character.
 */
bool la_trigger_set_channel(la_trigger_t *t, unsigned channel, char cond);

#ifdef __cplusplus
}
#endif

#endif /* LA_TRIGGER_H */
