#include "parser-braces.h"

#include "config.h"

#include "clang.h"
#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "parser-cpp.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "ruler.h"
#include "simple.h"
#include "token.h"

struct braces_arg {
	struct doc	*dc;
	struct ruler	*rl;
	unsigned int	 indent;
	unsigned int	 flags;
};

struct braces_field_arg {
	struct doc		*dc;
	struct ruler		*rl;
	struct token		*rbrace;
	unsigned int		 indent;
	unsigned int		 flags;
};

struct lbrace_cache {
	int		 valid;
	struct token	*lbrace;
};

static int	parser_braces1(struct parser *, struct braces_arg *);
static int	parser_braces_field(struct parser *, struct braces_field_arg *);
static int	parser_braces_field1(struct parser *,
    struct braces_field_arg *);

static struct token	*peek_expr_stop(struct parser *, struct lbrace_cache *,
    struct token *);
static struct token	*lbrace_cache_lookup(struct parser *,
    struct lbrace_cache *, struct token *);
static void		 insert_trailing_comma(struct parser *,
    struct token *);

int
parser_braces(struct parser *pr, struct doc *dc, unsigned int indent,
    unsigned int flags)
{
	struct ruler rl;
	int error;

	ruler_init(&rl, 0, RULER_ALIGN_SENSE);
	error = parser_braces1(pr, &(struct braces_arg){
	    .dc		= dc,
	    .rl		= &rl,
	    .indent	= indent,
	    .flags	= flags,
	});
	ruler_exec(&rl);
	ruler_free(&rl);
	return error;
}

static int
parser_braces1(struct parser *pr, struct braces_arg *arg)
{
	struct lbrace_cache lbrace_cache = {0};
	struct doc *braces, *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *pv, *rbrace, *tk;
	unsigned int col = 0;
	unsigned int w = 0;
	int align, error;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &lbrace,
	    &rbrace))
		return parser_fail(pr);

	braces = doc_alloc(DOC_CONCAT, arg->dc);

	if (!lexer_expect(lx, TOKEN_LBRACE, NULL))
		return parser_fail(pr);
	/*
	 * If any column is followed by a hard line, do not align but
	 * instead respect existing hard line(s).
	 */
	align = token_cmp(lbrace, rbrace) == 0;
	if (arg->flags & PARSER_BRACES_TRIM)
		parser_token_trim_after(pr, lbrace);
	parser_doc_token(pr, lbrace, braces);

	if (lexer_peek_if(lx, TOKEN_RBRACE, NULL)) {
		/* Honor spaces in empty braces. */
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);
		goto out;
	}

	if (token_has_line(lbrace, 1)) {
		unsigned int val = arg->indent;

		if (arg->flags & PARSER_BRACES_INDENT_MAYBE)
			val |= DOC_INDENT_NEWLINE;
		indent = doc_indent(val, braces);
		doc_alloc(DOC_HARDLINE, indent);
	} else if ((pv = token_prev(lbrace)) != NULL &&
	    token_cmp(pv, lbrace) == 0) {
		/*
		 * The lbrace is not the first token on this line. Regular
		 * continuation indentation is wanted.
		 */
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);
		indent = doc_indent(arg->indent, braces);
	} else {
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);

		/*
		 * Take note of the width of the document, must be accounted for
		 * while performing alignment.
		 */
		w = parser_width(pr, braces);
		indent = doc_indent(w, braces);
	}

	for (;;) {
		struct doc *expr = NULL;
		struct doc *concat;
		struct token *comma, *nx;

		if (!lexer_peek(lx, &tk) || tk->tk_type == LEXER_EOF)
			return parser_fail(pr);
		if (tk == rbrace)
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));

		if ((arg->flags & PARSER_BRACES_ENUM) ||
		    lexer_peek_if(lx, TOKEN_PERIOD, NULL) ||
		    lexer_peek_if(lx, TOKEN_LSQUARE, NULL)) {
			error = parser_braces_field(pr,
			    &(struct braces_field_arg){
				.dc	= concat,
				.rl	= arg->rl,
				.rbrace	= rbrace,
				.indent	= arg->indent,
				.flags	= arg->flags,
			});
			if (error & HALT)
				return parser_fail(pr);
		} else if (lexer_peek_if(lx, TOKEN_LBRACE, &nx)) {
			unsigned int rmflags = PARSER_BRACES_DEDENT;
			struct braces_arg newarg = {
				.dc	= concat,
				.rl	= arg->rl,
				.indent	= arg->indent,
				.flags	= arg->flags & ~rmflags,
			};

			if (parser_braces1(pr, &newarg) & HALT)
				return parser_fail(pr);
			/*
			 * Inherit the column as we're still on the same row in
			 * terms of alignment.
			 */
			if (align)
				col = ruler_get_column_count(arg->rl);
		} else {
			struct token *stop;

			stop = peek_expr_stop(pr, &lbrace_cache, rbrace);
			error = parser_expr(pr, &expr,
			    &(struct parser_expr_arg){
				.dc	= concat,
				.rl	= arg->rl,
				.stop	= stop,
				.flags	= EXPR_EXEC_ALIGN | EXPR_EXEC_NOSOFT,
			});
			if (error & HALT)
				return parser_fail(pr);
		}
		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			if (expr == NULL)
				expr = concat;
			if (lexer_peek_if(lx, TOKEN_RBRACE, &nx) &&
			    token_is_moveable(nx))
				token_trim(comma);
			parser_doc_token(pr, comma, expr);

			if (align) {
				col++;
				w += parser_width(pr, concat);
				ruler_insert(arg->rl, comma, concat, col, w, 0);
				w = 0;
				goto next;
			}
		}

		if (!lexer_back(lx, &pv) || !lexer_peek(lx, &nx)) {
			return parser_fail(pr);
		} else if (token_has_spaces(pv) && !token_has_line(pv, 1)) {
			/* Previous token already emitted, honor spaces. */
			parser_doc_token(pr, token_find_suffix_spaces(pv),
			    concat);
		} else if (pv->tk_type == TOKEN_COMMA &&
		    token_cmp(pv, nx) == 0) {
			/* Missing spaces after comma. */
			doc_alloc(DOC_LINE, indent);
		} else if (nx == rbrace) {
			/*
			 * Put the last hard line outside to get indentation
			 * right.
			 */
			if (token_cmp(pv, rbrace) < 0) {
				if (arg->flags & PARSER_BRACES_DEDENT) {
					braces = doc_dedent(arg->indent,
					    braces);
				}
				doc_alloc(DOC_HARDLINE, braces);
			}
		} else if (pv->tk_type == TOKEN_RPAREN &&
		    !token_has_line(pv, 1)) {
			/*
			 * Probably a cast followed by braces initializers, no
			 * hard line wanted.
			 */
		} else {
			doc_alloc(DOC_HARDLINE, indent);
		}

next:
		if (((arg->flags & PARSER_BRACES_ENUM) == 0) &&
		    lexer_back(lx, &nx) && token_has_line(nx, 2))
			ruler_exec(arg->rl);
	}

out:
	if (lexer_expect(lx, TOKEN_RBRACE, &rbrace)) {
		parser_token_trim_after(pr, rbrace);
		parser_doc_token(pr, rbrace, braces);
	}
	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL) &&
	    !lexer_peek_if(lx, TOKEN_COMMA, NULL) &&
	    !lexer_peek_if(lx, TOKEN_RBRACE, NULL) &&
	    !lexer_peek_if(lx, TOKEN_RPAREN, NULL) &&
	    !lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		doc_literal(" ", braces);

	return parser_good(pr);
}

static int
parser_braces_field(struct parser *pr, struct braces_field_arg *arg)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *dc = arg->dc;
	struct token *equal;
	int nfields = 0;
	int error;

	for (;;) {
		error = parser_braces_field1(pr, arg);
		if (error & NONE)
			break;
		if (error & HALT)
			return parser_fail(pr);
		nfields++;
	}
	if (nfields == 0)
		return parser_fail(pr);

	insert_trailing_comma(pr, arg->rbrace);

	if (lexer_if(lx, TOKEN_EQUAL, &equal)) {
		struct token *stop;

		ruler_insert(arg->rl, token_prev(equal), dc, 1,
		    parser_width(pr, dc), 0);

		parser_doc_token(pr, equal, dc);
		doc_literal(" ", dc);

		lexer_peek_until_comma(lx, arg->rbrace, &stop);
		error = parser_expr(pr, NULL, &(struct parser_expr_arg){
		    .dc		= dc,
		    .stop	= stop,
		    .indent	= arg->indent,
		    .flags	= token_has_line(equal, 1) ?
			EXPR_EXEC_HARDLINE : 0,
		});
		if (error & HALT)
			return parser_fail(pr);
	}

	return parser_good(pr);
}

static int
parser_braces_field1(struct parser *pr, struct braces_field_arg *arg)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *dc = arg->dc;
	struct token *tk;
	int error;

	if (lexer_if(lx, TOKEN_LSQUARE, &tk)) {
		struct doc *expr = NULL;

		parser_doc_token(pr, tk, dc);
		error = parser_expr(pr, &expr, &(struct parser_expr_arg){
		    .dc	= dc,
		});
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RSQUARE, &tk))
			parser_doc_token(pr, tk, expr);
		return parser_good(pr);
	} else if (lexer_if(lx, TOKEN_PERIOD, &tk)) {
		struct token *equal;

		parser_doc_token(pr, tk, dc);
		if (lexer_expect(lx, TOKEN_IDENT, &tk))
			parser_doc_token(pr, tk, dc);

		/* Correct alignment, must occur after the ident. */
		if (lexer_peek_if(lx, TOKEN_EQUAL, &equal) &&
		    token_has_tabs(equal))
			token_move_suffixes_if(equal, tk, TOKEN_SPACE);

		return parser_good(pr);
	} else if (lexer_if(lx, TOKEN_IDENT, &tk)) {
		parser_doc_token(pr, tk, dc);

		/* Enum making use of preprocessor directives. */
		if ((arg->flags & PARSER_BRACES_ENUM) &&
		    lexer_if(lx, TOKEN_LPAREN, &tk)) {
			struct doc *expr = NULL;

			parser_doc_token(pr, tk, dc);
			error = parser_expr(pr, &expr,
			    &(struct parser_expr_arg){
				.dc	= dc,
			});
			if (error & FAIL)
				return parser_fail(pr);
			if (error & HALT)
				expr = dc;
			if (lexer_expect(lx, TOKEN_RPAREN, &tk))
				parser_doc_token(pr, tk, expr);
		}

		return parser_good(pr);
	}

	return parser_none(pr);
}

static struct token *
peek_expr_stop(struct parser *pr, struct lbrace_cache *cache,
    struct token *rbrace)
{
	struct lexer *lx = pr->pr_lx;
	struct token *comma, *rparen, *stop;

	if (parser_cpp_peek_x(pr, &rparen))
		return token_next(rparen);

	/*
	 * Finding the next lbrace can be costly if the current pair of braces
	 * has many entries. Therefore utilize a tiny cache.
	 */
	stop = lbrace_cache_lookup(pr, cache, rbrace);
	if (lexer_peek_until_comma(lx, stop, &comma))
		return comma;
	return stop;
}

static struct token *
lbrace_cache_lookup(struct parser *pr, struct lbrace_cache *cache,
    struct token *fallback)
{
	if (!cache->valid) {
		lexer_peek_until(pr->pr_lx, TOKEN_LBRACE, &cache->lbrace);
		cache->valid = 1;
	}
	return cache->lbrace != NULL ? cache->lbrace : fallback;
}

static void
insert_trailing_comma(struct parser *pr, struct token *rbrace)
{
	struct token *comma, *pv;

	simple_cookie(simple);
	if (!simple_enter(pr->pr_si, SIMPLE_BRACES, 0, &simple))
		return;

	pv = token_prev(rbrace);
	if (pv->tk_type == TOKEN_COMMA ||
	    pv->tk_type == TOKEN_RPAREN ||
	    !token_has_line(pv, 1))
		return;

	comma = lexer_insert_after(pr->pr_lx, pv,
	    clang_keyword_token(TOKEN_COMMA));
	token_move_suffixes_if(pv, comma, TOKEN_SPACE);
}
