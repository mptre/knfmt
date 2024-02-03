#include "util.h"

#include "config.h"

#include <ctype.h>

#include "libks/arena-buffer.h"
#include "libks/buffer.h"
#include "libks/compiler.h"

unsigned int
colwidth(const char *str, size_t len, unsigned int cno, unsigned int *lno)
{
	/* Fast path. */
	if (likely(len == 1 && str[0] != '\n' && str[0] != '\t'))
		return cno + 1;

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

const char *
strnice(const char *str, size_t len, struct arena_scope *s)
{
	struct buffer *bf;

	bf = arena_buffer_alloc(s, 2 * len + 1);
	strnice_buffer(bf, str, len);
	return buffer_str(bf);
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
