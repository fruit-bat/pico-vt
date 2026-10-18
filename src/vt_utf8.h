#pragma once
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
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Substituted for invalid UTF-8
#define VT_UTF8_REPLACEMENT 0xFFFD

// UTF-8 decoder state
typedef struct {
    uint32_t cp;  // Code point being decoded
    uint32_t min; // Smallest valid code point for the sequence (rejects overlong forms)
    uint8_t need; // Continuation bytes still needed
} vt_utf8_t;

void vt_utf8_init(
    vt_utf8_t *u
);

// True if part way through a multi-byte sequence
static inline bool vt_utf8_busy(
    const vt_utf8_t *u
) {
    return u->need != 0;
}

// Decode one byte, writing 0, 1 or 2 code points to out and returning how many.
// Invalid input gives VT_UTF8_REPLACEMENT. A byte that interrupts a sequence
// gives the replacement followed by the result of decoding that byte.
uint32_t vt_utf8_decode(
    vt_utf8_t *u,
    uint8_t b,
    uint32_t out[2]
);

#ifdef __cplusplus
}
#endif
