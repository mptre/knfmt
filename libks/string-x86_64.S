#if defined(__x86_64__)

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

#include "libks/asm.h"

ASM_PROLOGUE

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

	.intel_syntax noprefix

ENTRY(KS_str_match_native)
ctrl_mask = \
	(CTRL_SOURCE_U8 << CTRL_SOURCE_SHIFT) | \
	(CTRL_AGGREGATE_RANGES << CTRL_AGGREGATE_SHIFT) | \
	(CTRL_POLARITY_NEGATIVE << CTRL_POLARITY_SHIFT) | \
	(CTRL_OUTPUT_MASK_BITMASK << CTRL_OUTPUT_MASK_SHIFT)
	xor	eax, eax
	mov	r8, rdi
	mov	rcx, rsi
	movdqu	xmm1, [rdx]
	.align 16
.Lloop:
	cmp	rcx, 16
	jb	.Ltail
	pcmpistrm xmm1, [r8 + rax], ctrl_mask
	/* As the polarity is negative, all zeros means that all 16
	 * bytes matched the ranges. */
	jna	.Lpartial_match
	add	rax, 16
	sub	rcx, 16
	jmp	.Lloop
	/* Partial match, the number of leading zeroes represents the
	 * number of matched bytes as the polarity is negative. */
.Lpartial_match:
	movq	r9, xmm0
	tzcnt	r9w, r9w
	add	rax, r9
	ret
	/* Copy the remaining less than 16 bytes to a dedicated buffer in
	 * order safely continue loading 16 bytes worth of data. Rely on the
	 * stack red zone since this is a leaf function. */
.Ltail:
	lea	rsi, [r8 + rax]
	pxor	xmm0, xmm0
	movaps	[rsp - 0x18], xmm0
	lea	rdi, [rsp - 0x18]
	rep	movsb
	pcmpistrm xmm1, [rsp - 0x18], ctrl_mask
	jmp	.Lpartial_match

#endif
