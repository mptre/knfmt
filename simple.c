#include "simple.h"

#include "config.h"

#include "libks/arena.h"

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

static int	is_pass_mutually_exclusive(const struct simple *,
    enum simple_pass);

struct simple *
simple_alloc(struct arena_scope *eternal_scope, const struct options *op)
{
	struct simple *si;

	si = arena_calloc(eternal_scope, 1, sizeof(*si));
	si->enable = op->simple;
	return si;
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

	if (is_pass_mutually_exclusive(si, pass)) {
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

/*
 * All passes are mutually exclusive by default. However, some combinations are
 * explicitly allowed as they do not interfere with each other.
 */
static int
is_pass_mutually_exclusive(const struct simple *si, enum simple_pass pass)
{
	static const enum simple_pass exceptions[][2] = {
		{ SIMPLE_DECL, SIMPLE_BRACES },
		{ SIMPLE_DECL, SIMPLE_DECL_PROTO },
		{ SIMPLE_DECL, SIMPLE_IMPLICIT_INT },
		{ SIMPLE_DECL, SIMPLE_STATIC },
		/*
		 * SIMPLE_DECL_FORWARD only operates on root level declarations
		 * as opposed to SIMPLE_DECL which only operates on nested
		 * levels. They should therefore not interfere.
		 */
		{ SIMPLE_DECL, SIMPLE_DECL_FORWARD },

		{ SIMPLE_EXPR_PARENS, SIMPLE_EXPR_SIZEOF },
	};
	unsigned int nexceptions = sizeof(exceptions) / sizeof(exceptions[0]);
	unsigned int i;

	for (i = 0; i < SIMPLE_LAST; i++) {
		unsigned int j;

		if (si->passes[i].state != SIMPLE_STATE_ENABLE &&
		    si->passes[i].state != SIMPLE_STATE_IGNORE)
			continue;

		for (j = 0; j < nexceptions; j++) {
			if (exceptions[j][0] == i && exceptions[j][1] == pass)
				return 0;
		}
	}

	return 1;
}
