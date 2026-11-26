/**
 * @file    node.h
 * @brief   Sensor node role: measure, send one report, deep sleep.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef NODE_H
#define NODE_H

/** Runs once per wake-up and ends in deep sleep (never returns). */
void node_run();

#endif /* NODE_H */
