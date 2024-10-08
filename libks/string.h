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

extern size_t	(*KS_str_match)(const char *, size_t, const char *);
size_t		KS_str_match_default(const char *, size_t, const char *);
size_t		KS_str_match_native(const char *, size_t, const char *);

extern size_t	(*KS_str_match_until)(const char *, size_t, const char *);
size_t		KS_str_match_until_default(const char *, size_t, const char *);
size_t		KS_str_match_until_native(const char *, size_t, const char *);

char	**KS_str_split(const char *, const char *, struct arena_scope *);

#endif /* !LIBKS_STRING_H */
