#include "simple-stmt.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <string.h>

#include "libks/arena-buffer.h"
#include "libks/arena.h"
#include "libks/buffer.h"
#include "libks/vector.h"

#include "clang.h"
#include "doc.h"
#include "lexer.h"
#include "options.h"
#include "token.h"

#define SIMPLE_STMT_MAX 32

struct stmt {
	struct doc	*root;
	struct doc	*indent;
	struct token	*lbrace;
	struct token	*rbrace;
	unsigned int	 indent_width;
	unsigned int	 flags;
#define STMT_BRACES			0x00000001u
#define STMT_IGNORE			0x00000002u
};

struct simple_stmt {
	VECTOR(struct stmt)	 stmts;
	struct lexer		*lx;
	const struct options	*op;
	const struct style	*st;

	struct {
		struct arena		*scratch;
		struct arena		*buffer;
	} arena;
};

static struct stmt	*simple_stmt_alloc(struct simple_stmt *, struct doc *,
    unsigned int, unsigned int);
static int		 simple_stmt_need_braces(struct simple_stmt *,
    const struct stmt *, struct buffer *);

static int	need_braces(struct simple_stmt *);
static void	add_braces(struct simple_stmt *);
static void	remove_braces(struct simple_stmt *);

static int		 isoneline(const char *, size_t);
static int		 is_brace_moveable(struct simple_stmt *,
    const struct token *);
static const char	*strtrim(const char *, size_t *);

struct simple_stmt *
simple_stmt_enter(struct lexer *lx, const struct style *st,
    struct arena_scope *eternal_scope, struct arena *scratch,
    struct arena *buffer, const struct options *op)
{
	struct simple_stmt *ss;

	ss = arena_calloc(eternal_scope, 1, sizeof(*ss));
	if (VECTOR_INIT(ss->stmts) ||
	    VECTOR_RESERVE(ss->stmts, SIMPLE_STMT_MAX))
		err(1, NULL);
	ss->arena.scratch = scratch;
	ss->arena.buffer = buffer;
	ss->lx = lx;
	ss->op = op;
	ss->st = st;
	return ss;
}

void
simple_stmt_leave(struct simple_stmt *ss)
{
	if (VECTOR_EMPTY(ss->stmts))
		return;

	if (need_braces(ss))
		add_braces(ss);
	else
		remove_braces(ss);
}

void
simple_stmt_free(struct simple_stmt *ss)
{
	if (ss == NULL)
		return;

	while (!VECTOR_EMPTY(ss->stmts)) {
		struct stmt *st;

		st = VECTOR_POP(ss->stmts);
		if (st->lbrace != NULL)
			token_rele(st->lbrace);
		if (st->rbrace != NULL)
			token_rele(st->rbrace);
	}
	VECTOR_FREE(ss->stmts);
}

struct doc *
simple_stmt_braces_enter(struct simple_stmt *ss, struct doc *dc,
    struct token *lbrace, struct token *rbrace, unsigned int indent)
{
	struct stmt *st;
	unsigned int flags = STMT_BRACES;

	/* Make sure both braces are covered by a diff chunk. */
	if (!is_brace_moveable(ss, lbrace) || !is_brace_moveable(ss, rbrace))
		flags |= STMT_IGNORE;
	st = simple_stmt_alloc(ss, dc, indent, flags);
	if (st == NULL)
		return dc;
	token_ref(lbrace);
	st->lbrace = lbrace;
	token_ref(rbrace);
	st->rbrace = rbrace;
	return st->indent;
}

struct doc *
simple_stmt_no_braces_enter(struct simple_stmt *ss, struct doc *dc,
    struct token *lbrace, unsigned int indent, void **cookie)
{
	struct stmt *st;
	unsigned int flags = 0;

	if (!is_brace_moveable(ss, lbrace))
		flags |= STMT_IGNORE;
	st = simple_stmt_alloc(ss, dc, indent, flags);
	if (st == NULL)
		return dc;
	token_ref(lbrace);
	st->lbrace = lbrace;
	*cookie = st;
	return st->indent;
}

void
simple_stmt_no_braces_leave(struct simple_stmt *ss, struct token *rbrace,
    void *cookie)
{
	struct stmt *st = cookie;

	if (!is_brace_moveable(ss, rbrace))
		st->flags |= STMT_IGNORE;
	token_ref(rbrace);
	st->rbrace = rbrace;
}

static struct stmt *
simple_stmt_alloc(struct simple_stmt *ss, struct doc *dc, unsigned int indent,
    unsigned int flags)
{
	struct stmt *st;

	/* Prevent reallocations causing dangling pointers. */
	if (VECTOR_LENGTH(ss->stmts) >= SIMPLE_STMT_MAX)
		return NULL;

	st = VECTOR_CALLOC(ss->stmts);
	if (st == NULL)
		err(1, NULL);
	st->root = doc_alloc(DOC_GROUP, dc);
	st->indent = doc_indent(indent, st->root);
	doc_alloc(DOC_HARDLINE, st->indent);
	st->indent_width = indent;
	st->flags = flags;
	return st;
}

static int
is_stmt_empty(const struct simple_stmt *ss, const struct stmt *st)
{
	const struct token *lbrace = st->lbrace;
	const struct token *rbrace = st->rbrace;
	const struct token *nx = token_next(lbrace);

	if (st->flags & STMT_BRACES) {
		/*
		 * GCC warning option Wempty-body (implied by Wextra) suggests
		 * adding braces around statement consisting only of a
		 * semicolon.
		 */
		if (nx == token_prev(rbrace) && nx->tk_type == TOKEN_SEMI &&
		    VECTOR_LENGTH(ss->stmts) == 1)
			return 1;
		return nx == rbrace;
	}

	return nx == rbrace && lbrace->tk_type != TOKEN_SEMI;
}

static int
simple_stmt_need_braces(struct simple_stmt *ss, const struct stmt *st,
    struct buffer *bf)
{
	const char *buf;
	size_t buflen;

	if (is_stmt_empty(ss, st))
		return 1;

	buffer_reset(bf);
	doc_exec(&(struct doc_exec_arg){
	    .dc		= st->root,
	    .scratch	= ss->arena.scratch,
	    .bf		= bf,
	    .st		= ss->st,
	});
	buflen = buffer_get_len(bf);
	buf = strtrim(buffer_get_ptr(bf), &buflen);
	if (isoneline(buf, buflen)) {
		return (st->flags & STMT_BRACES) &&
		    !token_is_moveable(st->rbrace);
	}
	return 1;
}

static int
need_braces(struct simple_stmt *ss)
{
	struct buffer *bf;
	size_t i;

	arena_scope(ss->arena.buffer, s);

	bf = arena_buffer_alloc(&s, 1 << 10);
	for (i = 0; i < VECTOR_LENGTH(ss->stmts); i++) {
		const struct stmt *st = &ss->stmts[i];

		if (st->flags & STMT_IGNORE) {
			if (st->flags & STMT_BRACES)
				return 1;
		} else if (simple_stmt_need_braces(ss, st, bf)) {
			return 1;
		}
	}

	return 0;
}

static void
add_braces(struct simple_stmt *ss)
{
	struct lexer *lx = ss->lx;
	size_t i;

	for (i = 0; i < VECTOR_LENGTH(ss->stmts); i++) {
		struct stmt *st = &ss->stmts[i];
		struct token *lbrace, *pv, *rbrace;

		if (st->flags & (STMT_IGNORE | STMT_BRACES))
			continue;

		pv = token_prev(st->lbrace);
		lbrace = lexer_insert_after(lx, pv,
		    clang_keyword_token(TOKEN_LBRACE));
		token_move_suffixes(pv, lbrace);

		pv = token_prev(st->rbrace);
		rbrace = lexer_insert_after(lx, pv,
		    clang_keyword_token(TOKEN_RBRACE));
		token_move_suffixes_if(pv, rbrace, TOKEN_SPACE);
	}
}

static void
remove_braces(struct simple_stmt *ss)
{
	struct lexer *lx = ss->lx;
	size_t i;

	for (i = 0; i < VECTOR_LENGTH(ss->stmts); i++) {
		struct stmt *st = &ss->stmts[i];

		if ((st->flags & STMT_IGNORE) ||
		    (st->flags & STMT_BRACES) == 0)
			continue;

		lexer_remove(lx, st->lbrace);
		lexer_remove(lx, st->rbrace);
	}
}

static int
isoneline(const char *str, size_t len)
{
	return memchr(str, '\n', len) == NULL;
}

static int
is_brace_moveable(struct simple_stmt *ss, const struct token *tk)
{
	if (ss->op->diffparse && (tk->tk_flags & TOKEN_FLAG_DIFF) == 0)
		return 0;
	/* Do not require token to be moveable as we can cope with comments. */
	return !token_has_cpp(tk);
}

static const char *
strtrim(const char *buf, size_t *buflen)
{
	size_t len = *buflen;

	for (; len > 0 && isspace((unsigned char)buf[0]); buf++, len--)
		continue;
	for (; len > 0 && isspace((unsigned char)buf[len - 1]); len--)
		continue;
	*buflen = len;
	return buf;
}
