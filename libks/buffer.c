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

#include "buffer.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct buffer {
	char	*bf_ptr;
	size_t	 bf_siz;
	size_t	 bf_len;
};

static int	buffer_reserve(struct buffer *, size_t);

struct buffer *
buffer_alloc(size_t sizhint)
{
	struct buffer *bf;

	bf = calloc(1, sizeof(*bf));
	if (bf == NULL)
		return NULL;
	if (buffer_reserve(bf, sizhint)) {
		buffer_free(bf);
		return NULL;
	}
	return bf;
}

struct buffer *
buffer_read(const char *path)
{
	struct buffer *bf;
	int fd;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return NULL;

	bf = buffer_read_fd(fd);
	close(fd);
	return bf;
}

struct buffer *
buffer_read_fd(int fd)
{
	struct buffer *bf;

	bf = buffer_alloc(1 << 13);
	if (bf == NULL)
		goto err;

	for (;;) {
		ssize_t n;

		n = read(fd, &bf->bf_ptr[bf->bf_len], bf->bf_siz - bf->bf_len);
		if (n == -1)
			goto err;
		if (n == 0)
			break;
		bf->bf_len += (size_t)n;
		if (buffer_reserve(bf, bf->bf_siz / 2))
			goto err;
	}
	return bf;

err:
	buffer_free(bf);
	return NULL;
}

void
buffer_free(struct buffer *bf)
{
	if (bf == NULL)
		return;

	free(bf->bf_ptr);
	free(bf);
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
buffer_cmp(const struct buffer *b1, const struct buffer *b2)
{
	if (b1->bf_len != b2->bf_len)
		return 1;
	return memcmp(b1->bf_ptr, b2->bf_ptr, b1->bf_len);
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
	ptr = realloc(bf->bf_ptr, newsiz);
	if (ptr == NULL)
		return 1;
	bf->bf_ptr = ptr;
	bf->bf_siz = newsiz;
	return 0;

overflow:
	errno = EOVERFLOW;
	return 1;
}
