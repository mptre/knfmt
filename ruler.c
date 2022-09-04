#include "ruler.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "doc.h"
#include "token.h"
#include "vector.h"

struct ruler_column {
	VECTOR(struct ruler_datum)	rc_datums;
	size_t				rc_len;
	size_t				rc_nspaces;
	size_t				rc_ntabs;
};

struct ruler_indent {
	struct doc	*ri_dc;
	unsigned int	 ri_rd;
	int		 ri_indent;
};

struct ruler_datum {
	struct doc	*rd_dc;
	unsigned int	 rd_indent;
	unsigned int	 rd_len;
	unsigned int	 rd_nspaces;
};

static void	ruler_reset(struct ruler *);

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
	if (VECTOR_INIT(rl->rl_columns) == NULL)
		err(1, NULL);
	rl->rl_len = len;
}

void
ruler_free(struct ruler *rl)
{
	while (!VECTOR_EMPTY(rl->rl_columns)) {
		struct ruler_column *rc = VECTOR_POP(rl->rl_columns);

		VECTOR_FREE(rc->rc_datums);
	}
	VECTOR_FREE(rl->rl_columns);
	VECTOR_FREE(rl->rl_indent);
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

	while (VECTOR_LENGTH(rl->rl_columns) < col) {
		rc = VECTOR_CALLOC(rl->rl_columns);
		if (rc == NULL)
			err(1, NULL);
		if (VECTOR_INIT(rc->rc_datums) == NULL)
			err(1, NULL);
	}
	rc = &rl->rl_columns[col - 1];

	rd = VECTOR_CALLOC(rc->rc_datums);
	if (rd == NULL)
		err(1, NULL);
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
 *
 * The optional cookie argument can be passed to ruler_remove() in order to
 * remove the indentation. As the ruler does not own the returned document, the
 * caller is expected to free it.
 */
struct doc *
__ruler_indent(struct ruler *rl, struct doc *dc, struct ruler_indent **cookie,
    int indent, const char *fun, int lno)
{
	struct ruler_column *rc;
	struct ruler_indent *ri;

	if (rl == NULL)
		goto err;

	/* Not applicable to fixed alignment. */
	assert(rl->rl_len == 0);

	if (VECTOR_LENGTH(rl->rl_columns) == 0)
		goto err;
	rc = &rl->rl_columns[0];
	if (VECTOR_LENGTH(rc->rc_datums) == 0)
		goto err;

	if (rl->rl_indent == NULL) {
		if (VECTOR_INIT(rl->rl_indent) == NULL)
			err(1, NULL);
	}
	ri = VECTOR_CALLOC(rl->rl_indent);
	if (ri == NULL)
		err(1, NULL);
	ri->ri_rd = VECTOR_LENGTH(rc->rc_datums) - 1;
	ri->ri_indent = indent;
	ri->ri_dc = __doc_alloc(DOC_INDENT, dc, 0, fun, lno);
	if (cookie != NULL)
		*cookie = ri;
	return ri->ri_dc;

err:
	return __doc_alloc(DOC_CONCAT, dc, 0, fun, lno);
}

void
ruler_remove(struct ruler *rl, struct ruler_indent *ri)
{
	if (ri == NULL)
		return;
	VECTOR_POP(rl->rl_indent);
}

void
ruler_exec(struct ruler *rl)
{
	size_t i, j;
	int fixedlen = rl->rl_len;
	unsigned int maxlen;

	for (i = 0; i < VECTOR_LENGTH(rl->rl_columns); i++) {
		struct ruler_column *rc = &rl->rl_columns[i];

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

		for (j = 0; j < VECTOR_LENGTH(rc->rc_datums); j++) {
			struct ruler_datum *rd = &rc->rc_datums[j];
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
	for (i = 0; i < VECTOR_LENGTH(rl->rl_indent); i++) {
		struct ruler_indent *ri = &rl->rl_indent[i];
		const struct ruler_column *rc = &rl->rl_columns[0];
		const struct ruler_datum *rd = &rc->rc_datums[ri->ri_rd];
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

	if (VECTOR_LENGTH(rc->rc_datums) < 2 ||
	    rc->rc_nspaces == 0 || rc->rc_len % 8 > 0)
		return 0;

	/* Find the longest datum with the smallest amount of spaces. */
	for (i = 0; i < VECTOR_LENGTH(rc->rc_datums); i++) {
		const struct ruler_datum *rd = &rc->rc_datums[i];
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
	while (!VECTOR_EMPTY(rl->rl_columns)) {
		struct ruler_column *rc = VECTOR_POP(rl->rl_columns);

		VECTOR_FREE(rc->rc_datums);
	}
	VECTOR_FREE(rl->rl_indent);
}
