#include "simple.h"

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "options.h"

struct simple {
	int			 states[SIMPLE_LAST];
	int			 enable;
};

static int	is_pass_valid(unsigned int);

struct simple *
simple_alloc(const struct options *op)
{
	struct simple *si;

	si = ecalloc(1, sizeof(*si));
	si->enable = op->op_flags.simple;
	return si;
}

void
simple_free(struct simple *si)
{
	if (si == NULL)
		return;
	free(si);
}

int
simple_enter(struct simple *si, unsigned int pass, int ignore,
    int *restore)
{
	unsigned int i;

	if (!is_pass_valid(pass)) {
		*restore = SIMPLE_STATE_NOP;
		return 0;
	}

	*restore = si->states[pass];
	if (!si->enable) {
		si->states[pass] = SIMPLE_STATE_DISABLE;
		return 0;
	}

	for (i = 0; i < SIMPLE_LAST; i++) {
		if (i != pass && si->states[i] != SIMPLE_STATE_DISABLE) {
			si->states[pass] = SIMPLE_STATE_DISABLE;
			return 0;
		}
	}

	if (ignore || si->states[pass] != SIMPLE_STATE_DISABLE) {
		si->states[pass] = SIMPLE_STATE_IGNORE;
		return 0;
	}

	si->states[pass] = SIMPLE_STATE_ENABLE;
	return 1;
}

void
simple_leave(struct simple *si, unsigned int pass, int restore)
{
	if (restore == SIMPLE_STATE_NOP || !is_pass_valid(pass))
		return;
	si->states[pass] = restore;
}

int
is_simple_enabled(const struct simple *si, unsigned int pass)
{
	return is_pass_valid(pass) &&
	    si->states[pass] == SIMPLE_STATE_ENABLE;
}

int
is_simple_any_enabled(const struct simple *si)
{
	int i;

	for (i = 0; i < SIMPLE_LAST; i++) {
		if (si->states[i] == SIMPLE_STATE_ENABLE)
			return 1;
	}
	return 0;
}

int
simple_disable(struct simple *si)
{
	int restore = si->enable;

	si->enable = 0;
	return restore;
}

void
simple_enable(struct simple *si, int restore)
{
	si->enable = restore;
}

static int
is_pass_valid(unsigned int pass)
{
	return pass < SIMPLE_LAST;
}
