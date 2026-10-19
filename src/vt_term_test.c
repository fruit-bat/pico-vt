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
 * ./vt_term_test.sh
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "vt_term.h"
#include "vt_term_test_utils.h"

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

// The expected strings below assume this terminal size
#define W 20
#define H 16

// Margins used by the scroll region tests
#define MT 2
#define MB (H - 5)

#define BLANK "                    " // A blank row
#define COLS  "ABCDEFGHIJKLMNOPQRST" // Any row after set_grid_cols()
#define ROWS  "ABCDEFGHIJKLMNOP"     // The rows after set_grid_rows()

static void set_margins(vt_term_t *t) {
    vt_term_margin_set(t, MT, MB);
}

static void clear_margins(vt_term_t *t) {
    vt_term_margin_set(t, 0, H - 1);
}

static void put_str(vt_term_t *t, const char *s) {
    while (*s) vt_term_putch(t, *s++);
}

static void check_cursor(vt_term_t *t, vt_coord_t r, vt_coord_t c, bool wrap) {
    assert(t->r == r);
    assert(t->c == c);
    assert(t->wrap == wrap);
}

// Check rows [from, to) are blank
static void check_rows_blank(vt_term_t *t, vt_coord_t from, vt_coord_t to) {
    for (vt_coord_t r = from; r < to; ++r) check_grid_row(t, r, BLANK);
}

static void set_wrap_pending_row5(vt_term_t *t);

typedef void (*scroll_fn_t)(vt_term_t *t, vt_coord_t rs, vt_coord_t n);

void test_scroll(vt_term_t *t) {
    static const struct {
        const char *name;
        scroll_fn_t fn;
        bool margins;
        vt_coord_t n;
        const char *rows; // NULL means the whole grid is blank
    } cases[] = {
        { "up",   vt_term_scroll_up,   false, 1,   "BCDEFGHIJKLMNOP " },
        { "up",   vt_term_scroll_up,   false, 2,   "CDEFGHIJKLMNOP  " },
        { "up",   vt_term_scroll_up,   false, 100, NULL },
        { "down", vt_term_scroll_down, false, 1,   " ABCDEFGHIJKLMNO" },
        { "down", vt_term_scroll_down, false, 2,   "  ABCDEFGHIJKLMN" },
        { "down", vt_term_scroll_down, false, 100, NULL },
        { "up",   vt_term_scroll_up,   true,  1,   "ABDEFGHIJKL MNOP" },
        { "up",   vt_term_scroll_up,   true,  2,   "ABEFGHIJKL  MNOP" },
        { "up",   vt_term_scroll_up,   true,  10,  "AB          MNOP" },
        { "down", vt_term_scroll_down, true,  1,   "AB CDEFGHIJKMNOP" },
        { "down", vt_term_scroll_down, true,  2,   "AB  CDEFGHIJMNOP" },
        { "down", vt_term_scroll_down, true,  10,  "AB          MNOP" },
    };

    set_grid_rows(t);
    print_grid(t);
    check_grid_rows(t, ROWS);

    for (size_t i = 0; i < ARRAY_LEN(cases); ++i) {
        printf("\nScroll %s %d rows%s\n", cases[i].name, cases[i].n,
            cases[i].margins ? " with margins" : "");
        if (cases[i].margins) set_margins(t); else clear_margins(t);
        set_grid_rows(t);
        cases[i].fn(t, t->mt, cases[i].n);
        print_grid(t);
        if (cases[i].rows) check_grid_rows(t, cases[i].rows);
        else check_grid_blank(t);
    }

    clear_margins(t);
}

void test_erase_in_display(vt_term_t *t) {
    printf("\nErase in display [0]\n");
    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_erase_in_display(t, 0);
    print_grid(t);
    check_grid_rows(t, "ABCDE?          ");
    check_grid_row(t, 5, "FFFFFFF             ");

    printf("\nErase in display [1]\n");
    set_grid_rows(t);
    vt_term_cursor_set(t, 10, 7);
    vt_term_erase_in_display(t, 1);
    print_grid(t);
    check_grid_rows(t, "          ?LMNOP");
    check_grid_row(t, 10, "        KKKKKKKKKKKK");

    printf("\nErase in display [2]\n");
    set_grid_rows(t);
    vt_term_erase_in_display(t, 2);
    print_grid(t);
    check_grid_blank(t);
}

// A single-row edit applied to row 5 of a set_grid_cols() grid
typedef void (*row_edit_fn_t)(vt_term_t *t, uint32_t p);

typedef struct {
    vt_coord_t c;     // Cursor column
    uint32_t p;       // Parameter for the edit
    const char *row5; // Expected row 5
} row_edit_case_t;

static void run_row_edit_cases(
    vt_term_t *t,
    row_edit_fn_t fn,
    const row_edit_case_t *cases,
    size_t n
) {
    for (size_t i = 0; i < n; ++i) {
        set_grid_cols(t);
        vt_term_cursor_set(t, 5, cases[i].c);
        fn(t, cases[i].p);
        print_grid(t);
        check_grid_row(t, 4, COLS);
        check_grid_row(t, 5, cases[i].row5);
        check_grid_row(t, 6, COLS);
    }
}

void test_insert_characters(vt_term_t *t) {
    static const row_edit_case_t cases[] = {
        { 0,     3,     "   ABCDEFGHIJKLMNOPQ" },
        { 5,     3,     "ABCDE   FGHIJKLMNOPQ" },
        { 5,     300,   "ABCDE               " },
        { 0,     W,     BLANK },                  // Previously read/wrote out of bounds
        { 0,     300,   BLANK },
        { 0,     W - 1, "                   A" },
        { W - 1, 1,     "ABCDEFGHIJKLMNOPQRS " },
    };
    printf("\nInsert characters\n");
    run_row_edit_cases(t, vt_term_insert_characters, cases, ARRAY_LEN(cases));
}

void test_delete_characters(vt_term_t *t) {
    static const row_edit_case_t cases[] = {
        { 0,     3,   "DEFGHIJKLMNOPQRST   " },
        { 5,     3,   "ABCDEIJKLMNOPQRST   " },
        { 5,     300, "ABCDE               " },
        { W - 1, 1,   "ABCDEFGHIJKLMNOPQRS " },
    };
    printf("\nDelete characters\n");
    run_row_edit_cases(t, vt_term_delete_characters, cases, ARRAY_LEN(cases));
    // Attributes of the freed cells are checked in test_erase_uses_current_attr
}

void test_erase_characters(vt_term_t *t) {
    static const row_edit_case_t cases[] = {
        { 7,     3,   "ABCDEFG   KLMNOPQRST" },
        { 7,     300, "ABCDEFG             " },
        { 0,     W,   BLANK },
        { W - 1, 1,   "ABCDEFGHIJKLMNOPQRS " },
        { 7,     0,   COLS },
    };
    printf("\nErase characters\n");
    run_row_edit_cases(t, vt_term_erase_characters, cases, ARRAY_LEN(cases));
    // The cursor does not move
    assert(t->r == 5 && t->c == 7);
}

void test_insert_delete_lines(vt_term_t *t) {
    printf("\nInsert and delete lines\n");

    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_insert_lines(t, 2);
    print_grid(t);
    check_cursor(t, 5, 0, false);
    check_grid_rows(t, "ABCDE  FGHIJKLMN");

    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_delete_lines(t, 2);
    print_grid(t);
    check_cursor(t, 5, 0, false);
    check_grid_rows(t, "ABCDEHIJKLMNOP  ");

    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_insert_lines(t, 100);
    check_grid_rows(t, "ABCDE           ");

    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_delete_lines(t, 100);
    check_grid_rows(t, "ABCDE           ");

    set_grid_rows(t);
    vt_term_insert_lines(t, 0);
    vt_term_delete_lines(t, 0);
    check_grid_rows(t, ROWS);

    // Within the margins only the scroll region moves
    set_margins(t);
    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 3);
    vt_term_insert_lines(t, 2);
    print_grid(t);
    check_grid_rows(t, "ABCDE  FGHIJMNOP");

    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 3);
    vt_term_delete_lines(t, 2);
    print_grid(t);
    check_grid_rows(t, "ABCDEHIJKL  MNOP");

    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 3);
    vt_term_insert_lines(t, 100);
    check_grid_rows(t, "ABCDE       MNOP");

    // Outside the scroll region nothing happens and the cursor stays put
    set_grid_rows(t);
    vt_term_cursor_set(t, MT - 1, 3);
    vt_term_insert_lines(t, 1);
    vt_term_delete_lines(t, 1);
    check_cursor(t, MT - 1, 3, false);
    vt_term_cursor_set(t, MB + 1, 3);
    vt_term_insert_lines(t, 1);
    vt_term_delete_lines(t, 1);
    check_cursor(t, MB + 1, 3, false);
    check_grid_rows(t, ROWS);

    clear_margins(t);

    // A pending wrap is cancelled and the edit happens on the written row
    set_wrap_pending_row5(t);
    vt_term_insert_lines(t, 1);
    print_grid(t);
    check_cursor(t, 5, 0, false);
    check_grid_row(t, 5, BLANK);
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRSx");

    set_wrap_pending_row5(t);
    vt_term_delete_lines(t, 1);
    check_cursor(t, 5, 0, false);
    check_grid_row(t, 5, COLS);
    check_grid_row(t, H - 1, BLANK);
}

void test_erase_in_line(vt_term_t *t) {
    static const row_edit_case_t cases[] = {
        { 7, 0, "ABCDEFG             " },
        { 7, 1, "        IJKLMNOPQRST" },
        { 7, 2, BLANK },
    };
    printf("\nErase in line\n");
    run_row_edit_cases(t, vt_term_erase_in_line, cases, ARRAY_LEN(cases));
}

void test_putch(vt_term_t *t) {
    printf("\nTesting putch no scroll\n");
    vt_term_clear_screen(t);
    vt_term_cursor_set(t, 5, 7);
    put_str(t, "ABC");
    print_grid(t);
    check_grid_rows(t, "     ?          ");
    check_grid_row(t, 5, "       ABC          ");

    printf("\nTesting putch wrap\n");
    vt_term_clear_screen(t);
    vt_term_cursor_set(t, 5, W - 1);
    put_str(t, "ABC");
    print_grid(t);
    check_grid_rows(t, "     ??         ");
    check_grid_row(t, 5, "                   A");
    check_grid_row(t, 6, "BC                  ");

    printf("\nTesting deferred wrap scroll\n");
    vt_term_clear_screen(t);
    vt_term_cursor_set(t, H - 1, W - 1);
    vt_term_putch(t, 'A');
    print_grid(t);
    // Cursor stays in the last column with a wrap pending; nothing scrolls yet
    check_cursor(t, H - 1, W - 1, true);
    check_rows_blank(t, 0, H - 1);
    check_grid_row(t, H - 1, "                   A");
    put_str(t, "BC");
    print_grid(t);
    check_rows_blank(t, 0, H - 2);
    check_grid_row(t, H - 2, "                   A");
    check_grid_row(t, H - 1, "BC                  ");

    printf("\nTesting deferred wrap scroll with margins\n");
    set_margins(t);
    set_grid_rows(t);
    vt_term_cursor_set(t, MB, W - 1);
    vt_term_putch(t, 'x');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "LLLLLLLLLLLLLLLLLLLx");
    put_str(t, "yz");
    print_grid(t);
    check_grid_rows(t, "ABDEFGHIJK??MNOP");
    check_grid_row(t, 10, "LLLLLLLLLLLLLLLLLLLx");
    check_grid_row(t, 11, "yz                  ");

    printf("\nTesting wrap below bottom margin\n");
    // Wrapping on a row below the scroll region moves down a row without scrolling
    set_grid_rows(t);
    vt_term_cursor_set(t, MB + 2, W - 1);
    vt_term_putch(t, 'x');
    print_grid(t);
    check_cursor(t, MB + 2, W - 1, true);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJKLM??P");
    check_grid_row(t, 13, "NNNNNNNNNNNNNNNNNNNx");
    check_grid_row(t, 14, "yOOOOOOOOOOOOOOOOOOO");

    printf("\nTesting wrap on last row below bottom margin\n");
    // On the last row below the scroll region, wrap to column 0 without scrolling
    set_grid_rows(t);
    vt_term_cursor_set(t, H - 1, W - 1);
    vt_term_putch(t, 'x');
    print_grid(t);
    check_cursor(t, H - 1, W - 1, true);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNO?");
    check_grid_row(t, 15, "yPPPPPPPPPPPPPPPPPPx");

    clear_margins(t);
}

void test_clear_screen_tall() {
    printf("\nTesting clear screen with width < height\n");
    // A terminal taller than it is wide (previously only cleared w rows)
    const vt_coord_t w = 10;
    const vt_coord_t h = 20;
    vt_cell_t grid[h][w];
    vt_term_t t;
    vt_term_init(&t, (vt_cell_t*)grid, w, h);
    set_grid_rows(&t);
    vt_term_clear_screen(&t);
    print_grid(&t);
    check_grid_blank(&t);
}

void test_reverse_nl(vt_term_t *t) {
    printf("\nTesting reverse nl margins\n");
    set_margins(t);

    set_grid_rows(t);
    vt_term_cursor_set(t, MT, W - 1);
    vt_term_putch(t, 'x');
    assert(t->wrap);
    // The cursor is still on the top margin, so reverse index scrolls the region down
    vt_term_reverse_nl(t);
    print_grid(t);
    check_cursor(t, MT, W - 1, false);
    check_grid_rows(t, "AB ?DEFGHIJKMNOP");
    check_grid_row(t, 3, "CCCCCCCCCCCCCCCCCCCx");
    put_str(t, "yz");
    print_grid(t);
    check_grid_rows(t, "AB??DEFGHIJKMNOP");
    check_grid_row(t, 2, "                   y");
    check_grid_row(t, 3, "zCCCCCCCCCCCCCCCCCCx");

    set_grid_rows(t);
    vt_term_cursor_set(t, MT, 2);
    vt_term_putch(t, 'x');
    vt_term_reverse_nl(t);
    print_grid(t);
    put_str(t, "yz");
    print_grid(t);
    check_grid_rows(t, "AB??DEFGHIJKMNOP");
    check_grid_row(t, 2, "   yz               ");
    check_grid_row(t, 3, "CCxCCCCCCCCCCCCCCCCC");

    clear_margins(t);
}

void test_nl(vt_term_t *t) {
    printf("\nTesting nl margins\n");
    set_margins(t);

    set_grid_rows(t);
    vt_term_cursor_set(t, MB, W - 1);
    vt_term_putch(t, 'x');
    assert(t->wrap);
    // LF scrolls the region but keeps the column (no implicit CR)
    vt_term_nl(t);
    print_grid(t);
    check_cursor(t, MB, W - 1, false);
    check_grid_rows(t, "ABDEFGHIJK? MNOP");
    check_grid_row(t, 10, "LLLLLLLLLLLLLLLLLLLx");
    // y fills the last column, z wraps and scrolls the region again
    put_str(t, "yz");
    print_grid(t);
    check_grid_rows(t, "ABEFGHIJK???MNOP");
    check_grid_row(t, 9, "LLLLLLLLLLLLLLLLLLLx");
    check_grid_row(t, 10, "                   y");
    check_grid_row(t, 11, "z                   ");

    // CR LF after filling a line must not leave a blank line
    set_grid_rows(t);
    vt_term_cursor_set(t, MB, W - 1);
    vt_term_putch(t, 'x');
    vt_term_cr(t);
    vt_term_nl(t);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABDEFGHIJK??MNOP");
    check_grid_row(t, 10, "LLLLLLLLLLLLLLLLLLLx");
    check_grid_row(t, 11, "y                   ");

    set_grid_rows(t);
    vt_term_cursor_set(t, MT, 2);
    vt_term_putch(t, 'x');
    vt_term_nl(t);
    print_grid(t);
    put_str(t, "yz");
    print_grid(t);
    check_grid_rows(t, "AB??EFGHIJKLMNOP");
    check_grid_row(t, 2, "CCxCCCCCCCCCCCCCCCCC");
    check_grid_row(t, 3, "DDDyzDDDDDDDDDDDDDDD");

    clear_margins(t);
}

void test_cr(vt_term_t *t) {
    printf("\nTesting cr margins\n");
    set_margins(t);

    set_grid_rows(t);
    vt_term_cursor_set(t, MB, W - 1);
    vt_term_putch(t, 'x');
    assert(t->wrap);
    // CR cancels the pending wrap and returns to the start of the same row
    vt_term_cr(t);
    print_grid(t);
    check_cursor(t, MB, 0, false);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "LLLLLLLLLLLLLLLLLLLx");
    put_str(t, "yz");
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "yzLLLLLLLLLLLLLLLLLx");

    // Same mid-screen, inside the margins
    set_grid_rows(t);
    vt_term_cursor_set(t, 5, W - 1);
    vt_term_putch(t, 'x');
    vt_term_cr(t);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABCDE?GHIJKLMNOP");
    check_grid_row(t, 5, "yFFFFFFFFFFFFFFFFFFx");

    set_grid_rows(t);
    vt_term_cursor_set(t, MB, 0);
    vt_term_putch(t, 'x');
    vt_term_cr(t);
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "xLLLLLLLLLLLLLLLLLLL");
    put_str(t, "yz");
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "yzLLLLLLLLLLLLLLLLLL");

    clear_margins(t);
}

// Fill the bottom-right cell so a wrap is pending at the bottom margin
static void set_wrap_pending_bottom(vt_term_t *t) {
    vt_term_clear_screen(t);
    vt_term_cursor_set(t, H - 1, W - 1);
    vt_term_putch(t, 'A');
    assert(t->wrap);
}

// The wrap must be cancelled: the next character goes at (r, c) without scrolling
static void check_wrap_cancelled(vt_term_t *t, vt_coord_t r, vt_coord_t c) {
    print_grid(t);
    check_cursor(t, r, c, false);
    vt_term_putch(t, 'B');
    assert(vt_cell_get_char(t->rp[r][c]) == 'B');
    // A scroll would have moved the 'A' up a row
    assert(vt_cell_get_char(t->rp[H - 2][W - 1]) != 'A');
    if (r != H - 1 || c != W - 1) {
        assert(vt_cell_get_char(t->rp[H - 1][W - 1]) == 'A');
    }
}

void test_wrap_cancelled_by_cursor_moves(vt_term_t *t) {
    printf("\nTesting cursor moves cancel a pending wrap\n");

    set_wrap_pending_bottom(t);
    vt_term_cursor_set(t, 0, 0);
    check_wrap_cancelled(t, 0, 0);

    set_wrap_pending_bottom(t);
    vt_term_cursor_set_col(t, 3);
    check_wrap_cancelled(t, H - 1, 3);

    set_wrap_pending_bottom(t);
    vt_term_cursor_set_row(t, 4);
    check_wrap_cancelled(t, 4, W - 1);

    set_wrap_pending_bottom(t);
    vt_term_cursor_up(t, 1);
    check_wrap_cancelled(t, H - 2, W - 1);

    set_wrap_pending_bottom(t);
    vt_term_cursor_down(t, 1);
    check_wrap_cancelled(t, H - 1, W - 1);

    set_wrap_pending_bottom(t);
    vt_term_cursor_left(t, 1);
    check_wrap_cancelled(t, H - 1, W - 2);

    set_wrap_pending_bottom(t);
    vt_term_cursor_right(t, 1);
    check_wrap_cancelled(t, H - 1, W - 1);

    set_wrap_pending_bottom(t);
    vt_term_next_line_up(t, 1);
    check_wrap_cancelled(t, H - 2, 0);

    set_wrap_pending_bottom(t);
    vt_term_next_line_down(t, 1);
    check_wrap_cancelled(t, H - 1, 0);

    vt_term_cursor_set(t, 2, 3);
    vt_term_save_cursor(t);
    set_wrap_pending_bottom(t);
    vt_term_restore_cursor(t);
    check_wrap_cancelled(t, 2, 3);
}

// Write the last column of row 5 so a wrap is pending there
static void set_wrap_pending_row5(vt_term_t *t) {
    set_grid_cols(t);
    vt_term_cursor_set(t, 5, W - 1);
    vt_term_putch(t, 'x');
    assert(t->wrap);
}

void test_wrap_pending_edits(vt_term_t *t) {
    printf("\nTesting edits with a pending wrap act on the written row\n");

    // Single-row edits: only row 5 changes
    static const struct {
        row_edit_fn_t fn;
        uint32_t p;
        const char *row5;
    } cases[] = {
        { vt_term_erase_in_line,     0, "ABCDEFGHIJKLMNOPQRS " },
        { vt_term_erase_in_line,     1, BLANK },
        { vt_term_erase_in_line,     2, BLANK },
        { vt_term_delete_characters, 1, "ABCDEFGHIJKLMNOPQRS " },
        { vt_term_insert_characters, 1, "ABCDEFGHIJKLMNOPQRS " },
        { vt_term_erase_characters,  1, "ABCDEFGHIJKLMNOPQRS " },
    };
    for (size_t i = 0; i < ARRAY_LEN(cases); ++i) {
        set_wrap_pending_row5(t);
        cases[i].fn(t, cases[i].p);
        print_grid(t);
        check_cursor(t, 5, W - 1, false);
        check_grid_row(t, 4, COLS);
        check_grid_row(t, 5, cases[i].row5);
        check_grid_row(t, 6, COLS);
    }

    // The next character overwrites the last column rather than wrapping
    set_wrap_pending_row5(t);
    vt_term_erase_in_line(t, 2);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_row(t, 5, "                   y");
    check_grid_row(t, 6, COLS);

    set_wrap_pending_row5(t);
    vt_term_erase_in_display(t, 0);
    print_grid(t);
    check_cursor(t, 5, W - 1, false);
    check_grid_row(t, 4, COLS);
    check_grid_row(t, 5, "ABCDEFGHIJKLMNOPQRS ");
    check_rows_blank(t, 6, H);

    set_wrap_pending_row5(t);
    vt_term_erase_in_display(t, 1);
    print_grid(t);
    check_cursor(t, 5, W - 1, false);
    check_rows_blank(t, 0, 6);
    check_grid_row(t, 6, COLS);
}

void test_repeat(vt_term_t *t) {
    printf("\nTesting repeat\n");
    set_grid_cols(t);
    vt_term_cursor_set(t, 5, 0);
    vt_term_putch(t, 'a');
    vt_term_repeat(t, 3);
    print_grid(t);
    check_grid_row(t, 5, "aaaaEFGHIJKLMNOPQRST");

    // With a wrap pending the repeated character is the one in the last column
    set_wrap_pending_row5(t);
    vt_term_repeat(t, 2);
    print_grid(t);
    check_grid_row(t, 5, "ABCDEFGHIJKLMNOPQRSx");
    check_grid_row(t, 6, "xxCDEFGHIJKLMNOPQRST");

    // Nothing to repeat at the start of a line
    set_grid_cols(t);
    vt_term_cursor_set(t, 5, 0);
    vt_term_repeat(t, 2);
    check_grid_row(t, 5, COLS);
}

static vt_cell_attr_t default_attr(void) {
    return vt_cell_enc_attr(VT_TERM_DEFAULT_FG, VT_TERM_DEFAULT_BG, VT_TERM_DEFAULT_FLAGS);
}

void test_margin_set(vt_term_t *t) {
    printf("\nTesting margin set\n");
    // Setting the margins homes the cursor
    vt_term_cursor_set(t, 5, 7);
    vt_term_margin_set(t, 3, 9);
    assert(t->mt == 3 && t->mb == 9);
    check_cursor(t, 0, 0, false);

    // Invalid margins are ignored and leave the cursor alone
    vt_term_cursor_set(t, 5, 7);
    vt_term_margin_set(t, 9, 9);
    assert(t->mt == 3 && t->mb == 9);
    vt_term_margin_set(t, 10, 9);
    assert(t->mt == 3 && t->mb == 9);
    vt_term_margin_set(t, 0, H);
    assert(t->mt == 3 && t->mb == 9);
    check_cursor(t, 5, 7, false);

    vt_term_margin_set(t, 0, H - 1);
    assert(t->mt == 0 && t->mb == H - 1);
}

void test_cursor_moves_margins(vt_term_t *t) {
    printf("\nTesting cursor moves stop at the margins\n");
    set_margins(t);

    // Inside the region, up and down stop at the margins
    vt_term_cursor_set(t, 5, 7);
    vt_term_cursor_up(t, 100);
    check_cursor(t, MT, 7, false);
    vt_term_cursor_down(t, 100);
    check_cursor(t, MB, 7, false);
    vt_term_cursor_set(t, 5, 7);
    vt_term_next_line_up(t, 100);
    check_cursor(t, MT, 0, false);
    vt_term_cursor_set(t, 5, 7);
    vt_term_next_line_down(t, 100);
    check_cursor(t, MB, 0, false);

    // Short moves are unaffected
    vt_term_cursor_set(t, 5, 7);
    vt_term_cursor_up(t, 3);
    check_cursor(t, MT, 7, false);
    vt_term_cursor_down(t, MB - MT);
    check_cursor(t, MB, 7, false);

    // Above the region, up stops at the top of the screen and down at the bottom margin
    vt_term_cursor_set(t, MT - 1, 7);
    vt_term_cursor_up(t, 100);
    check_cursor(t, 0, 7, false);
    vt_term_cursor_down(t, 100);
    check_cursor(t, MB, 7, false);

    // Below the region, down stops at the bottom of the screen and up at the top margin
    vt_term_cursor_set(t, MB + 1, 7);
    vt_term_cursor_down(t, 100);
    check_cursor(t, H - 1, 7, false);
    vt_term_cursor_up(t, 100);
    check_cursor(t, MT, 7, false);

    clear_margins(t);
}

void test_mode_autowrap(vt_term_t *t) {
    printf("\nTesting auto wrap mode (DECAWM)\n");
    assert(vt_term_mode_get(t, VT_TERM_MODE_AUTOWRAP));

    // Without auto wrap, characters at the right margin overwrite the last column
    vt_term_mode_set(t, VT_TERM_MODE_AUTOWRAP, false);
    set_grid_cols(t);
    vt_term_cursor_set(t, 5, W - 2);
    put_str(t, "xyz");
    print_grid(t);
    check_cursor(t, 5, W - 1, false);
    check_grid_row(t, 5, "ABCDEFGHIJKLMNOPQRxz");
    check_grid_row(t, 6, COLS);

    // Turning auto wrap off drops a pending wrap
    vt_term_mode_set(t, VT_TERM_MODE_AUTOWRAP, true);
    set_wrap_pending_row5(t);
    vt_term_mode_set(t, VT_TERM_MODE_AUTOWRAP, false);
    check_cursor(t, 5, W - 1, false);
    vt_term_putch(t, 'y');
    check_grid_row(t, 5, "ABCDEFGHIJKLMNOPQRSy");
    check_grid_row(t, 6, COLS);

    vt_term_mode_set(t, VT_TERM_MODE_AUTOWRAP, true);
}

void test_mode_origin(vt_term_t *t) {
    printf("\nTesting origin mode (DECOM)\n");
    set_margins(t);

    // Setting origin mode homes the cursor to the top margin
    vt_term_cursor_set(t, 5, 7);
    vt_term_mode_set(t, VT_TERM_MODE_ORIGIN, true);
    check_cursor(t, MT, 0, false);

    // Rows are relative to the top margin and kept within the margins
    vt_term_cursor_set(t, 3, 4);
    check_cursor(t, MT + 3, 4, false);
    vt_term_cursor_set(t, 100, 100);
    check_cursor(t, MB, W - 1, false);
    vt_term_cursor_set_row(t, 0);
    check_cursor(t, MT, W - 1, false);
    vt_term_cursor_set_row(t, 100);
    check_cursor(t, MB, W - 1, false);

    // Setting the margins homes the cursor to the new top margin
    vt_term_margin_set(t, 4, 8);
    check_cursor(t, 4, 0, false);

    // Save and restore cursor include origin mode
    vt_term_cursor_set(t, 1, 2);
    vt_term_save_cursor(t);
    vt_term_mode_set(t, VT_TERM_MODE_ORIGIN, false);
    vt_term_restore_cursor(t);
    assert(vt_term_mode_get(t, VT_TERM_MODE_ORIGIN));
    check_cursor(t, 5, 2, false);

    // Resetting origin mode homes the cursor to the top of the screen
    vt_term_mode_set(t, VT_TERM_MODE_ORIGIN, false);
    check_cursor(t, 0, 0, false);
    vt_term_cursor_set(t, 100, 100);
    check_cursor(t, H - 1, W - 1, false);

    // Restoring a cursor saved without origin mode turns it off
    vt_term_save_cursor(t);
    vt_term_mode_set(t, VT_TERM_MODE_ORIGIN, true);
    vt_term_restore_cursor(t);
    assert(!vt_term_mode_get(t, VT_TERM_MODE_ORIGIN));
    check_cursor(t, H - 1, W - 1, false);

    clear_margins(t);
}

void test_mode_insert(vt_term_t *t) {
    printf("\nTesting insert mode (IRM)\n");
    vt_term_mode_set(t, VT_TERM_MODE_INSERT, true);
    set_grid_cols(t);
    vt_term_cursor_set(t, 5, 3);
    put_str(t, "xy");
    print_grid(t);
    check_cursor(t, 5, 5, false);
    check_grid_row(t, 5, "ABCxyDEFGHIJKLMNOPQR");
    check_grid_row(t, 6, COLS);

    // In the last column the character just replaces the last one
    vt_term_cursor_set(t, 5, W - 1);
    vt_term_putch(t, 'z');
    check_grid_row(t, 5, "ABCxyDEFGHIJKLMNOPQz");
    assert(t->wrap);

    vt_term_mode_set(t, VT_TERM_MODE_INSERT, false);
    set_grid_cols(t);
    vt_term_cursor_set(t, 5, 3);
    put_str(t, "xy");
    check_grid_row(t, 5, "ABCxyFGHIJKLMNOPQRST");
}

void test_mode_lnm(vt_term_t *t) {
    printf("\nTesting line feed / new line mode (LNM)\n");
    // By default LF keeps the column
    vt_term_cursor_set(t, 5, 7);
    vt_term_nl(t);
    check_cursor(t, 6, 7, false);

    // In LNM mode LF also returns to column 0
    vt_term_mode_set(t, VT_TERM_MODE_LNM, true);
    vt_term_nl(t);
    check_cursor(t, 7, 0, false);

    // Including when it scrolls
    set_grid_rows(t);
    vt_term_cursor_set(t, H - 1, 7);
    vt_term_nl(t);
    check_cursor(t, H - 1, 0, false);
    check_grid_rows(t, "BCDEFGHIJKLMNOP ");

    vt_term_mode_set(t, VT_TERM_MODE_LNM, false);
}

void test_reset(vt_term_t *t) {
    printf("\nTesting reset\n");
    set_margins(t);
    set_grid_rows(t);
    vt_term_set_fgci(t, 3);
    vt_term_set_bgci(t, 4);
    vt_term_flags_add(t, VT_CELL_FLAGS_BOLD);
    vt_term_cursor_set(t, 5, W - 1);
    vt_term_putch(t, 'x');
    vt_term_mode_set(t, VT_TERM_MODE_INSERT | VT_TERM_MODE_LNM, true);

    vt_term_reset(t);
    check_cursor(t, 0, 0, false);
    assert(t->mt == 0 && t->mb == H - 1);
    assert(t->attr == default_attr());
    assert(t->flags == VT_TERM_MODES_DEFAULT);
    // The screen is cleared with the default attributes
    check_grid_blank(t);
    // The saved cursor is reset too
    vt_term_cursor_set(t, 7, 7);
    vt_term_set_bgci(t, 4);
    vt_term_restore_cursor(t);
    check_cursor(t, 0, 0, false);
    assert(t->attr == default_attr());
}

void test_save_restore_cursor(vt_term_t *t) {
    printf("\nTesting save and restore cursor\n");
    vt_term_cursor_set(t, 4, 6);
    vt_term_set_fgci(t, 3);
    vt_term_flags_add(t, VT_CELL_FLAGS_UNDERLINE);
    vt_cell_attr_t saved = t->attr;
    vt_term_save_cursor(t);

    vt_term_cursor_set(t, 10, 12);
    vt_term_reset_attr(t);
    vt_term_restore_cursor(t);
    check_cursor(t, 4, 6, false);
    assert(t->attr == saved);

    // Restoring again gives the same result
    vt_term_cursor_set(t, 1, 1);
    vt_term_restore_cursor(t);
    check_cursor(t, 4, 6, false);

    vt_term_reset_attr(t);
}

void test_cursor_moves(vt_term_t *t) {
    printf("\nTesting cursor moves and clamping\n");
    vt_term_cursor_set(t, 5, 7);
    vt_term_cursor_up(t, 2);
    check_cursor(t, 3, 7, false);
    vt_term_cursor_up(t, 100);
    check_cursor(t, 0, 7, false);

    vt_term_cursor_down(t, 2);
    check_cursor(t, 2, 7, false);
    vt_term_cursor_down(t, 100);
    check_cursor(t, H - 1, 7, false);

    vt_term_cursor_left(t, 3);
    check_cursor(t, H - 1, 4, false);
    vt_term_cursor_left(t, 100);
    check_cursor(t, H - 1, 0, false);

    vt_term_cursor_right(t, 3);
    check_cursor(t, H - 1, 3, false);
    vt_term_cursor_right(t, 100);
    check_cursor(t, H - 1, W - 1, false);

    // Moves of 0 leave the cursor where it is
    vt_term_cursor_set(t, 5, 7);
    vt_term_cursor_up(t, 0);
    vt_term_cursor_down(t, 0);
    vt_term_cursor_left(t, 0);
    vt_term_cursor_right(t, 0);
    check_cursor(t, 5, 7, false);

    vt_term_cursor_set(t, 100, 100);
    check_cursor(t, H - 1, W - 1, false);
    vt_term_cursor_set_col(t, 3);
    check_cursor(t, H - 1, 3, false);
    vt_term_cursor_set_col(t, 100);
    check_cursor(t, H - 1, W - 1, false);
    vt_term_cursor_set_row(t, 2);
    check_cursor(t, 2, W - 1, false);
    vt_term_cursor_set_row(t, 100);
    check_cursor(t, H - 1, W - 1, false);

    // Next line moves to column 0
    vt_term_cursor_set(t, 5, 7);
    vt_term_next_line_down(t, 2);
    check_cursor(t, 7, 0, false);
    vt_term_next_line_down(t, 100);
    check_cursor(t, H - 1, 0, false);
    vt_term_cursor_set(t, 5, 7);
    vt_term_next_line_up(t, 2);
    check_cursor(t, 3, 0, false);
    vt_term_next_line_up(t, 100);
    check_cursor(t, 0, 0, false);
}

void test_attributes(vt_term_t *t) {
    printf("\nTesting attributes\n");
    vt_term_reset_attr(t);
    assert(t->attr == default_attr());

    vt_term_set_fgci(t, 3);
    vt_term_set_bgci(t, 4);
    vt_term_flags_add(t, VT_CELL_FLAGS_BOLD | VT_CELL_FLAGS_UNDERLINE);
    vt_term_flags_clear(t, VT_CELL_FLAGS_BOLD);
    assert(vt_cell_fg_get(t->attr) == 3);
    assert(vt_cell_bg_get(t->attr) == 4);
    assert(vt_cell_flags_get(t->attr) == VT_CELL_FLAGS_UNDERLINE);

    // Characters are written with the current attributes
    vt_term_clear_screen(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_putch(t, 'x');
    assert(t->rp[5][7] == vt_cell_combine(t->attr, 'x'));

    vt_term_reset_attr(t);
    assert(t->attr == default_attr());
}

// An erased cell: a space in the current colours with no other attributes (as xterm)
static void check_erased(vt_term_t *t, vt_coord_t r, vt_coord_t c) {
    vt_cell_attr_t a = vt_cell_enc_attr(
        vt_cell_fg_get(t->attr), vt_cell_bg_get(t->attr), VT_CELL_FLAGS_NORMAL);
    assert(t->rp[r][c] == vt_cell_combine(a, ' '));
}

// A cell left alone by an erase still has the default attributes
static void check_untouched(vt_term_t *t, vt_coord_t r, vt_coord_t c) {
    assert(vt_cell_get_attr(t->rp[r][c]) == default_attr());
}

// Cells cleared by erase, insert, delete and scroll take the current colours only
void test_erase_uses_current_attr(vt_term_t *t) {
    printf("\nTesting erase uses the current colours\n");
    vt_term_reset_attr(t);
    vt_term_set_fgci(t, 3);
    vt_term_set_bgci(t, 4);
    vt_term_flags_add(t, VT_CELL_FLAGS_BOLD | VT_CELL_FLAGS_UNDERLINE | VT_CELL_FLAGS_REVERSE);

    set_grid_cols(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_erase_in_line(t, 0);
    check_untouched(t, 5, 6);
    check_erased(t, 5, 7);
    check_erased(t, 5, W - 1);

    set_grid_cols(t);
    vt_term_erase_in_display(t, 0);
    check_untouched(t, 5, 6);
    check_erased(t, 5, 7);
    check_erased(t, H - 1, 0);

    set_grid_cols(t);
    vt_term_insert_characters(t, 2);
    check_untouched(t, 5, 6);
    check_erased(t, 5, 7);
    check_erased(t, 5, 8);
    check_untouched(t, 5, 9);

    set_grid_cols(t);
    vt_term_delete_characters(t, 3);
    check_untouched(t, 5, W - 4);
    check_erased(t, 5, W - 3);
    check_erased(t, 5, W - 1);

    set_grid_rows(t);
    vt_term_scroll_up(t, 0, 1);
    check_untouched(t, H - 2, 0);
    check_erased(t, H - 1, 0);

    set_grid_rows(t);
    vt_term_scroll_down(t, 0, 1);
    check_erased(t, 0, 0);
    check_untouched(t, 1, 0);

    vt_term_clear_screen(t);
    check_erased(t, 0, 0);
    check_erased(t, H - 1, W - 1);

    // Written characters still take all the attributes
    vt_term_cursor_set(t, 0, 0);
    vt_term_putch(t, 'x');
    assert(t->rp[0][0] == vt_cell_combine(t->attr, 'x'));

    vt_term_reset_attr(t);
    vt_term_clear_screen(t);
}

void test_erase_params(vt_term_t *t) {
    printf("\nTesting erase parameters\n");
    // ED 3 erases the whole screen (no scrollback to clear)
    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 7);
    vt_term_erase_in_display(t, 3);
    check_grid_blank(t);
    check_cursor(t, 5, 7, false);

    // Unknown parameters do nothing
    set_grid_rows(t);
    vt_term_erase_in_display(t, 4);
    check_grid_rows(t, ROWS);

    set_grid_cols(t);
    vt_term_erase_in_line(t, 3);
    check_grid_row(t, 5, COLS);
}

void test_zero_counts(vt_term_t *t) {
    printf("\nTesting zero counts\n");
    static const row_edit_case_t cases[] = {
        { 7, 0, COLS },
    };
    run_row_edit_cases(t, vt_term_insert_characters, cases, ARRAY_LEN(cases));
    run_row_edit_cases(t, vt_term_delete_characters, cases, ARRAY_LEN(cases));
    run_row_edit_cases(t, vt_term_repeat, cases, ARRAY_LEN(cases));

    set_grid_rows(t);
    vt_term_scroll_up(t, 0, 0);
    vt_term_scroll_down(t, 0, 0);
    check_grid_rows(t, ROWS);
}

void test_scroll_start_row(vt_term_t *t) {
    printf("\nTesting scroll from a start row\n");
    // Scrolling from a row inside the region only moves the rows below it (as IL/DL would)
    set_grid_rows(t);
    vt_term_scroll_up(t, 5, 1);
    check_grid_rows(t, "ABCDEGHIJKLMNOP ");

    set_grid_rows(t);
    vt_term_scroll_down(t, 5, 2);
    check_grid_rows(t, "ABCDE  FGHIJKLMN");

    // A start row below the bottom margin does nothing
    set_margins(t);
    set_grid_rows(t);
    vt_term_scroll_up(t, MB + 1, 1);
    vt_term_scroll_down(t, MB + 1, 1);
    check_grid_rows(t, ROWS);
    clear_margins(t);
}

void test_nl_outside_margins(vt_term_t *t) {
    printf("\nTesting nl and reverse nl outside the margins\n");

    // Reverse index at the top of the screen with no margins scrolls the screen down
    set_grid_rows(t);
    vt_term_cursor_set(t, 0, 3);
    vt_term_reverse_nl(t);
    check_cursor(t, 0, 3, false);
    check_grid_rows(t, " ABCDEFGHIJKLMNO");

    // Newline at the bottom of the screen with no margins scrolls the screen up
    set_grid_rows(t);
    vt_term_cursor_set(t, H - 1, 3);
    vt_term_nl(t);
    check_cursor(t, H - 1, 3, false);
    check_grid_rows(t, "BCDEFGHIJKLMNOP ");

    set_margins(t);

    // Above the top margin reverse index moves up without scrolling...
    set_grid_rows(t);
    vt_term_cursor_set(t, MT - 1, 3);
    vt_term_reverse_nl(t);
    check_cursor(t, MT - 2, 3, false);
    // ...and stops at the top of the screen
    vt_term_reverse_nl(t);
    check_cursor(t, 0, 3, false);
    check_grid_rows(t, ROWS);

    // Below the bottom margin newline moves down without scrolling...
    vt_term_cursor_set(t, MB + 1, 3);
    vt_term_nl(t);
    check_cursor(t, MB + 2, 3, false);
    // ...and stops at the bottom of the screen
    vt_term_cursor_set(t, H - 1, 3);
    vt_term_nl(t);
    check_cursor(t, H - 1, 3, false);
    check_grid_rows(t, ROWS);

    // Inside the margins they just move the cursor
    vt_term_cursor_set(t, 5, 3);
    vt_term_nl(t);
    check_cursor(t, 6, 3, false);
    vt_term_reverse_nl(t);
    vt_term_reverse_nl(t);
    check_cursor(t, 4, 3, false);
    check_grid_rows(t, ROWS);

    clear_margins(t);
}

void test_alt_screen(vt_term_t *t) {
    printf("\nTesting alternate screen\n");
    vt_cell_t alt[H][W];

    // Without an alternate screen switching does nothing
    set_grid_rows(t);
    vt_term_alt_set(t, true);
    assert(!t->alt_active);
    check_grid_rows(t, ROWS);

    // The alternate screen starts blank
    for (vt_coord_t r = 0; r < H; ++r)
        for (vt_coord_t c = 0; c < W; ++c) alt[r][c] = 0;
    vt_term_alt_init(t, (vt_cell_t*)alt);
    assert(t->alt && !t->alt_active);
    check_grid_rows(t, ROWS);
    vt_term_alt_set(t, true);
    assert(t->alt_active);
    check_grid_blank(t);

    // Writing and scrolling on the alternate screen leave the normal screen alone
    vt_term_cursor_set(t, H - 1, 0);
    put_str(t, "alt");
    vt_term_nl(t);
    check_grid_row(t, H - 2, "alt                 ");

    vt_term_alt_set(t, false);
    assert(!t->alt_active);
    check_grid_rows(t, ROWS);

    // Switching back shows the alternate screen as it was left
    vt_term_alt_set(t, true);
    check_grid_row(t, H - 2, "alt                 ");

    // Reset returns to the normal screen
    vt_term_reset(t);
    assert(!t->alt_active && t->alt);

    // Removing the alternate screen
    vt_term_alt_set(t, true);
    vt_term_alt_init(t, NULL);
    assert(!t->alt && !t->alt_active);
    check_grid_blank(t);
}

void test_resize() {
    printf("\nTesting resize\n");
    vt_cell_t grid[H][W];
    vt_cell_t alt[H][W];
    vt_term_t tt;
    vt_term_t *t = &tt;

    // A 10 x 6 terminal that can grow to W x H
    vt_term_init_cap(t, (vt_cell_t*)grid, W, H, 10, 6);
    assert(t->w == 10 && t->h == 6 && t->cw == W && t->ch == H);
    check_grid_blank(t);
    // Rows of the capacity are W apart
    assert(t->rp[1] == t->rp[0] + W);

    // Growing wider shows blank cells, not what was beyond the old width
    set_grid_rows(t);
    t->rp[0][15] = vt_cell_combine(0, 'x');
    vt_term_resize(t, W, 6);
    check_grid_row(t, 0, "AAAAAAAAAA          ");
    check_grid_row(t, 5, "FFFFFFFFFF          ");

    // Shrinking hides cells; growing again shows them blank
    vt_term_resize(t, 5, 6);
    check_grid_row(t, 2, "CCCCC");
    vt_term_resize(t, 10, 6);
    check_grid_row(t, 2, "CCCCC     ");

    // Shrinking the height with the cursor near the top keeps the top
    set_grid_rows(t);
    vt_term_cursor_set(t, 1, 9);
    vt_term_resize(t, 10, 3);
    check_grid_rows(t, "ABC");
    check_cursor(t, 1, 9, false);

    // Growing the height shows blank rows
    vt_term_resize(t, 10, 6);
    check_grid_rows(t, "ABC???");
    check_rows_blank(t, 3, 6);

    // Shrinking the height with the cursor below the new bottom scrolls up
    // to keep the cursor's line in view
    set_grid_rows(t);
    vt_term_cursor_set(t, 5, 3);
    vt_term_resize(t, 10, 3);
    check_grid_rows(t, "DEF");
    check_cursor(t, 2, 3, false);
    // The lines scrolled off the top don't come back
    vt_term_resize(t, 10, 6);
    check_grid_rows(t, "DEF???");
    check_rows_blank(t, 3, 6);

    // The scroll region is reset, the cursors are kept on the screen and a
    // pending wrap is cancelled
    vt_term_margin_set(t, 1, 4);
    vt_term_cursor_set(t, 4, 8);
    vt_term_save_cursor(t);
    vt_term_cursor_set(t, 2, 9);
    put_str(t, "w");
    assert(t->wrap);
    vt_term_resize(t, 6, 6);
    assert(t->mt == 0 && t->mb == 5);
    check_cursor(t, 2, 5, false);
    vt_term_restore_cursor(t);
    check_cursor(t, 4, 5, false);

    // The size is limited to the capacity and is at least 1 x 1
    vt_term_resize(t, W + 5, H + 5);
    assert(t->w == W && t->h == H);
    vt_term_resize(t, 0, 0);
    assert(t->w == 1 && t->h == 1);
    check_cursor(t, 0, 0, false);

    // Both screens are resized
    vt_term_resize(t, 10, 6);
    vt_term_alt_init(t, (vt_cell_t*)alt);
    set_grid_rows(t);
    vt_term_alt_set(t, true);
    set_grid_cols(t);
    vt_term_resize(t, 12, 7);
    check_grid_row(t, 0, "ABCDEFGHIJ  ");
    check_grid_row(t, 6, "            ");
    vt_term_alt_set(t, false);
    check_grid_row(t, 0, "AAAAAAAAAA  ");
    check_grid_row(t, 6, "            ");
    // Including the rows out of view
    vt_term_resize(t, 12, H);
    check_rows_blank(t, 7, H);
    vt_term_alt_set(t, true);
    check_rows_blank(t, 7, H);
}

int main() {
    vt_cell_t grid[H][W];
    vt_term_t t;
    vt_term_init(&t, (vt_cell_t*)grid, W, H);
    check_cursor(&t, 0, 0, false);
    assert(t.w == W);
    assert(t.h == H);

    // Check the screen is blank on start-up
    printf("Initial display check\n");
    print_grid(&t);
    check_grid_blank(&t);

    test_scroll(&t);
    test_erase_in_display(&t);
    test_insert_characters(&t);
    test_delete_characters(&t);
    test_erase_in_line(&t);
    test_erase_characters(&t);
    test_insert_delete_lines(&t);
    test_putch(&t);
    test_reverse_nl(&t);
    test_nl(&t);
    test_cr(&t);
    test_wrap_cancelled_by_cursor_moves(&t);
    test_wrap_pending_edits(&t);
    test_repeat(&t);
    test_margin_set(&t);
    test_save_restore_cursor(&t);
    test_cursor_moves(&t);
    test_cursor_moves_margins(&t);
    test_mode_autowrap(&t);
    test_mode_origin(&t);
    test_mode_insert(&t);
    test_mode_lnm(&t);
    test_attributes(&t);
    test_erase_uses_current_attr(&t);
    test_erase_params(&t);
    test_zero_counts(&t);
    test_scroll_start_row(&t);
    test_nl_outside_margins(&t);
    test_reset(&t);
    test_alt_screen(&t);
    test_clear_screen_tall();
    test_resize();

    printf("all ok\n");
    return 0;
}
