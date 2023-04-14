#include "parser-simple.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "options.h"
#include "parser-priv.h"

struct parser_simple *
parser_simple_alloc(const struct options *op)
{
	struct parser_simple *simple;

	simple = ecalloc(1, sizeof(struct parser_simple));
	simple->enable = op->op_flags.simple;
	return simple;
}

void
parser_simple_free(struct parser_simple *simple)
{
	if (simple == NULL)
		return;
	free(simple);
}

int
parser_simple_enter(struct parser *pr, unsigned int pass, int ignore,
    int *restore)
{
	struct parser_simple *simple = pr->pr_simple;
	unsigned int i;

	assert(pass < SIMPLE_LAST);

	if (!simple->enable) {
		*restore = SIMPLE_STATE_NOP;
		return 0;
	}
	*restore = simple->states[pass];

	for (i = 0; i < SIMPLE_LAST; i++) {
		if (i != pass && simple->states[i] != SIMPLE_STATE_DISABLE) {
			simple->states[pass] = SIMPLE_STATE_DISABLE;
			return 0;
		}
	}

	if (ignore || simple->states[pass] != SIMPLE_STATE_DISABLE) {
		simple->states[pass] = SIMPLE_STATE_IGNORE;
		return 0;
	}

	simple->states[pass] = SIMPLE_STATE_ENABLE;
	return 1;
}

void
parser_simple_leave(struct parser *pr, unsigned int pass, int restore)
{
	struct parser_simple *simple = pr->pr_simple;

	assert(pass < SIMPLE_LAST);
	if (restore == SIMPLE_STATE_NOP)
		return;
	simple->states[pass] = restore;
}

int
is_simple_enabled(const struct parser *pr, unsigned int pass)
{
	assert(pass < SIMPLE_LAST);
	return pr->pr_simple->enable &&
	    pr->pr_simple->states[pass] == SIMPLE_STATE_ENABLE;
}

int
is_simple_any_enabled(const struct parser *pr)
{
	struct parser_simple *simple = pr->pr_simple;
	int i;

	for (i = 0; i < SIMPLE_LAST; i++) {
		if (simple->states[i] == SIMPLE_STATE_ENABLE)
			return 1;
	}
	return 0;
}

int
parser_simple_disable(struct parser *pr)
{
	int restore = pr->pr_simple->enable;

	pr->pr_simple->enable = 0;
	return restore;
}

void
parser_simple_enable(struct parser *pr, int restore)
{
	pr->pr_simple->enable = restore;
}
