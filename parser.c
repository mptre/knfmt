#include "parser.h"

#include "config.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "buffer.h"
#include "doc.h"
#include "error.h"
#include "lexer.h"
#include "options.h"
#include "parser-decl.h"
#include "parser-func.h"
#include "parser-priv.h"
#include "parser-stmt-asm.h"
#include "token.h"

static int	parser_exec1(struct parser *, struct doc *);

static int	parser_exec_extern(struct parser *, struct doc *);

static int	parser_simple_active(const struct parser *);

static int	parser_get_error(const struct parser *);

struct parser *
parser_alloc(const char *path, struct lexer *lx, struct error *er,
    const struct style *st, const struct options *op)
{
	struct parser *pr;

	pr = ecalloc(1, sizeof(*pr));
	pr->pr_path = path;
	pr->pr_er = er;
	pr->pr_st = st;
	pr->pr_op = op;
	pr->pr_lx = lx;
	pr->pr_scratch = buffer_alloc(1024);
	if (pr->pr_scratch == NULL)
		err(1, NULL);
	return pr;
}

void
parser_free(struct parser *pr)
{
	if (pr == NULL)
		return;

	buffer_free(pr->pr_scratch);
	free(pr);
}

struct buffer *
parser_exec(struct parser *pr, size_t sizehint)
{
	struct buffer *bf = NULL;
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	unsigned int flags;
	int error = 0;

	dc = doc_alloc(DOC_CONCAT, NULL);

	for (;;) {
		struct doc *concat;
		struct token *tk;

		concat = doc_alloc(DOC_CONCAT, dc);

		/* Always emit EOF token as it could have dangling tokens. */
		if (lexer_if(lx, LEXER_EOF, &tk)) {
			struct doc *eof;

			eof = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_GROUP, concat));
			doc_token(tk, eof);
			error = 0;
			break;
		}

		error = parser_exec1(pr, concat);
		if (error & GOOD) {
			lexer_stamp(lx);
		} else if (error & BRCH) {
			if (!lexer_branch(lx))
				break;
			parser_reset(pr);
		} else if (error & (FAIL | NONE)) {
			int r;

			r = lexer_recover(lx);
			if (r == 0)
				break;
			while (r-- > 0)
				doc_remove_tail(dc);
			parser_reset(pr);
		}
	}
	if (error) {
		parser_fail(pr);
		goto out;
	}

	bf = buffer_alloc(sizehint);
	if (bf == NULL)
		err(1, NULL);

	flags = DOC_EXEC_TRIM;
	if (pr->pr_op->op_flags & OPTIONS_DIFFPARSE)
		flags |= DOC_EXEC_DIFF;
	if (trace(pr->pr_op, 'd'))
		flags |= DOC_EXEC_TRACE;
	doc_exec(&(struct doc_exec_arg){
	    .dc		= dc,
	    .lx		= pr->pr_lx,
	    .bf		= bf,
	    .st		= pr->pr_st,
	    .op		= pr->pr_op,
	    .flags	= flags
	});

out:
	doc_free(dc);
	return bf;
}

static int
parser_exec1(struct parser *pr, struct doc *dc)
{
	int error;

	error = parser_decl(pr, dc,
	    PARSER_DECL_BREAK | PARSER_DECL_LINE |
	    PARSER_DECL_ROOT);
	if (error & NONE)
		error = parser_func_impl(pr, dc);
	if (error & NONE)
		error = parser_exec_extern(pr, dc);
	if (error & NONE)
		error = parser_asm(pr, dc);
	return error;
}

static int
parser_exec_extern(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_EXTERN, NULL) &&
	    lexer_if(lx, TOKEN_STRING, NULL) &&
	    lexer_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_EXTERN, &tk))
		doc_token(tk, dc);
	doc_literal(" ", dc);
	if (lexer_expect(lx, TOKEN_STRING, &tk))
		doc_token(tk, dc);
	doc_literal(" ", dc);
	if (lexer_expect(lx, TOKEN_LBRACE, &tk))
		doc_token(tk, dc);
	doc_alloc(DOC_HARDLINE, dc);
	for (;;) {
		int error;

		error = parser_exec1(pr, dc);
		if (error & NONE)
			break;
		if (error & HALT)
			return error;
	}
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, dc);
	doc_alloc(DOC_HARDLINE, dc);
	return parser_good(pr);
}

static int
parser_simple_active(const struct parser *pr)
{
	return pr->pr_simple.stmt != NULL || pr->pr_simple.decl != NULL;
}

int
parser_fail0(struct parser *pr, const char *fun, int lno)
{
	struct buffer *bf;
	struct token *tk;

	if (parser_get_error(pr))
		goto out;
	pr->pr_error = 1;

	bf = error_begin(pr->pr_er);
	buffer_printf(bf, "%s: ", pr->pr_path);
	if (trace(pr->pr_op, 'l'))
		buffer_printf(bf, "%s:%d: ", fun, lno);
	buffer_printf(bf, "error at ");
	if (lexer_back(pr->pr_lx, &tk))
		buffer_printf(bf, "%s\n", lexer_serialize(pr->pr_lx, tk));
	else
		buffer_printf(bf, "(null)\n");
	error_end(pr->pr_er);

out:
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return FAIL;
}

/*
 * Remove any preceding hard line(s) from the given token, unless the token has
 * prefixes intended to be emitted.
 */
void
parser_token_trim_before(const struct parser *pr, struct token *tk)
{
	struct token *pv;

	/* Avoid side effects in simple mode. */
	if (parser_simple_active(pr))
		return;

	if (!token_is_moveable(tk))
		return;
	pv = token_prev(tk);
	if (pv != NULL)
		token_trim(pv);
}

void
parser_simple_disable(struct parser *pr, struct parser_simple *simple)
{
	*simple = pr->pr_simple;
	memset(&pr->pr_simple, 0, sizeof(pr->pr_simple));
}

void
parser_simple_enable(struct parser *pr, const struct parser_simple *simple)
{
	pr->pr_simple = *simple;
}

/*
 * Remove any subsequent hard line(s) from the given token.
 */
void
parser_token_trim_after(const struct parser *pr, struct token *tk)
{
	/* Avoid side effects in simple mode. */
	if (parser_simple_active(pr))
		return;
	token_trim(tk);
}

/*
 * Returns the width of the given document.
 */
unsigned int
parser_width(struct parser *pr, const struct doc *dc)
{
	return doc_width(&(struct doc_exec_arg){
	    .dc	= dc,
	    .bf	= pr->pr_scratch,
	    .st	= pr->pr_st,
	    .op	= pr->pr_op
	});
}

static int
parser_get_error(const struct parser *pr)
{
	return pr->pr_error || lexer_get_error(pr->pr_lx);
}

int
parser_good(const struct parser *pr)
{
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return parser_get_error(pr) ? FAIL : GOOD;
}

int
parser_none(const struct parser *pr)
{
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return parser_get_error(pr) ? FAIL : NONE;
}

void
parser_reset(struct parser *pr)
{
	error_reset(pr->pr_er);
	pr->pr_error = 0;
}
