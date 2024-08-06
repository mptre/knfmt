#include "options.h"

#include "config.h"

#include <err.h>
#include <limits.h>
#include <string.h>

#include "trace.h"

static int	ctotrace(char c);

void
options_init(struct options *op)
{
	memset(op, 0, sizeof(*op));
}

int
options_trace_parse(struct options *op, const char *flags)
{
	for (; *flags != '\0'; flags++) {
		int idx;

		idx = ctotrace(*flags);
		if (idx == -1) {
			warnx("%c: unknown trace flag", *flags);
			return 1;
		} else if (idx == 0) {
			size_t i;

			for (i = 0; i < TRACE_MAX; i++)
				op->op_trace[i] = UINT_MAX;
		} else {
			op->op_trace[idx]++;
		}
	}
	return 0;
}

static int
ctotrace(char c)
{
	switch (c) {
#define OP(name, shortname) case shortname: return TRACE_ ## name;
	FOR_TRACE_TYPES(OP)
#undef OP
	}
	return -1;
}
