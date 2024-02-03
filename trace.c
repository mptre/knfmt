#include "trace.h"

#include <stdarg.h>
#include <stdio.h>

void
trace_impl(char ident, const char *fun, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "[%c] %s: ", ident, fun);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
