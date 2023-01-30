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

static int	buffer_grow(struct buffer *, unsigned int);

struct buffer *
buffer_alloc(size_t sizhint)
{
	struct buffer *bf;
	char *ptr;
	size_t siz = 1;

	while (siz < sizhint)
		siz <<= 1;

	ptr = malloc(siz);
	if (ptr == NULL)
		return NULL;
	bf = calloc(1, sizeof(*bf));
	if (bf == NULL) {
		free(ptr);
		return NULL;
	}
	bf->bf_ptr = ptr;
	bf->bf_siz = siz;

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
	for (;;) {
		ssize_t n;

		n = read(fd, &bf->bf_ptr[bf->bf_len], bf->bf_siz - bf->bf_len);
		if (n == -1)
			goto err;
		if (n == 0)
			break;
		bf->bf_len += (size_t)n;
		if (bf->bf_len > bf->bf_siz / 2 && buffer_grow(bf, 1))
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
	if (len > ULONG_MAX - bf->bf_len)
		goto overflow;
	if (bf->bf_len + len >= bf->bf_siz) {
		size_t siz = bf->bf_siz;
		unsigned int shift = 1;

		for (;;) {
			if (siz > ULONG_MAX / 2)
				goto overflow;
			siz *= 2;
			if (siz > bf->bf_len + len)
				break;
			shift++;
		}
		if (buffer_grow(bf, shift))
			return 1;
	}

	memcpy(&bf->bf_ptr[bf->bf_len], str, len);
	bf->bf_len += len;
	return 0;

overflow:
	errno = EOVERFLOW;
	return 1;
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
	size_t len, siz;
	unsigned int shift = 0;
	int error = 0;
	int n;

	va_copy(cp, ap);

	n = vsnprintf(NULL, 0, fmt, ap);
	if (n < 0) {
		error = 1;
		goto out;
	}

	siz = bf->bf_siz;
	for (;;) {
		if (siz > bf->bf_len + (size_t)n)
			break;
		if (siz > ULONG_MAX / 2)
			goto overflow;
		siz *= 2;
		shift++;
	}
	if (shift > 0 && buffer_grow(bf, shift)) {
		error = 1;
		goto out;
	}

	len = bf->bf_siz - bf->bf_len;
	n = vsnprintf(&bf->bf_ptr[bf->bf_len], len, fmt, cp);
	va_end(cp);
	if (n < 0 || (size_t)n >= len) {
		error = 1;
		goto out;
	}
	bf->bf_len += (size_t)n;

out:
	va_end(cp);
	return error;

overflow:
	errno = EOVERFLOW;
	return 1;
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
buffer_grow(struct buffer *bf, unsigned int shift)
{
	char *ptr;
	size_t nbits = sizeof(size_t) * 8;
	size_t newsiz;

	if (shift >= nbits || bf->bf_siz > ULONG_MAX / (1ULL << shift))
		goto overflow;
	newsiz = bf->bf_siz << shift;
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
