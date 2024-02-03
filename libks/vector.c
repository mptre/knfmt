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

enum vector_error {
	VECTOR_REALLOCATED = 1,
	VECTOR_SUCCESS = 0,
	VECTOR_ERROR = -1,
};

struct vector {
	size_t			vc_siz;
	size_t			vc_stride;
	/* Must come last. */
	struct vector_public	p;
};

static enum vector_error vector_reserve1(struct vector **, size_t);

static struct vector		*ptov(void *);

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
	case VECTOR_REALLOCATED:
		*vv = &vc[1];
		break;
	case VECTOR_SUCCESS:
		break;
	case VECTOR_ERROR:
		return 1;
	}
	return 0;
}

size_t
vector_alloc(void **vv, int zero)
{
	struct vector *vc = ptov(*vv);

	switch (vector_reserve1(&vc, 1)) {
	case VECTOR_REALLOCATED:
		*vv = &vc[1];
		break;
	case VECTOR_SUCCESS:
		break;
	case VECTOR_ERROR:
		return ULONG_MAX;
	}

	if (zero) {
		unsigned char *ptr;

		ptr = (unsigned char *)(&vc[1]);
		ptr += vc->vc_stride * vc->p.len;
		memset(ptr, 0, vc->vc_stride);
	}

	return vc->p.len++;
}

size_t
vector_pop(void *v)
{
	struct vector *vc = ptov(v);

	if (vc->p.len == 0)
		return ULONG_MAX;
	return --vc->p.len;
}

void
vector_clear(void *v)
{
	struct vector *vc = ptov(v);

	vc->p.len = 0;
}

void
vector_sort(void *v, int (*cmp)(const void *, const void *))
{
	struct vector *vc = ptov(v);

	if (vc->p.len > 0)
		qsort(v, vc->p.len, vc->vc_stride, cmp);
}

size_t
vector_first(void *v)
{
	struct vector *vc = ptov(v);

	if (vc->p.len == 0)
		return ULONG_MAX;
	return 0;
}

size_t
vector_last(void *v)
{
	struct vector *vc = ptov(v);

	if (vc->p.len == 0)
		return ULONG_MAX;
	return vc->p.len - 1;
}

static enum vector_error
vector_reserve1(struct vector **vv, size_t len)
{
	struct vector *vc = *vv;
	struct vector *newvc;
	size_t newsiz, totlen;

	if (vc->p.len > ULONG_MAX - len)
		goto overflow;
	if (vc->p.len + len < vc->vc_siz)
		return VECTOR_SUCCESS;

	newsiz = vc->vc_siz ? vc->vc_siz : 16;
	while (newsiz < vc->p.len + len) {
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
		return VECTOR_ERROR;
	newvc->vc_siz = newsiz;
	*vv = newvc;
	return VECTOR_REALLOCATED;

overflow:
	errno = EOVERFLOW;
	return VECTOR_ERROR;
}

static struct vector *
ptov(void *v)
{
	struct vector *vc = (struct vector *)v;

	return &vc[-1];
}
