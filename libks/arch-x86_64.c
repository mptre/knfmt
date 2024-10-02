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

enum avx_version {
	AVX_UNSUPPORTED = 0,
	AVX1,
	AVX2,
	AVX512,
};

enum bmi_version {
	BMI_UNSUPPORTED = 0,
	BMI1,
	BMI2,
};

enum sse_version {
	SSE_UNSUPPORTED = 0,
	SSE1_0,
	SSE2_0,
	SSE3_0,
	SSE4_1,
	SSE4_2,
};

struct avx512 {
	unsigned int bw:1;
};

struct cpuid {
	uint32_t a, b, c, d;
};

int	KS_str_match_init_default(const char *, struct KS_str_match *);
size_t	KS_str_match_native_128(const char *, size_t,
    const struct KS_str_match *);
size_t	KS_str_match_native_256(const char *, size_t,
    const struct KS_str_match *);
size_t	KS_str_match_native_512(const char *, size_t,
    const struct KS_str_match *);
size_t	KS_str_match_until_native_128(const char *, size_t,
    const struct KS_str_match *);
size_t	KS_str_match_until_native_256(const char *, size_t,
    const struct KS_str_match *);

static void KS_init(void) __attribute__((constructor));

size_t (*KS_str_match)(const char *, size_t, const struct KS_str_match *) =
    KS_str_match_default;
size_t (*KS_str_match_until)(const char *, size_t, const struct KS_str_match *) =
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

static enum avx_version
avx_version(uint32_t max_leaf, struct avx512 *avx512)
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
	enum avx_version version = AVX_UNSUPPORTED;

	if (max_leaf < 1)
		return version;
	cpuid(1, 0, &leaf);
	if ((leaf.c & CPUID_01_C_OSXSAVE_MASK) == 0)
		return version;
	xcr0 = xgetbv(0);
	if ((xcr0 & XCR0_XMM_MASK) == 0 || (xcr0 & XCR0_YMM_MASK) == 0)
		return version;
	version = AVX1;

	if ((leaf.c & CPUID_01_C_AVX_MASK) == 0)
		return version;
	if (max_leaf < 7)
		return version;
	cpuid(7, 0, &leaf);
	if ((leaf.b & CPUID_07_B_AVX2_MASK) == 0)
		return version;
	version = AVX2;

	if ((xcr0 & XCR0_OPMASK_MASK) == 0 ||
	    (xcr0 & XCR0_ZMM_HI256_MASK) == 0 ||
	    (xcr0 & XCR0_HI16_ZMM_MASK) == 0)
		return version;
	if ((leaf.b & CPUID_07_B_AVXF_MASK) == 0)
		return version;
	version = AVX512;

	if (leaf.b & CPUID_07_B_AVXBW_MASK)
		avx512->bw = 1;

	return version;
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

static enum bmi_version
bmi_version(uint32_t max_leaf)
{
#define CPUID_07_B_BMI1_MASK		(1 << 3)
#define CPUID_07_B_BMI2_MASK		(1 << 8)

	struct cpuid leaf;
	enum bmi_version version = BMI_UNSUPPORTED;

	if (max_leaf < 7)
		return 0;
	cpuid(7, 0, &leaf);
	if (leaf.b & CPUID_07_B_BMI1_MASK)
		version = BMI1;
	if (leaf.b & CPUID_07_B_BMI2_MASK)
		version = BMI2;
	return version;
}

/*
 * The gist of RUNNING_ON_VALGRIND from valgrind.h.
 */
static int
is_valgrind_running(void)
{
	uint64_t request = 0x1001;
	int d;

	__asm__(
	    "rolq $3, %%rdi\n"
	    "rolq $13, %%rdi\n"
	    "rolq $61, %%rdi\n"
	    "rolq $51, %%rdi\n"
	    "xchgq %%rbx, %%rbx\n"
	    : [res] "=d" (d)
	    : "a" (&request), "[res]" (0)
	    : "cc", "memory");

	return d;
}

static void
KS_init(void)
{
	struct avx512 avx512 = {0};
	uint32_t max_leaf = 0;
	enum avx_version avx;
	enum bmi_version bmi;
	enum sse_version sse;

	if (!is_x86_64(&max_leaf))
		return;

	avx = avx_version(max_leaf, &avx512);
	bmi = bmi_version(max_leaf);
	sse = sse_version(max_leaf);

	if (avx >= AVX512 && avx512.bw && bmi >= BMI1 /* TZCNT */)
		KS_str_match = KS_str_match_native_512;
	else if (avx >= AVX2 && bmi >= BMI1 /* TZCNT */)
		KS_str_match = KS_str_match_native_256;
	else if (sse >= SSE4_2 /* PCMPISTRM */ && bmi >= BMI1 /* TZCNT */)
		KS_str_match = KS_str_match_native_128;

	if (avx >= AVX2 && bmi >= BMI2 /* BZHI */)
		KS_str_match_until = KS_str_match_until_native_256;
	else if (sse >= SSE4_2 /* PCMPISTRI */ &&
	    /* Valgrind does not emulate PCMPISTRI $0x4. */
	    !is_valgrind_running())
		KS_str_match_until = KS_str_match_until_native_128;
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
			match->u512[(j & 0xf) + 0x00] |= 1 << (j >> 4);
			match->u512[(j & 0xf) + 0x10] |= 1 << (j >> 4);
			match->u512[(j & 0xf) + 0x20] |= 1 << (j >> 4);
			match->u512[(j & 0xf) + 0x30] |= 1 << (j >> 4);
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
