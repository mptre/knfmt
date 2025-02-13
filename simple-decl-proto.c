#include "simple-decl-proto.h"

#include "config.h"

#include <assert.h>
#include <err.h>

#include "libks/arena.h"
#include "libks/vector.h"

#include "lexer.h"
#include "token.h"

struct simple_decl_proto {
	VECTOR(struct argument)	 arguments;
	struct lexer		*lx;
	struct {
		unsigned int	ignore:1;
	} flags;
};

struct argument {
	struct token	*tk;
};

struct simple_decl_proto *
simple_decl_proto_enter(struct lexer *lx, struct arena_scope *s)
{
	struct simple_decl_proto *sp;

	sp = arena_calloc(s, 1, sizeof(*sp));
	if (VECTOR_INIT(sp->arguments))
		err(1, NULL);
	sp->lx = lx;
	return sp;
}

void
simple_decl_proto_leave(struct simple_decl_proto *sp)
{
	size_t nargs = VECTOR_LENGTH(sp->arguments);
	size_t nunnamed = 0;
	size_t i;

	if (sp->flags.ignore)
		return;

	for (i = 0; i < VECTOR_LENGTH(sp->arguments); i++) {
		struct argument *arg = &sp->arguments[i];

		if (arg->tk == NULL)
			nunnamed++;
	}
	if (nunnamed == 0 || nargs == nunnamed)
		return;

	for (i = 0; i < VECTOR_LENGTH(sp->arguments); i++) {
		struct argument *arg = &sp->arguments[i];

		if (arg->tk != NULL)
			lexer_remove(sp->lx, arg->tk);
	}
}

void
simple_decl_proto_free(struct simple_decl_proto *sp)
{
	if (sp == NULL)
		return;
	VECTOR_FREE(sp->arguments);
}

void
simple_decl_proto_arg(struct simple_decl_proto *sp)
{
	if (VECTOR_CALLOC(sp->arguments) == NULL)
		err(1, NULL);
}

static int
is_qualifier(const struct token *tk)
{
	if ((tk->tk_flags & TOKEN_FLAG_QUALIFIER) == 0)
		return 0;

	/* Ensure this is not the first token as part of the argument. */
	const struct token *pv = token_prev(tk);
	if (pv->tk_type == TOKEN_LPAREN || pv->tk_type == TOKEN_COMMA ||
	    (pv->tk_flags & TOKEN_FLAG_QUALIFIER))
		return 0;

	return 1;
}

void
simple_decl_proto_arg_ident(struct simple_decl_proto *sp, struct token *tk)
{
	struct argument *arg;
	struct token *pv;

	arg = VECTOR_LAST(sp->arguments);
	assert(arg != NULL);
	pv = token_prev(tk);
	if (pv->tk_type == TOKEN_STAR ||
	    (pv->tk_flags & TOKEN_FLAG_TYPE) ||
	    is_qualifier(pv))
		arg->tk = tk;
	else
		sp->flags.ignore = 1;
}
