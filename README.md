# pico-vt
Terminal emulator

This is an ANSI terminal emulator for embeded projects based loosely on [libtmt](https://github.com/deadpixi/libtmt).

There are specific (possibly not good) reasons for this library existing:
* I need cell (character & attributes) encoding/decoding to be fast as I am usinging it in a beam chasing renderer.
* I need it to have a small memory footprint and would rather it did not use malloc/free.
* I want it to be testable so I can correct mistakes and extend it.

The tests are very simple and do not rely on a framework (I am so tired of trying to get old testing frameworks to function).

The code is not complete and subject to change, but I am happy to recieve pull requests for changes... particularly if they have tests included.


An example of setting up a terminal is shown below:

```C
#include "vt/vt_emu.h"
#include "vt/vt_pallet.h"

#define CHAR_ROWS 80
#define CHAR_COLS 25

static hstx_dvi_pixel_t _pallet[256];
static vt_cell_t _screen[CHAR_ROWS][CHAR_COLS];
static vt_emu_t vt_emu;


vt_emu_t* __not_in_flash_func(hstx_dvi_vt_emu_get)() {
    return &vt_emu;
}

static void vt_pallet_make_cb(
    void *d,
    vt_cell_colour_t i,
    uint32_t r,
    uint32_t g,
    uint32_t b
) {
    _pallet[i] = hstx_dvi_pixel_rgb(r,g,b);
}

void hstx_dvi_vt_init() {

    // Setup the pallet
    vt_pallet_make_256_colours(0, vt_pallet_make_cb);

    // Setup the terminal emulator
    vt_emu_init(&vt_emu, (vt_cell_t*)_screen, CHAR_COLS, CHAR_ROWS);

    // Say hello
    vt_emu_put_str(&vt_emu, (vt_char_t*)"Pico virtual terminal v0.1\r\n");
}
```

