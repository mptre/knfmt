#include "vector.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

struct vector {
	size_t	vc_len;
	size_t	vc_siz;
	size_t	vc_esiz;
};

static int vector_reserve(struct vector **, size_t);

static struct vector	*ptov(void *);
static struct vector	*pptov(void **);

void
vector_init(void **vv, size_t esiz)
{
	struct vector *vc;

	vc = calloc(1, sizeof(*vc));
	if (vc == NULL)
		err(1, NULL);
	vc->vc_esiz = esiz;
	*vv = &vc[1];
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
vector_alloc(void **vv, int zero)
{
	struct vector *vc = pptov(vv);

	if (vector_reserve(&vc, 1))
		*vv = &vc[1];

	if (zero) {
		unsigned char *ptr;

		ptr = (unsigned char *)(&vc[1]);
		ptr += vc->vc_esiz * vc->vc_len;
		memset(ptr, 0, vc->vc_esiz);
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
vector_reserve(struct vector **vv, size_t len)
{
	struct vector *vc = *vv;
	size_t newsiz, totlen;

	if (vc->vc_len + len < vc->vc_siz)
		return 0;

	newsiz = vc->vc_siz ? vc->vc_siz : 16;
	while (newsiz < vc->vc_len + len) {
		if (newsiz > ULONG_MAX / 2)
			errc(1, EOVERFLOW, "%s", __func__);
		newsiz *= 2;
	}
	totlen = newsiz;
	if (totlen > ULONG_MAX / vc->vc_esiz)
		errc(1, EOVERFLOW, "%s", __func__);
	totlen *= vc->vc_esiz;
	if (totlen > ULONG_MAX - sizeof(*vc))
		errc(1, EOVERFLOW, "%s", __func__);
	totlen += sizeof(*vc);
	vc = realloc(vc, totlen);
	if (vc == NULL)
		err(1, NULL);
	vc->vc_siz = newsiz;
	*vv = vc;
	return 1;
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
