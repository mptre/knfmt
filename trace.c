#include "trace.h"

#include "config.h"

#include <stdarg.h>
#include <stdio.h>

static char
trace_type_str(enum trace_type type)
{
	switch (type) {
#define OP(name, shortname) case TRACE_ ## name: return shortname;
	FOR_TRACE_TYPES(OP)
#undef OP
	case TRACE_MAX:
		break;
	}
	return '-';
}

void
trace_impl(enum trace_type type, const char *fun, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "[%c]", trace_type_str(type));
	if (fun != NULL)
		fprintf(stderr, " %s:", fun);
	fprintf(stderr, " ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
