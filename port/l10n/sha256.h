/* A small SHA-256 (FIPS 180-4) for the text pack's source check (P6.7). */
#ifndef DW3_L10N_SHA256_H
#define DW3_L10N_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct Dw3Sha256 {
    uint32_t h[8];
    uint64_t bytes;
    unsigned char buf[64];
    unsigned fill;
} Dw3Sha256;

void dw3_sha256_init(Dw3Sha256 *s);
void dw3_sha256_update(Dw3Sha256 *s, const void *data, size_t len);
/* 64 lower-case hex digits and a NUL into `hex`. */
void dw3_sha256_hex(Dw3Sha256 *s, char hex[65]);

#endif /* DW3_L10N_SHA256_H */
