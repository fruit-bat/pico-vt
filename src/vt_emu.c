

#include "vt_emu.h"
#include "vt_pallet.h"
#include <stdio.h>
#include <string.h>

//#define DEBUG(...) printf(__VA_ARGS__)
#define DEBUG(...) 
typedef unsigned long ul;

static void vt_emu_reset_text(vt_emu_t *e);
uint32_t vt_emu_get_p0(vt_parser_t* p, uint32_t i);

void vt_emu_init(
    vt_emu_t* e,     // Emulator
    vt_cell_t* grid, // Cell grid for the display
    vt_coord_t w,    // Terminal width in characters
    vt_coord_t h     // Terminal height in characters
) {
    vt_emu_init_cap(e, grid, w, h, w, h);
}

void vt_emu_init_cap(
    vt_emu_t* e,     // Emulator
    vt_cell_t* grid, // Cell grid for the display, cw x ch
    vt_coord_t cw,   // Capacity width in characters
    vt_coord_t ch,   // Capacity height in characters
    vt_coord_t w,    // Terminal width in characters
    vt_coord_t h     // Terminal height in characters
) {
    vt_parser_init(
        &e->parser
    );
    vt_term_init_cap(
        &e->term,
        grid,
        cw,
        ch,
        w,
        h
    );
    vt_tabs_init(
        &e->tabs,
        e->term.w,
        8
    );
    vt_emu_set_reply(e, NULL, NULL);
    vt_emu_set_resize_req(e, NULL, NULL);
    vt_emu_set_cell_size(e, 0, 0);
    vt_emu_set_charmap(e, vt_charmap_ascii);
    vt_emu_reset_text(e);
    e->resize_report = false;
}

void vt_emu_set_resize_req(
    vt_emu_t *e,
    vt_emu_resize_req_t resize_req,
    void* ctx
) {
    e->resize_req = resize_req;
    e->resize_req_ctx = ctx;
}

void vt_emu_set_cell_size(
    vt_emu_t *e,
    uint16_t pw,
    uint16_t ph
) {
    e->cell_pw = pw;
    e->cell_ph = ph;
}

void vt_emu_set_charmap(
    vt_emu_t *e,
    vt_charmap_t charmap
) {
    e->charmap = charmap;
}

void vt_emu_set_alt_grid(
    vt_emu_t *e,
    vt_cell_t* grid
) {
    vt_term_alt_init(&e->term, grid);
}

// Write a non-ASCII code point, taking its width into account
static void vt_emu_put_cp(
    vt_emu_t *e,
    uint32_t cp
) {
    vt_term_t* const t = &e->term;
    const uint32_t w = vt_charmap_width(cp);
    if (w == 0) return; // Combining marks and controls are not shown
    const vt_char_t g = e->charmap(cp);
    if (w == 2 && !t->wrap && t->c == t->w - 1) {
        // A wide character doesn't fit in the last column
        if (vt_term_mode_get(t, VT_TERM_MODE_AUTOWRAP)) {
            vt_term_putch(t, ' '); // Leave it blank and wrap
        }
        else {
            vt_term_putch(t, g);   // No wrap: squeeze it into the last column
            return;
        }
    }
    vt_term_putch(t, g);
    if (w == 2) vt_term_putch(t, ' '); // The second cell of a wide character
}

void vt_emu_set_reply(
    vt_emu_t *e,
    vt_emu_reply_t reply,
    void* ctx
) {
    e->reply = reply;
    e->reply_ctx = ctx;
}

// Append a decimal number to s, returning the new end
static char* vt_emu_put_dec(
    char *s,
    uint32_t v
) {
    char tmp[10];
    int n = 0;
    do { tmp[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) *s++ = tmp[--n];
    return s;
}

// Append a 16 bit colour component (from 8 bits) as 4 hex digits, returning the new end
static char* vt_emu_put_hex16(
    char *s,
    uint32_t v8
) {
    static const char hex[] = "0123456789abcdef";
    const uint32_t v = v8 * 257; // 0xAB -> 0xABAB
    for (int shift = 12; shift >= 0; shift -= 4) *s++ = hex[(v >> shift) & 0xF];
    return s;
}

static void vt_emu_reply(
    vt_emu_t *e,
    const char *s
) {
    if (e->reply) e->reply(e->reply_ctx, (const vt_char_t*)s, (uint32_t)strlen(s));
}

// Reply with ESC [ v0 ; v1 ; ... t (the form of the window operation reports)
static void vt_emu_reply_csi_t(
    vt_emu_t *e,
    const uint32_t *v,
    uint32_t n
) {
    char s[64];
    char *q = s;
    *q++ = '\033';
    *q++ = '[';
    for (uint32_t i = 0; i < n; ++i) {
        if (i) *q++ = ';';
        q = vt_emu_put_dec(q, v[i]);
    }
    *q++ = 't';
    *q = 0;
    vt_emu_reply(e, s);
}

// In-band resize notification (mode ?2048): ESC [ 48 ; h ; w ; hpx ; wpx t
static void vt_emu_report_size(
    vt_emu_t *e
) {
    const vt_term_t* const t = &e->term;
    const uint32_t v[] = {48, t->h, t->w, (uint32_t)t->h * e->cell_ph, (uint32_t)t->w * e->cell_pw};
    vt_emu_reply_csi_t(e, v, 5);
}

void vt_emu_resize(
    vt_emu_t *e,
    vt_coord_t w,
    vt_coord_t h
) {
    vt_term_t* const t = &e->term;
    const vt_coord_t ow = t->w;
    const vt_coord_t oh = t->h;
    vt_term_resize(t, w, h);
    vt_tabs_resize(&e->tabs, t->w, 8);
    if (e->resize_report && (t->w != ow || t->h != oh)) vt_emu_report_size(e);
}

// XTWINOPS (ESC [ n ; ... t): the size requests and reports
static void vt_emu_window_ops(
    vt_emu_t *e
) {
    vt_term_t* const t = &e->term;
    vt_parser_t* const p = &e->parser;
    const bool px = e->cell_pw && e->cell_ph; // The cell size in pixels is known
    switch (vt_emu_get_p0(p, 0)) {
        case 8: { // Resize to h x w: an omitted size is unchanged, 0 is the most there is
            uint32_t h = p->n_params > 1 ? p->params[1] : t->h;
            uint32_t w = p->n_params > 2 ? p->params[2] : t->w;
            if (h == 0 || h > t->ch) h = t->ch;
            if (w == 0 || w > t->cw) w = t->cw;
            DEBUG("XTWINOPS resize %lu x %lu\n", (ul)w, (ul)h);
            if (e->resize_req) e->resize_req(e->resize_req_ctx, (vt_coord_t)w, (vt_coord_t)h);
            break;
        }
        case 14: { // Report the text area size in pixels: ESC [ 4 ; hpx ; wpx t
            const uint32_t v[] = {4, (uint32_t)t->h * e->cell_ph, (uint32_t)t->w * e->cell_pw};
            if (px) vt_emu_reply_csi_t(e, v, 3);
            break;
        }
        case 16: { // Report the cell size in pixels: ESC [ 6 ; hpx ; wpx t
            const uint32_t v[] = {6, e->cell_ph, e->cell_pw};
            if (px) vt_emu_reply_csi_t(e, v, 3);
            break;
        }
        case 18: { // Report the text area size in characters: ESC [ 8 ; h ; w t
            const uint32_t v[] = {8, t->h, t->w};
            vt_emu_reply_csi_t(e, v, 3);
            break;
        }
        case 19: { // Report the screen size (the most there could be) in characters: ESC [ 9 ; h ; w t
            const uint32_t v[] = {9, t->ch, t->cw};
            vt_emu_reply_csi_t(e, v, 3);
            break;
        }
    }
}

// Text state that is not part of the terminal: the UTF-8 decoder and character sets
static void vt_emu_reset_text(
    vt_emu_t *e
) {
    vt_utf8_init(&e->utf8);
    e->g_dec[0] = e->g_dec[1] = false;
    e->gl = 0;
    e->s_g_dec[0] = e->s_g_dec[1] = false;
    e->s_gl = 0;
}

void vt_emu_reset(
    vt_emu_t *e
) {
    vt_emu_reset_text(e);
    vt_parser_reset(
        &e->parser
    );
    vt_term_reset(
        &e->term
    );
    e->resize_report = false;
    vt_tabs_init(
        &e->tabs,
        e->term.w,
        8
    );    
}

uint32_t vt_emu_get_p0(vt_parser_t* p, uint32_t i) {
    return p->n_params <= i ? 0 : p->params[i];
}

uint32_t vt_emu_get_p1(vt_parser_t* p, uint32_t i) {
    return p->n_params <= i || p->params[i] == 0 ? 1 : p->params[i];


}

//
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
//
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
//
// Color Name	    Foreground Color Code	Background Color Code
// Bright Black	    90	                    100
// Bright Red	    91	                    101
// Bright Green	    92	                    102
// Bright Yellow	93	                    103
// Bright Blue	    94	                    104
// Bright Magenta	95	                    105
// Bright Cyan	    96	                    106
// Bright White	    97	                    107
//
static void vt_emu_sgr(vt_emu_t* const e) {
    vt_term_t* const t = &e->term;
    vt_parser_t* const p = &e->parser;
    static const vt_cell_flags_t zfb[] = {
        VT_CELL_FLAGS_NORMAL         ,
        VT_CELL_FLAGS_BOLD           ,
        VT_CELL_FLAGS_DIM            ,
        VT_CELL_FLAGS_ITALIC         ,
        VT_CELL_FLAGS_UNDERLINE      ,
        VT_CELL_FLAGS_BLINK          ,
        VT_CELL_FLAGS_NORMAL         ,
        VT_CELL_FLAGS_REVERSE        ,
        VT_CELL_FLAGS_INVISIBLE      ,
        VT_CELL_FLAGS_STRIKETHROUGH  ,
    };

    // ESC [ m with no parameters is the same as ESC [ 0 m
    if (p->n_params == 0) {
        vt_term_reset_attr(t);
        return;
    }

    for (uint32_t i = 0; i < p->n_params; ++i) {
        const uint32_t q = vt_emu_get_p0(p, i);
        if ((q < 10) || (q >= 20 && q < 30)) {
            //
            // ESC[0m		reset all modes (styles and colors)
            // ESC[1m	ESC[22m	set bold mode.
            // ESC[2m	ESC[22m	set dim/faint mode.
            // ESC[3m	ESC[23m	set italic mode.
            // ESC[4m	ESC[24m	set underline mode.
            // ESC[5m	ESC[25m	set blinking mode
            // ESC[7m	ESC[27m	set inverse/reverse mode
            // ESC[8m	ESC[28m	set hidden/invisible mode
            // ESC[9m	ESC[29m	set strikethrough mode.
            //
            const int set = q < 20;
            const uint32_t z = set ? q : q - 20;
            switch(z) {
                case 0: {
                    if (set) vt_term_reset_attr(t); 
                    break;
                }
                case 1: {
                    if (set) {
                        vt_term_flags_add(t, VT_CELL_FLAGS_BOLD);   // Set bold
                        vt_term_flags_clear(t, VT_CELL_FLAGS_DIM); // Clear faint                        
                    }
                    break;
                }
                case 2: {
                    if (set) {
                        vt_term_flags_add(t, VT_CELL_FLAGS_DIM);   // Set faint
                        vt_term_flags_clear(t, VT_CELL_FLAGS_BOLD); // Clear bold
                    }
                    else {
                        vt_term_flags_clear(t, VT_CELL_FLAGS_BOLD); // Clear bold
                        vt_term_flags_clear(t, VT_CELL_FLAGS_DIM); // Clear faint                        
                    }
                    break;       
                }
                default: {
                    if (set) {
                        vt_term_flags_add(t, zfb[z]);
                    } 
                    else {
                        vt_term_flags_clear(t, zfb[z]);
                    }
                    break;
                }
            }
        }

        //
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
        if (q >= 30 && q < 50) {
            const int fg = q < 40;
            const int z = fg ? q - 30 : q - 40;
            if (z < 8) {
                if (fg) {
                    vt_term_set_fgci(t, z);
                }
                else {
                    vt_term_set_bgci(t, z);
                }        
            }
            else if (z == 8) {
                // 38;5;n / 48;5;n     256 colours
                // 38;2;r;g;b / 48;...  24 bit colour, mapped to the nearest of the 256
                const uint32_t m = vt_emu_get_p0(p, i + 1);
                uint32_t ci = 256; // None
                if (m == 5) {
                    if (p->n_params > i + 2) ci = p->params[i + 2];
                    i += 2;
                }
                else if (m == 2) {
                    if (p->n_params > i + 4) {
                        ci = vt_pallet_rgb_to_256(p->params[i + 2], p->params[i + 3], p->params[i + 4]);
                    }
                    i += 4;
                }
                else {
                    // Unknown colour model: the remaining parameters can't be interpreted
                    i = p->n_params;
                }
                if (ci < 256) {
                    if (fg) {
                        vt_term_set_fgci(t, ci);
                    }
                    else {
                        vt_term_set_bgci(t, ci);
                    }
                }
            }
            else if (z == 9) {
                if (fg) {
                    vt_term_set_fgci(t, VT_TERM_DEFAULT_FG);
                }
                else {
                    vt_term_set_bgci(t, VT_TERM_DEFAULT_BG);
                }
            }
        }

        //
        // Color Name	    Foreground Color Code	Background Color Code
        // Bright Black	    90	                    100
        // Bright Red	    91	                    101
        // Bright Green	    92	                    102
        // Bright Yellow	93	                    103
        // Bright Blue	    94	                    104
        // Bright Magenta	95	                    105
        // Bright Cyan	    96	                    106
        // Bright White	    97	                    107
        //
        if (q >= 90 && q < 110) {
            const int fg = q < 100;
            const int z = fg ? q - 90 : q - 100;
            if (z < 8) {
                if (fg) {
                    vt_term_set_fgci(t, z + 8);
                }
                else {
                    vt_term_set_bgci(t, z + 8);
                }        
            }
        }
    }
}

// DECSC - save the cursor, attributes and character sets
static void vt_emu_save_cursor(
    vt_emu_t *e
) {
    vt_term_save_cursor(&e->term);
    e->s_g_dec[0] = e->g_dec[0];
    e->s_g_dec[1] = e->g_dec[1];
    e->s_gl = e->gl;
}

// DECRC - restore the cursor, attributes and character sets
static void vt_emu_restore_cursor(
    vt_emu_t *e
) {
    vt_term_restore_cursor(&e->term);
    e->g_dec[0] = e->s_g_dec[0];
    e->g_dec[1] = e->s_g_dec[1];
    e->gl = e->s_gl;
}

// Alternate screen private modes (as xterm)
static void vt_emu_alt_screen(
    vt_emu_t *e,
    uint32_t mode,
    bool on
) {
    vt_term_t* const t = &e->term;
    switch (mode) {
        case 47:   // Switch screen
            vt_term_alt_set(t, on);
            break;
        case 1047: // Switch screen, clearing the alternate screen when leaving it
            if (!on && t->alt_active) vt_term_erase_in_display(t, 2);
            vt_term_alt_set(t, on);
            break;
        case 1048: // Save/restore the cursor
            if (on) vt_emu_save_cursor(e);
            else vt_emu_restore_cursor(e);
            break;
        case 1049: // Save the cursor, switch and clear / switch back and restore the cursor
            if (on) {
                vt_emu_save_cursor(e);
                if (t->alt && !t->alt_active) {
                    vt_term_alt_set(t, true);
                    vt_term_erase_in_display(t, 2);
                }
            }
            else {
                vt_term_alt_set(t, false);
                vt_emu_restore_cursor(e);
            }
            break;
    }
}

// SM/RM - set or reset the modes listed in the parameters
static void vt_emu_set_modes(
    vt_emu_t *e,
    bool on
) {
    vt_term_t* const t = &e->term;
    vt_parser_t* const p = &e->parser;
    for (uint32_t i = 0; i < p->n_params; ++i) {
        uint32_t mode = 0;
        if (p->prefix == '?') {
            // DEC private modes
            switch (p->params[i]) {
                case 6: mode = VT_TERM_MODE_ORIGIN; break;   // DECOM
                case 7: mode = VT_TERM_MODE_AUTOWRAP; break; // DECAWM
                case 25: mode = VT_TERM_MODE_CURSOR; break;  // DECTCEM
                case 47:
                case 1047:
                case 1048:
                case 1049: vt_emu_alt_screen(e, p->params[i], on); break;
                case 2048: // In-band resize notifications, starting with the size now
                    e->resize_report = on;
                    if (on) vt_emu_report_size(e);
                    break;
            }
        }
        else {
            // ANSI modes
            switch (p->params[i]) {
                case 4: mode = VT_TERM_MODE_INSERT; break;   // IRM
                case 20: mode = VT_TERM_MODE_LNM; break;     // LNM
            }
        }
        DEBUG("VT_A_SM/RM %c%lu %d\n", p->prefix ? p->prefix : ' ', (ul)p->params[i], on);
        if (mode) vt_term_mode_set(t, mode, on);
    }
}

// Handle an ASCII byte (or a raw byte within an escape sequence)
static void vt_emu_put_byte(
    vt_emu_t *e, 
    vt_char_t ch
) {
    const vt_state_t* s = vt_parser_put_ch(&e->parser, ch);
    const vt_a_t a = vt_parser_action_from_state(s);
    vt_term_t* const t = &e->term;
    vt_parser_t* const p = &e->parser;
    DEBUG("Action %d\n", a);
    // Ignore private variants (e.g. ESC [ > c) of sequences that are not supported.
    // DECSED/DECSEL (ESC [ ? J/K) act as ED/EL as there are no protected characters.
    if (p->prefix && vt_parser_in_ground(p) && !(
        ((a == VT_A_SM || a == VT_A_RM) && p->prefix == '?') ||
        ((a == VT_A_ED || a == VT_A_EL) && p->prefix == '?')
    )) {
        DEBUG("Ignored private sequence %c action %d\n", p->prefix, a);
        return;
    }
    switch(a) {
    case VT_A_CHAR:        // A character is ready
        DEBUG("VT_A_CHAR (%d)\n", p->ch);
        if (e->g_dec[e->gl] && p->ch >= 0x5F && p->ch <= 0x7E) {
            vt_emu_put_cp(e, vt_charmap_dec_graphics(p->ch));
        }
        else {
            vt_term_putch(t, p->ch);
        }
        break;
    case VT_A_C0_NULL:     // C0 control codes
        DEBUG("VT_A_C0_NULL\n");
        break;
    case VT_A_C0_SOH:      // Start of Heading
        DEBUG("VT_A_C0_SOH\n");
        break;
    case VT_A_C0_STX:      // Start of Text
        DEBUG("VT_A_C0_STX\n");
        break;
    case VT_A_C0_ETX:      // End of Text
        DEBUG("VT_A_C0_ETX\n");
        break;
    case VT_A_C0_EOT:      // End of Transmission
        DEBUG("VT_A_C0_EOT\n");
        break;
    case VT_A_C0_ENQ:      // Enquiry 
        DEBUG("VT_A_C0_ENQ\n");
        break;
    case VT_A_C0_ACK:      // Acknowledge
        DEBUG("VT_A_C0_ACK\n");
        break;
    case VT_A_C0_BEL:      // Bell
        DEBUG("VT_A_C0_BEL\n");
        break;
    case VT_A_C0_BS:       // Backspace
        DEBUG("VT_A_C0_BS\n");
        vt_term_cursor_left(t,1);
        break;
    case VT_A_CHT:
    case VT_A_C0_HT: {      // CHT - Cursor Horizontal Tabulation, Horizontal Tab 
        DEBUG("VT_A_CHT/VT_A_C0_HT\n");
        const uint32_t n = vt_emu_get_p1(p, 0);
        vt_term_cursor_set_col(t, vt_tabs_next(&e->tabs, t->c, n));
        break;
    }
    case VT_A_HTB: {       // Horizontal Tab backwards
        DEBUG("VT_A_HTB\n");
        const uint32_t n = vt_emu_get_p1(p, 0);
        vt_term_cursor_set_col(t, vt_tabs_prev(&e->tabs, t->c, n));
        break;
    }
    case VT_A_C0_LF:       // Line Feed
    case VT_A_C0_VT:       // Vertical Tab (as LF, like xterm)
    case VT_A_C0_FF:       // Form Feed (as LF, like xterm)
        DEBUG("VT_A_C0_LF/VT/FF\n");
        vt_term_nl(t);     // Also does a CR in LNM mode
        break;
    case VT_A_C0_CR:       // Carriage Return
        DEBUG("VT_A_C0_CR\n");
        vt_term_cr(t);
        break;
    case VT_A_C0_SO:       // Shift Out: use G1
        DEBUG("VT_A_C0_SO\n");
        e->gl = 1;
        break;
    case VT_A_C0_SI:       // Shift In: use G0
        DEBUG("VT_A_C0_SI\n");
        e->gl = 0;
        break;
    case VT_A_C0_DLE:      // Data Link Escape
        DEBUG("VT_A_C0_DLE\n");
        break;
    case VT_A_C0_DC1:      // Device Control 1 
        DEBUG("VT_A_C0_DC1\n");
        break;
    case VT_A_C0_DC2:      // Device Control 2
        DEBUG("VT_A_C0_DC2\n");
        break;
    case VT_A_C0_DC3:      // Device Control 3
        DEBUG("VT_A_C0_DC3\n");
        break;
    case VT_A_C0_DC4:      // Device Control 4
        DEBUG("VT_A_C0_DC4\n");
        break;
    case VT_A_HTS: {        // VT100 Horizontal Tab Set (ESC H)
        DEBUG("VT_A_HTS %lu\n", (ul)t->c);
        vt_tabs_set_tab(&e->tabs, t->c);
        break;
    }
    case VT_A_HTC: {       // Clear horizontal tabs
        const uint32_t n = vt_emu_get_p0(p, 0);
        DEBUG("VT_A_HTC %lu\n", (ul)n);
        switch(n) {
            // TODO What does 0,1,2 do ?
            case 3:
                vt_tabs_clear_all(&e->tabs);
                break;
            default:
                break;
        }
        break;
    } 
    case VT_A_IND:         // Index (ESC D): down a line, scrolling at the bottom margin
        DEBUG("VT_A_IND\n");
        vt_term_index(t);
        break;
    case VT_A_NEL:         // Next Line (ESC E): CR + index
        DEBUG("VT_A_NEL\n");
        vt_term_cr(t);
        vt_term_index(t);
        break;
    case VT_A_RIS:         // Reset to Initial State (ESC c)
        DEBUG("VT_A_RIS\n");
        vt_emu_reset(e);
        break;
    case VT_A_RI:          // VT100 Reverse Index (ESC M)
        DEBUG("VT_A_RI\n");
        vt_term_reverse_nl(t);
        break;
    case VT_A_SS2:         // VT100 Single Shift 2 (ESC N)
        DEBUG("VT_A_SS2\n");
        break;
    case VT_A_SS3:         // VT100 Single Shift 3 (ESC O)
        DEBUG("VT_A_SS3\n");
        break;
    case VT_A_DCS:         // VT100 Device Control String (ESC P)
        DEBUG("VT_A_DCS\n");
        break;
    case VT_A_SOS:         // VT100 Start of String (ESC X)
        DEBUG("VT_A_SOS\n");
        break;
    case VT_A_PM:          // VT100 Privacy Message (ESC ^)
        DEBUG("VT_A_PM\n");
        break;
    case VT_A_APC:         // VT100 Application Program Command (ESC _)
        DEBUG("VT_A_APC\n");
        break;
    case VT_A_SAVE_CUR:    // VT100 Save Cursor Position (ESC 7)
        DEBUG("VT_A_SAVE_CUR\n");
        vt_emu_save_cursor(e);
        break;
    case VT_A_RESTORE_CUR: // VT100 Restore Cursor Position (ESC 8)
        DEBUG("VT_A_RESTORE_CUR\n");
        vt_emu_restore_cursor(e);
        break;
    case VT_A_CUU: {        // CUU - Cursor Up
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_CUU %lu\n", (ul)n);
        vt_term_cursor_up(t, n);
        break;
    }
    case VT_A_CUD: {       // CUD - Cursor Down
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_CUD %lu\n", (ul)n);
        vt_term_cursor_down(t, n);
        break;
    }
    case VT_A_CUF: {        // CUF - Cursor Forward
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_CUF %lu\n", (ul)n);
        vt_term_cursor_right(t, n);
        break;
    }
    case VT_A_CUB: {         // CUB - Cursor Back
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_CUB %lu\n", (ul)n);
        vt_term_cursor_left(t, n);
        break;
    }
    case VT_A_CNL: {       // CNL - Cursor Next Line
        const uint32_t n = vt_emu_get_p1(p, 0);        
        DEBUG("VT_A_CNL %lu\n", (ul)n);
        vt_term_next_line_down(t, n);    
        break;
    }
    case VT_A_CPL: {        // CPL - Cursor Previous Line
        const uint32_t n = vt_emu_get_p1(p, 0);        
        DEBUG("VT_A_CPL %lu\n", (ul)n);
        vt_term_next_line_up(t, n);
        break;
    }
    case VT_A_HVP:         // HVP - Horizontal Vertical Position
    case VT_A_CUP: {       // CUP - Cursor Position
        const uint32_t r = vt_emu_get_p0(p, 0);
        const uint32_t c = vt_emu_get_p0(p, 1);
        DEBUG("VT_A_CUP/VT_A_HVP %lu, %lu\n", (ul)r, (ul)c);
        vt_term_cursor_set(t, r == 0 ? 0 : r - 1, c == 0 ? 0 : c - 1);
        break;
    }
    case VT_A_CHA: {        // CHA - Cursor Horizontal Absolute 
        const uint32_t c = vt_emu_get_p0(p, 0);
        DEBUG("VT_A_CHA %lu\n", (ul)c);
        vt_term_cursor_set_col(t, c == 0 ? 0 : c - 1);
        break;
    }
    case VT_A_ED: {         // ED  - Erase in Display
        const uint32_t n = vt_emu_get_p0(p, 0);
        DEBUG("VT_A_ED %lu\n", (ul)n);
        vt_term_erase_in_display(t, n);
        break;
    }
    case VT_A_EL: {         // EL  - Erase in Line
        const uint32_t n = vt_emu_get_p0(p, 0);
        DEBUG("VT_A_EL %lu\n", (ul)n);
        vt_term_erase_in_line(t, n);
        break;
    }
    case VT_A_ICH: {         // ICH - Insert characters
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_ICH %lu\n", (ul)n);
        vt_term_insert_characters(t, n);
        break;
    }
    case VT_A_DCH: {         // DCH - Delete characters
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_DCH %lu\n", (ul)n);
        vt_term_delete_characters(t, n);
        break;
    }
    case VT_A_ECH: {         // ECH - Erase characters
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_ECH %lu\n", (ul)n);
        vt_term_erase_characters(t, n);
        break;
    }
    case VT_A_IL: {          // IL  - Insert lines
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_IL %lu\n", (ul)n);
        vt_term_insert_lines(t, n);
        break;
    }
    case VT_A_DL: {          // DL  - Delete lines
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_DL %lu\n", (ul)n);
        vt_term_delete_lines(t, n);
        break;
    }
    case VT_A_VPA: {         // VPA - Vertical position absolute
        const uint32_t r = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_VPA %lu\n", (ul)r);
        vt_term_cursor_set_row(t, r - 1);
        break;
    }
    case VT_A_SGR: {        // Select Graphic Rendition
        DEBUG("VT_A_SGR\n");
        vt_emu_sgr(e);
        break;
    }
    case VT_A_DECSTBM: {    // Set Top and Bottom Margins
        // A missing or 0 bottom, or one past the screen, means the last row
        const uint32_t mt = vt_emu_get_p1(p, 0) - 1;
        const uint32_t b = vt_emu_get_p0(p, 1);
        const uint32_t mb = (b == 0 || b > t->h) ? t->h - 1u : b - 1;
        DEBUG("VT_A_DECSTBM %lu, %lu\n", (ul)mt, (ul)mb);
        if (mt < mb) vt_term_margin_set(t, mt, mb);
        break;
    }
    case VT_A_DA:          // Device Attributes
        DEBUG("VT_A_DA\n");
        // Primary DA: a VT100 with Advanced Video Option
        if (vt_emu_get_p0(p, 0) == 0) vt_emu_reply(e, "\033[?1;2c");
        break;
    case VT_A_DSR: {       // Device Status Report
        const uint32_t n = vt_emu_get_p0(p, 0);
        DEBUG("VT_A_DSR %lu\n", (ul)n);
        if (n == 5) {
            // Status: OK
            vt_emu_reply(e, "\033[0n");
        }
        else if (n == 6) {
            // Cursor position report, 1 based and relative to the margins in origin mode
            char s[32];
            char *q = s;
            const uint32_t r = t->r - (vt_term_mode_get(t, VT_TERM_MODE_ORIGIN) ? t->mt : 0);
            *q++ = '\033';
            *q++ = '[';
            q = vt_emu_put_dec(q, r + 1);
            *q++ = ';';
            q = vt_emu_put_dec(q, t->c + 1);
            *q++ = 'R';
            *q = 0;
            vt_emu_reply(e, s);
        }
        break;
    }
    case VT_A_RM:          // Reset Mode
        vt_emu_set_modes(e, false);
        break;
    case VT_A_SM:          // Set Mode
        vt_emu_set_modes(e, true);
        break;
    case VT_A_SU: {         // Scroll Up
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_SU %lu\n", (ul)n);
        vt_term_scroll_up(t, t->mt, n);
        break;
    }
    case VT_A_SD: {         // Scroll Down
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_SD %lu\n", (ul)n);
        vt_term_scroll_down(t, t->mt, n);
        break;
    }
    case VT_A_REP: {         // Repeat character
        const uint32_t n = vt_emu_get_p1(p, 0);
        DEBUG("VT_A_REP %lu\n", (ul)n);
        vt_term_repeat(t, n);
        break;
    }
    case VT_A_XTWINOPS:    // Window operations (xterm)
        vt_emu_window_ops(e);
        break;
    case VT_A_XPALS:       // Linux set pallet 
        DEBUG("VT_A_XPALS\n");
        break;
    case VT_A_XPALR:       // Linux reset pallet
        DEBUG("VT_A_XPALR\n");
        break;
    case VT_A_OSC: {       // General OSC string handling
        const uint32_t cmd = vt_emu_get_p0(p, 0);
        DEBUG("VT_A_OSC %lu %s\n", (ul)cmd, (char*)p->osc_param);
        if ((cmd == 10 || cmd == 11) && strcmp((const char*)p->osc_param, "?") == 0) {
            // Query the default foreground (10) or background (11) colour.
            // Reply as xterm: ESC ] 1x ; rgb:RRRR/GGGG/BBBB, with the query's terminator
            uint32_t rgb[3];
            vt_pallet_get_rgb(cmd == 10 ? VT_TERM_DEFAULT_FG : VT_TERM_DEFAULT_BG, &rgb[0], &rgb[1], &rgb[2]);
            char r[40];
            char *q = r;
            *q++ = '\033';
            *q++ = ']';
            q = vt_emu_put_dec(q, cmd);
            memcpy(q, ";rgb:", 5);
            q += 5;
            for (int i = 0; i < 3; ++i) {
                if (i) *q++ = '/';
                q = vt_emu_put_hex16(q, rgb[i]);
            }
            if (s->m == 0x07) {
                *q++ = 0x07;   // BEL
            }
            else {
                *q++ = '\033'; // ST
                *q++ = '\\';
            }
            *q = 0;
            vt_emu_reply(e, r);
        }
        break;
    }
    case VT_A_DEC_SP_ON: { // G0 is DEC special graphics (box drawing)
        DEBUG("VT_A_DEC_SP_ON\n");
        e->g_dec[0] = true;
        break;
    }
    case VT_A_DEC_SP_OFF: { // G0 is ASCII
        DEBUG("VT_A_DEC_SP_OFF\n");
        e->g_dec[0] = false;
        break;
    }
    case VT_A_G1_DEC_SP_ON: { // G1 is DEC special graphics
        DEBUG("VT_A_G1_DEC_SP_ON\n");
        e->g_dec[1] = true;
        break;
    }
    case VT_A_G1_DEC_SP_OFF: { // G1 is ASCII
        DEBUG("VT_A_G1_DEC_SP_OFF\n");
        e->g_dec[1] = false;
        break;
    }
    case VT_A_NONE:         // Do nothing!
        DEBUG("VT_A_NONE\n");
        break;
    default:
        DEBUG("VT_A unrecognised\n");
        break;
    }
}

void vt_emu_put_str(
    vt_emu_t *e, 
    vt_char_t* s
) {
    while(*s) vt_emu_put_ch(e, *s++);
}

void vt_emu_put_ch(
    vt_emu_t *e, 
    vt_char_t ch
) {
    // Bytes within an escape sequence (e.g. an OSC title) go straight to the parser
    if (!vt_utf8_busy(&e->utf8) && !vt_parser_in_ground(&e->parser)) {
        vt_emu_put_byte(e, ch);
        return;
    }
    // Otherwise decode UTF-8. Raw C1 bytes (0x80-0x9F) are not controls (as xterm in UTF-8 mode).
    uint32_t cps[2];
    const uint32_t n = vt_utf8_decode(&e->utf8, ch, cps);
    for (uint32_t i = 0; i < n; ++i) {
        if (cps[i] < 0x80) {
            vt_emu_put_byte(e, (vt_char_t)cps[i]);
        }
        else {
            vt_emu_put_cp(e, cps[i]);
        }
    }
}
