/**
 * @file    cmd.c
 * @brief   Console command parser for the periodic-task demo.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "cmd.h"

#include <stdlib.h>
#include <string.h>

#define MAX_ARGS 3

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

static cmd_t fail(const char *msg)
{
    cmd_t c = {CMD_ERROR, 0, msg};
    return c;
}

static int parse_range(const char *s, int lo, int hi, int *out)
{
    char *end;
    const long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < lo || v > hi) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

cmd_t cmd_parse(char *line)
{
    char *argv[MAX_ARGS];
    const int argc = split(line, argv, MAX_ARGS);
    cmd_t c = {CMD_EMPTY, 0, NULL};
    if (argc == 0) {
        return c;
    }
    const char *cmd = argv[0];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        c.type = CMD_HELP;
    } else if (strcmp(cmd, "report") == 0 || strcmp(cmd, "r") == 0) {
        c.type = CMD_REPORT;
    } else if (strcmp(cmd, "reset") == 0) {
        c.type = CMD_RESET;
    } else if (strcmp(cmd, "watch") == 0) {
        if (argc < 2 || (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)) {
            return fail("watch on|off");
        }
        c.type = CMD_WATCH;
        c.value = strcmp(argv[1], "on") == 0;
    } else if (strcmp(cmd, "work") == 0) {
        if (argc < 2 || !parse_range(argv[1], 0, CMD_WORK_MAX_MS, &c.value)) {
            return fail("work 0..8 (ms)");
        }
        c.type = CMD_WORK;
    } else if (strcmp(cmd, "burst") == 0) {
        if (argc < 2 || !parse_range(argv[1], 0, CMD_BURST_MAX_MS, &c.value)) {
            return fail("burst 0..9 (ms)");
        }
        c.type = CMD_BURST;
    } else {
        return fail("unknown command (type help)");
    }
    return c;
}
