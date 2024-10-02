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

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "libks/arena-vector.h"
#include "libks/arena.h"
#include "libks/compiler.h"
#include "libks/section.h"
#include "libks/vector.h"

int	KS_str_match_init_default(const char *, struct KS_str_match *);

static void	KS_str_init(void) __attribute__((constructor));

static void
KS_str_init(void)
{
	struct KS_str_match_init_once *once = NULL;

	while (SECTION_ITERATE(once, KS_str_ranges)) {
		/* Suppress cppcheck nullPointer false positive. */
		assert(once != NULL);
		if (KS_str_match_init(once->ranges, once->match) == -1)
			__builtin_trap();
	}
}

int
KS_str_match_init_default(const char *ranges, struct KS_str_match *match)
{
	size_t len;

	len = strlen(ranges);
	if (len > 16) {
		errno = ENAMETOOLONG;
		return -1;
	}
	if (len == 0 || len & 1) {
		errno = EINVAL;
		return -1;
	}

	memset(match, 0, sizeof(*match));
	memcpy(match->u8, ranges, len);
	return 0;
}

size_t
KS_str_match_default(const char *str, size_t len,
    const struct KS_str_match *match)
{
	const char *ranges = match->u8;
	size_t i;

	for (i = 0; i < len; i++) {
		size_t j;
		int found = 0;

		for (j = 0; ranges[j] != '\0'; j += 2) {
			if (str[i] >= ranges[j] && str[i] <= ranges[j + 1]) {
				found = 1;
				break;
			}
		}
		if (!found)
			break;
	}

	return i;
}

size_t
KS_str_match_spaces(const char *str, size_t len)
{
	static struct KS_str_match match;

	KS_str_match_init_once("  \f\f\n\n\r\r\t\t\v\v", &match);

	return KS_str_match(str, len, &match);
}

size_t
KS_str_match_until_default(const char *str, size_t len,
    const struct KS_str_match *match)
{
	const char *ranges = match->u8;
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

size_t
KS_str_match_until_spaces(const char *str, size_t len)
{
	static struct KS_str_match match;

	KS_str_match_init_once("  \f\f\n\n\r\r\t\t\v\v", &match);

	return KS_str_match_until(str, len, &match);
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
