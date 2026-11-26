/**
 * @file    cmd.c
 * @brief   Console command parser for the priority-inversion demo.
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
    } else if (strcmp(cmd, "mode") == 0) {
        if (argc < 2) {
            return fail("mode semaphore|mutex");
        }
        if (strcmp(argv[1], "mutex") == 0 || strcmp(argv[1], "m") == 0) {
            c.value = 1;
        } else if (strcmp(argv[1], "semaphore") == 0 || strcmp(argv[1], "sem") == 0 || strcmp(argv[1], "s") == 0) {
            c.value = 0;
        } else {
            return fail("mode semaphore|mutex");
        }
        c.type = CMD_MODE;
    } else if (strcmp(cmd, "medium") == 0) {
        if (argc < 2 || !parse_range(argv[1], 0, CMD_MEDIUM_MAX_MS, &c.value)) {
            return fail("medium 0..60 (ms)");
        }
        c.type = CMD_MEDIUM;
    } else if (strcmp(cmd, "hold") == 0) {
        if (argc < 2 || !parse_range(argv[1], CMD_HOLD_MIN_MS, CMD_HOLD_MAX_MS, &c.value)) {
            return fail("hold 1..20 (ms)");
        }
        c.type = CMD_HOLD;
    } else if (strcmp(cmd, "watch") == 0) {
        if (argc < 2 || (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0)) {
            return fail("watch on|off");
        }
        c.type = CMD_WATCH;
        c.value = strcmp(argv[1], "on") == 0;
    } else {
        return fail("unknown command (type help)");
    }
    return c;
}
