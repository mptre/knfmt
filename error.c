#include "error.h"

#include "config.h"

#include <stdio.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"

struct error {
	struct buffer	*er_bf;
	int		 er_flush;
};

struct error *
error_alloc(int flush, struct arena_scope *s)
{
	struct error *er;

	er = arena_calloc(s, 1, sizeof(*er));
	er->er_bf = arena_buffer_alloc(s, 1 << 10);
	er->er_flush = flush;
	return er;
}

struct buffer *
error_begin(struct error *er)
{
	return er->er_bf;
}

void
error_end(struct error *er)
{
	error_flush(er, 0);
}

void
error_reset(struct error *er)
{
	buffer_reset(er->er_bf);
}

void
error_flush(struct error *er, int force)
{
	size_t buflen;

	if (!force && !er->er_flush)
		return;

	buflen = buffer_get_len(er->er_bf);
	if (buflen > 0)
		fprintf(stderr, "%.*s", (int)buflen, buffer_get_ptr(er->er_bf));
	error_reset(er);
}
