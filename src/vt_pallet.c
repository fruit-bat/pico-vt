/* Copyright (c) 2025 fruit-bat
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the copyright holder nor the
 *     names of contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS,
 * COPYRIGHT HOLDERS, OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "vt_pallet.h"

// https://en.wikipedia.org/wiki/ANSI_escape_code
void vt_pallet_make_256_colours(
    void *d,
    vt_pallet_make_cb_t vt_pallet_make_cb
) {
    // colors 0 - 7  Standard colours
    // colors 8 - 15 Bright colours
    static uint8_t p16[16][3] = {
        {0,  0,  0  }, // Black
        {170,0,  0  }, // Red
        {0,  170,0  }, // Green
        {170,85, 0  }, // Yellow
        {0,  0,  170}, // Blue
        {170,0,  170}, // Magenta
        {0,  170,170}, // Cyan
        {170,170,170}, // White
        {85, 85, 85 }, // Bright Black
        {255,85, 85 }, // Bright Red
        {85, 255,85 }, // Bright Green
        {255,255,85 }, // Bright Yellow
        {85, 85, 255}, // Bright Blue
        {255,85, 255}, // Bright Magenta
        {85, 255,255}, // Bright Cyan
        {255,255,255}, // Bright White
    };

    for(uint8_t i = 0; i < 16; ++i) {
        vt_pallet_make_cb(
            d, 
            (vt_cell_colour_t)i, 
            p16[i][0], 
            p16[i][1], 
            p16[i][2]
        );
    }
 
    // colors 16-231 are a 6x6x6 color cube
    for (uint8_t r = 0; r < 6; ++r) {
        for (uint8_t g = 0; g < 6; ++g) {
            for (uint8_t b = 0; b < 6; ++b) {
                uint32_t i = 16 + (r * 36) + (g * 6) + b;
                vt_pallet_make_cb(
                    d, 
                    (vt_cell_colour_t)i, 
                    r ? r * 40 + 55 : 0, 
                    g ? g * 40 + 55 : 0, 
                    b ? b * 40 + 55 : 0
                );
            }
        }
    }

    // colors 232-255 are a grayscale ramp, intentionally leaving out black and white
    for(uint8_t j = 0; j <  24; ++j) {
        uint32_t i = j + 232;
        uint8_t l = j * 10 + 8;
        vt_pallet_make_cb(
            d, 
            (vt_cell_colour_t)i, 
            l, 
            l, 
            l
        );
    }
}

// Index (0-5) of the nearest colour cube level (0, 95, 135, 175, 215, 255)
static uint32_t vt_pallet_cube_index(uint32_t v) {
    return v < 48 ? 0 : v < 115 ? 1 : (v - 35) / 40;
}

static uint32_t vt_pallet_cube_level(uint32_t i) {
    return i ? i * 40 + 55 : 0;
}

static uint32_t vt_pallet_dist2(uint32_t r1, uint32_t g1, uint32_t b1, uint32_t r2, uint32_t g2, uint32_t b2) {
    const int32_t dr = (int32_t)r1 - (int32_t)r2;
    const int32_t dg = (int32_t)g1 - (int32_t)g2;
    const int32_t db = (int32_t)b1 - (int32_t)b2;
    return (uint32_t)(dr * dr + dg * dg + db * db);
}

vt_cell_colour_t vt_pallet_rgb_to_256(
    uint32_t r,
    uint32_t g,
    uint32_t b
) {
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    // Nearest colour in the cube
    const uint32_t ri = vt_pallet_cube_index(r);
    const uint32_t gi = vt_pallet_cube_index(g);
    const uint32_t bi = vt_pallet_cube_index(b);
    const uint32_t cr = vt_pallet_cube_level(ri);
    const uint32_t cg = vt_pallet_cube_level(gi);
    const uint32_t cb = vt_pallet_cube_level(bi);

    // Nearest grey in the ramp (8, 18, ... 238)
    const uint32_t a = (r + g + b) / 3;
    const uint32_t yi = a < 8 ? 0 : a > 238 ? 23 : (a - 3) / 10;
    const uint32_t y = yi * 10 + 8;

    const uint32_t dc = vt_pallet_dist2(r, g, b, cr, cg, cb);
    const uint32_t dy = vt_pallet_dist2(r, g, b, y, y, y);
    return (vt_cell_colour_t)(dy < dc ? 232 + yi : 16 + ri * 36 + gi * 6 + bi);
}
