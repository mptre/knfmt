#include "simple-decl-forward.h"

#include "config.h"

#include <err.h>

#include "libks/arena.h"
#include "libks/vector.h"

#include "lexer.h"
#include "options.h"
#include "token.h"

struct simple_decl_forward {
	VECTOR(struct decl_forward)	 decls;
	const struct options		*op;
	struct lexer			*lx;
	struct token			*after;
};

struct decl_forward {
	struct token	*beg;
	struct token	*ident;
	struct token	*semi;
};

struct simple_decl_forward *
simple_decl_forward_enter(struct lexer *lx, struct arena_scope *eternal_scope,
    const struct options *op)
{
	struct simple_decl_forward *sd;

	sd = arena_calloc(eternal_scope, 1, sizeof(*sd));
	if (VECTOR_INIT(sd->decls))
		err(1, NULL);
	sd->op = op;
	sd->lx = lx;
	return sd;
}

static int
decl_forward_cmp(const struct decl_forward *a, const struct decl_forward *b)
{
	return token_strcmp(a->ident, b->ident);
}

static void
simple_decl_forward_reset(struct simple_decl_forward *sd)
{
	VECTOR_CLEAR(sd->decls);
	sd->after = NULL;
}

static struct token *
first_token(struct simple_decl_forward *sd)
{
	static struct token fallback;
	struct decl_forward *df;

	df = VECTOR_FIRST(sd->decls);
	return df != NULL ? df->beg : &fallback;
}

static struct token *
last_token(struct simple_decl_forward *sd)
{
	static struct token fallback;
	struct decl_forward *df;

	df = VECTOR_LAST(sd->decls);
	return df != NULL ? df->semi : &fallback;
}

void
simple_decl_forward_leave(struct simple_decl_forward *sd)
{
	struct token *after = sd->after;
	struct token *first_sorted, *first_unsorted, *last_sorted,
		     *last_unsorted;
	unsigned int i;

	if (VECTOR_LENGTH(sd->decls) < 2)
		goto out;

	/* Preserve prefixes/suffixes. */
	first_unsorted = first_token(sd);
	last_unsorted = last_token(sd);
	VECTOR_SORT(sd->decls, decl_forward_cmp);
	first_sorted = first_token(sd);
	last_sorted = last_token(sd);
	if (first_unsorted != first_sorted)
		token_move_prefixes(first_unsorted, first_sorted);
	if (last_unsorted != last_sorted)
		token_move_suffixes_if(last_unsorted, last_sorted, TOKEN_SPACE);

	for (i = 0; i < VECTOR_LENGTH(sd->decls); i++) {
		struct decl_forward *df = &sd->decls[i];
		struct token *tk = df->beg;

		for (;;) {
			struct token *nx;

			nx = token_next(tk);
			after = lexer_move_after(sd->lx, after, tk);
			if (tk == df->semi)
				break;
			tk = nx;
		}
	}

out:
	simple_decl_forward_reset(sd);
}

void
simple_decl_forward_free(struct simple_decl_forward *sd)
{
	if (sd == NULL)
		return;
	VECTOR_FREE(sd->decls);
}

static int
is_forward_decl(const struct token *beg, const struct token *end)
{
	const int token_types[2][6] = {
		{ TOKEN_TYPEDEF, TOKEN_STRUCT, TOKEN_IDENT, TOKEN_IDENT,
		  TOKEN_SEMI, TOKEN_NONE },
		{ TOKEN_STRUCT, TOKEN_IDENT, TOKEN_SEMI, TOKEN_NONE },
	};
	int n = sizeof(token_types) / sizeof(token_types[0]);
	int i;

	for (i = 0; i < n; i++) {
		const struct token *tk = beg;
		int j;

		for (j = 0; token_types[i][j] != TOKEN_NONE; j++) {
			if (tk->tk_type != token_types[i][j])
				break;
			if (tk == end)
				return 1;
			tk = token_next(tk);
		}
	}

	return 0;
}

static struct token *
find_ident(struct token *tk)
{
	if (tk->tk_type == TOKEN_TYPEDEF)
		tk = token_next(tk);
	if (tk->tk_type == TOKEN_STRUCT)
		return token_next(tk);
	return NULL; /* UNREACHABLE */
}

static int
is_covered_by_diff(struct token *beg, struct token *end)
{
	struct token *tk = beg;

	for (;;) {
		if ((tk->tk_flags & TOKEN_FLAG_DIFF) == 0)
			return 0;
		if (tk == end)
			break;
		tk = token_next(tk);
	}
	return 1;
}

void
simple_decl_forward(struct simple_decl_forward *sd, struct token *beg,
    struct token *semi)
{
	struct decl_forward *df;

	if (!is_forward_decl(beg, semi)) {
		simple_decl_forward_leave(sd);
		return;
	}
	if (sd->op->diffparse && !is_covered_by_diff(beg, semi))
		return;

	df = VECTOR_ALLOC(sd->decls);
	if (df == NULL)
		err(1, NULL);
	df->beg = beg;
	df->ident = find_ident(beg);
	df->semi = semi;

	if (sd->after == NULL)
		sd->after = semi;
}
