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

#ifndef LIBKS_BUFFER_H
#define LIBKS_BUFFER_H

#include <stdarg.h>	/* va_list */
#include <stddef.h>	/* size_t */

struct buffer;

struct buffer_callbacks {
	void	*(*alloc)(size_t, void *);
	void	*(*realloc)(void *, size_t, size_t, void *);
	void	 (*free)(void *, size_t, void *);
	void	*arg;
};

struct buffer_getline {
	struct buffer	*bf;
	size_t		 off;
};

struct buffer	*buffer_alloc(size_t);
struct buffer	*buffer_alloc_impl(size_t, struct buffer_callbacks *);
void		 buffer_free(struct buffer *);

struct buffer	*buffer_read(const char *);
int		 buffer_read_impl(struct buffer *, const char *);
struct buffer	*buffer_read_fd(int);
int		 buffer_read_fd_impl(struct buffer *, int);

char	*buffer_release(struct buffer *);
char	*buffer_str(struct buffer *);
void	 buffer_reset(struct buffer *);

size_t	buffer_pop(struct buffer *, size_t);

int	buffer_cmp(const struct buffer *, const struct buffer *);

int	buffer_puts(struct buffer *, const char *, size_t);
int	buffer_putc(struct buffer *, char);
int	buffer_printf(struct buffer *, const char *, ...)
	__attribute__((__format__(printf, 2, 3)));
int	buffer_vprintf(struct buffer *, const char *, va_list);

const char	*buffer_getline(const struct buffer *,
    struct buffer_getline *);
const char	*buffer_getline_impl(const struct buffer *,
    struct buffer_getline *);
void		 buffer_getline_free(struct buffer_getline *);

const char	*buffer_get_ptr(const struct buffer *);
size_t		 buffer_get_len(const struct buffer *);
size_t		 buffer_get_size(const struct buffer *);

#endif /* !LIBKS_BUFFER_H */
