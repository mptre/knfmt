#include "simple-stmt.h"

#include "config.h"

#include <ctype.h>
#include <err.h>
#include <string.h>

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
	struct doc	*st_root;
	struct doc	*st_indent;
	struct token	*st_lbrace;
	struct token	*st_rbrace;
	unsigned int	 st_flags;
#define STMT_BRACES			0x00000001u
#define STMT_IGNORE			0x00000002u
};

struct simple_stmt {
	VECTOR(struct stmt)	 ss_stmts;
	struct lexer		*ss_lx;
	const struct options	*ss_op;
	const struct style	*ss_st;

	struct {
		struct arena		*scratch;
		struct arena_scope	*doc_scope;
	} ss_arena;
};

static struct stmt	*simple_stmt_alloc(struct simple_stmt *, unsigned int,
    unsigned int);

static int	need_braces(struct simple_stmt *, const struct stmt *,
    struct buffer *);
static void	add_braces(struct simple_stmt *);
static void	remove_braces(struct simple_stmt *);

static int		 isoneline(const char *, size_t);
static int		 is_brace_moveable(struct simple_stmt *,
    const struct token *);
static const char	*strtrim(const char *, size_t *);

struct simple_stmt *
simple_stmt_enter(struct lexer *lx, const struct style *st,
    struct arena_scope *eternal_scope, struct arena_scope *doc_scope,
    struct arena *scratch, const struct options *op)
{
	struct simple_stmt *ss;

	ss = arena_calloc(eternal_scope, 1, sizeof(*ss));
	if (VECTOR_INIT(ss->ss_stmts) ||
	    VECTOR_RESERVE(ss->ss_stmts, SIMPLE_STMT_MAX))
		err(1, NULL);
	ss->ss_arena.scratch = scratch;
	ss->ss_arena.doc_scope = doc_scope;
	ss->ss_lx = lx;
	ss->ss_op = op;
	ss->ss_st = st;
	return ss;
}

void
simple_stmt_leave(struct simple_stmt *ss)
{
	struct buffer *bf;
	size_t i;
	int dobraces = 0;

	if (VECTOR_EMPTY(ss->ss_stmts))
		return;

	bf = buffer_alloc(1024);
	if (bf == NULL)
		err(1, NULL);
	for (i = 0; i < VECTOR_LENGTH(ss->ss_stmts); i++) {
		const struct stmt *st = &ss->ss_stmts[i];

		if (st->st_flags & STMT_IGNORE) {
			if (st->st_flags & STMT_BRACES) {
				dobraces = 1;
				break;
			}
			continue;
		}
		if (need_braces(ss, st, bf)) {
			/*
			 * No point in continuing as at least one statement
			 * spans over multiple lines.
			 */
			dobraces = 1;
			break;
		}
	}
	buffer_free(bf);

	if (dobraces)
		add_braces(ss);
	else
		remove_braces(ss);
}

void
simple_stmt_free(struct simple_stmt *ss)
{
	if (ss == NULL)
		return;

	while (!VECTOR_EMPTY(ss->ss_stmts)) {
		struct stmt *st;

		st = VECTOR_POP(ss->ss_stmts);
		if (st->st_lbrace != NULL)
			token_rele(st->st_lbrace);
		if (st->st_rbrace != NULL)
			token_rele(st->st_rbrace);
	}
	VECTOR_FREE(ss->ss_stmts);
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
	st = simple_stmt_alloc(ss, indent, flags);
	if (st == NULL)
		return dc;
	token_ref(lbrace);
	st->st_lbrace = lbrace;
	token_ref(rbrace);
	st->st_rbrace = rbrace;
	return st->st_indent;
}

struct doc *
simple_stmt_no_braces_enter(struct simple_stmt *ss, struct doc *dc,
    struct token *lbrace, unsigned int indent, void **cookie)
{
	struct stmt *st;
	unsigned int flags = 0;

	if (!is_brace_moveable(ss, lbrace))
		flags |= STMT_IGNORE;
	st = simple_stmt_alloc(ss, indent, flags);
	if (st == NULL)
		return dc;
	token_ref(lbrace);
	st->st_lbrace = lbrace;
	*cookie = st;
	return st->st_indent;
}

void
simple_stmt_no_braces_leave(struct simple_stmt *ss, struct token *rbrace,
    void *cookie)
{
	struct stmt *st = cookie;

	if (!is_brace_moveable(ss, rbrace))
		st->st_flags |= STMT_IGNORE;
	token_ref(rbrace);
	st->st_rbrace = rbrace;
}

static struct stmt *
simple_stmt_alloc(struct simple_stmt *ss, unsigned int indent,
    unsigned int flags)
{
	struct stmt *st;

	/* Prevent reallocations causing dangling pointers. */
	if (VECTOR_LENGTH(ss->ss_stmts) >= SIMPLE_STMT_MAX)
		return NULL;

	st = VECTOR_CALLOC(ss->ss_stmts);
	if (st == NULL)
		err(1, NULL);
	st->st_root = doc_root(ss->ss_arena.doc_scope);
	st->st_indent = doc_indent(indent, st->st_root);
	doc_alloc(DOC_HARDLINE, st->st_indent);
	st->st_flags = flags;
	return st;
}

static int
is_stmt_empty(const struct simple_stmt *ss, const struct stmt *st)
{
	const struct token *lbrace = st->st_lbrace;
	const struct token *rbrace = st->st_rbrace;
	const struct token *nx = token_next(lbrace);

	if (st->st_flags & STMT_BRACES) {
		/*
		 * GCC warning option Wempty-body (implied by Wextra) suggests
		 * adding braces around statement consisting only of a
		 * semicolon.
		 */
		if (nx == token_prev(rbrace) && nx->tk_type == TOKEN_SEMI &&
		    VECTOR_LENGTH(ss->ss_stmts) == 1)
			return 1;
		return nx == rbrace;
	}

	return nx == rbrace && lbrace->tk_type != TOKEN_SEMI;
}

static int
need_braces(struct simple_stmt *ss, const struct stmt *st, struct buffer *bf)
{
	const char *buf;
	size_t buflen;

	if (is_stmt_empty(ss, st))
		return 1;

	buffer_reset(bf);
	doc_exec(&(struct doc_exec_arg){
	    .dc		= st->st_root,
	    .scratch	= ss->ss_arena.scratch,
	    .bf		= bf,
	    .st		= ss->ss_st,
	    .op		= ss->ss_op,
	});
	buflen = buffer_get_len(bf);
	buf = strtrim(buffer_get_ptr(bf), &buflen);
	if (isoneline(buf, buflen)) {
		return (st->st_flags & STMT_BRACES) &&
		    !token_is_moveable(st->st_rbrace);
	}
	return 1;
}

static void
add_braces(struct simple_stmt *ss)
{
	struct lexer *lx = ss->ss_lx;
	size_t i;

	for (i = 0; i < VECTOR_LENGTH(ss->ss_stmts); i++) {
		struct stmt *st = &ss->ss_stmts[i];
		struct token *lbrace, *pv, *rbrace;

		if (st->st_flags & (STMT_IGNORE | STMT_BRACES))
			continue;

		pv = token_prev(st->st_lbrace);
		lbrace = lexer_insert_after(lx, pv,
		    clang_keyword_token(TOKEN_LBRACE));
		token_move_suffixes(pv, lbrace);

		pv = token_prev(st->st_rbrace);
		rbrace = lexer_insert_after(lx, pv,
		    clang_keyword_token(TOKEN_RBRACE));
		token_move_suffixes_if(pv, rbrace, TOKEN_SPACE);
	}
}

static void
remove_braces(struct simple_stmt *ss)
{
	struct lexer *lx = ss->ss_lx;
	size_t i;

	for (i = 0; i < VECTOR_LENGTH(ss->ss_stmts); i++) {
		struct stmt *st = &ss->ss_stmts[i];

		if ((st->st_flags & STMT_IGNORE) ||
		    (st->st_flags & STMT_BRACES) == 0)
			continue;

		lexer_remove(lx, st->st_lbrace);
		lexer_remove(lx, st->st_rbrace);
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
	if (ss->ss_op->diffparse && (tk->tk_flags & TOKEN_FLAG_DIFF) == 0)
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
