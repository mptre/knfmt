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
parser_attributes(struct parser *pr, struct doc *dc, struct doc **out)
{
	struct doc *def, *optional;
	struct lexer *lx = pr->pr_lx;
	struct token *pv;
	enum doc_type linetype;

	if (!lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		return parser_none(pr);

	if (out == NULL)
		out = &def;
	linetype = lexer_back(lx, &pv) && token_has_line(pv, 1) ?
	    DOC_HARDLINE : DOC_LINE;

	optional = doc_alloc(DOC_CONCAT, doc_alloc(DOC_OPTIONAL, dc));
	for (;;) {
		struct doc *concat;
		struct token *tk;
		int error;

		if (!lexer_if(lx, TOKEN_ATTRIBUTE, &tk))
			break;

		doc_alloc(linetype, optional);
		linetype = DOC_LINE;
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, optional));
		*out = concat;
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
	}

	return parser_good(pr);
}
