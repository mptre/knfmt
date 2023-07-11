#include "doc.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "buffer.h"
#include "cdefs.h"
#include "consistency.h"
#include "diff.h"
#include "lexer.h"
#include "style.h"
#include "token.h"
#include "util.h"
#include "vector.h"

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

#define IS_DOC_INDENT_PARENS(indent) \
	((indent) > 0 && ((indent) & DOC_INDENT_PARENS))
#define IS_DOC_INDENT_FORCE(indent) \
	((indent) > 0 && ((indent) & DOC_INDENT_FORCE))
#define IS_DOC_INDENT_NEWLINE(indent) \
	((indent) > 0 && ((indent) & DOC_INDENT_NEWLINE))
#define IS_DOC_INDENT_WIDTH(indent) \
	((indent) > 0 && ((indent) & DOC_INDENT_WIDTH))

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
		struct token	*dc_tk;
	};

	/* value */
	union {
		VECTOR(struct doc_minimize)	dc_minimizers;
		struct {
			const char	*dc_str;
			size_t		 dc_len;
		};
		struct doc_align		dc_align;
		int				dc_int;
	};

	TAILQ_ENTRY(doc) dc_entry;
};

struct doc_state_indent {
	int	cur;	/* current indent */
	int	pre;	/* last emitted indent */
};

struct doc_state {
	const struct options		*st_op;
	const struct style		*st_st;
	struct buffer			*st_bf;
	struct lexer			*st_lx;

	enum doc_mode {
		BREAK,
		MUNGE,
	} st_mode;

	struct {
		const struct token	*verbatim;
		int			 group;
		int			 mute;
		unsigned int		 beg;
		unsigned int		 end;
		unsigned int		 muteline;
	} st_diff;

	struct doc_state_indent		 st_indent;

	struct {
		int	idx;	/* index of best minimizer */
		int	force;	/* index of minimizer with force flag */
	} st_minimize;

	struct {
		unsigned int	nfits;		/* # doc_fits() invocations */
		unsigned int	nlines;		/* # emitted lines */
		unsigned int	nexceeds;	/* # lines exceeding column limit */
	} st_stats;

	VECTOR(const struct doc *)	 st_walk;	/* stack used by doc_walk() */

	unsigned int			 st_col;
	unsigned int			 st_depth;
	unsigned int			 st_refit;
	unsigned int			 st_parens;
	/* # allowed consecutive new line(s). */
	unsigned int			 st_maxlines;
	/* # consecutive emitted new line(s). */
	unsigned int			 st_nlines;
	/* Pending new line emitted on next doc_print() invocation. */
	unsigned int			 st_newline;
	/* Missed new line while muted. */
	unsigned int			 st_muteline;
	/* Optional new line(s) honored. */
	int				 st_optline;
	/* Muted, doc_print() does not emit anything. */
	int				 st_mute;
	/* Flags given to doc_exec(). */
	unsigned int			 st_flags;
};

struct doc_state_snapshot {
	struct doc_state sn_st;
	struct {
		char	*ptr;
		size_t	 len;
	} sn_bf;
};

/*
 * Tokens and line numbers extracted from a group document that covers a diff
 * chunk.
 */
struct doc_diff {
	const struct token	*dd_verbatim;	/* last verbatim token not covered by chunk */
	unsigned int		 dd_threshold;	/* last seen token */
	unsigned int		 dd_first;	/* first token in group */
	unsigned int		 dd_chunk;	/* first token covered by chunk */
	int			 dd_covers;	/* doc_diff_covers() return value */
};

struct doc_fits {
	int		fits;
	unsigned int	optline;
};

/*
 * Description of per document type specific semantics.
 */
static const struct doc_description {
	const char	*name;

	/* Indicates which field in child union being used. */
	struct {
		uint8_t	many:1;		/* dc_list */
		uint8_t	one:1;		/* dc_doc */
		uint8_t	token:1;	/* dc_tk */
	} children;

	/* Indicates which field in value union being used. */
	struct {
		uint8_t	minimizers:1;	/* dc_minimizers */
		uint8_t	integer:1;	/* dc_int */
		uint8_t	string:1;	/* dc_str/dc_len */
	} value;
} doc_descriptions[] = {
	[DOC_CONCAT] = {
		.name		= "CONCAT",
		.children	= { .many = 1 },
	},
	[DOC_GROUP] = {
		.name		= "GROUP",
		.children	= { .one = 1 },
	},
	[DOC_INDENT] = {
		.name		= "INDENT",
		.children	= { .one = 1 },
	},
	[DOC_NOINDENT] = {
		.name		= "NOINDENT",
		.children	= { .one = 1 },
	},
	[DOC_ALIGN] = {
		.name		= "ALIGN",
		.value		= { .integer = 1 },
	},
	[DOC_LITERAL] = {
		.name		= "LITERAL",
		.children	= { .token = 1 },
		.value		= { .string = 1 },
	},
	[DOC_VERBATIM] = {
		.name		= "VERBATIM",
		.children	= { .token = 1 },
		.value		= { .string = 1 },
	},
	[DOC_LINE] = {
		.name		= "LINE",
	},
	[DOC_SOFTLINE] = {
		.name		= "SOFTLINE",
	},
	[DOC_HARDLINE] = {
		.name		= "HARDLINE",
	},
	[DOC_OPTLINE] = {
		.name		= "OPTLINE",
	},
	[DOC_MUTE] = {
		.name		= "MUTE",
		.value		= { .integer = 1 },
	},
	[DOC_OPTIONAL] = {
		.name		= "OPTIONAL",
		.children	= { .one = 1 },
	},
	[DOC_MINIMIZE] = {
		.name		= "MINIMIZE",
		.children	= { .one = 1 },
		.value		= { .minimizers = 1 },
	},
	[DOC_SCOPE] = {
		.name		= "SCOPE",
		.children	= { .one = 1 },
	},
	[DOC_MAXLINES] = {
		.name		= "MAXLINES",
		.children	= { .one = 1 },
		.value		= { .integer = 1 },
	},
};

static void		doc_exec1(const struct doc *, struct doc_state *);
static void		doc_exec_indent(const struct doc *, struct doc_state *);
static void		doc_exec_align(const struct doc *, struct doc_state *);
static void		doc_exec_verbatim(const struct doc *,
    struct doc_state *);
static void		doc_exec_minimize(const struct doc *,
    struct doc_state *);
static void		doc_exec_minimize_indent(const struct doc *,
    struct doc_state *);
static void		doc_exec_minimize_indent1(struct doc *,
    struct doc_state *, int);
static void		doc_exec_scope(const struct doc *, struct doc_state *);
static void		doc_exec_maxlines(const struct doc *,
    struct doc_state *);
static void		doc_walk(const struct doc *, struct doc_state *,
    int (*)(const struct doc *, struct doc_state *, void *), void *);
static int		doc_fits(const struct doc *, struct doc_state *);
static int		doc_fits1(const struct doc *, struct doc_state *,
    void *);
static unsigned int	doc_indent(const struct doc *, struct doc_state *, int);
static unsigned int	doc_indent1(const struct doc *, struct doc_state *,
    int, int);
static void		doc_trim_spaces(const struct doc *, struct doc_state *);
static void		doc_trim_lines(const struct doc *, struct doc_state *);
static int		doc_is_mute(const struct doc_state *);
static int		doc_parens_align(const struct doc_state *);
static int		doc_has_list(const struct doc *);
static unsigned int	doc_column(struct doc_state *, const char *, size_t);
static int		doc_max1(const struct doc *, struct doc_state *,
    void *);

static void	doc_state_init(struct doc_state *, struct doc_exec_arg *,
    enum doc_mode);
static void	doc_state_reset(struct doc_state *);
static void	doc_state_reset_lines(struct doc_state *);
static void	doc_state_snapshot(struct doc_state_snapshot *,
    const struct doc_state *);
static void	doc_state_snapshot_restore(const struct doc_state_snapshot *,
    struct doc_state *);
static void	doc_state_snapshot_reset(struct doc_state_snapshot *);

#define DOC_DIFF(st) (((st)->st_flags & DOC_EXEC_DIFF))

static int		doc_diff_group_enter(const struct doc *,
    struct doc_state *);
static void		doc_diff_group_leave(const struct doc *,
    struct doc_state *, int);
static void		doc_diff_mute_enter(const struct doc *,
    struct doc_state *);
static void		doc_diff_mute_leave(const struct doc *,
    struct doc_state *);
static void		doc_diff_literal(const struct doc *,
    struct doc_state *);
static unsigned int	doc_diff_verbatim(const struct doc *,
    struct doc_state *);
static void		doc_diff_exit(const struct doc *, struct doc_state *);
static void		doc_diff_emit(const struct doc *, struct doc_state *,
    unsigned int, unsigned int);
static int		doc_diff_covers(const struct doc *, struct doc_state *,
    void *);
static int		doc_diff_is_mute(const struct doc_state *);

#define doc_diff_leave(a, b, c) \
	doc_diff_leave0((a), (b), (c), __func__)
static void	doc_diff_leave0(const struct doc *, struct doc_state *,
    unsigned int, const char *);

#define DOC_PRINT_INDENT	0x00000001u
#define DOC_PRINT_NEWLINE	0x00000002u
#define DOC_PRINT_FORCE		0x00000004u

static void	doc_print(const struct doc *, struct doc_state *, const char *,
    size_t, unsigned int);

#define doc_trace(dc, st, fmt, ...) do {				\
	if ((st)->st_flags & DOC_EXEC_TRACE)				\
		doc_trace0((dc), (st), (fmt), __VA_ARGS__);		\
} while (0)
static void	doc_trace0(const struct doc *, const struct doc_state *,
    const char *, ...)
	__attribute__((__format__(printf, 3, 4)));

#define doc_trace_enter(dc, st) do {					\
	if ((st)->st_flags & DOC_EXEC_TRACE)				\
		doc_trace_enter0((dc), (st));				\
} while (0)
static void	doc_trace_enter0(const struct doc *, struct doc_state *);

#define doc_trace_leave(dc, st) do {					\
	if ((st)->st_flags & DOC_EXEC_TRACE)				\
		doc_trace_leave0((dc), (st));				\
} while (0)
static void	doc_trace_leave0(const struct doc *, struct doc_state *);

static char		*docstr(const struct doc *, char *, size_t);
static void		 indentstr(const struct doc *,
    const struct doc_state *, struct buffer *);
static const char	*statestr(const struct doc_state *, unsigned int,
    char *, size_t);

static unsigned int	countlines(const char *, size_t);

void
doc_exec(struct doc_exec_arg *arg)
{
	const struct doc *dc = arg->dc;
	struct doc_state st;

	ASSERT_CONSISTENCY(arg->flags & DOC_EXEC_DIFF, arg->lx);

	buffer_reset(arg->bf);
	doc_state_init(&st, arg, BREAK);
	doc_exec1(dc, &st);
	if (arg->flags & DOC_EXEC_TRIM)
		doc_trim_lines(dc, &st);
	doc_state_reset(&st);
	doc_diff_exit(dc, &st);
	doc_trace(dc, &st, "%s: nfits %u", __func__, st.st_stats.nfits);
}

unsigned int
doc_width(struct doc_exec_arg *arg)
{
	struct doc_state st;

	buffer_reset(arg->bf);
	doc_state_init(&st, arg, MUNGE);
	st.st_op = arg->op;
	st.st_st = arg->st;
	st.st_bf = arg->bf;
	doc_exec1(arg->dc, &st);
	doc_state_reset(&st);
	return st.st_col;
}

void
doc_free(struct doc *dc)
{
	const struct doc_description *desc;

	if (dc == NULL)
		return;
	desc = &doc_descriptions[dc->dc_type];

	if (desc->children.many) {
		struct doc *concat;

		while ((concat = TAILQ_FIRST(&dc->dc_list)) != NULL) {
			TAILQ_REMOVE(&dc->dc_list, concat, dc_entry);
			doc_free(concat);
		}
	} else if (desc->children.one) {
		doc_free(dc->dc_doc);
	}

	if (desc->children.token) {
		if (dc->dc_tk != NULL)
			token_rele(dc->dc_tk);
	} else if (desc->value.minimizers) {
		VECTOR_FREE(dc->dc_minimizers);
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

int
doc_remove_tail(struct doc *parent)
{
	struct doc *dc;

	assert(doc_has_list(parent));
	dc = TAILQ_LAST(&parent->dc_list, doc_list);
	if (dc == NULL)
		return 0;
	TAILQ_REMOVE(&parent->dc_list, dc, dc_entry);
	doc_free(dc);
	return 1;
}

void
doc_set_indent(struct doc *dc, unsigned int indent)
{
	dc->dc_int = (int)indent;
}

void
doc_set_dedent(struct doc *dc, unsigned int indent)
{
	dc->dc_int = -(int)indent;
}

void
doc_set_align(struct doc *dc, const struct doc_align *align)
{
	dc->dc_align = *align;
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

void
doc_append_before(struct doc *dc, struct doc *before)
{
	TAILQ_INSERT_BEFORE(before, dc, dc_entry);
}

struct doc *
doc_alloc0(enum doc_type type, struct doc *parent, int val, const char *fun,
    int lno)
{
	struct doc *dc;

	dc = ecalloc(1, sizeof(*dc));
	dc->dc_type = type;
	dc->dc_fun = fun;
	dc->dc_lno = lno;
	dc->dc_int = val;
	if (doc_has_list(dc))
		TAILQ_INIT(&dc->dc_list);
	if (parent != NULL)
		doc_append(dc, parent);

#ifdef __COVERITY__
	/*
	 * Coverity cannot deduce that documents reassembles a tree like
	 * structure which is always freed. Instead of annotating all call
	 * sites, favor this dirty hack.
	 */
	static struct doc *leaked_storage;
	leaked_storage = dc;
#endif

	return dc;
}

struct doc *
doc_alloc_indent0(unsigned int val, struct doc *dc, const char *fun, int lno)
{
	struct doc *indent;

	indent = doc_alloc0(DOC_INDENT, dc, 0, fun, lno);
	doc_set_indent(indent, val);
	return doc_alloc0(DOC_CONCAT, indent, 0, fun, lno);
}

struct doc *
doc_alloc_dedent0(unsigned int val, struct doc *dc, const char *fun, int lno)
{
	struct doc *indent;

	indent = doc_alloc0(DOC_INDENT, dc, 0, fun, lno);
	doc_set_dedent(indent, val);
	return doc_alloc0(DOC_CONCAT, indent, 0, fun, lno);
}

struct doc *
doc_minimize0(struct doc *parent, const struct doc_minimize *minimizers,
    size_t nminimizers, const char *fun, int lno)
{
	struct doc *dc;
	size_t i;

	dc = doc_alloc0(DOC_MINIMIZE, parent, 0, fun, lno);
	if (VECTOR_INIT(dc->dc_minimizers))
		err(1, NULL);
	if (VECTOR_RESERVE(dc->dc_minimizers, nminimizers))
		err(1, NULL);
	for (i = 0; i < nminimizers; i++) {
		struct doc_minimize *dst;

		dst = VECTOR_ALLOC(dc->dc_minimizers);
		if (dst == NULL)
			err(1, NULL);
		*dst = minimizers[i];
	}
	return doc_alloc0(DOC_CONCAT, dc, 0, fun, lno);
}

struct doc *
doc_literal0(const char *str, size_t len, struct doc *dc, const char *fun,
    int lno)
{
	struct doc *literal;

	literal = doc_alloc0(DOC_LITERAL, dc, 0, fun, lno);
	literal->dc_str = str;
	literal->dc_len = len > 0 ? len : strlen(str);
	return literal;
}

struct doc *
doc_token0(const struct token *tk, struct doc *dc, enum doc_type type,
    const char *fun, int lno)
{
	struct doc *token;
	struct token *nx, *prefix, *suffix;

	if (tk->tk_flags & TOKEN_FLAG_UNMUTE)
		doc_alloc0(DOC_MUTE, dc, -1, fun, lno);

	TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry)
		doc_token0(prefix, dc, DOC_VERBATIM, __func__, __LINE__);

	token = doc_alloc0(type, dc, 0, fun, lno);
	/* Must be mutable for reference counting. */
	token->dc_tk = (struct token *)tk;
	token_ref(token->dc_tk);
	token->dc_str = tk->tk_str;
	token->dc_len = tk->tk_len;

	TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
		if (suffix->tk_flags & TOKEN_FLAG_DISCARD)
			continue;
		if (suffix->tk_flags & TOKEN_FLAG_OPTLINE) {
			doc_alloc(DOC_OPTLINE, dc);
		} else {
			doc_token0(suffix, dc, DOC_VERBATIM, __func__,
			    __LINE__);
		}
	}

	/* Mute if we're about to branch. */
	nx = token_next(tk);
	if (nx != NULL && token_is_branch(nx))
		doc_alloc0(DOC_MUTE, dc, 1, fun, lno);

	return token;
}

/*
 * Returns the maximum value associated with the given document.
 */
int
doc_max(const struct doc *dc)
{
	struct doc_state st;
	struct doc_exec_arg arg = {0};
	int max = 0;

	doc_state_init(&st, &arg, BREAK);
	doc_walk(dc, &st, doc_max1, &max);
	doc_state_reset(&st);
	return max;
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

		TAILQ_FOREACH(concat, &dc->dc_list, dc_entry)
			doc_exec1(concat, st);

		break;
	}

	case DOC_GROUP: {
		unsigned int oldmode;
		int diff;

		diff = doc_diff_group_enter(dc, st);
		switch (st->st_mode) {
		case MUNGE:
			if (st->st_refit == 0) {
				doc_exec1(dc->dc_doc, st);
				break;
			}
			FALLTHROUGH;
		case BREAK:
			st->st_refit = 0;
			oldmode = st->st_mode;
			st->st_mode = doc_fits(dc, st) ? MUNGE : BREAK;
			doc_exec1(dc->dc_doc, st);
			st->st_mode = oldmode;
			break;
		}
		doc_diff_group_leave(dc, st, diff);

		break;
	}

	case DOC_INDENT:
		doc_exec_indent(dc, st);
		break;

	case DOC_NOINDENT: {
		struct doc_state_indent oldindent;

		doc_trim_spaces(dc, st);
		oldindent = st->st_indent;
		memset(&st->st_indent, 0, sizeof(st->st_indent));
		doc_exec1(dc->dc_doc, st);
		st->st_indent = oldindent;
		break;
	}

	case DOC_ALIGN:
		doc_exec_align(dc, st);
		break;

	case DOC_LITERAL:
		doc_diff_literal(dc, st);
		doc_print(dc, st, dc->dc_str, dc->dc_len, DOC_PRINT_INDENT);
		break;

	case DOC_VERBATIM:
		doc_exec_verbatim(dc, st);
		break;

	case DOC_LINE:
		switch (st->st_mode) {
		case BREAK:
			doc_print(dc, st, "\n", 1, DOC_PRINT_INDENT);
			break;
		case MUNGE:
			doc_print(dc, st, " ", 1, DOC_PRINT_INDENT);
			doc_trace(dc, st, "%s: refit %u -> %d", __func__,
			    st->st_refit, 1);
			st->st_refit = 1;
			break;
		}
		break;

	case DOC_SOFTLINE:
		switch (st->st_mode) {
		case BREAK:
			doc_print(dc, st, "\n", 1, DOC_PRINT_INDENT);
			break;
		case MUNGE:
			break;
		}
		break;

	case DOC_HARDLINE:
		doc_print(dc, st, "\n", 1, DOC_PRINT_INDENT);
		break;

	case DOC_OPTLINE:
		/*
		 * Instruct the next doc_print() invocation to emit a new line.
		 * Necessary in order to get indentation right.
		 */
		if (st->st_optline)
			st->st_newline = 1;
		break;

	case DOC_MUTE:
		/*
		 * While going mute, check if any new line is missed while being
		 * inside a diff chunk.
		 */
		if (st->st_mute == 0 && dc->dc_int > 0)
			doc_diff_mute_enter(dc, st);

		if (dc->dc_int > 0 || st->st_mute >= -dc->dc_int)
			st->st_mute += dc->dc_int;

		/*
		 * While going unmute, instruct the next doc_print() invocation
		 * to emit any missed new line.
		 */
		if (st->st_mute == 0) {
			unsigned int muteline;

			doc_diff_mute_leave(dc, st);
			muteline = st->st_muteline;
			doc_state_reset_lines(st);
			st->st_newline = muteline;
		}
		break;

	case DOC_OPTIONAL: {
		int oldoptline = st->st_optline;

		st->st_optline++;
		doc_exec1(dc->dc_doc, st);
		/* Note, could already be cleared by doc_print(). */
		if (oldoptline <= st->st_optline)
			st->st_optline = oldoptline;
		break;
	}

	case DOC_MINIMIZE:
		doc_exec_minimize(dc, st);
		break;

	case DOC_SCOPE:
		doc_exec_scope(dc, st);
		break;

	case DOC_MAXLINES:
		doc_exec_maxlines(dc, st);
		break;
	}

	doc_trace_leave(dc, st);
}

static void
doc_exec_indent(const struct doc *dc, struct doc_state *st)
{
	unsigned int oldparens = 0;
	int indent = 0;

	if (IS_DOC_INDENT_PARENS(dc->dc_int)) {
		oldparens = st->st_parens;
		if (doc_parens_align(st))
			st->st_parens++;
	} else if (IS_DOC_INDENT_FORCE(dc->dc_int)) {
		doc_indent(dc, st, st->st_indent.cur);
	} else if (IS_DOC_INDENT_NEWLINE(dc->dc_int)) {
		if (st->st_stats.nlines > 0) {
			indent = dc->dc_int & ~DOC_INDENT_NEWLINE;
			st->st_indent.cur += indent;
		}
	} else if (IS_DOC_INDENT_WIDTH(dc->dc_int)) {
		if (!st->st_newline) {
			indent = (int)st->st_col - st->st_indent.cur;
			st->st_indent.cur += indent;
		}
	} else {
		indent = dc->dc_int;
		st->st_indent.cur += indent;
	}
	doc_exec1(dc->dc_doc, st);
	if (IS_DOC_INDENT_PARENS(dc->dc_int)) {
		st->st_parens = oldparens;
	} else if (IS_DOC_INDENT_FORCE(dc->dc_int)) {
		/* nothing */
	} else {
		st->st_indent.cur -= indent;
	}
}

static void
doc_exec_align(const struct doc *dc, struct doc_state *st)
{
	if (dc->dc_align.tabalign) {
		unsigned int indent = dc->dc_align.indent;

		while (indent > 0) {
			unsigned int n;

			n = doc_indent1(dc, st, 8, 1);
			indent = n < indent ? indent - n : 0;
		}
		doc_indent1(dc, st, (int)dc->dc_align.spaces, 0);
	} else {
		doc_indent1(dc, st,
		    (int)(dc->dc_align.indent + dc->dc_align.spaces), 0);
	}
}

static void
doc_exec_verbatim(const struct doc *dc, struct doc_state *st)
{
	struct token *tk = dc->dc_tk;
	unsigned int diff, oldcol;
	int unmute = 0;
	int isblock, isnewline;

	if (doc_is_mute(st)) {
		if (DOC_DIFF(st) &&
		    st->st_diff.verbatim == tk)
			unmute = 1;
		else
			return;
	}

	diff = doc_diff_verbatim(dc, st);

	isblock = dc->dc_len > 1 && dc->dc_str[dc->dc_len - 1] == '\n';
	isnewline = dc->dc_len == 1 && dc->dc_str[0] == '\n';

	/* Verbatims must never be indented. */
	doc_trim_spaces(dc, st);
	oldcol = st->st_col;

	/* Verbatim blocks must always start on a new line. */
	if (isblock && st->st_col > 0)
		st->st_newline = 1;

	doc_print(dc, st, dc->dc_str, dc->dc_len, 0);

	/* Restore indentation in diff mode. */
	if (unmute)
		st->st_diff.verbatim = NULL;

	/* Restore indentation after emitting a verbatim block or new line. */
	if (isblock || isnewline) {
		struct doc_state_indent *it = &st->st_indent;
		int indent;

		if (oldcol > 0) {
			/*
			 * The line is not empty after trimming. Assume this a
			 * continuation in which the current indentation level
			 * must be used.
			 */
			indent = it->cur;
		} else {
			/*
			 * The line is empty after trimming. Assume this is not
			 * a continuation in which the previously emitted
			 * indentation must be used.
			 */
			indent = it->pre;
		}
		doc_indent(dc, st, indent);
	}

	doc_diff_leave(dc, st, diff);
}

static void
doc_exec_minimize(const struct doc *dc, struct doc_state *st)
{
	/* All minimizers are expected to be of the same type. */
	switch (dc->dc_minimizers[0].type) {
	case DOC_MINIMIZE_INDENT:
		doc_exec_minimize_indent(dc, st);
		break;
	}
}

static void
doc_exec_minimize_indent(const struct doc *cdc, struct doc_state *st)
{
	/* Ugly, must be mutable for value mutation. */
	struct doc *dc = (struct doc *)cdc;
	VECTOR(struct doc_minimize) minimizers;
	struct doc_state_snapshot sn;
	ssize_t best = -1;
	size_t i;
	unsigned int nlines = 0;
	unsigned int nexceeds = 0;
	double minpenality = DBL_MAX;

	if (st->st_minimize.idx != -1) {
		doc_exec_minimize_indent1(dc, st, st->st_minimize.idx);
		return;
	}

	doc_state_snapshot(&sn, st);
	minimizers = dc->dc_minimizers;
	for (i = 0; i < VECTOR_LENGTH(minimizers); i++) {
		memset(&st->st_stats, 0, sizeof(st->st_stats));
		st->st_minimize.force = -1;
		st->st_flags &= ~DOC_EXEC_TRACE;

		st->st_minimize.idx = i;
		doc_exec_minimize_indent1(dc, st, i);
		st->st_minimize.idx = -1;
		if (st->st_minimize.force != -1)
			minimizers[i].flags |= DOC_MINIMIZE_FORCE;
		if (st->st_stats.nlines > nlines)
			nlines = st->st_stats.nlines;
		minimizers[i].penality.nlines = st->st_stats.nlines;
		if (st->st_stats.nexceeds > nexceeds)
			nexceeds = st->st_stats.nexceeds;
		minimizers[i].penality.nexceeds = st->st_stats.nexceeds;
		doc_state_snapshot_restore(&sn, st);
	}
	dc->dc_minimizers = minimizers;
	doc_state_snapshot_reset(&sn);

	for (i = 0; i < VECTOR_LENGTH(dc->dc_minimizers); i++) {
		struct doc_minimize *mi = &dc->dc_minimizers[i];
		double p = 0;

		if (nlines > 0)
			p += mi->penality.nlines / (double)nlines;
		if (nexceeds > 0)
			p += mi->penality.nexceeds / (double)nexceeds;
		p /= 2;
		mi->penality.sum = p;

		if (mi->flags & DOC_MINIMIZE_FORCE) {
			best = (ssize_t)i;
			break;
		}
		if (p < minpenality) {
			minpenality = p;
			best = (ssize_t)i;
		}
	}

	if (st->st_flags & DOC_EXEC_TRACE) {
		for (i = 0; i < VECTOR_LENGTH(dc->dc_minimizers); i++) {
			const struct doc_minimize *mi = &dc->dc_minimizers[i];
			const char *suffix = "";

			if (mi->flags & DOC_MINIMIZE_FORCE)
				suffix = ", force";
			else if ((ssize_t)i == best)
				suffix = ", best";
			doc_trace(dc, st, "%s: type indent, penality %.2f, "
			    "indent %u, nlines %u, nexceeds %u%s",
			    __func__, mi->penality.sum, mi->indent,
			    mi->penality.nlines, mi->penality.nexceeds,
			    suffix);
		}
	}

	assert(best != -1);
	assert(st->st_minimize.idx == -1);
	st->st_minimize.idx = best;
	doc_exec_minimize_indent1(dc, st, best);
	st->st_minimize.idx = -1;
}

static void
doc_exec_minimize_indent1(struct doc *dc, struct doc_state *st, int idx)
{
	VECTOR(struct doc_minimize) minimizers = dc->dc_minimizers;

	if (minimizers[idx].flags & DOC_MINIMIZE_FORCE)
		st->st_minimize.force = idx;

	doc_set_indent(dc, minimizers[idx].indent);
	doc_exec_indent(dc, st);
	dc->dc_minimizers = minimizers;
}

static void
doc_exec_scope(const struct doc *dc, struct doc_state *st)
{
	st->st_stats.nlines = 0;
	doc_exec1(dc->dc_doc, st);
}

static void
doc_exec_maxlines(const struct doc *dc, struct doc_state *st)
{
	unsigned int restore = st->st_maxlines;

	st->st_maxlines = (unsigned int)dc->dc_int;
	doc_exec1(dc->dc_doc, st);
	st->st_maxlines = restore;
}

static void
doc_walk(const struct doc *dc, struct doc_state *st,
    int (*cb)(const struct doc *, struct doc_state *, void *), void *arg)
{
	const struct doc **dst;

	if (st->st_walk == NULL) {
		if (VECTOR_INIT(st->st_walk))
			err(1, NULL);
	}

	/* Recursion flatten into a loop for increased performance. */
	dst = VECTOR_ALLOC(st->st_walk);
	if (dst == NULL)
		err(1, NULL);
	*dst = dc;
	while (!VECTOR_EMPTY(st->st_walk)) {
		const struct doc **tail;
		const struct doc_description *desc;

		tail = VECTOR_POP(st->st_walk);
		dc = *tail;
		desc = &doc_descriptions[dc->dc_type];

		if (!cb(dc, st, arg))
			break;

		if (desc->children.many) {
			const struct doc_list *dl = &dc->dc_list;

			TAILQ_FOREACH_REVERSE(dc, dl, doc_list, dc_entry) {
				dst = VECTOR_ALLOC(st->st_walk);
				if (dst == NULL)
					err(1, NULL);
				*dst = dc;
			}
		} else if (desc->children.one) {
			dst = VECTOR_ALLOC(st->st_walk);
			if (dst == NULL)
				err(1, NULL);
			*dst = dc->dc_doc;
		}
	}
	VECTOR_CLEAR(st->st_walk);
}

static int
doc_fits(const struct doc *dc, struct doc_state *st)
{
	struct doc_state fst;
	struct doc_fits fits = {
		.fits		= 1,
		.optline	= 0,
	};
	unsigned int col = 0;
	unsigned int optline = 0;

	if (st->st_flags & DOC_EXEC_TRACE)
		st->st_stats.nfits++;

	memcpy(&fst, st, sizeof(fst));
	/* Should not perform any printing. */
	fst.st_bf = NULL;
	fst.st_mode = MUNGE;
	fst.st_walk = NULL;
	doc_walk(dc, &fst, doc_fits1, &fits);
	doc_state_reset(&fst);
	col = fst.st_col;
	optline = fits.optline;
	doc_trace(dc, st, "%s: %u %s %u, optline %u", __func__,
	    col, fits.fits ? "<=" : ">", style(st->st_st, ColumnLimit),
	    optline);

	return fits.fits;
}

static int
doc_fits1(const struct doc *dc, struct doc_state *st, void *arg)
{
	struct doc_fits *fits = arg;

	if (st->st_newline) {
		fits->fits = st->st_col <= style(st->st_st, ColumnLimit);
		return 0;
	}

	switch (dc->dc_type) {
	case DOC_ALIGN: {
		unsigned int indent = dc->dc_align.indent;
		unsigned int spaces = dc->dc_align.spaces;

		if (dc->dc_align.tabalign) {
			while (indent > 0) {
				unsigned int n;

				n = doc_column(st, "\t", 1);
				indent = n < indent ? indent - n : 0;
			}
		}
		st->st_col += indent + spaces;
		break;
	}

	case DOC_LITERAL:
		doc_column(st, dc->dc_str, dc->dc_len);
		break;

	case DOC_VERBATIM:
		if (dc->dc_str[dc->dc_len - 1] != '\n')
			doc_column(st, dc->dc_str, dc->dc_len);
		break;

	case DOC_LINE:
		st->st_col++;
		break;

	case DOC_OPTLINE:
		if (st->st_optline) {
			fits->optline = 1;
			return 0;
		}
		break;

	case DOC_OPTIONAL:
		st->st_optline++;
		break;

	case DOC_CONCAT:
	case DOC_GROUP:
	case DOC_INDENT:
	case DOC_NOINDENT:
	case DOC_SOFTLINE:
	case DOC_HARDLINE:
	case DOC_MUTE:
	case DOC_MINIMIZE:
	case DOC_SCOPE:
	case DOC_MAXLINES:
		break;
	}

	if (st->st_col > style(st->st_st, ColumnLimit)) {
		fits->fits = 0;
		return 0;
	}
	return 1;
}

static unsigned int
doc_indent(const struct doc *dc, struct doc_state *st, int indent)
{
	if (st->st_parens > 0) {
		/* Align with the left parenthesis on the previous line. */
		indent = st->st_indent.pre + (int)st->st_parens;
	} else {
		st->st_indent.pre = indent;
	}
	return doc_indent1(dc, st, indent, style(st->st_st, UseTab) != Never);
}

static unsigned int
doc_indent1(const struct doc *UNUSED(dc), struct doc_state *st,
    int indent, int usetabs)
{
	unsigned int oldcol = st->st_col;

	st->st_col = strindent_buffer(st->st_bf, indent, usetabs, st->st_col);
	return st->st_col - oldcol;
}

static void
doc_print(const struct doc *dc, struct doc_state *st, const char *str,
    size_t len, unsigned int flags)
{
	int ismute = doc_is_mute(st) && (flags & DOC_PRINT_FORCE) == 0;
	int isnewline = len == 1 && str[0] == '\n';

	if (isnewline && ismute)
		st->st_muteline = 1;

	/* Emit pending new line. */
	if (st->st_newline) {
		int isspace = len == 1 && str[0] == ' ';

		/* Redundant if a new line is about to be emitted. */
		if (isspace)
			return;

		/* DOC_OPTLINE has the same semantics as DOC_LINE. */
		st->st_refit = 1;
		st->st_newline = 0;
		doc_print(dc, st, "\n", 1, flags | DOC_PRINT_NEWLINE);
		if (isnewline)
			return;
	}

	if (isnewline) {
		if (st->st_nlines >= st->st_maxlines)
			return;
		st->st_nlines++;
		st->st_stats.nlines++;

		/*
		 * Suppress optional line(s) while emitting a line. Mixing the
		 * two results in odd formatting.
		 */
		if ((flags & DOC_PRINT_NEWLINE) == 0 && st->st_optline) {
			doc_trace(dc, st, "%s: optline %d -> 0",
			    __func__, st->st_optline);
			st->st_optline = 0;
		}
	} else {
		st->st_nlines = 0;
	}

	if (isnewline) {
		doc_trim_spaces(dc, st);
		/* Invalidate choice of best minimizer. */
		if (st->st_minimize.force != -1)
			st->st_minimize.idx = -1;
	}
	if (!ismute)
		buffer_puts(st->st_bf, str, len);
	doc_column(st, str, len);

	if (isnewline && (flags & DOC_PRINT_INDENT))
		doc_indent(dc, st, st->st_indent.cur);
}

static void
doc_trim_spaces(const struct doc *dc, struct doc_state *st)
{
	const char *buf = buffer_get_ptr(st->st_bf);
	size_t buflen = buffer_get_len(st->st_bf);
	unsigned int oldcol = st->st_col;

	while (buflen > 0) {
		char ch;

		ch = buf[buflen - 1];
		if (ch != ' ' && ch != '\t')
			break;
		buflen -= buffer_pop(st->st_bf, 1);
		st->st_col -= ch == '\t' ? 8 - (st->st_col % 8) : 1;
	}
	if (oldcol > st->st_col) {
		doc_trace(dc, st, "%s: trimmed %u character(s)", __func__,
		    oldcol - st->st_col);
	}
}

/*
 * Trim trailing consequtive new line(s) from the rendered document.
 * Necessary if a verbatim document is at the end which may include up to two
 * new lines.
 */
static void
doc_trim_lines(const struct doc *dc, struct doc_state *st)
{
	const char *buf = buffer_get_ptr(st->st_bf);
	size_t buflen = buffer_get_len(st->st_bf);
	int ntrim = 0;

	while (buflen > 1 &&
	    buf[buflen - 1] == '\n' && buf[buflen - 2] == '\n') {
		buflen -= buffer_pop(st->st_bf, 1);
		ntrim++;
	}
	if (ntrim > 0)
		doc_trace(dc, st, "%s: trimmed %d line(s)", __func__, ntrim);
}

static int
doc_diff_group_enter(const struct doc *dc, struct doc_state *st)
{
	struct doc_diff dd;
	const struct diffchunk *du;
	unsigned int end;

	if (!DOC_DIFF(st))
		return 0;

	/*
	 * Only applicable while entering the first group. Unless the group
	 * above us was ignored, see below.
	 */
	if (st->st_diff.group)
		return 0;

	/*
	 * Check if the current group is covered by a diff chunk. If so, we must
	 * start emitting documents nested under the same group.
	 *
	 * A group is something intended to fit on a single line. However,
	 * there are exceptions in the sense of groups spanning multiple lines;
	 * for instance brace initializers. Such groups are ignored allowing the
	 * first nested group covering a single line to be found.
	 */
	memset(&dd, 0, sizeof(dd));
	dd.dd_threshold = st->st_diff.beg;
	doc_walk(dc, st, doc_diff_covers, &dd);
	switch (dd.dd_covers) {
	case -1:
		/*
		 * The group spans multiple lines. Ignore it and keep evaluating
		 * nested groups on subsequent invocations of this routine.
		 */
		return 0;
	case 0:
		/*
		 * The group is not covered by any diff chunk and all nested
		 * groups can therefore be ignored. However, if the previous
		 * group above us touched lines after the diff chunk due to
		 * reformatting make sure to reset the state.
		 */
		if (st->st_diff.end > 0)
			doc_diff_leave(dc, st, 1);
		st->st_diff.group = 1;
		return 1;
	case 1:
		/*
		 * The group is covered by a diff chunk. Make sure to leave any
		 * previous diff chunk if we're entering a new one.
		 */
		if (st->st_diff.end > 0 && dd.dd_first > st->st_diff.end)
			doc_diff_leave(dc, st, 1);
		break;
	}

	st->st_diff.group = 1;

	doc_trace(dc, st, "%s: enter chunk: beg %u, end %u, first %u, "
	    "chunk %u, seen %d", __func__,
	    st->st_diff.beg, st->st_diff.end,
	    dd.dd_first, dd.dd_chunk,
	    st->st_diff.end > 0);

	if (st->st_diff.end > 0) {
		/*
		 * The diff chunk is spanning more than one group. Any preceding
		 * verbatim lines are already emitted at this point.
		 */
		return 1;
	}

	du = lexer_get_diffchunk(st->st_lx, dd.dd_chunk);
	if (du == NULL)
		return 1;
	doc_trace(dc, st, "%s: chunk range [%u-%u]", __func__,
	    du->du_beg, du->du_end);

	/*
	 * Take a tentative note on which line the diff chunk ends. Note that if
	 * the current group spans beyond the diff chunk, the end line will be
	 * adjusted. This can happen when reformatting causes lines to be
	 * merged.
	 */
	st->st_diff.end = du->du_end;

	/*
	 * We could still be in a muted section of the document, ignore and then
	 * restore in doc_diff_leave();
	 */
	st->st_diff.mute = st->st_mute;
	st->st_mute = 0;
	if (dd.dd_verbatim != NULL) {
		const struct token *tk = dd.dd_verbatim;

		/*
		 * The diff chunk is preceeded with one or many verbatim tokens
		 * inside the same group, caused by verbatim tokens not being
		 * placed in seperate groups. Such verbatim tokens must
		 * not be formatted and we therefore stay mute until moving past
		 * the last verbatim token not covered by the diff chunk.
		 */
		st->st_diff.verbatim = tk;
		end = tk->tk_lno + countlines(tk->tk_str, tk->tk_len);
		doc_trace(dc, st, "%s: verbatim mute [%u-%u)", __func__,
		    st->st_diff.beg, end);
	} else {
		end = dd.dd_first;
	}

	/*
	 * Emit any preceding line(s) not covered by the diff chunk. It is of
	 * importance to begin at the line from the first token covered by this
	 * group and not the first line covered by the diff chunk; as a group
	 * represents something intended to fit on a single line but the diff
	 * chunk might only touch a subset of the group.
	 */
	doc_diff_emit(dc, st, st->st_diff.beg, end);
	return 1;
}

static void
doc_diff_group_leave(const struct doc *UNUSED(dc), struct doc_state *st,
    int enter)
{
	if (enter == 0 || !DOC_DIFF(st))
		return;
	assert(st->st_diff.group == 1);
	st->st_diff.group = 0;
}

static void
doc_diff_mute_enter(const struct doc *UNUSED(dc), struct doc_state *st)
{
	if (doc_diff_is_mute(st))
		return;
	st->st_diff.muteline = 1;
}

static void
doc_diff_mute_leave(const struct doc *dc, struct doc_state *st)
{
	if (!DOC_DIFF(st))
		return;

	if (st->st_diff.muteline && st->st_muteline) {
		doc_trace(dc, st, "%s: muteline %u", __func__,
		    st->st_diff.muteline);
		doc_state_reset_lines(st);
		doc_print(dc, st, "\n", 1, DOC_PRINT_FORCE);
	}
	st->st_diff.muteline = 0;
}

static void
doc_diff_literal(const struct doc *dc, struct doc_state *st)
{
	unsigned int lno;

	if (!DOC_DIFF(st))
		return;
	if (dc->dc_tk == NULL || st->st_diff.end == 0)
		return;

	lno = dc->dc_tk->tk_lno;
	if (st->st_diff.group) {
		if (lno > st->st_diff.end) {
			/*
			 * The current group spans beyond the diff chunk, adjust
			 * the end line. This can happen when reformatting
			 * causes lines to be merged.
			 */
			doc_trace(dc, st, "%s: end %u", __func__, lno);
			st->st_diff.end = lno;
		}
	} else if (lno > st->st_diff.end) {
		doc_diff_leave(dc, st, 1);
	}
}

static unsigned int
doc_diff_verbatim(const struct doc *dc, struct doc_state *st)
{
	unsigned int lno, n;

	if (!DOC_DIFF(st))
		return 0;

	assert(dc->dc_tk != NULL);
	lno = dc->dc_tk->tk_lno;
	if (lno == 0 || st->st_diff.end == 0)
		return 0;

	if (lno > st->st_diff.end) {
		doc_diff_leave(dc, st, 1);
		return 0;
	}

	/*
	 * A verbatim document could contain one or many hard line(s) which can
	 * takes us beyond the diff chunk. Signal that after emitting this
	 * document the diff chunk must be left.
	 */
	n = countlines(dc->dc_str, dc->dc_len);
	if (n > 0 && lno + n > st->st_diff.end) {
		unsigned int end;

		end = (lno + n) - st->st_diff.end;
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

	/*
	 * Ensure to leave any previous diff chunk, can happen if the last group
	 * is covered by a diff chunk.
	 */
	if (st->st_diff.end > 0)
		doc_diff_leave(dc, st, 1);
	doc_diff_emit(dc, st, st->st_diff.beg, 0);
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

	if (st->st_flags & DOC_EXEC_TRACE) {
		char *s;

		s = strnice(str, len);
		doc_trace(dc, st, "%s: verbatim \"%s\"", __func__, s);
		free(s);
	}

	doc_state_reset_lines(st);
	doc_trim_spaces(dc, st);
	doc_print(dc, st, str, len, DOC_PRINT_FORCE);
	doc_indent(dc, st, st->st_indent.cur);
}

/*
 * Returns non-zero if any document covers a token which is part of a diff
 * chunk.
 */
static int
doc_diff_covers(const struct doc *dc, struct doc_state *UNUSED(st), void *arg)
{
	struct doc_diff *dd = (struct doc_diff *)arg;

	switch (dc->dc_type) {
	case DOC_VERBATIM:
		if ((dc->dc_tk->tk_flags & TOKEN_FLAG_DIFF) == 0)
			dd->dd_verbatim = dc->dc_tk;
		FALLTHROUGH;
	case DOC_LITERAL:
		if (dc->dc_tk != NULL) {
			unsigned int lno = dc->dc_tk->tk_lno;

			/*
			 * If the current line number is less than the last one
			 * from the previous diff chunk, we have seen this
			 * document before. This could happen while traversing
			 * the same source code again after branching.
			 */
			if (lno < dd->dd_threshold)
				return 0;

			if (dd->dd_first == 0)
				dd->dd_first = lno;
			if (dc->dc_tk->tk_flags & TOKEN_FLAG_DIFF) {
				dd->dd_chunk = lno;
				dd->dd_covers = 1;
				return 0;
			}
		}
		break;

	case DOC_HARDLINE:
		dd->dd_covers = -1;
		return 0;

	default:
		break;
	}

	return 1;
}

/*
 * Returns non-zero if nothing should be emitted while being positioned on a
 * line not touched by the diff.
 */
static int
doc_diff_is_mute(const struct doc_state *st)
{
	return DOC_DIFF(st) &&
	    (st->st_diff.end == 0 || st->st_diff.verbatim != NULL);
}

static void
doc_diff_leave0(const struct doc *dc, struct doc_state *st, unsigned int end,
    const char *fun)
{
	if (end == 0)
		return;

	assert(st->st_diff.end > 0);
	assert(st->st_diff.verbatim == NULL);
	st->st_diff.beg = st->st_diff.end + end;
	st->st_diff.end = 0;
	st->st_mute = st->st_diff.mute;
	st->st_diff.mute = 0;
	doc_trace(dc, st, "doc_diff_leave: %s: leave chunk: beg %u",
	    fun, st->st_diff.beg);
	/*
	 * Emit pending new line immediately as we would otherwise lose track of
	 * it while being mute.
	 */
	if (st->st_newline)
		doc_print(dc, st, "\n", 1, DOC_PRINT_FORCE);
}

static int
doc_is_mute(const struct doc_state *st)
{
	return st->st_mute || doc_diff_is_mute(st);
}

/*
 * Returns non-zero if the current line is suitable for parenthesis alignment,
 * i.e. a line consisting of whitespace followed by one or many left
 * parenthesis. This is the desired outcome:
 *
 * 	if (a &&
 * 	    ((b &&
 * 	      c)))
 */
static int
doc_parens_align(const struct doc_state *st)
{
	const char *buf = buffer_get_ptr(st->st_bf);
	size_t buflen = buffer_get_len(st->st_bf);
	int nparens = 0;

	for (; buflen > 0 && buf[buflen - 1] == '('; buflen--)
		nparens++;
	if (nparens == 0 || buflen == 0)
		return 0;
	for (; buflen > 0; buflen--) {
		if (buf[buflen - 1] == '\n')
			break;
		if (buf[buflen - 1] != ' ' && buf[buflen - 1] != '\t')
			return 0;
	}
	return 1;
}

static int
doc_has_list(const struct doc *dc)
{
	return doc_descriptions[dc->dc_type].children.many;
}

/*
 * Set the column position, intended to be given the same string just added to
 * the document buffer.
 */
static unsigned int
doc_column(struct doc_state *st, const char *str, size_t len)
{
	unsigned int oldcol = st->st_col;

	st->st_col = strwidth(str, len, st->st_col);
	if (st->st_col > style(st->st_st, ColumnLimit))
		st->st_stats.nexceeds++;
	/* Cope new line(s). */
	return st->st_col > oldcol ? st->st_col - oldcol : 0;
}

static int
doc_max1(const struct doc *dc, struct doc_state *UNUSED(st), void *arg)
{
	int *max = arg;

	switch (dc->dc_type) {
	case DOC_SOFTLINE:
		if (dc->dc_int > *max)
			*max = dc->dc_int;
		break;
	default:
		break;
	}
	return 1;
}

static void
doc_state_init(struct doc_state *st, struct doc_exec_arg *arg,
    enum doc_mode mode)
{
	memset(st, 0, sizeof(*st));
	st->st_op = arg->op;
	st->st_st = arg->st;
	st->st_bf = arg->bf;
	st->st_lx = arg->lx;
	st->st_maxlines = 2;
	st->st_flags = arg->flags;
	st->st_mode = mode;
	st->st_diff.beg = 1;
	st->st_minimize.idx = -1;
	st->st_minimize.force = -1;
}

static void
doc_state_reset(struct doc_state *st)
{
	VECTOR_FREE(st->st_walk);
}

/*
 * Reset state related to emitting new lines.
 */
static void
doc_state_reset_lines(struct doc_state *st)
{
	st->st_nlines = 0;
	st->st_newline = 0;
	st->st_muteline = 0;
}

static void
doc_state_snapshot(struct doc_state_snapshot *sn, const struct doc_state *st)
{
	const char *buf = buffer_get_ptr(st->st_bf);
	size_t buflen = buffer_get_len(st->st_bf);

	sn->sn_st = *st;
	sn->sn_bf.ptr = emalloc(buflen);
	sn->sn_bf.len = buflen;
	memcpy(sn->sn_bf.ptr, buf, buflen);
}

static void
doc_state_snapshot_restore(const struct doc_state_snapshot *sn,
    struct doc_state *st)
{
	struct buffer *bf = st->st_bf;

	*st = sn->sn_st;
	buffer_reset(bf);
	buffer_puts(bf, sn->sn_bf.ptr, sn->sn_bf.len);
}

static void
doc_state_snapshot_reset(struct doc_state_snapshot *sn)
{
	free(sn->sn_bf.ptr);
}

static void
doc_trace0(const struct doc *UNUSED(dc), const struct doc_state *st,
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
doc_trace_enter0(const struct doc *dc, struct doc_state *st)
{
	char buf[128];
	const struct doc_description *desc = &doc_descriptions[dc->dc_type];
	unsigned int depth = st->st_depth;
	unsigned int i;

	st->st_depth++;

	fprintf(stderr, "%s ", statestr(st, st->st_depth, buf, sizeof(buf)));

	for (i = 0; i < depth; i++)
		fprintf(stderr, "  ");

	fprintf(stderr, "%s", docstr(dc, buf, sizeof(buf)));
	fprintf(stderr, "(");
	if (desc->children.many)
		fprintf(stderr, "[");
	if (desc->value.integer)
		fprintf(stderr, "%d", dc->dc_int);
	if (desc->value.string) {
		char *str;

		str = strnice(dc->dc_str, dc->dc_len);
		fprintf(stderr, "\"%s\", %zu", str, dc->dc_len);
		free(str);
	}

	switch (dc->dc_type) {
	case DOC_MINIMIZE:
		if (st->st_minimize.idx == -1) {
			fprintf(stderr, "-1");
			break;
		}
		FALLTHROUGH;

	case DOC_INDENT:
	case DOC_NOINDENT: {
		struct buffer *bf;

		bf = buffer_alloc(32);
		if (bf == NULL)
			err(1, NULL);
		indentstr(dc, st, bf);
		buffer_putc(bf, '\0');
		fprintf(stderr, "%s", buffer_get_ptr(bf));
		buffer_free(bf);
		break;
	}

	case DOC_LINE:
		if (st->st_mode == BREAK)
			fprintf(stderr, "\"\\n\", 1");
		else if (!st->st_newline)
			fprintf(stderr, "\" \", 1");
		else
			fprintf(stderr, "\"\", 0");
		break;

	case DOC_SOFTLINE:
		fprintf(stderr, "\"%s\", %d",
		    st->st_mode == BREAK ? "\\n" : "",
		    st->st_mode == BREAK ? 1 : 0);
		break;

	case DOC_HARDLINE:
		fprintf(stderr, "\"\\n\", 1");
		break;

	case DOC_OPTLINE:
		fprintf(stderr, "\"%s\", %d",
		    st->st_optline ? "\\n" : "", st->st_optline ? 1 : 0);
		break;

	default:
		break;
	}
	if (!desc->children.many && !desc->children.one)
		fprintf(stderr, ")");
	fprintf(stderr, "\n");
}

static void
doc_trace_leave0(const struct doc *dc, struct doc_state *st)
{
	char buf[128];
	const struct doc_description *desc = &doc_descriptions[dc->dc_type];
	unsigned int depth = st->st_depth;
	unsigned int i;

	st->st_depth--;

	if (!desc->children.many && !desc->children.one)
		return;

	fprintf(stderr, "%s ", statestr(st, depth, buf, sizeof(buf)));
	for (i = 0; i < st->st_depth; i++)
		fprintf(stderr, "  ");
	if (desc->children.many)
		fprintf(stderr, "]");
	if (desc->children.many || desc->children.one)
		fprintf(stderr, ")");
	fprintf(stderr, "\n");
}

static char *
docstr(const struct doc *dc, char *buf, size_t bufsiz)
{
	const char *name;
	int suffix = dc->dc_suffix != NULL;
	int n;

	name = doc_descriptions[dc->dc_type].name;
	n = snprintf(buf, bufsiz, "%s<%s:%d%s%s%s>",
	    name, dc->dc_fun, dc->dc_lno,
	    suffix ? ", \"" : "",
	    suffix ? dc->dc_suffix : "",
	    suffix ? "\"" : "");
	if (n < 0 || n >= (ssize_t)bufsiz)
		errc(1, ENAMETOOLONG, "%s", __func__);

	return buf;
}

static void
indentstr(const struct doc *dc, const struct doc_state *st, struct buffer *bf)
{
	int indent;

	if (dc->dc_type == DOC_MINIMIZE)
		indent = (int)dc->dc_minimizers[st->st_minimize.idx].indent;
	else
		indent = dc->dc_int;

	if (IS_DOC_INDENT_NEWLINE(indent)) {
		buffer_printf(bf, "NEWLINE, %d, %u",
		    dc->dc_int & ~DOC_INDENT_NEWLINE,
		    st->st_stats.nlines);
	} else if (IS_DOC_INDENT_PARENS(indent)) {
		buffer_printf(bf, "PARENS");
	} else if (IS_DOC_INDENT_FORCE(indent)) {
		buffer_printf(bf, "FORCE");
	} else if (IS_DOC_INDENT_WIDTH(indent)) {
		buffer_printf(bf, "WIDTH");
	} else {
		buffer_printf(bf, "%d", indent);
	}
}

static const char *
statestr(const struct doc_state *st, unsigned int depth, char *buf,
    size_t bufsiz)
{
	char mute[16];
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

	n = snprintf(buf, bufsiz, "[D] [%c C=%-3u D=%-3u U=%s O=%d]",
	    mode, st->st_col, depth, mute, st->st_optline);
	if (n < 0 || n >= (ssize_t)bufsiz)
		errc(1, ENAMETOOLONG, "%s", __func__);
	return buf;
}

static unsigned int
countlines(const char *str, size_t len)
{
	unsigned int nlines = 0;

	/* Only applicable to tokens ending with a hard line. */
	if (len > 0 && str[len - 1] != '\n')
		return 0;

	while (len > 0) {
		const char *p;
		size_t linelen;

		p = memchr(str, '\n', len);
		if (p == NULL)
			break;
		nlines++;
		linelen = (size_t)(p - str);
		len -= linelen + 1;
		str = p + 1;
	}
	return nlines;
}
