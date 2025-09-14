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

#ifndef LIBKS_CAPABILITIES_H
#define LIBKS_CAPABILITIES_H

#include <stdint.h>

#define CPUID_01_C_SSE3_0_MASK		(1 << 9)
#define CPUID_01_C_SSE4_1_MASK		(1 << 19)
#define CPUID_01_C_SSE4_2_MASK		(1 << 20)
#define CPUID_01_C_OSXSAVE_MASK		(1 << 27)
#define CPUID_01_C_AVX_MASK		(1 << 28)

#define CPUID_01_D_SSE1_0_MASK		(1 << 25)
#define CPUID_01_D_SSE2_0_MASK		(1 << 26)

#define CPUID_07_B_BMI1_MASK		(1 << 3)
#define CPUID_07_B_AVX2_MASK		(1 << 5)
#define CPUID_07_B_BMI2_MASK		(1 << 8)
#define CPUID_07_B_AVXF_MASK		(1 << 16)
#define CPUID_07_B_AVXBW_MASK		(1 << 30)

#define XCR0_XMM_MASK			(1 << 1)
#define XCR0_YMM_MASK			(1 << 2)
#define XCR0_OPMASK_MASK		(1 << 5)
#define XCR0_ZMM_HI256_MASK		(1 << 6)
#define XCR0_HI16_ZMM_MASK		(1 << 7)

struct KS_x86_capabilites {
	uint32_t mode;
	uint32_t avx;
	uint32_t bmi;
	uint32_t sse;

	struct {
		uint32_t bw:1;
	} avx512;
};

int KS_x86_capabilites(struct KS_x86_capabilites *);

#endif /* !LIBKS_CAPABILITIES_H */
