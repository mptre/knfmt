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

#include "libks/string.h"

#include <string.h>

#include "libks/arena-vector.h"
#include "libks/arena.h"
#include "libks/compiler.h"
#include "libks/vector.h"

size_t
KS_str_match_default(const char *str, size_t len, const char *ranges)
{
	size_t i;

	for (i = 0; i < len; i++) {
		size_t j;
		int match = 0;

		for (j = 0; ranges[j] != '\0'; j += 2) {
			if (str[i] >= ranges[j] && str[i] <= ranges[j + 1]) {
				match = 1;
				break;
			}
		}
		if (!match)
			break;
	}

	return i;
}

size_t
KS_str_match_until_default(const char *str, size_t len, const char *ranges)
{
	size_t i;

	for (i = 0; i < len; i++) {
		size_t j;

		for (j = 0; ranges[j] != '\0'; j += 2) {
			if (str[i] >= ranges[j] && str[i] <= ranges[j + 1])
				return i;
		}
	}

	return len;
}

char **
KS_str_split(const char *str, const char *delim, struct arena_scope *s)
{
	VECTOR(char *) parts;
	size_t delimlen;

	ARENA_VECTOR_INIT(s, parts, 2);
	delimlen = strlen(delim);

	for (;;) {
		const char *p;
		char **dst;

		dst = VECTOR_ALLOC(parts);
		if (unlikely(dst == NULL))
			return NULL; /* UNREACHABLE */
		p = strstr(str, delim);
		if (p != NULL) {
			*dst = arena_strndup(s, str, (size_t)(p - str));
			str = &p[delimlen];
		} else {
			*dst = arena_strdup(s, str);
			break;
		}
	}

	return parts;
}
