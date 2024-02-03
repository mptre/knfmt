#include "parser-stmt.h"

#include "config.h"

#include "libks/arena.h"

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "options.h"
#include "parser-decl.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "parser-stmt-asm.h"
#include "parser-stmt-expr.h"
#include "parser.h"
#include "simple-stmt.h"
#include "simple.h"
#include "style.h"
#include "token.h"

#define PARSER_STMT_EXPR_DOWHILE		0x00000001u

static int	parser_stmt(struct parser *, struct doc *);
static int	parser_stmt1(struct parser *, struct doc *);
static int	parser_stmt_if(struct parser *, struct doc *);
static int	parser_stmt_for(struct parser *, struct doc *);
static int	parser_stmt_dowhile(struct parser *, struct doc *);
static int	parser_stmt_kw_expr(struct parser *, struct doc *, int,
    unsigned int);
static int	parser_stmt_label(struct parser *, struct doc *);
static int	parser_stmt_case(struct parser *, struct doc *);
static int	parser_stmt_goto(struct parser *, struct doc *);
static int	parser_stmt_switch(struct parser *, struct doc *);
static int	parser_stmt_while(struct parser *, struct doc *);
static int	parser_stmt_break(struct parser *, struct doc *);
static int	parser_stmt_continue(struct parser *, struct doc *);
static int	parser_stmt_return(struct parser *, struct doc *);
static int	parser_stmt_semi(struct parser *, struct doc *);
static int	parser_stmt_cpp(struct parser *, struct doc *);

static int		 parser_simple_stmt_enter(struct parser *,
    struct simple_cookie *);
static struct doc	*parser_simple_stmt_no_braces_enter(struct parser *,
    struct doc *, void **);
static void		 parser_simple_stmt_no_braces_leave(struct parser *,
    void *);
static int		 is_simple_stmt(const struct token *);
static int		 peek_simple_stmt(struct parser *);

int
parser_stmt_peek(struct parser *pr)
{
	struct doc *dc;
	int error, simple;

	arena_scope(pr->pr_arena.doc, doc_scope);
	parser_arena_scope(&pr->pr_arena.doc_scope, &doc_scope);

	dc = doc_root(&doc_scope);
	simple = simple_disable(pr->pr_si);
	error = parser_stmt1(pr, dc);
	simple_enable(pr->pr_si, simple);
	return error & GOOD;
}

static int
parser_stmt(struct parser *pr, struct doc *dc)
{
	int error;

	simple_cookie(simple);
	if (peek_simple_stmt(pr)) {
		arena_scope(pr->pr_arena.scratch, scratch_scope);
		parser_arena_scope(&pr->pr_arena.scratch_scope, &scratch_scope);

		error = parser_simple_stmt_enter(pr, &simple);
		if (error & HALT)
			return error;
	}
	error = parser_stmt1(pr, dc);
	return error;
}

static int
parser_stmt1(struct parser *pr, struct doc *dc)
{
	struct parser_stmt_block_arg ps = {
		.head	= dc,
		.tail	= dc,
		.flags	= PARSER_STMT_BLOCK_TRIM,
	};

	/*
	 * Most likely statement comes first with some crucial exceptions:
	 *
	 *     1. Detect blocks before expressions as the expression parser can
	 *        also detect blocks through recovery using the parser.
	 *
	 *     2. Detect expressions before declarations as functions calls
	 *        would otherwise be treated as declarations. This in turn is
	 *        caused by parser_decl() being able to detect declarations
	 *        making use of preprocessor directives such as the ones
	 *        provided by queue(3).
	 */
	if ((parser_stmt_block(pr, &ps) & GOOD) ||
	    (parser_stmt_expr(pr, dc) & GOOD) ||
	    (parser_stmt_if(pr, dc) & GOOD) ||
	    (parser_stmt_return(pr, dc) & GOOD) ||
	    (parser_decl(pr, dc,
	     PARSER_DECL_BREAK | PARSER_DECL_SIMPLE |
	     PARSER_DECL_TRIM_SEMI) & GOOD) ||
	    (parser_stmt_case(pr, dc) & GOOD) ||
	    (parser_stmt_break(pr, dc) & GOOD) ||
	    (parser_stmt_goto(pr, dc) & GOOD) ||
	    (parser_stmt_for(pr, dc) & GOOD) ||
	    (parser_stmt_while(pr, dc) & GOOD) ||
	    (parser_stmt_label(pr, dc) & GOOD) ||
	    (parser_stmt_switch(pr, dc) & GOOD) ||
	    (parser_stmt_continue(pr, dc) & GOOD) ||
	    (parser_stmt_asm(pr, dc) & GOOD) ||
	    (parser_stmt_dowhile(pr, dc) & GOOD) ||
	    (parser_stmt_semi(pr, dc) & GOOD) ||
	    (parser_stmt_cpp(pr, dc) & GOOD))
		return parser_good(pr);
	return parser_none(pr);
}

/*
 * Parse a block statement wrapped in braces.
 */
int
parser_stmt_block(struct parser *pr, struct parser_stmt_block_arg *arg)
{
	struct doc *dc = arg->tail;
	struct doc *concat, *indent, *line;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *nx, *rbrace, *tk;
	int isswitch = arg->flags & PARSER_STMT_BLOCK_SWITCH;
	int doindent = !isswitch && !is_simple_enabled(pr->pr_si, SIMPLE_STMT);
	int nstmt = 0;
	int error;

	if (!lexer_peek_if(lx, TOKEN_LBRACE, &lbrace) ||
	    !lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return parser_none(pr);

	/*
	 * Remove semi before emitting the right brace in order to honor
	 * optional lines.
	 */
	nx = token_next(rbrace);
	if (nx != NULL && nx->tk_type == TOKEN_SEMI) {
		simple_cookie(simple);
		if (simple_enter(pr->pr_si, SIMPLE_STMT_SEMI, 0, &simple))
			lexer_remove(lx, nx, 1);
	}

	if (doindent)
		pr->pr_nindent++;

	if ((arg->flags & PARSER_STMT_BLOCK_EXPR_GNU) == 0 &&
	    is_simple_enabled(pr->pr_si, SIMPLE_STMT)) {
		dc = simple_stmt_braces_enter(pr->pr_simple.stmt, lbrace,
		    rbrace, pr->pr_nindent * style(pr->pr_st, IndentWidth));
	}

	parser_token_trim_before(pr, rbrace);

	if (!lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		return parser_fail(pr);
	/*
	 * Optionally remove empty lines after the opening left brace.
	 * An empty line is however allowed in the beginning of a
	 * function implementation, a convention used when the function lacks
	 * local variables. But discard it if the following line is a
	 * declaration.
	 */
	if ((arg->flags & PARSER_STMT_BLOCK_TRIM) ||
	    (token_has_line(lbrace, 2) && parser_decl_peek(pr)))
		parser_token_trim_after(pr, lbrace);
	parser_doc_token(pr, lbrace, arg->head);

	if (isswitch)
		indent = dc;
	else
		indent = doc_indent(style(pr->pr_st, IndentWidth), dc);
	if ((arg->flags & PARSER_STMT_BLOCK_EXPR_GNU) == 0)
		line = doc_alloc(DOC_HARDLINE, indent);
	else
		line = doc_literal(" ", indent);
	while ((error = parser_stmt(pr, indent)) & GOOD) {
		nstmt++;
		if (lexer_peek(lx, &tk) && tk == rbrace)
			break;
		doc_alloc(DOC_HARDLINE, indent);
	}
	/* Do not keep the hard line if the statement block is empty. */
	if (nstmt == 0 && (error & BRCH) == 0)
		doc_remove(line, indent);

	if ((arg->flags & PARSER_STMT_BLOCK_EXPR_GNU) == 0)
		doc_alloc(DOC_HARDLINE, arg->tail);
	else
		doc_literal(" ", arg->tail);

	/*
	 * The right brace and any following statement is expected to fit on a
	 * single line.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, arg->tail));
	if (lexer_expect(lx, TOKEN_RBRACE, &tk)) {
		if (lexer_peek_if(lx, TOKEN_ELSE, NULL))
			parser_token_trim_after(pr, tk);
		parser_doc_token(pr, tk, concat);
	}
	if (lexer_if(lx, TOKEN_SEMI, &tk))
		parser_doc_token(pr, tk, concat);
	arg->rbrace = concat;

	if (doindent)
		pr->pr_nindent--;

	return parser_good(pr);
}

static int
parser_stmt_if(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk, *tkelse, *tkif;

	if (!lexer_peek_if(lx, TOKEN_IF, NULL))
		return parser_none(pr);

	if (parser_stmt_kw_expr(pr, dc, TOKEN_IF, 0) & (FAIL | NONE))
		return parser_fail(pr);

	while (lexer_peek_if(lx, TOKEN_ELSE, &tkelse)) {
		int error;

		if (lexer_back_if(lx, TOKEN_RBRACE, NULL))
			doc_literal(" ", dc);
		else
			doc_alloc(DOC_HARDLINE, dc);
		if (!lexer_expect(lx, TOKEN_ELSE, &tk))
			break;
		parser_doc_token(pr, tk, dc);
		doc_literal(" ", dc);

		if (lexer_peek_if(lx, TOKEN_IF, &tkif) &&
		    token_cmp(tkelse, tkif) == 0) {
			error = parser_stmt_kw_expr(pr, dc, TOKEN_IF, 0);
			if (error & (FAIL | NONE))
				return parser_fail(pr);
		} else {
			if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
				error = parser_stmt(pr, dc);
				if (error & FAIL)
					return parser_fail(pr);
			} else {
				void *simple = NULL;

				dc = doc_indent(style(pr->pr_st, IndentWidth),
				    dc);
				doc_alloc(DOC_HARDLINE, dc);

				dc = parser_simple_stmt_no_braces_enter(pr, dc,
				    &simple);
				error = parser_stmt(pr, dc);
				parser_simple_stmt_no_braces_leave(pr, simple);
				if (error & (FAIL | NONE))
					return parser_fail(pr);
			}

			/* Terminate if/else chain. */
			break;
		}
	}

	return parser_good(pr);
}

static int
parser_stmt_for(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *expr = NULL;
	struct doc *space = NULL;
	struct doc *loop;
	struct token *semi = NULL;
	struct token *tk;
	unsigned int w;
	int error;

	if (!lexer_if(lx, TOKEN_FOR, &tk))
		return parser_none(pr);

	loop = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	parser_doc_token(pr, tk, loop);
	doc_literal(" ", loop);

	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		parser_doc_token(pr, tk, loop);

	if (style(pr->pr_st, AlignOperands) == Align)
		w = parser_width(pr, dc);
	else
		w = style(pr->pr_st, ContinuationIndentWidth);

	/* Declarations are allowed in the first expression. */
	if (parser_decl(pr, loop, 0) & NONE) {
		error = parser_expr(pr, &expr, &(struct parser_expr_arg){
		    .dc		= loop,
		    .indent	= w,
		});
		if (error & (FAIL | BRCH))
			return parser_fail(pr);
		/* Let the semicolon hang of the expression unless empty. */
		if (error & NONE)
			expr = loop;
		if (lexer_expect(lx, TOKEN_SEMI, &semi))
			parser_doc_token(pr, semi, expr);
	} else {
		expr = loop;
	}
	space = lexer_back(lx, &semi) && !token_has_line(semi, 1) ?
	    doc_literal(" ", expr) : NULL;

	/* If the expression does not fit, break after the semicolon. */
	error = parser_expr(pr, &expr, &(struct parser_expr_arg){
	    .dc		= loop,
	    .indent	= w,
	    .flags	= EXPR_EXEC_SOFTLINE,
	});
	if (error & (FAIL | BRCH))
		return parser_fail(pr);
	if (error & NONE) {
		/* Expression empty, remove the space. */
		if (space != NULL && (semi == NULL || !token_has_spaces(semi)))
			doc_remove(space, expr);
		expr = loop;
	}
	space = NULL;
	if (lexer_expect(lx, TOKEN_SEMI, &semi)) {
		parser_doc_token(pr, semi, expr);
		if (!token_has_line(semi, 1))
			space = doc_literal(" ", expr);
	}

	/* If the expression does not fit, break after the semicolon. */
	error = parser_expr(pr, &expr, &(struct parser_expr_arg){
	    .dc		= loop,
	    .indent	= w,
	    .flags	= EXPR_EXEC_SOFTLINE,
	});
	if (error & (FAIL | BRCH))
		return parser_fail(pr);
	if (error & NONE) {
		/* Expression empty, remove the space. */
		if (space != NULL)
			doc_remove(space, expr);
		expr = loop;
	}
	if (lexer_expect(lx, TOKEN_RPAREN, &tk)) {
		parser_token_trim_after(pr, tk);
		parser_doc_token(pr, tk, expr);
	}

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_literal(" ", expr);
	} else {
		dc = doc_indent(style(pr->pr_st, IndentWidth), dc);
		doc_alloc(DOC_HARDLINE, dc);
	}
	return parser_stmt(pr, dc);
}

static int
parser_stmt_dowhile(struct parser *pr, struct doc *dc)
{
	struct parser_stmt_block_arg ps = {
		.head	= dc,
		.tail	= dc,
		.flags	= PARSER_STMT_BLOCK_TRIM,
	};
	struct lexer *lx = pr->pr_lx;
	struct doc *concat = dc;
	struct token *tk;
	int error;

	if (!lexer_if(lx, TOKEN_DO, &tk))
		return parser_none(pr);

	parser_doc_token(pr, tk, concat);
	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_literal(" ", concat);
		error = parser_stmt_block(pr, &ps);
		/*
		 * The following while statement is intended to fit on the same
		 * line as the right brace.
		 */
		concat = ps.rbrace;
		doc_literal(" ", concat);
	} else {
		struct doc *indent;

		indent = doc_indent(style(pr->pr_st, IndentWidth), concat);
		doc_alloc(DOC_HARDLINE, indent);
		error = parser_stmt(pr, indent);
		doc_alloc(DOC_HARDLINE, concat);
	}
	if (error & HALT)
		return parser_fail(pr);

	if (lexer_peek_if(lx, TOKEN_WHILE, NULL)) {
		return parser_stmt_kw_expr(pr, concat, TOKEN_WHILE,
		    PARSER_STMT_EXPR_DOWHILE);
	}
	return parser_fail(pr);
}

/*
 * Parse a statement consisting of a keyword, expression wrapped in parenthesis
 * and followed by additional nested statement(s).
 */
static int
parser_stmt_kw_expr(struct parser *pr, struct doc *dc, int token_type,
    unsigned int flags)
{
	struct doc *expr = NULL;
	struct doc *stmt;
	struct lexer *lx = pr->pr_lx;
	struct token *kw, *lparen, *rparen, *tk;
	unsigned int w;
	int error;

	if (!lexer_expect(lx, token_type, &kw) ||
	    !lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_fail(pr);
	parser_token_trim_before(pr, rparen);
	parser_token_trim_after(pr, rparen);

	stmt = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	parser_doc_token(pr, kw, stmt);
	if (token_type != TOKEN_IDENT)
		doc_literal(" ", stmt);

	if (style(pr->pr_st, BasedOnStyle) == OpenBSD) {
		w = 0;
	} else {
		/*
		 * Usage of non-default style in which clang-format indentation
		 * is assumed causing the statement expression to be aligned
		 * with the left parenthesis.
		 *
		 * Note, must take note of the width before emitting the left
		 * parenthesis as it could be followed by comments which should
		 * not affect alignment.
		 */
		w = parser_width(pr, dc) + 1;
	}

	if (lexer_expect(lx, TOKEN_LPAREN, &lparen)) {
		struct doc *optional = stmt;

		if (token_has_suffix(lparen, TOKEN_COMMENT)) {
			optional = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_OPTIONAL, stmt));
		}
		parser_doc_token(pr, lparen, optional);
	}

	/*
	 * The tokens after the expression must be nested underneath the same
	 * expression since we want to fit everything until the following
	 * statement on a single line.
	 */
	error = parser_expr(pr, &expr, &(struct parser_expr_arg){
	    .dc		= stmt,
	    .stop	= rparen,
	    .indent	= style(pr->pr_st, ContinuationIndentWidth),
	    .align	= w,
	});
	if (error & (FAIL | BRCH))
		return parser_fail(pr);
	if (error & NONE)
		expr = stmt;
	if (lexer_expect(lx, TOKEN_RPAREN, &rparen)) {
		struct token *lbrace;

		/* Move suffixes if the left brace is about to move. */
		if (lexer_peek_if(lx, TOKEN_LBRACE, &lbrace) &&
		    token_cmp(rparen, lbrace) < 0)
			token_move_suffixes(rparen, lbrace);
		parser_doc_token(pr, rparen, expr);
	}

	if (flags & PARSER_STMT_EXPR_DOWHILE) {
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			parser_doc_token(pr, tk, expr);
		return parser_good(pr);
	}

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		struct parser_stmt_block_arg ps = {
			.head	= expr,
			.tail	= dc,
			.flags	= PARSER_STMT_BLOCK_TRIM,
		};

		if (token_type == TOKEN_SWITCH)
			ps.flags |= PARSER_STMT_BLOCK_SWITCH;
		doc_literal(" ", expr);
		return parser_stmt_block(pr, &ps);
	} else {
		struct doc *indent;
		void *simple = NULL;

		indent = doc_indent(style(pr->pr_st, IndentWidth), dc);
		doc_alloc(DOC_HARDLINE, indent);
		if (is_simple_stmt(kw)) {
			indent = parser_simple_stmt_no_braces_enter(pr, indent,
			    &simple);
		}
		error = parser_stmt(pr, indent);
		if (is_simple_stmt(kw))
			parser_simple_stmt_no_braces_leave(pr, simple);
		return error;
	}
}

static int
parser_stmt_label(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct doc *noindent;
	struct lexer *lx = pr->pr_lx;
	struct token *colon = NULL;
	struct token *ident;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if(lx, TOKEN_COLON, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	noindent = doc_alloc(DOC_CONCAT, doc_alloc(DOC_NOINDENT, dc));
	if (lexer_expect(lx, TOKEN_IDENT, &ident)) {
		struct doc *label;

		label = parser_doc_token(pr, ident, noindent);
		/*
		 * Honor indentation before label but make sure to emit it right
		 * before the label. Necessary when the label is prefixed with
		 * comment(s).
		 */
		if (token_has_indent(ident)) {
			doc_move_before(doc_literal(" ", noindent), label,
			    noindent);
		}
	}

	if (lexer_expect(lx, TOKEN_COLON, &colon)) {
		struct token *nx;

		parser_doc_token(pr, colon, noindent);

		/*
		 * A label is not necessarily followed by a hard line, there
		 * could be another statement on the same line.
		 */
		if (lexer_peek(lx, &nx) && token_cmp(colon, nx) == 0) {
			struct doc *indent;

			indent = doc_indent(DOC_INDENT_FORCE, dc);
			return parser_stmt(pr, indent);
		}
	}

	return parser_good(pr);
}

static int
parser_stmt_case(struct parser *pr, struct doc *dc)
{
	struct doc *indent, *lhs;
	struct lexer *lx = pr->pr_lx;
	struct token *kw, *tk;

	if (!lexer_if(lx, TOKEN_CASE, &kw) && !lexer_if(lx, TOKEN_DEFAULT, &kw))
		return parser_none(pr);

	lhs = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	parser_doc_token(pr, kw, lhs);
	if (!lexer_peek_until(lx, TOKEN_COLON, NULL))
		return parser_fail(pr);
	if (kw->tk_type == TOKEN_CASE) {
		int error;

		doc_alloc(DOC_LINE, lhs);
		error = parser_expr(pr, NULL, &(struct parser_expr_arg){
		    .dc	= lhs,
		});
		if (error & HALT)
			return parser_fail(pr);
	}
	if (!lexer_expect(lx, TOKEN_COLON, &tk))
		return parser_fail(pr);
	parser_token_trim_after(pr, tk);
	parser_doc_token(pr, tk, lhs);

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_alloc(DOC_LINE, lhs);
		if (parser_stmt(pr, dc) & FAIL)
			return parser_fail(pr);
	}

	indent = doc_indent(style(pr->pr_st, IndentWidth), dc);
	for (;;) {
		struct doc *line;
		struct token *nx;

		if (lexer_peek_if(lx, TOKEN_CASE, NULL) ||
		    lexer_peek_if(lx, TOKEN_DEFAULT, NULL))
			break;

		if (!lexer_peek(lx, &nx))
			return parser_fail(pr);

		/*
		 * Allow following statement(s) to be placed on the same line as
		 * the case/default keyword.
		 */
		if (token_cmp(kw, nx) == 0)
			line = doc_literal(" ", indent);
		else
			line = doc_alloc(DOC_HARDLINE, indent);

		if (parser_stmt(pr, indent) & HALT) {
			/* No statement, remove the line. */
			doc_remove(line, indent);
			break;
		}
	}

	return parser_good(pr);
}

static int
parser_stmt_goto(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat;
	struct token *tk;

	if (!lexer_if(lx, TOKEN_GOTO, &tk))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	parser_doc_token(pr, tk, concat);
	doc_alloc(DOC_LINE, concat);
	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		parser_doc_token(pr, tk, concat);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		parser_doc_token(pr, tk, concat);
	return parser_good(pr);
}

static int
parser_stmt_switch(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;

	if (!lexer_peek_if(lx, TOKEN_SWITCH, NULL))
		return parser_none(pr);
	return parser_stmt_kw_expr(pr, dc, TOKEN_SWITCH, 0);
}

static int
parser_stmt_while(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;

	if (!lexer_peek_if(lx, TOKEN_WHILE, NULL))
		return parser_none(pr);
	return parser_stmt_kw_expr(pr, dc, TOKEN_WHILE, 0);
}

static int
parser_stmt_break(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (!lexer_peek_if(lx, TOKEN_BREAK, NULL))
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_BREAK, &tk))
		parser_doc_token(pr, tk, dc);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		parser_doc_token(pr, tk, dc);
	return parser_good(pr);
}

static int
parser_stmt_continue(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (!lexer_peek_if(lx, TOKEN_CONTINUE, &tk))
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_CONTINUE, &tk))
		parser_doc_token(pr, tk, dc);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		parser_doc_token(pr, tk, dc);
	return parser_good(pr);
}

static int
parser_stmt_return(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat, *expr;
	struct token *tk;

	if (!lexer_if(lx, TOKEN_RETURN, &tk))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	parser_token_trim_after(pr, tk);
	parser_doc_token(pr, tk, concat);
	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL)) {
		int error;

		doc_literal(" ", concat);
		error = parser_expr(pr, &expr, &(struct parser_expr_arg){
		    .dc		= concat,
		    .indent	= style(pr->pr_st, ContinuationIndentWidth),
		});
		if (error & HALT)
			return parser_fail(pr);
	} else {
		expr = concat;
	}
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		parser_doc_token(pr, tk, expr);
	return parser_good(pr);
}

static int
parser_stmt_semi(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *semi;

	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_SEMI, &semi))
		parser_doc_token(pr, semi, dc);
	return parser_good(pr);
}

/*
 * Parse statement hidden behind cpp, such as a loop construct from queue(3).
 */
static int
parser_stmt_cpp(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL) &&
	    !lexer_if(lx, TOKEN_SEMI, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);
	return parser_stmt_kw_expr(pr, dc, TOKEN_IDENT, 0);
}

/*
 * Called while entering a section of the source code with one or many
 * statements potentially wrapped in curly braces ahead. The statements
 * will silently be formatted in order to determine if each statement fits on a
 * single line, making the curly braces redundant and thus removed. Otherwise,
 * curly braces will be added around all covered statements for consistency.
 * Once this routine returns, parsing continues as usual.
 */
static int
parser_simple_stmt_enter(struct parser *pr, struct simple_cookie *simple)
{
	struct lexer_state s;
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	int error;

	if (!simple_enter(pr->pr_si, SIMPLE_STMT, 0, simple))
		return parser_good(pr);

	arena_scope(pr->pr_arena.doc, doc_scope);
	parser_arena_scope(&pr->pr_arena.doc_scope, &doc_scope);

	pr->pr_simple.stmt = simple_stmt_enter(lx, pr->pr_st,
	    pr->pr_arena.scratch_scope, &doc_scope, pr->pr_arena.scratch,
	    pr->pr_op);
	dc = doc_root(&doc_scope);
	lexer_peek_enter(lx, &s);
	error = parser_stmt1(pr, dc);
	lexer_peek_leave(lx, &s);
	if (error & GOOD)
		simple_stmt_leave(pr->pr_simple.stmt);
	simple_stmt_free(pr->pr_simple.stmt);
	pr->pr_simple.stmt = NULL;
	simple_leave(simple);

	return error;
}

static struct doc *
parser_simple_stmt_no_braces_enter(struct parser *pr, struct doc *dc,
    void **cookie)
{
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace;

	if (!is_simple_enabled(pr->pr_si, SIMPLE_STMT) ||
	    !lexer_peek(lx, &lbrace))
		return dc;
	return simple_stmt_no_braces_enter(pr->pr_simple.stmt, lbrace,
	    (pr->pr_nindent + 1) * style(pr->pr_st, IndentWidth), cookie);
}

static void
parser_simple_stmt_no_braces_leave(struct parser *pr, void *cookie)
{
	struct lexer *lx = pr->pr_lx;
	struct token *rbrace;

	if (cookie == NULL || !lexer_peek(lx, &rbrace))
		return;
	simple_stmt_no_braces_leave(pr->pr_simple.stmt, rbrace, cookie);
}

static int
is_simple_stmt(const struct token *tk)
{
	switch (tk->tk_type) {
	case TOKEN_IF:
	case TOKEN_FOR:
	case TOKEN_WHILE:
	case TOKEN_IDENT:
		return 1;
	default:
		return 0;
	}
}

static int
peek_simple_stmt(struct parser *pr)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	int peek = 0;

	if (!pr->pr_op->simple)
		return 0;

	lexer_peek_enter(lx, &s);
	if (lexer_pop(lx, &tk) && is_simple_stmt(tk) &&
	    lexer_if(lx, TOKEN_LPAREN, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);

	return peek;
}
