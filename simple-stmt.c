#include "simple-stmt.h"

#include "config.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "doc.h"
#include "lexer.h"
#include "options.h"
#include "token.h"
#include "token-type.h"
#include "vector.h"

struct stmt {
	struct doc	*st_root;
	struct doc	*st_indent;
	struct token	*st_lbrace;
	struct token	*st_rbrace;
	unsigned int	 st_flags;
#define STMT_FLAG_BRACES		0x00000001u
#define STMT_FLAG_IGNORE		0x00000002u
};

struct simple_stmt {
	VECTOR(struct stmt)	 ss_stmts;
	struct lexer		*ss_lx;
	const struct options	*ss_op;
};

static struct stmt	*simple_stmt_alloc(struct simple_stmt *, int,
    unsigned int);

static int	linecount(const char *, size_t, int);

struct simple_stmt *
simple_stmt_enter(struct lexer *lx, const struct options *op)
{
	struct simple_stmt *ss;

	ss = calloc(1, sizeof(*ss));
	if (ss == NULL)
		err(1, NULL);
	if (VECTOR_INIT(ss->ss_stmts) == NULL)
		err(1, NULL);
	ss->ss_lx = lx;
	ss->ss_op = op;
	return ss;
}

void
simple_stmt_leave(struct simple_stmt *ss)
{
	struct buffer *bf = NULL;
	struct lexer *lx = ss->ss_lx;
	size_t i;
	int oneline = 1;

	if (VECTOR_EMPTY(ss->ss_stmts))
		return;

	bf = buffer_alloc(1024);

	for (i = 0; i < VECTOR_LENGTH(ss->ss_stmts); i++) {
		struct stmt *st = &ss->ss_stmts[i];

		if ((st->st_flags & STMT_FLAG_IGNORE) ||
		    (st->st_flags & STMT_FLAG_BRACES) == 0)
			continue;

		doc_exec(st->st_root, lx, bf, ss->ss_op, 0);
		if (!linecount(bf->bf_ptr, bf->bf_len, 1) ||
		    token_has_prefix(st->st_rbrace, TOKEN_COMMENT)) {
			/*
			 * No point in continuing as at least one statement
			 * spans over multiple lines.
			 */
			oneline = 0;
			break;
		}
	}

	if (oneline) {
		for (i = 0; i < VECTOR_LENGTH(ss->ss_stmts); i++) {
			struct stmt *st = &ss->ss_stmts[i];

			if ((st->st_flags & STMT_FLAG_IGNORE) ||
			    (st->st_flags & STMT_FLAG_BRACES) == 0)
				continue;

			lexer_remove(lx, st->st_lbrace, 1);
			lexer_remove(lx, st->st_rbrace, 1);
		}
	} else {
		for (i = 0; i < VECTOR_LENGTH(ss->ss_stmts); i++) {
			struct stmt *st = &ss->ss_stmts[i];
			struct token *pv, *tk;

			if (st->st_flags &
			    (STMT_FLAG_IGNORE | STMT_FLAG_BRACES))
				continue;

			pv = token_prev(st->st_lbrace);
			tk = lexer_insert_before(lx, st->st_lbrace,
			    TOKEN_LBRACE, "{");
			if (pv != NULL)
				token_list_move(&pv->tk_suffixes,
				    &tk->tk_suffixes);

			pv = token_prev(st->st_rbrace);
			tk = lexer_insert_before(lx, st->st_rbrace,
			    TOKEN_RBRACE, "}");
			if (pv != NULL)
				token_list_move(&pv->tk_suffixes,
				    &tk->tk_suffixes);
		}
	}

	buffer_free(bf);
}

void
simple_stmt_free(struct simple_stmt *ss)
{
	if (ss == NULL)
		return;

	while (!VECTOR_EMPTY(ss->ss_stmts)) {
		struct stmt *st;

		st = VECTOR_POP(ss->ss_stmts);
		doc_free(st->st_root);
		if (st->st_lbrace != NULL)
			token_rele(st->st_lbrace);
		if (st->st_rbrace != NULL)
			token_rele(st->st_rbrace);
	}
	VECTOR_FREE(ss->ss_stmts);
	free(ss);
}

struct doc *
simple_stmt_block(struct simple_stmt *ss, struct token *lbrace,
    struct token *rbrace, int indent)
{
	struct stmt *st;
	unsigned int flags = STMT_FLAG_BRACES;

	/* Make sure both braces are covered by a diff chunk. */
	if ((ss->ss_op->op_flags & OPTIONS_FLAG_DIFFPARSE) &&
	    ((lbrace->tk_flags & TOKEN_FLAG_DIFF) == 0 ||
	     (rbrace->tk_flags & TOKEN_FLAG_DIFF) == 0))
		flags |= STMT_FLAG_IGNORE;
	st = simple_stmt_alloc(ss, indent, flags);
	token_ref(lbrace);
	st->st_lbrace = lbrace;
	token_ref(rbrace);
	st->st_rbrace = rbrace;
	return st->st_indent;
}

void *
simple_stmt_ifelse_enter(struct simple_stmt *ss, struct token *lbrace,
    int indent)
{
	struct stmt *st;

	st = simple_stmt_alloc(ss, indent, 0);
	token_ref(lbrace);
	st->st_lbrace = lbrace;
	return st;
}

void
simple_stmt_ifelse_leave(struct simple_stmt *UNUSED(ss), struct token *rbrace,
    void *arg)
{
	struct stmt *st = arg;

	token_ref(rbrace);
	st->st_rbrace = rbrace;
}

static struct stmt *
simple_stmt_alloc(struct simple_stmt *ss, int indent, unsigned int flags)
{
	struct stmt *st;

	st = VECTOR_CALLOC(ss->ss_stmts);
	if (st == NULL)
		err(1, NULL);
	if (st == NULL)
		err(1, NULL);
	st->st_root = doc_alloc(DOC_CONCAT, NULL);
	st->st_indent = doc_alloc_indent(indent, st->st_root);
	st->st_flags = flags;
	return st;
}

/*
 * Count the number of lines. Returns non-zero if it's equal count and zero
 * otherwise.
 */
static int
linecount(const char *str, size_t len, int count)
{
	int n = 0;

	while (len > 0) {
		const char *p;

		p = memchr(str, '\n', len);
		if (p == NULL)
			break;
		if (++n > count)
			break;
		len -= (p - str) + 1;
		str = &p[1];
	}
	return n == count;
}
