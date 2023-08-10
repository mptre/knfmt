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

#include "libks/arithmetic.h"

#include <limits.h>

#define SIGNED_ADD_OVERFLOW(bits, a, b, c) do {				\
	if (((b) > 0 && (a) > (INT ## bits ## _MAX - (b))) ||		\
	    ((b) < 0 && (a) < (INT ## bits ## _MIN - (b))))		\
		return 1;						\
	*(c) = (a) + (b);						\
	return 0;							\
} while (0)

#define SIGNED_SUB_OVERFLOW(bits, a, b, c) do {				\
	if (((b) > 0 && (a) < (INT ## bits ## _MIN + (b))) ||		\
	    ((b) < 0 && (a) > (INT ## bits ## _MAX + (b))))		\
		return 1;						\
	*(c) = (a) - (b);						\
	return 0;							\
} while (0)

#define SIGNED_MUL_OVERFLOW(bits, a, b, c) do {				\
	int ## bits ## _t _max = INT ## bits ## _MAX;			\
	int ## bits ## _t _min = INT ## bits ## _MIN;			\
	if ((a) > 0 && (b) > 0 && (a) > _max / (b))			\
		return 1;						\
	if ((a) > 0 && (b) < 0 && (b) < _min / (a))			\
		return 1;						\
	if ((a) < 0 && (b) > 0 && (a) < _min / (b))			\
		return 1;						\
	if ((a) < 0 && (b) < 0 && (b) < _max / (a))			\
		return 1;						\
	*(c) = (a) * (b);						\
	return 0;							\
} while (0)

#define UNSIGNED_ADD_OVERFLOW(bits, a, b, c) do {			\
	if ((a) > UINT ## bits ## _MAX - (b))				\
		return 1;						\
	*(c) = (a) + (b);						\
	return 0;							\
} while (0)

#define UNSIGNED_SUB_OVERFLOW(bits, a, b, c) do {			\
	if ((b) > (a))							\
		return 1;						\
	*(c) = (a) - (b);						\
	return 0;							\
} while (0)

#define UNSIGNED_MUL_OVERFLOW(bits, a, b, c) do {			\
	if ((a) > UINT ## bits ## _MAX / (b))				\
		return 1;						\
	*(c) = (a) * (b);						\
	return 0;							\
} while (0)

int
i32_add_overflow0(int32_t a, int32_t b, int32_t *c)
{
	SIGNED_ADD_OVERFLOW(32, a, b, c);
}

int
i32_sub_overflow0(int32_t a, int32_t b, int32_t *c)
{
	SIGNED_SUB_OVERFLOW(32, a, b, c);
}

int
i32_mul_overflow0(int32_t a, int32_t b, int32_t *c)
{
	SIGNED_MUL_OVERFLOW(32, a, b, c);
}

int
i64_add_overflow0(int64_t a, int64_t b, int64_t *c)
{
	SIGNED_ADD_OVERFLOW(64, a, b, c);
}

int
i64_sub_overflow0(int64_t a, int64_t b, int64_t *c)
{
	SIGNED_SUB_OVERFLOW(64, a, b, c);
}

int
i64_mul_overflow0(int64_t a, int64_t b, int64_t *c)
{
	SIGNED_MUL_OVERFLOW(64, a, b, c);
}

int
u32_add_overflow0(uint32_t a, uint32_t b, uint32_t *c)
{
	UNSIGNED_ADD_OVERFLOW(32, a, b, c);
}

int
u32_sub_overflow0(uint32_t a, uint32_t b, uint32_t *c)
{
	UNSIGNED_SUB_OVERFLOW(32, a, b, c);
}

int
u32_mul_overflow0(uint32_t a, uint32_t b, uint32_t *c)
{
	UNSIGNED_MUL_OVERFLOW(32, a, b, c);
}

int
u64_add_overflow0(uint64_t a, uint64_t b, uint64_t *c)
{
	UNSIGNED_ADD_OVERFLOW(64, a, b, c);
}

int
u64_sub_overflow0(uint64_t a, uint64_t b, uint64_t *c)
{
	UNSIGNED_SUB_OVERFLOW(64, a, b, c);
}

int
u64_mul_overflow0(uint64_t a, uint64_t b, uint64_t *c)
{
	UNSIGNED_MUL_OVERFLOW(64, a, b, c);
}
