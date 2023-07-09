#include "parser-expr.h"

#include "config.h"

#include "cdefs.h"
#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-braces.h"
#include "parser-priv.h"
#include "parser-stmt-expr.h"
#include "parser-type.h"
#include "simple.h"
#include "token.h"

static int	expr_recover(const struct expr_exec_arg *, struct doc *,
    void *);
static int	expr_recover_cast(const struct expr_exec_arg *, struct doc *,
    void *);

int
parser_expr_peek(struct parser *pr, struct token **tk)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.callbacks	= {
			.recover	= expr_recover,
			.recover_cast	= expr_recover_cast,
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
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.rl		= arg->rl,
		.dc		= arg->dc,
		.stop		= arg->stop,
		.indent		= arg->indent,
		.flags		= arg->flags,
		.callbacks	= {
			.recover	= expr_recover,
			.recover_cast	= expr_recover_cast,
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

static int
expr_recover(const struct expr_exec_arg *ea, struct doc *dc, void *arg)
{
	struct parser_type type;
	struct parser *pr = arg;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *tk;

	if (parser_type_peek(pr, &type, PARSER_TYPE_EXPR)) {
		struct token *nx, *pv;

		if (lexer_back(lx, &pv) &&
		    (pv->tk_type == TOKEN_SIZEOF ||
		     ((pv->tk_type == TOKEN_LPAREN ||
		       pv->tk_type == TOKEN_COMMA) &&
		      ((nx = token_next(type.end)) != NULL &&
		       (nx->tk_type == TOKEN_RPAREN ||
			nx->tk_type == TOKEN_COMMA ||
			nx->tk_type == LEXER_EOF))))) {
			if (parser_type(pr, dc, &type, NULL) & GOOD)
				return 1;
		}
	} else if (lexer_if_flags(lx, TOKEN_FLAG_BINARY, &tk)) {
		struct token *pv;

		pv = token_prev(tk);
		if (pv != NULL &&
		    (pv->tk_type == TOKEN_LPAREN ||
		     pv->tk_type == TOKEN_COMMA)) {
			doc_token(tk, dc);
			return 1;
		}
	} else if (lexer_peek_if(lx, TOKEN_LBRACE, &lbrace)) {
		int error;

		error = parser_braces(pr, dc, ea->indent,
		    PARSER_BRACES_DEDENT | PARSER_BRACES_INDENT_MAYBE);
		if (error & GOOD)
			return 1;
		if (error & FAIL) {
			/* Try again, could be a GNU statement expression. */
			while (doc_remove_tail(dc))
				continue;
			parser_reset(pr);
			lexer_seek(lx, lbrace);
			if (parser_stmt_expr_gnu(pr, dc) & GOOD)
				return 1;
		}
	} else if (lexer_if(lx, TOKEN_COMMA, &tk)) {
		/* Some macros allow empty arguments such as queue(3). */
		doc_token(tk, dc);
		return 1;
	}

	return 0;
}

static int
expr_recover_cast(const struct expr_exec_arg *UNUSED(ea), struct doc *dc,
    void *arg)
{
	struct parser_type type;
	struct lexer_state s;
	struct parser *pr = arg;
	struct lexer *lx = pr->pr_lx;
	struct token *op, *rparens;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (parser_type_peek(pr, &type, PARSER_TYPE_CAST) &&
	    lexer_seek_after(lx, type.end) &&
	    lexer_if(lx, TOKEN_RPAREN, &rparens) &&
	    !lexer_if(lx, TOKEN_COMMA, NULL) &&
	    !(lexer_peek_if_flags(lx, TOKEN_FLAG_BINARY, &op) &&
	    token_has_spaces(rparens) && token_has_spaces(op)) &&
	    !(lexer_if(lx, TOKEN_AMP, NULL) &&
	    lexer_if(lx, TOKEN_TILDE, NULL)) &&
	    !lexer_if(lx, LEXER_EOF, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return 0;
	return parser_type(pr, dc, &type, NULL) & GOOD;
}
