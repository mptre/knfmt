/*
 * Copyright (c) 2023 Anton Lindqvist <anton@basename.se>
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

#ifndef LIBKS_CONSISTENCY_H
#define LIBKS_CONSISTENCY_H

#ifndef NDEBUG

#define ASSERT_CONSISTENCY(a, b) do {					\
	if (assert_consistency(!!(a) == !!(b), #a, #b,			\
	    __func__, __LINE__))					\
		__builtin_trap();					\
} while (0)

int	assert_consistency(int, const char *, const char *, const char *, int);

#else

#define ASSERT_CONSISTENCY(a, b) ((void)0)

#endif

#endif /* !LIBKS_CONSISTENCY_H */
