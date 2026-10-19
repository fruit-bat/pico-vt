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
 * ./vt_tabs_test.sh
 *
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "vt_tabs.h"
#include <string.h>
#include <assert.h>

int main() {

    vt_tabs_t t;
    vt_tabs_init(
        &t, // Tabs structure
        16, // Width of 16
        5   // Tab stop every 5
    );
    //printf("%s\n", (char*)&t.row);
    //                      0000000000111111
    //                      0123456789012345
    const char* expected = "*    *    *    *";
    for(vt_coord_t i = 0; i < 16; ++i) {
        assert(vt_tabs_is_tab(&t, i) == (expected[i] == '*'));
    }

    assert(vt_tabs_next(&t, 12, 1) == 15);
    assert(vt_tabs_next(&t, 15, 1) == 15);
    assert(vt_tabs_next(&t, 0, 1) == 5);
    assert(vt_tabs_next(&t, 0, 2) == 10);
    assert(vt_tabs_prev(&t, 8, 1) == 5);
    assert(vt_tabs_prev(&t, 5, 1) == 0);
    assert(vt_tabs_prev(&t, 0, 1) == 0);
    assert(vt_tabs_prev(&t, 8, 5) == 0);
    assert(vt_tabs_next(&t, 8, 5) == 15);
    assert(vt_tabs_prev(&t, 15, 2) == 5);

    vt_tabs_clear_all(&t);

    for(vt_coord_t i = 0; i < 16; ++i) {
        assert(!vt_tabs_is_tab(&t, i));
    }

    assert(vt_tabs_prev(&t, 8, 1) == 8);
    assert(vt_tabs_next(&t, 8, 1) == 8);

    // Resizing keeps the stops that were set and adds the defaults beyond
    vt_tabs_init(&t, 16, 5);
    vt_tabs_clear_tab(&t, 5);
    vt_tabs_set_tab(&t, 7);
    vt_tabs_resize(&t, 23, 5);
    //                              00000000001111111111222
    //                              01234567890123456789012
    const char* expected_grow   = "*      *  *    *    * *";
    for(vt_coord_t i = 0; i < 23; ++i) {
        assert(vt_tabs_is_tab(&t, i) == (expected_grow[i] == '*'));
    }
    assert(vt_tabs_next(&t, 20, 1) == 22);

    // Shrinking gives the new last column a stop
    vt_tabs_resize(&t, 13, 5);
    const char* expected_shrink = "*      *  * *";
    for(vt_coord_t i = 0; i < 13; ++i) {
        assert(vt_tabs_is_tab(&t, i) == (expected_shrink[i] == '*'));
    }
    assert(vt_tabs_next(&t, 10, 5) == 12);

    printf("all ok\n");
    return 0;
}   


