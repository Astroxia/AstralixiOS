#pragma once

#include <stdint.h>

#define GLYPH_HEIGHT (10)

typedef struct {
    uint8_t width;
    const uint8_t glyphs[128 * GLYPH_HEIGHT];
} font_t;

extern const font_t font_8x10;
extern const font_t font_5x10;