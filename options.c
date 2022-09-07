#include "options.h"

#include "config.h"

#include <string.h>

void
options_init(struct options *op)
{
	memset(op, 0, sizeof(*op));
}
