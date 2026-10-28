/* The one translation unit that holds the vendored stb_image (public domain /
 * MIT, third_party/stb_image.h, v2.30), PNG only, memory input only. Kept apart
 * from mod_pack.c so the decoder's own style does not meet the port's warning
 * flags. */
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcomment"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <stddef.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 8192
#include "third_party/stb_image.h"

unsigned char *dw3_mod_stb_decode(const unsigned char *data, size_t len,
                                  int *w, int *h);
void dw3_mod_stb_free(void *p);

unsigned char *dw3_mod_stb_decode(const unsigned char *data, size_t len,
                                  int *w, int *h) {
    int comp = 0;

    if (len > 0x7fffffffu) return (unsigned char *)0;
    return stbi_load_from_memory(data, (int)len, w, h, &comp, 4);
}

void dw3_mod_stb_free(void *p) { stbi_image_free(p); }
