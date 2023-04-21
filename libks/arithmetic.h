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

#include <stdint.h>

int	i32_add_overflow(int32_t, int32_t, int32_t *c);
int	i32_sub_overflow(int32_t, int32_t, int32_t *c);
int	i32_mul_overflow(int32_t, int32_t, int32_t *c);

int	i64_add_overflow(int64_t, int64_t, int64_t *c);
int	i64_sub_overflow(int64_t, int64_t, int64_t *c);
int	i64_mul_overflow(int64_t, int64_t, int64_t *c);

int	u32_add_overflow(uint32_t, uint32_t, uint32_t *c);
int	u32_sub_overflow(uint32_t, uint32_t, uint32_t *c);
int	u32_mul_overflow(uint32_t, uint32_t, uint32_t *c);

int	u64_add_overflow(uint64_t, uint64_t, uint64_t *c);
int	u64_sub_overflow(uint64_t, uint64_t, uint64_t *c);
int	u64_mul_overflow(uint64_t, uint64_t, uint64_t *c);
