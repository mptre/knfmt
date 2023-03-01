#include "parser-cpp.h"

#include "config.h"

#include <ctype.h>
#include <string.h>

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "token.h"

static int	iscdefs(const char *, size_t);

/*
 * Detect usage of types hidden behind cpp such as STACK_OF(X509).
 */
int
parser_cpp_peek_type(struct parser *pr)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, NULL)) {
		struct token *nx;

		if (lexer_if(lx, TOKEN_IDENT, &nx) &&
		    token_cmp(ident, nx) == 0)
			peek = 1;
		else if (lexer_if(lx, TOKEN_STAR, NULL))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Detect usage of preprocessor directives such as the ones provided by
 * queue(3).
 */
int
parser_cpp_peek_decl(struct parser *pr, struct token **end, unsigned int flags)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *macro, *tk;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	while (lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE,
	    NULL))
		continue;
	if (lexer_if(lx, TOKEN_IDENT, &macro) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, end)) {
		struct token *ident;

		for (;;) {
			if (!lexer_if(lx, TOKEN_STAR, &tk))
				break;
			*end = tk;
		}

		if (lexer_if(lx, TOKEN_EQUAL, NULL))
			peek = 1;
		else if ((flags & PARSER_CPP_DECL_ROOT) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		else if (lexer_if(lx, TOKEN_IDENT, &ident) &&
		    (token_cmp(macro, ident) == 0 ||
		     lexer_if(lx, TOKEN_SEMI, NULL)))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Detect usage of X macro. That is, something that looks like a function call
 * but is not followed by a semicolon nor comma if being part of an initializer.
 * One example are the macros provided by RBT_PROTOTYPE(9).
 */
int
parser_cpp_peek_x(struct parser *pr, struct token **tk)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *pv = NULL;
	struct token *ident, *rparen;
	int peek = 0;

	(void)lexer_back(lx, &pv);
	lexer_peek_enter(lx, &s);
	while (lexer_if_flags(lx, TOKEN_FLAG_STORAGE, NULL))
		continue;
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen)) {
		const struct token *nx;

		/*
		 * The previous token must not reside on the same line as the
		 * identifier. The next token must reside on the next line and
		 * have the same or less indentation. This is of importance in
		 * order to not confuse loop constructs hidden behind cpp.
		 */
		nx = token_next(rparen);
		if ((pv == NULL || token_cmp(pv, ident) < 0) &&
		    (nx == NULL || (token_cmp(nx, rparen) > 0 &&
		     nx->tk_cno <= ident->tk_cno)))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (peek && tk != NULL)
		*tk = rparen;
	return peek;
}

int
parser_cpp_x(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (!parser_cpp_peek_x(pr, NULL))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	while (lexer_if_flags(lx, TOKEN_FLAG_STORAGE, &tk)) {
		doc_token(tk, concat);
		doc_alloc(DOC_LINE, concat);
	}
	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		doc_token(tk, concat);
	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, concat);
	parser_expr(pr, NULL, &(struct parser_expr_arg){
	    .dc		= concat,
	    .rl		= rl,
	    .flags	= EXPR_EXEC_ALIGN,
	});
	if (lexer_expect(lx, TOKEN_RPAREN, &tk))
		doc_token(tk, concat);
	return parser_good(pr);
}

/*
 * Parse usage of macros from cdefs.h, such as __BEGIN_HIDDEN_DECLS.
 */
int
parser_cpp_cdefs(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident, *nx;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_pop(lx, &nx) &&
	    nx->tk_lno - ident->tk_lno >= 1 &&
	    iscdefs(ident->tk_str, ident->tk_len))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_IDENT, &ident))
		doc_token(ident, dc);
	return parser_good(pr);
}

int
parser_cpp_decl_root(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident, *semi;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_SEMI, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_IDENT, &ident))
		doc_token(ident, dc);
	if (lexer_expect(lx, TOKEN_SEMI, &semi))
		doc_token(semi, dc);
	return parser_good(pr);
}

static int
iscdefs(const char *str, size_t len)
{
	size_t i;

	if (len < 2 || strncmp(str, "__", 2) != 0)
		return 0;
	for (i = 2; i < len; i++) {
		unsigned char c = (unsigned char)str[i];

		if (!isupper(c) && !isdigit(c) && c != '_')
			return 0;
	}
	return 1;
}
