#include "util.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *
strnice(const char *str, size_t len)
{
	char *buf, *p;
	size_t i;

	buf = p = malloc(4 * len + 1);
	if (buf == NULL)
		err(1, NULL);
	for (i = 0; i < len; i++) {
		unsigned char c = str[i];

		if (c == '\n') {
			*buf++ = '\\';
			*buf++ = 'n';
		} else if (c == '\t') {
			*buf++ = '\\';
			*buf++ = 't';
		} else if (c == '"') {
			*buf++ = '\\';
			*buf++ = '"';
		} else if (isprint(c)) {
			*buf++ = c;
		} else {
			int n;

			n = sprintf(buf, "\\x%02x", c);
			if (n < 0)
				err(1, "%s: sprintf", __func__);
			buf += n;
		}
	}
	*buf = '\0';
	return p;
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
