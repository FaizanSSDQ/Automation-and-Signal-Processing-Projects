/**
 * @file    motors.h
 * @brief   Two L298N drivers, four DC motors, driven as left and right sides.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef MOTORS_H
#define MOTORS_H

/** Configure direction pins and the four PWM channels; motors stopped. */
bool motors_begin();

/** Apply wheel speeds in -1 .. 1 to the left pair and the right pair. */
void motors_set(float left, float right);

/** Coast all four motors. */
void motors_stop();

#endif /* MOTORS_H */
