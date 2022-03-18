#include <err.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

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

	bf = buffer_alloc(1024);

	for (;;) {
		ssize_t n;

		n = read(fd, &bf->bf_ptr[bf->bf_len], bf->bf_siz - bf->bf_len);
		if (n == -1) {
			warn("read: %s", path);
			goto err;
		}
		if (n == 0)
			break;
		bf->bf_len += n;
		if (bf->bf_len > bf->bf_siz / 2)
			buffer_grow(bf, 1);
	}

	close(fd);
	return bf;

err:
	close(fd);
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
buffer_append(struct buffer *bf, const char *str, size_t len)
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
buffer_appendc(struct buffer *bf, char ch)
{
	buffer_append(bf, &ch, 1);
}

void
buffer_appendv(struct buffer *bf, const char *fmt, ...)
{
	va_list ap, cp;
	unsigned int shift = 0;
	int n;

	va_start(ap, fmt);
	va_copy(cp, ap);

	n = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	if (n < 0)
		err(1, "vsnprintf");

	while ((bf->bf_siz << shift) - bf->bf_len < (size_t)n)
		shift++;
	if (shift > 0)
		buffer_grow(bf, shift);

	n = vsnprintf(&bf->bf_ptr[bf->bf_len], bf->bf_siz - bf->bf_len, fmt,
	    cp);
	va_end(cp);
	if (n < 0)
		err(1, "vsnprintf");
	bf->bf_len += n;
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
