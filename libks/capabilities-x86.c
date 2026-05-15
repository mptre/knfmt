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
#include "libks/compiler.h"

enum vendor {
	Vendor_Intel,
	Vendor_Amd,
};

struct cpuid {
	uint32_t a, b, c, d;
};

struct fms {
	uint32_t f, m, s;
};

struct enumerations {
	struct cpuid cpuid_01,
		     cpuid_07,
		     cpuid_80000001;

	uint64_t xcr0;
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
is_x86(uint32_t *max_leaf, uint32_t *extended_max_leaf, enum vendor *vendor)
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
	if (memcmp(ident.u8, "GenuineIntel", sizeof(ident)) == 0)
		*vendor = Vendor_Intel;
	else if (memcmp(ident.u8, "AuthenticAMD", sizeof(ident)) == 0)
		*vendor = Vendor_Amd;
	else
		return 0;
	*max_leaf = leaf.a;

	KS_cpuid(0x80000000, 0, &leaf);
	*extended_max_leaf = leaf.a;

	return 1;
}

static void
fms(const struct enumerations *e, struct fms *fms)
{
	uint32_t base_family = (e->cpuid_01.a >> 8) & 0xf;
	uint32_t ext_family = (e->cpuid_01.a >> 20) & 0xff;
	fms->f = base_family + (base_family == 0xf ? ext_family : 0);

	uint32_t base_model = (e->cpuid_01.a >> 4) & 0xf;
	uint32_t ext_model = (e->cpuid_01.a >> 16) & 0xf;
	fms->m = base_model + (base_family == 0x6 || base_family == 0xf ? (ext_model << 4) : 0);

	fms->s = e->cpuid_01.a & 0xf;
}

static void
intel_uarch(const struct enumerations *e, struct KS_x86_capabilites *caps)
{
	/* Based on intel-family.h from the Linux kernel. */
	static const struct {
		enum KS_x86_uarch uarch;
		struct fms fms;
	} lut[] = {
		/* clang-format off */

		{ KS_X86_INTEL_CORE_YONAH,       { 6, 0x0e, 0 } },
		{ KS_X86_INTEL_CORE2_MEROM,      { 6, 0x0f, 0 } },
		{ KS_X86_INTEL_CORE2_MEROM,      { 6, 0x16, 0 } },
		{ KS_X86_INTEL_CORE2_PENRYN,     { 6, 0x17, 0 } },
		{ KS_X86_INTEL_NEHALEM_EP,       { 6, 0x1a, 0 } },
		{ KS_X86_INTEL_BONNELL,          { 6, 0x1c, 0 } },
		{ KS_X86_INTEL_CORE2_DUNNINGTON, { 6, 0x1d, 0 } },
		{ KS_X86_INTEL_NEHALEM,          { 6, 0x1e, 0 } },
		{ KS_X86_INTEL_NEHALEM,          { 6, 0x1f, 0 } },
		{ KS_X86_INTEL_WESTMERE,         { 6, 0x25, 0 } },
		{ KS_X86_INTEL_BONNELL_MID,      { 6, 0x26, 0 } },
		{ KS_X86_INTEL_SALTWELL_MID,     { 6, 0x27, 0 } },
		{ KS_X86_INTEL_SANDYBRIDGE,      { 6, 0x2a, 0 } },
		{ KS_X86_INTEL_WESTMERE_EP,      { 6, 0x2c, 0 } },
		{ KS_X86_INTEL_SANDYBRIDGE,      { 6, 0x2d, 0 } },
		{ KS_X86_INTEL_NEHALEM_EX,       { 6, 0x2e, 0 } },
		{ KS_X86_INTEL_WESTMERE_EX,      { 6, 0x2f, 0 } },
		{ KS_X86_INTEL_SALTWELL_TABLET,  { 6, 0x35, 0 } },
		{ KS_X86_INTEL_SALTWELL,         { 6, 0x36, 0 } },
		{ KS_X86_INTEL_SILVERMONT,       { 6, 0x37, 0 } },
		{ KS_X86_INTEL_IVYBRIDGE,        { 6, 0x3a, 0 } },
		{ KS_X86_INTEL_HASWELL,          { 6, 0x3c, 0 } },
		{ KS_X86_INTEL_BROADWELL,        { 6, 0x3d, 0 } },
		{ KS_X86_INTEL_IVYBRIDGE,        { 6, 0x3e, 0 } },
		{ KS_X86_INTEL_HASWELL,          { 6, 0x3f, 0 } },
		{ KS_X86_INTEL_HASWELL,          { 6, 0x45, 0 } },
		{ KS_X86_INTEL_HASWELL,          { 6, 0x46, 0 } },
		{ KS_X86_INTEL_BROADWELL,        { 6, 0x47, 0 } },
		{ KS_X86_INTEL_SILVERMONT_MID,   { 6, 0x4a, 0 } },
		{ KS_X86_INTEL_AIRMONT,          { 6, 0x4c, 0 } },
		{ KS_X86_INTEL_SILVERMONT,       { 6, 0x4d, 0 } },
		{ KS_X86_INTEL_SKYLAKE,          { 6, 0x4e, 0 } },
		{ KS_X86_INTEL_BROADWELL,        { 6, 0x4f, 0 } },
		{ KS_X86_INTEL_SKYLAKE,          { 6, 0x55, 0 } },
		{ KS_X86_INTEL_CASCADELAKE,      { 6, 0x55, 7 } },
		{ KS_X86_INTEL_COOPERLAKE,       { 6, 0x55, 11 } },
		{ KS_X86_INTEL_BROADWELL,        { 6, 0x56, 0 } },
		{ KS_X86_INTEL_XEON_PHI_KNL,     { 6, 0x57, 0 } },
		{ KS_X86_INTEL_SKYLAKE,          { 6, 0x5e, 0 } },
		{ KS_X86_INTEL_SILVERMONT_MID2,  { 6, 0x5a, 0 } },
		{ KS_X86_INTEL_GOLDMONT,         { 6, 0x5c, 0 } },
		{ KS_X86_INTEL_GOLDMONT,         { 6, 0x5f, 0 } },
		{ KS_X86_INTEL_CANNONLAKE,       { 6, 0x66, 0 } },
		{ KS_X86_INTEL_ICELAKE,          { 6, 0x6a, 0 } },
		{ KS_X86_INTEL_ICELAKE,          { 6, 0x6c, 0 } },
		{ KS_X86_INTEL_AIRMONT_NP,       { 6, 0x75, 0 } },
		{ KS_X86_INTEL_ICELAKE,          { 6, 0x7d, 0 } },
		{ KS_X86_INTEL_ICELAKE,          { 6, 0x7e, 0 } },
		{ KS_X86_INTEL_GOLDMONT_PLUS,    { 6, 0x7a, 0 } },
		{ KS_X86_INTEL_XEON_PHI_KNM,     { 6, 0x85, 0 } },
		{ KS_X86_INTEL_TREMONT,          { 6, 0x86, 0 } },
		{ KS_X86_INTEL_TIGERLAKE,        { 6, 0x8c, 0 } },
		{ KS_X86_INTEL_TIGERLAKE,        { 6, 0x8d, 0 } },
		{ KS_X86_INTEL_KABYLAKE,         { 6, 0x8e, 0 } },
		{ KS_X86_INTEL_AMBERLAKE,        { 6, 0x8e, 9 } },
		{ KS_X86_INTEL_COFFEELAKE,       { 6, 0x8e, 10 } },
		{ KS_X86_INTEL_WHISKEYLAKE,      { 6, 0x8e, 11 } },
		{ KS_X86_INTEL_WHISKEYLAKE,      { 6, 0x8e, 12 } },
		{ KS_X86_INTEL_SAPPHIRERAPIDS,   { 6, 0x8f, 0 } },
		{ KS_X86_INTEL_LAKEFIELD,        { 6, 0x8a, 0 } },
		{ KS_X86_INTEL_TREMONT,          { 6, 0x96, 0 } },
		{ KS_X86_INTEL_ALDERLAKE,        { 6, 0x97, 0 } },
		{ KS_X86_INTEL_KABYLAKE,         { 6, 0x9e, 0 } },
		{ KS_X86_INTEL_COFFEELAKE,       { 6, 0x9e, 10 } },
		{ KS_X86_INTEL_COFFEELAKE,       { 6, 0x9e, 11 } },
		{ KS_X86_INTEL_COFFEELAKE,       { 6, 0x9e, 12 } },
		{ KS_X86_INTEL_COFFEELAKE,       { 6, 0x9e, 13 } },
		{ KS_X86_INTEL_ALDERLAKE,        { 6, 0x9a, 0 } },
		{ KS_X86_INTEL_TREMONT,          { 6, 0x9c, 0 } },
		{ KS_X86_INTEL_ICELAKE_NNPI,     { 6, 0x9d, 0 } },
		{ KS_X86_INTEL_METEORLAKE,       { 6, 0xaa, 0 } },
		{ KS_X86_INTEL_METEORLAKE,       { 6, 0xac, 0 } },
		{ KS_X86_INTEL_GRANITERAPIDS,    { 6, 0xad, 0 } },
		{ KS_X86_INTEL_GRANITERAPIDS,    { 6, 0xae, 0 } },
		{ KS_X86_INTEL_CRESTMONT,        { 6, 0xaf, 0 } },
		{ KS_X86_INTEL_ARROWLAKE,        { 6, 0xb5, 0 } },
		{ KS_X86_INTEL_CRESTMONT,        { 6, 0xb6, 0 } },
		{ KS_X86_INTEL_RAPTORLAKE,       { 6, 0xb7, 0 } },
		{ KS_X86_INTEL_RAPTORLAKE,       { 6, 0xba, 0 } },
		{ KS_X86_INTEL_GRACEMONT,        { 6, 0xbe, 0 } },
		{ KS_X86_INTEL_RAPTORLAKE,       { 6, 0xbf, 0 } },
		{ KS_X86_INTEL_COMETLAKE,        { 6, 0xa5, 0 } },
		{ KS_X86_INTEL_COMETLAKE,        { 6, 0xa6, 0 } },
		{ KS_X86_INTEL_ROCKETLAKE,       { 6, 0xa7, 0 } },
		{ KS_X86_INTEL_LUNARLAKE,        { 6, 0xbd, 0 } },
		{ KS_X86_INTEL_ARROWLAKE,        { 6, 0xc5, 0 } },
		{ KS_X86_INTEL_ARROWLAKE,        { 6, 0xc6, 0 } },
		{ KS_X86_INTEL_PANTHERLAKE,      { 6, 0xcc, 0 } },  /* Cougar Cove / Darkmont */
		{ KS_X86_INTEL_PANTHERLAKE,      { 6, 0xe5, 0 } },  /* Cougar Cove / Darkmont */
		{ KS_X86_INTEL_EMERALDRAPIDS,    { 6, 0xcf, 0 } },
		{ KS_X86_INTEL_WILDCATLAKE,      { 6, 0xd5, 0 } },
		{ KS_X86_INTEL_BARTLETTLAKE,     { 6, 0xd7, 0 } },
		{ KS_X86_INTEL_DARKMONT,         { 6, 0xdd, 0 } },

		{ KS_X86_INTEL_NOVALAKE,         { 18, 0x03, 0 } },

		{ KS_X86_INTEL_DIAMONDRAPIDS,    { 19, 0x01, 0 } },

		/* clang-format on */
	};

	struct fms f = {0};
	fms(e, &f);
	for (uint32_t i = 0; i < countof(lut); i++) {
		if (f.f == lut[i].fms.f && f.m == lut[i].fms.m && f.s == lut[i].fms.s) {
			caps->uarch = lut[i].uarch;
			break;
		}
	}
}

static void
amd_uarch(const struct enumerations *e, struct KS_x86_capabilites *caps)
{
	static const struct {
		enum KS_x86_uarch uarch;
		struct fms fms;
	} lut[] = {
		/* clang-format off */

		{ KS_X86_AMD_JAGUAR,       { 22, 0x30, 1 } },

		/* clang-format on */
	};

	struct fms f = {0};
	fms(e, &f);
	for (uint32_t i = 0; i < countof(lut); i++) {
		if (f.f == lut[i].fms.f && f.m == lut[i].fms.m && f.s == lut[i].fms.s) {
			caps->uarch = lut[i].uarch;
			break;
		}
	}
}

static void
mode(struct KS_x86_capabilites *caps)
{
	/* In 32-bit mode, the opcodes will be interpreted as dec eax; nop.
	 * In 64-bit mode, the opcodes will be interpreted as rex.w nop.
	 * As the eax register is initialized to 1, we must be operating in
	 * 32-bit mode if the same register is equal to 0 after executing this
	 * sequence. */
	int is_64 = 1;
	__asm__ volatile (".byte 0x48, 0x90" : "=a" (is_64) : "a" (is_64));
	caps->mode = is_64 ? 64 : 32;
}

static void
avx(const struct enumerations *e, struct KS_x86_capabilites *caps)
{
	if ((e->cpuid_01.c & CPUID_01_C_OSXSAVE_MASK) == 0)
		return;
	if ((e->xcr0 & XCR0_XMM_MASK) == 0 || (e->xcr0 & XCR0_YMM_MASK) == 0)
		return;
	caps->avx = 1;

	if ((e->cpuid_01.c & CPUID_01_C_AVX_MASK) == 0)
		return;
	if ((e->cpuid_07.b & CPUID_07_B_AVX2_MASK) == 0)
		return;
	caps->avx = 2;

	if ((e->xcr0 & XCR0_OPMASK_MASK) == 0 ||
	    (e->xcr0 & XCR0_ZMM_HI256_MASK) == 0 ||
	    (e->xcr0 & XCR0_HI16_ZMM_MASK) == 0)
		return;
	if ((e->cpuid_07.b & CPUID_07_B_AVXF_MASK) == 0)
		return;
	caps->avx = 512;

	if (e->cpuid_07.b & CPUID_07_B_AVXBW_MASK)
		caps->avx512.bw = 1;
}

static void
sse(const struct enumerations *e, struct KS_x86_capabilites *caps)
{
	if (e->cpuid_01.d & CPUID_01_D_SSE1_0_MASK)
		caps->sse = 0x10;
	if (caps->sse == 0x10 && (e->cpuid_01.d & CPUID_01_D_SSE2_0_MASK))
		caps->sse = 0x20;
	if (caps->sse == 0x20 && (e->cpuid_01.c & CPUID_01_C_SSE3_0_MASK))
		caps->sse = 0x30;
	if (caps->sse == 0x30 && (e->cpuid_01.c & CPUID_01_C_SSE4_1_MASK))
		caps->sse = 0x41;
	if (caps->sse == 0x41 && (e->cpuid_01.c & CPUID_01_C_SSE4_2_MASK))
		caps->sse = 0x42;
}

static void
bmi(const struct enumerations *e, struct KS_x86_capabilites *caps)
{
	if (e->cpuid_07.b & CPUID_07_B_BMI1_MASK)
		caps->bmi = 1;
	if (e->cpuid_07.b & CPUID_07_B_BMI2_MASK)
		caps->bmi = 2;
}

static void
fsgsbase(const struct enumerations *e, struct KS_x86_capabilites *caps)
{
	if (e->cpuid_07.b & CPUID_07_B_FSGSBASE_MASK)
		caps->fsgsbase = 1;
}

static void
lzcnt(const struct enumerations *e, struct KS_x86_capabilites *caps)
{
	if (e->cpuid_80000001.c & CPUID_0x80000001_C_LZCNT_MASK)
		caps->lzcnt = 1;
}

int
KS_x86_capabilites_impl(struct KS_x86_capabilites *caps)
{
	uint32_t extended_max_leaf, max_leaf;
	enum vendor vendor;
	if (!is_x86(&max_leaf, &extended_max_leaf, &vendor))
		return 0;

	struct enumerations e = {0};
	if (max_leaf >= 1)
		KS_cpuid(1, 0, &e.cpuid_01);
	if (max_leaf >= 7)
		KS_cpuid(7, 0, &e.cpuid_07);
	if (extended_max_leaf >= 0x80000001)
		KS_cpuid(0x80000001, 0, &e.cpuid_80000001);
	e.xcr0 = KS_xgetbv(0);

	if (vendor == Vendor_Intel)
		intel_uarch(&e, caps);
	else if (vendor == Vendor_Amd)
		amd_uarch(&e, caps);
	mode(caps);
	avx(&e, caps);
	bmi(&e, caps);
	fsgsbase(&e, caps);
	lzcnt(&e, caps);
	sse(&e, caps);
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

#include <stddef.h>	/* NULL */
#include "libks/compiler.h"

const struct KS_x86_capabilites *
KS_x86_capabilites(void)
{
	return NULL;
}

#endif
