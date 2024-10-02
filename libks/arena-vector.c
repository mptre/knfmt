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

#include "libks/arena-vector.h"

#include <err.h>

#include "libks/arena.h"
#include "libks/compiler.h"
#include "libks/vector.h"

static void *
callback_calloc(size_t nmemb, size_t size, void *arg)
{
	struct arena_scope *s = arg;

	return arena_calloc(s, nmemb, size);
}

static void *
callback_realloc(void *ptr, size_t oldsize, size_t newsize, void *arg)
{
	struct arena_scope *s = arg;

	return arena_realloc(s, ptr, oldsize, newsize);
}

static void
callback_free(void *ptr, size_t size, void *UNUSED(arg))
{
	arena_poison(ptr, size);
}

void
arena_vector_init(struct arena_scope *s, void **vv, size_t stride, size_t n)
{
	vector_init_impl(VECTOR_ARENA, vv, stride, &(struct vector_callbacks){
	    .calloc	= callback_calloc,
	    .realloc	= callback_realloc,
	    .free	= callback_free,
	    .arg	= s,
	});
	if (n > 0)
		vector_reserve(vv, n);
}

size_t
arena_vector_alloc(void **vv)
{
	if (vector_type(*vv) != VECTOR_ARENA) {
		errx(1, "expected arena vector");
		__builtin_unreachable();
	}
	return vector_alloc(vv, 0);
}

size_t
arena_vector_calloc(void **vv)
{
	if (vector_type(*vv) != VECTOR_ARENA) {
		errx(1, "expected arena vector");
		__builtin_unreachable();
	}
	return vector_alloc(vv, 1);
}
