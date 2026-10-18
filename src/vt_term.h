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
#include <stdbool.h>
#include "vt_types.h"
#include "vt_cell.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define VT_SCREEN_MAX_COLS 80
#define VT_SCREEN_MAX_ROWS 60
#define VT_TERM_DEFAULT_BG 0
#define VT_TERM_DEFAULT_FG 7 // Light grey (as xterm)
#define VT_TERM_DEFAULT_FLAGS VT_CELL_FLAGS_NORMAL

// Terminal modes, held in vt_term_t.flags and set with vt_term_mode_set()
#define VT_TERM_MODE_AUTOWRAP (1u << 0) // DECAWM (?7) wrap at the right margin
#define VT_TERM_MODE_ORIGIN   (1u << 1) // DECOM  (?6) cursor rows relative to the margins
#define VT_TERM_MODE_INSERT   (1u << 2) // IRM    (4)  insert characters rather than overwrite
#define VT_TERM_MODE_LNM      (1u << 3) // LNM    (20) LF, VT and FF also do a CR
#define VT_TERM_MODE_CURSOR   (1u << 4) // DECTCEM (?25) cursor visible (for the renderer)
#define VT_TERM_MODES_DEFAULT (VT_TERM_MODE_AUTOWRAP | VT_TERM_MODE_CURSOR)

// VT100-derived terminals have a deferred wrap behavior: writing a
// character in the last column leaves the cursor in that column and sets
// a "wrap pending" flag instead of moving to the next line.  This allows
// you to use the last column without getting extra blank lines or
// unintentionally scrolling the screen.  As in xterm:
// - The next printable character first does a CR + LF (scrolling the
//   region if the cursor is on the bottom margin) and is then written.
// - CR, LF, RI, cursor movement, erase, insert and delete clear the flag.
//   Since the cursor never left the last column, they act on the row and
//   column the character was written to.

typedef struct 
{
    vt_coord_t w; // Terminal width in characters
    vt_coord_t h; // Terminal height in characters
    vt_coord_t mt; // Margin top row
    vt_coord_t mb; // Margin bottom row
    vt_coord_t r; // Cursor row
    vt_coord_t c; // Cursor column
    vt_cell_attr_t attr; // The current attributes
    vt_coord_t s_r; // Cursor row
    vt_coord_t s_c; // Cursor column    
    vt_cell_attr_t s_attr; // The current attributes
    bool s_origin; // Saved origin mode
    bool wrap; // Wrap pending (deferred wrap at the last column)
    uint32_t flags; // Modes (VT_TERM_MODE_xxx)
    vt_cell_t* rp[VT_SCREEN_MAX_ROWS]; // Row pointers (of the screen in use)
    vt_cell_t* alt_rp[VT_SCREEN_MAX_ROWS]; // Row pointers of the screen not in use
    bool alt;        // An alternate screen has been provided
    bool alt_active; // The alternate screen is in use

} vt_term_t;

// DECSTBM - set the scroll region and home the cursor (ignored if invalid)
void vt_term_margin_set(
    vt_term_t* t, // The terminal
    vt_coord_t mt, // Margin top row
    vt_coord_t mb // Margin bottom row
);

// Set or reset one or more modes (VT_TERM_MODE_xxx)
void vt_term_mode_set(
    vt_term_t* t,  // The terminal
    uint32_t mode, // The mode(s)
    bool on        // Set or reset
);

static inline bool vt_term_mode_get(
    const vt_term_t* t, // The terminal
    uint32_t mode       // The mode
) {
    return (t->flags & mode) != 0;
}

void vt_term_init(
    vt_term_t* t, // The terminal
    vt_cell_t* grid, // Cell grid for the display
    vt_coord_t w, // Terminal width in characters
    vt_coord_t h  // Terminal height in characters
);

// Returns to the normal screen if the alternate screen is in use
void vt_term_reset(
    vt_term_t* t  // The terminal
);

// Provide a grid (the same size as the main one) for the alternate screen,
// or NULL for none. The grid is cleared.
void vt_term_alt_init(
    vt_term_t* t,   // The terminal
    vt_cell_t* grid // Cell grid for the alternate screen
);

// Switch to the alternate (true) or normal (false) screen. Does nothing
// without an alternate screen.
void vt_term_alt_set(
    vt_term_t* t, // The terminal
    bool on       // Use the alternate screen
);

void vt_term_save_cursor(
    vt_term_t* t  // The terminal
);

void vt_term_restore_cursor(
    vt_term_t* t  // The terminal
);

void vt_term_scroll_up(
    vt_term_t *t,  // The terminal
    vt_coord_t rs, // The start row
    vt_coord_t n   // Number of rows to scroll
);

void vt_term_scroll_down(
    vt_term_t *t,  // The terminal
    vt_coord_t rs, // The start row
    vt_coord_t n   // Number of rows to scroll
);

void vt_term_reset_attr(
    vt_term_t *t // The terminal
);

void vt_term_putch(
    vt_term_t *t, // The terminal
    vt_char_t ch
);

void vt_term_nl(
    vt_term_t *t // The terminal
);

void vt_term_cr(
    vt_term_t *t // The terminal
);

void vt_term_cursor_down(
    vt_term_t *t,
    vt_coord_t n
);

void vt_term_cursor_up(
    vt_term_t *t,
    vt_coord_t n
);

void vt_term_cursor_left(
    vt_term_t *t,
    vt_coord_t n
);

void vt_term_cursor_right(
    vt_term_t *t,
    vt_coord_t n
);

void vt_term_cursor_set(
    vt_term_t *t,
    vt_coord_t r,
    vt_coord_t c
);

void vt_term_cursor_set_col(
    vt_term_t *t,
    vt_coord_t c
);

void vt_term_cursor_set_row(
    vt_term_t *t,
    vt_coord_t r
);

void vt_term_next_line_down(
    vt_term_t *t,
    vt_coord_t n
);

void vt_term_next_line_up(
    vt_term_t *t,
    vt_coord_t n
);

// Erase in Display is a terminal control function (from ANSI escape codes, specifically ESC [ n J) that clears parts or all of the terminal screen, depending on the parameter n:
//
// n = 0: Erase from cursor to end of screen (including cursor position).
// n = 1: Erase from start of screen to cursor (including cursor position).
// n = 2: Erase entire screen.
void vt_term_erase_in_display(
    vt_term_t *t, // The terminal
    uint32_t p0   // Parameter to specify the erase mode (0, 1, 2)
);

// Erase in Display is a terminal control function (from ANSI escape codes, specifically ESC [ n J) that clears parts or all of the terminal screen, depending on the parameter n:
//
// n = 0: Erase from cursor to end of screen (including cursor position).
// n = 1: Erase from start of screen to cursor (including cursor position).
// n = 2: Erase entire screen.
void vt_term_erase_in_line(
    vt_term_t *t, // The terminal
    uint32_t p0   // Parameter to specify the erase mode (0, 1, 2)
);

void vt_term_insert_characters(
    vt_term_t *t, // The terminal
    uint32_t n   // number of characters
);

// IND - index: down a line, scrolling the region at the bottom margin.
// Unlike vt_term_nl, never returns to column 0.
void vt_term_index(
    vt_term_t *t // The terminal
);

// ESC M - reverse index / reverse new line
void vt_term_reverse_nl(
    vt_term_t *t // The terminal
);

void vt_term_flags_add(
    vt_term_t *t,
    vt_cell_flags_t f
);

void vt_term_flags_clear(
    vt_term_t *t,
    vt_cell_flags_t f
);

void vt_term_set_fgci(
    vt_term_t *t,
    vt_cell_colour_t fgci
);

void vt_term_set_bgci(
    vt_term_t *t,
    vt_cell_colour_t bgci
);

void vt_term_repeat(
    vt_term_t *t, // The terminal
    uint32_t n    // The number of characters to repeat
);

void vt_term_delete_characters(
    vt_term_t *t, // The terminal
    uint32_t n    // The number of characters to delete (note: 0 means 0)
);

// ECH - erase characters from the cursor without moving the rest of the line
void vt_term_erase_characters(
    vt_term_t *t, // The terminal
    uint32_t n    // The number of characters to erase (note: 0 means 0)
);

// IL - insert blank lines at the cursor row, pushing lines below it down
// within the scroll region. Does nothing outside the region.
void vt_term_insert_lines(
    vt_term_t *t, // The terminal
    uint32_t n    // The number of lines to insert (note: 0 means 0)
);

// DL - delete lines at the cursor row, pulling lines below it up within
// the scroll region. Does nothing outside the region.
void vt_term_delete_lines(
    vt_term_t *t, // The terminal
    uint32_t n    // The number of lines to delete (note: 0 means 0)
);

void vt_term_clear_screen(
    vt_term_t *t // The terminal
);

#ifdef __cplusplus
}
#endif
