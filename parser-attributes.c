#include "parser-attributes.h"

#include "config.h"

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "style.h"
#include "token.h"

int
parser_attributes(struct parser *pr, struct doc *dc, struct doc **out,
    unsigned int flags)
{
	struct doc *concat, *def;
	struct lexer *lx = pr->pr_lx;
	enum doc_type linetype;
	int nattributes = 0;

	if (!lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		return parser_none(pr);

	if (out == NULL)
		out = &def;
	linetype = (flags & PARSER_ATTRIBUTES_HARDLINE) ?
	    DOC_HARDLINE : DOC_LINE;

	doc_alloc(linetype, dc);
	for (;;) {
		struct token *tk;
		int error;

		if (!lexer_if(lx, TOKEN_ATTRIBUTE, &tk))
			break;

		if (nattributes > 0)
			doc_alloc(linetype, concat);
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		*out = concat;
		if (nattributes > 0)
			doc_alloc(DOC_SOFTLINE, concat);
		doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, concat);
		error = parser_expr(pr, out, &(struct parser_expr_arg){
		    .dc		= concat,
		    .indent	= style(pr->pr_st, ContinuationIndentWidth),
		    .flags	= EXPR_EXEC_SOFTLINE,
		});
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, *out);
		nattributes++;
	}

	return parser_good(pr);
}
