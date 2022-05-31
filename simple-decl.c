#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

#ifdef HAVE_UTHASH
#  include <uthash.h>
#else
#  include "compat-uthash.h"
#endif

struct token_range {
	struct token	*tr_beg;
	struct token	*tr_end;
};

#define TOKEN_RANGE_FOREACH(var, tr, tvar)				\
	for ((var) = (tr)->tr_beg, (tvar) = TAILQ_NEXT((var), tk_entry);\
	    (var) != NULL;						\
	    (var) = (var) == (tr)->tr_end ? NULL : (tvar),		\
	    (tvar) = (var) ? TAILQ_NEXT((var), tk_entry) : NULL)

struct decl_hash {
	char	*dh_ptr;
	size_t	 dh_len;
};

static void	decl_hash_init(struct decl_hash *, const struct token_range *);
static void	decl_hash_free(struct decl_hash *);

struct decl_var {
	struct token_range	 dv_ident;
	struct token		*dv_sort;
	struct token		*dv_delim;
};

static int	decl_var_cmp(const void *, const void *);

struct decl_type {
	struct token_range	dt_tr;
	unsigned int		dt_nrejects;
	TAILQ_ENTRY(decl_type)	dt_entry;
};

TAILQ_HEAD(decl_type_list, decl_type);

struct decl_var_list {
	struct decl_var		*dl_ptr;
	size_t			 dl_len;
	size_t			 dl_siz;

	struct token_range	 dl_type;
	struct token		*dl_semi;

	struct decl_hash	 dl_hash;
	UT_hash_handle		 dl_hh;
};

struct simple_decl {
	/* Type declarations to remove. */
	struct decl_type_list	 sd_decls;
	/* Variable declarations grouped by type. */
	struct decl_var_list	*sd_vars;

	struct decl_type	*sd_dt;
	struct decl_var_list	*sd_dl;
	struct decl_var		*sd_dv;

	struct lexer		*sd_lx;
	const struct config	*sd_cf;
};

static struct decl_var	*simple_decl_var_alloc(struct simple_decl *);
static struct decl_var	*simple_decl_var_end(struct simple_decl *,
    struct token *);
static void		 simple_decl_var_pop(struct simple_decl *);

static int	isident(const struct token_range *);

#define simple_trace(sd, fmt, ...) do {					\
	if (TRACE((sd)->sd_cf))						\
		__simple_trace(__func__, (fmt), __VA_ARGS__);		\
} while (0)
static void	__simple_trace(const char *, const char *, ...)
	__attribute__((__format__(printf, 2, 3)));

struct simple_decl *
simple_decl_enter(struct lexer *lx, const struct config *cf)
{
	struct simple_decl *sd;

	sd = calloc(1, sizeof(*sd));
	if (sd == NULL)
		err(1, NULL);
	TAILQ_INIT(&sd->sd_decls);
	sd->sd_lx = lx;
	sd->sd_cf = cf;
	return sd;
}

void
simple_decl_leave(struct simple_decl *sd)
{
	struct decl_var_list *dl;
	struct decl_type *dt;
	struct token *tmp;

	for (dl = sd->sd_vars; dl != NULL; dl = dl->dl_hh.next) {
		struct token *after = dl->dl_semi;
		struct token *cp, *tk;
		size_t i;

		if (dl->dl_len == 0)
			continue;

		/* Create new type declaration. */
		TOKEN_RANGE_FOREACH(tk, &dl->dl_type, tmp) {
			after = lexer_copy_after(sd->sd_lx, after, tk);
			/* Intentionally ignore prefixes. */
			token_list_copy(&tk->tk_suffixes, &after->tk_suffixes);
		}

		/* Sort the variables in alphabetical order. */
		qsort(dl->dl_ptr, dl->dl_len, sizeof(*dl->dl_ptr),
		    decl_var_cmp);

		/* Move variables to the new type declaration. */
		for (i = 0; i < dl->dl_len; i++) {
			struct decl_var *dv = &dl->dl_ptr[i];
			struct token *ident;

			if (dv->dv_delim != NULL)
				lexer_remove(sd->sd_lx, dv->dv_delim, 1);
			if (i > 0)
				after = lexer_insert_after(sd->sd_lx, after,
				    TOKEN_COMMA, ",");

			TOKEN_RANGE_FOREACH(ident, &dv->dv_ident, tmp)
				after = lexer_move_after(sd->sd_lx, after,
				    ident);
		}

		cp = lexer_insert_after(sd->sd_lx, after, TOKEN_SEMI, ";");
		if (token_is_moveable(dl->dl_semi)) {
			/* Move line break(s) to the new semicolon. */
			token_list_move(&dl->dl_semi->tk_prefixes,
			    &cp->tk_prefixes);
			token_list_move(&dl->dl_semi->tk_suffixes,
			    &cp->tk_suffixes);
		}
	}

	/* Remove by now empty declarations. */
	TAILQ_FOREACH(dt, &sd->sd_decls, dt_entry) {
		struct token *tk;

		if (dt->dt_nrejects > 0)
			continue;

		TOKEN_RANGE_FOREACH(tk, &dt->dt_tr, tmp)
			lexer_remove(sd->sd_lx, tk, 1);
	}
}

void
simple_decl_free(struct simple_decl *sd)
{
	struct decl_var_list *dl, *tmp;
	struct decl_type *dt;

	if (sd == NULL)
		return;

	while ((dt = TAILQ_FIRST(&sd->sd_decls)) != NULL) {
		TAILQ_REMOVE(&sd->sd_decls, dt, dt_entry);
		free(dt);
	}
	HASH_ITER(dl_hh, sd->sd_vars, dl, tmp) {
		HASH_DELETE(dl_hh, sd->sd_vars, dl);
		free(dl->dl_ptr);
		decl_hash_free(&dl->dl_hash);
		free(dl);
	}
	free(sd);
}

void
simple_decl_type(struct simple_decl *sd, struct token *beg, struct token *end)
{
	struct token_range tr = {
		.tr_beg	= beg,
		.tr_end	= end,
	};
	struct decl_hash dh;
	struct decl_type *dt;
	struct decl_var_list *dl;
	struct decl_var *dv;
	struct token *tk, *tmp;

	TOKEN_RANGE_FOREACH(tk, &tr, tmp) {
		if (!token_is_moveable(tk))
			return;
	}

	/* Any pointer(s) must be part of the identifier and not the type. */
	while (end->tk_type == TOKEN_STAR)
		end = TAILQ_PREV(end, token_list, tk_entry);

	decl_hash_init(&dh, &tr);
	HASH_FIND(dl_hh, sd->sd_vars, dh.dh_ptr, dh.dh_len, dl);
	if (dl == NULL) {
		simple_trace(sd, "new type \"%.*s\"",
		    (int)dh.dh_len, dh.dh_ptr);
		dl = calloc(1, sizeof(*dl));
		if (dl == NULL)
			err(1, NULL);
		dl->dl_type.tr_beg = beg;
		dl->dl_type.tr_end = end;
		dl->dl_hash = dh;
		HASH_ADD_KEYPTR(dl_hh, sd->sd_vars, dh.dh_ptr, dh.dh_len, dl);
	} else {
		decl_hash_free(&dh);
	}
	sd->sd_dl = dl;

	dt = calloc(1, sizeof(*dt));
	if (dt == NULL)
		err(1, NULL);
	dt->dt_tr = tr;
	TAILQ_INSERT_TAIL(&sd->sd_decls, dt, dt_entry);
	sd->sd_dt = dt;

	dv = simple_decl_var_alloc(sd);
	dv->dv_ident.tr_beg = TAILQ_NEXT(end, tk_entry);
}

void
simple_decl_semi(struct simple_decl *sd, struct token *semi)
{
	struct decl_type *dt = sd->sd_dt;
	struct decl_var_list *dl = sd->sd_dl;

	if (sd->sd_dv == NULL)
		return;

	simple_decl_var_end(sd, semi);

	/*
	 * If the declaration is empty, ensure deletion of everything including
	 * the semicolon.
	 */
	if (dt->dt_nrejects == 0)
		dt->dt_tr.tr_end = semi;

	/*
	 * Favor insertion of the merged declaration after the last kept
	 * declaration.
	 */
	if (dl->dl_semi == NULL || dt->dt_nrejects > 0)
		dl->dl_semi = semi;

	simple_trace(sd, "type \"%.*s\", semi %s, rejects %u",
	    (int)dl->dl_hash.dh_len, dl->dl_hash.dh_ptr,
	    token_sprintf(dl->dl_semi), dt->dt_nrejects);

	sd->sd_dt = NULL;
	sd->sd_dl = NULL;
	sd->sd_dv = NULL;
}

void
simple_decl_comma(struct simple_decl *sd, struct token *comma)
{
	struct decl_var *dv;
	struct token *delim = NULL;

	if (sd->sd_dv == NULL)
		return;

	dv = simple_decl_var_end(sd, comma);
	if (dv != NULL && dv->dv_delim == NULL) {
		dv->dv_delim = comma;
	} else if (token_is_moveable(comma)) {
		/*
		 * If the next variable ends up being moved, the preceding
		 * comma must be removed.
		 */
		delim = comma;
	}
	/* Another variable after the comma is expected. */
	dv = simple_decl_var_alloc(sd);
	dv->dv_ident.tr_beg = TAILQ_NEXT(comma, tk_entry);
	dv->dv_delim = delim;
}

static int
decl_var_cmp(const void *p1, const void *p2)
{
	const struct token *tk1 = ((const struct decl_var *)p1)->dv_sort;
	const struct token *tk2 = ((const struct decl_var *)p2)->dv_sort;
	size_t len;

	len = tk1->tk_len < tk2->tk_len ? tk1->tk_len : tk2->tk_len;
	return strncmp(tk1->tk_str, tk2->tk_str, len);
}

static struct decl_var *
simple_decl_var_alloc(struct simple_decl *sd)
{
	struct decl_var_list *dl = sd->sd_dl;
	struct decl_var *dv;

	if (dl->dl_len + 1 >= dl->dl_siz) {
		size_t newsiz;

		newsiz = dl->dl_siz == 0 ? 16 : 2 * dl->dl_siz;
		dl->dl_ptr = reallocarray(dl->dl_ptr, newsiz,
		    sizeof(*dl->dl_ptr));
		if (dl->dl_ptr == NULL)
			err(1, NULL);
		dl->dl_siz = newsiz;
	}

	dv = sd->sd_dv = &dl->dl_ptr[dl->dl_len++];
	memset(dv, 0, sizeof(*dv));
	return dv;
}

static struct decl_var *
simple_decl_var_end(struct simple_decl *sd, struct token *end)
{
	struct decl_var *dv = sd->sd_dv;
	struct decl_var_list *dl = sd->sd_dl;
	struct token *sort, *tmp;

	assert(dv->dv_ident.tr_end == NULL);
	/* The delimiter is not part of the identifier. */
	dv->dv_ident.tr_end = TAILQ_PREV(end, token_list, tk_entry);

	if (!token_is_moveable(end) || !isident(&dv->dv_ident)) {
		simple_decl_var_pop(sd);
		return NULL;
	}

	/* Find the identifier token used while sorting. */
	TOKEN_RANGE_FOREACH(sort, &dv->dv_ident, tmp) {
		if (sort->tk_type == TOKEN_IDENT)
			break;
	}
	dv->dv_sort = sort;

	simple_trace(sd, "type \"%.*s\", ident [%s, %s]",
	    (int)dl->dl_hash.dh_len, dl->dl_hash.dh_ptr,
	    token_sprintf(dv->dv_ident.tr_beg),
	    token_sprintf(dv->dv_ident.tr_end));

	return dv;
}

static void
simple_decl_var_pop(struct simple_decl *sd)
{
	struct decl_type *dt = sd->sd_dt;
	struct decl_var_list *dl = sd->sd_dl;

	assert(dl->dl_len > 0);
	dl->dl_len--;
	sd->sd_dv = NULL;

	if (dt != NULL)
		dt->dt_nrejects++;
}

static void
decl_hash_init(struct decl_hash *dh, const struct token_range *tr)
{
	struct buffer *bf;
	struct token *tk, *tmp;
	unsigned int ntokens = 0;

	bf = buffer_alloc(64);
	tk = tr->tr_beg;
	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		if (ntokens++ > 0)
			buffer_appendc(bf, ' ');
		buffer_append(bf, tk->tk_str, tk->tk_len);
	}
	dh->dh_len = bf->bf_len;
	dh->dh_ptr = buffer_release(bf);
	buffer_free(bf);
}

static void
decl_hash_free(struct decl_hash *dh)
{
	if (dh == NULL)
		return;
	free(dh->dh_ptr);
}

static int
isident(const struct token_range *tr)
{
	struct token *tk, *tmp;
	int nident = 0;

	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		switch (tk->tk_type) {
		case TOKEN_STAR:
			break;
		case TOKEN_IDENT:
			nident++;
			break;
		default:
			return 0;
		}

		if (!token_is_moveable(tk))
			return 0;
	}
	return nident;
}

static void
__simple_trace(const char *fun, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "[S] %s: ", fun);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}
