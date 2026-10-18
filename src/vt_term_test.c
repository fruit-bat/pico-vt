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

    // TODO Check attributes
    /* VT102 manual says the attribute for the newly empty characters
     * should be the same as the last character moved left, which isn't
     * what clearline() currently does.
     */
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
    test_putch(&t);
    test_reverse_nl(&t);
    test_nl(&t);
    test_cr(&t);
    test_wrap_cancelled_by_cursor_moves(&t);
    test_wrap_pending_edits(&t);
    test_repeat(&t);
    test_clear_screen_tall();

    printf("all ok\n");
    return 0;
}
