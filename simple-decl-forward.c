#include "simple-decl-forward.h"

#include <err.h>

#include "libks/arena.h"
#include "libks/vector.h"

#include "lexer.h"
#include "token.h"

struct simple_decl_forward {
	VECTOR(struct decl_forward)	 decls;
	struct lexer			*lx;
	struct token			*after;
};

struct decl_forward {
	struct token	*beg;
	struct token	*ident;
	struct token	*semi;
};

struct simple_decl_forward *
simple_decl_forward_enter(struct lexer *lx, struct arena_scope *s)
{
	struct simple_decl_forward *sd;

	sd = arena_calloc(s, 1, sizeof(*sd));
	if (VECTOR_INIT(sd->decls))
		err(1, NULL);
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
	first_unsorted = VECTOR_FIRST(sd->decls)->beg;
	last_unsorted = VECTOR_LAST(sd->decls)->semi;
	VECTOR_SORT(sd->decls, decl_forward_cmp);
	first_sorted = VECTOR_FIRST(sd->decls)->beg;
	last_sorted = VECTOR_LAST(sd->decls)->semi;
	if (first_unsorted != first_sorted)
		token_move_prefixes(first_unsorted, first_sorted);
	if (last_unsorted != last_sorted)
		token_move_suffixes(last_unsorted, last_sorted);

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
	if (beg->tk_type != TOKEN_STRUCT)
		return 0;

	beg = token_next(beg);
	if (beg->tk_type != TOKEN_IDENT)
		return 0;

	beg = token_next(beg);
	if (beg != end)
		return 0;

	return 1;
}

static struct token *
find_ident(struct token *tk)
{
	if (tk->tk_type == TOKEN_STRUCT)
		return token_next(tk);
	return NULL; /* UNREACHABLE */
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

	df = VECTOR_ALLOC(sd->decls);
	if (df == NULL)
		err(1, NULL);
	df->beg = beg;
	df->ident = find_ident(beg);
	df->semi = semi;

	if (sd->after == NULL)
		sd->after = semi;
}
