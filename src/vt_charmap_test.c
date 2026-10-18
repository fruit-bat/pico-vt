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
/* Build and run:
 *
 * ./vt_charmap_test.sh
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "vt_charmap.h"

static void test_width() {
    printf("Testing width\n");
    assert(vt_charmap_width('A') == 1);
    assert(vt_charmap_width(0x1B) == 0);   // C0
    assert(vt_charmap_width(0x7F) == 0);   // DEL
    assert(vt_charmap_width(0x85) == 0);   // C1
    assert(vt_charmap_width(0xE9) == 1);   // e acute
    assert(vt_charmap_width(0x0301) == 0); // Combining acute
    assert(vt_charmap_width(0x200B) == 0); // Zero width space
    assert(vt_charmap_width(0xFE0F) == 0); // Variation selector
    assert(vt_charmap_width(0x2500) == 1); // Box drawing
    assert(vt_charmap_width(0x4E2D) == 2); // CJK
    assert(vt_charmap_width(0xAC00) == 2); // Hangul
    assert(vt_charmap_width(0xFF21) == 2); // Fullwidth A
    assert(vt_charmap_width(0x1F600) == 2); // Emoji
    assert(vt_charmap_width(0x20000) == 2); // CJK extension B
    assert(vt_charmap_width(0x10FFFF) == 1);
}

static void test_dec_graphics() {
    printf("Testing DEC special graphics\n");
    assert(vt_charmap_dec_graphics('A') == 'A');
    assert(vt_charmap_dec_graphics(0x5E) == 0x5E);
    assert(vt_charmap_dec_graphics(0x5F) == 0x00A0);
    assert(vt_charmap_dec_graphics('j') == 0x2518);
    assert(vt_charmap_dec_graphics('l') == 0x250C);
    assert(vt_charmap_dec_graphics('q') == 0x2500);
    assert(vt_charmap_dec_graphics('x') == 0x2502);
    assert(vt_charmap_dec_graphics('~') == 0x00B7);
}

static void test_ascii() {
    printf("Testing ASCII map\n");
    // Printable ASCII is unchanged; everything else is printable ASCII
    for (uint32_t c = 0x20; c < 0x7F; ++c) assert(vt_charmap_ascii(c) == c);
    for (uint32_t c = 0; c < 0x30000; ++c) {
        const vt_char_t g = vt_charmap_ascii(c);
        assert(g >= 0x20 && g < 0x7F);
    }
    assert(vt_charmap_ascii(0x1F) == '?');
    assert(vt_charmap_ascii(0x7F) == '?');
    assert(vt_charmap_ascii(0xFFFD) == '?');

    // Accents are removed
    assert(vt_charmap_ascii(0xE9) == 'e');
    assert(vt_charmap_ascii(0xC7) == 'C');
    assert(vt_charmap_ascii(0xDF) == 's');
    assert(vt_charmap_ascii(0xFF) == 'y');

    // Box drawing
    assert(vt_charmap_ascii(0x2500) == '-');
    assert(vt_charmap_ascii(0x2550) == '-');
    assert(vt_charmap_ascii(0x2502) == '|');
    assert(vt_charmap_ascii(0x2551) == '|');
    assert(vt_charmap_ascii(0x250C) == '+');
    assert(vt_charmap_ascii(0x256C) == '+');
    assert(vt_charmap_ascii(0x2571) == '/');
    assert(vt_charmap_ascii(0x2572) == '\\');
    assert(vt_charmap_ascii(0x2588) == '#');

    // Every DEC special graphics character (0x5F-0x7E) has the ncurses ASCII fallback:
    // blank, diamond, checkerboard, HT FF CR LF symbols, degree, plus/minus, NL VT symbols,
    // corners, crossing, scan lines 1 3 5 7 9, tees, vertical line, <= >= pi != pound bullet
    static const char expect[32] = {
        ' ', '+', ':', '?', '?', '?', '?', '\'', '#', '#', '#',
        '+', '+', '+', '+', '+', '~', '-', '-', '-', '_',
        '+', '+', '+', '+', '|', '<', '>', '*', '!', 'f', 'o',
    };
    for (uint32_t i = 0; i < 32; ++i) {
        assert(vt_charmap_ascii(vt_charmap_dec_graphics(0x5F + i)) == expect[i]);
    }
}

int main() {
    test_width();
    test_dec_graphics();
    test_ascii();
    printf("all ok\n");
    return 0;
}
