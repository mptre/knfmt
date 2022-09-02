#include <stdio.h>

#include "extern.h"

void
error_init(struct error *er, int flush)
{
	er->er_bf = NULL;
	er->er_flush = flush;
}

void
error_close(struct error *er)
{
	buffer_free(er->er_bf);
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
