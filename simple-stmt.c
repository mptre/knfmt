#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

struct stmt {
	struct doc		*st_root;
	struct doc		*st_indent;
	struct token		*st_lbrace;
	struct token		*st_rbrace;
	unsigned int		 st_flags;
#define STMT_FLAG_BRACES		0x00000001u

	TAILQ_ENTRY(stmt)	 st_entry;
};

struct simple_stmt {
	TAILQ_HEAD(, stmt)	 ss_list;
	struct lexer		*ss_lx;
	const struct config	*ss_cf;
};

static struct stmt	*simple_stmt_alloc(struct simple_stmt *, int,
    unsigned int);

static int	linecount(const char *, size_t, int);

struct simple_stmt *
simple_stmt_enter(struct lexer *lx, const struct config *cf)
{
	struct simple_stmt *ss;

	ss = calloc(1, sizeof(*ss));
	if (ss == NULL)
		err(1, NULL);
	TAILQ_INIT(&ss->ss_list);
	ss->ss_lx = lx;
	ss->ss_cf = cf;
	return ss;
}

void
simple_stmt_leave(struct simple_stmt *ss)
{
	struct buffer *bf = NULL;
	struct lexer *lx = ss->ss_lx;
	struct stmt *st;
	int oneline = 1;

	if (TAILQ_EMPTY(&ss->ss_list))
		return;

	bf = buffer_alloc(1024);

	TAILQ_FOREACH(st, &ss->ss_list, st_entry) {
		if ((st->st_flags & STMT_FLAG_BRACES) == 0)
			continue;

		doc_exec(st->st_root, lx, bf, ss->ss_cf,
		    DOC_EXEC_FLAG_NODIFF | DOC_EXEC_FLAG_NOTRACE);
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
		TAILQ_FOREACH(st, &ss->ss_list, st_entry) {
			if ((st->st_flags & STMT_FLAG_BRACES) == 0)
				continue;
			lexer_remove(lx, st->st_lbrace, 1);
			lexer_remove(lx, st->st_rbrace, 1);
		}
	} else {
		TAILQ_FOREACH(st, &ss->ss_list, st_entry) {
			struct token *pv, *tk;

			if (st->st_flags & STMT_FLAG_BRACES)
				continue;

			pv = TAILQ_PREV(st->st_lbrace, token_list, tk_entry);
			tk = lexer_insert_before(lx, st->st_lbrace,
			    TOKEN_LBRACE, "{");
			if (pv != NULL)
				token_list_move(&pv->tk_suffixes,
				    &tk->tk_suffixes);

			pv = TAILQ_PREV(st->st_rbrace, token_list, tk_entry);
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

	while (!TAILQ_EMPTY(&ss->ss_list)) {
		struct stmt *st;

		st = TAILQ_FIRST(&ss->ss_list);
		TAILQ_REMOVE(&ss->ss_list, st, st_entry);
		doc_free(st->st_root);
		if (st->st_lbrace != NULL)
			token_rele(st->st_lbrace);
		if (st->st_rbrace != NULL)
			token_rele(st->st_rbrace);
		free(st);
	}
	free(ss);
}

struct doc *
simple_stmt_block(struct simple_stmt *ss, struct token *lbrace,
    struct token *rbrace, int indent)
{
	struct stmt *st;
	int braces = 1;

	/* Make sure both braces are covered by a diff chunk. */
	if (DIFF(ss->ss_cf) &&
	    ((lbrace->tk_flags & TOKEN_FLAG_DIFF) == 0 ||
	     (rbrace->tk_flags & TOKEN_FLAG_DIFF) == 0))
		braces = 0;
	st = simple_stmt_alloc(ss, indent, braces ? STMT_FLAG_BRACES : 0);
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

	st = calloc(1, sizeof(*st));
	if (st == NULL)
		err(1, NULL);
	st->st_root = doc_alloc(DOC_CONCAT, NULL);
	st->st_indent = doc_alloc_indent(indent, st->st_root);
	st->st_flags = flags;
	TAILQ_INSERT_TAIL(&ss->ss_list, st, st_entry);
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
