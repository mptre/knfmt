/*
 * Copyright (c) 2024 Anton Lindqvist <anton@basename.se>
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

#if defined(__x86_64__)

extern int unused;

#else

#include "libks/bit.h"
#include "libks/string.h"

int	KS_str_match_init_default(const char *, struct KS_str_match *);

size_t (*KS_str_match)(const char *, size_t, const struct KS_str_match *) =
    KS_str_match_default;
size_t (*KS_str_match_until)(const char *, size_t, const struct KS_str_match *) =
    KS_str_match_until_default;

uint64_t (*KS_extract_and_deposit)(uint64_t, uint64_t, uint64_t) =
    KS_extract_and_deposit_default;

int
KS_str_match_init(const char *ranges, struct KS_str_match *match)
{
	return KS_str_match_init_default(ranges, match);
}

#endif
