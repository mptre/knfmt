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

static int	is_pass_mutually_exclusive(enum simple_pass);

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
simple_enter(struct simple *si, enum simple_pass pass, unsigned int flags,
    int *restore)
{
	*restore = si->states[pass];
	if (!si->enable && (flags & SIMPLE_FORCE) == 0) {
		si->states[pass] = SIMPLE_STATE_DISABLE;
		return 0;
	}

	if (is_pass_mutually_exclusive(pass)) {
		unsigned int i;

		for (i = 0; i < SIMPLE_LAST; i++) {
			if (i != pass &&
			    si->states[i] != SIMPLE_STATE_DISABLE) {
				si->states[pass] = SIMPLE_STATE_DISABLE;
				return 0;
			}
		}
	}

	if ((flags & SIMPLE_IGNORE) ||
	    si->states[pass] != SIMPLE_STATE_DISABLE) {
		si->states[pass] = SIMPLE_STATE_IGNORE;
		return 0;
	}

	si->states[pass] = SIMPLE_STATE_ENABLE;
	return 1;
}

void
simple_leave(struct simple *si, enum simple_pass pass, int restore)
{
	if (restore == SIMPLE_STATE_NOP)
		return;
	si->states[pass] = restore;
}

int
is_simple_enabled(const struct simple *si, enum simple_pass pass)
{
	return si->states[pass] == SIMPLE_STATE_ENABLE;
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
is_pass_mutually_exclusive(enum simple_pass pass)
{
	/* The static pass runs as part of the decl pass.*/
	if (pass == SIMPLE_STATIC)
		return 0;
	return 1;
}
