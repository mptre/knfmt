#include "options.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <limits.h>
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
		int idx;

		idx = ctotrace(*flags);
		if (idx == -1) {
			errx(1, "%c: unknown trace flag", *flags);
			return 1;
		} else if (idx == 0) {
			size_t i;

			for (i = 0; i < sizeof(traces); i++)
				op->op_trace[i] = UINT_MAX;
		} else {
			op->op_trace[idx]++;
		}
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
	size_t len = sizeof(traces);
	size_t i;

	for (i = 0; i < len; i++) {
		if (traces[i] == c)
			return i;
	}
	return -1;
}
