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

#ifndef LIBKS_ARENA_H
#define LIBKS_ARENA_H

#include <stddef.h>	/* size_t */

#define arena_scope(arena, varname) \
	__attribute__((cleanup(arena_scope_leave))) \
	struct arena_scope varname = arena_scope_enter((arena))

struct arena_scope {
	struct arena		*arena;
	struct arena_frame	*frame;
	struct arena_cleanup	*cleanup;
	size_t			 frame_len;
	unsigned long		 bytes;
	unsigned long		 frames;
	unsigned long		 scopes;
	unsigned long		 alignment;
	int			 id;
};

struct arena_stats {
	struct {
		/* Effective amount of allocated bytes. */
		unsigned long	now;
		/* Total amount of allocated bytes. */
		unsigned long	total;
		/* Peek amount of effective allocated bytes. */
		unsigned long	max;
	} bytes;

	struct {
		/* Effective amount of allocated frames. */
		unsigned long	now;
		/* Total amount of allocated frames. */
		unsigned long	total;
		/* Peek amount of effective allocated frames. */
		unsigned long	max;
	} frames;

	struct {
		/* Effective amount of scopes. */
		unsigned long	now;
		/* Total amount of scopes. */
		unsigned long	total;
		/* Peek amount of effective scopes. */
		unsigned long	max;
	} scopes;

	struct {
		/* Total amount of registered cleanups. */
		unsigned long	total;
	} cleanup;

	struct {
		/* Effective amount of alignment. */
		unsigned long	now;
		/* Total amount of allocated alignment. */
		unsigned long	total;
		/* Peek amount of effective allocated alignment. */
		unsigned long	max;
	} alignment;

	struct {
		/* Number of fast reallocations. */
		unsigned long	fast;
		/* Number of zero sized reallocations. */
		unsigned long	zero;
		/* Number of reallocations. */
		unsigned long	total;
		/* Number of bytes spilled while moving allocations. */
		unsigned long	spill;
	} realloc;
};

struct arena	*arena_alloc(void);
void		 arena_free(struct arena *);

#define arena_scope_enter(a) \
	arena_scope_enter_impl((a), __func__, __LINE__)
struct arena_scope	arena_scope_enter_impl(struct arena *, const char *,
    int);

void			arena_scope_leave(struct arena_scope *);

void	*arena_malloc(struct arena_scope *, size_t)
	__attribute__((malloc, alloc_size(2), returns_nonnull));
void	*arena_calloc(struct arena_scope *, size_t, size_t)
	__attribute__((malloc, alloc_size(2, 3), returns_nonnull));
void	*arena_realloc(struct arena_scope *, void *, size_t, size_t)
	__attribute__((malloc, alloc_size(4), returns_nonnull));

char	*arena_sprintf(struct arena_scope *, const char *, ...)
	__attribute__((format(printf, 2, 3), returns_nonnull));

char	*arena_strdup(struct arena_scope *, const char *)
	__attribute__((returns_nonnull));
char	*arena_strndup(struct arena_scope *, const char *, size_t)
	__attribute__((returns_nonnull));

void    arena_cleanup(struct arena_scope *, void (*)(void *), void *);

struct arena_stats	*arena_stats(struct arena *);

void    arena_poison(const void *, size_t);

#endif /* !LIBKS_ARENA_H */
