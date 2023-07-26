#include "util.h"

#include "config.h"

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>	/* PATH_MAX */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
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

unsigned int
colwidth(const char *str, size_t len, unsigned int cno, unsigned int *lno)
{
	for (; len > 0; len--, str++) {
		switch (str[0]) {
		case '\n':
			cno = 1;
			if (lno != NULL)
				(*lno)++;
			break;
		case '\t':
			cno = ((cno + 8 - 1) & ~0x7u) + 1;
			break;
		default:
			cno++;
		}
	}
	return cno;
}

size_t
strindent_buffer(struct buffer *bf, size_t indent, int usetabs, size_t pos)
{
	size_t i = 0;

	if (usetabs) {
		for (; i + 8 <= indent; i += 8) {
			buffer_putc(bf, '\t');
			pos += 8 - (pos % 8);
		}
	}
	for (; i < indent; i++) {
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
	buf = buffer_str(bf);
	if (buf == NULL)
		err(1, NULL);
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
		else if (isprint((unsigned char)c))
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
	if (dirfd != -1)
		close(dirfd);
	if (fd != -1 && nlevels != NULL)
		*nlevels = i;
	return fd;
}

/*
 * Transform the given path into a mkstemp(3) compatible template.
 * The temporary file will reside in the same directory as a "hidden" file.
 */
char *
tmptemplate(const char *path)
{
	struct buffer *bf;
	char *template;
	const char *basename, *p;

	bf = buffer_alloc(PATH_MAX);

	p = strrchr(path, '/');
	if (p != NULL) {
		buffer_puts(bf, path, (size_t)(&p[1] - path));
		basename = &p[1];
	} else {
		basename = path;
	}
	buffer_printf(bf, ".%s.XXXXXXXX", basename);
	template = buffer_str(bf);
	buffer_free(bf);
	return template;
}
