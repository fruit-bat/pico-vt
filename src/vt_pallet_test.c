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
/* Build instructions:
 *
 * ./vt_tabs_test.sh
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "vt_pallet.h"
#include <string.h>
#include <assert.h>

typedef unsigned long lu;

void vt_pallet_make_cb(
    void *d,
    vt_cell_colour_t i,
    uint32_t r,
    uint32_t g,
    uint32_t b
) {
    printf("%lu %03lu %03lu %03lu\n", (lu)i, (lu)r, (lu)g, (lu)b);
    uint8_t (*rgb)[3] = d;
    rgb[i][0] = r;
    rgb[i][1] = g;
    rgb[i][2] = b;
}

// Exact palette colours map back to themselves
void test_rgb_to_256_exact(uint8_t rgb[256][3]) {
    for (uint32_t i = 16; i < 256; ++i) {
        assert(vt_pallet_rgb_to_256(rgb[i][0], rgb[i][1], rgb[i][2]) == i);
    }
}

void test_rgb_to_256_nearest() {
    assert(vt_pallet_rgb_to_256(0, 0, 0) == 16);
    assert(vt_pallet_rgb_to_256(255, 255, 255) == 231);
    assert(vt_pallet_rgb_to_256(255, 0, 0) == 196);
    assert(vt_pallet_rgb_to_256(250, 5, 10) == 196);         // Nearly red
    assert(vt_pallet_rgb_to_256(100, 100, 100) == 241);      // Grey 98 beats cube 95 (not grey 108)
    assert(vt_pallet_rgb_to_256(128, 128, 128) == 244);      // Grey ramp 128
    assert(vt_pallet_rgb_to_256(10, 10, 12) == 232);         // Near black is grey 8, not cube 0
    assert(vt_pallet_rgb_to_256(1000, 1000, 1000) == 231);   // Out of range values clamp
}

int main() {
    uint8_t rgb[256][3];
    vt_pallet_make_256_colours(rgb, vt_pallet_make_cb);

    // Bright cyan was previously yellow
    assert(rgb[14][0] == 85 && rgb[14][1] == 255 && rgb[14][2] == 255);

    test_rgb_to_256_exact(rgb);
    test_rgb_to_256_nearest();
    printf("all ok\n");
}
