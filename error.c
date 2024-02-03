#include "error.h"

#include "config.h"

#include <err.h>
#include <stdio.h>

#include "libks/arena.h"
#include "libks/buffer.h"

struct error {
	struct buffer	*er_bf;
	int		 er_flush;
};

struct error *
error_alloc(struct arena_scope *eternal_scope, int flush)
{
	struct error *er;

	er = arena_calloc(eternal_scope, 1, sizeof(*er));
	er->er_bf = NULL;
	er->er_flush = flush;
	return er;
}

void
error_free(struct error *er)
{
	if (er == NULL)
		return;
	buffer_free(er->er_bf);
}

struct buffer *
error_begin(struct error *er)
{
	if (er->er_bf == NULL) {
		er->er_bf = buffer_alloc(1024);
		if (er->er_bf == NULL)
			err(1, NULL);
	}
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
	if (er->er_bf != NULL)
		buffer_reset(er->er_bf);
}

void
error_flush(struct error *er, int force)
{
	size_t buflen;

	if (er->er_bf == NULL)
		return;
	if (!force && !er->er_flush)
		return;

	buflen = buffer_get_len(er->er_bf);
	if (buflen > 0)
		fprintf(stderr, "%.*s", (int)buflen, buffer_get_ptr(er->er_bf));
	error_reset(er);
}
