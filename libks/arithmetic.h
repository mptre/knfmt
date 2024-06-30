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

#ifndef LIBKS_ARITHMETIC_H
#define LIBKS_ARITHMETIC_H

#include <stddef.h> /* size_t */
#include <stdint.h>

int	KS_i32_add_overflow0(int32_t, int32_t, int32_t *);
int	KS_i32_sub_overflow0(int32_t, int32_t, int32_t *);
int	KS_i32_mul_overflow0(int32_t, int32_t, int32_t *);

int	KS_i64_add_overflow0(int64_t, int64_t, int64_t *);
int	KS_i64_sub_overflow0(int64_t, int64_t, int64_t *);
int	KS_i64_mul_overflow0(int64_t, int64_t, int64_t *);

int	KS_u32_add_overflow0(uint32_t, uint32_t, uint32_t *);
int	KS_u32_sub_overflow0(uint32_t, uint32_t, uint32_t *);
int	KS_u32_mul_overflow0(uint32_t, uint32_t, uint32_t *);

int	KS_u64_add_overflow0(uint64_t, uint64_t, uint64_t *);
int	KS_u64_sub_overflow0(uint64_t, uint64_t, uint64_t *);
int	KS_u64_mul_overflow0(uint64_t, uint64_t, uint64_t *);

int	KS_size_add_overflow0(size_t, size_t, size_t *);
int	KS_size_sub_overflow0(size_t, size_t, size_t *);
int	KS_size_mul_overflow0(size_t, size_t, size_t *);

#if defined(__has_builtin)
#define has_builtin(x) __has_builtin(x)
#else
#define has_builtin(x) 0
#endif

static inline int
KS_i32_add_overflow(int32_t a, int32_t b, int32_t *c)
{
#if has_builtin(__builtin_add_overflow)
	return __builtin_add_overflow(a, b, c) ? 1 : 0;
#else
	return KS_i32_add_overflow0(a, b, c);
#endif
}

static inline int
KS_i32_sub_overflow(int32_t a, int32_t b, int32_t *c)
{
#if has_builtin(__builtin_sub_overflow)
	return __builtin_sub_overflow(a, b, c) ? 1 : 0;
#else
	return KS_i32_sub_overflow0(a, b, c);
#endif
}

static inline int
KS_i32_mul_overflow(int32_t a, int32_t b, int32_t *c)
{
#if has_builtin(__builtin_mul_overflow)
	return __builtin_mul_overflow(a, b, c) ? 1 : 0;
#else
	return KS_i32_mul_overflow0(a, b, c);
#endif
}

static inline int
KS_i64_add_overflow(int64_t a, int64_t b, int64_t *c)
{
#if has_builtin(__builtin_add_overflow)
	return __builtin_add_overflow(a, b, c) ? 1 : 0;
#else
	return KS_i64_add_overflow0(a, b, c);
#endif
}

static inline int
KS_i64_sub_overflow(int64_t a, int64_t b, int64_t *c)
{
#if has_builtin(__builtin_sub_overflow)
	return __builtin_sub_overflow(a, b, c) ? 1 : 0;
#else
	return KS_i64_sub_overflow0(a, b, c);
#endif
}

static inline int
KS_i64_mul_overflow(int64_t a, int64_t b, int64_t *c)
{
#if has_builtin(__builtin_mul_overflow)
	return __builtin_mul_overflow(a, b, c) ? 1 : 0;
#else
	return KS_i64_mul_overflow0(a, b, c);
#endif
}

static inline int
KS_u32_add_overflow(uint32_t a, uint32_t b, uint32_t *c)
{
#if has_builtin(__builtin_add_overflow)
	return __builtin_add_overflow(a, b, c) ? 1 : 0;
#else
	return KS_u32_add_overflow0(a, b, c);
#endif
}

static inline int
KS_u32_sub_overflow(uint32_t a, uint32_t b, uint32_t *c)
{
#if has_builtin(__builtin_sub_overflow)
	return __builtin_sub_overflow(a, b, c) ? 1 : 0;
#else
	return KS_u32_sub_overflow0(a, b, c);
#endif
}

static inline int
KS_u32_mul_overflow(uint32_t a, uint32_t b, uint32_t *c)
{
#if has_builtin(__builtin_mul_overflow)
	return __builtin_mul_overflow(a, b, c) ? 1 : 0;
#else
	return KS_u32_mul_overflow0(a, b, c);
#endif
}

static inline int
KS_u64_add_overflow(uint64_t a, uint64_t b, uint64_t *c)
{
#if has_builtin(__builtin_add_overflow)
	return __builtin_add_overflow(a, b, c) ? 1 : 0;
#else
	return KS_u64_add_overflow0(a, b, c);
#endif
}

static inline int
KS_u64_sub_overflow(uint64_t a, uint64_t b, uint64_t *c)
{
#if has_builtin(__builtin_sub_overflow)
	return __builtin_sub_overflow(a, b, c) ? 1 : 0;
#else
	return KS_u64_sub_overflow0(a, b, c);
#endif
}

static inline int
KS_u64_mul_overflow(uint64_t a, uint64_t b, uint64_t *c)
{
#if has_builtin(__builtin_mul_overflow)
	return __builtin_mul_overflow(a, b, c) ? 1 : 0;
#else
	return KS_u64_mul_overflow0(a, b, c);
#endif
}

static inline int
KS_size_add_overflow(size_t a, size_t b, size_t *c)
{
#if has_builtin(__builtin_add_overflow)
	return __builtin_add_overflow(a, b, c) ? 1 : 0;
#else
	return KS_size_add_overflow0(a, b, c);
#endif
}

static inline int
KS_size_sub_overflow(size_t a, size_t b, size_t *c)
{
#if has_builtin(__builtin_sub_overflow)
	return __builtin_sub_overflow(a, b, c) ? 1 : 0;
#else
	return KS_size_sub_overflow0(a, b, c);
#endif
}

static inline int
KS_size_mul_overflow(size_t a, size_t b, size_t *c)
{
#if has_builtin(__builtin_mul_overflow)
	return __builtin_mul_overflow(a, b, c) ? 1 : 0;
#else
	return KS_size_mul_overflow0(a, b, c);
#endif
}

#undef has_builtin

#endif /* !LIBKS_ARITHMETIC_H */
