#include "parser-simple.h"

#include <string.h>

#include "parser-priv.h"

int
parser_simple_active(const struct parser *pr)
{
	return pr->pr_simple.nstmt == SIMPLE_STATE_ENABLE ||
	    pr->pr_simple.ndecl == SIMPLE_STATE_ENABLE;
}

void
parser_simple_disable(struct parser *pr, struct parser_simple *simple)
{
	*simple = pr->pr_simple;
	memset(&pr->pr_simple, 0, sizeof(pr->pr_simple));
}

void
parser_simple_enable(struct parser *pr, const struct parser_simple *simple)
{
	pr->pr_simple = *simple;
}
