/**
 * @file    cmd.c
 * @brief   Console command parser for the scheduling demo.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARGS 4

static const struct {
    const char *name;
    int prio;
    int user;  /* may be set with the prio command */
} prio_names[] = {
    {"idle", PRIO_IDLE, 0},
    {"low", PRIO_LOW, 1},
    {"below", PRIO_BELOW_NORMAL, 1},
    {"normal", PRIO_NORMAL, 1},
    {"above", PRIO_ABOVE_NORMAL, 1},
    {"high", PRIO_HIGH, 0},
    {"realtime", PRIO_REALTIME, 0},
};

int cmd_prio_from_name(const char *name)
{
    for (size_t i = 0; i < sizeof(prio_names) / sizeof(prio_names[0]); i++) {
        if (prio_names[i].user && strcmp(name, prio_names[i].name) == 0) {
            return prio_names[i].prio;
        }
    }
    return -1;
}

const char *cmd_prio_name(int prio)
{
    static char other[8];
    for (size_t i = 0; i < sizeof(prio_names) / sizeof(prio_names[0]); i++) {
        if (prio_names[i].prio == prio) {
            return prio_names[i].name;
        }
    }
    snprintf(other, sizeof(other), "p%d", prio);
    return other;
}

static int split(char *line, char *argv[], int max)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t') {
            *p++ = '\0';
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') {
            if (*p >= 'A' && *p <= 'Z') {
                *p = (char)(*p - 'A' + 'a');
            }
            p++;
        }
    }
    return argc;
}

static int find_task(const char *name, const char *const *names, int n)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(name, names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static cmd_t fail(const char *msg)
{
    cmd_t c = {CMD_ERROR, -1, 0, msg};
    return c;
}

static int parse_int(const char *s, int *out)
{
    char *end;
    const long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return 0;
    }
    *out = (int)v;
    return 1;
}

cmd_t cmd_parse(char *line, const char *const *names, int n_names)
{
    char *argv[MAX_ARGS];
    const int argc = split(line, argv, MAX_ARGS);
    cmd_t c = {CMD_EMPTY, -1, 0, NULL};
    if (argc == 0) {
        return c;
    }
    const char *cmd = argv[0];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        c.type = CMD_HELP;
    } else if (strcmp(cmd, "top") == 0) {
        c.type = CMD_TOP;
    } else if (strcmp(cmd, "watch") == 0) {
        if (argc < 2 || (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)) {
            return fail("watch on|off");
        }
        c.type = CMD_WATCH;
        c.value = strcmp(argv[1], "on") == 0;
    } else if (strcmp(cmd, "load") == 0) {
        if (argc < 2 || !parse_int(argv[1], &c.value) || c.value < 0 || c.value > 100) {
            return fail("load 0..100");
        }
        c.type = CMD_LOAD;
    } else if (strcmp(cmd, "prio") == 0) {
        if (argc < 3) {
            return fail("prio <task> low|below|normal|above");
        }
        c.task = find_task(argv[1], names, n_names);
        if (c.task < 0) {
            return fail("unknown task");
        }
        c.value = cmd_prio_from_name(argv[2]);
        if (c.value < 0) {
            return fail("priority must be low, below, normal or above");
        }
        c.type = CMD_PRIO;
    } else if (strcmp(cmd, "suspend") == 0 || strcmp(cmd, "resume") == 0) {
        if (argc < 2) {
            return fail("suspend|resume <task>");
        }
        c.task = find_task(argv[1], names, n_names);
        if (c.task < 0) {
            return fail("unknown task");
        }
        c.type = (cmd[0] == 's') ? CMD_SUSPEND : CMD_RESUME;
    } else {
        return fail("unknown command (type help)");
    }
    return c;
}
