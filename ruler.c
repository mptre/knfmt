#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

struct ruler_indent {
	struct doc			*ri_dc;
	unsigned int			 ri_rd;
	int				 ri_indent;
	TAILQ_ENTRY(ruler_indent)	 ri_entry;
};

TAILQ_HEAD(ruler_indent_list, ruler_indent);

struct ruler_datum {
	struct doc	*rd_dc;
	unsigned int	 rd_indent;
	unsigned int	 rd_len;
	unsigned int	 rd_nspaces;
};

static void	ruler_reset(struct ruler *);
static void	ruler_reset_indent(struct ruler *);

static int		minimize(const struct ruler_column *);
static unsigned int	tabalign(unsigned int);

/*
 * Initialize ruler, the given length is interpreted as follows:
 *
 *     >0    Maximum alignment, the aligment for all columns will be less or
 *           equal to this length.
 *      0    Use the alignment necessary to align all columns.
 *     <0    Fixed alignment, unconditionally align all columns to this length
 *           with its sign inverted.
 */
void
ruler_init(struct ruler *rl, int len)
{
	memset(rl, 0, sizeof(*rl));
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

	ruler_reset_indent(rl);
}

/*
 * Insert a new datum, indicating that this line must be aligned after the given
 * token with any preceeding or following line(s).
 *
 * The caller is expected to call ruler_exec() once all datums have been emitted
 * in order to insert the alignment in the document.
 */
void
__ruler_insert(struct ruler *rl, const struct token *tk, struct doc *dc,
    unsigned int col, unsigned int len, unsigned int nspaces, const char *fun,
    int lno)
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

	rd->rd_indent = 1;
	rd->rd_dc = __doc_alloc(DOC_ALIGN, dc, rd->rd_indent, fun, lno);
	rd->rd_len = len;
	rd->rd_nspaces = nspaces;

	if (rd->rd_len > rc->rc_len)
		rc->rc_len = rd->rd_len;
	if (rd->rd_nspaces > rc->rc_nspaces)
		rc->rc_nspaces = rd->rd_nspaces;
	if (token_has_tabs(tk))
		rc->rc_ntabs++;
}

/*
 * Returns an indent document which will cause everything that does not fit on
 * the current line to be aligned with the column on the previous line. Used to
 * align long declarations:
 *
 * 	int x, y,
 * 	    z;
 */
struct doc *
__ruler_indent(struct ruler *rl, struct doc *dc, int indent, const char *fun,
    int lno)
{
	struct ruler_column *rc;
	struct ruler_indent *ri;

	if (rl == NULL)
		goto err;

	/* Not applicable to fixed alignment. */
	assert(rl->rl_len == 0);

	if (rl->rl_columns.b_len == 0)
		goto err;
	rc = &rl->rl_columns.b_ptr[0];
	if (rc->rc_datums.b_len == 0)
		goto err;

	if (rl->rl_indent == NULL) {
		rl->rl_indent = malloc(sizeof(*rl->rl_indent));
		if (rl->rl_indent == NULL)
			err(1, NULL);
		TAILQ_INIT(rl->rl_indent);
	}
	ri = calloc(1, sizeof(*ri));
	if (ri == NULL)
		err(1, NULL);
	ri->ri_rd = rc->rc_datums.b_len - 1;
	ri->ri_indent = indent;
	ri->ri_dc = __doc_alloc(DOC_INDENT, dc, 0, fun, lno);
	TAILQ_INSERT_TAIL(rl->rl_indent, ri, ri_entry);
	return ri->ri_dc;

err:
	return __doc_alloc(DOC_CONCAT, dc, 0, fun, lno);
}

void
ruler_exec(struct ruler *rl)
{
	struct ruler_indent *ri;
	size_t i, j;
	int fixedlen = rl->rl_len;
	unsigned int maxlen;

	for (i = 0; i < rl->rl_columns.b_len; i++) {
		struct ruler_column *rc = &rl->rl_columns.b_ptr[i];

		if (rc->rc_ntabs == 0 && fixedlen == 0)
			continue;

		if (fixedlen != 0) {
			maxlen = fixedlen;
		} else {
			maxlen = rc->rc_len;
			if (!minimize(rc)) {
				/* Ceil the longest datum to multiple of 8. */
				maxlen = tabalign(maxlen);
			}
		}

		for (j = 0; j < rc->rc_datums.b_len; j++) {
			struct ruler_datum *rd = &rc->rc_datums.b_ptr[j];
			unsigned int indent;

			if (fixedlen < 0) {
				doc_set_indent(rd->rd_dc, -fixedlen);
				continue;
			}
			if (rd->rd_len > maxlen) {
				assert(fixedlen > 0);
				doc_set_indent(rd->rd_dc, 0);
				continue;
			}

			indent = maxlen - rd->rd_len;
			if (indent % 8 > 0)
				indent = tabalign(indent);
			indent += rc->rc_nspaces - rd->rd_nspaces;
			rd->rd_indent = indent;
			doc_set_indent(rd->rd_dc, indent);
		}
	}

	if (rl->rl_indent == NULL)
		goto out;
	TAILQ_FOREACH(ri, rl->rl_indent, ri_entry) {
		const struct ruler_column *rc = &rl->rl_columns.b_ptr[0];
		const struct ruler_datum *rd = &rc->rc_datums.b_ptr[ri->ri_rd];
		unsigned int indent;

		if (rc->rc_ntabs == 0) {
			indent = rd->rd_len + rd->rd_indent;
		} else {
			indent = rc->rc_len;
			if (!minimize(rc))
				indent = tabalign(indent);
			indent += rc->rc_nspaces - rd->rd_nspaces;
		}
		doc_set_indent(ri->ri_dc, ri->ri_indent * indent);
	}

out:
	/* Reset the ruler paving the way for reuse. */
	ruler_reset(rl);
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

/*
 * Ceil to multiple of 8.
 */
static unsigned int
tabalign(unsigned int len)
{
	return len + (8 - (len % 8));
}

static void
ruler_reset(struct ruler *rl)
{
	size_t i;

	for (i = 0; i < rl->rl_columns.b_len; i++) {
		struct ruler_column *rc = &rl->rl_columns.b_ptr[i];

		free(rc->rc_datums.b_ptr);
		memset(rc, 0, sizeof(*rc));
	}
	rl->rl_columns.b_len = 0;
	ruler_reset_indent(rl);
}

static void
ruler_reset_indent(struct ruler *rl)
{
	struct ruler_indent *ri;

	if (rl->rl_indent == NULL)
		return;

	while ((ri = TAILQ_FIRST(rl->rl_indent)) != NULL) {
		TAILQ_REMOVE(rl->rl_indent, ri, ri_entry);
		free(ri);
	}
	free(rl->rl_indent);
	rl->rl_indent = NULL;
}
