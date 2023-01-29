#include "util.h"

#include "config.h"

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"

void
tracef(unsigned char ident, const char *fun, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "[%c] %s: ", ident, fun);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

size_t
strindent_buffer(struct buffer *bf, int indent, int usetabs, size_t pos)
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

char *
strnice(const char *str, size_t len)
{
	struct buffer *bf;
	char *buf;

	bf = buffer_alloc(2 * len + 1);
	if (bf == NULL)
		err(1, NULL);
	strnice_buffer(bf, str, len);
	buffer_putc(bf, '\0');
	buf = buffer_release(bf);
	buffer_free(bf);
	return buf;
}

void
strnice_buffer(struct buffer *bf, const char *str, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		char c = str[i];

		if (c == '\n')
			buffer_printf(bf, "\\n");
		else if (c == '\t')
			buffer_printf(bf, "\\t");
		else if (c == '"')
			buffer_printf(bf, "\\\"");
		else if (isprint(c))
			buffer_putc(bf, c);
		else
			buffer_printf(bf, "\\x%02x", (unsigned char)c);
	}
}

/*
 * Returns the width of the last line in the given string, with respect to tabs.
 */
size_t
strwidth(const char *str, size_t len, size_t pos)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (str[i] == '\n')
			pos = 0;
		else if (str[i] == '\t')
			pos += 8 - (pos % 8);
		else
			pos += 1;
	}
	return pos;
}

/*
 * Search for the given filename starting at the current working directory and
 * traverse upwards. Returns a file descriptor to the file if found, -1 otherwise.
 * The optional nlevels argument reflects the number of directories traversed
 * upwards.
 */
int
searchpath(const char *filename, int *nlevels)
{
	struct stat sb;
	dev_t dev = 0;
	ino_t ino = 0;
	int fd = -1;
	int flags = O_RDONLY | O_CLOEXEC;
	int i = 0;
	int dirfd;

	dirfd = open(".", flags | O_DIRECTORY);
	if (dirfd == -1)
		return -1;
	if (fstat(dirfd, &sb) == -1)
		goto out;
	dev = sb.st_dev;
	ino = sb.st_ino;
	for (;;) {
		fd = openat(dirfd, filename, flags);
		if (fd >= 0)
			break;

		fd = openat(dirfd, "..", flags | O_DIRECTORY);
		close(dirfd);
		dirfd = fd;
		fd = -1;
		if (dirfd == -1)
			break;
		if (fstat(dirfd, &sb) == -1)
			break;
		if (dev == sb.st_dev && ino == sb.st_ino)
			break;
		dev = sb.st_dev;
		ino = sb.st_ino;
		i++;
	}

out:
	close(dirfd);
	if (fd != -1 && nlevels != NULL)
		*nlevels = i;
	return fd;
}
