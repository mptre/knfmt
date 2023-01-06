#include "parser-func.h"

#include "doc.h"
#include "lexer.h"
#include "parser-attributes.h"
#include "parser-priv.h"
#include "parser-type.h"
#include "token.h"

int
parser_func_arg(struct parser *pr, struct doc *dc, struct doc **out,
    const struct token *rparen)
{
	struct parser_private *pp = parser_get_private(pr);
	struct doc *concat;
	struct lexer *lx = pp->lx;
	struct token *pv = NULL;
	struct token *tk, *type;
	int error = 0;

	if (!parser_type_peek(pr, &type, LEXER_TYPE_ARG))
		return parser_none(pr);

	/*
	 * Let each argument begin with a soft line, causing a line to be
	 * emitted immediately if the argument does not fit instead of breaking
	 * the argument.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_alloc(DOC_SOFTLINE, concat);
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_OPTIONAL, concat));

	if (parser_type(pr, concat, type, NULL) & (FAIL | NONE))
		return parser_fail(pr);

	/* Put the argument identifier in its own group to trigger a refit. */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, concat));
	if (out != NULL)
		*out = concat;

	/* Put a line between the type and identifier when wanted. */
	if (type->tk_type != TOKEN_STAR &&
	    !lexer_peek_if(lx, TOKEN_COMMA, NULL) &&
	    !lexer_peek_if(lx, TOKEN_RPAREN, NULL) &&
	    !lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		doc_alloc(DOC_LINE, concat);

	for (;;) {
		if (lexer_peek_if(lx, LEXER_EOF, NULL))
			return parser_fail(pr);

		if (parser_attributes(pr, concat, NULL, 0) & GOOD)
			break;

		if (lexer_if(lx, TOKEN_COMMA, &tk)) {
			doc_token(tk, concat);
			doc_alloc(DOC_LINE, concat);
			break;
		}
		if (lexer_peek(lx, &tk) && tk == rparen)
			break;

		if (!lexer_pop(lx, &tk)) {
			error = parser_fail(pr);
			break;
		}
		/* Identifiers must be separated. */
		if (pv != NULL && pv->tk_type == TOKEN_IDENT &&
		    tk->tk_type == TOKEN_IDENT)
			doc_alloc(DOC_LINE, concat);
		doc_token(tk, concat);
		pv = tk;
	}

	return error ? error : parser_good(pr);
}
