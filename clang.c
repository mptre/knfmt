#include "clang.h"

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "cdefs.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "vector.h"
#include "util.h"

struct clang {
	const struct options	*cl_op;
	VECTOR(struct token *)	 cl_branches;
};

static void	clang_branch_enter(struct clang *, struct lexer *,
    struct token *,
    struct token *);
static void	clang_branch_link(struct clang *, struct lexer *,
    struct token *,
    struct token *);
static void	clang_branch_leave(struct clang *, struct lexer *,
    struct token *,
    struct token *);
static void	clang_branch_purge(struct clang *, struct lexer *);

static void	token_branch_link(struct token *, struct token *);

#define clang_trace(cl, fmt, ...) do {					\
	if (trace((cl)->cl_op, 'c'))					\
		tracef('C', __func__, (fmt), __VA_ARGS__);		\
} while (0)

struct clang *
clang_alloc(const struct options *op)
{
	struct clang *cl;

	cl = calloc(1, sizeof(*cl));
	if (cl == NULL)
		err(1, NULL);
	cl->cl_op = op;
	if (VECTOR_INIT(cl->cl_branches) == NULL)
		err(1, NULL);
	return cl;
}

void
clang_free(struct clang *cl)
{
	if (cl == NULL)
		return;

	assert(VECTOR_EMPTY(cl->cl_branches));
	VECTOR_FREE(cl->cl_branches);
	free(cl);
}

struct token *
clang_read(struct lexer *lx, void *arg)
{
	struct clang *cl = arg;
	struct token *prefix, *tk;

	tk = lexer_read(lx, arg);
	if (tk == NULL) {
		clang_branch_purge(cl, lx);
		return NULL;
	}

	/* Establish links between cpp branches. */
	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
		switch (prefix->tk_type) {
		case TOKEN_CPP_IF:
			clang_branch_enter(cl, lx, prefix, tk);
			break;
		case TOKEN_CPP_ELSE:
			clang_branch_link(cl, lx, prefix, tk);
			break;
		case TOKEN_CPP_ENDIF:
			clang_branch_leave(cl, lx, prefix, tk);
			break;
		}
	}
	if (tk->tk_type == LEXER_EOF)
		clang_branch_purge(cl, lx);

	return tk;
}

static void
clang_branch_enter(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *tk)
{
	struct token **br;

	clang_trace(cl, "%s", lexer_serialize(lx, cpp));
	cpp->tk_token = tk;
	br = VECTOR_ALLOC(cl->cl_branches);
	if (br == NULL)
		err(1, NULL);
	*br = cpp;
}

static void
clang_branch_link(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *tk)
{
	struct token *br;

	/* Silently ignore broken branch. */
	if (VECTOR_EMPTY(cl->cl_branches)) {
		token_branch_unlink(cpp);
		return;
	}
	br = *VECTOR_LAST(cl->cl_branches);

	/*
	 * Discard branches hanging of the same token, such branch cannot cause
	 * removal of any tokens.
	 */
	if (br->tk_token == tk) {
		token_branch_unlink(cpp);
		return;
	}

	clang_trace(cl, "%s -> %s",
	    lexer_serialize(lx, br), lexer_serialize(lx, cpp));

	cpp->tk_token = tk;
	token_branch_link(br, cpp);
	*VECTOR_LAST(cl->cl_branches) = cpp;
}

static void
clang_branch_leave(struct clang *cl, struct lexer *lx, struct token *cpp,
    struct token *tk)
{
	struct token *br;

	/* Silently ignore broken branch. */
	if (VECTOR_EMPTY(cl->cl_branches)) {
		token_branch_unlink(cpp);
		return;
	}
	br = *VECTOR_LAST(cl->cl_branches);

	/*
	 * Discard branches hanging of the same token, such branch cannot cause
	 * removal of any tokens.
	 */
	if (br->tk_token == tk) {
		struct token *pv;

		clang_trace(cl, "%s -> %s, discard empty branch",
		    lexer_serialize(lx, br), lexer_serialize(lx, cpp));

		/*
		 * Prevent the previous branch from being exhausted if we're
		 * about to link it again below.
		 */
		pv = br->tk_branch.br_pv;
		if (pv != NULL)
			br->tk_branch.br_pv = NULL;
		token_branch_unlink(br);

		/*
		 * If this is an empty else branch, try to link with the
		 * previous one instead.
		 */
		br = pv;
	}

	if (br != NULL) {
		cpp->tk_token = tk;
		token_branch_link(br, cpp);
		clang_trace(cl, "%s -> %s",
		    lexer_serialize(lx, br),
		    lexer_serialize(lx, cpp));
	} else {
		token_branch_unlink(cpp);
	}

	VECTOR_POP(cl->cl_branches);
}

/*
 * Purge pending broken branches.
 */
static void
clang_branch_purge(struct clang *cl, struct lexer *lx)
{
	while (!VECTOR_EMPTY(cl->cl_branches)) {
		struct token *pv, *tk;

		tk = *VECTOR_POP(cl->cl_branches);
		do {
			pv = tk->tk_branch.br_pv;
			clang_trace(cl, "broken branch: %s%s%s",
			    lexer_serialize(lx, tk),
			    pv ? " -> " : "",
			    pv ? lexer_serialize(lx, pv) : "");
			while (token_branch_unlink(tk) == 0)
				continue;
			tk = pv;
		} while (tk != NULL);
	}
}

static void
token_branch_link(struct token *src, struct token *dst)
{
	src->tk_branch.br_nx = dst;
	dst->tk_branch.br_pv = src;
}
