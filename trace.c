#include "trace.h"

#include <stdarg.h>
#include <stdio.h>

void
trace_impl(char ident, const char *fun, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "[%c]", ident);
	if (fun != NULL)
		fprintf(stderr, " %s:", fun);
	fprintf(stderr, " ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
