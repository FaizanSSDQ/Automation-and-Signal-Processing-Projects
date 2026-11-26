/**
 * @file    cli.h
 * @brief   Text command interface for the waveform generator.
 *
 *   wave sine|square|triangle|saw|noise     freq 1500 | freq 2.5k
 *   amp 2.0        (Vpp)                     offset 1.65   (V)
 *   duty 25        (% for square)            aa on|off     (anti-aliasing)
 *   sweep 100 10k 2 [lin|log]  (start Hz, stop Hz, seconds)   sweep off
 *   on | off       status | help
 *
 * Numbers accept decimals and a k suffix (x1000). Parsing is done in integer
 * milli-units, so no floating point is needed.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dds.h"

#define CLI_LINE_MAX 64u

typedef struct {
    char buf[CLI_LINE_MAX];
    uint8_t len;
    bool overflow;
} cli_line_t;

typedef enum {
    CLI_EMPTY = 0,   /* blank line */
    CLI_CHANGED,     /* settings changed: apply them; reply holds a confirmation */
    CLI_INFO,        /* reply holds information (status, help) */
    CLI_ERROR        /* reply holds an error message; settings untouched */
} cli_result_t;

void cli_line_reset(cli_line_t *l);

/**
 * Add a received character. Handles backspace; CR or LF ends the line.
 * @return true when a complete line is ready in l->buf (NUL-terminated).
 */
bool cli_line_feed(cli_line_t *l, char c);

/** Execute one command line (modified in place) against the settings. */
cli_result_t cli_execute(char *line, dds_settings_t *s, char *reply, size_t cap);

/** One-line summary of the settings. */
void cli_format_status(const dds_settings_t *s, char *out, size_t cap);

/** Parse "12", "2.5", "1.2k" into milli-units (2.5 -> 2500). */
bool cli_parse_milli(const char *text, uint64_t *milli);

/** "1234.500" from 1234500 with 3 decimals; @p decimals is 0..3. */
void cli_format_milli(char *out, size_t cap, uint64_t milli, unsigned decimals);

const char *cli_wave_name(dds_wave_t w);

#endif /* CLI_H */
