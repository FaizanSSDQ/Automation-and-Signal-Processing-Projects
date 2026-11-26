/**
 * Host tool: run the firmware's command parser and DDS core, and write the
 * PWM compare values the timer would receive. Used by the Python analysis
 * (spectra, THD, anti-aliasing) and the README figures.
 *
 * Usage:  dump_wave <samples> "<command>" ["<command>" ...]  > out.bin
 * Output: little-endian uint16 pairs (waveform, sync) per sample.
 *
 * Example: dump_wave 125000 "wave saw" "freq 5k" "aa off"
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cli.h"
#include "dds.h"
#include "gen_config.h"

int main(int argc, char **argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (argc < 2) {
        fprintf(stderr, "usage: %s <samples> [commands...]\n", argv[0]);
        return 2;
    }
    const long n = strtol(argv[1], NULL, 10);
    if (n <= 0 || n > 50000000) {
        fprintf(stderr, "samples out of range\n");
        return 2;
    }

    dds_settings_t s;
    dds_settings_default(&s);
    for (int i = 2; i < argc; i++) {
        char line[CLI_LINE_MAX], reply[512];
        snprintf(line, sizeof(line), "%s", argv[i]);
        if (cli_execute(line, &s, reply, sizeof(reply)) == CLI_ERROR) {
            fprintf(stderr, "%s: %s\n", argv[i], reply);
            return 1;
        }
    }

    dds_t d;
    dds_init(&d, &s);
    uint16_t block[2u * GEN_BLOCK];
    for (long done = 0; done < n; done += GEN_BLOCK) {
        const uint32_t k = (uint32_t)((n - done < (long)GEN_BLOCK) ? n - done : (long)GEN_BLOCK);
        dds_render(&d, block, k);
        fwrite(block, sizeof(uint16_t), 2u * k, stdout); /* host is little-endian */
    }
    return 0;
}
