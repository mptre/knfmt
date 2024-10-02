#include "parser-attributes.h"

#include "config.h"

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "simple-attributes.h"
#include "simple.h"
#include "style.h"
#include "token.h"

int
parser_attributes_peek(struct parser *pr, struct token **rparen,
    unsigned int flags)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	int nattributes = 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		struct token *tmp;

		if (lexer_if(lx, TOKEN_ATTRIBUTE, NULL) &&
		    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL,
		    rparen)) {
			/* nothing */
		} else if ((flags & PARSER_ATTRIBUTES_FUNC) &&
		    lexer_if(lx, TOKEN_IDENT, NULL) &&
		    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL, &tmp) &&
		    lexer_if(lx, TOKEN_IDENT, NULL) &&
		    lexer_if(lx, TOKEN_LPAREN, NULL)) {
			/* Possible attribute hidden behind cpp macro. */
			*rparen = tmp;
		} else {
			break;
		}
		nattributes++;
	}
	lexer_peek_leave(lx, &s);
	return nattributes > 0;
}

static int
parser_attributes_expr(struct parser *pr, struct doc *dc, struct doc **out,
    struct token *rparen)
{
	struct lexer *lx = pr->pr_lx;

	simple_cookie(cookie);
	simple_enter(pr->pr_si, SIMPLE_ATTRIBUTES, 0, &cookie);

	for (;;) {
		struct token *comma, *nx, *stop;
		int error;

		if (is_simple_enabled(pr->pr_si, SIMPLE_ATTRIBUTES))
			simple_attributes(lx, pr->pr_arena.eternal_scope);

		lexer_peek_until_comma(lx, rparen, &stop);
		error = parser_expr(pr, out, &(struct parser_expr_arg){
		    .dc		= dc,
		    .stop	= stop,
		    .indent	= style(pr->pr_st, ContinuationIndentWidth),
		    .flags	= EXPR_EXEC_SOFTLINE,
		});
		if (error & HALT)
			return parser_fail(pr);

		if (lexer_peek_if(lx, TOKEN_RPAREN, &nx) && nx == rparen)
			break;

		if (lexer_expect(lx, TOKEN_COMMA, &comma)) {
			parser_doc_token(pr, comma, *out);
			doc_alloc(DOC_LINE, *out);
		}
	}

	return parser_good(pr);
}

int
parser_attributes(struct parser *pr, struct doc *dc, struct doc **out,
    unsigned int flags)
{
	struct doc *def, *optional;
	struct lexer *lx = pr->pr_lx;
	struct token *end, *pv;
	enum doc_type linetype;
	int nattributes = 0;

	if (!parser_attributes_peek(pr, &end, flags))
		return parser_none(pr);

	if (out == NULL)
		out = &def;
	linetype = lexer_back(lx, &pv) && token_has_line(pv, 1) ?
	    DOC_HARDLINE : DOC_LINE;

	optional = doc_alloc(DOC_CONCAT, doc_alloc(DOC_OPTIONAL, dc));
	for (;;) {
		struct doc *concat;
		struct token *rparen = end;
		struct token *lparen, *tk;
		int error;

		if (!lexer_if(lx, TOKEN_ATTRIBUTE, &tk) &&
		    !lexer_if(lx, TOKEN_IDENT, &tk))
			break;

		if ((flags & PARSER_ATTRIBUTES_LINE) || nattributes > 0)
			doc_alloc(linetype, optional);
		linetype = DOC_LINE;
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, optional));
		parser_doc_token(pr, tk, concat);
		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			parser_doc_token(pr, tk, concat);
		if (lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN,
		    NULL, &rparen) && lexer_if(lx, TOKEN_LPAREN, &lparen))
			parser_doc_token(pr, lparen, concat);
		error = parser_attributes_expr(pr, concat, out, rparen);
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
			parser_doc_token(pr, rparen, *out);
		if (lexer_if(lx, TOKEN_RPAREN, &rparen))
			parser_doc_token(pr, rparen, *out);
		nattributes++;
		if (rparen == end)
			break;
	}

	return parser_good(pr);
}
