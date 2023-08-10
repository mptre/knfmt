/*
 * Copyright (c) 2022 Anton Lindqvist <anton@basename.se>
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

#include "libks/vector.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct vector {
	size_t	vc_len;
	size_t	vc_siz;
	size_t	vc_stride;
};

static int vector_reserve1(struct vector **, size_t);

static struct vector		*ptov(void *);
static const struct vector	*cptov(const void *);

int
vector_init(void **vv, size_t stride)
{
	struct vector *vc;

	vc = calloc(1, sizeof(*vc));
	if (vc == NULL)
		return 1;
	vc->vc_stride = stride;
	*vv = &vc[1];
	return 0;
}

void
vector_free(void **vv)
{
	struct vector *vc;

	if (*vv == NULL)
		return;
	vc = ptov(*vv);
	free(vc);
	*vv = NULL;
}

int
vector_reserve(void **vv, size_t n)
{
	struct vector *vc = ptov(*vv);

	switch (vector_reserve1(&vc, n)) {
	case 1:
		*vv = &vc[1];
		break;
	case 0:
		break;
	case -1:
		return 1;
	}
	return 0;
}

size_t
vector_alloc(void **vv, int zero)
{
	struct vector *vc = ptov(*vv);

	switch (vector_reserve1(&vc, 1)) {
	case 1:
		*vv = &vc[1];
		break;
	case 0:
		break;
	case -1:
		return ULONG_MAX;
	}

	if (zero) {
		unsigned char *ptr;

		ptr = (unsigned char *)(&vc[1]);
		ptr += vc->vc_stride * vc->vc_len;
		memset(ptr, 0, vc->vc_stride);
	}

	return vc->vc_len++;
}

size_t
vector_pop(void *v)
{
	struct vector *vc = ptov(v);

	if (vc->vc_len == 0)
		return ULONG_MAX;
	return --vc->vc_len;
}

void
vector_clear(void *v)
{
	struct vector *vc = ptov(v);

	vc->vc_len = 0;
}

void
vector_sort(void *v, int (*cmp)(const void *, const void *))
{
	struct vector *vc = ptov(v);

	if (vc->vc_len > 0)
		qsort(v, vc->vc_len, vc->vc_stride, cmp);
}

size_t
vector_first(void *v)
{
	struct vector *vc = ptov(v);

	if (vc->vc_len == 0)
		return ULONG_MAX;
	return 0;
}

size_t
vector_last(void *v)
{
	struct vector *vc = ptov(v);

	if (vc->vc_len == 0)
		return ULONG_MAX;
	return vc->vc_len - 1;
}

size_t
vector_length(const void *v)
{
	const struct vector *vc = cptov(v);

	return vc->vc_len;
}

static int
vector_reserve1(struct vector **vv, size_t len)
{
	struct vector *vc = *vv;
	struct vector *newvc;
	size_t newsiz, totlen;

	if (vc->vc_len > ULONG_MAX - len)
		goto overflow;
	if (vc->vc_len + len < vc->vc_siz)
		return 0;

	newsiz = vc->vc_siz ? vc->vc_siz : 16;
	while (newsiz < vc->vc_len + len) {
		if (newsiz > ULONG_MAX / 2)
			goto overflow;
		newsiz *= 2;
	}
	totlen = newsiz;
	if (totlen > ULONG_MAX / vc->vc_stride)
		goto overflow;
	totlen *= vc->vc_stride;
	if (totlen > ULONG_MAX - sizeof(*vc))
		goto overflow;
	totlen += sizeof(*vc);
	newvc = realloc(vc, totlen);
	if (newvc == NULL)
		return -1;
	newvc->vc_siz = newsiz;
	*vv = newvc;
	return 1;

overflow:
	errno = EOVERFLOW;
	return -1;
}

static const struct vector *
cptov(const void *v)
{
	const struct vector *vc = (struct vector *)v;

	return &vc[-1];
}

static struct vector *
ptov(void *v)
{
	struct vector *vc = (struct vector *)v;

	return &vc[-1];
}
