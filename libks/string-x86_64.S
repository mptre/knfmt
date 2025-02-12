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

#define KS_STR_MATCH_U8			0
#define KS_STR_MATCH_U512		64

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

	ASM_PROLOGUE
	.intel_syntax noprefix

/*
 * Copy 16 bytes to a stack buffer in order safely continue loading 16 bytes
 * worth of data. Rely on the stack red zone since this is only expected to be
 * used in leaf functions.
 *
 * Clobbers rcx, rdx, rdi, rsi, xmm0.
 */
.macro copy_to_stack_128
	lea	rsi, [rdi + rax]
	lea	rdi, [rsp - 16]
	/* Ensure alignment. */
	and	rdi, -16
	mov	rdx, rdi
	pxor	xmm0, xmm0
	movaps	[rdi], xmm0
	rep	movsb
.endm

/*
 * Copy 32 bytes to a stack buffer in order safely continue loading 32 bytes
 * worth of data. Rely on the stack red zone since this is only expected to be
 * used in leaf functions.
 *
 * Clobbers rcx, rdx, rdi, r8, ymm0.
 */
.macro copy_to_stack_256
	mov	r8, rsi
	mov	rcx, rsi
	lea	rsi, [rdi]
	lea	rdx, [rsp - 32]
	/* Ensure alignment. */
	and	rdx, -32
	mov	rdi, rdx
	vmovaps	[rdi], ymm0
	rep	movsb
	mov	rdi, rdx
	mov	rsi, r8
.endm

.macro vpcmpneb reg1, reg2, reg3
	vpcmpb \reg1, \reg2, \reg3, 4
.endm

ENTRY(KS_str_match_native_128)
ctrl_mask = \
	(CTRL_SOURCE_U8 << CTRL_SOURCE_SHIFT) | \
	(CTRL_AGGREGATE_RANGES << CTRL_AGGREGATE_SHIFT) | \
	(CTRL_POLARITY_NEGATIVE << CTRL_POLARITY_SHIFT) | \
	(CTRL_OUTPUT_MASK_BITMASK << CTRL_OUTPUT_MASK_SHIFT)
	xor	eax, eax
	mov	rcx, rsi
	movdqa	xmm1, [rdx + KS_STR_MATCH_U8]
	.align 16
.Lmatch_128_loop:
	cmp	rcx, 16
	jb	.Lmatch_128_tail
	pcmpistrm xmm1, [rdi + rax], ctrl_mask
	/* As the polarity is negative, all zeros means that all 16
	 * bytes matched the ranges. */
	jna	.Lmatch_128_partial_match
	add	rax, 16
	sub	rcx, 16
	jmp	.Lmatch_128_loop
.Lmatch_128_partial_match:
	/* Partial match, the number of leading zeroes represents the
	 * number of matched bytes as the polarity is negative. */
	movq	r9, xmm0
	tzcnt	r9w, r9w
	add	rax, r9
	ret
.Lmatch_128_tail:
	copy_to_stack_128
	pcmpistrm xmm1, [rdx], ctrl_mask
	jmp	.Lmatch_128_partial_match
END(KS_str_match_native_128)

	.align 16

ENTRY(KS_str_match_native_256)
	xor	eax, eax
	xor	r9, r9
	vpxor	ymm0, ymm0, ymm0
	vpcmpeqd ymm1, ymm1, ymm1
	/* Load aligned constants. */
	vmovdqa	ymm4, [rdx + KS_STR_MATCH_U512]
	vmovdqa	ymm5, [rip + .Lmatch_bit_mask]
	vmovdqa	ymm6, [rip + .Lmatch_lo_mask]
	test	rdi, 0x1f
	jnz	.Lmatch_256_unaligned
	.align 16
.Lmatch_256_loop:
	cmp	rsi, 32
	jb	.Lmatch_256_tail
.Lmatch_256_match:
	/* Match low nibbles. */
	vpshufb	ymm2, ymm4, [rdi]
	/* Match high nibbles. As no byte granular vector shift instruction is
	 * available, operate on words (16 bits) which requires masking to get
	 * rid of spill. */
	vmovdqa	ymm3, [rdi]
	vpsraw	ymm3, ymm3, 4
	vpand	ymm3, ymm3, ymm6
	/* Transform high nibbles into bitmask. */
	vpshufb	ymm3, ymm5, ymm3
	vpand	ymm2, ymm2, ymm3
	vpcmpeqb ymm2, ymm2, ymm0
	vpandn	ymm2, ymm2, ymm1
	/* Construct mask in which each bit represents a byte, a set bit implies
	 * that the corresponding byte is non-zero. */
	vpmovmskb edx, ymm2
	/* If 32 bits are set implies that 32 bytes worth of input did match. */
	cmp	edx, -1
	jne	.Lmatch_256_partial_match
	add	rax, 32
	add	rdi, 32
	sub	rsi, 32
	jmp	.Lmatch_256_loop
.Lmatch_256_partial_match:
	/* Invert match bitmask, number of consecutive zeros in the least
	 * significant bits represents the number of matched bytes. */
	not	edx
	tzcnt	edx, edx
	add 	rax, rdx
	test	r9, r9
	jnz	.Lmatch_256_aligned
	ret
.Lmatch_256_tail:
	copy_to_stack_256
	jmp	.Lmatch_256_match
.Lmatch_256_unaligned:
	/* Handle unaligned input by copying the bytes to an aligned stack
	 * buffer. Only relevant if there's 32 or more bytes left, otherwise
	 * just fallback to the tail logic. */
	cmp	rsi, 32
	jl	.Lmatch_256_tail
	push	rdi
	push	rsi
	mov	r8, rdi
	and	r8, 0x1f
	mov	r9, 32
	sub	r9, r8
	mov	rsi, r9
	jmp	.Lmatch_256_tail
.Lmatch_256_aligned:
	/* Restore rdi and rsi after handling unaligned input. Number of handled
	 * unaligned bytes is reflected in r9. */
	pop	rsi
	pop	rdi
	add	rdi, r9
	sub	rsi, r9
	/* Unaligned input can only occur on the first iteration. Hence, rax
	 * reflects the number of matched bytes in the unaligned input. If rax
	 * is not equal to the number of unaligned bytes we have a partial
	 * match. */
	cmp	rax, r9
	jne	.Lmatch_256_done
	xor	r9, r9
	jmp	.Lmatch_256_loop
.Lmatch_256_done:
	ret
END(KS_str_match_native_256)

	.align 16

ENTRY(KS_str_match_native_512)
	xor	eax, eax
	xor	r9, r9
	vpxorq	zmm0, zmm0, zmm0
	/* Load aligned constants. */
	vmovdqa64 zmm4, [rdx + KS_STR_MATCH_U512]
	vmovdqa64 zmm5, [rip + .Lmatch_bit_mask]
	vmovdqa64 zmm6, [rip + .Lmatch_lo_mask]
	test	rdi, 0x3f
	jnz	.Lmatch_512_unaligned
	.align 16
.Lmatch_512_loop:
	cmp	rsi, 64
	jb	.Lmatch_512_tail
.Lmatch_512_match:
	/* Match low nibbles. */
	vpshufb	zmm2, zmm4, [rdi]
	/* Match high nibbles. As no byte granular vector shift instruction is
	 * available, operate on words (16 bits) which requires masking to get
	 * rid of spill. */
	vmovdqa64 zmm3, [rdi]
	vpsraw	zmm3, zmm3, 4
	vpandq	zmm3, zmm3, zmm6
	/* Transform high nibbles into bitmask. */
	vpshufb	zmm3, zmm5, zmm3
	vpandq	zmm2, zmm2, zmm3
	/* Construct mask in which each bit represents a byte, a set bit implies
	 * that the corresponding byte is non-zero. */
	vpcmpneb k1, zmm0, zmm2
	kmovq	rdx, k1
	/* If 64 bits are set implies that 64 bytes worth of input did match. */
	cmp	rdx, -1
	jne	.Lmatch_512_partial_match
	add	rax, 64
	add	rdi, 64
	sub	rsi, 64
	jmp	.Lmatch_512_loop
.Lmatch_512_partial_match:
	/* Invert match bitmask, number of consecutive zeros in the least
	 * significant bits represents the number of matched bytes. */
	not	rdx
	tzcnt	rdx, rdx
	add 	rax, rdx
	test	r9, r9
	jnz	.Lmatch_512_aligned
	ret
.Lmatch_512_tail:
	/* Copy the remaining less than 64 bytes to a dedicated buffer in
	 * order safely continue loading 64 bytes worth of data. Rely on the
	 * stack red zone since this is a leaf function. */
	mov	rcx, rsi
	lea	rsi, [rdi]
	lea	rdx, [rsp - 64]
	/* Ensure 64 bytes alignment. */
	and	rdx, -64
	mov	rdi, rdx
	vmovaps	[rdi], zmm0
	rep	movsb
	mov	rdi, rdx
	jmp	.Lmatch_512_match
.Lmatch_512_unaligned:
	/* Handle unaligned input by copying the bytes to an aligned stack
	 * buffer. Only relevant if there's 64 or more bytes left, otherwise
	 * just fallback to the tail logic. */
	cmp	rsi, 64
	jl	.Lmatch_512_tail
	push	rdi
	push	rsi
	mov	r8, rdi
	and	r8, 0x3f
	mov	r9, 64
	sub	r9, r8
	mov	rsi, r9
	jmp	.Lmatch_512_tail
.Lmatch_512_aligned:
	/* Restore rdi and rsi after handling unaligned input. Number of handled
	 * unaligned bytes is reflected in r9. */
	pop	rsi
	pop	rdi
	add	rdi, r9
	sub	rsi, r9
	/* Unaligned input can only occur on the first iteration. Hence, rax
	 * reflects the number of matched bytes in the unaligned input. If rax
	 * is not equal to the number of unaligned bytes we have a partial
	 * match. */
	cmp	rax, r9
	jne	.Lmatch_512_done
	xor	r9, r9
	jmp	.Lmatch_512_loop
.Lmatch_512_done:
	ret
END(KS_str_match_native_512)

	.align 16

ENTRY(KS_str_match_until_native_128)
ctrl_mask = \
	(CTRL_SOURCE_U8 << CTRL_SOURCE_SHIFT) | \
	(CTRL_AGGREGATE_RANGES << CTRL_AGGREGATE_SHIFT) | \
	(CTRL_POLARITY_POSITIVE << CTRL_POLARITY_SHIFT) | \
	(CTRL_OUTPUT_INDEX_LSB << CTRL_OUTPUT_INDEX_SHIFT) | \
	(CTRL_OUTPUT_MASK_BITMASK << CTRL_OUTPUT_MASK_SHIFT)
	xor	eax, eax
	mov	r8, rsi
	movdqa	xmm1, [rdx + KS_STR_MATCH_U8]
	.align 16
.Luntil_128_loop:
	cmp	r8, 16
	jb	.Luntil_128_tail
	pcmpistri xmm1, [rdi + rax], ctrl_mask
	/* The carry flag will be set if there's a match. */
	jc	.Luntil_128_match
	add	rax, 16
	sub	r8, 16
	jmp	.Luntil_128_loop
.Luntil_128_match:
	/* At least one byte matched the ranges, the index of the first matching
	 * byte resides in rcx. */
	add	rax, rcx
	ret
.Luntil_128_tail:
	mov	rcx, r8
	copy_to_stack_128
	pcmpistri xmm1, [rdx], ctrl_mask
	/* The carry flag will be set if there's a match. */
	jc	.Luntil_128_match
	add	rax, r8
	ret
END(KS_str_match_until_native_128)

ENTRY(KS_str_match_until_native_256)
	xor	eax, eax
	xor	r9, r9
	vpxor	ymm0, ymm0, ymm0
	vpcmpeqd ymm1, ymm1, ymm1
	/* Load aligned constants. */
	vmovdqa	ymm4, [rdx + KS_STR_MATCH_U512]
	vmovdqa	ymm5, [rip + .Lmatch_bit_mask]
	vmovdqa	ymm6, [rip + .Lmatch_lo_mask]
	/* Save lower bound of the first range, used to enforce a match while
	 * handling unaligned/tail input. */
	mov	r10b, byte ptr [rdx]
	test	rdi, 0x1f
	jnz	.Luntil_256_unaligned
	.align 16
.Luntil_256_loop:
	cmp	rsi, 32
	jb	.Luntil_256_tail
.Luntil_256_match:
	/* Match low nibbles. */
	vpshufb	ymm2, ymm4, [rdi]
	/* Match high nibbles. As no byte granular vector shift instruction is
	 * available, operate on words (16 bits) which requires masking to get
	 * rid of spill. */
	vmovdqa	ymm3, [rdi]
	vpsraw	ymm3, ymm3, 4
	vpand	ymm3, ymm3, ymm6
	/* Transform high nibbles into bitmask. */
	vpshufb	ymm3, ymm5, ymm3
	vpand	ymm2, ymm2, ymm3
	vpcmpeqb ymm2, ymm2, ymm0
	/* Construct mask in which each bit represents a byte, a set bit implies
	 * that the corresponding byte is non-zero. */
	vpmovmskb edx, ymm2
	/* If 32 bits are set implies that 32 bytes worth of input did not
	 * match. */
	cmp	edx, -1
	jne	.Luntil_256_partial_match
	add	rax, 32
	add	rdi, 32
	sub	rsi, 32
	jmp	.Luntil_256_loop
.Luntil_256_partial_match:
	/* Only keep the lower bits when there's less than 32 bytes left. */
	cmp	rsi, 32
	ja	1f
	bzhi	edx, edx, esi
1:
	/* Invert match bitmask, number of consecutive zeros in the least
	 * significant bits represents the number of non-matched bytes. */
	not	edx
	tzcnt	edx, edx
	add 	rax, rdx
	test	r9, r9
	jnz	.Luntil_256_aligned
	ret
.Luntil_256_tail:
	copy_to_stack_256
	/* Set the first byte after the copied ones to the lower bound of the
	 * first range. This will ensure that we have match and thus terminate
	 * the main loop. */
	mov	byte ptr [rdi + rsi], r10b
	jmp	.Luntil_256_match
.Luntil_256_unaligned:
	/* Handle unaligned input by copying the bytes to an aligned stack
	 * buffer. Only relevant if there's 32 or more bytes left, otherwise
	 * just fallback to the tail logic. */
	cmp	rsi, 32
	jl	.Luntil_256_tail
	push	rdi
	push	rsi
	mov	r8, rdi
	and	r8, 0x1f
	mov	r9, 32
	sub	r9, r8
	mov	rsi, r9
	jmp	.Luntil_256_tail
.Luntil_256_aligned:
	/* Restore rdi and rsi after handling unaligned input. Number of handled
	 * unaligned bytes is reflected in r9. */
	pop	rsi
	pop	rdi
	add	rdi, r9
	sub	rsi, r9
	/* Unaligned input can only occur on the first iteration. Hence, rax
	 * reflects the number of non-matched bytes in the unaligned input. If
	 * rax is not equal to the number of unaligned bytes we have a partial
	 * match. */
	cmp	rax, r9
	jne	.Luntil_256_done
	xor	r9, r9
	jmp	.Luntil_256_loop
.Luntil_256_done:
	ret
END(KS_str_match_until_native_256)

	.data
	.align 64
.Lmatch_lo_mask:
	.quad 0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f
	.quad 0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f
	.quad 0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f
	.quad 0x0f0f0f0f0f0f0f0f, 0x0f0f0f0f0f0f0f0f
.Lmatch_bit_mask:
	.quad 0x8040201008040201, 0x8040201008040201
	.quad 0x8040201008040201, 0x8040201008040201
	.quad 0x8040201008040201, 0x8040201008040201
	.quad 0x8040201008040201, 0x8040201008040201

#endif
