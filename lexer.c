#include "lexer.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/compiler.h"
#include "libks/string.h"
#include "libks/vector.h"

#include "diff.h"
#include "error.h"
#include "options.h"
#include "token.h"
#include "trace.h"
#include "util.h"

#ifdef HAVE_QUEUE
#  include <sys/queue.h>
#else
#  include "compat-queue.h"
#endif

struct lexer {
	struct lexer_state	 lx_st;
	struct lexer_callbacks	 lx_callbacks;
	const char		*lx_path;
	struct error		*lx_er;
	const struct options	*lx_op;
	const struct diffchunk	*lx_diff;

	struct {
		struct arena_scope	*eternal_scope;
	} lx_arena;

	struct {
		const struct buffer	*bf;
		const char		*ptr;
		size_t			 len;
	} lx_input;

	/* Line number to buffer offset mapping. */
	VECTOR(size_t)		 lx_lines;

	int			 lx_peek;

	struct token_list	 lx_tokens;
};

static void	lexer_free(void *);

static void		lexer_line_alloc(struct lexer *, unsigned int);
static unsigned int	lexer_column(const struct lexer *,
    const struct lexer_state *);

static void	lexer_expect_error(struct lexer *, int, const struct token *,
    const char *, int);

static int	lexer_peek_until_not_nested(struct lexer *, int,
    const struct token *, struct token **);

static const struct diffchunk	*lexer_get_diffchunk(const struct lexer *,
    unsigned int);

static void	lexer_copy_token_list(struct lexer *,
    const struct token_list *, struct token_list *);

#define lexer_trace(lx, fmt, ...) trace('l', (lx)->lx_op, (fmt), __VA_ARGS__)

struct lexer *
lexer_alloc(const struct lexer_arg *arg)
{
	VECTOR(struct token *) discarded;
	struct lexer *lx;

	lx = arena_calloc(arg->eternal_scope, 1, sizeof(*lx));
	arena_cleanup(arg->eternal_scope, lexer_free, lx);
	lx->lx_callbacks = arg->callbacks;
	lx->lx_path = arg->path;
	lx->lx_er = error_alloc(arg->eternal_scope, arg->error_flush);
	lx->lx_op = arg->op;
	lx->lx_arena.eternal_scope = arg->eternal_scope;
	lx->lx_input.bf = arg->bf;
	lx->lx_input.ptr = buffer_get_ptr(arg->bf);
	lx->lx_input.len = buffer_get_len(arg->bf);
	lx->lx_diff = arg->diff;
	lx->lx_st.st_lno = 1;
	if (VECTOR_INIT(lx->lx_lines))
		err(1, NULL);
	TAILQ_INIT(&lx->lx_tokens);
	lexer_line_alloc(lx, 1);

	if (VECTOR_INIT(discarded))
		err(1, NULL);

	for (;;) {
		struct token *tk;

		tk = lx->lx_callbacks.read(lx, lx->lx_callbacks.arg);
		if (tk == NULL)
			goto err;
		TAILQ_INSERT_TAIL(&lx->lx_tokens, tk, tk_entry);
		if (tk->tk_flags & TOKEN_FLAG_DISCARD) {
			struct token **dst;

			dst = VECTOR_ALLOC(discarded);
			if (dst == NULL)
				err(1, NULL);
			*dst = tk;
		}
		if (tk->tk_type == LEXER_EOF)
			break;
	}

	while (!VECTOR_EMPTY(discarded)) {
		struct token **tail;

		tail = VECTOR_POP(discarded);
		lexer_remove(lx, *tail);
	}
	VECTOR_FREE(discarded);

	if (lx->lx_callbacks.after_read != NULL)
		lx->lx_callbacks.after_read(lx, lx->lx_callbacks.arg);

	if (options_trace_level(lx->lx_op, 't') > 0)
		lexer_dump(lx);

	return lx;

err:
	if (lx->lx_callbacks.after_read != NULL)
		lx->lx_callbacks.after_read(lx, lx->lx_callbacks.arg);
	VECTOR_FREE(discarded);
	return NULL;
}

static void
lexer_free(void *arg)
{
	struct lexer *lx = arg;
	struct token *tk;

	if (lx->lx_callbacks.before_free != NULL)
		lx->lx_callbacks.before_free(lx, lx->lx_callbacks.arg);

	VECTOR_FREE(lx->lx_lines);

	while ((tk = TAILQ_FIRST(&lx->lx_tokens)) != NULL) {
		TAILQ_REMOVE(&lx->lx_tokens, tk, tk_entry);
		assert(tk->tk_refs == 1);
		token_rele(tk);
	}
}

struct lexer_state
lexer_get_state(const struct lexer *lx)
{
	return lx->lx_st;
}

void
lexer_set_state(struct lexer *lx, const struct lexer_state *st)
{
	lx->lx_st = *st;
}

const char *
lexer_get_path(const struct lexer *lx)
{
	return lx->lx_path;
}

int
lexer_getc(struct lexer *lx, unsigned char *ch)
{
	size_t off;
	unsigned char c;

	if (unlikely(lexer_eof(lx))) {
		/*
		 * Do not immediately report EOF. Instead, return something
		 * that's not expected while reading a token.
		 */
		if (lx->lx_st.st_eof++ > 0)
			return 1;
		*ch = '\0';
		return 0;
	}

	off = lx->lx_st.st_off++;
	c = (unsigned char)lx->lx_input.ptr[off];
	if (unlikely(c == '\n')) {
		lx->lx_st.st_lno++;
		lexer_line_alloc(lx, lx->lx_st.st_lno);
	}
	*ch = c;

	return 0;
}

void
lexer_ungetc(struct lexer *lx)
{
	struct lexer_state *st = &lx->lx_st;
	unsigned char c;

	if (st->st_eof)
		return;

	assert(st->st_off > 0);
	st->st_off--;

	c = (unsigned char)lx->lx_input.ptr[st->st_off];
	if (c == '\n')
		st->st_lno--;
}

size_t
lexer_match(struct lexer *lx, const char *ranges)
{
	size_t len;

	len = KS_str_match(&lx->lx_input.ptr[lx->lx_st.st_off],
	    lx->lx_input.len - lx->lx_st.st_off, ranges);
	lx->lx_st.st_off += len;
	return len;
}

struct token *
lexer_emit(struct lexer *lx, const struct lexer_state *st,
    const struct token *tk)
{
	struct token *t;

	t = lx->lx_callbacks.alloc(lx->lx_arena.eternal_scope, tk);
	t->tk_off = st->st_off;
	t->tk_lno = st->st_lno;
	t->tk_cno = lexer_column(lx, st);
	if (lexer_get_diffchunk(lx, t->tk_lno) != NULL)
		t->tk_flags |= TOKEN_FLAG_DIFF;
	if (t->tk_str == NULL) {
		const char *buf = lx->lx_input.ptr;

		t->tk_str = &buf[st->st_off];
		t->tk_len = lx->lx_st.st_off - st->st_off;
	}
	return t;
}

static int
has_line(const char *str, size_t len)
{
	return memchr(str, '\n', len) != NULL;
}

void
lexer_error(struct lexer *lx, const struct token *ctx, const char *fun, int lno,
    const char *fmt, ...)
{
	va_list ap;
	struct buffer *bf;
	const char *line;
	size_t linelen;
	unsigned int l = ctx != NULL ? ctx->tk_lno : 0;

	lx->lx_st.st_err++;

	bf = error_begin(lx->lx_er);

	buffer_printf(bf, "%s", lx->lx_path);
	if (l > 0)
		buffer_printf(bf, ":%u", l);
	buffer_printf(bf, ": ");
	va_start(ap, fmt);
	buffer_vprintf(bf, fmt, ap);
	va_end(ap);
	if (options_trace_level(lx->lx_op, 'f') > 0)
		buffer_printf(bf, " [%s:%d]", fun, lno);
	buffer_printf(bf, "\n");

	/*
	 * Include best effort line context. However, do not bother with
	 * verbatim hard lines(s) as the column calculation becomes trickier.
	 */
	if (l > 0 && lexer_get_lines(lx, l, l + 1, &line, &linelen) &&
	    !has_line(ctx->tk_str, ctx->tk_len)) {
		unsigned int cno = ctx->tk_cno;
		unsigned int w;

		buffer_printf(bf, "%.*s", (int)linelen, line);

		buffer_printf(bf, "%*s", (int)cno - 1, "");
		w = colwidth(ctx->tk_str, ctx->tk_len, cno, NULL) - cno;
		for (; w > 0; w--)
			buffer_putc(bf, '^');
		buffer_putc(bf, '\n');
	}

	error_end(lx->lx_er);
}

void
lexer_error_flush(struct lexer *lx)
{
	error_flush(lx->lx_er, 1);
}

void
lexer_error_reset(struct lexer *lx)
{
	error_reset(lx->lx_er);
	lx->lx_st.st_err = 0;
}

/*
 * Serialize the given token. The returned string will be freed once
 * lexer_free() is invoked.
 */
const char *
lexer_serialize(struct lexer *lx, const struct token *tk)
{
	if (tk == NULL)
		return "(null)";
	return lx->lx_callbacks.serialize_token(tk, lx->lx_arena.eternal_scope);
}

unsigned int
lexer_get_error(const struct lexer *lx)
{
	return lx->lx_st.st_err;
}

/*
 * Get the buffer contents for the lines [beg, end). If end is equal to 0, the
 * line number of the last line is used.
 */
int
lexer_get_lines(const struct lexer *lx, unsigned int beg, unsigned int end,
    const char **str, size_t *len)
{
	const char *buf = lx->lx_input.ptr;
	size_t nlines = VECTOR_LENGTH(lx->lx_lines);
	size_t bo, eo;

	if (beg > nlines || end > nlines)
		return 0;

	bo = lx->lx_lines[beg - 1];
	if (end == 0)
		eo = lx->lx_input.len;
	else
		eo = lx->lx_lines[end - 1];
	*str = &buf[bo];
	*len = eo - bo;
	return 1;
}

int
lexer_seek(struct lexer *lx, struct token *tk)
{
	if (lx->lx_peek == 0)
		lexer_trace(lx, "seek to %s", lexer_serialize(lx, tk));
	lx->lx_st.st_tk = token_prev(tk);
	return lx->lx_st.st_tk == NULL ? 0 : 1;
}

int
lexer_seek_after(struct lexer *lx, struct token *tk)
{
	struct token *nx;

	nx = token_next(tk);
	return nx != NULL && lexer_seek(lx, nx);
}

int
lexer_pop(struct lexer *lx, struct token **tk)
{
	struct lexer_state *st = &lx->lx_st;

	if (st->st_tk == NULL) {
		*tk = st->st_tk = TAILQ_FIRST(&lx->lx_tokens);
		return 1;
	}

	if (st->st_tk->tk_type == LEXER_EOF) {
		*tk = st->st_tk;
		return 1;
	}

	/* Do not move passed a branch. */
	if (lx->lx_peek == 0 && (st->st_tk->tk_flags & TOKEN_FLAG_BRANCH))
		return 0;

	st->st_tk = token_next(st->st_tk);
	if (unlikely(st->st_tk->tk_flags & TOKEN_FLAG_BRANCH)) {
		if (lx->lx_peek == 0) {
			/* While not peeking, instruct the parser to halt. */
			lexer_trace(lx, "halt %s",
			    lexer_serialize(lx, st->st_tk));
			return 0;
		} else {
			/* While peeking, act as taking the current branch. */
			st->st_tk = lx->lx_callbacks.end_of_branch(lx,
			    st->st_tk, lx->lx_callbacks.arg);
		}
	}

	*tk = st->st_tk;
	return 1;
}

/*
 * Get the last consumed token. Returns non-zero if such token is found.
 */
int
lexer_back(const struct lexer *lx, struct token **tk)
{
	if (lx->lx_st.st_tk == NULL)
		return 0;
	*tk = lx->lx_st.st_tk;
	return 1;
}

int
lexer_back_if(const struct lexer *lx, int type, struct token **tk)
{
	struct token *t;

	if (!lexer_back(lx, &t) || t->tk_type != type)
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

struct token *
lexer_copy_after(struct lexer *lx, struct token *after, const struct token *src)
{
	struct token *tk;

	tk = lx->lx_callbacks.alloc(lx->lx_arena.eternal_scope, src);
	lexer_copy_token_list(lx, &src->tk_prefixes, &tk->tk_prefixes);
	lexer_copy_token_list(lx, &src->tk_suffixes, &tk->tk_suffixes);
	token_position_after(after, tk);
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, tk, tk_entry);
	return tk;
}

struct token *
lexer_insert_after(struct lexer *lx, struct token *after,
    const struct token *def)
{
	struct token *tk;

	tk = lx->lx_callbacks.alloc(lx->lx_arena.eternal_scope, def);
	tk->tk_flags |= token_flags_inherit(after);
	token_position_after(after, tk);
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, tk, tk_entry);
	return tk;
}

struct token *
lexer_move_after(struct lexer *lx, struct token *after, struct token *tk)
{
	TAILQ_REMOVE(&lx->lx_tokens, tk, tk_entry);
	token_position_after(after, tk);
	TAILQ_INSERT_AFTER(&lx->lx_tokens, after, tk, tk_entry);
	return tk;
}

struct token *
lexer_move_before(struct lexer *lx, struct token *before, struct token *mv)
{
	unsigned int mv_suffix_flags = 0;

	if (token_is_first(mv))
		mv_suffix_flags |= TOKEN_FLAG_OPTSPACE;

	TAILQ_REMOVE(&lx->lx_tokens, mv, tk_entry);
	TAILQ_INSERT_BEFORE(before, mv, tk_entry);
	mv->tk_lno = before->tk_lno;

	lx->lx_callbacks.move_prefixes(before, mv);
	token_list_swap(&before->tk_suffixes, TOKEN_FLAG_OPTLINE,
	    &mv->tk_suffixes, mv_suffix_flags);

	return mv;
}

void
lexer_remove(struct lexer *lx, struct token *tk)
{
	struct token *nx, *pv;

	/*
	 * Next token must always be present, we're in trouble while
	 * trying to remove the EOF token which is the only one lacking
	 * a next token.
	 */
	assert(tk->tk_type != LEXER_EOF);
	nx = token_next(tk);
	assert(nx != NULL);
	lx->lx_callbacks.move_prefixes(tk, nx);

	pv = token_prev(tk);
	if (pv == NULL)
		pv = nx;
	token_move_suffixes(tk, pv);

	if (lx->lx_st.st_tk == tk)
		lx->lx_st.st_tk = token_prev(tk);
	token_list_remove(&lx->lx_tokens, tk);
}

int
lexer_expect0(struct lexer *lx, int type, struct token **tk,
    const char *fun, int lno)
{
	struct token *t = NULL;

	if (!lexer_if(lx, type, &t)) {
		/* Peek at the next token to provide meaningful errors. */
		(void)lexer_peek(lx, &t);
		lexer_expect_error(lx, type, t, fun, lno);
		return 0;
	}
	if (tk != NULL)
		*tk = t;
	return 1;
}

void
lexer_peek_enter(struct lexer *lx, struct lexer_state *st)
{
	*st = lx->lx_st;
	lx->lx_peek++;
}

void
lexer_peek_leave(struct lexer *lx, const struct lexer_state *st)
{
	lx->lx_st = *st;
	assert(lx->lx_peek > 0);
	lx->lx_peek--;
}

/*
 * Peek at the next token without consuming it. Returns non-zero if such token
 * was found.
 */
int
lexer_peek(struct lexer *lx, struct token **tk)
{
	struct lexer_state s;
	struct token *t = NULL;
	int pop;

	lexer_peek_enter(lx, &s);
	pop = lexer_pop(lx, &t);
	lexer_peek_leave(lx, &s);
	if (!pop)
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Peek at the next token without consuming it only if it matches the given
 * type. Returns non-zero if such token was found.
 */
int
lexer_peek_if(struct lexer *lx, int type, struct token **tk)
{
	struct token *t;

	if (lexer_peek(lx, &t) && t->tk_type == type) {
		if (tk != NULL)
			*tk = t;
		return 1;
	}
	return 0;
}

/*
 * Consume the next token if it matches the given type. Returns non-zero if such
 * token was found.
 */
int
lexer_if(struct lexer *lx, int type, struct token **tk)
{
	struct lexer_state s;
	struct token *t;

	/* Must use lexer_pop() without peeking to not cross branches. */
	s = lexer_get_state(lx);
	if (!lexer_pop(lx, &t))
		return 0;
	if (t->tk_type != type) {
		lexer_set_state(lx, &s);
		return 0;
	}
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Peek at the flags of the next token without consuming it. Returns non-zero if
 * such token was found.
 */
int
lexer_peek_if_flags(struct lexer *lx, unsigned int flags, struct token **tk)
{
	struct token *t;

	if (!lexer_peek(lx, &t) || (t->tk_flags & flags) == 0)
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Consume the next token if it matches the given flags. Returns non-zero such
 * token was found.
 */
int
lexer_if_flags(struct lexer *lx, unsigned int flags, struct token **tk)
{
	struct token *t;

	if (!lexer_peek_if_flags(lx, flags, &t) || !lexer_pop(lx, &t))
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
}

/*
 * Peek at the next balanced pair of tokens such as parenthesis or squares.
 * Returns non-zero if such tokens was found.
 */
int
lexer_peek_if_pair(struct lexer *lx, int lhs_type, int rhs_type,
    struct token **lhs, struct token **rhs)
{
	struct lexer_state s;
	struct token *t = NULL;
	int pair = 0;

	if (!lexer_peek_if(lx, lhs_type, lhs))
		return 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (!lexer_pop(lx, &t))
			break;
		if (t->tk_type == LEXER_EOF)
			break;
		if (t->tk_type == lhs_type)
			pair++;
		if (t->tk_type == rhs_type)
			pair--;
		if (pair == 0)
			break;
	}
	lexer_peek_leave(lx, &s);
	if (pair > 0)
		return 0;
	if (rhs != NULL)
		*rhs = t;
	return 1;
}

/*
 * Consume the next balanced pair of tokens such as parenthesis or squares.
 * Returns non-zero if such tokens was found.
 */
int
lexer_if_pair(struct lexer *lx, int lhs_type, int rhs_type, struct token **lhs,
    struct token **rhs)
{
	struct token *end;

	if (!lexer_peek_if_pair(lx, lhs_type, rhs_type, lhs, &end))
		return 0;

	lx->lx_st.st_tk = end;
	if (rhs != NULL)
		*rhs = end;
	return 1;
}

/*
 * Peek at the prefixes of the next token without consuming it. Returns non-zero
 * if any prefix has the given flags.
 */
int
lexer_peek_if_prefix_flags(struct lexer *lx, unsigned int flags,
    struct token **tk)
{
	struct token *px, *t;

	/* Cannot use lexer_peek() as it would move past cpp branches. */
	if (!lexer_back(lx, &t))
		return 0;
	t = token_next(t);
	if (t == NULL)
		return 0;
	TAILQ_FOREACH(px, &t->tk_prefixes, tk_entry) {
		if (px->tk_flags & flags) {
			if (tk != NULL)
				*tk = px;
			return 1;
		}
	}
	return 0;
}

/*
 * Peek until the given token type is encountered. Returns non-zero if such
 * token was found.
 */
int
lexer_peek_until(struct lexer *lx, int type, struct token **tk)
{
	struct lexer_state s;
	int peek;

	lexer_peek_enter(lx, &s);
	peek = lexer_until(lx, type, tk);
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Peek until the given token type is encountered and it is not nested under any
 * pairs of parenthesis nor braces but halt while trying to move beyond the
 * given stop token. Returns non-zero if such token was found.
 *
 * Assuming tk is not NULL and the stop is reached, tk will point to the stop
 * token.
 */
static int
lexer_peek_until_not_nested(struct lexer *lx, int type,
    const struct token *stop, struct token **tk)
{
	struct lexer_state s;
	struct token *t = NULL;
	int nest = 0;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		if (!lexer_pop(lx, &t) || t == stop || t->tk_type == LEXER_EOF)
			break;
		if (t->tk_type == TOKEN_LPAREN || t->tk_type == TOKEN_LBRACE)
			nest++;
		else if (t->tk_type == TOKEN_RPAREN ||
		    t->tk_type == TOKEN_RBRACE)
			nest--;
		if (t->tk_type == type && !nest) {
			peek = 1;
			break;
		}
	}
	lexer_peek_leave(lx, &s);
	if (tk != NULL)
		*tk = peek ? t : (struct token *)stop;
	return peek;
}

int
lexer_peek_until_comma(struct lexer *lx, const struct token *stop,
    struct token **tk)
{
	return lexer_peek_until_not_nested(lx, TOKEN_COMMA, stop, tk);
}

/*
 * Consume token(s) until the given token type is encountered. Returns non-zero
 * if such token is found.
 */
int
lexer_until(struct lexer *lx, int type, struct token **tk)
{
	for (;;) {
		struct token *t = NULL;

		if (!lexer_pop(lx, &t) || t->tk_type == LEXER_EOF)
			return 0;
		if (t->tk_type == type) {
			if (tk != NULL)
				*tk = t;
			return 1;
		}
	}
}

int
lexer_peek_first(struct lexer *lx, struct token **tk)
{
	*tk = TAILQ_FIRST(&lx->lx_tokens);
	return *tk != NULL;
}

int
lexer_peek_last(struct lexer *lx, struct token **tk)
{
	*tk = TAILQ_LAST(&lx->lx_tokens, token_list);
	return *tk != NULL;
}

static const struct diffchunk *
lexer_get_diffchunk(const struct lexer *lx, unsigned int lno)
{
	if (lx->lx_diff == NULL)
		return NULL;
	return diff_get_chunk(lx->lx_diff, lno);
}

/*
 * Looks unused but only used while debugging and therefore not declared static.
 */
void
lexer_dump(struct lexer *lx)
{
	struct token *tk;
	unsigned int i = 0;

	fprintf(stderr, "[L] %s:\n", lx->lx_path);

	TAILQ_FOREACH(tk, &lx->lx_tokens, tk_entry) {
		struct token *prefix, *suffix;
		const char *str;

		i++;

		TAILQ_FOREACH(prefix, &tk->tk_prefixes, tk_entry) {
			str = lx->lx_callbacks.serialize_prefix(prefix,
			    lx->lx_arena.eternal_scope);
			fprintf(stderr, "[L] %-6u   prefix %s\n", i, str);
		}

		str = lexer_serialize(lx, tk);
		fprintf(stderr, "[L] %-6u %s\n", i, str);

		TAILQ_FOREACH(suffix, &tk->tk_suffixes, tk_entry) {
			str = lexer_serialize(lx, suffix);
			fprintf(stderr, "[L] %-6u   suffix %s\n", i, str);
		}
	}
}

/*
 * Consume empty line(s) until a line beginning with optional whitespace
 * followed by none whitespace is found. The lexer will be positioned before the
 * first none whitespace character and the given state at the beginning of the
 * same line line, thus including the whitespace if present.
 */
void
lexer_eat_lines_and_spaces(struct lexer *lx, struct lexer_state *st)
{
	int gotspaces = 0;

	for (;;) {
		if (lexer_eat_lines(lx, 0, NULL)) {
			if (st != NULL)
				*st = lx->lx_st;
		} else if (gotspaces) {
			break;
		}

		if (lexer_eat_spaces(lx, NULL))
			gotspaces = 1;
		else
			break;
	}
}

int
lexer_eat_lines(struct lexer *lx, int threshold, struct token **tk)
{
	struct lexer_state oldst, st;
	int nlines = 0;
	unsigned char ch;

	oldst = st = lx->lx_st;

	for (;;) {
		if (lexer_getc(lx, &ch))
			break;
		if (ch == '\r') {
			continue;
		} else if (ch == '\n') {
			oldst = lx->lx_st;
			if (++nlines == threshold)
				break;
		} else if (ch != ' ' && ch != '\t') {
			lexer_ungetc(lx);
			break;
		}
	}
	lx->lx_st = oldst;
	if (nlines == 0 || nlines < threshold)
		return 0;
	if (tk != NULL) {
		*tk = lexer_emit(lx, &st, &(struct token){
		    .tk_type	= TOKEN_SPACE,
		    .tk_str	= "\n",
		    .tk_len	= 1,
		});
	}
	return nlines;
}

int
lexer_eat_spaces(struct lexer *lx, struct token **tk)
{
	struct lexer_state st = lx->lx_st;
	size_t nspaces;

	nspaces = KS_str_match(&lx->lx_input.ptr[lx->lx_st.st_off],
	    lx->lx_input.len - lx->lx_st.st_off, "  \f\f\t\t");
	if (nspaces == 0)
		return 0;
	lx->lx_st.st_off += nspaces;
	if (tk != NULL) {
		*tk = lexer_emit(lx, &st,
		    &(struct token){.tk_type = TOKEN_SPACE});
	}
	return 1;
}

int
lexer_eof(const struct lexer *lx)
{
	return lx->lx_st.st_off == lx->lx_input.len;
}

static void
lexer_line_alloc(struct lexer *lx, unsigned int lno)
{
	size_t *dst;

	/* We could end up here again after lexer_ungetc(). */
	if (lno - 1 < VECTOR_LENGTH(lx->lx_lines))
		return;

	dst = VECTOR_ALLOC(lx->lx_lines);
	if (dst == NULL)
		err(1, NULL);
	*dst = lx->lx_st.st_off;
}

static unsigned int
lexer_column(const struct lexer *lx, const struct lexer_state *st)
{
	size_t line_offset;

	line_offset = lx->lx_lines[st->st_lno - 1];
	return colwidth(&lx->lx_input.ptr[line_offset],
	    st->st_off - line_offset, 1, NULL);
}

int
lexer_buffer_streq(const struct lexer *lx, const struct lexer_state *st,
    const char *str)
{
	size_t buflen, len;

	buflen = lx->lx_st.st_off - st->st_off;
	if (buflen == 0)
		return 0;
	len = strlen(str);
	if (len > buflen)
		return 0;
	return strncmp(&lx->lx_input.ptr[st->st_off], str, len) == 0;
}

const char *
lexer_buffer_slice(const struct lexer *lx, const struct lexer_state *st,
    size_t *len)
{
	*len = lx->lx_st.st_off - st->st_off;
	if (*len == 0)
		return NULL;
	return &lx->lx_input.ptr[st->st_off];
}

static void
lexer_expect_error(struct lexer *lx, int type, const struct token *tk,
    const char *fun, int lno)
{
	struct token *t;

	/* Be quiet while about to branch. */
	if (lexer_back(lx, &t) && (t->tk_flags & TOKEN_FLAG_BRANCH)) {
		lexer_trace(lx, "%s:%d: suppressed, expected %s", fun, lno,
		    lexer_serialize(lx, &(struct token){.tk_type = type}));
		return;
	}

	/* Be quiet if an error already has been emitted. */
	if (lx->lx_st.st_err++ > 0)
		return;

	/* Be quiet while peeking. */
	if (lx->lx_peek > 0)
		return;

	lexer_error(lx, tk, fun, lno,
	    "expected type %s got %s",
	    lexer_serialize(lx, &(struct token){.tk_type = type}),
	    lexer_serialize(lx, tk));
}

static void
lexer_copy_token_list(struct lexer *lx, const struct token_list *src,
    struct token_list *dst)
{
	const struct token *tk;

	TAILQ_FOREACH(tk, src, tk_entry) {
		struct token *cp;

		cp = lx->lx_callbacks.alloc(lx->lx_arena.eternal_scope, tk);
		TAILQ_INSERT_TAIL(dst, cp, tk_entry);
	}
}
