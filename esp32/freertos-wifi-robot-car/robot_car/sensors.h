/**
 * @file    sensors.h
 * @brief   Reading the car's sensors: ultrasonic, Sharp infrared, line sensors, battery.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "src/core/autopilot.h"

void sensors_begin();

/** One complete reading (takes up to ~25 ms for the ultrasonic echo). */
sensors_t sensors_read();

#endif /* SENSORS_H */
