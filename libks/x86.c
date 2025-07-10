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

#include "libks/x86.h"

#if defined(__x86_64__)

#include <stdint.h>
#include <string.h>

struct cpuid {
	uint32_t a, b, c, d;
};

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

static uint64_t
xgetbv(uint32_t regno)
{
	union {
		uint32_t u32[2];
		uint64_t u64;
	} xcr;

	__asm__("xgetbv"
	    : "=a" (xcr.u32[0]), "=d" (xcr.u32[1])
	    : "c" (regno));
	return xcr.u64;
}

static void
avx(uint32_t max_leaf, struct KS_x86_capabilites *caps)
{
#define CPUID_01_C_OSXSAVE_MASK		(1 << 27)
#define CPUID_01_C_AVX_MASK		(1 << 28)
#define CPUID_07_B_AVX2_MASK		(1 << 5)
#define CPUID_07_B_AVXF_MASK		(1 << 16)
#define CPUID_07_B_AVXBW_MASK		(1 << 30)
#define XCR0_XMM_MASK			(1 << 1)
#define XCR0_YMM_MASK			(1 << 2)
#define XCR0_OPMASK_MASK		(1 << 5)
#define XCR0_ZMM_HI256_MASK		(1 << 6)
#define XCR0_HI16_ZMM_MASK		(1 << 7)

	struct cpuid leaf;
	uint64_t xcr0;

	if (max_leaf < 1)
		return;
	cpuid(1, 0, &leaf);
	if ((leaf.c & CPUID_01_C_OSXSAVE_MASK) == 0)
		return;
	xcr0 = xgetbv(0);
	if ((xcr0 & XCR0_XMM_MASK) == 0 || (xcr0 & XCR0_YMM_MASK) == 0)
		return;
	caps->avx.major = 1;

	if ((leaf.c & CPUID_01_C_AVX_MASK) == 0)
		return;
	if (max_leaf < 7)
		return;
	cpuid(7, 0, &leaf);
	if ((leaf.b & CPUID_07_B_AVX2_MASK) == 0)
		return;
	caps->avx.major = 2;

	if ((xcr0 & XCR0_OPMASK_MASK) == 0 ||
	    (xcr0 & XCR0_ZMM_HI256_MASK) == 0 ||
	    (xcr0 & XCR0_HI16_ZMM_MASK) == 0)
		return;
	if ((leaf.b & CPUID_07_B_AVXF_MASK) == 0)
		return;
	caps->avx.major = 512;

	if (leaf.b & CPUID_07_B_AVXBW_MASK)
		caps->avx512.bw = 1;
}

static void
sse(uint32_t max_leaf, struct KS_x86_capabilites *caps)
{
#define CPUID_01_C_SSE3_0_MASK		(1 << 9)
#define CPUID_01_C_SSE4_1_MASK		(1 << 19)
#define CPUID_01_C_SSE4_2_MASK		(1 << 20)
#define CPUID_01_D_SSE1_0_MASK		(1 << 25)
#define CPUID_01_D_SSE2_0_MASK		(1 << 26)

	struct cpuid leaf;

	if (max_leaf < 1)
		return;

	cpuid(1, 0, &leaf);
	if (leaf.d & CPUID_01_D_SSE1_0_MASK)
		caps->sse.major = 1;
	if (caps->sse.major == 1 && (leaf.d & CPUID_01_D_SSE2_0_MASK))
		caps->sse.major = 2;
	if (caps->sse.major == 2 && (leaf.c & CPUID_01_C_SSE3_0_MASK))
		caps->sse.major = 3;
	if (caps->sse.major == 3 && (leaf.c & CPUID_01_C_SSE4_1_MASK)) {
		caps->sse.major = 4;
		caps->sse.minor = 1;
	}
	if (caps->sse.major == 4 && caps->sse.minor == 1 &&
	    (leaf.c & CPUID_01_C_SSE4_2_MASK)) {
		caps->sse.major = 4;
		caps->sse.minor = 2;
	}
}

static void
bmi(uint32_t max_leaf, struct KS_x86_capabilites *caps)
{
#define CPUID_07_B_BMI1_MASK		(1 << 3)
#define CPUID_07_B_BMI2_MASK		(1 << 8)

	struct cpuid leaf;

	if (max_leaf < 7)
		return;
	cpuid(7, 0, &leaf);
	if (leaf.b & CPUID_07_B_BMI1_MASK)
		caps->bmi.major = 1;
	if (leaf.b & CPUID_07_B_BMI2_MASK)
		caps->bmi.major = 2;
}

int
KS_x86_capabilites(struct KS_x86_capabilites *caps)
{
	uint32_t max_leaf;

	if (!is_x86_64(&max_leaf))
		return 0;

	memset(caps, 0, sizeof(*caps));
	avx(max_leaf, caps);
	bmi(max_leaf, caps);
	sse(max_leaf, caps);
	return 1;
}

#else

#include "libks/compiler.h"

int
KS_x86_capabilites(struct KS_x86_capabilites *UNUSED(caps))
{
	return 0;
}

#endif
