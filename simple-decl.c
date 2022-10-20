#include "simple-decl.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"
#include "cdefs.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "vector.h"
#include "util.h"

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

static int	decl_var_cmp(const void *, const void *);
static int	decl_var_empty(const struct decl_var *);

struct decl_type {
	struct token_range		 dt_tr;
	VECTOR(struct decl_var_list)	 dt_slots;
	char				*dt_str;
	UT_hash_handle			 hh;
};

static void			 decl_type_free(struct decl_type *);
static struct decl_var_list	*decl_type_slot(struct decl_type *,
    unsigned int);
struct token			*decl_type_after(struct decl_type *);

struct decl_var_list {
	VECTOR(struct decl_var)	 dl_vars;
	struct token		*dl_semi;
};

struct simple_decl {
	/*
	 * Duplicate type declarations ending up empty due to variables of the
	 * same type being grouped into one declaration.
	 */
	VECTOR(struct decl)	 sd_empty_decls;

	struct decl_type	*sd_types;
	struct decl_type	*sd_dt;

	struct decl_var		 sd_dv;

	struct lexer		*sd_lx;
	const struct options	*sd_op;
};

static struct decl_type	*simple_decl_type_create(struct simple_decl *,
    const char *, const struct token_range *);

static struct decl_var	*simple_decl_var_init(struct simple_decl *);
static struct decl_var	*simple_decl_var_end(struct simple_decl *,
    struct token *);
static int		 simple_decl_var_reject(struct simple_decl *);

static int		isident(const struct token_range *);
static unsigned int	nstars(const struct token_range *);

#define simple_trace(sd, fmt, ...) do {					\
	if (trace((sd)->sd_op, 'S'))					\
		tracef('S', __func__, (fmt), __VA_ARGS__);		\
} while (0)

struct simple_decl *
simple_decl_enter(struct lexer *lx, const struct options *op)
{
	struct simple_decl *sd;

	sd = calloc(1, sizeof(*sd));
	if (sd == NULL)
		err(1, NULL);
	if (VECTOR_INIT(sd->sd_empty_decls) == NULL)
		err(1, NULL);
	sd->sd_lx = lx;
	sd->sd_op = op;
	return sd;
}

void
simple_decl_leave(struct simple_decl *sd)
{
	struct decl_type *dt;
	struct token *tk, *tmp;
	size_t i;

	for (dt = sd->sd_types; dt != NULL; dt = dt->hh.next) {
		struct token *after;
		unsigned int slot;

		/*
		 * The first type slot could lack a stamped semi assuming it's
		 * a new type declaration. Fallback to inserting tokens after
		 * the first stamped semi.
		 */
		after = decl_type_after(dt);

		size_t slen = VECTOR_LENGTH(dt->dt_slots);
		for (slot = 0; slot < slen; slot++) {
			struct decl_var_list *dl = &dt->dt_slots[slot];
			struct token *semi;

			if (VECTOR_LENGTH(dl->dl_vars) == 0)
				continue;

			/*
			 * Favor insertion after the stamped semi if present,
			 * otherwise continue after the previous type slot.
			 */
			if (dl->dl_semi != NULL)
				after = semi = dl->dl_semi;
			else
				semi = after;
			assert(after->tk_type == TOKEN_SEMI);

			/* Create new type declaration. */
			TOKEN_RANGE_FOREACH(tk, &dt->dt_tr, tmp) {
				after = lexer_copy_after(sd->sd_lx, after, tk);
				/* Intentionally ignore prefixes. */
				token_list_copy(&tk->tk_suffixes,
				    &after->tk_suffixes);
			}

			/* Sort the variables in alphabetical order. */
			qsort(dl->dl_vars, VECTOR_LENGTH(dl->dl_vars),
			    sizeof(*dl->dl_vars), decl_var_cmp);

			/* Move variables to the new type declaration. */
			for (i = 0; i < VECTOR_LENGTH(dl->dl_vars); i++) {
				struct decl_var *dv = &dl->dl_vars[i];
				struct token *ident;

				if (dv->dv_delim != NULL)
					lexer_remove(sd->sd_lx, dv->dv_delim,
					    1);
				if (i > 0)
					after = lexer_insert_after(sd->sd_lx,
					    after, TOKEN_COMMA, ",");

				TOKEN_RANGE_FOREACH(ident, &dv->dv_ident, tmp)
					after = lexer_move_after(sd->sd_lx,
					    after, ident);
			}

			after = lexer_insert_after(sd->sd_lx, after,
			    TOKEN_SEMI, ";");

			/* Move line break(s) to the new semicolon. */
			if (semi != NULL)
				token_move_suffixes_if(semi, after,
				    TOKEN_SPACE);
		}
	}

	/* Remove by now empty declarations. */
	for (i = 0; i < VECTOR_LENGTH(sd->sd_empty_decls); i++) {
		struct decl *dc = &sd->sd_empty_decls[i];

		TOKEN_RANGE_FOREACH(tk, &dc->dc_tr, tmp)
			lexer_remove(sd->sd_lx, tk, 1);
	}
}

void
simple_decl_free(struct simple_decl *sd)
{
	struct decl_type *dt, *tmp;

	if (sd == NULL)
		return;

	VECTOR_FREE(sd->sd_empty_decls);

	HASH_ITER(hh, sd->sd_types, dt, tmp) {
		while (!VECTOR_EMPTY(dt->dt_slots)) {
			struct decl_var_list *dl = VECTOR_POP(dt->dt_slots);

			VECTOR_FREE(dl->dl_vars);
		}
		VECTOR_FREE(dt->dt_slots);
		HASH_DELETE(hh, sd->sd_types, dt);
		decl_type_free(dt);
		free(dt);
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

	if (decl_var_empty(&sd->sd_dv))
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

	if (decl_var_empty(&sd->sd_dv))
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
	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		if (tk->tk_type == TOKEN_STAR)
			continue;

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		buffer_puts(bf, tk->tk_str, tk->tk_len);
	}
	buffer_putc(bf, '\0');
	str = buffer_release(bf);
	buffer_free(bf);
	return str;
}

static int
decl_var_cmp(const void *p1, const void *p2)
{
	const struct token *tk1 = ((const struct decl_var *)p1)->dv_sort;
	const struct token *tk2 = ((const struct decl_var *)p2)->dv_sort;
	size_t minlen;
	int cmp;

	minlen = tk1->tk_len < tk2->tk_len ? tk1->tk_len : tk2->tk_len;
	cmp = strncmp(tk1->tk_str, tk2->tk_str, minlen);
	if (cmp != 0)
		return cmp;

	if (tk1->tk_len < tk2->tk_len)
		return -1;
	if (tk1->tk_len > tk2->tk_len)
		return 1;
	return 0;
}

static int
decl_var_empty(const struct decl_var *dv)
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

static struct decl_var_list *
decl_type_slot(struct decl_type *dt, unsigned int n)
{
	while (VECTOR_LENGTH(dt->dt_slots) <= n) {
		struct decl_var_list *dl;

		dl = VECTOR_CALLOC(dt->dt_slots);
		if (dl == NULL)
			err(1, NULL);
		if (VECTOR_INIT(dl->dl_vars) == NULL)
			err(1, NULL);
	}
	return &dt->dt_slots[n];
}

struct token *
decl_type_after(struct decl_type *dt)
{
	size_t slot;

	for (slot = 0; slot < VECTOR_LENGTH(dt->dt_slots); slot++) {
		struct decl_var_list *dl = &dt->dt_slots[slot];

		if (dl->dl_semi != NULL)
			return dl->dl_semi;
	}
	assert(0 && "IMPOSSIBLE");
	return NULL;
}

/*
 * Create or find the given type.
 */
static struct decl_type *
simple_decl_type_create(struct simple_decl *sd, const char *type,
    const struct token_range *tr)
{
	struct decl_type *dt;
	struct token *end, *tk, *tmp;

	HASH_FIND_STR(sd->sd_types, type, dt);
	if (dt != NULL)
		return dt;

	dt = calloc(1, sizeof(*dt));
	if (dt == NULL)
		err(1, NULL);

	dt->dt_tr = *tr;
	/* Pointer(s) are not part of the type. */
	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		if (tk->tk_type != TOKEN_STAR)
			end = tk;
	}
	dt->dt_tr.tr_end = end;

	if (VECTOR_INIT(dt->dt_slots) == NULL)
		err(1, NULL);
	dt->dt_str = strdup(type);
	if (dt->dt_str == NULL)
		err(1, NULL);
	HASH_ADD_STR(sd->sd_types, dt_str, dt);
	simple_trace(sd, "new type \"%s\"", dt->dt_str);
	return dt;
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
	struct decl_var *dv = &sd->sd_dv;
	struct decl_var *dst;
	struct decl_var_list *dl;
	struct token *sort, *tmp;
	unsigned int slot;
	int rejected = 0;

	assert(dv->dv_ident.tr_end == NULL);
	/* The delimiter is not part of the identifier. */
	dv->dv_ident.tr_end = token_prev(end);

	/*
	 * Allocate the slot even if the variable is about to be rejected since
	 * there could be other variables of the same type that we want to place
	 * after this rejected declaration.
	 */
	slot = nstars(&dv->dv_ident);
	dl = decl_type_slot(sd->sd_dt, slot);

	if (!token_is_moveable(end) || !isident(&dv->dv_ident))
		rejected = simple_decl_var_reject(sd);

	/*
	 * Favor insertion of the merged declaration after the last kept
	 * declaration.
	 */
	if (end->tk_type == TOKEN_SEMI &&
	    (dl->dl_semi == NULL || dc->dc_nrejects > 0))
		dl->dl_semi = end;

	if (rejected)
		return NULL;

	/* Find the identifier token used while sorting. */
	TOKEN_RANGE_FOREACH(sort, &dv->dv_ident, tmp) {
		if (sort->tk_type == TOKEN_IDENT)
			break;
	}
	dv->dv_sort = sort;

	dst = VECTOR_CALLOC(dl->dl_vars);
	if (dst == NULL)
		err(1, NULL);
	*dst = *dv;

	simple_trace(sd, "type \"%s\", slot %u, ident %s",
	    sd->sd_dt->dt_str, slot, lexer_serialize(sd->sd_lx, dv->dv_sort));

	return dst;
}

static int
simple_decl_var_reject(struct simple_decl *sd)
{
	struct decl *dc = VECTOR_LAST(sd->sd_empty_decls);

	dc->dc_nrejects++;
	return 1;
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

static unsigned int
nstars(const struct token_range *tr)
{
	const struct token *tk, *tmp;
	unsigned int n = 0;

	TOKEN_RANGE_FOREACH(tk, tr, tmp) {
		if (tk->tk_type == TOKEN_STAR)
			n++;
	}
	return n;
}
