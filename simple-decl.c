#include "simple-decl.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "buffer.h"
#include "cdefs.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "util.h"
#include "vector.h"

struct token_range {
	struct token	*tr_beg;
	struct token	*tr_end;
};

#define TOKEN_RANGE_FOREACH(var, tr, tvar)				\
	for ((var) = (tr)->tr_beg, (tvar) = token_next((var));		\
	    (var) != NULL;						\
	    (var) = (var) == (tr)->tr_end ? NULL : (tvar),		\
	    (tvar) = (var) ? token_next((var)) : NULL)

static char	*token_range_str(const struct token_range *);

struct decl {
	struct token_range	dc_tr;
	int			dc_nrejects;
};

struct decl_var {
	struct token_range	 dv_ident;
	struct token		*dv_sort;
	struct token		*dv_delim;
};

static int	decl_var_cmp(const struct decl_var *, const struct decl_var *);
static int	decl_var_is_empty(const struct decl_var *);

struct decl_type {
	struct token_range		 dt_tr;
	VECTOR(struct decl_type_vars)	 dt_slots;
	char				*dt_str;

	/*
	 * First stamped semi in dt_slots. Since new type slots lacks a stamped
	 * semi we favor insertion of new types after the first kept one.
	 */
	struct {
		struct token	*tk;
		unsigned int	 slot;
	} dt_after;
};

static void			 decl_type_free(struct decl_type *);
static struct decl_type_vars	*decl_type_slot(struct decl_type *,
    unsigned int);

/*
 * Variables associated with a declaration type.
 */
struct decl_type_vars {
	VECTOR(struct decl_var)	 ds_vars;
	struct token		*ds_semi;
};

struct simple_decl {
	/*
	 * Duplicate type declarations ending up empty due to variables of the
	 * same type being grouped into one declaration.
	 */
	VECTOR(struct decl)	 sd_empty_decls;

	/* All seen type declarations. */
	VECTOR(struct decl_type) sd_types;

	/* Current type declaration. */
	struct decl_type	*sd_dt;

	struct decl_var		 sd_dv;

	struct lexer		*sd_lx;
	const struct options	*sd_op;
};

static struct decl_type	*simple_decl_type_create(struct simple_decl *,
    const char *, const struct token_range *);
static struct decl_type	*simple_decl_type_find(struct simple_decl *,
    const char *);
static struct token	*simple_decl_move_vars(struct simple_decl *,
    struct decl_type *, struct decl_type_vars *, struct token *);

static struct decl_var	*simple_decl_var_init(struct simple_decl *);
static struct decl_var	*simple_decl_var_end(struct simple_decl *,
    struct token *);

static int	classify(const struct token_range *, unsigned int *);

#define simple_trace(sd, fmt, ...) do {					\
	if (trace((sd)->sd_op, 'S'))					\
		tracef('S', __func__, (fmt), __VA_ARGS__);		\
} while (0)

struct simple_decl *
simple_decl_enter(struct lexer *lx, const struct options *op)
{
	struct simple_decl *sd;

	sd = ecalloc(1, sizeof(*sd));
	if (VECTOR_INIT(sd->sd_empty_decls) == NULL)
		err(1, NULL);
	if (VECTOR_INIT(sd->sd_types) == NULL)
		err(1, NULL);
	sd->sd_lx = lx;
	sd->sd_op = op;
	return sd;
}

void
simple_decl_leave(struct simple_decl *sd)
{
	struct lexer *lx = sd->sd_lx;
	struct token *tk, *tmp;
	size_t i;

	for (i = 0; i < VECTOR_LENGTH(sd->sd_types); i++) {
		struct decl_type *dt = &sd->sd_types[i];
		struct token *after = dt->dt_after.tk;
		unsigned int slot;

		for (slot = 0; slot < VECTOR_LENGTH(dt->dt_slots); slot++) {
			struct decl_type_vars *ds = &dt->dt_slots[slot];
			struct token *semi;

			if (VECTOR_LENGTH(ds->ds_vars) == 0)
				continue;

			/* Sort the variables in alphabetical order. */
			VECTOR_SORT(ds->ds_vars, decl_var_cmp);

			/*
			 * Favor insertion after the stamped semi if present,
			 * otherwise continue after the previous type slot.
			 */
			if (ds->ds_semi != NULL)
				after = semi = ds->ds_semi;
			else
				semi = after;
			assert(after->tk_type == TOKEN_SEMI);

			after = simple_decl_move_vars(sd, dt, ds, after);
			/* Move line break(s) to the new semicolon. */
			if (semi != NULL) {
				token_move_suffixes_if(semi, after,
				    TOKEN_SPACE);
			}
		}
	}

	/* Remove by now empty declarations. */
	for (i = 0; i < VECTOR_LENGTH(sd->sd_empty_decls); i++) {
		struct decl *dc = &sd->sd_empty_decls[i];

		TOKEN_RANGE_FOREACH(tk, &dc->dc_tr, tmp)
			lexer_remove(lx, tk, 1);
	}
}

void
simple_decl_free(struct simple_decl *sd)
{
	if (sd == NULL)
		return;

	VECTOR_FREE(sd->sd_empty_decls);

	while (!VECTOR_EMPTY(sd->sd_types)) {
		struct decl_type *dt;

		dt = VECTOR_POP(sd->sd_types);
		while (!VECTOR_EMPTY(dt->dt_slots)) {
			struct decl_type_vars *ds = VECTOR_POP(dt->dt_slots);

			VECTOR_FREE(ds->ds_vars);
		}
		VECTOR_FREE(dt->dt_slots);
		decl_type_free(dt);
	}
	VECTOR_FREE(sd->sd_types);

	free(sd);
}

void
simple_decl_type(struct simple_decl *sd, struct token *beg, struct token *end)
{
	struct token_range tr = {
		.tr_beg	= beg,
		.tr_end	= end,
	};
	struct decl *dc;
	struct decl_var *dv;
	struct token *tk, *tmp;
	char *type;

	TOKEN_RANGE_FOREACH(tk, &tr, tmp) {
		if (!token_is_moveable(tk))
			return;
	}

	type = token_range_str(&tr);
	sd->sd_dt = simple_decl_type_create(sd, type, &tr);
	free(type);

	dv = simple_decl_var_init(sd);
	/* Pointer(s) are part of the variable. */
	while (end->tk_type == TOKEN_STAR)
		end = token_prev(end);
	dv->dv_ident.tr_beg = token_next(end);

	dc = VECTOR_CALLOC(sd->sd_empty_decls);
	if (dc == NULL)
		err(1, NULL);
	dc->dc_tr = tr;
}

void
simple_decl_semi(struct simple_decl *sd, struct token *semi)
{
	struct decl *dc = VECTOR_LAST(sd->sd_empty_decls);

	if (decl_var_is_empty(&sd->sd_dv))
		return;

	simple_decl_var_end(sd, semi);

	/*
	 * If the type declaration is empty, ensure deletion of everything
	 * including the semicolon.
	 */
	if (dc->dc_nrejects == 0)
		dc->dc_tr.tr_end = semi;
	else
		VECTOR_POP(sd->sd_empty_decls);

	simple_decl_var_init(sd);
	sd->sd_dt = NULL;
}

void
simple_decl_comma(struct simple_decl *sd, struct token *comma)
{
	struct decl_var *dv;
	struct token *delim = NULL;

	if (decl_var_is_empty(&sd->sd_dv))
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
	dv = simple_decl_var_init(sd);
	dv->dv_ident.tr_beg = token_next(comma);
	dv->dv_delim = delim;
}

static char *
token_range_str(const struct token_range *tr)
{
	struct buffer *bf;
	struct token *tk, *tmp;
	char *str;
	int ntokens = 0;

	bf = buffer_alloc(64);
	if (bf == NULL)
		err(1, NULL);
	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		if (tk->tk_type == TOKEN_STAR)
			continue;

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		buffer_puts(bf, tk->tk_str, tk->tk_len);
	}
	str = buffer_str(bf);
	buffer_free(bf);
	return str;
}

static int
decl_var_cmp(const struct decl_var *a, const struct decl_var *b)
{
	return token_strcmp(a->dv_sort, b->dv_sort);
}

static int
decl_var_is_empty(const struct decl_var *dv)
{
	return dv->dv_ident.tr_beg == NULL;
}

static void
decl_type_free(struct decl_type *dt)
{
	if (dt == NULL)
		return;
	free(dt->dt_str);
}

static struct decl_type_vars *
decl_type_slot(struct decl_type *dt, unsigned int n)
{
	while (VECTOR_LENGTH(dt->dt_slots) <= n) {
		struct decl_type_vars *ds;

		ds = VECTOR_CALLOC(dt->dt_slots);
		if (ds == NULL)
			err(1, NULL);
		if (VECTOR_INIT(ds->ds_vars) == NULL)
			err(1, NULL);
	}
	return &dt->dt_slots[n];
}

/*
 * Create or find the given type.
 */
static struct decl_type *
simple_decl_type_create(struct simple_decl *sd, const char *type,
    const struct token_range *tr)
{
	struct decl_type *dt;
	struct token *end = NULL;
	struct token *tk, *tmp;

	dt = simple_decl_type_find(sd, type);
	if (dt != NULL)
		return dt;

	dt = VECTOR_CALLOC(sd->sd_types);
	if (dt == NULL)
		err(1, NULL);
	dt->dt_tr = *tr;
	/* Pointer(s) are not part of the type. */
	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		if (tk->tk_type != TOKEN_STAR)
			end = tk;
	}
	assert(end != NULL);
	dt->dt_tr.tr_end = end;

	if (VECTOR_INIT(dt->dt_slots) == NULL)
		err(1, NULL);
	dt->dt_str = estrdup(type);
	simple_trace(sd, "new type \"%s\"", dt->dt_str);
	return dt;
}

static struct decl_type *
simple_decl_type_find(struct simple_decl *sd, const char *type)
{
	size_t i;

	for (i = 0; i < VECTOR_LENGTH(sd->sd_types); i++) {
		struct decl_type *dt = &sd->sd_types[i];

		if (strcmp(dt->dt_str, type) == 0)
			return dt;
	}
	return NULL;
}

static struct token *
simple_decl_move_vars(struct simple_decl *sd, struct decl_type *dt,
    struct decl_type_vars *ds, struct token *after)
{
	struct lexer *lx = sd->sd_lx;
	struct token *tk, *tmp;
	size_t i;

	/* Create new type declaration. */
	TOKEN_RANGE_FOREACH(tk, &dt->dt_tr, tmp)
		after = lexer_copy_after(lx, after, tk);

	/* Move variables to the new type declaration. */
	for (i = 0; i < VECTOR_LENGTH(ds->ds_vars); i++) {
		struct decl_var *dv = &ds->ds_vars[i];
		struct token *ident;

		if (dv->dv_delim != NULL)
			lexer_remove(lx, dv->dv_delim, 1);
		if (i > 0)
			after = lexer_insert_after(lx, after, TOKEN_COMMA, ",");

		TOKEN_RANGE_FOREACH(ident, &dv->dv_ident, tmp)
			after = lexer_move_after(lx, after, ident);
	}

	return lexer_insert_after(lx, after, TOKEN_SEMI, ";");
}

static struct decl_var *
simple_decl_var_init(struct simple_decl *sd)
{
	memset(&sd->sd_dv, 0, sizeof(sd->sd_dv));
	return &sd->sd_dv;
}

static struct decl_var *
simple_decl_var_end(struct simple_decl *sd, struct token *end)
{
	struct decl *dc = VECTOR_LAST(sd->sd_empty_decls);
	struct decl_type *dt = sd->sd_dt;
	struct decl_var *dv = &sd->sd_dv;
	struct decl_var *dst;
	struct decl_type_vars *ds;
	struct token *sort, *tmp;
	unsigned int slot;
	int rejected = 0;

	assert(dv->dv_ident.tr_end == NULL);
	/* The delimiter is not part of the identifier. */
	dv->dv_ident.tr_end = token_prev(end);
	if (!classify(&dv->dv_ident, &slot) || !token_is_moveable(end)) {
		dc->dc_nrejects++;
		rejected = 1;
	}

	/*
	 * Allocate the slot even if the variable is rejected since there could
	 * be other variables of the same type that we want to place after this
	 * rejected declaration.
	 */
	ds = decl_type_slot(sd->sd_dt, slot);

	/*
	 * Favor insertion of the merged declaration after the last kept
	 * declaration.
	 */
	if (end->tk_type == TOKEN_SEMI &&
	    (ds->ds_semi == NULL || dc->dc_nrejects > 0)) {
		ds->ds_semi = end;
		if (slot <= dt->dt_after.slot || dt->dt_after.tk == NULL) {
			dt->dt_after.tk = end;
			dt->dt_after.slot = slot;
		}
	}

	if (rejected)
		return NULL;

	/* Find the identifier token used while sorting. */
	TOKEN_RANGE_FOREACH(sort, &dv->dv_ident, tmp) {
		if (sort->tk_type == TOKEN_IDENT)
			break;
	}
	dv->dv_sort = sort;

	dst = VECTOR_CALLOC(ds->ds_vars);
	if (dst == NULL)
		err(1, NULL);
	*dst = *dv;

	simple_trace(sd, "type \"%s\", slot %u, ident %s",
	    sd->sd_dt->dt_str, slot, lexer_serialize(sd->sd_lx, dv->dv_sort));

	return dst;
}

static int
classify(const struct token_range *tr, unsigned int *slot)
{
	struct token *tk, *tmp;
	unsigned int nident = 0;
	unsigned int nstars = 0;

	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		switch (tk->tk_type) {
		case TOKEN_STAR:
			nstars++;
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
	if (nident == 0)
		return 0;

	*slot = nstars;
	return 1;
}
