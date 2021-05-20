#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

TAILQ_HEAD(doc_list, doc);

struct doc {
	enum doc_type	dc_type;

	int		 dc_lno;
	const char	*dc_fun;

	union {
		struct doc_list	 dc_list;
		struct doc	*dc_doc;
		const char	*dc_str;
		int		 dc_int;
	};

	int	dc_indent;
	size_t	dc_len;

	TAILQ_ENTRY(doc)	dc_entry;
};

struct doc_state {
	const struct config	*st_cf;
	struct buffer		*st_bf;

	enum {
		BREAK,
		MUNGE,
	} st_mode;

	struct {
		int		f_fits;
		unsigned int	f_pos;
		unsigned int	f_ppos;
	} st_fits;

	struct {
		int	i_cur;
		int	i_pre;
		size_t	i_pos;
	} st_indent;

	struct {
		unsigned int	s_nfits;
		unsigned int	s_nfits_cache;
	} st_stats;

	unsigned int	st_pos;
	unsigned int	st_depth;
	unsigned int	st_refit;
	unsigned int	st_line;
	unsigned int	st_noline;
	unsigned int	st_parens;
	int		st_mute;
	unsigned int	st_flags;
#define DOC_STATE_FLAG_WIDTH	0x00000001u
};

static void	doc_exec1(const struct doc *, struct doc_state *);
static int	doc_fits(const struct doc *, struct doc_state *);
static int	doc_fits1(const struct doc *, struct doc_state *);
static void	doc_indent(const struct doc *, struct doc_state *, int);
static void	doc_indent1(const struct doc *, struct doc_state *, int);
static void	doc_print(const struct doc *, struct doc_state *, const char *,
    size_t, int);
static void	doc_trim(const struct doc *, struct doc_state *);
static int	doc_parens(const struct doc_state *);
static int	doc_has_list(const struct doc *);

static struct doc	*__doc_alloc_mute(int, struct doc *, const char *, int);

#define DOC_TRACE(st)	(UNLIKELY((st)->st_cf->cf_verbose >= 2 &&	\
	((st)->st_flags & DOC_STATE_FLAG_WIDTH) == 0))

#define doc_trace(dc, st, fmt, ...) do {				\
	if (DOC_TRACE(st))						\
		__doc_trace((dc), (st), (fmt), __VA_ARGS__);		\
} while (0)
static void	__doc_trace(const struct doc *, const struct doc_state *,
    const char *, ...)
	__attribute__((__format__(printf, 3, 4)));

#define doc_trace_enter(dc, st) do {					\
	if (DOC_TRACE(st))						\
		__doc_trace_enter((dc), (st));				\
} while (0)
static void	__doc_trace_enter(const struct doc *, struct doc_state *);

#define doc_trace_leave(dc, st) do {					\
	if (DOC_TRACE(st))						\
		__doc_trace_leave((dc), (st));				\
} while (0)
static void	__doc_trace_leave(const struct doc *, struct doc_state *);

static char		*docstr(const struct doc *, char *, size_t);
static const char	*indentstr(const struct doc *);
static const char	*statestr(const struct doc_state *, unsigned int,
    char *, size_t);

void
doc_exec(const struct doc *dc, struct buffer *bf, const struct config *cf)
{
	struct doc_state st;

	buffer_reset(bf);
	memset(&st, 0, sizeof(st));
	st.st_cf = cf;
	st.st_bf = bf;
	st.st_mode = BREAK;
	st.st_fits.f_fits = -1;
	doc_exec1(dc, &st);
	buffer_appendc(bf, '\0');

	doc_trace(dc, &st, "%s: nfits %u/%u", __func__,
	    st.st_stats.s_nfits_cache, st.st_stats.s_nfits);
}

unsigned int
doc_width(const struct doc *dc, struct buffer *bf, const struct config *cf)
{
	struct doc_state st;

	buffer_reset(bf);
	memset(&st, 0, sizeof(st));
	st.st_cf = cf;
	st.st_bf = bf;
	st.st_mode = MUNGE;
	st.st_fits.f_fits = -1;
	st.st_flags = DOC_STATE_FLAG_WIDTH;
	doc_exec1(dc, &st);

	return st.st_pos;
}

void
doc_free(struct doc *dc)
{
	if (dc == NULL)
		return;

	switch (dc->dc_type) {
	case DOC_CONCAT:
	case DOC_NOLINE: {
		struct doc *concat;

		while ((concat = TAILQ_FIRST(&dc->dc_list)) != NULL) {
			TAILQ_REMOVE(&dc->dc_list, concat, dc_entry);
			doc_free(concat);
		}
		break;
	}

	case DOC_GROUP:
	case DOC_INDENT:
	case DOC_DEDENT:
		doc_free(dc->dc_doc);
		break;

	case DOC_ALIGN:
	case DOC_LITERAL:
	case DOC_VERBATIM:
	case DOC_LINE:
	case DOC_SOFTLINE:
	case DOC_HARDLINE:
	case DOC_MUTE:
		break;
	}

	free(dc);
}

void
doc_remove(struct doc *dc, struct doc *parent)
{
	assert(doc_has_list(parent));
	TAILQ_REMOVE(&parent->dc_list, dc, dc_entry);
	doc_free(dc);
}

void
doc_set_indent(struct doc *dc, int indent)
{
	dc->dc_indent = indent;
}

void
doc_append(struct doc *dc, struct doc *parent)
{
	if (doc_has_list(parent)) {
		TAILQ_INSERT_TAIL(&parent->dc_list, dc, dc_entry);
	} else {
		assert(parent->dc_doc == NULL);
		parent->dc_doc = dc;
	}
}

struct doc *
__doc_alloc(enum doc_type type, struct doc *parent, const char *fun, int lno)
{
	struct doc *dc;

	dc = calloc(1, sizeof(*dc));
	if (dc == NULL)
		err(1, NULL);
	dc->dc_type = type;
	dc->dc_fun = fun;
	dc->dc_lno = lno;
	if (doc_has_list(dc))
		TAILQ_INIT(&dc->dc_list);
	if (parent != NULL)
		doc_append(dc, parent);

	return dc;
}

struct doc *
__doc_alloc_indent(enum doc_type type, int ind, struct doc *dc,
    const char *fun, int lno)
{
	struct doc *concat, *group, *indent;

	group = __doc_alloc(DOC_GROUP, dc, fun, lno);
	indent = __doc_alloc(type, group, fun, lno);
	indent->dc_indent = ind;
	concat = __doc_alloc(DOC_CONCAT, indent, fun, lno);
	return concat;
}

struct doc *
__doc_literal(const char *str, struct doc *dc, const char *fun, int lno)
{
	struct doc *literal;

	literal = __doc_alloc(DOC_LITERAL, dc, fun, lno);
	literal->dc_str = str;
	literal->dc_len = strlen(str);
	return literal;
}

struct doc *
__doc_token(const struct token *tk, struct doc *dc, enum doc_type type,
    const char *fun, int lno)
{
	struct doc *token;
	struct token *tmp;
	int dangling = 0;

	/*
	 * If the token has dangling tokens, a parent is mandatory as we're
	 * about to append more than one token.
	 */
	if (dc == NULL && token_has_dangling(tk)) {
		dangling = 1;
		dc = doc_alloc(DOC_CONCAT, NULL);
	}

	if (tk->tk_flags & TOKEN_FLAG_UNMUTE)
		__doc_alloc_mute(-1, dc, fun, lno);

	TAILQ_FOREACH(tmp, &tk->tk_prefixes, tk_entry) {
		__doc_token(tmp, dc, DOC_VERBATIM, __func__, __LINE__);
	}

	token = __doc_alloc(type, dc, fun, lno);
	token->dc_str = tk->tk_str;
	token->dc_len = tk->tk_len;

	TAILQ_FOREACH(tmp, &tk->tk_suffixes, tk_entry) {
		__doc_token(tmp, dc, DOC_VERBATIM, __func__, __LINE__);
	}

	/* Mute if we're about to branch. */
	tmp = TAILQ_NEXT(tk, tk_entry);
	if (tmp != NULL && token_is_branch(tmp))
		__doc_alloc_mute(1, dc, fun, lno);

	return dangling ? dc : token;
}

static void
doc_exec1(const struct doc *dc, struct doc_state *st)
{
	doc_trace_enter(dc, st);

	switch (dc->dc_type) {
	case DOC_CONCAT:
	case DOC_NOLINE: {
		struct doc *concat;
		int ndocs = 0;
		int noline = dc->dc_type == DOC_NOLINE;

		/*
		 * If the document has more than one child the cache must be
		 * invalidated since it spans over all children.
		 */
		TAILQ_FOREACH(concat, &dc->dc_list, dc_entry) {
			if (++ndocs > 1)
				break;
		}
		if (ndocs > 1)
			st->st_fits.f_fits = -1;

		if (noline)
			st->st_noline++;
		TAILQ_FOREACH(concat, &dc->dc_list, dc_entry) {
			doc_exec1(concat, st);
		}
		if (noline)
			st->st_noline--;
		break;
	}

	case DOC_GROUP: {
		unsigned int oldmode;

		switch (st->st_mode) {
		case MUNGE:
			if (st->st_refit == 0) {
				doc_exec1(dc->dc_doc, st);
				break;
			}
			/* FALLTHROUGH */
		case BREAK:
			st->st_refit = 0;
			oldmode = st->st_mode;
			st->st_mode = doc_fits(dc, st) ? MUNGE : BREAK;
			doc_exec1(dc->dc_doc, st);
			st->st_mode = oldmode;
			break;
		}
		break;
	}

	case DOC_INDENT:
		if (dc->dc_indent == DOC_INDENT_PARENS)
			st->st_parens++;
		else if (dc->dc_indent == DOC_INDENT_FORCE)
			doc_indent(dc, st, st->st_indent.i_cur);
		else
			st->st_indent.i_cur += dc->dc_indent;
		doc_exec1(dc->dc_doc, st);
		if (dc->dc_indent == DOC_INDENT_PARENS) {
			st->st_parens--;
		} else if (dc->dc_indent == DOC_INDENT_FORCE) {
			/* nothing */
		} else {
			st->st_indent.i_cur -= dc->dc_indent;
		}
		/*
		 * While reaching the first column, there's no longer any
		 * previous indentation to consider.
		 */
		if (st->st_indent.i_cur == 0)
			st->st_indent.i_pre = 0;
		break;

	case DOC_DEDENT: {
		int indent;

		if (dc->dc_indent == DOC_DEDENT_NONE) {
			indent = st->st_indent.i_cur;
		} else {
			indent = dc->dc_indent;
			if (indent > st->st_indent.i_cur)
				indent = st->st_indent.i_cur;
		}
		doc_trim(dc, st);
		st->st_indent.i_cur -= indent;
		doc_indent(dc, st, st->st_indent.i_cur);
		doc_exec1(dc->dc_doc, st);
		st->st_indent.i_cur += indent;
		break;
	}

	case DOC_ALIGN:
		if ((st->st_flags & DOC_STATE_FLAG_WIDTH) == 0)
			doc_indent1(dc, st, dc->dc_indent);
		break;

	case DOC_LITERAL:
		doc_print(dc, st, dc->dc_str, dc->dc_len, 1);
		break;

	case DOC_VERBATIM: {
		unsigned int oldpos;

		if (st->st_mute)
			break;

		/*
		 * A verbatim block is either a comment or preprocessor
		 * directive.
		 */
		int isblock = dc->dc_len > 1 &&
		    dc->dc_str[dc->dc_len - 1] == '\n';

		/*
		 * Verbatims must never be indented, therefore trim the current
		 * line.
		 */
		doc_trim(dc, st);
		oldpos = st->st_pos;

		/* Verbatim blocks must always start on a new line. */
		if (isblock && st->st_pos > 0)
			doc_print(dc, st, "\n", 1, 0);

		doc_print(dc, st, dc->dc_str, dc->dc_len, 1);

		/*
		 * Restore the indentation after emitting a verbatim block.
		 * If the position after trimming is greater than zero must
		 * imply that we're in the middle of some construct and the
		 * current indentation must be honored as what follows is a
		 * continuation of the same construct. Otherwise, the verbatim
		 * block was emitted starting with an already blank line. Then
		 * the previously emitted indentation must be honored as this is
		 * not a continuation.
		 */
		if (isblock) {
			st->st_pos = 0;
			doc_indent(dc, st,
			    oldpos > 0 ? st->st_indent.i_cur : st->st_indent.i_pre);
		}

		break;
	}

	case DOC_LINE:
		switch (st->st_mode) {
		case BREAK:
			doc_print(dc, st, "\n", 1, 1);
			break;
		case MUNGE:
			doc_print(dc, st, " ", 1, 1);
			doc_trace(dc, st, "%s: refit %u -> %d", __func__,
			    st->st_refit, 1);
			st->st_refit = 1;
			break;
		}
		break;

	case DOC_SOFTLINE:
		switch (st->st_mode) {
		case BREAK:
			doc_print(dc, st, "\n", 1, 1);
			break;
		case MUNGE:
			break;
		}
		break;

	case DOC_HARDLINE:
		doc_print(dc, st, "\n", 1, 1);
		break;

	case DOC_MUTE:
		if ((st->st_flags & DOC_STATE_FLAG_WIDTH) == 0)
			st->st_mute += dc->dc_int;
		break;
	}

	doc_trace_leave(dc, st);
}

static int
doc_fits(const struct doc *dc, struct doc_state *st)
{
	struct doc_state sst;
	int cached = 0;

	if (DOC_TRACE(st))
		st->st_stats.s_nfits++;

	if (st->st_fits.f_fits == -1 || st->st_fits.f_pos != st->st_pos) {
		memcpy(&sst, st, sizeof(sst));
		/* Should not perform any printing. */
		sst.st_bf = NULL;
		// XXX we always want to fit in MUNGE mode? If so, remove conditionals in doc_fits1()
		sst.st_mode = MUNGE;
		st->st_fits.f_fits = doc_fits1(dc, &sst);
		st->st_fits.f_pos = st->st_pos;
		st->st_fits.f_ppos = sst.st_pos;
	} else {
		if (DOC_TRACE(st))
			st->st_stats.s_nfits_cache++;
		cached = 1;
	}
	doc_trace(dc, st, "%s: %u %s %u%s", __func__, st->st_fits.f_ppos,
	    st->st_fits.f_fits ? "<=" : ">", st->st_cf->cf_mw,
	    cached ? " (cached)" : "");

	return st->st_fits.f_fits;
}

static int
doc_fits1(const struct doc *dc, struct doc_state *st)
{
	/*
	 * When calculating the document width using doc_width(), everything is
	 * expected to fit on a single line.
	 */
	if (st->st_flags & DOC_STATE_FLAG_WIDTH)
		return 1;

	switch (dc->dc_type) {
	case DOC_CONCAT:
	case DOC_NOLINE: {
		struct doc *concat;

		TAILQ_FOREACH(concat, &dc->dc_list, dc_entry) {
			if (!doc_fits1(concat, st))
				return 0;
		}
		break;
	}

	case DOC_GROUP:
	case DOC_INDENT:
	case DOC_DEDENT:
		return doc_fits1(dc->dc_doc, st);

	case DOC_ALIGN:
		break;

	case DOC_LITERAL:
		st->st_pos += dc->dc_len;
		break;

	case DOC_VERBATIM:
		return 1;

	case DOC_LINE:
		switch (st->st_mode) {
		case BREAK:
			return 1;
		case MUNGE:
			st->st_pos++;
			break;
		}
		break;

	case DOC_SOFTLINE:
		switch (st->st_mode) {
		case BREAK:
			return 1;
		case MUNGE:
			break;
		}
		break;

	case DOC_HARDLINE:
		return 1;

	case DOC_MUTE:
		break;
	}

	return st->st_pos <= st->st_cf->cf_mw;
}

static void
doc_indent(const struct doc *dc, struct doc_state *st, int indent)
{
	int parens = 0;

	if (doc_parens(st)) {
		/* Align with the left parenthesis on the previous line. */
		indent = st->st_indent.i_pre + (st->st_parens - 1);
		parens = 1;
	} else {
		st->st_indent.i_pre = indent;
	}

	doc_indent1(dc, st, indent);

	if (!parens)
		st->st_indent.i_pos = st->st_bf->bf_len;
}

static void
doc_indent1(const struct doc *UNUSED(dc), struct doc_state *st, int indent)
{
	for (; indent >= 8; indent -= 8) {
		buffer_appendc(st->st_bf, '\t');
		st->st_pos += 8 - (st->st_pos % 8);
	}
	for (; indent > 0; indent--) {
		buffer_appendc(st->st_bf, ' ');
		st->st_pos++;
	}
}

static void
doc_print(const struct doc *dc, struct doc_state *st, const char *str,
    size_t len, int doindent)
{
	int newline = len == 1 && str[0] == '\n';

	if (newline && dc->dc_type == DOC_VERBATIM && st->st_noline > 0)
		return;

	if (newline) {
		/* Skip new lines while testing. */
		if (st->st_cf->cf_flags & CONFIG_FLAG_TEST)
			return;

		/* Never emit more than two consecutive lines. */
		if (st->st_line >= 2)
			return;
		st->st_line++;
	} else {
		st->st_line = 0;
	}

	if (newline)
		doc_trim(dc, st);

	if (st->st_mute == 0 || dc->dc_type == DOC_HARDLINE)
		buffer_append(st->st_bf, str, len);
	st->st_pos += len;

	if (newline) {
		st->st_pos = 0;
		if (doindent)
			doc_indent(dc, st, st->st_indent.i_cur);
	}
}

static void
doc_trim(const struct doc *dc, struct doc_state *st)
{
	struct buffer *bf = st->st_bf;
	unsigned int oldpos = st->st_pos;

	while (bf->bf_len > 0) {
		unsigned char ch;

		ch = bf->bf_ptr[bf->bf_len - 1];
		if (ch != ' ' && ch != '\t')
			break;
		bf->bf_len--;
		st->st_pos -= ch == '\t' ? 8 - (st->st_pos % 8) : 1;
	}
	if (oldpos > st->st_pos)
		doc_trace(dc, st, "%s: trimmed %u character(s)", __func__,
		    oldpos - st->st_pos);
}

/*
 * Returns non-zero if we're in a pair of parenthesis that's applicable to
 * aligned indentation.
 */
static int
doc_parens(const struct doc_state *st)
{
	return st->st_parens > 0 && st->st_indent.i_pre > 0 &&
	    st->st_bf->bf_ptr[st->st_indent.i_pos] == '(';
}

static int
doc_has_list(const struct doc *dc)
{
	switch (dc->dc_type) {
	case DOC_CONCAT:
	case DOC_NOLINE:
		return 1;
	default:
		break;
	}
	return 0;
}

static struct doc *
__doc_alloc_mute(int mute, struct doc *parent, const char *fun, int lno)
{
	struct doc *dc;

	dc = __doc_alloc(DOC_MUTE, parent, fun, lno);
	dc->dc_int = mute;
	return dc;
}

static void
__doc_trace(const struct doc *UNUSED(dc), const struct doc_state *st,
    const char *fmt, ...)
{
	char buf[32];
	va_list ap;
	unsigned int depth, i;

	fprintf(stderr, "%s", statestr(st, st->st_depth, buf, sizeof(buf)));
	depth = st->st_depth * 2 + 1;
	for (i = 0; i < depth; i++)
		fprintf(stderr, "-");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
}

static void
__doc_trace_enter(const struct doc *dc, struct doc_state *st)
{
	char buf[128];
	unsigned int depth = st->st_depth;
	unsigned int i;

	st->st_depth++;

	fprintf(stderr, "%s ", statestr(st, st->st_depth, buf, sizeof(buf)));

	for (i = 0; i < depth; i++)
		fprintf(stderr, "  ");

	fprintf(stderr, "%s", docstr(dc, buf, sizeof(buf)));
	switch (dc->dc_type) {
	case DOC_CONCAT:
	case DOC_NOLINE:
		fprintf(stderr, "([");
		break;

	case DOC_GROUP:
		fprintf(stderr, "(");
		break;

	case DOC_INDENT:
	case DOC_DEDENT: {
		const char *str;

		fprintf(stderr, "(");
		if ((str = indentstr(dc)) != NULL)
			fprintf(stderr, "%s", str);
		else
			fprintf(stderr, "%d", dc->dc_indent);
		break;
	}

	case DOC_ALIGN:
		fprintf(stderr, "(%d)", dc->dc_indent);
		break;

	case DOC_LITERAL:
	case DOC_VERBATIM: {
		char *str;

		str = strnice(dc->dc_str, dc->dc_len);
		fprintf(stderr, "(\"%s\", %zu)", str, dc->dc_len);
		free(str);
		break;
	}

	case DOC_LINE:
		fprintf(stderr, "(\"%s\"%s%s)",
		    st->st_mode == BREAK ? "\\n" : " ",
		    st->st_mode == BREAK ? "" : ", ",
		    st->st_mode == BREAK ? "" : "1");
		break;

	case DOC_SOFTLINE:
		fprintf(stderr, "(\"%s\")", st->st_mode == BREAK ? "\\n" : "");
		break;

	case DOC_HARDLINE:
		break;

	case DOC_MUTE:
		fprintf(stderr, "(%d)", dc->dc_int);
		break;
	}
	fprintf(stderr, "\n");
}

static void
__doc_trace_leave(const struct doc *dc, struct doc_state *st)
{
	char buf[128];
	unsigned int depth = st->st_depth;
	unsigned int i;
	int parens = 0;
	int brackets = 0;

	st->st_depth--;

	switch (dc->dc_type) {
	case DOC_CONCAT:
	case DOC_NOLINE:
		brackets = 1;
		/* FALLTHROUGH */
	case DOC_GROUP:
	case DOC_INDENT:
	case DOC_DEDENT:
		parens = 1;
		break;
	case DOC_ALIGN:
	case DOC_LITERAL:
	case DOC_VERBATIM:
	case DOC_LINE:
	case DOC_SOFTLINE:
	case DOC_HARDLINE:
	case DOC_MUTE:
		break;
	}

	if (!parens && !brackets)
		return;

	fprintf(stderr, "%s ", statestr(st, depth, buf, sizeof(buf)));
	for (i = 0; i < st->st_depth; i++)
		fprintf(stderr, "  ");
	if (brackets)
		fprintf(stderr, "]");
	if (parens)
		fprintf(stderr, ")");
	fprintf(stderr, "\n");
}

static char *
docstr(const struct doc *dc, char *buf, size_t bufsiz)
{
	const char *str = NULL;
	int n;

	switch (dc->dc_type) {
#define CASE(t) case t: str = &#t[sizeof("DOC_") - 1]; break
	CASE(DOC_CONCAT);
	CASE(DOC_GROUP);
	CASE(DOC_INDENT);
	CASE(DOC_DEDENT);
	CASE(DOC_ALIGN);
	CASE(DOC_LITERAL);
	CASE(DOC_VERBATIM);
	CASE(DOC_LINE);
	CASE(DOC_SOFTLINE);
	CASE(DOC_HARDLINE);
	CASE(DOC_NOLINE);
	CASE(DOC_MUTE);
#undef CASE
	}

	n = snprintf(buf, bufsiz, "%s<%s:%d>", str, dc->dc_fun, dc->dc_lno);
	if (n < 0 || n >= (ssize_t)bufsiz)
		errc(1, ENAMETOOLONG, "%s", __func__);

	return buf;
}

static const char *
indentstr(const struct doc *dc)
{
	if (dc->dc_type == DOC_INDENT) {
		if (dc->dc_indent == DOC_INDENT_PARENS)
			return "PARENS";
		else if (dc->dc_indent == DOC_INDENT_FORCE)
			return "FORCE";
	} else if (dc->dc_type == DOC_DEDENT) {
		if (dc->dc_indent == DOC_DEDENT_NONE)
			return "NONE";
	}
	return NULL;
}

static const char *
statestr(const struct doc_state *st, unsigned int depth, char *buf,
    size_t bufsiz)
{
	unsigned char mode = 'U';
	int n;

	switch (st->st_mode) {
	case BREAK:
		mode = 'B';
		break;
	case MUNGE:
		mode = 'M';
		break;
	}
	n = snprintf(buf, bufsiz, "[D] [%c,%3u,%3u,%3d]", mode, st->st_pos,
	    depth, st->st_mute);
	if (n < 0 || n >= (ssize_t)bufsiz)
		errc(1, ENAMETOOLONG, "%s", __func__);
	return buf;
}
