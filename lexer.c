#include "lexer.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "libks/arena-vector.h"
#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/compiler.h"
#include "libks/list.h"
#include "libks/string.h"
#include "libks/vector.h"

#include "diff.h"
#include "error.h"
#include "options.h"
#include "token.h"
#include "trace-types.h"
#include "trace.h"
#include "util.h"

#define lexer_trace(lx, fmt, ...) \
	trace(TRACE_LEXER, (lx)->lx_op, (fmt), __VA_ARGS__)

struct lexer {
	struct lexer_state	 lx_st;
	struct lexer_callbacks	 lx_callbacks;
	const char		*lx_path;
	struct error		*lx_er;
	const struct options	*lx_op;
	const struct diffchunk	*lx_diff;

	struct {
		struct arena_scope	*eternal_scope;
		struct arena		*scratch;
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

static void	lexer_expect_error(struct lexer *, int, const struct token *,
    const char *, int);

static int	lexer_peek_until_not_nested(struct lexer *, int,
    struct token *, struct token **);

static const struct diffchunk	*lexer_get_diffchunk(const struct lexer *,
    unsigned int);

static void	lexer_copy_token_list(struct lexer *,
    const struct token_list *, struct token_list *);

static const char       *lexer_serialize_impl(struct lexer *,
    const struct token *, struct arena_scope *);

struct lexer *
lexer_tokenize(const struct lexer_arg *arg)
{
	VECTOR(struct token *) discarded;
	struct lexer *lx;

	lx = arena_calloc(arg->arena.eternal_scope, 1, sizeof(*lx));
	arena_cleanup(arg->arena.eternal_scope, lexer_free, lx);
	lx->lx_callbacks = arg->callbacks;
	lx->lx_path = arg->path;
	lx->lx_er = error_alloc(arg->error_flush, arg->arena.eternal_scope);
	lx->lx_op = arg->op;
	lx->lx_arena.eternal_scope = arg->arena.eternal_scope;
	lx->lx_arena.scratch = arg->arena.scratch;
	lx->lx_input.bf = arg->bf;
	lx->lx_input.ptr = buffer_get_ptr(arg->bf);
	lx->lx_input.len = buffer_get_len(arg->bf);
	lx->lx_diff = arg->diff;
	lx->lx_st.st_lno = 1;
	if (VECTOR_INIT(lx->lx_lines))
		err(1, NULL);
	LIST_INIT(&lx->lx_tokens);
	lexer_line_alloc(lx, 1);

	arena_scope(lx->lx_arena.scratch, scratch_scope);
	ARENA_VECTOR_INIT(&scratch_scope, discarded, 1 << 3);

	for (;;) {
		struct token *tk;

		tk = lx->lx_callbacks.tokenize(lx, lx->lx_callbacks.arg);
		if (tk == NULL)
			goto err;
		LIST_INSERT_TAIL(&lx->lx_tokens, tk);
		if (tk->tk_flags & TOKEN_FLAG_DISCARD)
			*ARENA_VECTOR_ALLOC(discarded) = tk;
		if (tk->tk_type == LEXER_EOF)
			break;
	}

	while (!VECTOR_EMPTY(discarded)) {
		struct token **tail;

		tail = VECTOR_POP(discarded);
		lexer_remove(lx, *tail);
	}

	if (lx->lx_callbacks.after_tokenize != NULL)
		lx->lx_callbacks.after_tokenize(lx, lx->lx_callbacks.arg);

	if (options_trace_level(lx->lx_op, TRACE_TOKEN) > 0)
		lexer_dump(lx);

	return lx;

err:
	if (lx->lx_callbacks.after_tokenize != NULL)
		lx->lx_callbacks.after_tokenize(lx, lx->lx_callbacks.arg);
	return NULL;
}

static void
lexer_free(void *arg)
{
	struct lexer *lx = arg;
	struct token *tk, *tmp;

	if (lx->lx_callbacks.before_free != NULL)
		lx->lx_callbacks.before_free(lx, lx->lx_callbacks.arg);

	VECTOR_FREE(lx->lx_lines);

	LIST_FOREACH_SAFE(tk, &lx->lx_tokens, tmp) {
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
lexer_get_peek(const struct lexer *lx)
{
	return lx->lx_peek;
}

/*
 * Returns the arena scope with the same lifetime as the given lexer.
 */
struct arena_scope *
lexer_get_arena_scope(const struct lexer *lx)
{
	return lx->lx_arena.eternal_scope;
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
		if (lx->lx_st.st_flags.eof)
			return 1;
		lx->lx_st.st_flags.eof = 1;
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

	if (st->st_flags.eof)
		return;

	assert(st->st_off > 0);
	st->st_off--;

	c = (unsigned char)lx->lx_input.ptr[st->st_off];
	if (c == '\n')
		st->st_lno--;
}

struct token *
lexer_emit(struct lexer *lx, const struct lexer_state *st, int token_type)
{
	const struct token template = {.tk_type = token_type};

	return lexer_emit_template(lx, st, &template);
}

struct token *
lexer_emit_template(struct lexer *lx, const struct lexer_state *st,
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

/*
 * Emit a token that's not part of the tokenized source code.
 */
struct token *
lexer_emit_synthetic(struct lexer *lx, const struct token *tk)
{
	return lx->lx_callbacks.alloc(lx->lx_arena.eternal_scope, tk);
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
	unsigned int l = ctx->tk_lno;

	lx->lx_st.st_flags.error = 1;

	/* Be quiet while peeking. */
	if (lx->lx_peek > 0)
		return;

	bf = error_begin(lx->lx_er);

	buffer_printf(bf, "%s:%u", lx->lx_path, l);
	buffer_printf(bf, ": ");
	va_start(ap, fmt);
	buffer_vprintf(bf, fmt, ap);
	va_end(ap);
	if (options_trace_level(lx->lx_op, TRACE_FUNC) > 0)
		buffer_printf(bf, " [%s:%d]", fun, lno);
	buffer_printf(bf, "\n");

	/*
	 * Include best effort line context. However, do not bother with
	 * verbatim hard lines(s) as the column calculation becomes trickier.
	 */
	if (lexer_get_lines(lx, l, l + 1, &line, &linelen) &&
	    !has_line(ctx->tk_str, ctx->tk_len)) {
		unsigned int cno = ctx->tk_cno;
		unsigned int w;

		buffer_printf(bf, "%.*s", (int)linelen, line);

		buffer_printf(bf, "%*s", (int)cno - 1, "");
		w = colwidth(ctx->tk_str, ctx->tk_len, cno) - cno;
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
	lx->lx_st.st_flags.error = 0;
}

/*
 * Serialize the given token. The returned string will be freed once
 * lexer_free() is invoked.
 */
const char *
lexer_serialize(struct lexer *lx, const struct token *tk)
{
	return lexer_serialize_impl(lx, tk, lx->lx_arena.eternal_scope);
}

unsigned int
lexer_get_error(const struct lexer *lx)
{
	return lx->lx_st.st_flags.error ? 1 : 0;
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

void
lexer_seek(struct lexer *lx, struct token *tk)
{
	if (lx->lx_peek == 0)
		lexer_trace(lx, "seek to %s", lexer_serialize(lx, tk));
	lx->lx_st.st_tk = token_prev(tk);
}

int
lexer_seek_after(struct lexer *lx, struct token *tk)
{
	struct token *nx;

	nx = token_next(tk);
	if (nx == NULL)
		return 0;
	lexer_seek(lx, nx);
	return 1;
}

int
lexer_pop(struct lexer *lx, struct token **tk)
{
	struct lexer_state *st = &lx->lx_st;

	if (st->st_tk == NULL) {
		*tk = st->st_tk = LIST_FIRST(&lx->lx_tokens);
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
	LIST_INSERT_AFTER(&lx->lx_tokens, after, tk);
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
	LIST_INSERT_AFTER(&lx->lx_tokens, after, tk);
	return tk;
}

struct token *
lexer_move_after(struct lexer *lx, struct token *after, struct token *mv)
{
	LIST_REMOVE(&lx->lx_tokens, mv);
	token_position_after(after, mv);
	LIST_INSERT_AFTER(&lx->lx_tokens, after, mv);
	return mv;
}

struct token *
lexer_move_before(struct lexer *lx, struct token *before, struct token *mv)
{
	LIST_REMOVE(&lx->lx_tokens, mv);
	LIST_INSERT_BEFORE(before, mv);
	mv->tk_lno = before->tk_lno;
	lx->lx_callbacks.move_prefixes(before, mv);
	token_list_swap(&before->tk_suffixes, &mv->tk_suffixes);
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

void
lexer_move_prefixes(struct lexer *lx, struct token *src, struct token *dst)
{
	lx->lx_callbacks.move_prefixes(src, dst);
}

int
lexer_expect_impl(struct lexer *lx, int type, struct token **tk,
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

	if (!lexer_peek(lx, &t) || t->tk_type != type)
		return 0;
	if (tk != NULL)
		*tk = t;
	return 1;
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
	LIST_FOREACH(px, &t->tk_prefixes) {
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
    struct token *stop, struct token **tk)
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
		*tk = peek ? t : stop;
	return peek;
}

int
lexer_peek_until_comma(struct lexer *lx, struct token *stop, struct token **tk)
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
	*tk = LIST_FIRST(&lx->lx_tokens);
	return *tk != NULL;
}

int
lexer_peek_last(struct lexer *lx, struct token **tk)
{
	*tk = LIST_LAST(&lx->lx_tokens);
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

	arena_scope(lx->lx_arena.scratch, s);

	fprintf(stderr, "[L] %s:\n", lx->lx_path);

	LIST_FOREACH(tk, &lx->lx_tokens) {
		struct token *prefix, *suffix;
		const char *str;

		i++;

		LIST_FOREACH(prefix, &tk->tk_prefixes) {
			str = lx->lx_callbacks.serialize_prefix(prefix, &s);
			fprintf(stderr, "[L] %-6u   prefix %s\n", i, str);
		}

		str = lexer_serialize_impl(lx, tk, &s);
		fprintf(stderr, "[L] %-6u %s\n", i, str);

		LIST_FOREACH(suffix, &tk->tk_suffixes) {
			str = lexer_serialize_impl(lx, suffix, &s);
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
		*tk = lexer_emit_template(lx, &st, &(struct token){
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
	static struct KS_str_match match;
	struct lexer_state st = lx->lx_st;
	size_t nspaces;

	KS_str_match_init_once("  \f\f\t\t", &match);

	nspaces = KS_str_match(&lx->lx_input.ptr[lx->lx_st.st_off],
	    lx->lx_input.len - lx->lx_st.st_off, &match);
	if (nspaces == 0)
		return 0;
	lx->lx_st.st_off += nspaces;
	if (tk != NULL)
		*tk = lexer_emit(lx, &st, TOKEN_SPACE);
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

unsigned int
lexer_column(const struct lexer *lx, const struct lexer_state *st)
{
	size_t line_offset;

	line_offset = lx->lx_lines[st->st_lno - 1];
	return colwidth(&lx->lx_input.ptr[line_offset],
	    st->st_off - line_offset, 1);
}

void
lexer_buffer_peek(const struct lexer *lx, struct lexer_buffer *buf)
{
	buf->ptr = &lx->lx_input.ptr[lx->lx_st.st_off];
	buf->len = lx->lx_input.len - lx->lx_st.st_off;
}

int
lexer_buffer_slice(const struct lexer *lx, const struct lexer_state *st,
    struct lexer_buffer *buf)
{
	buf->len = lx->lx_st.st_off - st->st_off;
	if (buf->len == 0)
		return 0;
	buf->ptr = &lx->lx_input.ptr[st->st_off];
	return 1;
}

void
lexer_buffer_seek(struct lexer *lx, size_t off)
{
	lx->lx_st.st_off += off;
}

static const char *
lexer_serialize_token_type(struct lexer *lx, int type)
{
	return lx->lx_callbacks.serialize_token_type(type,
	    lx->lx_arena.eternal_scope);
}

static void
lexer_expect_error(struct lexer *lx, int type, const struct token *tk,
    const char *fun, int lno)
{
	struct token *t;

	/* Be quiet while about to branch. */
	if (lexer_back(lx, &t) && (t->tk_flags & TOKEN_FLAG_BRANCH)) {
		lexer_trace(lx, "%s:%d: suppressed, expected %s", fun, lno,
		    lexer_serialize_token_type(lx, type));
		return;
	}

	/* Be quiet if an error already has been emitted. */
	if (lx->lx_st.st_flags.error)
		return;

	lexer_error(lx, tk, fun, lno,
	    "expected type %s got %s",
	    lexer_serialize_token_type(lx, type),
	    lexer_serialize(lx, tk));
}

static void
lexer_copy_token_list(struct lexer *lx, const struct token_list *src,
    struct token_list *dst)
{
	const struct token *tk;

	LIST_FOREACH(tk, src) {
		struct token *cp;

		cp = lx->lx_callbacks.alloc(lx->lx_arena.eternal_scope, tk);
		LIST_INSERT_TAIL(dst, cp);
	}
}

static const char *
lexer_serialize_impl(struct lexer *lx, const struct token *tk,
    struct arena_scope *s)
{
	if (tk == NULL)
		return "(null)";
	return lx->lx_callbacks.serialize_token(tk, s);
}
