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

#ifndef LIBKS_ASM_H
#define LIBKS_ASM_H

#if defined(__x86_64__)

#if defined(__linux__) && defined(__ELF__)
#  define ASM_PROLOGUE .section .note.GNU-stack,"",%progbits
#else
#  define ASM_PROLOGUE
#endif

#if defined(__CET__)
#  include <cet.h>
#else
#  define _CET_ENDBR
#endif

/* NOLINTBEGIN(bugprone-macro-parentheses) */
#define ENTRY(x)	.globl x; .text; .type x,@function; x: _CET_ENDBR
/* NOLINTEND(bugprone-macro-parentheses) */
#define END(x)		.size x, . - x

#endif

#endif /* !LIBKS_ASM_H */
