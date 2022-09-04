#include "options.h"

#include "config.h"

#include <string.h>

void
options_init(struct options *op)
{
	memset(op, 0, sizeof(*op));
	op->op_mw = 80;
	op->op_tw = 8;
	op->op_sw = 4;
}
