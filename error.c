#include "error.h"

#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

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

void
error_reset(struct error *er)
{
	if (er->er_bf != NULL)
		buffer_reset(er->er_bf);
}

void
error_flush(struct error *er)
{
	if (!er->er_flush || er->er_bf == NULL)
		return;

	if (er->er_bf->bf_len > 0)
		fprintf(stderr, "%.*s",
		    (int)er->er_bf->bf_len, er->er_bf->bf_ptr);
	error_reset(er);
}

struct buffer *
error_get_buffer(struct error *er)
{
	if (er->er_bf == NULL)
		er->er_bf = buffer_alloc(1024);
	return er->er_bf;
}
