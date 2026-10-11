#include "str_disc.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    char out[1024];
    unsigned long long min_bytes;

    if (argc != 3) {
        return 2;
    }
    min_bytes = strtoull(argv[2], NULL, 10);
    if (dw3_str_find_disc(argv[1], out, sizeof(out), min_bytes) != 0) {
        return 1;
    }
    puts(out);
    return 0;
}
