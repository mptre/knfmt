#include "parser-func.h"

#include "doc.h"
#include "lexer.h"
#include "parser-attributes.h"
#include "parser-priv.h"
#include "parser-type.h"
#include "token.h"

/*
 * Returns non-zero if the next tokens denotes a function. The type argument
 * points to the last token of the return type.
 */
enum parser_func_peek
parser_func_peek(struct parser *pr, struct token **type)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	enum parser_func_peek peek = PARSER_FUNC_PEEK_NONE;

	lexer_peek_enter(lx, &s);
	if (parser_type_peek(pr, type, 0) &&
	    lexer_seek(lx, token_next(*type))) {
		if (lexer_if(lx, TOKEN_IDENT, NULL)) {
			/* nothing */
		} else if (lexer_if(lx, TOKEN_LPAREN, NULL) &&
		    lexer_if(lx, TOKEN_STAR, NULL) &&
		    lexer_if(lx, TOKEN_IDENT, NULL) &&
		    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL) &&
		    lexer_if(lx, TOKEN_RPAREN, NULL)) {
			/*
			 * Function returning a function pointer, used by
			 * parser_exec_func_proto().
			 */
			(*type)->tk_flags |= TOKEN_FLAG_TYPE_FUNC;
		} else {
			goto out;
		}

		if (!lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL))
			goto out;

		while (lexer_if(lx, TOKEN_ATTRIBUTE, NULL) &&
		    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL))
			continue;

		if (lexer_if(lx, TOKEN_SEMI, NULL))
			peek = PARSER_FUNC_PEEK_DECL;
		else if (lexer_if(lx, TOKEN_LBRACE, NULL))
			peek = PARSER_FUNC_PEEK_IMPL;
		else if (parser_type_peek(pr, NULL, 0))
			peek = PARSER_FUNC_PEEK_IMPL;	/* K&R */
	}
out:
	lexer_peek_leave(lx, &s);
	return peek;
}

int
parser_func_arg(struct parser *pr, struct doc *dc, struct doc **out,
    const struct token *rparen)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *pv = NULL;
	struct token *tk, *type;
	int error = 0;

	if (!parser_type_peek(pr, &type, PARSER_TYPE_ARG))
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
