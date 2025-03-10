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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libks/compiler.h"

struct buffer {
	struct buffer_callbacks	 bf_callbacks;
	char			*bf_ptr;
	size_t			 bf_siz;
	size_t			 bf_len;
};

static int	buffer_reserve(struct buffer *, size_t, int);

static void	*callback_alloc(size_t, void *);
static void	*callback_realloc(void *, size_t, size_t, void *);
static void	 callback_free(void *, size_t, void *);

struct buffer *
buffer_alloc(size_t init_size)
{
	return buffer_alloc_impl(init_size, 1, &(struct buffer_callbacks){
	    .alloc	= callback_alloc,
	    .realloc	= callback_realloc,
	    .free	= callback_free,
	});
}

struct buffer *
buffer_alloc_impl(size_t init_size, int overshoot,
    const struct buffer_callbacks *callbacks)
{
	struct buffer *bf;

	bf = callbacks->alloc(sizeof(*bf), callbacks->arg);
	if (bf == NULL)
		return NULL;
	memset(bf, 0, sizeof(*bf));
	bf->bf_callbacks = *callbacks;
	if (buffer_reserve(bf, init_size, overshoot)) {
		buffer_free(bf);
		return NULL;
	}
	return bf;
}

static int
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

static size_t
estimate_size(int fd)
{
	struct stat sb;

	if (fstat(fd, &sb) == -1 || !S_ISREG(sb.st_mode) || sb.st_size <= 0)
		return 0;
	return (size_t)sb.st_size;
}

int
buffer_read_fd_impl(struct buffer *bf, int fd)
{
	int doreserve = estimate_size(fd) != bf->bf_siz - bf->bf_len;

	for (;;) {
		ssize_t n;

		n = read(fd, &bf->bf_ptr[bf->bf_len], bf->bf_siz - bf->bf_len);
		if (n == -1)
			return 1;
		if (n == 0)
			break;
		bf->bf_len += (size_t)n;
		if (doreserve && buffer_reserve(bf, bf->bf_siz / 8, 1))
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
	if (buffer_reserve(bf, len, 1))
		return -1;
	memcpy(&bf->bf_ptr[bf->bf_len], str, len);
	bf->bf_len += len;
	return (int)len;
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

static int
is_string_directive(const char *fmt)
{
	return fmt[0] == '%' && fmt[1] == 's' && fmt[2] == '\0';
}

int
buffer_vprintf(struct buffer *bf, const char *fmt, va_list ap)
{
	va_list cp;
	size_t len;
	int n;

	if (is_string_directive(fmt)) {
		const char *str;

		str = va_arg(ap, const char *);
		if (str == NULL)
			str = "(null)";
		return buffer_puts(bf, str, strlen(str));
	}

	va_copy(cp, ap);

	n = vsnprintf(NULL, 0, fmt, ap);
	if (n < 0 || buffer_reserve(bf, (size_t)n + 1, 1)) {
		va_end(cp);
		return -1;
	}

	len = bf->bf_siz - bf->bf_len;
	n = vsnprintf(&bf->bf_ptr[bf->bf_len], len, fmt, cp);
	va_end(cp);
	if (n < 0 || (size_t)n >= len)
		return -1;
	bf->bf_len += (size_t)n;

	return n;
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
	if (bf->bf_len == 0 || bf->bf_ptr[bf->bf_len - 1] != '\0') {
		if (buffer_putc(bf, '\0') == -1)
			return NULL;
	}
	return buffer_release(bf);
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

const char *
buffer_getline(const struct buffer *bf, struct buffer_getline *getline)
{
	if (getline->bf == NULL) {
		getline->bf = buffer_alloc(1 << 10);
		if (getline->bf == NULL)
			return NULL;
	}

	return buffer_getline_impl(bf, getline);
}

const char *
buffer_getline_impl(const struct buffer *bf, struct buffer_getline *getline)
{
	const char *line, *newline;
	size_t linelen;

	if (getline->off >= bf->bf_len)
		goto done;

	line = &bf->bf_ptr[getline->off];
	newline = memchr(line, '\n', bf->bf_len - getline->off);
	if (newline == NULL)
		newline = &bf->bf_ptr[bf->bf_len];
	linelen = (size_t)(newline - line);
	buffer_reset(getline->bf);
	buffer_puts(getline->bf, line, linelen);
	buffer_putc(getline->bf, '\0');
	getline->off += linelen + 1;
	return buffer_get_ptr(getline->bf);

done:
	buffer_getline_free(getline);
	return NULL;
}

void
buffer_getline_free(struct buffer_getline *getline)
{
	buffer_free(getline->bf);
	memset(getline, 0, sizeof(*getline));
}

static int
buffer_reserve(struct buffer *bf, size_t len, int overshoot)
{
	size_t newlen, newsiz;
	char *ptr;

	if (len > ULONG_MAX - bf->bf_len)
		goto overflow;
	newlen = bf->bf_len + len;
	if (bf->bf_siz > 0 && bf->bf_siz >= newlen)
		return 0;

	if (overshoot) {
		newsiz = bf->bf_siz ? bf->bf_siz : 16;
		while (newsiz < newlen) {
			if (newsiz > ULONG_MAX / 2)
				goto overflow;
			newsiz *= 2;
		}
	} else {
		newsiz = newlen;
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
callback_alloc(size_t size, void *UNUSED(arg))
{
	return malloc(size);
}

static void *
callback_realloc(void *ptr, size_t UNUSED(old_size), size_t new_size,
    void *UNUSED(arg))
{
	return realloc(ptr, new_size);
}

static void
callback_free(void *ptr, size_t UNUSED(size), void *UNUSED(arg))
{
	free(ptr);
}
