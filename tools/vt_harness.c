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
/* pico-vt desktop harness
 *
 * Runs a program in a pseudo terminal, feeds its output through the pico-vt
 * emulator and draws the emulator's screen in this terminal, so ordinary
 * terminal programs can be tried against the emulator on a PC.
 *
 *   make harness
 *   build-test/vt_harness [options] [-- command [args...]]
 *
 * See usage() or the README for the options.
 */
#define _DEFAULT_SOURCE
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include "vt_emu.h"
#include "vt_pallet.h"

#define KEY_QUIT 0x1D // Ctrl-]
#define MAX_KEYS 64

static vt_emu_t emu;
// Packed cols x rows, as the library expects
static vt_cell_t grid[VT_SCREEN_MAX_ROWS * VT_SCREEN_MAX_COLS];
static vt_cell_t alt_grid[VT_SCREEN_MAX_ROWS * VT_SCREEN_MAX_COLS];
static vt_cell_t shadow[VT_SCREEN_MAX_ROWS][VT_SCREEN_MAX_COLS]; // What is on the host screen
static uint8_t rgb[256][3]; // The palette, as the Pico would show it

static int cols = 80;
static int rows = 24;
static int master = -1;
static pid_t child = -1;

static bool interactive = true;
static struct termios saved_termios;
static bool host_raw = false;
static volatile sig_atomic_t host_resized = 0;

// Output to the host terminal
static char out[1 << 20];
static size_t out_len;

static void out_flush(void) {
    size_t done = 0;
    while (done < out_len) {
        const ssize_t n = write(STDOUT_FILENO, out + done, out_len - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        done += (size_t)n;
    }
    out_len = 0;
}

static void outf(const char *fmt, ...) {
    if (out_len > sizeof(out) - 256) out_flush();
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(out + out_len, sizeof(out) - out_len, fmt, ap);
    va_end(ap);
    if (n > 0) out_len += (size_t)n;
}

static void write_all(int fd, const void *p, size_t n) {
    const char *s = p;
    while (n) {
        const ssize_t k = write(fd, s, n);
        if (k < 0) {
            if (errno == EINTR) continue;
            return;
        }
        s += k;
        n -= (size_t)k;
    }
}

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void pallet_cb(void *d, vt_cell_colour_t i, uint32_t r, uint32_t g, uint32_t b) {
    (void)d;
    rgb[i][0] = r;
    rgb[i][1] = g;
    rgb[i][2] = b;
}

// Replies from the emulator (e.g. cursor position reports) go back to the program
static void reply_cb(void *ctx, const vt_char_t *s, uint32_t n) {
    (void)ctx;
    write_all(master, s, n);
}

// ---- Host terminal ----

static void host_restore(void) {
    if (!host_raw) return;
    outf("\033[0m\033[?25h\033[?1049l");
    out_flush();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    host_raw = false;
}

static void on_sigwinch(int sig) {
    (void)sig;
    host_resized = 1;
}

static void on_fatal(int sig) {
    host_restore();
    signal(sig, SIG_DFL);
    raise(sig);
}

static bool host_setup(void) {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "vt_harness: interactive mode needs a terminal (use -H for headless)\n");
        return false;
    }
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && (ws.ws_col < cols + 1 || ws.ws_row < rows + 2)) {
        fprintf(stderr, "vt_harness: this terminal is %dx%d but %dx%d is needed for a %dx%d screen (see -s)\n",
            ws.ws_col, ws.ws_row, cols + 1, rows + 2, cols, rows);
        return false;
    }
    tcgetattr(STDIN_FILENO, &saved_termios);
    struct termios raw = saved_termios;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    host_raw = true;
    atexit(host_restore);
    signal(SIGWINCH, on_sigwinch);
    signal(SIGTERM, on_fatal);
    signal(SIGSEGV, on_fatal);
    signal(SIGABRT, on_fatal);
    return true;
}

// ---- Rendering ----

static void out_sgr(vt_cell_attr_t a) {
    const vt_cell_flags_t f = vt_cell_flags_get(a);
    const uint8_t *fg = rgb[vt_cell_fg_get(a)];
    const uint8_t *bg = rgb[vt_cell_bg_get(a)];
    outf("\033[0%s%s%s%s%s%s%s%s;38;2;%d;%d;%d;48;2;%d;%d;%dm",
        (f & VT_CELL_FLAGS_BOLD) ? ";1" : "",
        (f & VT_CELL_FLAGS_DIM) ? ";2" : "",
        (f & VT_CELL_FLAGS_ITALIC) ? ";3" : "",
        (f & VT_CELL_FLAGS_UNDERLINE) ? ";4" : "",
        (f & VT_CELL_FLAGS_BLINK) ? ";5" : "",
        (f & VT_CELL_FLAGS_REVERSE) ? ";7" : "",
        (f & VT_CELL_FLAGS_INVISIBLE) ? ";8" : "",
        (f & VT_CELL_FLAGS_STRIKETHROUGH) ? ";9" : "",
        fg[0], fg[1], fg[2], bg[0], bg[1], bg[2]);
}

static char glyph(vt_cell_t cell) {
    const vt_char_t g = vt_cell_get_char(cell);
    return (g >= 0x20 && g < 0x7F) ? (char)g : '?';
}

static void render(bool full) {
    vt_term_t *const t = &emu.term;
    outf("\033[?25l");
    if (full) {
        // Clear and draw the frame
        outf("\033[0m\033[2J");
        for (int r = 0; r < rows; ++r) outf("\033[%d;%dH|", r + 1, cols + 1);
        outf("\033[%d;1H", rows + 1);
        for (int c = 0; c < cols; ++c) outf("-");
        outf("+");
        memset(shadow, 0xFF, sizeof(shadow));
    }

    // Draw the cells that have changed
    vt_cell_attr_t attr = 0xFFFFFFFF;
    for (int r = 0; r < rows; ++r) {
        int next = -1; // Column the host cursor is at, if on this row
        for (int c = 0; c < cols; ++c) {
            const vt_cell_t cell = t->rp[r][c];
            if (cell == shadow[r][c]) continue;
            shadow[r][c] = cell;
            if (next != c) outf("\033[%d;%dH", r + 1, c + 1);
            const vt_cell_attr_t a = vt_cell_get_attr(cell);
            if (a != attr) {
                out_sgr(a);
                attr = a;
            }
            outf("%c", glyph(cell));
            next = c + 1;
        }
    }

    // Status line
    outf("\033[0m\033[%d;1H\033[2K", rows + 2);
    outf("pico-vt %dx%d  cursor %d,%d%s%s%s%s%s  Ctrl-] quits",
        cols, rows, t->r + 1, t->c + 1,
        t->wrap ? "  wrap" : "",
        t->alt_active ? "  alt" : "",
        vt_term_mode_get(t, VT_TERM_MODE_ORIGIN) ? "  origin" : "",
        vt_term_mode_get(t, VT_TERM_MODE_INSERT) ? "  insert" : "",
        vt_term_mode_get(t, VT_TERM_MODE_AUTOWRAP) ? "" : "  nowrap");

    // The cursor
    outf("\033[%d;%dH", t->r + 1, t->c + 1);
    if (vt_term_mode_get(t, VT_TERM_MODE_CURSOR)) outf("\033[?25h");
    out_flush();
}

// The screen as text, with trailing spaces removed
static void dump(FILE *f) {
    vt_term_t *const t = &emu.term;
    for (int r = 0; r < rows; ++r) {
        char line[VT_SCREEN_MAX_COLS + 1];
        int n = 0;
        for (int c = 0; c < cols; ++c) line[n++] = glyph(t->rp[r][c]);
        while (n && line[n - 1] == ' ') --n;
        line[n] = 0;
        fprintf(f, "%s\n", line);
    }
    fprintf(f, "-- cursor %d,%d%s\n", t->r + 1, t->c + 1,
        vt_term_mode_get(t, VT_TERM_MODE_CURSOR) ? "" : " (hidden)");
}

// ---- Options ----

// Decode \r \n \t \e \\ and \xHH in a -k argument
static size_t unescape(const char *s, char *d) {
    size_t n = 0;
    while (*s) {
        if (*s != '\\' || !s[1]) {
            d[n++] = *s++;
            continue;
        }
        ++s;
        switch (*s++) {
            case 'r': d[n++] = '\r'; break;
            case 'n': d[n++] = '\n'; break;
            case 't': d[n++] = '\t'; break;
            case 'e': d[n++] = 0x1B; break;
            case '\\': d[n++] = '\\'; break;
            case 'x': {
                unsigned v = 0;
                for (int i = 0; i < 2 && *s; ++i, ++s) {
                    const char c = *s;
                    v = v * 16 + (unsigned)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
                }
                d[n++] = (char)v;
                break;
            }
            default: d[n++] = s[-1]; break;
        }
    }
    return n;
}

static void usage(void) {
    fprintf(stderr,
        "usage: vt_harness [options] [-- command [args...]]\n"
        "\n"
        "Runs command (default $SHELL) in a pseudo terminal through the pico-vt emulator.\n"
        "\n"
        "  -s COLSxROWS  screen size (default 80x24, at most %dx%d)\n"
        "  -t TERM       TERM for the command (default xterm-256color)\n"
        "  -A            no alternate screen\n"
        "  -H            headless: don't draw or read the keyboard\n"
        "  -k KEYS       type KEYS (\\r \\n \\t \\e \\\\ \\xHH escapes); may be repeated\n"
        "  -K MS         delay before and between each -k (default 500)\n"
        "  -w MS         stop after MS milliseconds\n"
        "  -d            print the screen as text when finished\n"
        "\n"
        "Interactive keys go to the command; Ctrl-] quits.\n",
        VT_SCREEN_MAX_COLS, VT_SCREEN_MAX_ROWS);
}

int main(int argc, char **argv) {
    const char *term = "xterm-256color";
    bool use_alt = true;
    bool do_dump = false;
    uint64_t wait_ms = 0;
    uint64_t key_delay = 500;
    static char keys[MAX_KEYS][4096];
    size_t key_len[MAX_KEYS];
    int n_keys = 0;

    int opt;
    while ((opt = getopt(argc, argv, "s:t:AHk:K:w:dh")) != -1) {
        switch (opt) {
            case 's':
                if (sscanf(optarg, "%dx%d", &cols, &rows) != 2 || cols < 2 || rows < 2 ||
                    cols > VT_SCREEN_MAX_COLS || rows > VT_SCREEN_MAX_ROWS) {
                    usage();
                    return 2;
                }
                break;
            case 't': term = optarg; break;
            case 'A': use_alt = false; break;
            case 'H': interactive = false; break;
            case 'k':
                if (n_keys == MAX_KEYS || strlen(optarg) >= sizeof(keys[0])) {
                    usage();
                    return 2;
                }
                key_len[n_keys] = unescape(optarg, keys[n_keys]);
                ++n_keys;
                break;
            case 'K': key_delay = strtoull(optarg, NULL, 10); break;
            case 'w': wait_ms = strtoull(optarg, NULL, 10); break;
            case 'd': do_dump = true; break;
            default: usage(); return opt == 'h' ? 0 : 2;
        }
    }
    // The emulator
    vt_pallet_make_256_colours(NULL, pallet_cb);
    vt_emu_init(&emu, grid, (vt_coord_t)cols, (vt_coord_t)rows);
    if (use_alt) vt_emu_set_alt_grid(&emu, alt_grid);
    vt_emu_set_reply(&emu, reply_cb, NULL);

    if (interactive && !host_setup()) return 1;

    // The command
    struct winsize ws = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)cols };
    child = forkpty(&master, NULL, NULL, &ws);
    if (child < 0) {
        host_restore();
        perror("vt_harness: forkpty");
        return 1;
    }
    if (child == 0) {
        setenv("TERM", term, 1);
        if (optind < argc) {
            execvp(argv[optind], argv + optind);
            perror(argv[optind]);
        }
        else {
            const char *sh = getenv("SHELL");
            if (!sh) sh = "/bin/sh";
            execl(sh, sh, (char *)NULL);
            perror(sh);
        }
        _exit(127);
    }

    if (interactive) {
        outf("\033[?1049h");
        render(true);
    }

    const uint64_t start = now_ms();
    uint64_t next_key = start + key_delay;
    uint64_t last_render = 0;
    bool dirty = false;
    int k = 0;
    for (;;) {
        struct pollfd fds[2] = {
            { .fd = master, .events = POLLIN },
            { .fd = interactive ? STDIN_FILENO : -1, .events = POLLIN },
        };
        if (poll(fds, 2, 10) < 0 && errno != EINTR) break;

        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            uint8_t buf[4096];
            const ssize_t n = read(master, buf, sizeof(buf));
            if (n <= 0) break; // The command has finished
            for (ssize_t i = 0; i < n; ++i) vt_emu_put_ch(&emu, buf[i]);
            dirty = true;
        }

        if (fds[1].revents & POLLIN) {
            uint8_t buf[256];
            const ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;
            if (memchr(buf, KEY_QUIT, (size_t)n)) break;
            write_all(master, buf, (size_t)n);
        }

        const uint64_t now = now_ms();
        if (k < n_keys && now >= next_key) {
            write_all(master, keys[k], key_len[k]);
            ++k;
            next_key = now + key_delay;
        }
        if (wait_ms && now - start >= wait_ms) break;

        if (interactive) {
            if (host_resized) {
                host_resized = 0;
                render(true);
                last_render = now;
                dirty = false;
            }
            else if (dirty && now - last_render >= 16) {
                render(false);
                last_render = now;
                dirty = false;
            }
        }
    }

    // Stop the command if it is still running
    int status = 0;
    if (waitpid(child, &status, WNOHANG) == 0) {
        kill(child, SIGHUP);
        waitpid(child, &status, 0);
    }
    host_restore();
    if (do_dump) dump(stdout);
    return 0;
}
