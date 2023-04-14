#include "parser-simple.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "options.h"
#include "parser-priv.h"

struct parser_simple *
parser_simple_alloc(void)
{
	return ecalloc(1, sizeof(struct parser_simple));
}

void
parser_simple_free(struct parser_simple *simple)
{
	if (simple == NULL)
		return;
	free(simple);
}

int
parser_simple_active(const struct parser *pr)
{
	return pr->pr_simple->nstmt == SIMPLE_STATE_ENABLE ||
	    pr->pr_simple->ndecl == SIMPLE_STATE_ENABLE;
}

void
parser_simple_disable(struct parser *pr, struct parser_simple *simple)
{
	if (!pr->pr_op->op_flags.simple)
		return;
	*simple = *pr->pr_simple;
	memset(pr->pr_simple, 0, sizeof(*pr->pr_simple));
}

void
parser_simple_enable(struct parser *pr, const struct parser_simple *simple)
{
	if (!pr->pr_op->op_flags.simple)
		return;
	*pr->pr_simple = *simple;
}
