/*
 * Copyright (c) 2023 Anton Lindqvist <anton@basename.se>
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

#include "libks/string.h"

#include <stdint.h>
#include <string.h>

#include "libks/arena-vector.h"
#include "libks/arena.h"
#include "libks/compiler.h"
#include "libks/vector.h"

#define CTRL_SOURCE_SHIFT		0
#define  CTRL_SOURCE_U8			0x00
#define  CTRL_SOURCE_U16		0x01
#define  CTRL_SOURCE_S8			0x02
#define  CTRL_SOURCE_S16		0x03
#define CTRL_AGGREGATE_SHIFT		2
#define  CTRL_AGGREGATE_EQUAL_ANY 	0x00
#define  CTRL_AGGREGATE_RANGES 		0x01
#define  CTRL_AGGREGATE_EQUAL_EACH	0x02
#define  CTRL_AGGREGATE_EQUAL_ORDERED	0x03
#define CTRL_POLARITY_SHIFT		4
#define  CTRL_POLARITY_POSITIVE		0x00
#define  CTRL_POLARITY_NEGATIVE		0x01
#define  CTRL_POLARITY_MASKED_POSITIVE	0x02
#define  CTRL_POLARITY_MASKED_NEGATIVE	0x03
#define CTRL_OUTPUT_INDEX_SHIFT		6
#define  CTRL_OUTPUT_INDEX_LSB		0x00
#define  CTRL_OUTPUT_INDEX_MSB		0x01
#define CTRL_OUTPUT_MASK_SHIFT		7
#define  CTRL_OUTPUT_MASK_BITMASK	0x00
#define  CTRL_OUTPUT_MASK_BYTEMASK	0x01

struct cpuid {
	uint32_t a, b, d, c;
};

enum sse_version {
	SSE1_0 = 1,
	SSE2_0,
	SSE3_0,
	SSE4_1,
	SSE4_2,
};

unsigned int KS_features = 0;

static void KS_str_init(void) __attribute__((constructor));

/* Ignore warnings related to using extended inline assembler. */
#if defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#endif

static int
is_intel(uint32_t *MAYBE_UNUSED(max_leaf))
{
#if defined(__x86_64__)
	struct cpuid leaf;

	asm("cpuid"
	    : "=a" (leaf.a), "=b" (leaf.b), "=c" (leaf.c), "=d" (leaf.d)
	    : "a" (0), "c" (0));
	if (memcmp(&leaf.b, "GenuineIntel", 12) != 0)
		return 0;
	*max_leaf = leaf.a;
	return 1;
#else
	return 0;
#endif
}

static enum sse_version
sse_version(uint32_t MAYBE_UNUSED(max_leaf))
{
#if defined(__x86_64__)
#define CPUID_01_C_SSE3_0_MASK		(1 << 9)
#define CPUID_01_C_SSE4_1_MASK		(1 << 19)
#define CPUID_01_C_SSE4_2_MASK		(1 << 20)
#define CPUID_01_D_SSE1_0_MASK		(1 << 25)
#define CPUID_01_D_SSE2_0_MASK		(1 << 26)

	struct cpuid leaf;
	enum sse_version version = 0;

	if (max_leaf < 1)
		return 0;

	asm("cpuid" : "=c" (leaf.c), "=d" (leaf.d) : "a" (1), "c" (0));
	if (leaf.d & CPUID_01_D_SSE1_0_MASK)
		version = SSE1_0;
	if (version == SSE1_0 &&
	    (leaf.d & CPUID_01_D_SSE2_0_MASK))
		version = SSE2_0;
	if (version == SSE2_0 &&
	    (leaf.c & CPUID_01_C_SSE3_0_MASK))
		version = SSE3_0;
	if (version == SSE3_0 &&
	    (leaf.c & CPUID_01_C_SSE4_1_MASK))
		version = SSE4_1;
	if (version == SSE4_1 &&
	    (leaf.c & CPUID_01_C_SSE4_2_MASK))
		version = SSE4_2;
	return version;
#else
	return 0;
#endif
}

static int
has_bmi1(uint32_t MAYBE_UNUSED(max_leaf))
{
#if defined(__x86_64__)
#define CPUID_07_B_BMI1		(1 << 3)

	struct cpuid leaf;

	if (max_leaf < 7)
		return 0;
	asm("cpuid" : "=b" (leaf.b) : "a" (7), "c" (0));
	return (int)(leaf.b & CPUID_07_B_BMI1);
#else
	return 0;
#endif
}

static void
KS_str_init(void)
{
	uint32_t max_leaf = 0;

	if (is_intel(&max_leaf) &&
	    sse_version(max_leaf) >= SSE4_2 /* PCMPISTRM */ &&
	    has_bmi1(max_leaf) /* TZCNT */)
		KS_features |= KS_FEATURE_STR_NATIVE;
}

size_t
KS_str_match_default(const char *str, size_t len, const char *ranges)
{
	size_t i;

	for (i = 0; i < len; i++) {
		size_t j;
		int match = 0;

		for (j = 0; ranges[j] != '\0'; j += 2) {
			if (str[i] >= ranges[j] && str[i] <= ranges[j + 1]) {
				match = 1;
				break;
			}
		}
		if (!match)
			break;
	}

	return i;
}

size_t
KS_str_match_native(const char *MAYBE_UNUSED(str), size_t MAYBE_UNUSED(len),
    const char *MAYBE_UNUSED(ranges))
{
#if defined(__x86_64__)
	if (len < 8)
		return KS_str_match_default(str, len, ranges);

	char tail[16] = {0};
	size_t i = 0;
	uint64_t mask = 0;
	const uint8_t ctrl = (CTRL_SOURCE_U8 << CTRL_SOURCE_SHIFT) |
	    (CTRL_AGGREGATE_RANGES << CTRL_AGGREGATE_SHIFT) |
	    (CTRL_POLARITY_NEGATIVE << CTRL_POLARITY_SHIFT) |
	    (CTRL_OUTPUT_MASK_BITMASK << CTRL_OUTPUT_MASK_SHIFT);

	asm(
	    "   movdqu (%[ranges]), %%xmm1\n"
	    ".align 16\n"
	    "1: cmp $16, %[len]\n"
	    "   jb 3f\n"
	    "   pcmpistrm %[ctrl], (%[str],%[i]), %%xmm1\n"
	    /* As the polarity is negative, all zeros means that all 16
	     * bytes matched the ranges. */
	    "   jna 2f\n"
	    "   add $16, %[i]\n"
	    "   sub $16, %[len]\n"
	    "   jmp 1b\n"
	    /* Partial match, the number of leading zeroes represents the
	     * number of matched bytes as the polarity is negative. */
	    "2: movq %%xmm0, %[mask]\n"
	    "   tzcnt %w[mask], %w[mask]\n"
	    "   add %[mask], %[i]\n"
	    "   jmp 4f\n"
	    /* Copy the remaining less than 16 bytes to a dedicated buffer in
	     * order safetly continue loading 16 bytes worth of data. */
	    "3: lea (%[str],%[i]), %%rsi\n"
	    "   lea %[tail], %%rdi\n"
	    "   rep movsb\n"
	    "   pcmpistrm %[ctrl], %[tail], %%xmm1\n"
	    "   jmp 2b\n"
	    "4:\n"
	    : [i] "+a" (i), [len] "+c" (len), [mask] "+r" (mask),
	      [tail] "+m" (tail)
	    : [ctrl] "i" (ctrl), [ranges] "r" (ranges), [str] "r" (str)
	    : "xmm0", "xmm1", "rdi", "rsi");

	return i;
#else
	return 0;
#endif
}

#if defined(__clang__)
#  pragma GCC diagnostic pop
#endif

char **
KS_str_split(const char *str, char delim, struct arena_scope *s)
{
	VECTOR(char *) parts;

	ARENA_VECTOR_INIT(s, parts, 2);

	for (;;) {
		const char *p;
		char **dst;

		dst = VECTOR_ALLOC(parts);
		p = strchr(str, delim);
		if (p != NULL) {
			*dst = arena_strndup(s, str, (size_t)(p - str));
			str = &p[1];
		} else {
			*dst = arena_strdup(s, str);
			break;
		}
	}

	return parts;
}
