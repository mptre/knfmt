#include "parser-cpp.h"

#include "config.h"

#include <string.h>

#include "libks/arena.h"
#include "libks/string.h"

#include "clang.h"
#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "parser-type.h"
#include "ruler.h"
#include "token.h"

static int	iscdefs(const char *, size_t);

static int
is_list_entry(const struct token *tk)
{
	return clang_token_type(tk) == CLANG_TOKEN_LIST_ENTRY;
}

int
parser_cpp_peek_type(struct parser *pr, struct token **rparen)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident;
	int peek = 0;

	/* Detect usage of types hidden behind cpp such as STACK_OF(X509). */
	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, rparen)) {
		struct token *nx;

		if (lexer_peek_if(lx, TOKEN_IDENT, &nx) &&
		    token_cmp(ident, nx) == 0)
			peek = 1;
		else if (lexer_peek_if(lx, TOKEN_STAR, NULL))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (peek)
		return 1;

	/* Detect LIST_ENTRY(list, struct s) from libks:list(3). */
	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) && is_list_entry(ident) &&
	    lexer_if(lx, TOKEN_LPAREN, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_COMMA, NULL) &&
	    lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_RPAREN, rparen) &&
	    lexer_if(lx, TOKEN_SEMI, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);

	return peek;
}

/*
 * Detect usage of preprocessor directives such as the ones provided by
 * queue(3).
 */
int
parser_cpp_peek_decl(struct parser *pr, struct parser_type *type,
    unsigned int flags)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *macro, *tk;
	int peek = 0;

	if (!lexer_peek(lx, &type->beg))
		return 0;

	lexer_peek_enter(lx, &s);
	while (lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE,
	    NULL))
		continue;
	if (lexer_if(lx, TOKEN_IDENT, &macro) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL, &type->end)) {
		struct token *ident;

		for (;;) {
			if (!lexer_if(lx, TOKEN_STAR, &tk))
				break;
			type->end = tk;
		}

		if (lexer_if(lx, TOKEN_EQUAL, NULL)) {
			peek = 1;
		} else if ((flags & PARSER_CPP_DECL_ROOT) &&
		    lexer_if(lx, TOKEN_SEMI, NULL)) {
			peek = 1;
		} else if (lexer_if(lx, TOKEN_IDENT, &ident)) {
			while (lexer_if(lx, TOKEN_IDENT, &ident))
				continue;
			if (token_cmp(macro, ident) == 0 ||
			    lexer_if(lx, TOKEN_SEMI, NULL))
				peek = 1;
		}
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
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL, &rparen)) {
		const struct token *nx;

		/*
		 * The previous token must not reside on the same line as the
		 * identifier. The next token must reside on the next line and
		 * have the same or less indentation, assuming the identifier is
		 * not positioned at the start of the line. This is of
		 * importance in order to not confuse loop constructs hidden
		 * behind cpp.
		 */
		nx = token_next(rparen);
		if ((pv == NULL || token_cmp(pv, ident) < 0) &&
		    (nx == NULL || (token_cmp(nx, rparen) > 0 &&
		     (ident->tk_cno == 1 || nx->tk_cno <= ident->tk_cno))))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (peek && tk != NULL)
		*tk = rparen;
	return peek;
}

int
parser_cpp_x(struct parser *pr, struct doc *dc)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *tk;
	int error;

	if (!parser_cpp_peek_x(pr, &rparen))
		return parser_none(pr);

	if (pr->pr_cpp.ruler == NULL) {
		pr->pr_cpp.ruler = arena_malloc(pr->pr_arena.scratch_scope,
		    sizeof(*pr->pr_cpp.ruler));
		ruler_init(pr->pr_cpp.ruler, 0, RULER_ALIGN_SENSE);
	}

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	while (lexer_if_flags(lx, TOKEN_FLAG_STORAGE, &tk)) {
		parser_doc_token(pr, tk, concat);
		doc_alloc(DOC_LINE, concat);
	}
	error = parser_expr(pr, NULL, &(struct parser_expr_arg){
	    .dc		= concat,
	    .rl		= pr->pr_cpp.ruler,
	    .stop	= token_next(rparen),
	    .flags	= EXPR_EXEC_ALIGN,
	});
	if (error & HALT)
		return parser_fail(pr);
	/* Compensate for the expression parser only honoring one new line. */
	if (token_has_line(rparen, 2))
		doc_alloc(DOC_HARDLINE, concat);

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
		parser_doc_token(pr, ident, dc);
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
		parser_doc_token(pr, ident, dc);
	if (lexer_expect(lx, TOKEN_SEMI, &semi))
		parser_doc_token(pr, semi, dc);
	return parser_good(pr);
}

void *
parser_cpp_decl_enter(struct parser *pr)
{
	struct ruler *cookie = pr->pr_cpp.ruler;

	/* Cope with nested declarations, use a dedicated ruler per scope. */
	pr->pr_cpp.ruler = NULL;
	return cookie;
}

void
parser_cpp_decl_leave(struct parser *pr, void *cookie)
{
	struct ruler *rl = pr->pr_cpp.ruler;

	pr->pr_cpp.ruler = cookie;
	if (rl == NULL)
		return;

	ruler_exec(rl);
	ruler_free(rl);
}

static int
iscdefs(const char *str, size_t len)
{
	struct suffix {
		const char	*str;
		size_t		 len;
	} suffixes[] = {
#define S(s) { s, sizeof(s) - 1 }
		S("_BEGIN_DECLS"),
		S("_END_DECLS"),
#undef S
	};
	size_t nsuffixes = sizeof(suffixes) / sizeof(suffixes[0]);
	size_t i;

	for (i = 0; i < nsuffixes; i++) {
		const struct suffix *s = &suffixes[i];

		if (len >= s->len &&
		    strncmp(&str[len - s->len], s->str, s->len) == 0)
			return 1;
	}

	if (len < 2 || strncmp(str, "__", 2) != 0)
		return 0;
	return KS_str_match(&str[2], len - 2, "AZ09__") == len - 2;
}
