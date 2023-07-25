#include "simple.h"

#include "config.h"

#include <stdlib.h>

#include "alloc.h"
#include "options.h"

enum simple_state {
	SIMPLE_STATE_NOP	= -1,
	SIMPLE_STATE_DISABLE	= 0,
	SIMPLE_STATE_ENABLE	= 1,
	SIMPLE_STATE_IGNORE	= 2,
};

struct simple {
	struct {
		enum simple_state	state;
		unsigned int		flags;
	} passes[SIMPLE_LAST];
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
    struct simple_cookie *cookie)
{
	*cookie = (struct simple_cookie){
	    .si		= si,
	    .pass	= pass,
	    .state	= si->passes[pass].state,
	};

	if (!si->enable && (flags & SIMPLE_FORCE) == 0) {
		si->passes[pass].state = SIMPLE_STATE_IGNORE;
		return 0;
	}

	if (is_pass_mutually_exclusive(pass)) {
		unsigned int i;

		for (i = 0; i < SIMPLE_LAST; i++) {
			if (i != pass &&
			    si->passes[i].state != SIMPLE_STATE_DISABLE) {
				si->passes[pass].state = SIMPLE_STATE_DISABLE;
				return 0;
			}
		}
	}

	if ((flags & SIMPLE_IGNORE) ||
	    si->passes[pass].state != SIMPLE_STATE_DISABLE) {
		si->passes[pass].state = SIMPLE_STATE_IGNORE;
		return 0;
	}

	si->passes[pass].state = SIMPLE_STATE_ENABLE;
	si->passes[pass].flags = flags;
	return 1;
}

void
simple_leave(struct simple_cookie *cookie)
{
	struct simple *si = cookie->si;
	enum simple_pass pass = cookie->pass;
	enum simple_state state = cookie->state;

	if (si == NULL)
		return;

	*cookie = (struct simple_cookie){0};
	if (state == SIMPLE_STATE_NOP)
		return;
	si->passes[pass].state = state;
}

int
is_simple_enabled(const struct simple *si, enum simple_pass pass)
{
	return (si->enable || (si->passes[pass].flags & SIMPLE_FORCE)) &&
	    si->passes[pass].state == SIMPLE_STATE_ENABLE;
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
	switch (pass) {
	case SIMPLE_BRACES:
	case SIMPLE_DECL_PROTO:
	case SIMPLE_STATIC:
		/* Nested under decl pass and should not interfere. */
		return 0;
	default:
		return 1;
	}
}
