/**
 * @file    cli.c
 * @brief   Text command interface for the waveform generator.
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "gen_config.h"

#define MAX_ARGS 6

static const char *const wave_names[DDS_WAVE_COUNT] = {"sine", "square", "triangle", "saw", "noise"};

static const char help_text[] =
    "commands:\r\n"
    "  wave sine|square|triangle|saw|noise\r\n"
    "  freq <Hz>            0.01 .. 20k, e.g. freq 1.5k\r\n"
    "  amp <Vpp>            0 .. 3.3\r\n"
    "  offset <V>           0 .. 3.3 (centre of the waveform)\r\n"
    "  duty <%>             1 .. 99 (square)\r\n"
    "  sweep <f1> <f2> <s> [lin|log]   repeat a sweep;  sweep off\r\n"
    "  aa on|off            anti-aliasing of square/saw edges\r\n"
    "  on | off             enable / disable the output\r\n"
    "  status | help";

const char *cli_wave_name(dds_wave_t w)
{
    return (w < DDS_WAVE_COUNT) ? wave_names[w] : "?";
}

void cli_line_reset(cli_line_t *l)
{
    l->len = 0u;
    l->overflow = false;
    l->buf[0] = '\0';
}

bool cli_line_feed(cli_line_t *l, char c)
{
    if (c == '\r' || c == '\n') {
        l->buf[l->len] = '\0';
        return true;
    }
    if (c == '\b' || c == 0x7F) {
        if (l->len > 0u) {
            l->len--;
        }
        return false;
    }
    if (l->len < CLI_LINE_MAX - 1u) {
        l->buf[l->len++] = c;
    } else {
        l->overflow = true;
    }
    return false;
}

bool cli_parse_milli(const char *text, uint64_t *milli)
{
    uint64_t whole = 0u, frac = 0u, scale = 1000000u; /* parse to micro-units */
    const char *p = text;
    bool digits = false;

    while (*p >= '0' && *p <= '9') {
        whole = whole * 10u + (uint64_t)(*p - '0');
        if (whole > 1000000000u) {
            return false;
        }
        digits = true;
        p++;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            if (scale > 1u) {
                scale /= 10u;
                frac += (uint64_t)(*p - '0') * scale;
            }
            digits = true;
            p++;
        }
    }
    if (!digits) {
        return false;
    }
    uint64_t micro = whole * 1000000u + frac;
    if (*p == 'k' || *p == 'K') {
        micro *= 1000u;
        p++;
    }
    if (*p != '\0') {
        return false;
    }
    *milli = (micro + 500u) / 1000u;
    return true;
}

void cli_format_milli(char *out, size_t cap, uint64_t milli, unsigned decimals)
{
    static const uint32_t div[4] = {1000u, 100u, 10u, 1u};
    if (decimals > 3u) {
        decimals = 3u;
    }
    const uint64_t rounded = (milli + div[decimals] / 2u) / div[decimals];
    if (decimals == 0u) {
        snprintf(out, cap, "%lu", (unsigned long)rounded);
        return;
    }
    static const uint32_t pow10[4] = {1u, 10u, 100u, 1000u};
    snprintf(out, cap, "%lu.%0*lu", (unsigned long)(rounded / pow10[decimals]), (int)decimals,
             (unsigned long)(rounded % pow10[decimals]));
}

static void lowercase(char *s)
{
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z') {
            *s = (char)(*s - 'A' + 'a');
        }
    }
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
            p++;
        }
    }
    return argc;
}

/* Strip a unit suffix such as "hz", "v" or "%" so "1khz" and "2.5v" also work. */
static void strip_suffix(char *arg, const char *unit)
{
    const size_t n = strlen(arg), u = strlen(unit);
    if (n > u && strcmp(arg + n - u, unit) == 0) {
        arg[n - u] = '\0';
    }
}

static bool parse_freq(char *arg, uint32_t *mhz)
{
    uint64_t v;
    strip_suffix(arg, "hz");
    if (!cli_parse_milli(arg, &v) || v < GEN_FREQ_MIN_MHZ || v > GEN_FREQ_MAX_MHZ) {
        return false;
    }
    *mhz = (uint32_t)v;
    return true;
}

static bool parse_volts(char *arg, uint16_t *mv)
{
    uint64_t v;
    strip_suffix(arg, "v");
    if (!cli_parse_milli(arg, &v) || v > GEN_VREF_MV) {
        return false;
    }
    *mv = (uint16_t)v;
    return true;
}

static bool parse_on_off(const char *arg, bool *value)
{
    if (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0) {
        *value = true;
        return true;
    }
    if (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0) {
        *value = false;
        return true;
    }
    return false;
}

static bool clips(const dds_settings_t *s)
{
    const uint32_t half = s->amp_mv / 2u;
    return half > s->offset_mv || s->offset_mv + half > GEN_VREF_MV;
}

void cli_format_status(const dds_settings_t *s, char *out, size_t cap)
{
    char f[20], a[12], o[12];
    cli_format_milli(f, sizeof(f), s->freq_mhz, 3);
    cli_format_milli(a, sizeof(a), s->amp_mv, 3);
    cli_format_milli(o, sizeof(o), s->offset_mv, 3);
    int n = (s->sweep == DDS_SWEEP_OFF)
                ? snprintf(out, cap, "%s %s Hz, %s Vpp, offset %s V", cli_wave_name(s->wave), f, a, o)
                : snprintf(out, cap, "%s, %s Vpp, offset %s V", cli_wave_name(s->wave), a, o);
    if (n < 0 || (size_t)n >= cap) {
        return;
    }
    if (s->wave == DDS_SQUARE) {
        n += snprintf(out + n, cap - (size_t)n, ", duty %u %%", (unsigned)s->duty_pct);
    }
    if ((size_t)n < cap && s->sweep != DDS_SWEEP_OFF) {
        char f1[20], f2[20], t[16];
        cli_format_milli(f1, sizeof(f1), s->sweep_start_mhz, 2);
        cli_format_milli(f2, sizeof(f2), s->sweep_stop_mhz, 2);
        cli_format_milli(t, sizeof(t), s->sweep_ms, 3);
        n += snprintf(out + n, cap - (size_t)n, ", %s sweep %s -> %s Hz in %s s",
                      s->sweep == DDS_SWEEP_LOG ? "log" : "lin", f1, f2, t);
    }
    if ((size_t)n < cap) {
        snprintf(out + n, cap - (size_t)n, ", anti-alias %s, output %s%s", s->antialias ? "on" : "off",
                 s->output_on ? "on" : "off", clips(s) ? " (clipping at 0 / 3.3 V)" : "");
    }
}

static cli_result_t error(char *reply, size_t cap, const char *msg)
{
    snprintf(reply, cap, "error: %s", msg);
    return CLI_ERROR;
}

cli_result_t cli_execute(char *line, dds_settings_t *s, char *reply, size_t cap)
{
    char *argv[MAX_ARGS];
    lowercase(line);
    const int argc = split(line, argv, MAX_ARGS);
    reply[0] = '\0';
    if (argc == 0) {
        return CLI_EMPTY;
    }

    dds_settings_t n = *s; /* work on a copy: an error leaves the settings untouched */
    const char *cmd = argv[0];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        snprintf(reply, cap, "%s", help_text);
        return CLI_INFO;
    }
    if (strcmp(cmd, "status") == 0) {
        cli_format_status(s, reply, cap);
        return CLI_INFO;
    }
    if (strcmp(cmd, "on") == 0 || strcmp(cmd, "off") == 0) {
        n.output_on = (cmd[1] == 'n');
    } else if (strcmp(cmd, "wave") == 0 || strcmp(cmd, "w") == 0) {
        if (argc < 2) {
            return error(reply, cap, "wave sine|square|triangle|saw|noise");
        }
        int found = -1;
        for (int i = 0; i < (int)DDS_WAVE_COUNT; i++) {
            if (strncmp(argv[1], wave_names[i], 3) == 0) { /* "sq", "tri", "saw" ... */
                found = i;
            }
        }
        if (found < 0 || strlen(argv[1]) < 3u) {
            return error(reply, cap, "unknown waveform");
        }
        n.wave = (dds_wave_t)found;
    } else if (strcmp(cmd, "freq") == 0 || strcmp(cmd, "f") == 0) {
        if (argc < 2 || !parse_freq(argv[1], &n.freq_mhz)) {
            return error(reply, cap, "freq 0.01 .. 20k");
        }
        n.sweep = DDS_SWEEP_OFF; /* a fixed frequency ends a sweep */
    } else if (strcmp(cmd, "amp") == 0 || strcmp(cmd, "a") == 0) {
        if (argc < 2 || !parse_volts(argv[1], &n.amp_mv)) {
            return error(reply, cap, "amp 0 .. 3.3 (Vpp)");
        }
    } else if (strcmp(cmd, "offset") == 0 || strcmp(cmd, "o") == 0) {
        if (argc < 2 || !parse_volts(argv[1], &n.offset_mv)) {
            return error(reply, cap, "offset 0 .. 3.3 (V)");
        }
    } else if (strcmp(cmd, "duty") == 0 || strcmp(cmd, "d") == 0) {
        uint64_t v;
        if (argc < 2) {
            return error(reply, cap, "duty 1 .. 99");
        }
        strip_suffix(argv[1], "%");
        if (!cli_parse_milli(argv[1], &v) || v < 1000u || v > 99000u) {
            return error(reply, cap, "duty 1 .. 99");
        }
        n.duty_pct = (uint8_t)((v + 500u) / 1000u);
    } else if (strcmp(cmd, "aa") == 0) {
        if (argc < 2 || !parse_on_off(argv[1], &n.antialias)) {
            return error(reply, cap, "aa on|off");
        }
    } else if (strcmp(cmd, "sweep") == 0) {
        if (argc >= 2 && strcmp(argv[1], "off") == 0) {
            n.sweep = DDS_SWEEP_OFF;
        } else {
            uint64_t secs;
            if (argc < 4 || !parse_freq(argv[1], &n.sweep_start_mhz) ||
                !parse_freq(argv[2], &n.sweep_stop_mhz)) {
                return error(reply, cap, "sweep <f1> <f2> <seconds> [lin|log]");
            }
            strip_suffix(argv[3], "s");
            if (!cli_parse_milli(argv[3], &secs) || secs < GEN_SWEEP_MIN_MS || secs > GEN_SWEEP_MAX_MS) {
                return error(reply, cap, "sweep time 0.01 .. 600 s");
            }
            n.sweep_ms = (uint32_t)secs;
            n.sweep = DDS_SWEEP_LIN;
            if (argc >= 5) {
                if (strcmp(argv[4], "log") == 0) {
                    n.sweep = DDS_SWEEP_LOG;
                } else if (strcmp(argv[4], "lin") != 0) {
                    return error(reply, cap, "sweep mode lin|log");
                }
            }
        }
    } else {
        return error(reply, cap, "unknown command (type help)");
    }

    *s = n;
    char status[160];
    cli_format_status(s, status, sizeof(status));
    snprintf(reply, cap, "ok: %s", status);
    return CLI_CHANGED;
}
