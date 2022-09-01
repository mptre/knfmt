#include "vector.h"

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

static struct vector	*ptov(void *);
static struct vector	*pptov(void **);

size_t
vector_init(void **vv, size_t stride)
{
	struct vector *vc;

	vc = calloc(1, sizeof(*vc));
	if (vc == NULL)
		return ULONG_MAX;
	vc->vc_stride = stride;
	*vv = &vc[1];
	return 0;
}

void
vector_free(void **vv)
{
	struct vector *vc = pptov(vv);

	if (vc == NULL)
		return;
	free(vc);
	*vv = NULL;
}

size_t
vector_reserve(void **vv, size_t n)
{
	struct vector *vc = pptov(vv);

	switch (vector_reserve1(&vc, n)) {
	case 1:
		*vv = &vc[1];
		break;
	case 0:
		break;
	case -1:
		return ULONG_MAX;
	}
	return 0;
}

size_t
vector_alloc(void **vv, int zero)
{
	struct vector *vc = pptov(vv);

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
vector_length(void *v)
{
	struct vector *vc = ptov(v);

	return vc->vc_len;
}

static int
vector_reserve1(struct vector **vv, size_t len)
{
	struct vector *vc = *vv;
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
	vc = realloc(vc, totlen);
	if (vc == NULL)
		return -1;
	vc->vc_siz = newsiz;
	*vv = vc;
	return 1;

overflow:
	errno = EOVERFLOW;
	return -1;
}

static struct vector *
pptov(void **ptr)
{
	struct vector **vc = (struct vector **)ptr;

	if (*vc == NULL)
		return NULL;
	return &(*vc)[-1];
}

static struct vector *
ptov(void *ptr)
{
	struct vector *vc = (struct vector *)ptr;

	if (vc == NULL)
		return NULL;
	return &vc[-1];
}
