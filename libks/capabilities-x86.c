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

#include "libks/capabilities.h"

#if defined(__x86_64__) || defined(__i386__)

#include <stdint.h>
#include <string.h>

struct cpuid {
	uint32_t a, b, c, d;
};

int KS_x86_capabilites_impl(struct KS_x86_capabilites *);

static void cpuid(uint32_t, uint32_t, struct cpuid *);
static uint64_t xgetbv(uint32_t);

extern void (*KS_cpuid)(uint32_t, uint32_t, struct cpuid *);
void (*KS_cpuid)(uint32_t, uint32_t, struct cpuid *) = cpuid;

extern uint64_t (*KS_xgetbv)(uint32_t);
uint64_t (*KS_xgetbv)(uint32_t) = xgetbv;

static void
cpuid(uint32_t leaf, uint32_t subleaf, struct cpuid *out)
{
	__asm__("cpuid"
	    : "=a" (out->a), "=b" (out->b), "=c" (out->c), "=d" (out->d)
	    : "a" (leaf), "c" (subleaf));
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

static int
is_x86(uint32_t *max_leaf, uint32_t *extended_max_leaf)
{
	union {
		uint8_t u8[12];
		uint32_t u32[3];
	} ident;
	struct cpuid leaf;

	KS_cpuid(0, 0, &leaf);
	ident.u32[0] = leaf.b;
	ident.u32[1] = leaf.d;
	ident.u32[2] = leaf.c;
	if (memcmp(ident.u8, "GenuineIntel", sizeof(ident)) != 0 &&
	    memcmp(ident.u8, "AuthenticAMD", sizeof(ident)) != 0)
		return 0;
	*max_leaf = leaf.a;

	KS_cpuid(0x80000000, 0, &leaf);
	*extended_max_leaf = leaf.a;

	return 1;
}

static void
mode(struct KS_x86_capabilites *caps)
{
	/* In 32-bit mode, the opcodes will be interpreted as dec eax, nop.
	 * In 64-bit mode, the opcodes will be interpreted as rex.w nop.
	 * As the eax register is initialized to 1, we must be operating in
	 * 32-bit mode if the same register is equal to 0 after executing these
	 * two opcodes. */
	int is_64 = 1;
	__asm__ volatile (".byte 0x48, 0x90" : "=a" (is_64) : "a" (is_64));
	caps->mode = is_64 ? 64 : 32;
}

static void
avx(uint32_t max_leaf, struct KS_x86_capabilites *caps)
{
	struct cpuid leaf;
	uint64_t xcr0;

	if (max_leaf < 1)
		return;
	KS_cpuid(1, 0, &leaf);
	if ((leaf.c & CPUID_01_C_OSXSAVE_MASK) == 0)
		return;
	xcr0 = KS_xgetbv(0);
	if ((xcr0 & XCR0_XMM_MASK) == 0 || (xcr0 & XCR0_YMM_MASK) == 0)
		return;
	caps->avx = 1;

	if ((leaf.c & CPUID_01_C_AVX_MASK) == 0)
		return;
	if (max_leaf < 7)
		return;
	KS_cpuid(7, 0, &leaf);
	if ((leaf.b & CPUID_07_B_AVX2_MASK) == 0)
		return;
	caps->avx = 2;

	if ((xcr0 & XCR0_OPMASK_MASK) == 0 ||
	    (xcr0 & XCR0_ZMM_HI256_MASK) == 0 ||
	    (xcr0 & XCR0_HI16_ZMM_MASK) == 0)
		return;
	if ((leaf.b & CPUID_07_B_AVXF_MASK) == 0)
		return;
	caps->avx = 512;

	if (leaf.b & CPUID_07_B_AVXBW_MASK)
		caps->avx512.bw = 1;
}

static void
sse(uint32_t max_leaf, struct KS_x86_capabilites *caps)
{
	struct cpuid leaf;

	if (max_leaf < 1)
		return;

	KS_cpuid(1, 0, &leaf);
	if (leaf.d & CPUID_01_D_SSE1_0_MASK)
		caps->sse = 0x10;
	if (caps->sse == 0x10 && (leaf.d & CPUID_01_D_SSE2_0_MASK))
		caps->sse = 0x20;
	if (caps->sse == 0x20 && (leaf.c & CPUID_01_C_SSE3_0_MASK))
		caps->sse = 0x30;
	if (caps->sse == 0x30 && (leaf.c & CPUID_01_C_SSE4_1_MASK))
		caps->sse = 0x41;
	if (caps->sse == 0x41 && (leaf.c & CPUID_01_C_SSE4_2_MASK))
		caps->sse = 0x42;
}

static void
bmi(uint32_t max_leaf, struct KS_x86_capabilites *caps)
{
	struct cpuid leaf;

	if (max_leaf < 7)
		return;
	KS_cpuid(7, 0, &leaf);
	if (leaf.b & CPUID_07_B_BMI1_MASK)
		caps->bmi = 1;
	if (leaf.b & CPUID_07_B_BMI2_MASK)
		caps->bmi = 2;
}

static void
lzcnt(uint32_t extended_max_leaf, struct KS_x86_capabilites *caps)
{
	if (extended_max_leaf < 0x80000001)
		return;

	struct cpuid leaf;
	KS_cpuid(0x80000001, 0, &leaf);
	if (leaf.c & CPUID_0x80000001_C_LZCNT_MASK)
		caps->lzcnt = 1;
}

int
KS_x86_capabilites_impl(struct KS_x86_capabilites *caps)
{
	uint32_t extended_max_leaf, max_leaf;
	if (!is_x86(&max_leaf, &extended_max_leaf))
		return 0;

	mode(caps);
	avx(max_leaf, caps);
	bmi(max_leaf, caps);
	lzcnt(extended_max_leaf, caps);
	sse(max_leaf, caps);
	return 1;
}

const struct KS_x86_capabilites *
KS_x86_capabilites(void)
{
	static struct KS_x86_capabilites storage = {0};
	static struct KS_x86_capabilites *caps = NULL;
	static int first = 1;
	if (!first)
		return caps;
	first = 0;

	if (!KS_x86_capabilites_impl(&storage))
		return NULL;
	caps = &storage;
	return caps;
}

#else

#include "libks/compiler.h"

const struct KS_x86_capabilites *
KS_x86_capabilites(void)
{
	return NULL;
}

#endif
