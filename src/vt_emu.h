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

// Called when the host asks for a new size (ESC [ 8 ; h ; w t). The display
// decides: it may call vt_emu_resize() (from here or later) with that size,
// a different one or not at all.
typedef void (*vt_emu_resize_req_t)(
    void* ctx,     // The context passed to vt_emu_set_resize_req()
    vt_coord_t w,  // The width asked for, in characters
    vt_coord_t h   // The height asked for, in characters
);

typedef struct {
    vt_parser_t parser;
    vt_term_t term;
    vt_tabs_t tabs;
    vt_emu_reply_t reply; // NULL if replies are not wanted
    void* reply_ctx;
    vt_emu_resize_req_t resize_req; // NULL to ignore the host's size requests
    void* resize_req_ctx;
    uint16_t cell_pw;     // Cell width in pixels, 0 if unknown (for size reports)
    uint16_t cell_ph;     // Cell height in pixels, 0 if unknown
    bool resize_report;   // Mode ?2048: tell the host when the size changes
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

// As vt_emu_init, but the grid has a capacity of cw x ch cells (cw per row)
// so the terminal can be resized up to that (see vt_emu_resize)
void vt_emu_init_cap(
    vt_emu_t* e,     // Emulator
    vt_cell_t* grid, // Cell grid for the display, cw x ch
    vt_coord_t cw,   // Capacity width in characters
    vt_coord_t ch,   // Capacity height in characters
    vt_coord_t w,    // Terminal width in characters
    vt_coord_t h     // Terminal height in characters
);

// Change the size, within the capacity (see vt_term_resize). Call it from the
// same core as vt_emu_put_ch(). If the host has asked (mode ?2048) and the size
// changed, the new size is sent as ESC [ 48 ; h ; w ; hpx ; wpx t.
void vt_emu_resize(
    vt_emu_t *e,
    vt_coord_t w, // New width in characters
    vt_coord_t h  // New height in characters
);

// Set what is called when the host asks for a new size (reset does not change
// this). Without one (NULL, the default) the requests are ignored.
void vt_emu_set_resize_req(
    vt_emu_t *e,
    vt_emu_resize_req_t resize_req, // NULL to ignore requests
    void* ctx                       // Passed to resize_req
);

// The size of a cell in pixels, for the reports of the size in pixels.
// Unknown (0) by default, when those reports are not answered (or give 0).
void vt_emu_set_cell_size(
    vt_emu_t *e,
    uint16_t pw, // Cell width in pixels
    uint16_t ph  // Cell height in pixels
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

// Provide a grid, the same capacity as the main one, for the alternate screen used
// by full screen programs (ESC [ ? 1049 h). Without one (NULL, the default) the
// alternate screen is not available. Reset does not change this.
void vt_emu_set_alt_grid(
    vt_emu_t *e,
    vt_cell_t* grid
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
