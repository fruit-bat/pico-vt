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
#include "vt_parser.h"
#include "vt_term.h"
#include "vt_tabs.h"
#include "vt_utf8.h"
#include "vt_charmap.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Called with bytes the terminal sends back to the host (e.g. a cursor position report)
typedef void (*vt_emu_reply_t)(
    void* ctx,           // The context passed to vt_emu_set_reply()
    const vt_char_t* s,  // The bytes to send
    uint32_t n           // The number of bytes
);

typedef struct {
    vt_parser_t parser;
    vt_term_t term;
    vt_tabs_t tabs;
    vt_emu_reply_t reply; // NULL if replies are not wanted
    void* reply_ctx;
    vt_utf8_t utf8;       // Decoder for printable text
    vt_charmap_t charmap; // Code point to cell glyph
    bool g_dec[2];        // G0/G1 use DEC special graphics (line drawing)
    uint8_t gl;           // The set in use: 0 (G0, SI) or 1 (G1, SO)
    bool s_g_dec[2];      // Saved with the cursor
    uint8_t s_gl;
} vt_emu_t;

void vt_emu_init(
    vt_emu_t* e,     // Emulator
    vt_cell_t* grid, // Cell grid for the display
    vt_coord_t w,    // Terminal width in characters
    vt_coord_t h     // Terminal height in characters
);

void vt_emu_put_ch(
    vt_emu_t *e, 
    vt_char_t ch
);

void vt_emu_put_str(
    vt_emu_t *e, 
    vt_char_t* s
);

void vt_emu_reset(
    vt_emu_t *e
);

// Set how code points map to the 8 bit glyphs in the cells, to match the
// display font. The default is vt_charmap_ascii. Reset does not change this.
void vt_emu_set_charmap(
    vt_emu_t *e,
    vt_charmap_t charmap
);

// Set where replies to the host go (reset does not change this)
void vt_emu_set_reply(
    vt_emu_t *e,
    vt_emu_reply_t reply, // NULL for no replies
    void* ctx             // Passed to reply
);

#ifdef __cplusplus
}
#endif
