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

#define SIGNED_ADD_OVERFLOW(max, min, a, b, c) do {			\
	if (((b) > 0 && (a) > ((max) - (b))) ||				\
	    ((b) < 0 && (a) < ((min) - (b))))				\
		return 1;						\
	*(c) = (a) + (b);						\
	return 0;							\
} while (0)

#define SIGNED_SUB_OVERFLOW(max, min, a, b, c) do {			\
	if (((b) > 0 && (a) < ((min) + (b))) ||				\
	    ((b) < 0 && (a) > ((max) + (b))))				\
		return 1;						\
	*(c) = (a) - (b);						\
	return 0;							\
} while (0)

#define SIGNED_MUL_OVERFLOW(max, min, a, b, c) do {			\
	if ((a) > 0 && (b) > 0 && (a) > (max) / (b))			\
		return 1;						\
	if ((a) > 0 && (b) < 0 && (b) < (min) / (a))			\
		return 1;						\
	if ((a) < 0 && (b) > 0 && (a) < (min) / (b))			\
		return 1;						\
	if ((a) < 0 && (b) < 0 && (b) < (max) / (a))			\
		return 1;						\
	*(c) = (a) * (b);						\
	return 0;							\
} while (0)

#define UNSIGNED_ADD_OVERFLOW(max, a, b, c) do {			\
	if ((a) > (max) - (b))						\
		return 1;						\
	*(c) = (a) + (b);						\
	return 0;							\
} while (0)

#define UNSIGNED_SUB_OVERFLOW(max, a, b, c) do {			\
	if ((b) > (a))							\
		return 1;						\
	*(c) = (a) - (b);						\
	return 0;							\
} while (0)

#define UNSIGNED_MUL_OVERFLOW(max, a, b, c) do {			\
	if ((a) > (max) / (b))						\
		return 1;						\
	*(c) = (a) * (b);						\
	return 0;							\
} while (0)

int
KS_i32_add_overflow0(int32_t a, int32_t b, int32_t *c)
{
	SIGNED_ADD_OVERFLOW(INT32_MAX, INT32_MIN, a, b, c);
}

int
KS_i32_sub_overflow0(int32_t a, int32_t b, int32_t *c)
{
	SIGNED_SUB_OVERFLOW(INT32_MAX, INT32_MIN, a, b, c);
}

int
KS_i32_mul_overflow0(int32_t a, int32_t b, int32_t *c)
{
	SIGNED_MUL_OVERFLOW(INT32_MAX, INT32_MIN, a, b, c);
}

int
KS_i64_add_overflow0(int64_t a, int64_t b, int64_t *c)
{
	SIGNED_ADD_OVERFLOW(INT64_MAX, INT64_MIN, a, b, c);
}

int
KS_i64_sub_overflow0(int64_t a, int64_t b, int64_t *c)
{
	SIGNED_SUB_OVERFLOW(INT64_MAX, INT64_MIN, a, b, c);
}

int
KS_i64_mul_overflow0(int64_t a, int64_t b, int64_t *c)
{
	SIGNED_MUL_OVERFLOW(INT64_MAX, INT64_MIN, a, b, c);
}

int
KS_u32_add_overflow0(uint32_t a, uint32_t b, uint32_t *c)
{
	UNSIGNED_ADD_OVERFLOW(UINT32_MAX, a, b, c);
}

int
KS_u32_sub_overflow0(uint32_t a, uint32_t b, uint32_t *c)
{
	UNSIGNED_SUB_OVERFLOW(UINT32_MAX, a, b, c);
}

int
KS_u32_mul_overflow0(uint32_t a, uint32_t b, uint32_t *c)
{
	UNSIGNED_MUL_OVERFLOW(UINT32_MAX, a, b, c);
}

int
KS_u64_add_overflow0(uint64_t a, uint64_t b, uint64_t *c)
{
	UNSIGNED_ADD_OVERFLOW(UINT64_MAX, a, b, c);
}

int
KS_u64_sub_overflow0(uint64_t a, uint64_t b, uint64_t *c)
{
	UNSIGNED_SUB_OVERFLOW(UINT64_MAX, a, b, c);
}

int
KS_u64_mul_overflow0(uint64_t a, uint64_t b, uint64_t *c)
{
	UNSIGNED_MUL_OVERFLOW(UINT64_MAX, a, b, c);
}

int
KS_size_add_overflow0(size_t a, size_t b, size_t *c)
{
	UNSIGNED_ADD_OVERFLOW(SIZE_MAX, a, b, c);
}

int
KS_size_sub_overflow0(size_t a, size_t b, size_t *c)
{
	UNSIGNED_SUB_OVERFLOW(SIZE_MAX, a, b, c);
}

int
KS_size_mul_overflow0(size_t a, size_t b, size_t *c)
{
	UNSIGNED_MUL_OVERFLOW(SIZE_MAX, a, b, c);
}
