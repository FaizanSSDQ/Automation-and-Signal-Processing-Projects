/**
 * Host simulation of the control loop (the firmware's autopilot, mixing and
 * ramping at 50 Hz) on a simple 1-D car model, for the README figures.
 *
 *   sim_drive wall   full throttle towards a wall 200 cm ahead
 *   sim_drive link   driving at 70 %, phone link lost after 2 s
 *
 * Output: CSV  t_s,distance_cm,joystick,command,wheel,stop
 * Car model: speed = wheel x 80 cm/s (a typical small 4WD chassis), no inertia
 * beyond the firmware's own ramp.
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <string.h>

#include "../robot_car/src/core/autopilot.h"
#include "../robot_car/src/core/drive.h"

#define HZ 50
#define TOP_SPEED_CM_S 80.0f
#define RAMP_STEP (4.0f / HZ)

int main(int argc, char **argv)
{
    const int wall = argc > 1 && strcmp(argv[1], "wall") == 0;
    autopilot_cfg_t cfg;
    autopilot_defaults(&cfg);
    autopilot_t ap;
    autopilot_init(&ap, &cfg);

    float distance = wall ? 200.0f : 400.0f;
    float wheel = 0.0f;
    uint32_t last_cmd = 0;
    const float joystick = wall ? 1.0f : 0.7f;

    printf("t_s,distance_cm,joystick,command,wheel,stop\n");
    for (int k = 0; k <= 5 * HZ; k++) {
        const uint32_t now = (uint32_t)k * 1000u / HZ;
        const int link_up = wall || now < 2000u;
        if (link_up && k % 5 == 0) {
            last_cmd = now;            /* the browser sends a command every 100 ms */
        }
        sensors_t s = {distance, distance <= 400.0f, false, false, 7.6f};
        const drive_cmd_t joy = {joystick, 0.0f};
        const drive_cmd_t cmd = autopilot_step(&ap, &s, joy, now, last_cmd);
        const wheels_t w = drive_mix(cmd);
        wheel = drive_ramp(wheel, w.left, RAMP_STEP);
        printf("%.3f,%.2f,%.2f,%.3f,%.3f,%s\n", now / 1000.0, distance, link_up ? joystick : 0.0,
               cmd.throttle, wheel, autopilot_stop_name(ap.stop));
        if (wall) {
            distance -= wheel * TOP_SPEED_CM_S / HZ;
        }
    }
    return 0;
}
