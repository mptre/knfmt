#include "parser-expr.h"

#include "config.h"

#include "libks/compiler.h"

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-braces.h"
#include "parser-priv.h"
#include "parser-stmt-expr.h"
#include "parser-type.h"
#include "simple.h"
#include "token.h"

static struct doc	*expr_recover(const struct expr_exec_arg *, void *);
static struct doc	*expr_recover_cast(const struct expr_exec_arg *,
    void *);
static struct doc	*expr_doc_token(struct token *, struct doc *,
    const char *, int, void *);

int
parser_expr_peek(struct parser *pr, struct token **tk)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.lx		= pr->pr_lx,
		.arena		= {
			.scratch	= pr->pr_arena.scratch,
			.buffer		= pr->pr_arena.buffer,
		},
		.callbacks	= {
			.recover	= expr_recover,
			.recover_cast	= expr_recover_cast,
			.doc_token	= expr_doc_token,
			.arg		= pr,
		},
	};
	int peek, simple;

	simple = simple_disable(pr->pr_si);
	peek = expr_peek(&ea, tk);
	simple_enable(pr->pr_si, simple);
	return peek;
}

int
parser_expr(struct parser *pr, struct doc **expr, struct parser_expr_arg *arg)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.si		= pr->pr_si,
		.lx		= pr->pr_lx,
		.dc		= arg->dc,
		.rl		= arg->rl,
		.stop		= arg->stop,
		.indent		= arg->indent,
		.align		= arg->align,
		.flags		= arg->flags,
		.arena		= {
			.scratch	= pr->pr_arena.scratch,
			.buffer		= pr->pr_arena.buffer,
		},
		.callbacks	= {
			.recover	= expr_recover,
			.recover_cast	= expr_recover_cast,
			.doc_token	= expr_doc_token,
			.arg		= pr,
		},
	};
	struct doc *ex;

	ex = expr_exec(&ea);
	if (ex == NULL)
		return parser_none(pr);
	if (expr != NULL)
		*expr = ex;
	return parser_good(pr);
}

static struct doc *
expr_recover(const struct expr_exec_arg *ea, void *arg)
{
	struct parser_type type;
	struct doc *dc = NULL;
	struct parser *pr = arg;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *tk;

	if (parser_type_peek(pr, &type, PARSER_TYPE_EXPR)) {
		struct token *nx;

		if (lexer_back_if(lx, TOKEN_SIZEOF, NULL) ||
		    ((lexer_back_if(lx, TOKEN_LPAREN, NULL) ||
		      lexer_back_if(lx, TOKEN_COMMA, NULL)) &&
		     ((nx = token_next(type.end)) != NULL &&
		      (nx->tk_type == TOKEN_RPAREN ||
		       nx->tk_type == TOKEN_COMMA ||
		       nx->tk_type == LEXER_EOF)))) {
			dc = doc_root(pr->pr_arena.doc_scope);
			if (parser_type(pr, dc, &type, NULL) & GOOD)
				return dc;
		}
	} else if (lexer_if_flags(lx, TOKEN_FLAG_BINARY, &tk)) {
		struct token *pv;

		pv = token_prev(tk);
		if (pv != NULL &&
		    (pv->tk_type == TOKEN_LPAREN ||
		     pv->tk_type == TOKEN_COMMA)) {
			dc = doc_root(pr->pr_arena.doc_scope);
			parser_doc_token(pr, tk, dc);
			return dc;
		}
	} else if (lexer_peek_if(lx, TOKEN_LBRACE, &lbrace)) {
		int error;

		dc = doc_root(pr->pr_arena.doc_scope);
		error = parser_braces(pr, dc, dc, ea->indent,
		    PARSER_BRACES_DEDENT | PARSER_BRACES_INDENT_MAYBE);
		if (error & GOOD)
			return dc;
		if (error & FAIL) {
			/* Try again, could be a GNU statement expression. */
			dc = doc_root(pr->pr_arena.doc_scope);
			parser_reset(pr);
			lexer_seek(lx, lbrace);
			if (parser_stmt_expr_gnu(pr, dc) & GOOD)
				return dc;
		}
	} else if (lexer_if(lx, TOKEN_COMMA, &tk)) {
		/* Some macros allow empty arguments such as queue(3). */
		dc = doc_root(pr->pr_arena.doc_scope);
		parser_doc_token(pr, tk, dc);
		return dc;
	}

	return NULL;
}

static int
peek_binary_operator(struct lexer *lx, const struct token *rparen)
{
	struct token *op;

	return lexer_peek_if_flags(lx, TOKEN_FLAG_BINARY, &op) &&
	    token_has_spaces(rparen) &&
	    (token_has_spaces(op) || token_has_line(op, 1));
}

static struct doc *
expr_recover_cast(const struct expr_exec_arg *UNUSED(ea), void *arg)
{
	struct parser_type type;
	struct doc *dc = NULL;
	struct lexer_state s;
	struct parser *pr = arg;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (parser_type_peek(pr, &type, PARSER_TYPE_CAST) &&
	    lexer_seek_after(lx, type.end) &&
	    lexer_if(lx, TOKEN_RPAREN, &rparen) &&
	    !lexer_if(lx, TOKEN_RPAREN, NULL) &&
	    !lexer_if(lx, TOKEN_COMMA, NULL) &&
	    !peek_binary_operator(lx, rparen) &&
	    !(lexer_if(lx, TOKEN_AMP, NULL) &&
	    lexer_if(lx, TOKEN_TILDE, NULL)) &&
	    !lexer_if(lx, LEXER_EOF, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return NULL;

	dc = doc_root(pr->pr_arena.doc_scope);
	if (parser_type(pr, dc, &type, NULL) & GOOD)
		return dc;
	return NULL;
}

static struct doc *
expr_doc_token(struct token *tk, struct doc *dc, const char *fun, int lno,
    void *arg)
{
	struct parser *pr = arg;

	return parser_doc_token_impl(pr, tk, dc, fun, lno);
}
