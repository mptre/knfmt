#include "ruler.h"

#include "config.h"

#include <assert.h>
#include <limits.h>	/* UINT_MAX */
#include <string.h>

#include "libks/arena-vector.h"
#include "libks/arena.h"
#include "libks/vector.h"

#include "doc.h"
#include "token.h"
#include "util.h"

struct ruler_column {
	VECTOR(struct ruler_datum)	rc_datums;
	size_t				rc_len;
	size_t				rc_nspaces;
	size_t				rc_ntabs;
};

struct ruler_indent {
	struct doc	*ri_dc;
	unsigned int	 ri_rd;
	int		 ri_sign;
};

struct ruler_datum {
	struct doc	*rd_dc;
	struct token	*rd_tk;
	unsigned int	 rd_len;
	unsigned int	 rd_nspaces;
};

struct ruler_length {
	unsigned int	max;
	unsigned int	tabalign;
};

static void	ruler_exec_indent(struct ruler *);
static void	ruler_reset(struct ruler *);

static unsigned int	sense_column_length(struct ruler_column *);
static int		ruler_column_length(const struct ruler *,
    struct ruler_column *, struct ruler_length *);
static unsigned int	count_trailing_spaces(const char *, size_t);

static int	minimize(const struct ruler_column *);

static void
ruler_cleanup(void *arg)
{
	struct ruler *rl = arg;

	ruler_reset(rl);
}

void
ruler_init(struct ruler *rl, unsigned int align, unsigned int flags,
    struct arena_scope *s)
{
	memset(rl, 0, sizeof(*rl));
	ARENA_VECTOR_INIT(s, rl->rl_columns, 1 << 3);
	rl->rl_align = align;
	rl->rl_flags = flags;
	rl->rl_arena.ruler_scope = s;
	arena_cleanup(s, ruler_cleanup, rl);
}

/*
 * Insert a new datum, indicating that this line must be aligned after the given
 * token with any preceding or subsequent line(s).
 *
 * The caller is expected to call ruler_exec() once all datums have been emitted
 * in order to insert the alignment in the document.
 */
void
ruler_insert_impl(struct ruler *rl, struct token *tk, struct doc *dc,
    unsigned int col, unsigned int len, unsigned int nspaces, const char *fun,
    int lno)
{
	struct ruler_column *rc;
	struct ruler_datum *rd;

	while (VECTOR_LENGTH(rl->rl_columns) < col) {
		rc = ARENA_VECTOR_CALLOC(rl->rl_columns);
		ARENA_VECTOR_INIT(rl->rl_arena.ruler_scope, rc->rc_datums,
		    1 << 6);
	}
	rc = &rl->rl_columns[col - 1];

	rd = ARENA_VECTOR_CALLOC(rc->rc_datums);
	rd->rd_dc = doc_alloc_impl(DOC_ALIGN, dc, 1, fun, lno);
	token_ref(tk);
	rd->rd_tk = tk;
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
 * the current line to be aligned with the column on the previous line.
 * Optionally adding an extra amount of indentation. Typically used to align
 * long declarations:
 *
 * 	int x, y,
 * 	    z;
 *
 * The optional cookie argument can be passed to ruler_indent_remove() in order
 * to remove the indentation. As the ruler does not own the returned document,
 * the caller is expected to free it.
 */
struct doc *
ruler_indent_impl(struct ruler *rl, struct doc *dc,
    struct ruler_indent **cookie, int sign, const char *fun, int lno)
{
	struct ruler_column *rc;
	struct ruler_indent *ri;

	if (VECTOR_LENGTH(rl->rl_columns) == 0)
		goto err;
	rc = &rl->rl_columns[0];
	if (VECTOR_LENGTH(rc->rc_datums) == 0)
		goto err;

	if (rl->rl_indent == NULL) {
		ARENA_VECTOR_INIT(rl->rl_arena.ruler_scope, rl->rl_indent,
		    1 << 6);
	}
	ri = ARENA_VECTOR_CALLOC(rl->rl_indent);
	ri->ri_rd = VECTOR_LENGTH(rc->rc_datums) - 1;
	ri->ri_sign = sign;
	ri->ri_dc = doc_alloc_impl(DOC_INDENT, dc, 0, fun, lno);
	if (cookie != NULL)
		*cookie = ri;
	return ri->ri_dc;

err:
	return doc_alloc_impl(DOC_CONCAT, dc, 0, fun, lno);
}

void
ruler_indent_remove(struct ruler *rl, const struct ruler_indent *ri)
{
	if (ri == NULL)
		return;
	VECTOR_POP(rl->rl_indent);
}

void
ruler_exec(struct ruler *rl)
{
	size_t i, j;

	for (i = 0; i < VECTOR_LENGTH(rl->rl_columns); i++) {
		struct ruler_length len;
		struct ruler_column *rc = &rl->rl_columns[i];

		if (ruler_column_length(rl, rc, &len))
			continue;

		for (j = 0; j < VECTOR_LENGTH(rc->rc_datums); j++) {
			struct ruler_datum *rd = &rc->rc_datums[j];

			if (rl->rl_flags & RULER_ALIGN_FIXED) {
				doc_set_indent(rd->rd_dc, len.max);
				continue;
			}
			if (rd->rd_len > len.max) {
				assert(rl->rl_flags & RULER_ALIGN_MAX);
				doc_set_indent(rd->rd_dc, 0);
				continue;
			}

			doc_set_align(rd->rd_dc, &(struct doc_align){
			    .indent	= len.max - rd->rd_len,
			    .spaces	= rc->rc_nspaces - rd->rd_nspaces,
			    .tabalign	= len.tabalign,
			});
		}
	}

	ruler_exec_indent(rl);

	/* Reset the ruler paving the way for reuse. */
	ruler_reset(rl);
}

unsigned int
ruler_get_column_count(const struct ruler *rl)
{
	return VECTOR_LENGTH(rl->rl_columns);
}

static void
ruler_exec_indent(struct ruler *rl)
{
	struct ruler_length len = {0};
	struct ruler_column *rc;
	size_t i;

	if (rl->rl_indent == NULL)
		return;

	assert(VECTOR_LENGTH(rl->rl_columns) == 1);
	rc = &rl->rl_columns[0];
	(void)ruler_column_length(rl, rc, &len);
	if (len.tabalign)
		len.max = (len.max + 8 - 1) & ~0x7u;

	for (i = 0; i < VECTOR_LENGTH(rl->rl_indent); i++) {
		struct ruler_indent *ri = &rl->rl_indent[i];
		const struct ruler_datum *rd = &rc->rc_datums[ri->ri_rd];
		unsigned int indent;

		if (len.max > 0)
			indent = len.max + (rc->rc_nspaces - rd->rd_nspaces);
		else
			indent = rd->rd_len + 1;
		if (ri->ri_sign > 0)
			doc_set_indent(ri->ri_dc, indent);
		else
			doc_set_dedent(ri->ri_dc, indent);
	}
}

static void
ruler_reset(struct ruler *rl)
{
	while (!VECTOR_EMPTY(rl->rl_columns)) {
		struct ruler_column *rc = VECTOR_POP(rl->rl_columns);
		size_t i;

		for (i = 0; i < VECTOR_LENGTH(rc->rc_datums); i++)
			token_rele(rc->rc_datums[i].rd_tk);
	}

	VECTOR_FREE(rl->rl_indent);
}

/*
 * Sense effective number of spaces when tabs are used for alignment.
 * Redundant spaces can be present after removing a pointer type from
 * declarations. Such spaces are considered harmless, otherwise needless
 * formatting churn would be introduced.
 */
static unsigned int
sense_column_spaces(const struct ruler_column *rc)
{
	unsigned int nspaces = rc->rc_nspaces;
	unsigned int i;

	if (rc->rc_ntabs == 0)
		return nspaces;

	for (i = 0; i < VECTOR_LENGTH(rc->rc_datums); i++) {
		const struct ruler_datum *rd = &rc->rc_datums[i];
		struct token *suffix;
		unsigned int n;

		if (!token_has_tabs(rd->rd_tk))
			continue;
		suffix = token_find_suffix_spaces(rd->rd_tk);
		if (suffix == NULL)
			continue;

		n = count_trailing_spaces(suffix->tk_str,
		    suffix->tk_len);
		if (n > nspaces)
			nspaces = n;
	}

	return nspaces;
}

static unsigned int
sense_datum_width(const struct ruler_datum *rd, unsigned int nspaces)
{
	const struct token *tk = rd->rd_tk;
	const struct token *nx;
	unsigned int alignment_width, datum_width;

	if (token_has_suffix(tk, TOKEN_COMMENT))
		return 0;

	nx = token_next(tk);
	if (nx == NULL || token_cmp(tk, nx) != 0)
		return 0;
	/*
	 * <tab> struct sss <tab> <space> *
	 * ^datum_width---^             | |
	 * ^alignment_width-------------^ |
	 * ^nx->tk_cno--------------------^
	 */
	datum_width = colwidth(tk->tk_str, tk->tk_len, tk->tk_cno);
	alignment_width = nx->tk_cno - (nspaces - rd->rd_nspaces);
	if (datum_width >= alignment_width)
		return 0;
	return alignment_width;
}

static const struct ruler_datum *
sense_column_longest_datum(const struct ruler_column *rc)
{
	const struct ruler_datum *rd = NULL;
	unsigned int i;

	for (i = 0; i < VECTOR_LENGTH(rc->rc_datums); i++) {
		const struct ruler_datum *candidate = &rc->rc_datums[i];
		if (rd == NULL || candidate->rd_len > rd->rd_len)
			rd = candidate;
	}
	assert(rd != NULL);

	return rd;
}

static unsigned int
sense_column_length(struct ruler_column *rc)
{
	size_t i;
	unsigned int alignment_width, effective_nspaces, effective_width, len, width;

	assert(!VECTOR_EMPTY(rc->rc_datums));

	effective_nspaces = sense_column_spaces(rc);

	/*
	 * Since all datums are expected to be of same width, operate on the
	 * first one.
	 */
	effective_width = sense_datum_width(&rc->rc_datums[0],
	    effective_nspaces);
	if (effective_width == 0)
		return 0;

	for (i = 0; i < VECTOR_LENGTH(rc->rc_datums); i++) {
		struct ruler_datum *rd = &rc->rc_datums[i];

		width = sense_datum_width(rd, effective_nspaces);
		if (width == 0 || width != effective_width)
			return 0;
	}

	/* Commit potentially changed number of spaces. */
	rc->rc_nspaces = effective_nspaces;

	/*
	 * Column is aligned, calculate the wanted column length including
	 * alignment.
	 *
	 * <tab> struct sss <tab> <space> *
	 * |     ^rd_len--^             |
	 * ^width---------^             |
	 * ^effective_width-------------^
	 */
	const struct ruler_datum *longest_datum =
	    sense_column_longest_datum(rc);
	const struct token *tk = longest_datum->rd_tk;
	width = colwidth(tk->tk_str, tk->tk_len, tk->tk_cno);
	alignment_width = effective_width - width;
	len = longest_datum->rd_len + alignment_width;
	return len;
}

static int
ruler_column_length(const struct ruler *rl, struct ruler_column *rc,
    struct ruler_length *len)
{
	unsigned int maxlen = 0;
	unsigned int tabalign = rl->rl_flags & RULER_ALIGN_TABS;

	if (rl->rl_flags & RULER_ALIGN_MIN) {
		maxlen = rc->rc_len + 1;
	} else if (rl->rl_flags & RULER_ALIGN_MAX) {
		maxlen = rl->rl_align;
	} else if (rl->rl_flags & RULER_ALIGN_FIXED) {
		maxlen = rl->rl_align;
	} else if (rl->rl_flags & RULER_ALIGN_SENSE) {
		maxlen = sense_column_length(rc);
		if (maxlen == 0) {
			if (rc->rc_ntabs > 0 &&
			    rc->rc_ntabs >= VECTOR_LENGTH(rc->rc_datums) / 4)
				goto tabs;
			return 1;
		}
		tabalign = rc->rc_ntabs > 0;
	} else if (rl->rl_flags & RULER_ALIGN_TABS) {
tabs:
		maxlen = rc->rc_len;
		if (!minimize(rc))
			maxlen++;
		tabalign = 1;
	}

	len->max = maxlen;
	len->tabalign = tabalign;
	return 0;
}

static unsigned int
count_trailing_spaces(const char *str, size_t len)
{
	unsigned int nspaces = 0;

	for (; len > 0 && str[len - 1] == ' '; len--)
		nspaces++;
	return nspaces;
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
