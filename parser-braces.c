#include "parser-braces.h"

#include "config.h"

#include "libks/arena.h"

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

struct braces_field_arg {
	struct doc		*parent;
	struct doc		*dc;
	struct ruler		*rl;
	struct token		*rbrace;
	unsigned int		 indent;
	unsigned int		 col;
	unsigned int		 flags;
};

struct lbrace_cache {
	int		 valid;
	struct token	*lbrace;
};

static int	parser_braces_with_ruler(struct parser *, struct doc *,
    struct doc *, struct ruler *, unsigned int, unsigned int);
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
parser_braces(struct parser *pr, struct doc *parent, struct doc *dc,
    unsigned int indent, unsigned int flags)
{
	struct ruler rl;
	int error;

	arena_scope(pr->pr_arena.ruler, ruler_scope);

	ruler_init(&rl, 0, RULER_ALIGN_SENSE, &ruler_scope);
	pr->pr_braces.depth++;
	error = parser_braces_with_ruler(pr, parent, dc, &rl, indent, flags);
	pr->pr_braces.depth--;
	ruler_exec(&rl);
	return error;
}

static int
is_first_token_on_line(const struct token *tk)
{
	const struct token *pv;

	pv = token_prev(tk);
	return pv != NULL && token_cmp(pv, tk) < 0;
}

/*
 * Returns non-zero if the first element inside the braces is aligned with the
 * first one on the subsequent line.
 */
static int
is_row_aligned(const struct token *lbrace, const struct token *rbrace)
{
	const struct token *first, *tk;

	if (is_first_token_on_line(lbrace))
		return 0;

	first = token_next(lbrace);
	for (tk = token_next(first); tk != rbrace; tk = token_next(tk)) {
		if (token_cmp(tk, first) > 0)
			return tk->tk_cno == first->tk_cno;
	}

	return 0;
}

static int
parser_braces_with_ruler(struct parser *pr, struct doc *parent, struct doc *dc,
    struct ruler *rl, unsigned int indent_width, unsigned int flags)
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

	braces = doc_alloc(DOC_CONCAT, dc);

	if (!lexer_expect(lx, TOKEN_LBRACE, NULL))
		return parser_fail(pr);
	/*
	 * If any column is followed by a hard line, do not align but
	 * instead respect existing hard line(s).
	 */
	align = token_cmp(lbrace, rbrace) == 0;
	if (flags & PARSER_BRACES_TRIM)
		parser_token_trim_after(pr, lbrace);
	parser_doc_token(pr, lbrace, braces);

	if (lexer_peek_if(lx, TOKEN_RBRACE, NULL)) {
		/* Honor spaces in empty braces. */
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);
		goto out;
	}

	if (token_has_line(lbrace, 1)) {
		unsigned int val = indent_width;

		if (flags & PARSER_BRACES_INDENT_MAYBE)
			val |= DOC_INDENT_NEWLINE;
		indent = doc_indent(val, braces);
		doc_alloc(DOC_HARDLINE, indent);
	} else if (is_row_aligned(lbrace, rbrace)) {
		unsigned int effective_indent;

		/*
		 * Rows are aligned using an indent equal to the position of the
		 * lbrace.
		 *
		 * 	int v[] = { 0x1, 0x2,
		 * 		    0x3, 0x4 };
		 */
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);
		effective_indent = parser_width(pr, parent) -
		    ((pr->pr_braces.depth - 1) * indent_width);
		indent = doc_indent(effective_indent, braces);
	} else if (!is_first_token_on_line(lbrace)) {
		/*
		 * The lbrace is not the first token on this line. Regular
		 * continuation indentation is wanted.
		 */
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);
		indent = doc_indent(indent_width, braces);
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

		if ((flags & PARSER_BRACES_ENUM) ||
		    lexer_peek_if(lx, TOKEN_PERIOD, NULL) ||
		    lexer_peek_if(lx, TOKEN_LSQUARE, NULL)) {
			struct braces_field_arg arg = {
				.parent	= parent,
				.dc	= concat,
				.rl	= rl,
				.rbrace	= rbrace,
				.indent	= indent_width,
				.col	= col,
				.flags	= flags,
			};
			error = parser_braces_field(pr, &arg);
			if (error & HALT)
				return parser_fail(pr);
			/*
			 * Inherit the column as we're still on the same row in
			 * terms of alignment.
			 */
			col = arg.col;
		} else if (lexer_peek_if(lx, TOKEN_LBRACE, &nx)) {
			error = parser_braces_with_ruler(pr, parent, concat, rl,
			    indent_width, flags & ~PARSER_BRACES_DEDENT);
			if (error & HALT)
				return parser_fail(pr);
			/*
			 * Inherit the column as we're still on the same row in
			 * terms of alignment.
			 */
			col = ruler_get_column_count(rl);
		} else {
			struct token *stop;

			stop = peek_expr_stop(pr, &lbrace_cache, rbrace);
			/* Delegate column alignment to the expression parser. */
			error = parser_expr(pr, &expr,
			    &(struct parser_expr_arg){
				.dc	= concat,
				.rl	= !align ? rl : NULL,
				.stop	= stop,
				.flags	= EXPR_EXEC_NOSOFT |
				    (!align ? EXPR_EXEC_ALIGN : 0),
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
				w += parser_width(pr, concat);
				ruler_insert(rl, comma, concat, ++col, w, 0);
				w = 0;
				goto next;
			} else if (token_has_line(comma, 1)) {
				/* Only relevant when aligning field
				 * initializers which are allowed span multiple
				 * lines. */
				col = 0;
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
				if (flags & PARSER_BRACES_DEDENT) {
					braces = doc_dedent(indent_width,
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
		if ((flags & PARSER_BRACES_ENUM) == 0 &&
		    lexer_back(lx, &nx) && token_has_line(nx, 2))
			ruler_exec(rl);
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
	    !lexer_peek_if(lx, TOKEN_PERIOD, NULL) &&
	    !lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		doc_literal(" ", braces);

	return parser_good(pr);
}

static int
parser_braces_field(struct parser *pr, struct braces_field_arg *arg)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *dc = arg->dc;
	struct ruler *rl = arg->rl;
	struct token *equal, *lbrace, *stop;
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

	if (!lexer_if(lx, TOKEN_EQUAL, &equal))
		return parser_good(pr);

	ruler_insert(rl, token_prev(equal), dc, ++arg->col,
	    parser_width(pr, dc), 0);
	parser_doc_token(pr, equal, dc);
	doc_literal(" ", dc);

	/* Do not go through expr recovery for nested braces. */
	if (lexer_peek_if(lx, TOKEN_LBRACE, &lbrace) &&
	    token_cmp(equal, lbrace) == 0) {
		unsigned int nested_flags = arg->flags & ~PARSER_BRACES_DEDENT;

		return parser_braces(pr, arg->parent, dc, arg->indent,
		    nested_flags);
	}

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
