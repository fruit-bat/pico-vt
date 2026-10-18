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
 * ./vt_utf8_test.sh
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "vt_utf8.h"

#define R VT_UTF8_REPLACEMENT

// Decode a byte string and check the code points produced
static void check(const char *in, const uint32_t *expect, uint32_t n_expect) {
    vt_utf8_t u;
    vt_utf8_init(&u);
    uint32_t got[64];
    uint32_t n = 0;
    for (const char *s = in; *s; ++s) {
        uint32_t out[2];
        const uint32_t k = vt_utf8_decode(&u, (uint8_t)*s, out);
        assert(k <= 2);
        for (uint32_t i = 0; i < k; ++i) got[n++] = out[i];
    }
    printf("%s: %lu code points\n", in, (unsigned long)n);
    assert(n == n_expect);
    assert(memcmp(got, expect, n * sizeof(uint32_t)) == 0);
    // Nothing left part way through
    assert(!vt_utf8_busy(&u));
}

#define CHECK(IN, ...) do { \
    const uint32_t e[] = { __VA_ARGS__ }; \
    check(IN, e, sizeof(e) / sizeof(e[0])); \
} while (0)

int main() {
    // ASCII and controls pass straight through
    CHECK("Ab\x1b[1m", 'A', 'b', 0x1B, '[', '1', 'm');

    // 2, 3 and 4 byte forms, including the smallest and largest of each
    CHECK("\xC2\x80\xDF\xBF", 0x80, 0x7FF);
    CHECK("\xE0\xA0\x80\xE2\x94\x80\xEF\xBF\xBD", 0x800, 0x2500, 0xFFFD);
    CHECK("\xF0\x90\x80\x80\xF0\x9F\x98\x80\xF4\x8F\xBF\xBF", 0x10000, 0x1F600, 0x10FFFF);

    // Cyrillic whose continuation bytes are C1 values (0x8D, 0x9B)
    CHECK("\xD1\x8D\xD0\x9B", 0x44D, 0x41B);

    // Stray continuation bytes and invalid leads
    CHECK("\x80" "a\xBF", R, 'a', R);
    CHECK("\xC0\xC1\xF5\xFF", R, R, R, R);

    // Overlong forms, surrogates and values beyond Unicode
    CHECK("\xE0\x80\xAF", R);
    CHECK("\xF0\x80\x80\xAF", R);
    CHECK("\xED\xA0\x80", R);
    CHECK("\xF4\x90\x80\x80", R);

    // An interrupted sequence is replaced and the interrupting byte still counts
    CHECK("\xE2\x94" "a", R, 'a');
    CHECK("\xE2\x1b[m", R, 0x1B, '[', 'm');
    CHECK("\xE2\xC3\xA9", R, 0xE9);

    printf("all ok\n");
    return 0;
}
