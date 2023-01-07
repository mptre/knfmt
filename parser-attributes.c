#include "parser-attributes.h"

#include "doc.h"
#include "lexer.h"
#include "token.h"
#include "parser-expr.h"
#include "parser-priv.h"

int
parser_attributes(struct parser *pr, struct doc *dc, struct doc **out,
    unsigned int flags)
{
	struct parser_private *pp = parser_get_private(pr);
	struct doc *concat = NULL;
	struct lexer *lx = pp->lx;
	enum doc_type linetype = (flags & PARSER_ATTRIBUTES_HARDLINE) ?
	    DOC_HARDLINE : DOC_LINE;
	int nattributes = 0;

	if (!lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		return parser_none(pr);

	doc_alloc(linetype, dc);
	for (;;) {
		struct token *tk;
		int error;

		if (!lexer_if(lx, TOKEN_ATTRIBUTE, &tk))
			break;

		if (nattributes > 0)
			doc_alloc(linetype, concat);
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(DOC_SOFTLINE, concat);
		doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, concat);
		error = parser_expr(pr, concat, NULL, NULL, NULL, 0, 0);
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, concat);
		nattributes++;
	}
	if (out != NULL)
		*out = concat;

	return parser_good(pr);
}
