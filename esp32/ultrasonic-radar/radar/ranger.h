/**
 * @file    ranger.h
 * @brief   HC-SR04 echo timing by interrupt, Sharp IR reading, servo output.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef RANGER_H
#define RANGER_H

#include <stdint.h>

void ranger_begin();

/** Fire one ping and return the echo width in us, or 0 on timeout. Blocks only the calling task. */
uint32_t ranger_ping_us();

/** Average of SHARP_SAMPLES readings of the Sharp output, in volts. */
float ranger_sharp_volts();

/** Move the servo to an angle, 0..180 degrees. */
void servo_write(int angle);

#endif /* RANGER_H */
