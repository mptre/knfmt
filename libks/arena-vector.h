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

#ifndef LIBKS_ARENA_VECTOR_H
#define LIBKS_ARENA_VECTOR_H

#include <stddef.h>	/* size_t */

struct arena_scope;

#define ARENA_VECTOR_INIT(s, vc, n) arena_vector_init(s, (void **)&(vc), sizeof(*(vc)), n)
void	arena_vector_init(struct arena_scope *, void **, size_t, size_t);

#define ARENA_VECTOR_ALLOC(vc) __extension__ ({				\
	size_t _i = arena_vector_alloc((void **)&(vc));			\
	(vc) + _i;							\
})
size_t	arena_vector_alloc(void **);

#define ARENA_VECTOR_CALLOC(vc) __extension__ ({			\
	size_t _i = arena_vector_calloc((void **)&(vc));		\
	(vc) + _i;							\
})
size_t	arena_vector_calloc(void **);

#endif /* !LIBKS_ARENA_VECTOR_H */
