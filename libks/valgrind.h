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

#include <stdint.h>

/*
 * The gist of RUNNING_ON_VALGRIND from valgrind.h.
 */
static inline int
is_valgrind_running(void)
{
#if defined(__x86_64__)
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
#else
	return 0;
#endif
}

#endif /* !LIBKS_VALGRIND_H */
