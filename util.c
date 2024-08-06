#include "util.h"

#include "config.h"

#include <ctype.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/buffer.h"
#include "libks/string.h"
#include "libks/vector.h"

unsigned int
colwidth(const char *str, size_t len, unsigned int cno, unsigned int *lno)
{
	while (len > 0) {
		size_t n;

		n = KS_str_match_until(str, len, "\t\t\n\n");
		cno += n;
		str += n;
		len -= n;

		for (; len > 0; len--, str++) {
			char c = str[0];

			if (c == '\t') {
				cno = ((cno + 8 - 1) & ~0x7u) + 1;
			} else if (c == '\n') {
				cno = 1;
				if (lno != NULL)
					(*lno)++;
			} else {
				break;
			}
		}
	}

	return cno;
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

const char *
path_slice(const char *path, unsigned int ncomponents, struct arena_scope *s)
{
	VECTOR(char *) components;
	struct buffer *bf;
	unsigned int i, n;

	components = KS_str_split(path, "/", s);
	n = VECTOR_LENGTH(components);
	if (n < ncomponents) {
		/* Less components than wanted, return the whole path. */
		return path;
	}

	bf = arena_buffer_alloc(s, 1 << 8);
	for (i = n - ncomponents; i < n; i++) {
		if (buffer_get_len(bf) > 0)
			buffer_putc(bf, '/');
		buffer_printf(bf, "%s", components[i]);
	}
	return buffer_str(bf);
}

int
is_path_header(const char *path)
{
	const char needle[] = ".h";
	const char *dot;

	dot = strrchr(path, '.');
	if (dot == NULL)
		return 0;
	return strcmp(dot, needle) == 0 && dot[sizeof(needle) - 1] == '\0';
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
