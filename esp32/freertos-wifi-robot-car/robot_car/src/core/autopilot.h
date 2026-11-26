/**
 * @file    autopilot.h
 * @brief   Driving modes and safety rules of the robot car (no hardware dependency).
 *
 * Every control cycle the firmware calls autopilot_step() with the latest
 * sensor values and the latest joystick command. It returns what the wheels
 * should do, after applying, in this order:
 *
 *   1. link watchdog  - no joystick message for LINK_TIMEOUT_MS: stop (all modes)
 *   2. battery cut-off - battery below the limit: stop (all modes)
 *   3. the mode       - manual / obstacle avoidance / line following
 *   4. obstacle guard - forward speed limited as the obstacle gets closer
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef AUTOPILOT_H
#define AUTOPILOT_H

#include <stdbool.h>
#include <stdint.h>

#include "drive.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { MODE_MANUAL = 0, MODE_AVOID, MODE_LINE, MODE_COUNT } car_mode_t;

typedef enum {
    STOP_NONE = 0,
    STOP_LINK,       /* joystick messages stopped arriving */
    STOP_BATTERY,    /* battery below the cut-off */
    STOP_OBSTACLE,   /* obstacle inside the stop distance */
    STOP_LINE_END    /* both line sensors on: end of track or junction */
} stop_reason_t;

typedef struct {
    float distance_cm;   /* front distance (ultrasonic) */
    bool distance_ok;    /* false: no echo / out of range (treated as clear) */
    bool ir_left;        /* line / edge sensors: true = line detected */
    bool ir_right;
    float battery_v;
} sensors_t;

typedef struct {
    uint32_t link_timeout_ms;   /* e.g. 500 */
    float battery_min_v;        /* e.g. 6.4 V for a 2S pack */
    float stop_cm;              /* closer than this: no forward motion */
    float slow_cm;              /* from here on forward speed scales down */
    float cruise;               /* forward speed in autonomous modes */
    float turn;                 /* turning effort in autonomous modes */
    uint32_t backup_ms;         /* avoidance: reverse for this long */
    uint32_t turn_ms;           /* avoidance: then turn for this long */
} autopilot_cfg_t;

typedef enum { AV_CRUISE = 0, AV_BACKUP, AV_TURN } avoid_state_t;

typedef struct {
    autopilot_cfg_t cfg;
    car_mode_t mode;
    avoid_state_t av_state;
    uint32_t av_until_ms;
    int av_turn_dir;          /* alternates +1 / -1 so the car does not circle */
    stop_reason_t stop;       /* why the output is zero (for telemetry) */
} autopilot_t;

void autopilot_defaults(autopilot_cfg_t *cfg);
void autopilot_init(autopilot_t *ap, const autopilot_cfg_t *cfg);
void autopilot_set_mode(autopilot_t *ap, car_mode_t mode);

/**
 * One control step. @p manual is the latest joystick command, received at
 * @p last_cmd_ms; @p now_ms is the current time.
 */
drive_cmd_t autopilot_step(autopilot_t *ap, const sensors_t *s, drive_cmd_t manual,
                           uint32_t now_ms, uint32_t last_cmd_ms);

/** Forward speed limit (0..1) for a measured distance. */
float autopilot_speed_limit(const autopilot_cfg_t *cfg, const sensors_t *s);

const char *autopilot_mode_name(car_mode_t mode);
const char *autopilot_stop_name(stop_reason_t reason);
/** "manual" / "avoid" / "line" -> mode; MODE_COUNT if unknown. */
car_mode_t autopilot_mode_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AUTOPILOT_H */
