#include "util.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>

#include "libks/buffer.h"

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
		else if (c == '\f')
			buffer_printf(bf, "\\f");
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
