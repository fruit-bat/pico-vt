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
 * cc -DVT_BUILD_TEST -Wall vt_term.c
 * 
 * ./a.out
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "vt_term.h"
#include <string.h>
#include <assert.h>

// Print out the characters in the grid so we can see what is going on
void print_grid(vt_term_t *t) {
    vt_coord_t w = t->w;
    vt_coord_t h = t->h;
    for (vt_coord_t c = 0; c < w * 7 + 5; ++c) printf(c == (t->c + 1) ? "+" : "-");
    printf("\n");
    for (vt_coord_t r = 0; r < h; ++r) {
        printf(r == (t->r) ? "+" : "|");
        for (vt_coord_t c = 0; c < w; ++c) {
            vt_cell_t ct = t->rp[r][c];
            vt_char_t ch = vt_cell_get_char(ct);
            printf("%c", (uint8_t)ch);
        }
        printf(r == (t->r) ? "+" : "|");
        for (vt_coord_t c = 0; c < w; ++c) {
            vt_cell_t ct = t->rp[r][c];
            vt_cell_attr_t ca = vt_cell_get_attr(ct);
            vt_cell_colour_t ci = vt_cell_fg_get(ca);
            printf("%02X", (uint8_t)ci);
        }
        printf("|");
        for (vt_coord_t c = 0; c < w; ++c) {
            vt_cell_t ct = t->rp[r][c];
            vt_cell_attr_t ca = vt_cell_get_attr(ct);
            vt_cell_colour_t ci = vt_cell_bg_get(ca);
            printf("%02X", (uint8_t)ci);
        }
        printf("|");
        for (vt_coord_t c = 0; c < w; ++c) {
            vt_cell_t ct = t->rp[r][c];
            vt_cell_attr_t ca = vt_cell_get_attr(ct);
            vt_cell_flags_t fl = vt_cell_flags_get(ca);
            printf("%02X", (uint8_t)fl);
        }
        printf("|\n");
    }
    for (vt_coord_t c = 0; c < w * 7 + 5; ++c) printf(c == (t->c + 1) ? "+" : "-");
    printf("\n");
}

void check_grid_blank(vt_term_t *t) {
    // Create a cell with default attributes and a space character
    vt_cell_attr_t c1 = vt_cell_enc_attr(
        VT_TERM_DEFAULT_FG,
        VT_TERM_DEFAULT_BG,
        VT_TERM_DEFAULT_FLAGS
    );
    vt_cell_t c3 = vt_cell_combine(c1, 32);
    vt_coord_t w = t->w;
    vt_coord_t h = t->h;    
    for (vt_coord_t c = 0; c < w; ++c) {
        for (vt_coord_t r = 0; r < h; ++r) {
            vt_cell_t ct = t->rp[r][c];
            assert(ct == c3);
        }
    }
}

void set_grid_rows(vt_term_t *t) {
    // Create a cell with default attributes and a space character
    vt_cell_attr_t c1 = vt_cell_enc_attr(
        VT_TERM_DEFAULT_FG,
        VT_TERM_DEFAULT_BG,
        VT_TERM_DEFAULT_FLAGS
    );
    vt_coord_t w = t->w;
    vt_coord_t h = t->h;    

    // Put characters in the grid so we can test scroll up
    // AAAAA...
    // BBBBB...
    // etc.
    for (vt_coord_t c = 0; c < w; ++c) {
        for (vt_coord_t r = 0; r < h; ++r) {
            t->rp[r][c] = vt_cell_combine(c1, r + 'A');
        }
    } 
}

void set_grid_cols(vt_term_t *t) {
    // Create a cell with default attributes and a space character
    vt_cell_attr_t c1 = vt_cell_enc_attr(
        VT_TERM_DEFAULT_FG,
        VT_TERM_DEFAULT_BG,
        VT_TERM_DEFAULT_FLAGS
    );
    vt_coord_t w = t->w;
    vt_coord_t h = t->h;    

    // Put characters in the grid so we can test scroll up
    // AAAAA...
    // BBBBB...
    // etc.
    for (vt_coord_t c = 0; c < w; ++c) {
        for (vt_coord_t r = 0; r < h; ++r) {
            t->rp[r][c] = vt_cell_combine(c1, c + 'A');
        }
    } 
}

void check_grid_rows(vt_term_t *t, char* chs) {
    vt_coord_t w = t->w;
    vt_coord_t h = t->h;    
    for (vt_coord_t c = 0; c < w; ++c) {
        for (vt_coord_t r = 0; r < h; ++r) {
            vt_cell_t ct = t->rp[r][c];
            vt_char_t ch = vt_cell_get_char(ct);
            if (chs[r] != '?') assert(ch == (vt_char_t)(chs[r]));
        }
    }
}

void check_grid_row(vt_term_t *t, vt_coord_t r, char* chs) {
    vt_coord_t w = t->w;
    for (vt_coord_t c = 0; c < w; ++c) {
        vt_cell_t ct = t->rp[r][c];
        vt_char_t ch = vt_cell_get_char(ct);
        assert(ch == (vt_char_t)(chs[c]));
    }
}

void test_scroll(vt_term_t *t) {
    // Put characters in the grid so we can test scroll up
    // AAAAA...
    // BBBBB...
    // etc.
    set_grid_rows(t);
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 

    printf("\nScroll up 1 row\n");
    vt_term_scroll_up(t, 0, 1);
    print_grid(t);
    check_grid_rows(t, "BCDEFGHIJKLMNOP "); 

    printf("\nScroll up 2 rows\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_scroll_up(t, 0, 2);
    print_grid(t);
    check_grid_rows(t, "CDEFGHIJKLMNOP  "); 

    printf("\nScroll up 100 rows\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_scroll_up(t, 0, 100);
    print_grid(t);
    check_grid_blank(t);

    printf("\nScroll down 1 row\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_scroll_down(t, 0, 1);
    print_grid(t);
    check_grid_rows(t, " ABCDEFGHIJKLMNO"); 

    printf("\nScroll down 2 rows\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_scroll_down(t, 0, 2);
    print_grid(t);
    check_grid_rows(t, "  ABCDEFGHIJKLMN"); 

    printf("\nScroll down 100 rows\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_scroll_down(t, 0, 100);
    print_grid(t);
    check_grid_blank(t);

    // Set top and bottom margins
    t->mt = 2;
    t->mb = t->h - 5;

    set_grid_rows(t);
    vt_term_scroll_up(t, t->mt, 1);
    print_grid(t);
    check_grid_rows(t, "ABDEFGHIJKL MNOP"); 

    set_grid_rows(t);
    vt_term_scroll_up(t, t->mt, 2);
    print_grid(t);
    check_grid_rows(t, "ABEFGHIJKL  MNOP"); 

    set_grid_rows(t);
    vt_term_scroll_up(t, t->mt, 10);
    print_grid(t);
    check_grid_rows(t, "AB          MNOP"); 

    set_grid_rows(t);
    vt_term_scroll_down(t, t->mt, 1);
    print_grid(t);
    check_grid_rows(t, "AB CDEFGHIJKMNOP"); 

    set_grid_rows(t);
    vt_term_scroll_down(t, t->mt, 2);
    print_grid(t);
    check_grid_rows(t, "AB  CDEFGHIJMNOP"); 

    set_grid_rows(t);
    vt_term_scroll_down(t, t->mt, 10);
    print_grid(t);
    check_grid_rows(t, "AB          MNOP"); 

    // Clear margins
    t->mt = 0;
    t->mb = t->h - 1;

}

void test_erase_in_display(vt_term_t *t) {
    // Erase in Display is a terminal control function (from ANSI escape codes, specifically ESC [ n J) that clears parts or all of the terminal screen, depending on the parameter n:
    //
    // n = 0: Erase from cursor to end of screen (including cursor position).
    // n = 1: Erase from start of screen to cursor (including cursor position).
    // n = 2: Erase entire screen.

    printf("\nErase in display [0]\n");
    set_grid_rows(t);
    t->r = 5; t->c = 7;
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_erase_in_display(t, 0);
    print_grid(t);
    check_grid_row(t, 0, "AAAAAAAAAAAAAAAAAAAA");
    check_grid_row(t, 1, "BBBBBBBBBBBBBBBBBBBB");
    check_grid_row(t, 2, "CCCCCCCCCCCCCCCCCCCC");
    check_grid_row(t, 3, "DDDDDDDDDDDDDDDDDDDD");
    check_grid_row(t, 4, "EEEEEEEEEEEEEEEEEEEE");
    check_grid_row(t, 5, "FFFFFFF             ");
    for(vt_coord_t r = 6; r < t->h; ++r)
        check_grid_row(t, r, "                    ");

    printf("\nErase in display [1]\n");
    set_grid_rows(t);
    t->r = 10; t->c = 7;
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_erase_in_display(t, 1);
    print_grid(t);
    for(vt_coord_t r = 0; r < 10; ++r) {
        check_grid_row(t, r, "                    ");
    }
    check_grid_row(t, 10, "        KKKKKKKKKKKK");
    check_grid_row(t, 11, "LLLLLLLLLLLLLLLLLLLL");
    check_grid_row(t, 12, "MMMMMMMMMMMMMMMMMMMM");
    check_grid_row(t, 13, "NNNNNNNNNNNNNNNNNNNN");
    check_grid_row(t, 14, "OOOOOOOOOOOOOOOOOOOO");
    check_grid_row(t, 15, "PPPPPPPPPPPPPPPPPPPP");

    printf("\nErase in display [2]\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_term_erase_in_display(t, 2);
    print_grid(t);
    check_grid_blank(t);
}

void test_insert_characters(vt_term_t *t) {
    t->r = 5;
    t->c = 0;
    set_grid_cols(t);
    vt_term_insert_characters(t, 3);
    print_grid(t);
    check_grid_row(t, 5, "   ABCDEFGHIJKLMNOPQ");

    t->c = 5;
    set_grid_cols(t);
    vt_term_insert_characters(t, 3);
    print_grid( t);
    check_grid_row(t, 5, "ABCDE   FGHIJKLMNOPQ");

    t->c = 5;
    set_grid_cols(t);
    vt_term_insert_characters(t, 300);
    print_grid( t);
    check_grid_row(t, 5, "ABCDE               ");

    // Insert a full line's worth at column 0 (previously read/wrote out of bounds)
    t->c = 0;
    set_grid_cols(t);
    vt_term_insert_characters(t, t->w);
    print_grid( t);
    check_grid_row(t, 5, "                    ");
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");

    t->c = 0;
    set_grid_cols(t);
    vt_term_insert_characters(t, 300);
    print_grid( t);
    check_grid_row(t, 5, "                    ");

    t->c = 0;
    set_grid_cols(t);
    vt_term_insert_characters(t, t->w - 1);
    print_grid( t);
    check_grid_row(t, 5, "                   A");

    t->c = t->w - 1;
    set_grid_cols(t);
    vt_term_insert_characters(t, 1);
    print_grid( t);
    check_grid_row(t, 5, "ABCDEFGHIJKLMNOPQRS ");
}

void test_delete_characters(vt_term_t *t) {
    t->r = 5;
    t->c = 0;
    set_grid_cols(t);
    vt_term_delete_characters(t, 3);
    print_grid(t);
    check_grid_row(t, 5, "DEFGHIJKLMNOPQRST   ");

    t->c = 5;
    set_grid_cols(t);
    vt_term_delete_characters(t, 3);
    print_grid( t);
    check_grid_row(t, 5, "ABCDEIJKLMNOPQRST   ");

    t->c = 5;
    set_grid_cols(t);
    vt_term_delete_characters(t, 300);
    print_grid( t);
    check_grid_row(t, 5, "ABCDE               ");

    t->c = t->w - 1;
    set_grid_cols(t);
    vt_term_delete_characters(t, 1);
    print_grid( t);
    check_grid_row(t, 5, "ABCDEFGHIJKLMNOPQRS ");

    // TODO Check attributes
    /* VT102 manual says the attribute for the newly empty characters
     * should be the same as the last character moved left, which isn't
     * what clearline() currently does.
     */
}

void test_erase_in_line(vt_term_t *t) {

    t->r = 5;
    t->c = 7;
    set_grid_cols(t);
    vt_term_erase_in_line(t, 0);
    print_grid(t);
    check_grid_row(t, 5, "ABCDEFG             ");

    set_grid_cols(t);
    vt_term_erase_in_line(t, 1);
    print_grid(t);
    check_grid_row(t, 5, "        IJKLMNOPQRST");

    set_grid_cols(t);
    vt_term_erase_in_line(t, 2);
    print_grid(t);
    check_grid_row(t, 5, "                    ");
}

void test_putch(vt_term_t *t) {
    printf("\nTesting putch no scroll\n");
    vt_term_clear_screen(t);
    t->r = 5;t->c = 7;
    vt_term_putch(t, 'A');
    vt_term_putch(t, 'B');
    vt_term_putch(t, 'C');
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if(r != 5) check_grid_row(t, r, "                    ");
    }
    check_grid_row(t, 5, "       ABC          ");

    printf("\nTesting putch wrap\n");
    vt_term_clear_screen(t);
    t->r = 5;t->c = t->w - 1;
    vt_term_putch(t, 'A');
    vt_term_putch(t, 'B');
    vt_term_putch(t, 'C');
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if(r != 5 && r != 6) check_grid_row(t, r, "                    ");
    }
    check_grid_row(t, 5, "                   A");
    check_grid_row(t, 6, "BC                  ");

    printf("\nTesting deferred wrap scroll\n");
    vt_term_clear_screen(t);
    t->r = t->h - 1;t->c = t->w - 1;
    vt_term_putch(t, 'A');
    print_grid(t);
    // Cursor stays in the last column with a wrap pending; nothing scrolls yet
    assert(t->r == t->h - 1);
    assert(t->c == t->w - 1);
    assert(t->wrap);
    for(vt_coord_t r = 0; r < (t->h - 1); ++r) {
        check_grid_row(t, r, "                    ");
    }
    check_grid_row(t, t->h - 1, "                   A");
    vt_term_putch(t, 'B');
    vt_term_putch(t, 'C');
    print_grid(t);
    for(vt_coord_t r = 0; r < (t->h - 2); ++r) {
        check_grid_row(t, r, "                    ");
    }
    check_grid_row(t, t->h - 2, "                   A");
    check_grid_row(t, t->h - 1, "BC                  ");

    printf("\nTesting deferred wrap scroll with margins\n");
    // Set top and bottom margins
    t->mt = 2;
    t->mb = t->h - 5;
    t->r = t->mb; 
    t->c = t->w - 1;

    set_grid_rows(t);
    vt_term_putch(t, 'x');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "LLLLLLLLLLLLLLLLLLLx");
    vt_term_putch(t, 'y');
    vt_term_putch(t, 'z');
    print_grid(t);
    check_grid_rows(t, "ABDEFGHIJK??MNOP"); 
    check_grid_row(t, 10, "LLLLLLLLLLLLLLLLLLLx");
    check_grid_row(t, 11, "yz                  ");

    printf("\nTesting wrap below bottom margin\n");
    // Wrapping on a row below the scroll region moves down a row without scrolling
    t->r = t->mb + 2;
    t->c = t->w - 1;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    print_grid(t);
    assert(t->r == t->mb + 2);
    assert(t->c == t->w - 1);
    assert(t->wrap);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJKLM??P");
    check_grid_row(t, 13, "NNNNNNNNNNNNNNNNNNNx");
    check_grid_row(t, 14, "yOOOOOOOOOOOOOOOOOOO");

    printf("\nTesting wrap on last row below bottom margin\n");
    // On the last row below the scroll region, wrap to column 0 without scrolling
    t->r = t->h - 1;
    t->c = t->w - 1;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    print_grid(t);
    assert(t->r == t->h - 1);
    assert(t->c == t->w - 1);
    assert(t->wrap);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNO?");
    check_grid_row(t, 15, "yPPPPPPPPPPPPPPPPPPx");

    // Clear margins
    t->mt = 0;
    t->mb = t->h - 1;
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
    // Set top and bottom margins
    t->mt = 2;
    t->mb = t->h - 5;

    t->r = t->mt; 
    t->c = t->w - 1;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    assert(t->wrap);
    // The cursor is still on the top margin, so reverse index scrolls the region down
    vt_term_reverse_nl(t);
    print_grid(t);
    assert(!t->wrap);
    assert(t->r == t->mt);
    assert(t->c == t->w - 1);
    check_grid_rows(t, "AB ?DEFGHIJKMNOP");
    check_grid_row(t, 3, "CCCCCCCCCCCCCCCCCCCx");
    vt_term_putch(t, 'y');
    vt_term_putch(t, 'z');
    print_grid(t);
    check_grid_rows(t, "AB??DEFGHIJKMNOP");
    check_grid_row(t, 2, "                   y");
    check_grid_row(t, 3, "zCCCCCCCCCCCCCCCCCCx");

    t->r = t->mt; 
    t->c = 2;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    vt_term_reverse_nl(t);
    print_grid(t);
    vt_term_putch(t, 'y');
    vt_term_putch(t, 'z');
    print_grid(t);
    check_grid_rows(t, "AB??DEFGHIJKMNOP");
    check_grid_row(t, 2, "   yz               ");
    check_grid_row(t, 3, "CCxCCCCCCCCCCCCCCCCC");

    // Clear margins
    t->mt = 0;
    t->mb = t->h - 1;
}

void test_nl(vt_term_t *t) {
    printf("\nTesting nl margins\n");
    // Set top and bottom margins
    t->mt = 2;
    t->mb = t->h - 5;

    t->r = t->mb; 
    t->c = t->w - 1;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    assert(t->wrap);
    // LF scrolls the region but keeps the column (no implicit CR)
    vt_term_nl(t);
    print_grid(t);
    assert(!t->wrap);
    assert(t->r == t->mb);
    assert(t->c == t->w - 1);
    check_grid_rows(t, "ABDEFGHIJK? MNOP");
    check_grid_row(t, 10, "LLLLLLLLLLLLLLLLLLLx");
    // y fills the last column, z wraps and scrolls the region again
    vt_term_putch(t, 'y');
    vt_term_putch(t, 'z');
    print_grid(t);
    check_grid_rows(t, "ABEFGHIJK???MNOP");
    check_grid_row(t, 9, "LLLLLLLLLLLLLLLLLLLx");
    check_grid_row(t, 10, "                   y");
    check_grid_row(t, 11, "z                   ");

    // CR LF after filling a line must not leave a blank line
    t->r = t->mb;
    t->c = t->w - 1;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    vt_term_cr(t);
    vt_term_nl(t);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABDEFGHIJK??MNOP");
    check_grid_row(t, 10, "LLLLLLLLLLLLLLLLLLLx");
    check_grid_row(t, 11, "y                   ");

    t->r = t->mt; 
    t->c = 2;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    vt_term_nl(t);
    print_grid(t);
    vt_term_putch(t, 'y');
    vt_term_putch(t, 'z');
    print_grid(t);
    check_grid_rows(t, "AB??EFGHIJKLMNOP");
    check_grid_row(t, 2, "CCxCCCCCCCCCCCCCCCCC");
    check_grid_row(t, 3, "DDDyzDDDDDDDDDDDDDDD");

    // Clear margins
    t->mt = 0;
    t->mb = t->h - 1;
}

void test_cr(vt_term_t *t) {
    printf("\nTesting cr margins\n");
    // Set top and bottom margins
    t->mt = 2;
    t->mb = t->h - 5;

    t->r = t->mb; 
    t->c = t->w - 1;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    assert(t->wrap);
    // CR cancels the pending wrap and returns to the start of the same row
    vt_term_cr(t);
    print_grid(t);
    assert(!t->wrap);
    assert(t->r == t->mb);
    assert(t->c == 0);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "LLLLLLLLLLLLLLLLLLLx");
    vt_term_putch(t, 'y');
    vt_term_putch(t, 'z');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "yzLLLLLLLLLLLLLLLLLx");

    // Same mid-screen, inside the margins
    t->r = 5;
    t->c = t->w - 1;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    vt_term_cr(t);
    vt_term_putch(t, 'y');
    print_grid(t);
    check_grid_rows(t, "ABCDE?GHIJKLMNOP");
    check_grid_row(t, 5, "yFFFFFFFFFFFFFFFFFFx");

    t->r = t->mb; 
    t->c = 0;
    set_grid_rows(t);
    vt_term_putch(t, 'x');
    vt_term_cr(t);
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "xLLLLLLLLLLLLLLLLLLL");
    vt_term_putch(t, 'y');
    vt_term_putch(t, 'z');
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJK?MNOP");
    check_grid_row(t, 11, "yzLLLLLLLLLLLLLLLLLL");

    // Clear margins
    t->mt = 0;
    t->mb = t->h - 1;
}

// Fill the bottom-right cell so a wrap is pending at the bottom margin
static void set_wrap_pending_bottom(vt_term_t *t) {
    vt_term_clear_screen(t);
    vt_term_cursor_set(t, t->h - 1, t->w - 1);
    vt_term_putch(t, 'A');
    assert(t->wrap);
}

// The wrap must be cancelled: the next character goes at (r, c) without scrolling
static void check_wrap_cancelled(vt_term_t *t, vt_coord_t r, vt_coord_t c) {
    print_grid(t);
    assert(!t->wrap);
    assert(t->r == r);
    assert(t->c == c);
    vt_term_putch(t, 'B');
    assert(vt_cell_get_char(t->rp[r][c]) == 'B');
    // A scroll would have moved the 'A' up a row
    assert(vt_cell_get_char(t->rp[t->h - 2][t->w - 1]) != 'A');
    if (r != t->h - 1 || c != t->w - 1) {
        assert(vt_cell_get_char(t->rp[t->h - 1][t->w - 1]) == 'A');
    }
}

void test_wrap_cancelled_by_cursor_moves(vt_term_t *t) {
    printf("\nTesting cursor moves cancel a pending wrap\n");
    vt_coord_t br = t->h - 1;
    vt_coord_t bc = t->w - 1;

    set_wrap_pending_bottom(t);
    vt_term_cursor_set(t, 0, 0);
    check_wrap_cancelled(t, 0, 0);

    set_wrap_pending_bottom(t);
    vt_term_cursor_set_col(t, 3);
    check_wrap_cancelled(t, br, 3);

    set_wrap_pending_bottom(t);
    vt_term_cursor_set_row(t, 4);
    check_wrap_cancelled(t, 4, bc);

    set_wrap_pending_bottom(t);
    vt_term_cursor_up(t, 1);
    check_wrap_cancelled(t, br - 1, bc);

    set_wrap_pending_bottom(t);
    vt_term_cursor_down(t, 1);
    check_wrap_cancelled(t, br, bc);

    set_wrap_pending_bottom(t);
    vt_term_cursor_left(t, 1);
    check_wrap_cancelled(t, br, bc - 1);

    set_wrap_pending_bottom(t);
    vt_term_cursor_right(t, 1);
    check_wrap_cancelled(t, br, bc);

    set_wrap_pending_bottom(t);
    vt_term_next_line_up(t, 1);
    check_wrap_cancelled(t, br - 1, 0);

    set_wrap_pending_bottom(t);
    vt_term_next_line_down(t, 1);
    check_wrap_cancelled(t, br, 0);

    t->r = 2; t->c = 3;
    vt_term_save_cursor(t);
    set_wrap_pending_bottom(t);
    vt_term_restore_cursor(t);
    check_wrap_cancelled(t, 2, 3);
}

// Write the last column of row 5 so a wrap is pending there
static void set_wrap_pending_row5(vt_term_t *t) {
    set_grid_cols(t);
    vt_term_cursor_set(t, 5, t->w - 1);
    vt_term_putch(t, 'x');
    assert(t->wrap);
}

// The edit must have acted on row 5 and left the cursor there with no wrap pending
static void check_edit_row5(vt_term_t *t, char *row5) {
    print_grid(t);
    assert(!t->wrap);
    assert(t->r == 5);
    assert(t->c == t->w - 1);
    check_grid_row(t, 5, row5);
}

void test_wrap_pending_edits(vt_term_t *t) {
    printf("\nTesting edits with a pending wrap act on the written row\n");

    set_wrap_pending_row5(t);
    vt_term_erase_in_line(t, 0);
    check_edit_row5(t, "ABCDEFGHIJKLMNOPQRS ");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");

    set_wrap_pending_row5(t);
    vt_term_erase_in_line(t, 1);
    check_edit_row5(t, "                    ");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");

    set_wrap_pending_row5(t);
    vt_term_erase_in_line(t, 2);
    check_edit_row5(t, "                    ");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");
    // The next character overwrites the last column rather than wrapping
    vt_term_putch(t, 'y');
    check_grid_row(t, 5, "                   y");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");

    set_wrap_pending_row5(t);
    vt_term_erase_in_display(t, 0);
    check_edit_row5(t, "ABCDEFGHIJKLMNOPQRS ");
    for(vt_coord_t r = 6; r < t->h; ++r)
        check_grid_row(t, r, "                    ");

    set_wrap_pending_row5(t);
    vt_term_erase_in_display(t, 1);
    check_edit_row5(t, "                    ");
    for(vt_coord_t r = 0; r < 5; ++r)
        check_grid_row(t, r, "                    ");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");

    set_wrap_pending_row5(t);
    vt_term_delete_characters(t, 1);
    check_edit_row5(t, "ABCDEFGHIJKLMNOPQRS ");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");

    set_wrap_pending_row5(t);
    vt_term_insert_characters(t, 1);
    check_edit_row5(t, "ABCDEFGHIJKLMNOPQRS ");
    check_grid_row(t, 6, "ABCDEFGHIJKLMNOPQRST");
}

void test_repeat(vt_term_t *t) {
    printf("\nTesting repeat\n");
    set_grid_cols(t);
    t->r = 5;
    t->c = 0;
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
    t->r = 5;
    t->c = 0;
    vt_term_repeat(t, 2);
    check_grid_row(t, 5, "ABCDEFGHIJKLMNOPQRST");
}

int main() {
    const vt_coord_t w = 20;
    const vt_coord_t h = 16;
    vt_cell_t grid[h][w];
    vt_term_t t;
    vt_term_init(&t, (vt_cell_t*)grid, w, h);
    assert(t.c == 0);
    assert(t.r == 0);
    assert(t.w == w);
    assert(t.h == h);

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

