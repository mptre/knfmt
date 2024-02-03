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

#include "libks/arena-buffer.h"

#include "libks/arena.h"
#include "libks/buffer.h"

static void	*callback_alloc(size_t, void *);
static void	*callback_realloc(void *, size_t, size_t, void *);
static void	 callback_free(void *, size_t, void *);

struct buffer *
arena_buffer_alloc(struct arena_scope *s, size_t init_size)
{
	return buffer_alloc_impl(init_size, &(struct buffer_callbacks){
	    .alloc	= callback_alloc,
	    .realloc	= callback_realloc,
	    .free	= callback_free,
	    .arg	= s,
	});
}

struct buffer *
arena_buffer_read(struct arena_scope *s, const char *path)
{
	struct buffer *bf;

	bf = arena_buffer_alloc(s, 1 << 13);
	if (buffer_read_impl(bf, path)) {
		buffer_free(bf);
		return NULL;
	}
	return bf;
}

struct buffer *
arena_buffer_read_fd(struct arena_scope *s, int fd)
{
	struct buffer *bf;

	bf = arena_buffer_alloc(s, 1 << 13);
	if (buffer_read_fd_impl(bf, fd)) {
		buffer_free(bf);
		return NULL;
	}
	return bf;
}

static void *
callback_alloc(size_t size, void *arg)
{
	struct arena_scope *s = arg;

	return arena_malloc(s, size);
}

static void *
callback_realloc(void *ptr, size_t old_size, size_t new_size, void *arg)
{
	struct arena_scope *s = arg;

	return arena_realloc(s, ptr, old_size, new_size);
}

static void
callback_free(void *ptr, size_t size, void *arg __attribute__((unused)))
{
	arena_poison(ptr, size);
}
