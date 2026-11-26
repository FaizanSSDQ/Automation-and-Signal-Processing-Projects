/**
 * @file    la_trigger.c
 * @brief   Level and edge trigger matching for 8-channel samples.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "la_trigger.h"
#include "la_core.h"   /* LA_NUM_CHANNELS */

bool la_trigger_is_immediate(const la_trigger_t *t)
{
    return (t->level_mask | t->rise_mask | t->fall_mask) == 0u;
}

bool la_trigger_uses_edges(const la_trigger_t *t)
{
    return (t->rise_mask | t->fall_mask) != 0u;
}

bool la_trigger_set_channel(la_trigger_t *t, unsigned channel, char cond)
{
    if (channel >= LA_NUM_CHANNELS) {
        return false;
    }
    const uint8_t bit = (uint8_t)(1u << channel);
    const uint8_t keep = (uint8_t)~bit;

    /* Clear any previous condition on this channel first. */
    uint8_t level_mask = t->level_mask & keep;
    uint8_t level_value = t->level_value & keep;
    uint8_t rise_mask = t->rise_mask & keep;
    uint8_t fall_mask = t->fall_mask & keep;

    switch (cond) {
    case 'x': case 'X': break;
    case '0': level_mask |= bit; break;
    case '1': level_mask |= bit; level_value |= bit; break;
    case 'r': case 'R': rise_mask |= bit; break;
    case 'f': case 'F': fall_mask |= bit; break;
    default: return false;
    }

    t->level_mask = level_mask;
    t->level_value = level_value;
    t->rise_mask = rise_mask;
    t->fall_mask = fall_mask;
    return true;
}
