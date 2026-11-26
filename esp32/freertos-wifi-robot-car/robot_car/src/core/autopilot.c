/**
 * @file    autopilot.c
 * @brief   Driving modes and safety rules of the robot car.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "autopilot.h"

#include <string.h>

static const char *const mode_names[MODE_COUNT] = {"manual", "avoid", "line"};

void autopilot_defaults(autopilot_cfg_t *cfg)
{
    cfg->link_timeout_ms = 500u;
    cfg->battery_min_v = 6.4f;
    cfg->stop_cm = 15.0f;
    cfg->slow_cm = 60.0f;
    cfg->cruise = 0.55f;
    cfg->turn = 0.7f;
    cfg->backup_ms = 400u;
    cfg->turn_ms = 450u;
}

void autopilot_init(autopilot_t *ap, const autopilot_cfg_t *cfg)
{
    memset(ap, 0, sizeof(*ap));
    ap->cfg = *cfg;
    ap->mode = MODE_MANUAL;
    ap->av_state = AV_CRUISE;
    ap->av_turn_dir = 1;
}

void autopilot_set_mode(autopilot_t *ap, car_mode_t mode)
{
    if (mode < MODE_COUNT && mode != ap->mode) {
        ap->mode = mode;
        ap->av_state = AV_CRUISE;
    }
}

float autopilot_speed_limit(const autopilot_cfg_t *cfg, const sensors_t *s)
{
    if (!s->distance_ok || s->distance_cm >= cfg->slow_cm) {
        return 1.0f;
    }
    if (s->distance_cm <= cfg->stop_cm) {
        return 0.0f;
    }
    return (s->distance_cm - cfg->stop_cm) / (cfg->slow_cm - cfg->stop_cm);
}

static bool time_reached(uint32_t now, uint32_t until)
{
    return (int32_t)(now - until) >= 0;
}

static drive_cmd_t avoid_step(autopilot_t *ap, const sensors_t *s, uint32_t now)
{
    const autopilot_cfg_t *c = &ap->cfg;
    drive_cmd_t out = {0.0f, 0.0f};

    switch (ap->av_state) {
    case AV_CRUISE:
        if (s->distance_ok && s->distance_cm <= c->stop_cm + 10.0f) {
            ap->av_state = AV_BACKUP;
            ap->av_until_ms = now + c->backup_ms;
            out.throttle = -c->cruise;
        } else {
            out.throttle = c->cruise;
        }
        break;
    case AV_BACKUP:
        if (time_reached(now, ap->av_until_ms)) {
            ap->av_state = AV_TURN;
            ap->av_until_ms = now + c->turn_ms;
            out.steer = c->turn * (float)ap->av_turn_dir;
        } else {
            out.throttle = -c->cruise;
        }
        break;
    case AV_TURN:
    default:
        if (time_reached(now, ap->av_until_ms)) {
            ap->av_state = AV_CRUISE;
            ap->av_turn_dir = -ap->av_turn_dir;   /* next time turn the other way */
            out.throttle = c->cruise;
        } else {
            out.steer = c->turn * (float)ap->av_turn_dir;
        }
        break;
    }
    return out;
}

static drive_cmd_t line_step(autopilot_t *ap, const sensors_t *s)
{
    const autopilot_cfg_t *c = &ap->cfg;
    drive_cmd_t out = {c->cruise, 0.0f};
    if (s->ir_left && s->ir_right) {
        out.throttle = 0.0f;              /* end of track or junction: stop */
        ap->stop = STOP_LINE_END;
    } else if (s->ir_left) {
        out.throttle = c->cruise * 0.5f;  /* line under the left sensor: steer left */
        out.steer = -c->turn;
    } else if (s->ir_right) {
        out.throttle = c->cruise * 0.5f;
        out.steer = c->turn;
    }
    return out;
}

drive_cmd_t autopilot_step(autopilot_t *ap, const sensors_t *s, drive_cmd_t manual,
                           uint32_t now_ms, uint32_t last_cmd_ms)
{
    const drive_cmd_t stop = {0.0f, 0.0f};
    ap->stop = STOP_NONE;

    if (now_ms - last_cmd_ms > ap->cfg.link_timeout_ms) {
        ap->stop = STOP_LINK;
        ap->av_state = AV_CRUISE;
        return stop;
    }
    if (s->battery_v > 0.0f && s->battery_v < ap->cfg.battery_min_v) {
        ap->stop = STOP_BATTERY;
        return stop;
    }

    drive_cmd_t out;
    switch (ap->mode) {
    case MODE_AVOID:
        out = avoid_step(ap, s, now_ms);
        break;
    case MODE_LINE:
        out = line_step(ap, s);
        break;
    case MODE_MANUAL:
    default:
        out.throttle = drive_clamp(manual.throttle);
        out.steer = drive_clamp(manual.steer);
        break;
    }

    /* Obstacle guard: forward motion only, so the car can always back away. */
    if (out.throttle > 0.0f) {
        const float limit = autopilot_speed_limit(&ap->cfg, s);
        out.throttle *= limit;
        if (limit == 0.0f) {
            ap->stop = STOP_OBSTACLE;
        }
    }
    return out;
}

const char *autopilot_mode_name(car_mode_t mode)
{
    return mode < MODE_COUNT ? mode_names[mode] : "?";
}

const char *autopilot_stop_name(stop_reason_t reason)
{
    switch (reason) {
    case STOP_LINK:     return "link lost";
    case STOP_BATTERY:  return "battery low";
    case STOP_OBSTACLE: return "obstacle";
    case STOP_LINE_END: return "line end";
    default:            return "";
    }
}

car_mode_t autopilot_mode_from_name(const char *name)
{
    for (int m = 0; m < MODE_COUNT; m++) {
        if (strcmp(name, mode_names[m]) == 0) {
            return (car_mode_t)m;
        }
    }
    return MODE_COUNT;
}
