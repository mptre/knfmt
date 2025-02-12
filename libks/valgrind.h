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

#ifndef LIBKS_VALGRIND_H
#define LIBKS_VALGRIND_H

#if defined(__x86_64__) && !defined(NDEBUG)

#include <stddef.h>	/* size_t */
#include <stdint.h>

static int
KS_valgrind_request(const uint64_t *request)
{
	int d;

	__asm__ volatile (
	    "rolq $3, %%rdi\n"
	    "rolq $13, %%rdi\n"
	    "rolq $61, %%rdi\n"
	    "rolq $51, %%rdi\n"
	    "xchgq %%rbx, %%rbx\n"
	    : [res] "=d" (d)
	    : "a" (request), "[res]" (0)
	    : "cc", "memory");
	return d;
}

static inline int
KS_valgrind_is_running(void)
{
	static int is_running = -1;

	if (is_running == -1) {
		const uint64_t request[] = { 0x1001U };
		is_running = KS_valgrind_request(request);
	}
	return is_running;
}

static inline int
KS_valgrind_make_mem_noaccess(const void *ptr, size_t len)
{
	if (!KS_valgrind_is_running())
		return 0;

	const uint64_t request[] = { 0x4d430000U, (uintptr_t)ptr, len };
	return KS_valgrind_request(request) == -1;
}

static inline int
KS_valgrind_make_mem_undefined(const void *ptr, size_t len)
{
	if (!KS_valgrind_is_running())
		return 0;

	const uint64_t request[] = { 0x4d430001U, (uintptr_t)ptr, len };
	return KS_valgrind_request(request) == -1;
}

static inline int
KS_valgrind_make_mem_defined(const void *ptr, size_t len)
{
	if (!KS_valgrind_is_running())
		return 0;

	const uint64_t request[] = { 0x4d430002U, (uintptr_t)ptr, len };
	return KS_valgrind_request(request) == -1;
}

#else

#include "libks/compiler.h"

static inline int
KS_valgrind_is_running(void)
{
	return 0;
}

static inline int
KS_valgrind_make_mem_noaccess(const void *UNUSED(ptr), size_t UNUSED(len))
{
	return 0;
}

static inline int
KS_valgrind_make_mem_undefined(const void *UNUSED(ptr), size_t UNUSED(len))
{
	return 0;
}

static inline int
KS_valgrind_make_mem_defined(const void *UNUSED(ptr), size_t UNUSED(len))
{
	return 0;
}

#endif

#endif /* !LIBKS_VALGRIND_H */
