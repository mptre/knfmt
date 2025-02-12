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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/compiler.h"

static void	*callback_alloc(size_t, void *);
static void	*callback_realloc(void *, size_t, size_t, void *);
static void	 callback_free(void *, size_t, void *);

static struct buffer_callbacks *
init_callbacks(struct buffer_callbacks *callbacks, struct arena_scope *s)
{
	*callbacks = (struct buffer_callbacks){
	    .alloc	= callback_alloc,
	    .realloc	= callback_realloc,
	    .free	= callback_free,
	    .arg	= s,
	};
	return callbacks;
}

struct buffer *
arena_buffer_alloc(struct arena_scope *s, size_t init_size)
{
	struct buffer_callbacks callbacks;

	return buffer_alloc_impl(init_size, 1, init_callbacks(&callbacks, s));
}

static int
estimate_size(int fd, size_t *size)
{
	struct stat sb;

	if (fstat(fd, &sb) == -1 || !S_ISREG(sb.st_mode) || sb.st_size <= 0)
		return 0;
	*size = (size_t)sb.st_size;
	return 1;
}

struct buffer *
arena_buffer_read(struct arena_scope *s, const char *path)
{
	struct buffer_callbacks callbacks;
	struct buffer *bf;
	size_t init_size = 1 << 10;
	int overshoot = 1;
	int errno_save, error, fd;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return NULL;

	if (estimate_size(fd, &init_size))
		overshoot = 0;
	bf = buffer_alloc_impl(init_size, overshoot,
	    init_callbacks(&callbacks, s));
	error = buffer_read_fd_impl(bf, fd);
	errno_save = errno;
	close(fd);
	if (error) {
		buffer_free(bf);
		errno = errno_save;
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

const char *
arena_buffer_getline(struct arena_scope *s, const struct buffer *bf,
    struct buffer_getline *getline)
{
	if (getline->bf == NULL)
		getline->bf = arena_buffer_alloc(s, 1 << 10);

	return buffer_getline_impl(bf, getline);
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
callback_free(void *ptr, size_t size, void *UNUSED(arg))
{
	arena_poison(ptr, size);
}
