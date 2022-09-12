#include "options.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <string.h>

static int	ctotrace(unsigned char c);

void
options_init(struct options *op)
{
	memset(op, 0, sizeof(*op));
}

int
options_trace_parse(struct options *op, const char *flags)
{
	for (; *flags != '\0'; flags++) {
		int trace;

		trace = ctotrace(*flags);
		if (trace == -1) {
			errx(1, "%c: unknown trace flag", *flags);
			return 1;
		}
		op->op_trace[trace]++;
	}
	return 0;
}

unsigned int
trace(const struct options *op, unsigned char c)
{
	return op->op_trace[ctotrace(c)];
}

static int
ctotrace(unsigned char c)
{
	/* Keep in sync with NTRACES and tests/trace-001.c. */
	switch (c) {
	case 'd': return 0;	/* doc */
	case 'D': return 1;	/* diff */
	case 'l': return 2;	/* lexer */
	case 's': return 3;	/* style */
	case 'S': return 4;	/* simple */
	default:
		return -1;
	}
}
