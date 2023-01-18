#include "buffer.h"

#include "config.h"

#include <err.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	buffer_grow(struct buffer *, unsigned int);

struct buffer *
buffer_alloc(size_t sizhint)
{
	struct buffer *bf;
	char *ptr;
	size_t siz = 1;

	for (siz = 1; siz < sizhint; siz <<= 1)
		continue;

	ptr = malloc(siz);
	if (ptr == NULL)
		err(1, NULL);
	bf = calloc(1, sizeof(*bf));
	if (bf == NULL)
		err(1, NULL);
	bf->bf_ptr = ptr;
	bf->bf_siz = siz;

	return bf;
}

/*
 * Read the file located at path into a buffer.
 */
struct buffer *
buffer_read(const char *path)
{
	struct buffer *bf;
	int fd;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		warn("open: %s", path);
		return NULL;
	}

	bf = buffer_read_fd(fd);
	if (bf == NULL)
		warn("read: %s", path);
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
		if (bf->bf_len > bf->bf_siz / 2)
			buffer_grow(bf, 1);
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

void
buffer_puts(struct buffer *bf, const char *str, size_t len)
{
	if (bf->bf_len + len >= bf->bf_siz) {
		size_t siz = bf->bf_siz;
		unsigned int shift;

		for (shift = 1; bf->bf_len + len >= siz << shift; shift++)
			continue;
		buffer_grow(bf, shift);
	}

	memcpy(&bf->bf_ptr[bf->bf_len], str, len);
	bf->bf_len += len;
}

void
buffer_putc(struct buffer *bf, char ch)
{
	buffer_puts(bf, &ch, 1);
}

void
buffer_printf(struct buffer *bf, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	buffer_vprintf(bf, fmt, ap);
	va_end(ap);
}

void
buffer_vprintf(struct buffer *bf, const char *fmt, va_list ap)
{
	va_list cp;
	unsigned int shift = 0;
	int n;

	va_copy(cp, ap);

	n = vsnprintf(NULL, 0, fmt, ap);
	if (n < 0)
		err(1, "vsnprintf");

	while ((bf->bf_siz << shift) - bf->bf_len <= (size_t)n)
		shift++;
	if (shift > 0)
		buffer_grow(bf, shift);

	n = vsnprintf(&bf->bf_ptr[bf->bf_len], bf->bf_siz - bf->bf_len, fmt,
	    cp);
	va_end(cp);
	if (n < 0)
		err(1, "vsnprintf");
	bf->bf_len += (size_t)n;
}

size_t
buffer_indent(struct buffer *bf, size_t indent, int usetabs, size_t pos)
{
	if (usetabs) {
		for (; indent >= 8; indent -= 8) {
			buffer_putc(bf, '\t');
			pos += 8 - (pos % 8);
		}
	}
	for (; indent > 0; indent--) {
		buffer_putc(bf, ' ');
		pos++;
	}
	return pos;
}

/*
 * Release and take ownership of the underlying buffer.
 */
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

int
buffer_cmp(const struct buffer *b1, const struct buffer *b2)
{
	if (b1->bf_len != b2->bf_len)
		return 1;
	return memcmp(b1->bf_ptr, b2->bf_ptr, b1->bf_len);
}

static void
buffer_grow(struct buffer *bf, unsigned int shift)
{
	size_t newsiz;

	newsiz = bf->bf_siz << shift;
	bf->bf_ptr = realloc(bf->bf_ptr, newsiz);
	if (bf->bf_ptr == NULL)
		err(1, NULL);
	bf->bf_siz = newsiz;
}
