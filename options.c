#include "options.h"

#include "config.h"

#include <string.h>

void
options_init(struct options *op)
{
	memset(op, 0, sizeof(*op));
}

int
options_trace(const struct options *op)
{
	return op->op_verbose >= 2;
}
