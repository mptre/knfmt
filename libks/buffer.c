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

#include "libks/buffer.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct buffer {
	struct buffer_callbacks	 bf_callbacks;
	char			*bf_ptr;
	size_t			 bf_siz;
	size_t			 bf_len;
};

static int	buffer_reserve(struct buffer *, size_t);

static void	*callback_alloc(size_t, void *);
static void	*callback_realloc(void *, size_t, size_t, void *);
static void	 callback_free(void *, size_t, void *);

struct buffer *
buffer_alloc(size_t init_size)
{
	return buffer_alloc_impl(init_size, &(struct buffer_callbacks){
	    .alloc	= callback_alloc,
	    .realloc	= callback_realloc,
	    .free	= callback_free,
	});
}

struct buffer *
buffer_alloc_impl(size_t init_size, struct buffer_callbacks *callbacks)
{
	struct buffer *bf;

	bf = callbacks->alloc(sizeof(*bf), callbacks->arg);
	if (bf == NULL)
		return NULL;
	memset(bf, 0, sizeof(*bf));
	bf->bf_callbacks = *callbacks;
	if (buffer_reserve(bf, init_size)) {
		buffer_free(bf);
		return NULL;
	}
	return bf;
}

struct buffer *
buffer_read(const char *path)
{
	struct buffer *bf;

	bf = buffer_alloc(1 << 13);
	if (bf == NULL)
		return NULL;
	if (buffer_read_impl(bf, path)) {
		buffer_free(bf);
		return NULL;
	}
	return bf;
}

int
buffer_read_impl(struct buffer *bf, const char *path)
{
	int error, fd;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return 1;
	error = buffer_read_fd_impl(bf, fd);
	close(fd);
	return error;
}

struct buffer *
buffer_read_fd(int fd)
{
	struct buffer *bf;

	bf = buffer_alloc(1 << 13);
	if (bf == NULL)
		return NULL;
	if (buffer_read_fd_impl(bf, fd)) {
		buffer_free(bf);
		return NULL;
	}
	return bf;
}

int
buffer_read_fd_impl(struct buffer *bf, int fd)
{
	for (;;) {
		ssize_t n;

		n = read(fd, &bf->bf_ptr[bf->bf_len], bf->bf_siz - bf->bf_len);
		if (n == -1)
			return 1;
		if (n == 0)
			break;
		bf->bf_len += (size_t)n;
		if (buffer_reserve(bf, bf->bf_siz / 2))
			return 1;
	}

	return 0;
}

void
buffer_free(struct buffer *bf)
{
	if (bf == NULL)
		return;
	bf->bf_callbacks.free(bf->bf_ptr, bf->bf_siz, bf->bf_callbacks.arg);
	bf->bf_callbacks.free(bf, sizeof(*bf), bf->bf_callbacks.arg);
}

int
buffer_puts(struct buffer *bf, const char *str, size_t len)
{
	if (str == NULL || len == 0)
		return 0;
	if (buffer_reserve(bf, len))
		return 1;
	memcpy(&bf->bf_ptr[bf->bf_len], str, len);
	bf->bf_len += len;
	return 0;
}

int
buffer_putc(struct buffer *bf, char ch)
{
	return buffer_puts(bf, &ch, 1);
}

int
buffer_printf(struct buffer *bf, const char *fmt, ...)
{
	va_list ap;
	int error;

	va_start(ap, fmt);
	error = buffer_vprintf(bf, fmt, ap);
	va_end(ap);
	return error;
}

int
buffer_vprintf(struct buffer *bf, const char *fmt, va_list ap)
{
	va_list cp;
	size_t len;
	int error = 0;
	int n;

	va_copy(cp, ap);

	n = vsnprintf(NULL, 0, fmt, ap);
	if (n < 0 || buffer_reserve(bf, (size_t)n + 1)) {
		va_end(cp);
		return 1;
	}

	len = bf->bf_siz - bf->bf_len;
	n = vsnprintf(&bf->bf_ptr[bf->bf_len], len, fmt, cp);
	va_end(cp);
	if (n < 0 || (size_t)n >= len)
		return 1;
	bf->bf_len += (size_t)n;

	return error;
}

char *
buffer_release(struct buffer *bf)
{
	char *ptr;

	ptr = bf->bf_ptr;
	bf->bf_ptr = NULL;
	bf->bf_siz = 0;
	bf->bf_len = 0;
	return ptr;
}

char *
buffer_str(struct buffer *bf)
{
#ifdef __COVERITY__
	/*
	 * Coverity cannot deduce that the returned string is always
	 * NUL-terminated below.
	 */
	char *str;

	str = strndup(bf->bf_ptr, bf->bf_len);
	buffer_reset(bf);
	return str;
#else
	if (bf->bf_len == 0 || bf->bf_ptr[bf->bf_len - 1] != '\0') {
		if (buffer_putc(bf, '\0'))
			return NULL;
	}
	return buffer_release(bf);
#endif
}

void
buffer_reset(struct buffer *bf)
{
	bf->bf_len = 0;
}

size_t
buffer_pop(struct buffer *bf, size_t n)
{
	if (n > bf->bf_len)
		n = bf->bf_len;
	bf->bf_len -= n;
	return n;
}

int
buffer_cmp(const struct buffer *a, const struct buffer *b)
{
	if (a->bf_len != b->bf_len)
		return 1;
	if (a->bf_len == 0)
		return 0;
	return memcmp(a->bf_ptr, b->bf_ptr, a->bf_len);
}

const char *
buffer_get_ptr(const struct buffer *bf)
{
	return bf->bf_ptr;
}

size_t
buffer_get_len(const struct buffer *bf)
{
	return bf->bf_len;
}

size_t
buffer_get_size(const struct buffer *bf)
{
	return bf->bf_siz;
}

static int
buffer_reserve(struct buffer *bf, size_t len)
{
	size_t newlen, newsiz;
	char *ptr;

	if (len > ULONG_MAX - bf->bf_len)
		goto overflow;
	newlen = bf->bf_len + len;
	if (bf->bf_siz >= newlen)
		return 0;

	newsiz = bf->bf_siz ? bf->bf_siz : 16;
	while (newsiz < newlen) {
		if (newsiz > ULONG_MAX / 2)
			goto overflow;
		newsiz *= 2;
	}
	ptr = bf->bf_callbacks.realloc(bf->bf_ptr, bf->bf_siz, newsiz,
	    bf->bf_callbacks.arg);
	if (ptr == NULL)
		return 1;
	bf->bf_ptr = ptr;
	bf->bf_siz = newsiz;
	return 0;

overflow:
	errno = EOVERFLOW;
	return 1;
}

static void *
callback_alloc(size_t size, void *arg __attribute__((unused)))
{
	return malloc(size);
}

static void *
callback_realloc(void *ptr, size_t old_size __attribute__((unused)),
    size_t new_size, void *arg __attribute__((unused)))
{
	return realloc(ptr, new_size);
}

static void
callback_free(void *ptr, size_t size __attribute__((unused)),
    void *arg __attribute__((unused)))
{
	free(ptr);
}
