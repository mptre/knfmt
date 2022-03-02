#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

struct ruler_datum {
	struct doc	*rd_dc;
	unsigned int	 rd_len;
	unsigned int	 rd_nspaces;
};

static int	isdecl(const struct token *);
static int	isnexttoken(const struct token *, enum token_type);
static int	minimize(const struct ruler_column *);

void
ruler_init(struct ruler *rl, unsigned int len)
{
	size_t i;

	for (i = 0; i < rl->rl_columns.b_len; i++) {
		struct ruler_column *rc = &rl->rl_columns.b_ptr[i];

		free(rc->rc_datums.b_ptr);
		memset(rc, 0, sizeof(*rc));
	}

	rl->rl_columns.b_len = 0;
	rl->rl_len = len;
}

void
ruler_free(struct ruler *rl)
{
	size_t i;

	for (i = 0; i < rl->rl_columns.b_len; i++) {
		struct ruler_column *rc = &rl->rl_columns.b_ptr[i];

		free(rc->rc_datums.b_ptr);
	}
	free(rl->rl_columns.b_ptr);
}

/*
 * Insert a new datum, indicating that this line must be aligned after the given
 * token with any preceeding or following line(s).
 *
 * The caller is expected to call ruler_exec() once all datums have been emitted
 * in order to insert the alignment in the document.
 */
void
ruler_insert(struct ruler *rl, const struct token *tk, struct doc *dc,
    unsigned int col, unsigned int len, unsigned int nspaces)
{
	struct ruler_column *rc;
	struct ruler_datum *rd;

	if (col > rl->rl_columns.b_len) {
		struct ruler_column *ptr = rl->rl_columns.b_ptr;

		ptr = reallocarray(ptr, col, sizeof(*ptr));
		if (ptr == NULL)
			err(1, NULL);
		rl->rl_columns.b_ptr = ptr;
		rl->rl_columns.b_len = col;
		memset(&rl->rl_columns.b_ptr[col - 1], 0, sizeof(*ptr));
	}
	rc = &rl->rl_columns.b_ptr[col - 1];

	if (rc->rc_datums.b_len >= rc->rc_datums.b_siz) {
		struct ruler_datum *ptr = rc->rc_datums.b_ptr;

		if (rc->rc_datums.b_siz == 0)
			rc->rc_datums.b_siz = 8;
		ptr = reallocarray(ptr, rc->rc_datums.b_siz, 2 * sizeof(*ptr));
		if (ptr == NULL)
			err(1, NULL);
		rc->rc_datums.b_ptr = ptr;
		rc->rc_datums.b_siz *= 2;
	}
	rd = &rc->rc_datums.b_ptr[rc->rc_datums.b_len++];
	memset(rd, 0, sizeof(*rd));

	/* No space wanted at all if the following token is a semicolon. */
	if (isnexttoken(tk, TOKEN_SEMI))
		return;

	/* Only a space is wanted for enum/struct/union declarations. */
	if (isdecl(tk))
		goto out;

	rd->rd_len = len;
	rd->rd_nspaces = nspaces;

	if (rd->rd_len > rc->rc_len)
		rc->rc_len = rd->rd_len;
	if (rd->rd_nspaces > rc->rc_nspaces)
		rc->rc_nspaces = rd->rd_nspaces;
	if (token_has_tabs(tk))
		rc->rc_ntabs++;

out:
	rd->rd_dc = doc_alloc_align(1, dc);
}

void
ruler_exec(struct ruler *rl)
{
	size_t i;
	unsigned int fixedlen = rl->rl_len;

	for (i = 0; i < rl->rl_columns.b_len; i++) {
		struct ruler_column *rc = &rl->rl_columns.b_ptr[i];
		size_t j;
		unsigned int maxlen;

		if (rc->rc_ntabs == 0 && fixedlen == 0)
			continue;

		if (fixedlen > 0) {
			maxlen = fixedlen;
		} else {
			maxlen = rc->rc_len;
			if (!minimize(rc)) {
				/* Ceil the longest datum to multiple of 8. */
				maxlen += 8 - (maxlen % 8);
			}
		}

		for (j = 0; j < rc->rc_datums.b_len; j++) {
			struct ruler_datum *rd = &rc->rc_datums.b_ptr[j];
			unsigned int indent;

			if (rd->rd_len == 0)
				continue;

			if (rd->rd_len > maxlen) {
				assert(fixedlen > 0);
				doc_set_indent(rd->rd_dc, 0);
				continue;
			}

			indent = maxlen - rd->rd_len;
			if (indent % 8 > 0)
				indent += 8 - (indent % 8);
			indent += rc->rc_nspaces - rd->rd_nspaces;
			doc_set_indent(rd->rd_dc, indent);
		}
	}

	/* Reset the ruler paving the way for reuse. */
	ruler_init(rl, rl->rl_len);
}

static int
minimize(const struct ruler_column *rc)
{
	size_t i;
	unsigned int minspaces = UINT_MAX;

	if (rc->rc_datums.b_len < 2 ||
	    rc->rc_nspaces == 0 ||
	    rc->rc_len % 8 > 0)
		return 0;

	/* Find the longest datum with the smallest amount of spaces. */
	for (i = 0; i < rc->rc_datums.b_len; i++) {
		const struct ruler_datum *rd = &rc->rc_datums.b_ptr[i];
		unsigned int nspaces;

		if (rd->rd_len < rc->rc_len)
			continue;

		nspaces = rc->rc_nspaces - rd->rd_nspaces;
		if (minspaces == UINT_MAX || nspaces < minspaces)
			minspaces = nspaces;
	}

	/*
	 * If the number of spaces required for the longest datum with smallest
	 * amount of spaces is less than the maximum amount of spaces, other
	 * datums will overlap.
	 */
	return minspaces >= rc->rc_nspaces;
}

static int
isdecl(const struct token *tk)
{
	return token_is_decl(tk, TOKEN_ENUM) ||
	    token_is_decl(tk, TOKEN_STRUCT) ||
	    token_is_decl(tk, TOKEN_UNION);
}

static int
isnexttoken(const struct token *tk, enum token_type type)
{
	tk = TAILQ_NEXT(tk, tk_entry);
	if (tk == NULL)
		return 0;
	return tk->tk_type == type;
}
