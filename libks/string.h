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

struct arena_scope;

#define KS_FEATURE_STR_NATIVE		0x00000001u

extern unsigned int KS_features;

#define KS_str_match(str, len, ranges) __extension__ ({			\
	char _static_assert[sizeof(ranges) - 1 > 16 ? -1 : 0]		\
		__attribute__((unused));				\
	size_t _rv;							\
	if (KS_features & KS_FEATURE_STR_NATIVE)			\
		_rv = KS_str_match_native((str), (len),	(ranges));	\
	else								\
		_rv = KS_str_match_default((str), (len), (ranges));	\
	_rv;								\
})

size_t	KS_str_match_default(const char *, size_t, const char *);
size_t	KS_str_match_native(const char *, size_t, const char *);

char	**KS_str_split(const char *, const char *, struct arena_scope *);

#endif /* !LIBKS_STRING_H */
