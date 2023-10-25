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

#include <stddef.h>	/* size_t */

#define ARENA_FATAL		0x00000001u

#define arena_scope(arena, varname) \
	__attribute__((cleanup(arena_scope_leave))) \
	struct arena_scope varname = arena_scope_enter((arena))

struct arena_scope {
	struct arena		*arena;
	struct arena_frame	*frame;
	size_t			 frame_len;
};

struct arena_stats {
	struct {
		/* Effective amount of allocated bytes. */
		unsigned long	now;
		/* Total amount of allocated bytes. */
		unsigned long	total;
	} bytes;

	struct {
		/* Effective amount of allocated frames. */
		unsigned long	now;
		/* Total amount of allocated frames. */
		unsigned long	total;
	} frames;

	struct {
		/* Number of fast reallocations. */
		unsigned long	fast;
		/* Number of reallocations. */
		unsigned long	total;
		/* Number of bytes spilled while moving allocations. */
		unsigned long	spill;
	} realloc;

	/* Overflow scenario(s) hit during frame size calculation. */
	unsigned long	overflow;
};

struct arena	*arena_alloc(unsigned int);
void		 arena_free(struct arena *);

struct arena_scope	arena_scope_enter(struct arena *);
void			arena_scope_leave(struct arena_scope *);

void	*arena_malloc(struct arena_scope *, size_t)
	__attribute__((malloc, alloc_size(2)));
void	*arena_calloc(struct arena_scope *, size_t, size_t)
	__attribute__((malloc, alloc_size(2, 3)));
void	*arena_realloc(struct arena_scope *, void *, size_t, size_t)
	__attribute__((malloc, alloc_size(4)));

char	*arena_sprintf(struct arena_scope *, const char *, ...)
	__attribute__((__format__(printf, 2, 3)));

char	*arena_strdup(struct arena_scope *, const char *);
char	*arena_strndup(struct arena_scope *, const char *, size_t);

struct arena_stats	*arena_stats(struct arena *);

void    arena_poison(void *, size_t);
