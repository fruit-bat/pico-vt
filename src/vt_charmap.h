#pragma once
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
#include <stdint.h>
#include "vt_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Maps a Unicode code point to the 8 bit glyph stored in a cell. Provide one
// to match the display font (e.g. CP437); vt_charmap_ascii is the default.
typedef vt_char_t (*vt_charmap_t)(
    uint32_t cp
);

// The number of cells a code point occupies: 0 (combining marks, format and
// C1 controls), 2 (East Asian wide and emoji) or 1. An approximation of wcwidth().
uint32_t vt_charmap_width(
    uint32_t cp
);

// The Unicode code point for ch in the DEC special graphics (line drawing)
// set, or ch unchanged if it is outside 0x5F-0x7E
uint32_t vt_charmap_dec_graphics(
    vt_char_t ch
);

// Map to printable ASCII (0x20-0x7E) for an ASCII only font. Line drawing and
// symbols use the ncurses ASCII fallbacks, accented Latin-1 letters lose their
// accents and anything else is '?'.
vt_char_t vt_charmap_ascii(
    uint32_t cp
);

#ifdef __cplusplus
}
#endif
