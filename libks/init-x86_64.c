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

#include <stdint.h>
#include <string.h>

#include "libks/bit.h"
#include "libks/capabilities.h"
#include "libks/string.h"
#include "libks/valgrind.h"

static void KS_init(void) __attribute__((constructor));

int KS_str_match_init_default(const char *, struct KS_str_match *);
size_t KS_str_match_native_128(const char *, size_t,
    const struct KS_str_match *);
size_t KS_str_match_native_256(const char *, size_t,
    const struct KS_str_match *);
size_t KS_str_match_native_512(const char *, size_t,
    const struct KS_str_match *);
size_t KS_str_match_until_native_128(const char *, size_t,
    const struct KS_str_match *);
size_t KS_str_match_until_native_256(const char *, size_t,
    const struct KS_str_match *);

size_t (*KS_str_match)(const char *, size_t, const struct KS_str_match *) =
    KS_str_match_default;
size_t (*KS_str_match_until)(const char *, size_t, const struct KS_str_match *) =
    KS_str_match_until_default;

static void
KS_init(void)
{
	struct KS_x86_capabilites caps;

	if (!KS_x86_capabilites(&caps))
		return;

	if (caps.avx >= 512 && caps.avx512.bw && caps.bmi >= 1 /* TZCNT */)
		KS_str_match = KS_str_match_native_512;
	else if (caps.avx >= 2 && caps.bmi >= 1 /* TZCNT */)
		KS_str_match = KS_str_match_native_256;
	else if (caps.sse == 0x42 /* PCMPISTRM */ && caps.bmi >= 1 /* TZCNT */)
		KS_str_match = KS_str_match_native_128;

	if (caps.avx >= 2 && caps.bmi >= 2 /* BZHI */)
		KS_str_match_until = KS_str_match_until_native_256;
	else if (caps.sse == 0x42 /* PCMPISTRI */ &&
	    /* Valgrind does not emulate PCMPISTRI $0x4. */
	    !KS_valgrind_is_running())
		KS_str_match_until = KS_str_match_until_native_128;

	KS_bit_init();
}

static void
KS_str_match_init_512(const char *ranges, struct KS_str_match *match)
{
	size_t i, ranges_len;

	ranges_len = strlen(ranges);

	for (i = 0; i < ranges_len; i += 2) {
		uint8_t hi, j, lo;

		lo = (uint8_t)ranges[i];
		hi = (uint8_t)ranges[i + 1];
		for (j = lo; j < hi + 1; j++) {
			uint8_t mask = (uint8_t)(1 << (j >> 4));

	/* Avoid loss of precision due to conversion warnings. Caused by the
	 * bitwise OR operator promoting its operands to integers. */
#define OR(off) do {							\
	uint8_t *u8 = (uint8_t *)&match->u512[(j & 0xf) + (off)];	\
	*u8 = *u8 | mask;						\
} while (0)
			OR(0x00);
			OR(0x10);
			OR(0x20);
			OR(0x30);
#undef OR
		}
	}
}

int
KS_str_match_init(const char *ranges, struct KS_str_match *match)
{
	if (KS_str_match_init_default(ranges, match) == -1)
		return -1;

	KS_str_match_init_512(ranges, match);
	return 0;
}

#else

extern int unused;

#endif
