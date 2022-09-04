#include "util.h"

#include "config.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

char *
strnice(const char *str, size_t len)
{
	char *buf, *p;
	size_t i;

	buf = p = malloc(2 * len + 1);
	if (buf == NULL)
		err(1, NULL);
	for (i = 0; i < len; i++) {
		if (str[i] == '\n') {
			*buf++ = '\\';
			*buf++ = 'n';
		} else if (str[i] == '\t') {
			*buf++ = '\\';
			*buf++ = 't';
		} else if (str[i] == '"') {
			*buf++ = '\\';
			*buf++ = '"';
		} else {
			*buf++ = str[i];
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
