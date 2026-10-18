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

#include "vt_utf8.h"

void vt_utf8_init(
    vt_utf8_t *u
) {
    u->cp = 0;
    u->min = 0;
    u->need = 0;
}

// Start a sequence, or decode a single byte character
static uint32_t vt_utf8_start(
    vt_utf8_t *u,
    uint8_t b,
    uint32_t *out
) {
    if (b < 0x80) {
        *out = b;
        return 1;
    }
    if (b >= 0xC2 && b <= 0xDF) {
        u->cp = b & 0x1F; u->need = 1; u->min = 0x80;
        return 0;
    }
    if (b >= 0xE0 && b <= 0xEF) {
        u->cp = b & 0x0F; u->need = 2; u->min = 0x800;
        return 0;
    }
    if (b >= 0xF0 && b <= 0xF4) {
        u->cp = b & 0x07; u->need = 3; u->min = 0x10000;
        return 0;
    }
    // A stray continuation byte, an overlong lead (C0, C1) or out of range lead (F5-FF)
    *out = VT_UTF8_REPLACEMENT;
    return 1;
}

uint32_t vt_utf8_decode(
    vt_utf8_t *u,
    uint8_t b,
    uint32_t out[2]
) {
    if (u->need == 0) return vt_utf8_start(u, b, out);

    if ((b & 0xC0) != 0x80) {
        // Interrupted: replace the incomplete sequence and start again with this byte
        u->need = 0;
        out[0] = VT_UTF8_REPLACEMENT;
        return 1 + vt_utf8_start(u, b, out + 1);
    }

    u->cp = (u->cp << 6) | (b & 0x3F);
    if (--u->need) return 0;

    // Reject overlong forms, surrogates and values beyond Unicode
    const uint32_t cp = u->cp;
    const bool ok = cp >= u->min && cp <= 0x10FFFF && (cp < 0xD800 || cp > 0xDFFF);
    out[0] = ok ? cp : VT_UTF8_REPLACEMENT;
    return 1;
}
