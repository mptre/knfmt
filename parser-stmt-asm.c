#include "parser-stmt-asm.h"

#include "config.h"

#include "doc.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "style.h"
#include "token.h"

/*
 * Returns a document which favors break before the operand.
 */
static struct doc *
doc_asm_operand(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;

	dc = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_alloc(lexer_back_if(lx, TOKEN_COLON, NULL) ?
	    DOC_LINE : DOC_SOFTLINE, dc);
	return doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
}

static int
parser_stmt_asm_operand_cpp(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *expr;
	struct token *stop = NULL;
	struct token *comma;
	int error;

	dc = doc_asm_operand(pr, dc);

	if (lexer_peek_until_comma(lx, NULL, &comma)) {
		parser_token_trim_before(pr, comma);
		stop = comma;
	}
	error = parser_expr(pr, &expr, &(struct parser_expr_arg){
	    .dc		= dc,
	    .stop	= stop,
	});
	if (error & HALT)
		return parser_fail(pr);
	if (lexer_if(lx, TOKEN_COMMA, &comma)) {
		parser_doc_token(pr, comma, expr);
		doc_alloc(DOC_LINE, expr);
	}
	return parser_good(pr);
}

static int
parser_stmt_asm_operand(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *expr;
	struct token *comma, *tk;

	if (!lexer_peek_if(lx, TOKEN_LSQUARE, NULL) &&
	    !lexer_peek_if(lx, TOKEN_STRING, NULL)) {
		if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
			return parser_stmt_asm_operand_cpp(pr, dc);
		return parser_none(pr);
	}

	dc = doc_asm_operand(pr, dc);

	/* symbolic name */
	if (lexer_if(lx, TOKEN_LSQUARE, &tk)) {
		parser_doc_token(pr, tk, dc);
		if (lexer_expect(lx, TOKEN_IDENT, &tk))
			parser_doc_token(pr, tk, dc);
		if (lexer_expect(lx, TOKEN_RSQUARE, &tk))
			parser_doc_token(pr, tk, dc);
		doc_alloc(DOC_LINE, dc);
	}

	/* constraint */
	if (lexer_expect(lx, TOKEN_STRING, &tk)) {
		parser_doc_token(pr, tk, dc);
		doc_alloc(DOC_LINE, dc);
	}

	/* cexpression */
	if (lexer_expect(lx, TOKEN_LPAREN, &tk)) {
		int error;

		parser_doc_token(pr, tk, dc);
		error = parser_expr(pr, &expr, &(struct parser_expr_arg){
		    .dc	= dc,
		});
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			parser_doc_token(pr, tk, expr);
		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			parser_doc_token(pr, comma, expr);
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
	struct token *assembly, *lparen, *rparen, *tk;
	int ninputs = 0;
	int error;

	if (!lexer_peek_if(lx, TOKEN_ASSEMBLY, NULL))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	if (lexer_expect(lx, TOKEN_ASSEMBLY, &assembly))
		parser_doc_token(pr, assembly, concat);

	while (lexer_if(lx, TOKEN_VOLATILE, &qualifier) ||
	    lexer_if(lx, TOKEN_INLINE, &qualifier) ||
	    lexer_if(lx, TOKEN_GOTO, &qualifier)) {
		doc_alloc(DOC_LINE, concat);
		parser_doc_token(pr, qualifier, concat);
	}
	if (qualifier != NULL && token_has_spaces(qualifier))
		doc_alloc(DOC_LINE, concat);

	opt = doc_indent(style(pr->pr_st, ContinuationIndentWidth),
	    doc_alloc(DOC_OPTIONAL, dc));
	if (lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &lparen,
	    &rparen))
		parser_token_trim_before(pr, rparen);
	if (lexer_expect(lx, TOKEN_LPAREN, NULL))
		parser_doc_token(pr, lparen, opt);

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
		parser_doc_token(pr, colon, concat);
		concat = doc_indent(2, concat);
		while (parser_stmt_asm_operand(pr, concat) & GOOD)
			ninputs++;
	}

	/* output operands */
	if (lexer_if(lx, TOKEN_COLON, &colon)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		if (ninputs > 0)
			doc_alloc(DOC_LINE, concat);
		parser_doc_token(pr, colon, concat);
		concat = doc_indent(2, concat);
		while (parser_stmt_asm_operand(pr, concat) & GOOD)
			continue;
	}

	/* clobbers */
	if (lexer_if(lx, TOKEN_COLON, &colon)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		parser_doc_token(pr, colon, concat);
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
		parser_doc_token(pr, colon, concat);
		if (!lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			doc_alloc(DOC_LINE, concat);
		error = parser_expr(pr, NULL, &(struct parser_expr_arg){
		    .dc	= concat,
		});
		if (error & FAIL)
			return parser_fail(pr);
	}

	if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
		parser_doc_token(pr, rparen, concat);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		parser_doc_token(pr, tk, concat);

	return parser_good(pr);
}
