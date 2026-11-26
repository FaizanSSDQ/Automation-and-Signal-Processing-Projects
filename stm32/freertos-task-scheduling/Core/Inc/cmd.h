/**
 * @file    cmd.h
 * @brief   Console command parser for the scheduling demo (no RTOS dependency).
 *
 *   top                    task table for the last second
 *   watch on|off           print the table every second
 *   load <0..100>          CPU load of the worker task in percent
 *   prio <task> <level>    level: low | below | normal | above
 *   suspend <task>         resume <task>
 *   help
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CMD_H
#define CMD_H

/* CMSIS-RTOS2 priority values (identical to FreeRTOS priorities in this port). */
#define PRIO_IDLE         1
#define PRIO_LOW          8
#define PRIO_BELOW_NORMAL 16
#define PRIO_NORMAL       24
#define PRIO_ABOVE_NORMAL 32
#define PRIO_HIGH         40
#define PRIO_REALTIME     48

typedef enum {
    CMD_EMPTY = 0,
    CMD_HELP,
    CMD_TOP,
    CMD_WATCH,
    CMD_LOAD,
    CMD_PRIO,
    CMD_SUSPEND,
    CMD_RESUME,
    CMD_ERROR
} cmd_type_t;

typedef struct {
    cmd_type_t type;
    int task;            /* index into the task name list, or -1 */
    int value;           /* load %, priority, or watch on (1) / off (0) */
    const char *error;   /* message when type == CMD_ERROR */
} cmd_t;

/**
 * Parse one line (modified in place). @p names lists the tasks that commands
 * may address; @p n_names its length.
 */
cmd_t cmd_parse(char *line, const char *const *names, int n_names);

/** "low" -> PRIO_LOW ... "above" -> PRIO_ABOVE_NORMAL; -1 if not a user-settable level. */
int cmd_prio_from_name(const char *name);

/** Short name of any CMSIS priority value ("idle", "low", ... "realtime", or "p<n>"). */
const char *cmd_prio_name(int prio);

#endif /* CMD_H */
