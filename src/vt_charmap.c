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

#include <stddef.h>
#include "vt_charmap.h"

#define COUNT_ARR(X) ((sizeof (X))/(sizeof (X)[0]))

typedef struct {
    uint32_t lo;
    uint32_t hi;
} vt_charmap_range_t;

// Zero width: combining marks, zero width spaces/joiners, direction and
// variation selectors
static const vt_charmap_range_t vt_charmap_zero[] = {
    {0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD}, {0x0610, 0x061A},
    {0x064B, 0x065F}, {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF}, {0x200B, 0x200F},
    {0x202A, 0x202E}, {0x2060, 0x2064}, {0x20D0, 0x20FF}, {0xFE00, 0xFE0F},
    {0xFE20, 0xFE2F}, {0xFEFF, 0xFEFF},
};

// Double width: East Asian wide/fullwidth and emoji presentation
static const vt_charmap_range_t vt_charmap_wide[] = {
    {0x1100, 0x115F}, {0x231A, 0x231B}, {0x2329, 0x232A}, {0x23E9, 0x23EC},
    {0x23F0, 0x23F0}, {0x23F3, 0x23F3}, {0x25FD, 0x25FE}, {0x2614, 0x2615},
    {0x2648, 0x2653}, {0x267F, 0x267F}, {0x2693, 0x2693}, {0x26A1, 0x26A1},
    {0x26AA, 0x26AB}, {0x26BD, 0x26BE}, {0x26C4, 0x26C5}, {0x26CE, 0x26CE},
    {0x26D4, 0x26D4}, {0x26EA, 0x26EA}, {0x26F2, 0x26F3}, {0x26F5, 0x26F5},
    {0x26FA, 0x26FA}, {0x26FD, 0x26FD}, {0x2705, 0x2705}, {0x270A, 0x270B},
    {0x2728, 0x2728}, {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755},
    {0x2757, 0x2757}, {0x2795, 0x2797}, {0x27B0, 0x27B0}, {0x27BF, 0x27BF},
    {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55}, {0x2E80, 0x303E},
    {0x3041, 0x33FF}, {0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xA000, 0xA4CF},
    {0xA960, 0xA97F}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF}, {0xFE10, 0xFE19},
    {0xFE30, 0xFE6F}, {0xFF00, 0xFF60}, {0xFFE0, 0xFFE6}, {0x1F004, 0x1F004},
    {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E}, {0x1F191, 0x1F19A}, {0x1F200, 0x1F251},
    {0x1F300, 0x1F64F}, {0x1F680, 0x1F6FF}, {0x1F900, 0x1F9FF}, {0x1FA70, 0x1FAFF},
    {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD},
};

static bool vt_charmap_in(
    const vt_charmap_range_t *r,
    size_t n,
    uint32_t cp
) {
    // Binary search of sorted, non-overlapping ranges
    size_t lo = 0;
    size_t hi = n;
    while (lo < hi) {
        const size_t m = (lo + hi) / 2;
        if (cp < r[m].lo) hi = m;
        else if (cp > r[m].hi) lo = m + 1;
        else return true;
    }
    return false;
}

uint32_t vt_charmap_width(
    uint32_t cp
) {
    if (cp < 0x7F) return cp < 0x20 ? 0 : 1;
    if (cp < 0xA0) return 0; // DEL and C1 controls
    if (cp < 0x0300) return 1;
    if (vt_charmap_in(vt_charmap_zero, COUNT_ARR(vt_charmap_zero), cp)) return 0;
    if (vt_charmap_in(vt_charmap_wide, COUNT_ARR(vt_charmap_wide), cp)) return 2;
    return 1;
}

// DEC special graphics 0x5F-0x7E as Unicode
static const uint16_t vt_charmap_dec[32] = {
    0x00A0, // 5F blank
    0x25C6, // 60 diamond
    0x2592, // 61 checkerboard
    0x2409, // 62 HT symbol
    0x240C, // 63 FF symbol
    0x240D, // 64 CR symbol
    0x240A, // 65 LF symbol
    0x00B0, // 66 degree
    0x00B1, // 67 plus/minus
    0x2424, // 68 NL symbol
    0x240B, // 69 VT symbol
    0x2518, // 6A lower right corner
    0x2510, // 6B upper right corner
    0x250C, // 6C upper left corner
    0x2514, // 6D lower left corner
    0x253C, // 6E crossing lines
    0x23BA, // 6F scan line 1
    0x23BB, // 70 scan line 3
    0x2500, // 71 horizontal line (scan line 5)
    0x23BC, // 72 scan line 7
    0x23BD, // 73 scan line 9
    0x251C, // 74 left tee
    0x2524, // 75 right tee
    0x2534, // 76 bottom tee
    0x252C, // 77 top tee
    0x2502, // 78 vertical line
    0x2264, // 79 less than or equal
    0x2265, // 7A greater than or equal
    0x03C0, // 7B pi
    0x2260, // 7C not equal
    0x00A3, // 7D pound sterling
    0x00B7, // 7E centred dot
};

uint32_t vt_charmap_dec_graphics(
    vt_char_t ch
) {
    return (ch >= 0x5F && ch <= 0x7E) ? vt_charmap_dec[ch - 0x5F] : ch;
}

// Latin-1 letters 0xC0-0xFF without their accents
static const char vt_charmap_latin1[] =
    "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPs"
    "aaaaaaaceeeeiiiidnooooo/ouuuuypy";

// Box drawing (0x2500-0x257F) lines; other box drawing characters are '+'
static const uint8_t vt_charmap_box_h[] = {
    0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x4C, 0x4D, 0x50, 0x74, 0x76, 0x78, 0x7A, 0x7C, 0x7E,
};
static const uint8_t vt_charmap_box_v[] = {
    0x02, 0x03, 0x06, 0x07, 0x0A, 0x0B, 0x4E, 0x4F, 0x51, 0x75, 0x77, 0x79, 0x7B, 0x7D, 0x7F,
};

static bool vt_charmap_has(
    const uint8_t *a,
    size_t n,
    uint8_t v
) {
    for (size_t i = 0; i < n; ++i) if (a[i] == v) return true;
    return false;
}

typedef struct {
    uint32_t cp;
    char ch;
} vt_charmap_pair_t;

// Other symbols, sorted by code point. The DEC special graphics use the
// ncurses ACS ASCII fallbacks.
static const vt_charmap_pair_t vt_charmap_pairs[] = {
    {0x00A0, ' '},  // No break space
    {0x00A3, 'f'},  // Pound sterling (ACS_STERLING)
    {0x00AB, '<'},  // Left guillemet
    {0x00B0, '\''}, // Degree (ACS_DEGREE)
    {0x00B1, '#'},  // Plus/minus (ACS_PLMINUS)
    {0x00B7, 'o'},  // Centred dot (ACS_BULLET)
    {0x00BB, '>'},  // Right guillemet
    {0x03C0, '*'},  // Pi (ACS_PI)
    {0x2013, '-'},  // En dash
    {0x2014, '-'},  // Em dash
    {0x2018, '\''}, // Quotes
    {0x2019, '\''},
    {0x201C, '"'},
    {0x201D, '"'},
    {0x2022, 'o'},  // Bullet
    {0x2026, '.'},  // Ellipsis
    {0x2190, '<'},  // Arrows (ACS_LARROW etc.)
    {0x2191, '^'},
    {0x2192, '>'},
    {0x2193, 'v'},
    {0x2260, '!'},  // Not equal (ACS_NEQUAL)
    {0x2264, '<'},  // Less than or equal (ACS_LEQUAL)
    {0x2265, '>'},  // Greater than or equal (ACS_GEQUAL)
    {0x23BA, '~'},  // Scan line 1 (ACS_S1)
    {0x23BB, '-'},  // Scan line 3 (ACS_S3)
    {0x23BC, '-'},  // Scan line 7 (ACS_S7)
    {0x23BD, '_'},  // Scan line 9 (ACS_S9)
    {0x240B, '#'},  // VT symbol (ACS_LANTERN)
    {0x2424, '#'},  // NL symbol (ACS_BOARD)
    {0x2592, ':'},  // Checkerboard (ACS_CKBOARD)
    {0x25C6, '+'},  // Diamond (ACS_DIAMOND)
};

vt_char_t vt_charmap_ascii(
    uint32_t cp
) {
    if (cp >= 0x20 && cp < 0x7F) return (vt_char_t)cp;
    if (cp >= 0xC0 && cp <= 0xFF) return (vt_char_t)vt_charmap_latin1[cp - 0xC0];
    if (cp >= 0x2500 && cp <= 0x257F) {
        const uint8_t b = cp - 0x2500;
        if (vt_charmap_has(vt_charmap_box_h, sizeof(vt_charmap_box_h), b)) return '-';
        if (vt_charmap_has(vt_charmap_box_v, sizeof(vt_charmap_box_v), b)) return '|';
        if (cp == 0x2571) return '/';
        if (cp == 0x2572) return '\\';
        if (cp == 0x2573) return 'X';
        return '+';
    }
    // Binary search the symbols
    size_t lo = 0;
    size_t hi = COUNT_ARR(vt_charmap_pairs);
    while (lo < hi) {
        const size_t m = (lo + hi) / 2;
        if (cp < vt_charmap_pairs[m].cp) hi = m;
        else if (cp > vt_charmap_pairs[m].cp) lo = m + 1;
        else return (vt_char_t)vt_charmap_pairs[m].ch;
    }
    if (cp >= 0x2580 && cp <= 0x259F) return '#'; // Block elements (ACS_BLOCK)
    return '?';
}
