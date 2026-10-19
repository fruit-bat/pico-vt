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
 * ./vt_emu_test.sh
 * 

Shell 1)

fifo=/tmp/myfifo
rm -f "$fifo"
mkfifo "$fifo"
pty=$(socat -d -d PTY,raw,echo=0 FILE:"$fifo",ignoreeof 2>&1 | \
awk -F' ' '/PTY is/ {print $NF; exit}')
echo "Run: tio $pty"

Shell 2)
a.out < /tmp/myfifo

Shell 3)
tio /tmp/myfifo

* 
 */
#include "vt_emu.h"
#include "vt_term_test_utils.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "vt_pallet.h"

#define BLANK_ROW "                    "

// Every cell is a space (whatever its attributes)
static void check_grid_blank_chars(vt_term_t* t) {
    for (vt_coord_t r = 0; r < t->h; ++r) check_grid_row(t, r, BLANK_ROW);
}

// TODO
// ESC[6n	request cursor position (reports as ESC[#;#R)

// ESC[H	moves cursor to home position (0, 0)
// ESC[{line};{column}H moves cursor to line #, column #
// ESC[{line};{column}f	moves cursor to line #, column #
// ESC[#A	moves cursor up # lines
// ESC[#B	moves cursor down # lines
// ESC[#C	moves cursor right # columns
// ESC[#D	moves cursor left # columns
// ESC[#E	moves cursor to beginning of next line, # lines down
// ESC[#F	moves cursor to beginning of previous line, # lines up
// ESC[#G	moves cursor to column #
// ESC M	moves cursor one line up, scrolling if needed
// ESC 7	save cursor position (DEC)
// ESC 8	restores the cursor to the last saved position (DEC)
// ESC[s	save cursor position (SCO)
// ESC[u	restores the cursor to the last saved position (SCO)
void test_cursor(vt_emu_t* e) {
    vt_term_t* const t = &e->term;

    vt_emu_reset(e);
    assert(t->r == 0);
    assert(t->c == 0);
    // ESC[{line};{column}H moves cursor to line #, column #
    vt_emu_put_str(e, (vt_char_t*)"\033[4;5H");
    assert(t->r == 3);
    assert(t->c == 4);
    // ESC[{line};{column}f	moves cursor to line #, column #
    vt_emu_put_str(e, (vt_char_t*)"\033[7;13f");
    assert(t->r == 6);
    assert(t->c == 12);
    // ESC[H	moves cursor to home position (0, 0)
    vt_emu_put_str(e, (vt_char_t*)"\033[H");
    assert(t->r == 0);
    assert(t->c == 0);
    // ESC[#B	moves cursor down # lines
    vt_emu_put_str(e, (vt_char_t*)"\033[7B");
    assert(t->r == 7);
    assert(t->c == 0);
    // ESC[#C	moves cursor right # columns
    vt_emu_put_str(e, (vt_char_t*)"\033[13C");
    assert(t->r == 7);
    assert(t->c == 13);
    // ESC[#A	moves cursor up # lines
    vt_emu_put_str(e, (vt_char_t*)"\033[3A");
    assert(t->r == 4);
    assert(t->c == 13);
    // ESC[#D	moves cursor left # columns
    vt_emu_put_str(e, (vt_char_t*)"\033[8D");
    assert(t->r == 4);
    assert(t->c == 5);
    // ESC[#E	moves cursor to beginning of next line, # lines down
    vt_emu_put_str(e, (vt_char_t*)"\033[E");
    assert(t->r == 5);
    assert(t->c == 0);
    // ESC[#E	moves cursor to beginning of next line, # lines down
    vt_emu_put_str(e, (vt_char_t*)"\033[2E");
    assert(t->r == 7);
    assert(t->c == 0);
    // ESC[#F	moves cursor to beginning of previous line, # lines up
    vt_emu_put_str(e, (vt_char_t*)"\033[F");
    assert(t->r == 6);
    assert(t->c == 0);
    // ESC[#F	moves cursor to beginning of previous line, # lines up
    vt_emu_put_str(e, (vt_char_t*)"\033[2F");
    assert(t->r == 4);
    assert(t->c == 0);
    // ESC M	moves cursor one line up, scrolling if needed
    vt_emu_put_str(e, (vt_char_t*)"\033M");
    assert(t->r == 3);
    assert(t->c == 0);
    // Change horizontal attribute
    vt_emu_put_str(e, (vt_char_t*)"\033[11G");
    assert(t->r == 3);
    assert(t->c == 10);
    // Save & Restore ESC7, ESC8
    assert(t->r == 3);
    assert(t->c == 10);
    vt_emu_put_str(e, (vt_char_t*)"\0337");
    vt_emu_put_str(e, (vt_char_t*)"\033[4;5H");
    assert(t->r == 3);
    assert(t->c == 4);
    vt_emu_put_str(e, (vt_char_t*)"\0338");
    assert(t->r == 3);
    assert(t->c == 10);
    // Save & Restore ESC[s, ESC[u]
    assert(t->r == 3);
    assert(t->c == 10);
    vt_emu_put_str(e, (vt_char_t*)"\033[s");
    vt_emu_put_str(e, (vt_char_t*)"\033[4;5H");
    assert(t->r == 3);
    assert(t->c == 4);
    vt_emu_put_str(e, (vt_char_t*)"\033[u");
    assert(t->r == 3);
    assert(t->c == 10);
}

// ESC[J	erase in display (same as ESC[0J)
// ESC[0J	erase from cursor until end of screen
// ESC[1J	erase from cursor to beginning of screen
// ESC[2J	erase entire screen
// ESC[3J	erase saved lines
// ESC[K	erase in line (same as ESC[0K)
// ESC[0K	erase from cursor to end of line
// ESC[1K	erase start of line to the cursor
// ESC[2K	erase the entire line
void test_erase(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);
    
    printf("Test Erase\n");
    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[J	erase in display (same as ESC[0J)
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[J");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if (r <  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
        if (r == 4) check_grid_row(t, r, "ABCDE               ");
        if (r >  4) check_grid_row(t, r, "                    ");
    }

    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[J	erase in display (same as ESC[0J)
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[0J");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if (r <  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
        if (r == 4) check_grid_row(t, r, "ABCDE               ");
        if (r >  4) check_grid_row(t, r, "                    ");
    }

    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[1J	erase from cursor to beginning of screen
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[1J");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if (r <  4) check_grid_row(t, r, "                    ");
        if (r == 4) check_grid_row(t, r, "      GHIJKLMNOPQRST");
        if (r >  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
    }

    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[2J	erase entire screen
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[2J");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        check_grid_row(t, r, "                    ");
    }

    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[2J	erase entire screen
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[3J");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        check_grid_row(t, r, "                    ");
    }



    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[K	erase in line (same as ESC[0K)
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[K");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if (r <  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
        if (r == 4) check_grid_row(t, r, "ABCDE               ");
        if (r >  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
    }

    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[0K	erase from cursor to end of line
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[0K");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if (r <  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
        if (r == 4) check_grid_row(t, r, "ABCDE               ");
        if (r >  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
    }

    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[1K	erase start of line to the cursor
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[1K");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if (r <  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
        if (r == 4) check_grid_row(t, r, "      GHIJKLMNOPQRST");
        if (r >  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
    }

    set_grid_cols(t);
    print_grid(t);
    check_grid_row(t, 4, "ABCDEFGHIJKLMNOPQRST");
    // ESC[{line};{column}H moves cursor to line #, column #
    // ESC[2K	erase the entire line
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[2K");
    print_grid(t);
    for(vt_coord_t r = 0; r < t->h; ++r) {
        if (r <  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
        if (r == 4) check_grid_row(t, r, "                    ");
        if (r >  4) check_grid_row(t, r, "ABCDEFGHIJKLMNOPQRST");
    }
}

// ESC[1;34;{...}m		Set graphics modes for cell, separated by semicolon (;).
// ESC[0m		reset all modes (styles and colors)
// ESC[1m	ESC[22m	set bold mode.
// ESC[2m	ESC[22m	set dim/faint mode.
// ESC[3m	ESC[23m	set italic mode.
// ESC[4m	ESC[24m	set underline mode.
// ESC[5m	ESC[25m	set blinking mode
// ESC[7m	ESC[27m	set inverse/reverse mode
// ESC[8m	ESC[28m	set hidden/invisible mode
// ESC[9m	ESC[29m	set strikethrough mode.
void test_cell_modes(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);
    uint8_t r1[] = {
        0b00000000,
        0b00000001,
        0b00000010,
        0b00000110,
        0b00001110,
        0b00011110,
        0b00011110,
        0b00111110,
        0b01111110,
        0b11111110,
    };
    for(int i = 0; i <= 9; ++i) {
        vt_emu_put_ch(e, 27);
        vt_emu_put_ch(e, '[');
        vt_emu_put_ch(e, '0' + i);
        vt_emu_put_ch(e, 'm');
        vt_emu_put_ch(e, 'a' + i);
        printf("Attr set test %d, expecting attribute flags: %08b\n", i, r1[i]);
        print_grid(t);
        const vt_cell_attr_t attr = vt_cell_get_attr(t->rp[0][i]);
        const vt_cell_flags_t flags = vt_cell_flags_get(attr);
        assert(flags == r1[i]);
    }
    vt_emu_put_ch(e, 13);
    vt_emu_put_ch(e, 10);
    uint8_t r2[] = {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000010,
        0b00000110,
        0b00001110,
        0b00011110,
        0b00011110,
        0b00111110,
        0b01111110,
    };
    for(int i = 9; i >= 0; --i) {
        vt_emu_put_ch(e, 27);
        vt_emu_put_ch(e, '[');
        vt_emu_put_ch(e, '2');
        vt_emu_put_ch(e, '0' + i);
        vt_emu_put_ch(e, 'm');
        vt_emu_put_ch(e, 'a' + i);
        printf("Attr clear test %d, expecting attribute flags: %08b\n", i, r2[i]);
        print_grid(t);
        const vt_cell_attr_t attr = vt_cell_get_attr(t->rp[1][9 - i]);
        const vt_cell_flags_t flags = vt_cell_flags_get(attr);
        assert(flags == r2[i]);
    }
}


void test_colours(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);
    // Color Name	Foreground Color Code	Background Color Code
    // Black        30	                    40
    // Red	        31	                    41
    // Green	    32	                    42
    // Yellow	    33	                    43
    // Blue	        34	                    44
    // Magenta	    35	                    45
    // Cyan	        36	                    46
    // White	    37	                    47
    // Default	    39	                    49
    uint8_t r1[] = {0, 1, 2, 3, 4, 5, 6, 7};
    for(int i = 0; i <= 7; ++i) {
        vt_emu_put_ch(e, 27);
        vt_emu_put_ch(e, '[');
        vt_emu_put_ch(e, '3');
        vt_emu_put_ch(e, '0' + i);
        vt_emu_put_ch(e, 'm');
        vt_emu_put_ch(e, 'a' + i);
        printf("Foreground colour test %d, expecting colour: %08b\n", i, r1[i]);
        print_grid(t);
        const vt_cell_attr_t attr = vt_cell_get_attr(t->rp[0][i]);
        const vt_cell_colour_t ci = vt_cell_fg_get(attr);
        assert(ci == r1[i]);
    }
    vt_emu_put_ch(e, 13);
    vt_emu_put_ch(e, 10);
    for(int i = 0; i <= 7; ++i) {
        vt_emu_put_ch(e, 27);
        vt_emu_put_ch(e, '[');
        vt_emu_put_ch(e, '4');
        vt_emu_put_ch(e, '0' + i);
        vt_emu_put_ch(e, 'm');
        vt_emu_put_ch(e, 'a' + i);
        printf("Background colour test %d, expecting colour: %08b\n", i, r1[i]);
        print_grid(t);
        const vt_cell_attr_t attr = vt_cell_get_attr(t->rp[1][i]);
        const vt_cell_colour_t ci = vt_cell_bg_get(attr);
        assert(ci == r1[i]);
    }
    // Color Name	    Foreground Color Code	Background Color Code
    // Bright Black	    90	                    100
    // Bright Red	    91	                    101
    // Bright Green	    92	                    102
    // Bright Yellow	93	                    103
    // Bright Blue	    94	                    104
    // Bright Magenta	95	                    105
    // Bright Cyan	    96	                    106
    // Bright White	    97	                    107
    vt_emu_put_ch(e, 13);
    vt_emu_put_ch(e, 10);
    uint8_t r2[] = {8, 9, 10, 11, 12, 13, 14, 15};
    for(int i = 0; i <= 7; ++i) {
        vt_emu_put_ch(e, 27);
        vt_emu_put_ch(e, '[');
        vt_emu_put_ch(e, '9');
        vt_emu_put_ch(e, '0' + i);
        vt_emu_put_ch(e, 'm');
        vt_emu_put_ch(e, 'a' + i);
        printf("Foreground bright colour test %d, expecting colour: %08b\n", i, r2[i]);
        print_grid(t);
        const vt_cell_attr_t attr = vt_cell_get_attr(t->rp[2][i]);
        const vt_cell_colour_t ci = vt_cell_fg_get(attr);
        assert(ci == r2[i]);
    }
    vt_emu_put_ch(e, 13);
    vt_emu_put_ch(e, 10);
    for(int i = 0; i <= 7; ++i) {
        vt_emu_put_ch(e, 27);
        vt_emu_put_ch(e, '[');
        vt_emu_put_ch(e, '1');
        vt_emu_put_ch(e, '0');
        vt_emu_put_ch(e, '0' + i);
        vt_emu_put_ch(e, 'm');
        vt_emu_put_ch(e, 'a' + i);
        printf("Background bright colour test %d, expecting colour: %08b\n", i, r2[i]);
        print_grid(t);
        const vt_cell_attr_t attr = vt_cell_get_attr(t->rp[3][i]);
        const vt_cell_colour_t ci = vt_cell_bg_get(attr);
        assert(ci == r2[i]);
    }

    // ESC Code Sequence	Description
    // ESC[38;5;{ID}m	Set foreground color.
    // ESC[48;5;{ID}m	Set background color.
    vt_emu_reset(e);
    char buf[32];
    for(int i = 0; i <= 15; ++i) {
        for(int j = 0; j <= 15; ++j) {
            vt_cell_colour_t fg = i * 16 + j;
            vt_cell_colour_t bg = 255 - fg;
            snprintf((char*)&buf, 32, "\033[38;5;%d;48;5;%dm%c", fg, bg, 'a' + i );
            vt_emu_put_str(e, (vt_char_t*)&buf);
            const vt_cell_attr_t attr = vt_cell_get_attr(t->rp[0][j]);
            const vt_cell_colour_t fgci = vt_cell_fg_get(attr);
            const vt_cell_colour_t bgci = vt_cell_bg_get(attr);
            assert(fg == fgci);
            assert(bg == bgci);
        }
        vt_emu_put_ch(e, 13);
    }
}

// ESC[@
void test_insert(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    t->c = 5;
    set_grid_cols(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[3@");
    print_grid( t);
    check_grid_row(t, 0, "ABCDE   FGHIJKLMNOPQ");
}

// CSI n S	SU	Scroll Up	Scroll whole page up by n (default 1) lines. New lines are added at the bottom. (not ANSI.SYS)
// CSI n T	SD	Scroll Down	Scroll whole page down by n (default 1) lines. New lines are added at the top. (not ANSI.SYS)
void test_scroll(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    set_grid_rows(t);
    print_grid(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 

    printf("\nScroll up 1 row\n");
    vt_emu_put_str(e, (vt_char_t*)"\033[S");
    print_grid(t);
    check_grid_rows(t, "BCDEFGHIJKLMNOP "); 

    printf("\nScroll up 2 rows\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_emu_put_str(e, (vt_char_t*)"\033[2S");
    print_grid(t);
    check_grid_rows(t, "CDEFGHIJKLMNOP  "); 

    printf("\nScroll down 1 row\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_emu_put_str(e, (vt_char_t*)"\033[T");
    print_grid(t);
    check_grid_rows(t, " ABCDEFGHIJKLMNO"); 

    printf("\nScroll down 2 rows\n");
    set_grid_rows(t);
    check_grid_rows(t, "ABCDEFGHIJKLMNOP"); 
    vt_emu_put_str(e, (vt_char_t*)"\033[2T");
    print_grid(t);
    check_grid_rows(t, "  ABCDEFGHIJKLMN"); 
}

void test_margin(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test setting margin\n");
    vt_emu_put_str(e, (vt_char_t*)"\033[5;10r");
    assert(t->mt == 4);
    assert(t->mb == 9);
    vt_emu_put_str(e, (vt_char_t*)"\033[r");
    assert(t->mt == 0);
    assert(t->mb == t->h - 1);

    // DECSTBM homes the cursor
    vt_emu_put_str(e, (vt_char_t*)"\033[5;7H\033[3;8r");
    assert(t->mt == 2 && t->mb == 7);
    assert(t->r == 0 && t->c == 0);

    // A 0 or out of range bottom means the last row
    vt_emu_put_str(e, (vt_char_t*)"\033[2;0r");
    assert(t->mt == 1 && t->mb == t->h - 1);
    vt_emu_put_str(e, (vt_char_t*)"\033[3;100r");
    assert(t->mt == 2 && t->mb == t->h - 1);

    // Invalid margins are ignored and the cursor stays put
    vt_emu_put_str(e, (vt_char_t*)"\033[5;7H\033[8;8r\033[9;3r\033[100;200r");
    assert(t->mt == 2 && t->mb == t->h - 1);
    assert(t->r == 4 && t->c == 6);

    // Cursor up/down stop at the margins
    vt_emu_put_str(e, (vt_char_t*)"\033[3;8r\033[5;7H\033[100A");
    assert(t->r == 2 && t->c == 6);
    vt_emu_put_str(e, (vt_char_t*)"\033[100B");
    assert(t->r == 7 && t->c == 6);

    vt_emu_put_str(e, (vt_char_t*)"\033[r");
}

void test_private_sequences(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test private sequences are not printed\n");
    // Hide/show cursor, alternate screen, bracketed paste and DA2 are not supported
    vt_emu_put_str(e, (vt_char_t*)"\033[?25lA\033[?25h\033[?1049h\033[?2004hB\033[>cC");
    print_grid(t);
    check_grid_row(t, 0, "ABC                 ");

    // Unsupported private variants are ignored rather than treated as the plain sequence
    vt_emu_put_str(e, (vt_char_t*)"\033[?1m\033[>4;1m\033[?s");
    assert(t->attr == vt_cell_enc_attr(VT_TERM_DEFAULT_FG, VT_TERM_DEFAULT_BG, VT_TERM_DEFAULT_FLAGS));

    // DECSEL acts as EL
    vt_emu_put_str(e, (vt_char_t*)"\033[1;2H\033[?K");
    check_grid_row(t, 0, "A                   ");
}

void test_modes(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test modes\n");
    assert(t->flags == VT_TERM_MODES_DEFAULT);

    // DECAWM off: overwrite the last column
    vt_emu_put_str(e, (vt_char_t*)"\033[?7l\033[1;19Habcd");
    check_grid_row(t, 0, "                  ad");
    check_grid_row(t, 1, "                    ");
    // DECAWM on: wrap
    vt_emu_put_str(e, (vt_char_t*)"\033[?7h\033[1;19Habcd");
    check_grid_row(t, 0, "                  ab");
    check_grid_row(t, 1, "cd                  ");

    // DECOM: cursor addressing relative to the margins
    vt_emu_put_str(e, (vt_char_t*)"\033[5;10r\033[?6h");
    assert(t->r == 4 && t->c == 0);
    vt_emu_put_str(e, (vt_char_t*)"\033[2;3H");
    assert(t->r == 5 && t->c == 2);
    vt_emu_put_str(e, (vt_char_t*)"\033[100;3H");
    assert(t->r == 9 && t->c == 2);
    vt_emu_put_str(e, (vt_char_t*)"\033[?6l");
    assert(t->r == 0 && t->c == 0);
    vt_emu_put_str(e, (vt_char_t*)"\033[r");

    // Several modes in one sequence
    vt_emu_put_str(e, (vt_char_t*)"\033[?6;7l\033[4;20h");
    assert(t->flags == (VT_TERM_MODE_INSERT | VT_TERM_MODE_LNM | VT_TERM_MODE_CURSOR));
    vt_emu_put_str(e, (vt_char_t*)"\033[4;20l\033[?7h");
    assert(t->flags == VT_TERM_MODES_DEFAULT);

    // DECTCEM: cursor visibility
    vt_emu_put_str(e, (vt_char_t*)"\033[?25l");
    assert(!vt_term_mode_get(t, VT_TERM_MODE_CURSOR));
    vt_emu_put_str(e, (vt_char_t*)"\033[?25h");
    assert(vt_term_mode_get(t, VT_TERM_MODE_CURSOR));

    // IRM: insert rather than overwrite
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"abcd\r\033[4hxy\033[4lz");
    check_grid_row(t, 0, "xyzbcd              ");

    // LNM off: LF keeps the column, VT and FF act as LF
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"ab\nc\vd\fe");
    check_grid_row(t, 0, "ab                  ");
    check_grid_row(t, 1, "  c                 ");
    check_grid_row(t, 2, "   d                ");
    check_grid_row(t, 3, "    e               ");
    // LNM on: LF also does CR
    vt_emu_put_str(e, (vt_char_t*)"\033[20h\nf\033[20l");
    check_grid_row(t, 4, "f                   ");
    // CR LF works either way
    vt_emu_put_str(e, (vt_char_t*)"\r\ng");
    check_grid_row(t, 5, "g                   ");
}

void test_repeat_character(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test repeat character\n");
    vt_emu_put_ch(e, 'a');
    vt_emu_put_str(e, (vt_char_t*)"\033[b");
    print_grid(t);
    check_grid_row(t, 0, "aa                  ");
    vt_emu_put_str(e, (vt_char_t*)"\033[3b");
    print_grid(t);//      01234567890123456789
    check_grid_row(t, 0, "aaaaa               ");
}

void test_horizontal_tabs(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test tabs\n");
    print_tabs(&e->tabs);

    vt_emu_put_str(e, (vt_char_t*)"\033[I");
    assert(t->c == 8);
    vt_emu_put_str(e, (vt_char_t*)"\x09");
    assert(t->c == 16);
    vt_emu_put_str(e, (vt_char_t*)"\033[Z");
    assert(t->c == 8);
    vt_emu_put_ch(e, '\r');
    vt_emu_put_str(e, (vt_char_t*)"\033[2I");
    assert(t->c == 16);
    vt_emu_put_str(e, (vt_char_t*)"\033[0;19H");
    vt_emu_put_str(e, (vt_char_t*)"\033[2Z");
    assert(t->c == 8);

    vt_emu_put_ch(e, 'a');
    vt_emu_put_str(e, (vt_char_t*)"\033H");
    print_tabs(&e->tabs);
    vt_emu_put_ch(e, 'b');
    vt_emu_put_str(e, (vt_char_t*)"\033[Z");
    assert(t->c == 9);

    // Clear all tabs
    printf("Test clear all tabs\n");
    vt_emu_put_str(e, (vt_char_t*)"\033[3g");
    print_tabs(&e->tabs);
    vt_emu_put_str(e, (vt_char_t*)"\033[I");
    assert(t->c == 9);
    vt_emu_put_str(e, (vt_char_t*)"\033[Z");
    assert(t->c == 9);    

}

void test_stdin(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    setvbuf(stdin, NULL, _IONBF, 0);

    char ch;
    while((ch = getchar()) != EOF) {
        printf("character %d\n", ch);
        vt_emu_put_ch(e, ch);
        print_grid(t);
    }
}

// Collects replies sent to the host
static char reply_buf[64];
static uint32_t reply_len;

static void capture_reply(void* ctx, const vt_char_t* s, uint32_t n) {
    assert(ctx == reply_buf);
    assert(reply_len + n < sizeof(reply_buf));
    memcpy(reply_buf + reply_len, s, n);
    reply_len += n;
    reply_buf[reply_len] = 0;
}

// Send a sequence and check the reply (empty for none)
static void check_reply(vt_emu_t* e, const char* send, const char* expect) {
    reply_len = 0;
    reply_buf[0] = 0;
    vt_emu_put_str(e, (vt_char_t*)send);
    assert(strcmp(reply_buf, expect) == 0);
}

void test_replies(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test replies\n");
    // Without a reply function nothing is sent (and nothing breaks)
    vt_emu_put_str(e, (vt_char_t*)"\033[c\033[6n");

    vt_emu_set_reply(e, capture_reply, reply_buf);

    // Primary device attributes
    check_reply(e, "\033[c", "\033[?1;2c");
    check_reply(e, "\033[0c", "\033[?1;2c");
    check_reply(e, "\033[1c", "");
    check_reply(e, "\033[>c", "");

    // Device status and cursor position reports
    check_reply(e, "\033[5n", "\033[0n");
    check_reply(e, "\033[6n", "\033[1;1R");
    check_reply(e, "\033[12;17H\033[6n", "\033[12;17R");
    check_reply(e, "\033[3n", "");

    // In origin mode the row is relative to the top margin
    check_reply(e, "\033[5;10r\033[?6h\033[3;4H\033[6n", "\033[3;4R");
    assert(t->r == 6);
    check_reply(e, "\033[?6l\033[r", "");

    // Default colour queries (OSC 10/11) are answered with the query's terminator
    char q[64];
    uint32_t r, g, b;
    vt_pallet_get_rgb(VT_TERM_DEFAULT_FG, &r, &g, &b);
    snprintf(q, sizeof(q), "\033]10;rgb:%04x/%04x/%04x\007", (unsigned)(r * 257), (unsigned)(g * 257), (unsigned)(b * 257));
    check_reply(e, "\033]10;?\007", q);
    vt_pallet_get_rgb(VT_TERM_DEFAULT_BG, &r, &g, &b);
    snprintf(q, sizeof(q), "\033]11;rgb:%04x/%04x/%04x\033\\", (unsigned)(r * 257), (unsigned)(g * 257), (unsigned)(b * 257));
    check_reply(e, "\033]11;?\033\\", q);
    // Only queries are answered
    check_reply(e, "\033]11;rgb:1234/5678/9abc\007", "");
    check_reply(e, "\033]1;?\007", "");
    check_reply(e, "\033]110;?\007", "");

    // Reset keeps the reply function
    vt_emu_reset(e);
    check_reply(e, "\033[6n", "\033[1;1R");

    vt_emu_set_reply(e, NULL, NULL);
}

void test_line_and_char_edits(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test line and character edits\n");
    // IL and DL (default 1)
    set_grid_rows(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[6;8H\033[L");
    print_grid(t);
    assert(t->r == 5 && t->c == 0);
    check_grid_rows(t, "ABCDE FGHIJKLMNO");
    vt_emu_put_str(e, (vt_char_t*)"\033[2M");
    print_grid(t);
    check_grid_rows(t, "ABCDEGHIJKLMNO  ");

    // ICH, DCH and ECH (default 1)
    set_grid_cols(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[1;6H\033[@");
    check_grid_row(t, 0, "ABCDE FGHIJKLMNOPQRS");
    set_grid_cols(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[P");
    check_grid_row(t, 0, "ABCDEGHIJKLMNOPQRST ");
    vt_emu_put_str(e, (vt_char_t*)"\033[2P");
    check_grid_row(t, 0, "ABCDEIJKLMNOPQRST   ");
    set_grid_cols(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[X");
    check_grid_row(t, 0, "ABCDE GHIJKLMNOPQRST");
    vt_emu_put_str(e, (vt_char_t*)"\033[3X");
    check_grid_row(t, 0, "ABCDE   IJKLMNOPQRST");
    assert(t->r == 0 && t->c == 5);

    // VPA (default 1) and HPA
    vt_emu_put_str(e, (vt_char_t*)"\033[9d");
    assert(t->r == 8 && t->c == 5);
    vt_emu_put_str(e, (vt_char_t*)"\033[d");
    assert(t->r == 0 && t->c == 5);
    vt_emu_put_str(e, (vt_char_t*)"\033[100d");
    assert(t->r == t->h - 1 && t->c == 5);
    vt_emu_put_str(e, (vt_char_t*)"\033[7`");
    assert(t->r == t->h - 1 && t->c == 6);

    // VPA is relative to the margins in origin mode
    // Huge parameters clamp to the screen rather than wrapping around
    vt_emu_put_str(e, (vt_char_t*)"\033[65537;65537H");
    assert(t->r == t->h - 1 && t->c == t->w - 1);
    vt_emu_put_str(e, (vt_char_t*)"\033[H\033[99999999999999999999d\033[4294967297G");
    assert(t->r == t->h - 1 && t->c == t->w - 1);
    vt_emu_put_str(e, (vt_char_t*)"\033[H\033[65537B\033[65537C");
    assert(t->r == t->h - 1 && t->c == t->w - 1);
    vt_emu_put_str(e, (vt_char_t*)"\033[65537A\033[65537D");
    assert(t->r == 0 && t->c == 0);
    vt_emu_put_str(e, (vt_char_t*)"\033[2;65537r");
    assert(t->mt == 1 && t->mb == t->h - 1);
    vt_emu_put_str(e, (vt_char_t*)"\033[r");

    vt_emu_put_str(e, (vt_char_t*)"\033[5;10r\033[?6h\033[2d");
    assert(t->r == 5);
    vt_emu_put_str(e, (vt_char_t*)"\033[100d");
    assert(t->r == 9);
    vt_emu_put_str(e, (vt_char_t*)"\033[?6l\033[r");
}

void test_sgr_colours_extended(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test SGR default and 24 bit colours\n");
    // ESC [ m with no parameters resets everything, as ESC [ 0 m
    vt_emu_put_str(e, (vt_char_t*)"\033[1;4;94;45m\033[m");
    assert(t->attr == vt_cell_enc_attr(VT_TERM_DEFAULT_FG, VT_TERM_DEFAULT_BG, VT_TERM_DEFAULT_FLAGS));
    // ...and so does an empty first parameter
    vt_emu_put_str(e, (vt_char_t*)"\033[1;94m\033[;4m");
    assert(t->attr == vt_cell_enc_attr(VT_TERM_DEFAULT_FG, VT_TERM_DEFAULT_BG, VT_CELL_FLAGS_UNDERLINE));
    vt_emu_put_str(e, (vt_char_t*)"\033[m");

    // 39/49 restore the default foreground/background
    vt_emu_put_str(e, (vt_char_t*)"\033[33;44m\033[49m");
    assert(vt_cell_fg_get(t->attr) == 3);
    assert(vt_cell_bg_get(t->attr) == VT_TERM_DEFAULT_BG);
    vt_emu_put_str(e, (vt_char_t*)"\033[39m");
    assert(vt_cell_fg_get(t->attr) == VT_TERM_DEFAULT_FG);

    // 24 bit colour maps to the nearest palette colour and its values are not read as SGR codes
    vt_emu_put_str(e, (vt_char_t*)"\033[0;38;2;1;4;5m");
    assert(vt_cell_fg_get(t->attr) == 16);
    assert(vt_cell_flags_get(t->attr) == VT_CELL_FLAGS_NORMAL);
    vt_emu_put_str(e, (vt_char_t*)"\033[38;2;255;0;0;48;2;128;128;128;1m");
    assert(vt_cell_fg_get(t->attr) == 196);
    assert(vt_cell_bg_get(t->attr) == 244);
    assert(vt_cell_flags_get(t->attr) == VT_CELL_FLAGS_BOLD);

    // 256 colours, including codes after them
    vt_emu_put_str(e, (vt_char_t*)"\033[0;38;5;123;48;5;45;4m");
    assert(vt_cell_fg_get(t->attr) == 123);
    assert(vt_cell_bg_get(t->attr) == 45);
    assert(vt_cell_flags_get(t->attr) == VT_CELL_FLAGS_UNDERLINE);

    // Out of range or incomplete colours are ignored
    vt_emu_put_str(e, (vt_char_t*)"\033[0;38;5;300m");
    assert(vt_cell_fg_get(t->attr) == VT_TERM_DEFAULT_FG);
    vt_emu_put_str(e, (vt_char_t*)"\033[0;38;2;10;20m");
    assert(vt_cell_fg_get(t->attr) == VT_TERM_DEFAULT_FG);
    assert(vt_cell_flags_get(t->attr) == VT_CELL_FLAGS_NORMAL);
    // An unknown colour model stops the rest being misread
    vt_emu_put_str(e, (vt_char_t*)"\033[0;38;3;1;4m");
    assert(vt_cell_fg_get(t->attr) == VT_TERM_DEFAULT_FG);
    assert(vt_cell_flags_get(t->attr) == VT_CELL_FLAGS_NORMAL);

    vt_emu_put_str(e, (vt_char_t*)"\033[0m");
}

void test_index_next_line(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test IND and NEL\n");
    // IND moves down keeping the column, NEL also returns to column 0
    vt_emu_put_str(e, (vt_char_t*)"ab\033Dc");
    check_grid_row(t, 0, "ab                  ");
    check_grid_row(t, 1, "  c                 ");
    vt_emu_put_str(e, (vt_char_t*)"\033Ed");
    check_grid_row(t, 2, "d                   ");

    // IND never returns to column 0, even in LNM mode
    vt_emu_put_str(e, (vt_char_t*)"\033[20h\033Dx\033[20l");
    check_grid_row(t, 3, " x                  ");

    // Both scroll the region at the bottom margin
    set_grid_rows(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[3;6r\033[6;5H\033D");
    assert(t->r == 5 && t->c == 4);
    check_grid_rows(t, "ABDEF GHIJKLMNOP");
    vt_emu_put_str(e, (vt_char_t*)"\033E");
    assert(t->r == 5 && t->c == 0);
    check_grid_rows(t, "ABEF  GHIJKLMNOP");
    vt_emu_put_str(e, (vt_char_t*)"\033[r");
}

void test_reset_to_initial_state(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test RIS\n");
    vt_emu_put_str(e, (vt_char_t*)"abc\033[3;8r\033[?7l\033[4h\033[1;31m\033[3g\033[5;6H");
    vt_emu_put_str(e, (vt_char_t*)"\033c");
    check_grid_blank(t);
    assert(t->r == 0 && t->c == 0);
    assert(t->mt == 0 && t->mb == t->h - 1);
    assert(t->flags == VT_TERM_MODES_DEFAULT);
    assert(t->attr == vt_cell_enc_attr(VT_TERM_DEFAULT_FG, VT_TERM_DEFAULT_BG, VT_TERM_DEFAULT_FLAGS));
    // Tab stops are back to every 8 columns
    vt_emu_put_str(e, (vt_char_t*)"\t");
    assert(t->c == 8);
}

// A CP437 style map: box drawing lines get their own glyphs
static vt_char_t test_charmap(uint32_t cp) {
    if (cp == 0x2500) return 0xC4;
    if (cp == 0x2502) return 0xB3;
    return vt_charmap_ascii(cp);
}

void test_utf8(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test UTF-8\n");
    // Accented letters, and Cyrillic whose bytes include C1 values (not controls)
    vt_emu_put_str(e, (vt_char_t*)"caf\xC3\xA9 \xD1\x8D\xD0\x9B!");
    print_grid(t);
    check_grid_row(t, 0, "cafe ??" "!            ");
    assert(t->r == 0 && t->c == 8);

    // Raw C1 bytes are invalid UTF-8, not CSI or other controls
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"a\x9B" "2Jb\x84" "c");
    check_grid_row(t, 0, "a?2Jb?c             ");

    // Combining marks take no space
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"e\xCC\x81x");
    check_grid_row(t, 0, "ex                  ");

    // Wide characters take two cells
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"a\xE4\xB8\xAD" "b");
    check_grid_row(t, 0, "a? b                ");
    assert(t->c == 4);
    // ...and wrap rather than split at the end of a line
    vt_emu_put_str(e, (vt_char_t*)"\033[1;20H\xE4\xB8\xAD" "c");
    check_grid_row(t, 0, "a? b                ");
    check_grid_row(t, 1, "? c                 ");
    // ...unless auto wrap is off
    vt_emu_put_str(e, (vt_char_t*)"\033[?7l\033[3;20H\xE4\xB8\xAD\033[?7h");
    check_grid_row(t, 2, "                   ?");
    assert(t->r == 2 && t->c == t->w - 1);

    // Invalid and interrupted sequences show a replacement and the interruption still works
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"\xFF\xE2\033[1mA");
    check_grid_row(t, 0, "??A                 ");
    assert(vt_cell_flags_get(vt_cell_get_attr(t->rp[0][2])) == VT_CELL_FLAGS_BOLD);
    vt_emu_put_str(e, (vt_char_t*)"\033[m");

    // A title ended with ESC \ (ST) doesn't swallow what follows
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"\033]0;my title\033\\after");
    check_grid_row(t, 0, "after               ");

    // UTF-8 within an OSC title is not displayed
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"\033]0;t\xC3\xAFtle\x07X");
    check_grid_row(t, 0, "X                   ");

    // A custom character map
    vt_emu_reset(e);
    vt_emu_set_charmap(e, test_charmap);
    vt_emu_put_str(e, (vt_char_t*)"\xE2\x94\x80\xE2\x94\x82\xE2\x94\x8C");
    assert(vt_cell_get_char(t->rp[0][0]) == 0xC4);
    assert(vt_cell_get_char(t->rp[0][1]) == 0xB3);
    assert(vt_cell_get_char(t->rp[0][2]) == '+');
    // Reset keeps it
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"\033(0q\033(B");
    assert(vt_cell_get_char(t->rp[0][0]) == 0xC4);
    vt_emu_set_charmap(e, vt_charmap_ascii);
}

void test_dec_graphics(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test DEC special graphics\n");
    // G0 designated as line drawing; characters outside 0x5F-0x7E are unchanged
    vt_emu_put_str(e, (vt_char_t*)"\033(0lqkAx\033(Blqk");
    print_grid(t);
    check_grid_row(t, 0, "+-+A|lqk            ");

    // G1 with shift out/in
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"\033)0q\x0Eq\x0Fq");
    check_grid_row(t, 0, "q-q                 ");

    // The character sets are saved and restored with the cursor (which returns to column 0)
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"\033(0\0337\033(Bq");
    check_grid_row(t, 0, "q                   ");
    vt_emu_put_str(e, (vt_char_t*)"\0338q");
    check_grid_row(t, 0, "-                   ");

    // Reset returns to ASCII
    vt_emu_put_str(e, (vt_char_t*)"\033(0\033cq");
    check_grid_row(t, 0, "q                   ");
}

void test_controls_in_sequences(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_emu_reset(e);

    printf("Test C0 controls within sequences\n");
    // BS is executed part way through the CUP
    vt_emu_put_str(e, (vt_char_t*)"a\033[1;\b2Hb");
    check_grid_row(t, 0, "ab                  ");
    assert(t->r == 0 && t->c == 2);

    // CR within ED, and LF within a private mode sequence
    vt_emu_put_str(e, (vt_char_t*)"\033[2\rJ");
    check_grid_row(t, 0, BLANK_ROW);
    assert(t->r == 0 && t->c == 0);
    vt_emu_put_str(e, (vt_char_t*)"\033[?\n25l");
    assert(t->r == 1);
    assert(!vt_term_mode_get(t, VT_TERM_MODE_CURSOR));
    vt_emu_put_str(e, (vt_char_t*)"\033[?25h");

    // DEL is ignored
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"\033[1;\x7F" "3Hz");
    check_grid_row(t, 0, "  z                 ");

    // ESC starts a new sequence
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"x\033[1\033[2Jy");
    check_grid_row(t, 0, " y                  ");

    // CAN and SUB abandon the sequence
    vt_emu_reset(e);
    vt_emu_put_str(e, (vt_char_t*)"a\033[2\x18" "b\033[1\x1A" "c");
    check_grid_row(t, 0, "abc                 ");
}

void test_alt_screen(vt_emu_t* e) {
    vt_term_t* const t = &e->term;
    vt_cell_t alt[16][20];
    vt_emu_reset(e);

    printf("Test alternate screen\n");
    // Without an alternate screen 1049 still saves and restores the cursor, and doesn't clear
    vt_emu_put_str(e, (vt_char_t*)"main\033[3;4H\033[?1049h");
    assert(!t->alt_active);
    check_grid_row(t, 0, "main                ");
    vt_emu_put_str(e, (vt_char_t*)"\033[H\033[?1049l");
    assert(t->r == 2 && t->c == 3);

    vt_emu_set_alt_grid(e, (vt_cell_t*)alt);

    // 1049: save the cursor, switch and clear; then switch back and restore the cursor
    vt_emu_put_str(e, (vt_char_t*)"\033[1;31m\033[?1049h");
    assert(t->alt_active);
    check_grid_blank_chars(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[m\033[Hfull screen\033[?1049l");
    assert(!t->alt_active);
    check_grid_row(t, 0, "main                ");
    assert(t->r == 2 && t->c == 3);
    assert(vt_cell_fg_get(t->attr) == 1 && vt_cell_flags_get(t->attr) == VT_CELL_FLAGS_BOLD);
    vt_emu_put_str(e, (vt_char_t*)"\033[m");

    // 1049 clears the alternate screen each time
    vt_emu_put_str(e, (vt_char_t*)"\033[?1049h");
    check_grid_blank_chars(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[?1049l");

    // 47 switches without clearing or moving the cursor
    vt_emu_put_str(e, (vt_char_t*)"\033[?47hx\033[?47l");
    check_grid_row(t, 0, "main                ");
    assert(t->r == 2 && t->c == 4);
    vt_emu_put_str(e, (vt_char_t*)"\033[?47h");
    assert(vt_cell_get_char(t->rp[2][3]) == 'x');

    // 1047 clears the alternate screen when leaving it
    vt_emu_put_str(e, (vt_char_t*)"\033[?47l\033[?1047h");
    assert(vt_cell_get_char(t->rp[2][3]) == 'x');
    vt_emu_put_str(e, (vt_char_t*)"\033[?1047l\033[?47h");
    check_grid_blank_chars(t);
    vt_emu_put_str(e, (vt_char_t*)"\033[?47l");

    // 1048 saves and restores the cursor
    vt_emu_put_str(e, (vt_char_t*)"\033[5;6H\033[?1048h\033[H\033[?1048l");
    assert(t->r == 4 && t->c == 5);

    // Reset returns to the normal screen and keeps the alternate one
    vt_emu_put_str(e, (vt_char_t*)"\033[?1049h\033c");
    assert(!t->alt_active && t->alt);

    vt_emu_set_alt_grid(e, NULL);
}

static vt_coord_t req_w, req_h;
static uint32_t req_n;

// A display that accepts whatever size the host asks for
static void accept_resize(void* ctx, vt_coord_t w, vt_coord_t h) {
    req_w = w;
    req_h = h;
    req_n++;
    vt_emu_resize((vt_emu_t*)ctx, w, h);
}

// Resize from the display without a sequence, checking what the host is told
static void check_resize_reply(vt_emu_t* e, vt_coord_t w, vt_coord_t h, const char* expect) {
    reply_len = 0;
    reply_buf[0] = 0;
    vt_emu_resize(e, w, h);
    assert(strcmp(reply_buf, expect) == 0);
}

void test_resize() {
    vt_cell_t grid[16][20];
    vt_emu_t ee;
    vt_emu_t* const e = &ee;
    vt_term_t* const t = &e->term;

    printf("Test resize\n");
    // A 10 x 6 terminal that can grow to 20 x 16
    vt_emu_init_cap(e, (vt_cell_t*)grid, 20, 16, 10, 6);
    assert(t->w == 10 && t->h == 6);
    vt_emu_set_reply(e, capture_reply, reply_buf);

    // Size reports: the text area and the most it could be
    check_reply(e, "\033[18t", "\033[8;6;10t");
    check_reply(e, "\033[19t", "\033[9;16;20t");
    // Pixel sizes are only reported when the cell size is known
    check_reply(e, "\033[14t", "");
    check_reply(e, "\033[16t", "");
    vt_emu_set_cell_size(e, 8, 12);
    check_reply(e, "\033[14t", "\033[4;72;80t");
    check_reply(e, "\033[16t", "\033[6;12;8t");
    // Other window operations and private variants are ignored
    check_reply(e, "\033[11t\033[>18t\033[?18t", "");

    // Without a resize function the host's requests are ignored
    check_reply(e, "\033[8;7;12t", "");
    assert(t->w == 10 && t->h == 6);

    // The display decides
    vt_emu_set_resize_req(e, accept_resize, e);
    check_reply(e, "\033[8;7;12t", "");
    assert(req_n == 1 && req_w == 12 && req_h == 7);
    assert(t->w == 12 && t->h == 7);
    // Omitted sizes are unchanged
    check_reply(e, "\033[8t", "");
    assert(req_n == 2 && req_w == 12 && req_h == 7);
    check_reply(e, "\033[8;9t", "");
    assert(req_w == 12 && req_h == 9);
    // 0, or too big, is the most there is
    check_reply(e, "\033[8;0;0t", "");
    assert(req_w == 20 && req_h == 16);
    check_reply(e, "\033[8;99;5t", "");
    assert(req_w == 5 && req_h == 16);
    assert(t->w == 5 && t->h == 16);

    // The text after a request goes on the resized screen
    vt_emu_put_str(e, (vt_char_t*)"\033[8;6;10t\033[H\033[2Jhello world");
    assert(t->w == 10 && t->h == 6);
    check_grid_row(t, 0, "hello worl");
    check_grid_row(t, 1, "d         ");

    // In-band resize notifications (?2048) start with the size now
    check_reply(e, "\033[?2048h", "\033[48;6;10;72;80t");
    check_resize_reply(e, 20, 16, "\033[48;16;20;192;160t");
    // Only when the size changes
    check_resize_reply(e, 20, 16, "");
    check_resize_reply(e, 30, 30, "");
    // Including when the host asked for it
    check_reply(e, "\033[8;6;10t", "\033[48;6;10;72;80t");
    // Turned off by the host or a reset
    check_reply(e, "\033[?2048l", "");
    check_resize_reply(e, 12, 7, "");
    check_reply(e, "\033[?2048h", "\033[48;7;12;84;96t");
    vt_emu_reset(e);
    check_resize_reply(e, 10, 6, "");
    // Reset keeps the resize function and the size
    check_reply(e, "\033[8;6;11t", "");
    assert(req_w == 11 && t->w == 11 && t->h == 6);

    // The tab stops follow the width
    vt_emu_resize(e, 20, 6);
    vt_emu_put_str(e, (vt_char_t*)"\033[H\t\t");
    assert(t->c == 16);
    vt_emu_put_str(e, (vt_char_t*)"\t");
    assert(t->c == 19);
    vt_emu_resize(e, 10, 6);
    assert(t->c == 9);
    vt_emu_put_str(e, (vt_char_t*)"\033[H\t\t");
    assert(t->c == 9);
}

int main() {

    const vt_coord_t w = 20;
    const vt_coord_t h = 16;
    vt_cell_t grid[h][w];
    vt_emu_t e;
    vt_emu_init(&e, (vt_cell_t*)grid, w, h);

    test_cursor(&e);
    test_erase(&e);
    test_cell_modes(&e);
    test_colours(&e);
    test_insert(&e);
    test_scroll(&e);
    test_margin(&e);
    test_private_sequences(&e);
    test_modes(&e);
    test_replies(&e);
    test_line_and_char_edits(&e);
    test_sgr_colours_extended(&e);
    test_index_next_line(&e);
    test_reset_to_initial_state(&e);
    test_utf8(&e);
    test_dec_graphics(&e);
    test_controls_in_sequences(&e);
    test_alt_screen(&e);
    test_repeat_character(&e);
    test_horizontal_tabs(&e);
    test_resize();
    // test_stdin(&e);

    printf("All OK\n");
}
