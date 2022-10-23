#include "error.h"

#include "config.h"

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"

struct error {
	struct buffer	*er_bf;
	int		 er_flush;
};

struct error *
error_alloc(int flush)
{
	struct error *er;

	er = malloc(sizeof(*er));
	if (er == NULL)
		err(1, NULL);
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
	free(er);
}

struct buffer *
error_begin(struct error *er)
{
	if (er->er_bf == NULL)
		er->er_bf = buffer_alloc(1024);
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
	if (er->er_bf == NULL)
		return;
	if (!force && !er->er_flush)
		return;

	if (er->er_bf->bf_len > 0) {
		fprintf(stderr, "%.*s",
		    (int)er->er_bf->bf_len, er->er_bf->bf_ptr);
	}
	error_reset(er);
}
