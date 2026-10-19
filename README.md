# pico-vt
Terminal emulator

This is an ANSI terminal emulator for embeded projects based loosely on [libtmt](https://github.com/deadpixi/libtmt).

There are specific (possibly not good) reasons for this library existing:
* I need cell (character & attributes) encoding/decoding to be fast as I am usinging it in a beam chasing renderer.
* I need it to have a small memory footprint and would rather it did not use malloc/free.
* I want it to be testable so I can correct mistakes and extend it.

The tests are very simple and do not rely on a framework (I am so tired of trying to get old testing frameworks to function).

The code is not complete and subject to change, but I am happy to recieve pull requests... particularly if they have tests included.


## Using it

Add the library to a Pico SDK project with `add_subdirectory` and link `pico_vtlib`. An example of setting up a terminal is shown below:

```C
#include "vt_emu.h"
#include "vt_pallet.h"

#define CHAR_COLS 80
#define CHAR_ROWS 25

static hstx_dvi_pixel_t _pallet[256];
static vt_cell_t _screen[CHAR_ROWS][CHAR_COLS];
static vt_emu_t vt_emu;

static void vt_pallet_make_cb(
    void *d,
    vt_cell_colour_t i,
    uint32_t r,
    uint32_t g,
    uint32_t b
) {
    _pallet[i] = hstx_dvi_pixel_rgb(r,g,b);
}

// Replies to the host, e.g. the answer to a cursor position request (optional)
static void vt_reply_cb(void *ctx, const vt_char_t *s, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) putchar(s[i]);
}

void hstx_dvi_vt_init() {

    // Setup the pallet
    vt_pallet_make_256_colours(0, vt_pallet_make_cb);

    // Setup the terminal emulator
    vt_emu_init(&vt_emu, (vt_cell_t*)_screen, CHAR_COLS, CHAR_ROWS);
    vt_emu_set_reply(&vt_emu, vt_reply_cb, NULL);

    // Say hello
    vt_emu_put_str(&vt_emu, (vt_char_t*)"Pico virtual terminal v0.1\r\n");
}
```

Then pass everything received from the host to `vt_emu_put_ch()` (or `vt_emu_put_str()`).

### Input

* The emulator aims to behave like xterm.
* Text is decoded as UTF-8. Bytes 0x80-0x9F are not treated as C1 controls (as xterm in UTF-8 mode).
* **LF (`\n`) moves down a line but does not return to column 0**, as on a real terminal; the host's tty
  normally sends `\r\n`. If your program writes directly to the emulator with just `\n`, either send
  `\r\n` or turn on new line mode: `vt_term_mode_set(&vt_emu.term, VT_TERM_MODE_LNM, true)`.
* Replies (device attributes, status and cursor position reports) are only sent if a reply function is set
  with `vt_emu_set_reply()`.

### Rendering

The screen is a grid of `vt_cell_t` (`term.rp[row][col]`), each holding an 8 bit glyph and its attributes:

* `vt_cell_get_char()` is the glyph, `vt_cell_fg_get()` / `vt_cell_bg_get()` are palette indexes (see
  `vt_pallet_make_256_colours()`) and `vt_cell_flags_get()` is the `VT_CELL_FLAGS_xxx` (bold, underline,
  reverse etc.).
* The cursor is at `term.r`, `term.c`. Don't draw it when the host has hidden it:
  `vt_term_mode_get(&vt_emu.term, VT_TERM_MODE_CURSOR)` is false.
* 24 bit colours are mapped to the nearest of the 256 palette colours.

#### Glyphs and fonts

Unicode code points are mapped to the 8 bit glyph stored in each cell by a character map. The default,
`vt_charmap_ascii`, suits a font with just printable ASCII (0x20-0x7E) and never produces anything else:
line drawing uses the ncurses ASCII fallbacks (`+`, `-`, `|` ...), accented Latin-1 letters lose their
accents and anything else is `?`.

If your font has more glyphs (e.g. CP437 box drawing), provide your own map:

```C
static vt_char_t my_charmap(uint32_t cp) {
    switch (cp) {
        case 0x2500: return 0xC4; // ─
        case 0x2502: return 0xB3; // │
        case 0x250C: return 0xDA; // ┌
        // ...
        default: return vt_charmap_ascii(cp);
    }
}

vt_emu_set_charmap(&vt_emu, my_charmap);
```

DEC special graphics (line drawing selected with `ESC ( 0`) is converted to the equivalent Unicode, so it
goes through the same map. Wide characters (CJK, emoji) take two cells (the glyph then a space) and
combining marks are dropped.

### Alternate screen

Full screen programs (vim, less, htop) switch to an alternate screen and back, so the screen is restored
when they exit. It needs a second grid, the same size as the first, so it is optional:

```C
static vt_cell_t _alt_screen[CHAR_ROWS][CHAR_COLS];

vt_emu_set_alt_grid(&vt_emu, (vt_cell_t*)_alt_screen);
```

Switching swaps the row pointers, so `term.rp` is always the screen in use and the renderer needs no
changes. Without an alternate grid the switch is ignored (the screen is not cleared) but the cursor is
still saved and restored.

### Resizing

To change the size at run time, give the terminal a grid for the most it could be (its capacity) and
start it at any size within that:

```C
static vt_cell_t _screen[MAX_ROWS][MAX_COLS];

vt_emu_init_cap(&vt_emu, (vt_cell_t*)_screen, MAX_COLS, MAX_ROWS, 64, 20);
```

Then `vt_emu_resize(&vt_emu, w, h)` changes it, moving no cells and allocating nothing. As xterm, text
is not reflowed: shrinking hides cells and growing shows blank ones. If the cursor would be below the new
bottom, the screen scrolls up to keep its line in view. The scroll region is reset. The alternate grid,
if there is one, must have the same capacity. The capacity is at most `VT_SCREEN_MAX_COLS` x
`VT_SCREEN_MAX_ROWS` (80 x 60), which can be changed with `-D`.

The renderer should draw `term.w` x `term.h` cells (read once a frame) and fill the rest of the display.
Every row pointer up to the capacity is always valid, so a renderer on the other core can't read outside
the grid while a resize is under way; at worst it shows one odd frame. Call `vt_emu_resize()` from the
same core as `vt_emu_put_ch()`.

Changes can start from either end:

* **The display** (a font change, a status line, a window manager) calls `vt_emu_resize()`.
* **The host** asks with `ESC [ 8 ; h ; w t`. The emulator doesn't decide: it calls the function set
  with `vt_emu_set_resize_req()`, which may resize to that size (it is already limited to the capacity), a
  different one or not at all. Without one, requests are ignored.

Over a serial line the host's tty doesn't know the size has changed (there is no `SIGWINCH`), so it
has to be told:

* `eval $(resize)` (from xterm) asks for the size and sets the tty's (`stty rows ... cols ...`).
* `ESC [ 18 t` reports the size as `ESC [ 8 ; h ; w t` and `ESC [ 19 t` the capacity as
  `ESC [ 9 ; h ; w t`. With `vt_emu_set_cell_size()`, `ESC [ 14 t` and `ESC [ 16 t` report the
  text area and cell sizes in pixels.
* A program that turns on mode `?2048` (in-band resize notifications, as Contour, kitty and foot) is
  sent `ESC [ 48 ; h ; w ; hpx ; wpx t` straight away and each time the size changes.

The desktop harness (`make harness`) shows this through a pty: `-R` follows the size of the terminal
it runs in and the command's requests are allowed if they fit.

### Modes

Modes are set by the host with SM/RM (`ESC [ n h` / `ESC [ n l`) or directly with `vt_term_mode_set()`:

| Mode | Sequence | Default | Effect |
|---|---|---|---|
| `VT_TERM_MODE_AUTOWRAP` | `?7` | on | Wrap at the right margin (DECAWM) |
| `VT_TERM_MODE_ORIGIN` | `?6` | off | Cursor rows relative to the scroll region (DECOM) |
| `VT_TERM_MODE_CURSOR` | `?25` | on | Cursor visible (DECTCEM), for the renderer |
| `VT_TERM_MODE_INSERT` | `4` | off | Insert rather than overwrite characters (IRM) |
| `VT_TERM_MODE_LNM` | `20` | off | LF, VT and FF also return to column 0 (LNM) |

The alternate screen modes `?47`, `?1047`, `?1048` and `?1049` are also supported (as xterm), as is
`?2048` (resize notifications, see [Resizing](#resizing)).

### Supported sequences

| Kind | Supported |
|---|---|
| C0 | BS, HT, LF, VT, FF, CR, SO, SI |
| ESC | `7` `8` save/restore cursor, `D` index, `E` next line, `H` set tab, `M` reverse index, `c` reset, `( 0` `( B` `) 0` `) B` character sets |
| Cursor | CUU/CUD/CUF/CUB `A` `B` `C` `D`, CNL/CPL `E` `F`, CHA/HPA `G` `` ` ``, CUP/HVP `H` `f`, VPA `d`, `s` `u` save/restore |
| Editing | ED `J`, EL `K`, ICH `@`, DCH `P`, ECH `X`, IL `L`, DL `M`, REP `b` |
| Scrolling | DECSTBM `r`, SU `S`, SD `T` |
| Tabs | CHT `I`, CBT `Z`, TBC `g` |
| Attributes | SGR `m`: styles 0-9 and 22-29, colours 30-37/39, 40-47/49, 90-97, 100-107, `38;5;n`, `48;5;n`, `38;2;r;g;b`, `48;2;r;g;b` |
| Modes | SM/RM `h` `l`: `4`, `20`, `?6`, `?7`, `?25`, `?47`, `?1047`, `?1048`, `?1049`, `?2048` |
| Window | XTWINOPS `t`: `8` resize request, `14` `16` `18` `19` size reports |
| Reports | DA `c`, DSR `5n` and `6n`, OSC `10;?` and `11;?` (default foreground and background colours) |

Other OSC strings (e.g. window titles) are consumed and ignored; they can end with BEL, `ESC \` or ST. Unsupported private sequences (`ESC [ ? ...`,
`ESC [ > ...`) are ignored. As xterm, C0 controls within a sequence are executed without interrupting it,
CAN and SUB abandon it and ESC starts a new one.

## Desktop harness

The harness runs ordinary terminal programs through the emulator on a PC (Linux or macOS), drawing the
emulator's screen in your terminal with the Pico palette. It is a quick way to see how real programs
(a shell, vim, less, top, nano, vttest...) behave before trying them on a Pico.

```sh
make harness
build-test/vt_harness                      # your shell in an 80x24 emulator
build-test/vt_harness -s 80x40 -- vim      # any program, any size up to 80x60
```

Keys go to the program and `Ctrl-]` quits. The status line under the screen shows the cursor position
and modes (wrap pending, alternate screen, insert...). Your terminal needs to be at least one column and
two rows bigger than the emulator.

It can also be scripted, which is handy for checking a fix or reproducing a problem:

```sh
# Run vim headless, type into it and print the screen as text
build-test/vt_harness -H -d -s 50x10 -K 700 -w 2500 -k 'iHello\e' -k ':set number\r' -- vim -u NONE
```

| Option | |
|---|---|
| `-s COLSxROWS` | Screen size (default 80x24) |
| `-t TERM` | `TERM` for the program (default `xterm-256color`) |
| `-A` | No alternate screen |
| `-H` | Headless: don't draw or read the keyboard |
| `-k KEYS` | Type `KEYS` (with `\r` `\n` `\t` `\e` `\\` `\xHH` escapes); may be repeated |
| `-K MS` | Delay before and between each `-k` (default 500) |
| `-w MS` | Stop after `MS` milliseconds |
| `-d` | Print the screen as text when finished |

[vttest](https://invisible-island.net/vttest/) (e.g. `apt install vttest`) is a good workout:
`build-test/vt_harness -- vttest`.

## Tests

The tests are plain C programs, one per module. Build and run them all, with AddressSanitizer and
UndefinedBehaviorSanitizer, with:

```sh
make test
```

They also run on GitHub for every push and pull request. Each suite can still be built and run on its
own with its script in `src` (e.g. `cd src && ./vt_term_test.sh`).
