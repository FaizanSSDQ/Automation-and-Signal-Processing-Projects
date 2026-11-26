/**
 * @file    cmd.h
 * @brief   Console command parser for the periodic-task demo (no RTOS dependency).
 *
 *   report               statistics of both periodic tasks
 *   watch on|off         print the report every 2 s
 *   work <0..8>          busy time of each periodic task per cycle, in ms
 *   burst <0..9>         busy time of the interference task every 37 ms, in ms
 *   reset                clear the statistics
 *   help
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CMD_H
#define CMD_H

#define CMD_WORK_MAX_MS   8
#define CMD_BURST_MAX_MS  9

typedef enum {
    CMD_EMPTY = 0,
    CMD_HELP,
    CMD_REPORT,
    CMD_WATCH,
    CMD_WORK,
    CMD_BURST,
    CMD_RESET,
    CMD_ERROR
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    int value;
    const char *error;
} cmd_t;

/** Parse one line (modified in place). */
cmd_t cmd_parse(char *line);

#endif /* CMD_H */
