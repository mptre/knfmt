#include "parser-stmt-asm.h"

#include "config.h"

#include "doc.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "style.h"
#include "token.h"

static int
parser_stmt_asm_operand(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat, *expr;
	struct token *comma, *pv, *tk;

	if (!lexer_back(lx, &pv) ||
	    (!lexer_peek_if(lx, TOKEN_LSQUARE, NULL) &&
	     !lexer_peek_if(lx, TOKEN_STRING, NULL)))
		return parser_none(pr);

	/* Favor break before the operand. */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_alloc(pv->tk_type == TOKEN_COLON ? DOC_LINE : DOC_SOFTLINE, concat);
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, concat));

	/* symbolic name */
	if (lexer_if(lx, TOKEN_LSQUARE, &tk)) {
		doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_IDENT, &tk))
			doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_RSQUARE, &tk))
			doc_token(tk, concat);
		doc_alloc(DOC_LINE, concat);
	}

	/* constraint */
	if (lexer_expect(lx, TOKEN_STRING, &tk)) {
		doc_token(tk, concat);
		doc_alloc(DOC_LINE, concat);
	}

	/* cexpression */
	if (lexer_expect(lx, TOKEN_LPAREN, &tk)) {
		int error;

		doc_token(tk, concat);
		error = parser_expr(pr, &expr, &(struct parser_expr_arg){
		    .dc	= concat,
		});
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, expr);
		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			doc_token(comma, expr);
			doc_alloc(DOC_LINE, expr);
		}
	}

	return parser_good(pr);
}

int
parser_asm(struct parser *pr, struct doc *dc)
{
	int error;

	error = parser_stmt_asm(pr, dc);
	if (error & HALT)
		return error;
	doc_alloc(DOC_HARDLINE, dc);
	return parser_good(pr);
}

int
parser_stmt_asm(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat, *opt;
	struct token *colon = NULL;
	struct token *qualifier = NULL;
	struct token *assembly, *rparen, *tk;
	int ninputs = 0;
	int error;

	if (!lexer_peek_if(lx, TOKEN_ASSEMBLY, NULL))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	if (lexer_expect(lx, TOKEN_ASSEMBLY, &assembly))
		doc_token(assembly, concat);

	while (lexer_if(lx, TOKEN_VOLATILE, &qualifier) ||
	    lexer_if(lx, TOKEN_INLINE, &qualifier) ||
	    lexer_if(lx, TOKEN_GOTO, &qualifier)) {
		doc_alloc(DOC_LINE, concat);
		doc_token(qualifier, concat);
	}
	if (qualifier != NULL && token_has_spaces(qualifier))
		doc_alloc(DOC_LINE, concat);

	opt = doc_indent(style(pr->pr_st, ContinuationIndentWidth),
	    doc_alloc(DOC_OPTIONAL, dc));
	if (lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		parser_token_trim_before(pr, rparen);
	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, opt);

	/* instructions */
	if (!lexer_peek_until(lx, TOKEN_COLON, &colon)) {
		/* Basic inline assembler, only instructions are required. */
		concat = opt;
	}
	error = parser_expr(pr, NULL, &(struct parser_expr_arg){
	    .dc		= opt,
	    .stop	= colon,
	});
	if (error & HALT)
		return parser_fail(pr);

	/* input operands */
	if (lexer_if(lx, TOKEN_COLON, &colon)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		doc_alloc(DOC_LINE, concat);
		doc_token(colon, concat);
		concat = doc_indent(2, concat);
		while (parser_stmt_asm_operand(pr, concat) & GOOD)
			ninputs++;
	}

	/* output operands */
	if (lexer_if(lx, TOKEN_COLON, &colon)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		if (ninputs > 0)
			doc_alloc(DOC_LINE, concat);
		doc_token(colon, concat);
		concat = doc_indent(2, concat);
		while (parser_stmt_asm_operand(pr, concat) & GOOD)
			continue;
	}

	/* clobbers */
	if (lexer_if(lx, TOKEN_COLON, &colon)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		doc_token(colon, concat);
		if (!lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			doc_alloc(DOC_LINE, concat);
		error = parser_expr(pr, NULL, &(struct parser_expr_arg){
		    .dc	= concat,
		});
		if (error & FAIL)
			return parser_fail(pr);
	}

	/* goto labels */
	if (lexer_if(lx, TOKEN_COLON, &colon)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		doc_token(colon, concat);
		if (!lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			doc_alloc(DOC_LINE, concat);
		error = parser_expr(pr, NULL, &(struct parser_expr_arg){
		    .dc	= concat,
		});
		if (error & FAIL)
			return parser_fail(pr);
	}

	if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
		doc_token(rparen, concat);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);

	return parser_good(pr);
}
