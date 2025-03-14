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
#include <stdint.h>

#define arena_scope(arena, varname) \
	__attribute__((cleanup(arena_scope_leave))) \
	struct arena_scope varname = arena_scope_enter((arena))

struct arena_scope {
	struct arena		*arena;
	struct arena_frame	*frame;
	struct arena_cleanup	*cleanup;
	size_t			 frame_len;
	size_t			 bytes;
	size_t			 frames;
	size_t			 scopes;
	int			 id;
};

struct arena_trace_event {
	uintptr_t	arena;
#define ARENA_TRACE_STACK_TRACE_DEPTH 10
	uintptr_t	stack_trace[ARENA_TRACE_STACK_TRACE_DEPTH];

	union {
		struct {
			size_t	size;
			size_t	alignment_spill;
		} push;

		struct {
			size_t	size;
		} frame_spill;

		struct {
			size_t	size;
		} realloc_spill;

		struct {
			size_t	max;
			size_t	total;
		} stats;
	} data;

	enum {
		ARENA_TRACE_PUSH,
		ARENA_TRACE_FRAME_SPILL,
		ARENA_TRACE_REALLOC_SPILL,
		ARENA_TRACE_STATS_BYTES,
		ARENA_TRACE_STATS_FRAMES,
		ARENA_TRACE_STATS_SCOPES,
	} type;
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

size_t	arena_capacity(const struct arena_scope *);
void	arena_poison(const void *, size_t);

#endif /* !LIBKS_ARENA_H */
