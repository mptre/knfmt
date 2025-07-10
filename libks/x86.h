/*
 * Copyright (c) 2025 Anton Lindqvist <anton@basename.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef LIBKS_X86_H
#define LIBKS_X86_H

#include <stdint.h>

struct KS_x86_capabilites {
	struct {
		uint32_t major;
	} avx;

	struct {
		uint32_t bw:1;
	} avx512;

	struct {
		uint32_t major;
	} bmi;

	struct {
		uint32_t major;
		uint32_t minor;
	} sse;
};

int KS_x86_capabilites(struct KS_x86_capabilites *);

#endif /* !LIBKS_X86_H */
