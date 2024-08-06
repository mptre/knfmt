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

#include "libks/string.h"

enum sse_version {
	SSE_UNSUPPORTED = 0,
	SSE1_0,
	SSE2_0,
	SSE3_0,
	SSE4_1,
	SSE4_2,
};

struct cpuid {
	uint32_t a, b, c, d;
};

static void KS_init(void) __attribute__((constructor));

size_t (*KS_str_match)(const char *, size_t, const char *) =
    KS_str_match_default;
size_t (*KS_str_match_until)(const char *, size_t, const char *) =
    KS_str_match_until_default;

static inline void
cpuid(uint32_t leaf, uint32_t subleaf, struct cpuid *out)
{
	__asm__("cpuid"
	    : "=a" (out->a), "=b" (out->b), "=c" (out->c), "=d" (out->d)
	    : "a" (leaf), "c" (subleaf));
}

static int
is_x86_64(uint32_t *max_leaf)
{
	union {
		uint8_t u8[12];
		uint32_t u32[3];
	} ident;
	struct cpuid leaf;

	cpuid(0, 0, &leaf);
	ident.u32[0] = leaf.b;
	ident.u32[1] = leaf.d;
	ident.u32[2] = leaf.c;
	if (memcmp(ident.u8, "GenuineIntel", sizeof(ident)) != 0 &&
	    memcmp(ident.u8, "AuthenticAMD", sizeof(ident)) != 0)
		return 0;
	*max_leaf = leaf.a;
	return 1;
}

static enum sse_version
sse_version(uint32_t max_leaf)
{
#define CPUID_01_C_SSE3_0_MASK		(1 << 9)
#define CPUID_01_C_SSE4_1_MASK		(1 << 19)
#define CPUID_01_C_SSE4_2_MASK		(1 << 20)
#define CPUID_01_D_SSE1_0_MASK		(1 << 25)
#define CPUID_01_D_SSE2_0_MASK		(1 << 26)

	struct cpuid leaf;
	enum sse_version version = SSE_UNSUPPORTED;

	if (max_leaf < 1)
		return version;

	cpuid(1, 0, &leaf);
	if (leaf.d & CPUID_01_D_SSE1_0_MASK)
		version = SSE1_0;
	if (version == SSE1_0 && (leaf.d & CPUID_01_D_SSE2_0_MASK))
		version = SSE2_0;
	if (version == SSE2_0 && (leaf.c & CPUID_01_C_SSE3_0_MASK))
		version = SSE3_0;
	if (version == SSE3_0 && (leaf.c & CPUID_01_C_SSE4_1_MASK))
		version = SSE4_1;
	if (version == SSE4_1 && (leaf.c & CPUID_01_C_SSE4_2_MASK))
		version = SSE4_2;
	return version;
}

static int
has_bmi1(uint32_t max_leaf)
{
#define CPUID_07_B_BMI1		(1 << 3)

	struct cpuid leaf;

	if (max_leaf < 7)
		return 0;
	cpuid(7, 0, &leaf);
	return (leaf.b & CPUID_07_B_BMI1) ? 1 : 0;
}

static void
KS_init(void)
{
	uint32_t max_leaf = 0;
	int bmi1, sse4_2;

	if (!is_x86_64(&max_leaf))
		return;

	sse4_2 = sse_version(max_leaf) >= SSE4_2;
	bmi1 = has_bmi1(max_leaf);
	if (sse4_2 /* PCMPISTRM */ && bmi1 /* TZCNT */)
		KS_str_match = KS_str_match_native;
	if (sse4_2 /* PCMPISTRI */)
		KS_str_match_until = KS_str_match_until_native;
}

#else

extern int unused;

#endif
