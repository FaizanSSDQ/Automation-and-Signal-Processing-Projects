/**
 * Print the DAC codes the generator outputs, one sample per line, for the
 * README figures:  dump_wave <sine|square|triangle|saw|noise> <freq_hz> <antialias 0/1> <samples>
 *
 * Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include <stdio.h>
#include <stdlib.h>

#include "../wavegen/src/core/dds.h"

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: dump_wave <wave> <freq_hz> <antialias> <samples>\n");
        return 2;
    }
    dds_settings_t s;
    dds_settings_default(&s);
    if (!dds_set(&s, "wave", argv[1]) || !dds_set(&s, "freq", argv[2]) || !dds_set(&s, "antialias", argv[3])) {
        fprintf(stderr, "bad arguments\n");
        return 2;
    }
    s.amp_mv = 3000u;
    dds_t d;
    dds_init(&d, &s);
    const long n = atol(argv[4]);
    uint8_t pair[2];
    for (long i = 0; i < n; i++) {
        dds_render(&d, pair, 1);
        printf("%u %u\n", pair[0], pair[1]);
    }
    return 0;
}
