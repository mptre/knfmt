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

#ifndef LIBKS_STRING_H
#define LIBKS_STRING_H

#include <stddef.h>	/* size_t */

#include "libks/section.h"

struct arena_scope;

struct KS_str_match {
	char u8[16 + 1 /* NUL */];

	char _padding[47];

	char u512[64];
} __attribute__((packed, aligned(64)));

struct KS_str_match_init_once {
	const char		*ranges;
	struct KS_str_match	*match;
} __attribute__((aligned(16)));

int		KS_str_match_init(const char *, struct KS_str_match *);

#define KS_str_match_init_once(r, m) do {				\
	__attribute__((used))						\
	SECTION(KS_str_ranges)						\
	static struct KS_str_match_init_once _once = {			\
		.ranges	= (r),						\
		.match	= (m),						\
	};								\
	/* Avoid unused warnings. */					\
	(void)_once;							\
} while (0)

extern size_t	(*KS_str_match)(const char *, size_t,
    const struct KS_str_match *);
size_t		KS_str_match_default(const char *, size_t,
    const struct KS_str_match *);
size_t		KS_str_match_spaces(const char *, size_t);

extern size_t	(*KS_str_match_until)(const char *, size_t,
    const struct KS_str_match *);
size_t		KS_str_match_until_default(const char *, size_t,
    const struct KS_str_match *);
size_t		KS_str_match_until_spaces(const char *, size_t);

char	**KS_str_split(const char *, const char *, struct arena_scope *);

#endif /* !LIBKS_STRING_H */
