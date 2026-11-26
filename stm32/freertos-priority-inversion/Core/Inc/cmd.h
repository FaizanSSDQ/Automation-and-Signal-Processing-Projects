/**
 * @file    cmd.h
 * @brief   Console command parser for the priority-inversion demo (no RTOS dependency).
 *
 *   mode semaphore|mutex   lock type shared by the low and high tasks
 *   medium <0..60>         CPU time of the medium task per cycle, in ms
 *   hold <1..20>           how long the low task holds the lock, in ms
 *   report                 blocking-time statistics for both lock types
 *   watch on|off           print one line per cycle
 *   reset | help
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CMD_H
#define CMD_H

#define CMD_MEDIUM_MAX_MS 60
#define CMD_HOLD_MIN_MS   1
#define CMD_HOLD_MAX_MS   20

typedef enum {
    CMD_EMPTY = 0,
    CMD_HELP,
    CMD_MODE,
    CMD_MEDIUM,
    CMD_HOLD,
    CMD_REPORT,
    CMD_WATCH,
    CMD_RESET,
    CMD_ERROR
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    int value;          /* ms, watch on/off, or mode (0 semaphore, 1 mutex) */
    const char *error;
} cmd_t;

cmd_t cmd_parse(char *line);

#endif /* CMD_H */
