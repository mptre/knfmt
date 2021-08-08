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
	enum doc_type	 dc_type;

	/* allocation trace */
	int		 dc_lno;
	const char	*dc_fun;
	const char	*dc_suffix;

	/* children */
	union {
		struct doc_list	 dc_list;
		struct doc	*dc_doc;
		struct {
			unsigned int	tk_lno;
			unsigned int	tk_flags;
		} dc_tk;
	};

	/* value */
	union {
		struct {
			const char	*dc_str;
			size_t		 dc_len;
		};
		int	dc_int;
	};

	TAILQ_ENTRY(doc) dc_entry;
};

struct doc_state_indent {
	int	i_cur;
	int	i_pre;
	int	i_mute;
};

struct doc_state {
	const struct config	*st_cf;
	struct buffer		*st_bf;
	struct lexer		*st_lx;

	enum {
		BREAK,
		MUNGE,
	} st_mode;

	struct {
		int		d_groups;
		int		d_ignore;
		int		d_mute;
		unsigned int	d_beg;
		unsigned int	d_end;
	} st_diff;

	struct {
		int		f_fits;
		unsigned int	f_optline;
		unsigned int	f_pos;
		unsigned int	f_ppos;
	} st_fits;

	struct doc_state_indent	st_indent;

	struct {
		unsigned int	s_nfits;
		unsigned int	s_nfits_cache;
	} st_stats;

	unsigned int		st_pos;
	unsigned int		st_depth;
	unsigned int		st_refit;
	unsigned int		st_parens;
	unsigned int		st_nlines;
	unsigned int		st_newline;
	int			st_optline;
	int			st_mute;
	unsigned int		st_flags;
#define DOC_STATE_FLAG_WIDTH	0x00000001u
};

/*
 * Line numbers extracted from a document group that covers a diff chunk.
 */
struct doc_diff {
	unsigned int	dd_first;	/* first token in group */
	unsigned int	dd_chunk;	/* first token covered by the chunk */
};

static void	doc_exec1(const struct doc *, struct doc_state *);
static int	doc_fits(const struct doc *, struct doc_state *);
static int	doc_fits1(const struct doc *, struct doc_state *);
static void	doc_indent(const struct doc *, struct doc_state *, int);
static void	doc_indent1(const struct doc *, struct doc_state *, int);
static void	doc_trim(const struct doc *, struct doc_state *);
static int	doc_is_mute(const struct doc_state *);
static int	doc_parens_align(const struct doc_state *);
static int	doc_has_list(const struct doc *);

#define DOC_DIFF(st) 							\
	(((st)->st_cf->cf_flags & CONFIG_FLAG_DIFFPARSE) &&		\
	((st)->st_flags & DOC_STATE_FLAG_WIDTH) == 0)

static int		doc_diff_group_enter(const struct doc *,
    struct doc_state *);
static void		doc_diff_group_leave(const struct doc *,
    struct doc_state *, int);
static void		doc_diff_literal(const struct doc *,
    struct doc_state *);
static unsigned int	doc_diff_verbatim(const struct doc *,
    struct doc_state *);
static void		doc_diff_exit(const struct doc *, struct doc_state *);
static void		doc_diff_emit(const struct doc *, struct doc_state *,
    unsigned int, unsigned int);
static int		doc_diff_covers(const struct doc *, struct doc_diff *);
static int		doc_diff_is_mute(const struct doc_state *);

#define doc_diff_leave(a, b, c) \
	__doc_diff_leave((a), (b), (c), __func__)
static void	__doc_diff_leave(const struct doc *, struct doc_state *,
    unsigned int, const char *);

#define DOC_PRINT_FLAG_INDENT	0x00000001u
#define DOC_PRINT_FLAG_NEWLINE	0x00000002u
#define DOC_PRINT_FLAG_FORCE	0x00000004u

static void	doc_print(const struct doc *, struct doc_state *, const char *,
    size_t, unsigned int);

#define DOC_TRACE(st)	(TRACE((st)->st_cf) &&	\
	((st)->st_flags & DOC_STATE_FLAG_WIDTH) == 0)

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
static const char	*intstr(const struct doc *);
static const char	*statestr(const struct doc_state *, unsigned int,
    char *, size_t);

static unsigned int	countlines(const char *, size_t);

void
doc_exec(const struct doc *dc, struct lexer *lx, struct buffer *bf,
    const struct config *cf)
{
	struct doc_state st;

	buffer_reset(bf);
	memset(&st, 0, sizeof(st));
	st.st_cf = cf;
	st.st_bf = bf;
	st.st_lx = lx;
	st.st_mode = BREAK;
	st.st_diff.d_beg = 1;
	st.st_fits.f_fits = -1;

	doc_exec1(dc, &st);
	doc_diff_exit(dc, &st);
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
	case DOC_CONCAT: {
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
	case DOC_OPTIONAL:
		doc_free(dc->dc_doc);
		break;

	case DOC_ALIGN:
	case DOC_LITERAL:
	case DOC_VERBATIM:
	case DOC_LINE:
	case DOC_SOFTLINE:
	case DOC_HARDLINE:
	case DOC_NEWLINE:
	case DOC_OPTLINE:
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
doc_remove_tail(struct doc *parent)
{
	struct doc *dc;

	assert(doc_has_list(parent));
	dc = TAILQ_LAST(&parent->dc_list, doc_list);
	if (dc == NULL)
		return;
	TAILQ_REMOVE(&parent->dc_list, dc, dc_entry);
	doc_free(dc);
}

void
doc_set_indent(struct doc *dc, int indent)
{
	dc->dc_int = indent;
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
__doc_alloc(enum doc_type type, struct doc *parent, int val, const char *fun,
    int lno)
{
	struct doc *dc;

	dc = calloc(1, sizeof(*dc));
	if (dc == NULL)
		err(1, NULL);
	dc->dc_type = type;
	dc->dc_fun = fun;
	dc->dc_lno = lno;
	dc->dc_int = val;
	if (doc_has_list(dc))
		TAILQ_INIT(&dc->dc_list);
	if (parent != NULL)
		doc_append(dc, parent);

	return dc;
}

struct doc *
__doc_alloc_indent(enum doc_type type, int val, struct doc *dc,
    const char *fun, int lno)
{
	struct doc *indent;

	indent = __doc_alloc(type, dc, val, fun, lno);
	return __doc_alloc(DOC_CONCAT, indent, 0, fun, lno);
}

struct doc *
__doc_literal(const char *str, struct doc *dc, const char *fun, int lno)
{
	struct doc *literal;

	literal = __doc_alloc(DOC_LITERAL, dc, 0, fun, lno);
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

	/* Fake token created by lexer_recover_hard(), emit nothing. */
	if (tk->tk_flags & TOKEN_FLAG_FAKE)
		return NULL;

	/*
	 * If the token has dangling tokens, a parent is mandatory as we're
	 * about to append more than one token.
	 */
	if (dc == NULL && token_has_dangling(tk)) {
		dangling = 1;
		dc = doc_alloc(DOC_CONCAT, NULL);
	}

	if (tk->tk_flags & TOKEN_FLAG_UNMUTE)
		__doc_alloc(DOC_MUTE, dc, -1, fun, lno);

	TAILQ_FOREACH(tmp, &tk->tk_prefixes, tk_entry) {
		__doc_token(tmp, dc, DOC_VERBATIM, __func__, __LINE__);
	}

	token = __doc_alloc(type, dc, 0, fun, lno);
	token->dc_tk.tk_lno = tk->tk_lno;
	token->dc_tk.tk_flags = tk->tk_flags;
	token->dc_str = tk->tk_str;
	token->dc_len = tk->tk_len;

	TAILQ_FOREACH(tmp, &tk->tk_suffixes, tk_entry) {
		if (tmp->tk_flags & TOKEN_FLAG_OPTLINE)
			doc_alloc(DOC_OPTLINE, dc);
		else if ((tmp->tk_flags & TOKEN_FLAG_OPTSPACE) == 0)
			__doc_token(tmp, dc, DOC_VERBATIM, __func__, __LINE__);
	}

	/* lexer_comment() signalled that hard line(s) must be emitted. */
	if (tk->tk_flags & TOKEN_FLAG_NEWLINE)
		__doc_alloc(DOC_NEWLINE, dc, tk->tk_int, fun, lno);

	/* Mute if we're about to branch. */
	tmp = TAILQ_NEXT(tk, tk_entry);
	if (tmp != NULL && token_is_branch(tmp))
		__doc_alloc(DOC_MUTE, dc, 1, fun, lno);

	return dangling ? dc : token;
}

void
doc_annotate(struct doc *dc, const char *suffix)
{
	dc->dc_suffix = suffix;
}

static void
doc_exec1(const struct doc *dc, struct doc_state *st)
{
	doc_trace_enter(dc, st);

	switch (dc->dc_type) {
	case DOC_CONCAT: {
		struct doc *concat;
		int ndocs = 0;

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

		TAILQ_FOREACH(concat, &dc->dc_list, dc_entry) {
			doc_exec1(concat, st);
		}
		break;
	}

	case DOC_GROUP: {
		unsigned int oldmode;
		int ignore;

		ignore = doc_diff_group_enter(dc, st);
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
		doc_diff_group_leave(dc, st, ignore);

		break;
	}

	case DOC_INDENT: {
		int oldparens = 0;

		if (dc->dc_int == DOC_INDENT_PARENS) {
			oldparens = st->st_parens;
			if (doc_parens_align(st))
				st->st_parens++;
		} else if (dc->dc_int == DOC_INDENT_FORCE) {
			doc_indent(dc, st, st->st_indent.i_cur);
		} else {
			st->st_indent.i_cur += dc->dc_int;
		}
		doc_exec1(dc->dc_doc, st);
		if (dc->dc_int == DOC_INDENT_PARENS) {
			st->st_parens = oldparens;
		} else if (dc->dc_int == DOC_INDENT_FORCE) {
			/* nothing */
		} else {
			st->st_indent.i_cur -= dc->dc_int;
		}
		/*
		 * While reaching the first column, there's no longer any
		 * previous indentation to consider.
		 */
		if (st->st_indent.i_cur == 0)
			st->st_indent.i_pre = 0;
		break;
	}

	case DOC_DEDENT: {
		int oldindent;

		doc_trim(dc, st);
		oldindent = st->st_indent.i_cur;
		st->st_indent.i_cur = 0;
		st->st_indent.i_pre = 0;
		doc_exec1(dc->dc_doc, st);
		st->st_indent.i_cur = oldindent;
		break;
	}

	case DOC_ALIGN:
		if ((st->st_flags & DOC_STATE_FLAG_WIDTH) == 0)
			doc_indent1(dc, st, dc->dc_int);
		break;

	case DOC_LITERAL:
		doc_diff_literal(dc, st);
		doc_print(dc, st, dc->dc_str, dc->dc_len,
		    DOC_PRINT_FLAG_INDENT);
		break;

	case DOC_VERBATIM: {
		unsigned int end, oldpos;

		end = doc_diff_verbatim(dc, st);

		if (doc_is_mute(st))
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

		doc_print(dc, st, dc->dc_str, dc->dc_len,
		    DOC_PRINT_FLAG_INDENT);

		/* Restore the indentation after emitting a verbatim block. */
		if (isblock) {
			struct doc_state_indent *it = &st->st_indent;
			unsigned int indent;

			if (it->i_mute > 0) {
				/*
				 * Honor last emitted indentation before going
				 * mute. However, discard it if the current
				 * indentation is smaller since it implies that
				 * we're in a different scope by now.
				 */
				indent = it->i_cur < it->i_mute ?
				    it->i_cur : it->i_mute;
				it->i_mute = 0;
			} else if (oldpos > 0) {
				/*
				 * The line is not empty after trimming. Assume
				 * this a continuation in which the current
				 * indentation level must be used.
				 */
				indent = it->i_cur;
			} else {
				/*
				 * The line is empty after trimming. Assume this
				 * is not a continuation in which the previously
				 * emitted indentation must be used.
				 */
				indent = it->i_pre;
			}
			st->st_pos = 0;
			doc_indent(dc, st, indent);
		}

		if (end > 0)
			doc_diff_leave(dc, st, end);

		break;
	}

	case DOC_LINE:
		switch (st->st_mode) {
		case BREAK:
			doc_print(dc, st, "\n", 1, DOC_PRINT_FLAG_INDENT);
			break;
		case MUNGE:
			/* Redundant if we're about to emit a hard line. */
			if (st->st_newline)
				break;
			doc_print(dc, st, " ", 1, DOC_PRINT_FLAG_INDENT);
			doc_trace(dc, st, "%s: refit %u -> %d", __func__,
			    st->st_refit, 1);
			st->st_refit = 1;
			break;
		}
		break;

	case DOC_SOFTLINE:
		switch (st->st_mode) {
		case BREAK:
			doc_print(dc, st, "\n", 1, DOC_PRINT_FLAG_INDENT);
			break;
		case MUNGE:
			break;
		}
		break;

	case DOC_HARDLINE:
		/* Take note of new line(s), later emitted by doc_print(). */
		if (st->st_mute)
			st->st_newline = 1;
		doc_print(dc, st, "\n", 1, DOC_PRINT_FLAG_INDENT);
		break;

	case DOC_NEWLINE:
		/*
		 * Signal to doc_print() that we've got pending hard line(s) to
		 * emit.
		 */
		st->st_newline = dc->dc_int;
		break;

	case DOC_OPTLINE:
		/*
		 * Signal to doc_print() to emit a hard line on the next
		 * invocation. Necessary in order to get indentation right.
		 */
		if (st->st_optline)
			st->st_newline = 1;
		break;

	case DOC_MUTE:
		if (!DOC_DIFF(st) &&
		    (st->st_flags & DOC_STATE_FLAG_WIDTH) == 0) {
			/*
			 * Take note of the previously emitted indentation
			 * before going mute.
			 */
			if (st->st_mute == 0 && dc->dc_int > 0)
				st->st_indent.i_mute = st->st_indent.i_pre;

			if (dc->dc_int > 0 || st->st_mute >= -dc->dc_int)
				st->st_mute += dc->dc_int;
		}
		break;

	case DOC_OPTIONAL: {
		if (st->st_optline != DOC_OPTIONAL_STICKY) {
			int oldoptline = st->st_optline;

			if (dc->dc_int == DOC_OPTIONAL_STICKY)
				st->st_optline = DOC_OPTIONAL_STICKY;
			else
				st->st_optline++;
			doc_exec1(dc->dc_doc, st);
			/* Note, could already be cleared by doc_print(). */
			if (oldoptline <= st->st_optline)
				st->st_optline = oldoptline;
		} else {
			doc_exec1(dc->dc_doc, st);
		}
		break;
	}
	}

	doc_trace_leave(dc, st);
}

static int
doc_fits(const struct doc *dc, struct doc_state *st)
{
	struct doc_state fst;
	int cached = 0;
	int optline = 0;

	/*
	 * When calculating the document width using doc_width(), everything is
	 * expected to fit on a single line.
	 */
	if (st->st_flags & DOC_STATE_FLAG_WIDTH)
		return 1;

	/*
	 * Nothing fits while being outside of a diff chunk. Needed in order to
	 * stay in break mode until entering a diff chunk.
	 */
	if (doc_diff_is_mute(st))
		return 0;

	if (DOC_TRACE(st))
		st->st_stats.s_nfits++;

	if (st->st_newline > 0) {
		/* Pending hard line(s), assume that everything fits. */
		optline = 1;
	} else if (st->st_fits.f_fits == -1 ||
	    st->st_fits.f_pos != st->st_pos) {
		memcpy(&fst, st, sizeof(fst));
		/* Should not perform any printing. */
		fst.st_bf = NULL;
		fst.st_mode = MUNGE;
		fst.st_fits.f_optline = 0;
		st->st_fits.f_fits = doc_fits1(dc, &fst);
		st->st_fits.f_pos = st->st_pos;
		st->st_fits.f_ppos = fst.st_pos;
		if (!st->st_fits.f_fits &&
		    fst.st_fits.f_optline > 0 &&
		    fst.st_fits.f_optline <= st->st_cf->cf_mw) {
			/* Honoring an optional line makes everything fit. */
			st->st_fits.f_fits = 1;
			st->st_fits.f_ppos = fst.st_fits.f_optline;
			optline = 1;
		}
	} else {
		if (DOC_TRACE(st))
			st->st_stats.s_nfits_cache++;
		cached = 1;
	}
	doc_trace(dc, st, "%s: %u %s %u, cached %d, optline %d",
	    __func__,
	    st->st_fits.f_ppos,
	    st->st_fits.f_fits ? "<=" : ">",
	    st->st_cf->cf_mw,
	    cached, optline);

	return st->st_fits.f_fits;
}

static int
doc_fits1(const struct doc *dc, struct doc_state *st)
{
	switch (dc->dc_type) {
	case DOC_CONCAT: {
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
	case DOC_OPTIONAL:
		return doc_fits1(dc->dc_doc, st);

	case DOC_ALIGN:
		break;

	case DOC_LITERAL:
		st->st_pos += dc->dc_len;
		break;

	case DOC_VERBATIM:
		return 1;

	case DOC_LINE:
		st->st_pos++;
		break;

	case DOC_SOFTLINE:
		break;

	case DOC_HARDLINE:
	case DOC_NEWLINE:
		return 1;

	case DOC_OPTLINE:
		if (st->st_optline) {
			if (st->st_fits.f_optline == 0)
				st->st_fits.f_optline = st->st_pos;
			return 1;
		}
		break;

	case DOC_MUTE:
		break;
	}

	return st->st_pos <= st->st_cf->cf_mw;
}

static void
doc_indent(const struct doc *dc, struct doc_state *st, int indent)
{
	if (st->st_parens > 0) {
		/* Align with the left parenthesis on the previous line. */
		indent = st->st_indent.i_pre + st->st_parens;
	} else {
		st->st_indent.i_pre = indent;
	}

	doc_indent1(dc, st, indent);
}

static void
doc_indent1(const struct doc *UNUSED(dc), struct doc_state *st, int indent)
{
	if (doc_is_mute(st))
		return;

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
    size_t len, unsigned int flags)
{
	int newline = len == 1 && str[0] == '\n';

	if (doc_is_mute(st) && (flags & DOC_PRINT_FLAG_FORCE) == 0)
		return;

	/* Emit any pending hard line(s). */
	if (st->st_newline > 0) {
		int space = len == 1 && str[0] == ' ';
		int n;

		n = st->st_newline;
		st->st_newline = 0;
		for (; n > 0; n--)
			doc_print(dc, st, "\n", 1,
			    (n - 1 == 0 ? flags : 0) | DOC_PRINT_FLAG_NEWLINE);
		if (newline || space)
			return;
	}

	if (newline) {
		/* Never emit more than two consecutive lines. */
		if (st->st_nlines >= 2)
			return;
		st->st_nlines++;

		/*
		 * Suppress optional line(s) while emitting a line. Mixing the
		 * two results in odd formatting.
		 */
		if ((flags & DOC_PRINT_FLAG_NEWLINE) == 0 &&
		    st->st_optline > 0 &&
		    st->st_optline < DOC_OPTIONAL_STICKY) {
			doc_trace(dc, st, "%s: optline %d -> 0",
			    __func__, st->st_optline);
			st->st_optline = 0;
		}
	} else {
		st->st_nlines = 0;
	}

	if (newline)
		doc_trim(dc, st);

	buffer_append(st->st_bf, str, len);
	st->st_pos += len;

	if (newline) {
		st->st_pos = 0;
		if (flags & DOC_PRINT_FLAG_INDENT)
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

static int
doc_diff_group_enter(const struct doc *dc, struct doc_state *st)
{
	struct doc_diff dd;
	const struct diffchunk *du;

	if (!DOC_DIFF(st))
		return 0;

	/*
	 * Only applicable while entering the first group. Unless the group
	 * above us was ignored, see below.
	 */
	if (st->st_diff.d_groups++ > 0 && st->st_diff.d_ignore == 0)
		return 0;

	/*
	 * Check if the current group is covered by a diff chunk. If so, we must
	 * start emitting documents nested under the same group.
	 *
	 * A group is something intended to fit on a single line. However,
	 * there are expections in the sense of groups spanning multiple lines;
	 * one example is brace initializers. Such groups are ignored allowing
	 * the first group covering a single line to be found.
	 */
	memset(&dd, 0, sizeof(dd));
	switch (doc_diff_covers(dc, &dd)) {
	case 0:
		/*
		 * The group is not covered by any diff chunk. However, if the
		 * previous group above us touched lines after the diff chunk
		 * due to reformatting make sure to reset the state.
		 */
		if (st->st_diff.d_end > 0)
			doc_diff_leave(dc, st, 1);
		return 0;
	case -1:
		/*
		 * The group spans multiple lines. Ignore it and keep evaluating
		 * nested groups on subsequent invocations of this routine.
		 */
		doc_trace(dc, st, "%s: ignore", __func__);
		st->st_diff.d_ignore++;
		return 1;
	}

	doc_trace(dc, st, "%s: enter chunk: beg %u, end %u, first %u, "
	    "chunk %u, groups %d, ignore %d, seen %d", __func__,
	    st->st_diff.d_beg, st->st_diff.d_end,
	    dd.dd_first, dd.dd_chunk,
	    st->st_diff.d_groups, st->st_diff.d_ignore, st->st_diff.d_end > 0);

	if (st->st_diff.d_end > 0) {
		/*
		 * The diff chunk is spanning more than one group. Any preceding
		 * verbatim lines are already emitted at this point.
		 */
		return 0;
	}

	du = lexer_get_diffchunk(st->st_lx, dd.dd_chunk);
	if (du == NULL)
		return 0;
	doc_trace(dc, st, "%s: chunk range [%u-%u]", __func__,
	    du->du_beg, du->du_end);

	/*
	 * Take a tentative note on which line the diff chunk ends. Note that if
	 * the current group spans beyond the diff chunk, the end line will be
	 * adjusted by doc_diff_literal(). This can happen when reformatting
	 * causes lines to be merged.
	 */
	st->st_diff.d_end = du->du_end;

	/*
	 * We could still be in a muted section of the document, ignore and then
	 * restore in doc_diff_leave();
	 */
	st->st_diff.d_mute = st->st_mute;
	st->st_mute = 0;

	/*
	 * Emit any preceding line(s) not covered by the diff chunk. It is of
	 * importance to end at the line from the first token covered by this
	 * group and not the first line covered by the diff chunk; as a group
	 * represents something intended to fit on a single line but the diff
	 * chunk might only touch a subset of the group.
	 */
	doc_diff_emit(dc, st, st->st_diff.d_beg, dd.dd_first);
	st->st_pos = 0;
	doc_indent(dc, st, st->st_indent.i_cur);
	return 0;
}

static void
doc_diff_group_leave(const struct doc *UNUSED(dc), struct doc_state *st,
    int ignore)
{
	if (!DOC_DIFF(st))
		return;

	if (ignore)
		st->st_diff.d_ignore--;
	st->st_diff.d_groups--;
}

static void
doc_diff_literal(const struct doc *dc, struct doc_state *st)
{
	unsigned int lno = dc->dc_tk.tk_lno;

	if (!DOC_DIFF(st))
		return;
	if (lno == 0 || st->st_diff.d_end == 0)
		return;

	if (st->st_diff.d_groups > 0) {
		if (lno > st->st_diff.d_end) {
			/*
			 * The current group spans beyond the diff chunk, adjust
			 * the end line. This can happen when reformatting
			 * causes lines to be merged.
			 */
			doc_trace(dc, st, "%s: end %u", __func__, lno);
			st->st_diff.d_end = lno;
		}
	} else if (lno > st->st_diff.d_end) {
		doc_diff_leave(dc, st, 1);
	}
}

static unsigned int
doc_diff_verbatim(const struct doc *dc, struct doc_state *st)
{
	unsigned int lno = dc->dc_tk.tk_lno;
	unsigned int n;

	if (!DOC_DIFF(st))
		return 0;
	if (lno == 0 || st->st_diff.d_end == 0)
		return 0;

	if (lno > st->st_diff.d_end) {
		doc_diff_leave(dc, st, 1);
		return 0;
	}

	/*
	 * A verbatim document could contain one or many hard line(s) which can
	 * takes us beyond the diff chunk. Signal that after emitting this
	 * document the diff chunk must be left.
	 */
	n = countlines(dc->dc_str, dc->dc_len);
	if (n > 0 && lno + n > st->st_diff.d_end) {
		unsigned int end;

		end = (lno + n) - st->st_diff.d_end;
		doc_trace(dc, st, "%s: postpone leave: end %u", __func__, end);
		return end;
	}

	return 0;
}

static void
doc_diff_exit(const struct doc *dc, struct doc_state *st)
{
	if (!DOC_DIFF(st))
		return;
	doc_diff_emit(dc, st, st->st_diff.d_beg, 0);
}

/*
 * Emit everything between the given lines as is.
 */
static void
doc_diff_emit(const struct doc *dc, struct doc_state *st, unsigned int beg,
    unsigned int end)
{
	const char *str;
	size_t len;

	doc_trace(dc, st, "%s: range [%u, %u)", __func__, beg, end);
	if (!lexer_get_lines(st->st_lx, beg, end, &str, &len))
		return;

	if (DOC_TRACE(st)) {
		char *s;

		s = strnice(str, len);
		doc_trace(dc, st, "%s: verbatim \"%s\"", __func__, s);
		free(s);
	}

	st->st_newline = 0;
	doc_trim(dc, st);
	doc_print(dc, st, str, len, DOC_PRINT_FLAG_FORCE);
}

/*
 * Returns non-zero if any document covers a token which is part of a diff
 * chunk.
 */
static int
doc_diff_covers(const struct doc *dc, struct doc_diff *dd)
{
	switch (dc->dc_type) {
	case DOC_CONCAT: {
		const struct doc *concat;

		TAILQ_FOREACH(concat, &dc->dc_list, dc_entry) {
			int c;

			if ((c = doc_diff_covers(concat, dd)) != 0)
				return c;
		}
		break;
	}
	case DOC_GROUP:
	case DOC_INDENT:
	case DOC_DEDENT:
	case DOC_OPTIONAL:
		return doc_diff_covers(dc->dc_doc, dd);

	case DOC_LITERAL:
	case DOC_VERBATIM: {
		unsigned int lno = dc->dc_tk.tk_lno;

		if (lno > 0) {
			if (dd->dd_first == 0)
				dd->dd_first = lno;
			if (dc->dc_tk.tk_flags & TOKEN_FLAG_DIFF) {
				dd->dd_chunk = lno;
				return 1;
			}
		}
		break;
	}

	case DOC_HARDLINE:
		return -1;

	case DOC_ALIGN:
	case DOC_LINE:
	case DOC_SOFTLINE:
	case DOC_NEWLINE:
	case DOC_OPTLINE:
	case DOC_MUTE:
		break;
	}

	return 0;
}

/*
 * Returns non-zero if nothing should be emitted while being positioned on a
 * line not touched by the diff.
 */
static int
doc_diff_is_mute(const struct doc_state *st)
{
	return DOC_DIFF(st) && st->st_diff.d_end == 0;
}

static void
__doc_diff_leave(const struct doc *dc, struct doc_state *st, unsigned int end,
    const char *fun)
{
	assert(st->st_diff.d_end > 0);
	st->st_diff.d_beg = st->st_diff.d_end + end;
	st->st_diff.d_end = 0;
	st->st_mute = st->st_diff.d_mute;
	st->st_diff.d_mute = 0;
	doc_trace(dc, st, "doc_diff_leave: %s: leave chunk: beg %u",
	    fun, st->st_diff.d_beg);
}

static int
doc_is_mute(const struct doc_state *st)
{
	return st->st_mute || doc_diff_is_mute(st);
}

/*
 * Returns non-zero if the current line is suitable for parenthesis alignment,
 * i.e. a line consisting of whitespace followed by one or many left
 * parenthesisa. This is the desired outcome:
 *
 * 	if (a &&
 * 	    ((b &&
 * 	      c)))
 */
static int
doc_parens_align(const struct doc_state *st)
{
	const char *buf;
	size_t i;
	int nparens = 0;

	buf = st->st_bf->bf_ptr;
	i = st->st_bf->bf_len;
	for (; i > 0 && buf[i - 1] == '('; i--)
		nparens++;
	if (nparens == 0 || i == 0)
		return 0;
	while (--i > 0) {
		if (buf[i - 1] == '\n')
			break;
		if (buf[i - 1] != ' ' && buf[i - 1] != '\t')
			return 0;
	}
	return 1;
}

static int
doc_has_list(const struct doc *dc)
{
	switch (dc->dc_type) {
	case DOC_CONCAT:
		return 1;
	default:
		break;
	}
	return 0;
}

static void
__doc_trace(const struct doc *UNUSED(dc), const struct doc_state *st,
    const char *fmt, ...)
{
	char buf[128];
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
		fprintf(stderr, "([");
		break;

	case DOC_GROUP:
		fprintf(stderr, "(");
		break;

	case DOC_INDENT:
	case DOC_DEDENT:
	case DOC_OPTIONAL: {
		const char *str;

		fprintf(stderr, "(");
		if ((str = intstr(dc)) != NULL)
			fprintf(stderr, "%s", str);
		else
			fprintf(stderr, "%d", dc->dc_int);
		break;
	}

	case DOC_ALIGN:
		fprintf(stderr, "(%d)", dc->dc_int);
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
		fprintf(stderr, "(\"%s\", 1)",
		    st->st_mode == BREAK ? "\\n" : " ");
		break;

	case DOC_SOFTLINE:
		fprintf(stderr, "(\"%s\", %d)",
		    st->st_mode == BREAK ? "\\n" : "",
		    st->st_mode == BREAK ? 1 : 0);
		break;

	case DOC_HARDLINE:
		fprintf(stderr, "(\"\\n\", 1)");
		break;

	case DOC_OPTLINE:
		fprintf(stderr, "(\"%s\", %d)",
		    st->st_optline ? "\\n" : "", st->st_optline ? 1 : 0);
		break;

	case DOC_MUTE:
	case DOC_NEWLINE:
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
	case DOC_NEWLINE:
	case DOC_OPTLINE:
	case DOC_MUTE:
	case DOC_OPTIONAL:
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
	int suffix = dc->dc_suffix != NULL;

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
	CASE(DOC_NEWLINE);
	CASE(DOC_OPTLINE);
	CASE(DOC_MUTE);
	CASE(DOC_OPTIONAL);
#undef CASE
	}

	n = snprintf(buf, bufsiz, "%s<%s:%d%s%s>",
	    str, dc->dc_fun, dc->dc_lno,
	    suffix ? ", " : "", suffix ? dc->dc_suffix : "");
	if (n < 0 || n >= (ssize_t)bufsiz)
		errc(1, ENAMETOOLONG, "%s", __func__);

	return buf;
}

static const char *
intstr(const struct doc *dc)
{
	switch (dc->dc_int) {
	case DOC_INDENT_PARENS:
		return "PARENS";
	case DOC_INDENT_FORCE:
		return "FORCE";
	case DOC_OPTIONAL_STICKY:
		return "STICKY";
	}
	return NULL;
}

static const char *
statestr(const struct doc_state *st, unsigned int depth, char *buf,
    size_t bufsiz)
{
	char mute[16], optline[16];
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

	if (doc_diff_is_mute(st))
		(void)snprintf(mute, sizeof(mute), "D");
	else
		(void)snprintf(mute, sizeof(mute), "%d", st->st_mute);

	if (st->st_optline >= DOC_OPTIONAL_STICKY)
		(void)snprintf(optline, sizeof(optline), "S");
	else
		(void)snprintf(optline, sizeof(optline), "%d", st->st_optline);

	n = snprintf(buf, bufsiz, "[D] [%c C=%-3u D=%-3u U=%s O=%s]",
	    mode, st->st_pos, depth, mute, optline);
	if (n < 0 || n >= (ssize_t)bufsiz)
		errc(1, ENAMETOOLONG, "%s", __func__);
	return buf;
}

static unsigned int
countlines(const char *str, size_t len)
{
	unsigned int nlines = 0;

	while (len > 0) {
		const char *p;

		p = memchr(str, '\n', len);
		if (p == NULL)
			break;
		nlines++;
		len -= (p - str) + 1;
		str = p + 1;
	}
	return nlines;
}
