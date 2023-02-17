#include "parser-decl.h"

#include "config.h"

#include "doc.h"
#include "expr.h"
#include "lexer.h"
#include "options.h"
#include "parser-attributes.h"
#include "parser-braces.h"
#include "parser-cpp.h"
#include "parser-expr.h"
#include "parser-func.h"
#include "parser-priv.h"
#include "parser-type.h"
#include "ruler.h"
#include "simple-decl.h"
#include "style.h"
#include "token.h"

struct parser_decl_init_arg {
	struct doc		*dc;
	struct doc		*out;
	struct ruler		*rl;
	const struct token	*semi;
	unsigned int		 indent;
	unsigned int		 flags;
/* Insert space before assignment operator. */
#define PARSER_DECL_INIT_ASSIGN		0x00000001u
};

static int	parser_decl1(struct parser *, struct doc *, unsigned int);
static int	parser_decl2(struct parser *, struct doc *, struct ruler *,
    unsigned int);
static int	parser_decl_init(struct parser *,
    struct parser_decl_init_arg *);
static int	parser_decl_init1(struct parser *, struct doc *,
    struct parser_decl_init_arg *);
static int	parser_decl_init_assign(struct parser *, struct doc *,
    struct parser_decl_init_arg *);
static int	parser_decl_bitfield(struct parser *, struct doc *);
static int	parser_decl_cpp(struct parser *, struct doc *, struct ruler *,
    unsigned int);
static int	parser_decl_cppx(struct parser *, struct doc *, struct ruler *);

static int	parser_simple_decl_active(const struct parser *);
static int	parser_simple_decl_enter(struct parser *, unsigned int);
static void	parser_simple_decl_leave(struct parser *, int);

int
parser_decl_peek(struct parser *pr)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct doc *dc;
	int error;

	dc = doc_alloc(DOC_CONCAT, NULL);
	lexer_peek_enter(lx, &s);
	error = parser_decl(pr, dc, 0);
	lexer_peek_leave(lx, &s);
	doc_free(dc);
	return error & GOOD;
}

int
parser_decl(struct parser *pr, struct doc *dc, unsigned int flags)
{
	int error, simple;

	simple = parser_simple_decl_enter(pr, flags);
	error = parser_decl1(pr, dc, flags);
	parser_simple_decl_leave(pr, simple);
	return error;
}

static int
parser_decl1(struct parser *pr, struct doc *dc, unsigned int flags)
{
	struct ruler rl;
	struct doc *line = NULL;
	struct doc *decl;
	struct lexer *lx = pr->pr_lx;
	int ndecl = 0;
	int error;

	decl = doc_alloc(DOC_CONCAT, dc);
	ruler_init(&rl, 0, RULER_ALIGN_TABS | RULER_REQUIRE_TABS);

	for (;;) {
		struct token *tk;

		error = parser_decl2(pr, decl, &rl, flags);
		if (error & (FAIL | NONE)) {
			if (line != NULL)
				doc_remove(line, decl);
			break;
		}
		ndecl++;
		if (error & BRCH)
			break;

		line = doc_alloc(DOC_HARDLINE, decl);

		if (flags & PARSER_DECL_BREAK) {
			/*
			 * Honor empty line(s) which denotes the end of this
			 * block of declarations.
			 */
			if (lexer_back(lx, &tk) && token_has_line(tk, 2))
				break;

			/*
			 * Any cpp directive also denotes the end of this block
			 * of declarations.
			 */
			if (lexer_peek_if_prefix_flags(lx, TOKEN_FLAG_CPP,
			    NULL)) {
				doc_remove(line, decl);
				break;
			}
		}
	}
	if (ndecl == 0)
		doc_remove(decl, dc);
	else if ((error & FAIL) == 0)
		ruler_exec(&rl);
	ruler_free(&rl);

	if (ndecl == 0)
		return parser_none(pr);

	if (flags & PARSER_DECL_LINE)
		doc_alloc(DOC_HARDLINE, decl);
	return parser_good(pr);
}

static int
parser_decl2(struct parser *pr, struct doc *dc, struct ruler *rl,
    unsigned int flags)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat;
	struct token *beg, *end, *fun, *semi, *tk;
	int error;

	if (parser_cpp_cdefs(pr, dc) & GOOD)
		return parser_good(pr);

	if (!parser_type_peek(pr, &end, 0)) {
		/* No type found, this declaration could make use of cpp. */
		return parser_decl_cpp(pr, dc, rl, flags);
	}

	/*
	 * Presence of a type does not necessarily imply that this a declaration
	 * since it could be a function declaration or implementation.
	 */
	switch (parser_func_peek(pr, &fun)) {
	case PARSER_FUNC_PEEK_NONE:
		break;
	case PARSER_FUNC_PEEK_DECL:
		return parser_func_decl(pr, dc, rl, fun);
	case PARSER_FUNC_PEEK_IMPL:
		return parser_none(pr);
	}

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	if (!lexer_peek(lx, &beg))
		return parser_fail(pr);
	if (parser_simple_decl_active(pr))
		simple_decl_type(pr->pr_simple.decl, beg, end);
	if (parser_type(pr, concat, end, rl) & (FAIL | NONE))
		return parser_fail(pr);

	/* Presence of semicolon implies that this declaration is done. */
	if (lexer_peek_if(lx, TOKEN_SEMI, NULL))
		goto out;

	if (token_is_decl(end, TOKEN_STRUCT) ||
	    token_is_decl(end, TOKEN_UNION)) {
		struct doc *indent;
		struct token *lbrace, *rbrace;

		if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE,
		    &rbrace))
			return parser_fail(pr);
		parser_token_trim_before(pr, rbrace);
		if (lexer_expect(lx, TOKEN_LBRACE, &lbrace)) {
			parser_token_trim_after(pr, lbrace);
			if ((style_brace_wrapping(pr->pr_st, AfterStruct) &&
			    token_is_decl(end, TOKEN_STRUCT)) ||
			    (style_brace_wrapping(pr->pr_st, AfterUnion) &&
			     token_is_decl(end, TOKEN_UNION)))
				doc_alloc(DOC_HARDLINE, concat);
			doc_token(lbrace, concat);
		}

		indent = doc_alloc_indent(style(pr->pr_st, IndentWidth),
		    concat);
		doc_alloc(DOC_HARDLINE, indent);
		if (parser_decl(pr, indent, 0) & FAIL)
			return parser_fail(pr);
		doc_alloc(DOC_HARDLINE, concat);

		if (lexer_expect(lx, TOKEN_RBRACE, &tk))
			doc_token(tk, concat);

		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL) &&
		    !lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
			doc_literal(" ", concat);
	} else if (token_is_decl(end, TOKEN_ENUM)) {
		struct token *lbrace, *rbrace;
		unsigned int w;

		if (!lexer_peek_if(lx, TOKEN_LBRACE, &lbrace) ||
		    !lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE,
		    &rbrace))
			return parser_fail(pr);
		parser_token_trim_before(pr, rbrace);
		if (style_brace_wrapping(pr->pr_st, AfterEnum))
			doc_alloc(DOC_HARDLINE, concat);

		w = style(pr->pr_st, IndentWidth);
		error = parser_braces(pr, concat, w,
		    PARSER_DECL_BRACES_ENUM | PARSER_DECL_BRACES_TRIM);
		if (error & HALT)
			return parser_fail(pr);
	}

	if (!lexer_peek_until(lx, TOKEN_SEMI, &semi))
		return parser_fail(pr);

	struct parser_decl_init_arg arg = {
		.dc	= concat,
		.rl	= rl,
		.semi	= semi,
		.indent	= style(pr->pr_st, IndentWidth),
		.flags	= PARSER_DECL_INIT_ASSIGN,
	};
	error = parser_decl_init(pr, &arg);
	if (error & (FAIL | NONE))
		return parser_fail(pr);
	if (arg.out != NULL)
		concat = arg.out;

out:
	if (lexer_expect(lx, TOKEN_SEMI, &semi)) {
		doc_token(semi, concat);
		if (parser_simple_decl_active(pr))
			simple_decl_semi(pr->pr_simple.decl, semi);
	}
	return parser_good(pr);
}

/*
 * Parse any initialization as part of a declaration.
 */
static int
parser_decl_init(struct parser *pr, struct parser_decl_init_arg *arg)
{
	struct doc *concat, *dc, *indent;
	struct lexer *lx = pr->pr_lx;
	struct ruler_indent *cookie = NULL;
	int ninit = 0;

	indent = ruler_indent(arg->rl, arg->dc, &cookie);
	dc = doc_alloc(DOC_CONCAT, indent);
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	for (;;) {
		struct token *comma, *tk;
		int error;

		if (lexer_peek(lx, &tk) && tk == arg->semi)
			break;

		error = parser_decl_init1(pr, concat, arg);
		if (error & NONE)
			break;
		ninit++;
		if (error & HALT)
			break;

		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			doc_token(comma, concat);
			doc_alloc(DOC_LINE, concat);
			if (parser_simple_decl_active(pr))
				simple_decl_comma(pr->pr_simple.decl, comma);
			/* Break before the argument. */
			concat = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_GROUP, dc));
			doc_alloc(DOC_SOFTLINE, concat);
			concat = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_GROUP, concat));
			/*
			 * Any preceeding expr cannot be the last one of this
			 * declaration.
			 */
			arg->out = concat;
		}
	}
	if (ninit == 0) {
		ruler_remove(arg->rl, cookie);
		doc_remove(indent, arg->dc);
	}
	return parser_good(pr);
}

static int
parser_decl_init1(struct parser *pr, struct doc *dc,
    struct parser_decl_init_arg *arg)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (lexer_if(lx, TOKEN_IDENT, &tk)) {
		doc_token(tk, dc);
		if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
			doc_literal(" ", dc);
		return parser_good(pr);
	} else if (lexer_if(lx, TOKEN_LSQUARE, &tk) ||
	    lexer_if(lx, TOKEN_LPAREN, &tk)) {
		struct doc *expr = NULL;
		int rhs = tk->tk_type == TOKEN_LSQUARE ?
		    TOKEN_RSQUARE : TOKEN_RPAREN;
		int error;

		doc_token(tk, dc);
		/* Let the remaning tokens hang of the expression. */
		error = parser_expr(pr, &expr, &(struct parser_expr_arg){
		    .dc	= dc,
		});
		if (error & HALT)
			expr = dc;
		if (lexer_expect(lx, rhs, &tk))
			doc_token(tk, expr);
		if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
			doc_literal(" ", dc);
		return parser_good(pr);
	} else if (parser_decl_bitfield(pr, dc) & GOOD) {
		return parser_good(pr);
	} else if (lexer_if_flags(lx,
	    TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE, &tk)) {
		doc_token(tk, dc);
		doc_literal(" ", dc);
		return parser_good(pr);
	} else if (lexer_if(lx, TOKEN_STAR, &tk)) {
		doc_token(tk, dc);
		return parser_good(pr);
	} else if (parser_attributes(pr, dc, &arg->out, 0) & GOOD) {
		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL)) {
			doc_literal(" ", dc);
			arg->out = NULL;
		}
		return parser_good(pr);
	}

	return parser_decl_init_assign(pr, dc, arg);
}

static int
parser_decl_init_assign(struct parser *pr, struct doc *dc,
    struct parser_decl_init_arg *arg)
{
	struct doc *dedent;
	struct lexer *lx = pr->pr_lx;
	struct token *equal;
	int error;

	if (!lexer_if(lx, TOKEN_EQUAL, &equal))
		return parser_none(pr);

	if (arg->flags & PARSER_DECL_INIT_ASSIGN)
		doc_literal(" ", dc);
	doc_token(equal, dc);
	doc_literal(" ", dc);

	dedent = doc_alloc(DOC_CONCAT, ruler_dedent(arg->rl, dc, NULL));
	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		error = parser_braces(pr, dedent, arg->indent, 0);
		if (error & (FAIL | NONE))
			return parser_fail(pr);
	} else {
		struct token *pv, *stop;
		unsigned int flags = 0;

		/* Never break before the assignment operator. */
		if (!parser_simple_decl_active(pr) &&
		    (pv = token_prev(equal)) != NULL &&
		    token_has_line(pv, 1)) {
			parser_token_trim_after(pr, pv);
			token_add_optline(equal);
		}

		/*
		 * Honor hard line after assignment operator which must be
		 * emitted inside the expression document to get indentation
		 * right.
		 */
		if (token_has_line(equal, 1))
			flags |= EXPR_EXEC_HARDLINE;

		lexer_peek_until_comma(lx, arg->semi, &stop);
		error = parser_expr(pr, &arg->out, &(struct parser_expr_arg){
		    .dc		= dedent,
		    .stop	= stop,
		    .indent	= style(pr->pr_st, ContinuationIndentWidth),
		    .flags	= flags,
		});
		if (error & HALT)
			return parser_fail(pr);
	}

	return parser_good(pr);
}

static int
parser_decl_bitfield(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *colon, *size;
	unsigned int dospace = style(pr->pr_st, BitFieldColonSpacing);

	if (!lexer_if(lx, TOKEN_COLON, &colon))
		return parser_none(pr);

	if (dospace == Both || dospace == Before)
		doc_literal(" ", dc);
	doc_token(colon, dc);
	if (dospace == Both || dospace == After)
		doc_literal(" ", dc);
	if (lexer_expect(lx, TOKEN_LITERAL, &size))
		doc_token(size, dc);
	return parser_good(pr);
}

/*
 * Parse a declaration making use of preprocessor directives such as the ones
 * provided by queue(3).
 */
static int
parser_decl_cpp(struct parser *pr, struct doc *dc, struct ruler *rl,
    unsigned int flags)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *end, *macro, *semi, *tk;
	struct doc *expr = dc;
	int peek = 0;
	int error, hasident;

	lexer_peek_enter(lx, &s);
	while (lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE,
	    NULL))
		continue;
	if (lexer_if(lx, TOKEN_IDENT, &macro) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &end)) {
		struct token *ident;

		for (;;) {
			if (!lexer_if(lx, TOKEN_STAR, &tk))
				break;
			end = tk;
		}

		if (lexer_if(lx, TOKEN_EQUAL, NULL))
			peek = 1;
		else if ((flags & PARSER_DECL_ROOT) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		else if (lexer_if(lx, TOKEN_IDENT, &ident) &&
		    (token_cmp(macro, ident) == 0 ||
		     lexer_if(lx, TOKEN_SEMI, NULL)))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (!peek && (flags & PARSER_DECL_ROOT)) {
		lexer_peek_enter(lx, &s);
		if (lexer_if(lx, TOKEN_IDENT, &end) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		lexer_peek_leave(lx, &s);
	}
	if (!peek) {
		if (parser_decl_cppx(pr, dc, rl) & GOOD)
			return parser_good(pr);
		return parser_none(pr);
	}

	if (parser_type(pr, dc, end, rl) & (FAIL | NONE))
		return parser_fail(pr);

	if (!lexer_peek_until(lx, TOKEN_SEMI, &semi))
		return parser_fail(pr);
	hasident = !lexer_peek_if(lx, TOKEN_EQUAL, NULL);
	error = parser_decl_init(pr, &(struct parser_decl_init_arg){
	    .dc		= dc,
	    .rl		= rl,
	    .semi	= semi,
	    .indent	= style(pr->pr_st, IndentWidth),
	    .flags	= hasident ? PARSER_DECL_INIT_ASSIGN : 0,
	});
	if (error & (FAIL | NONE))
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, expr);

	return parser_good(pr);
}

static int
parser_decl_cppx(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *stop, *tk;

	if (!parser_cpp_peek_x(pr, NULL))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	while (lexer_if_flags(lx, TOKEN_FLAG_STORAGE, &tk)) {
		doc_token(tk, concat);
		doc_alloc(DOC_LINE, concat);
	}
	if (!lexer_peek_until_freestanding(lx, TOKEN_RPAREN, NULL, &rparen) ||
	    (stop = token_next(rparen)) == NULL)
		return parser_fail(pr);
	return parser_expr(pr, NULL, &(struct parser_expr_arg){
	    .dc		= dc,
	    .stop	= stop,
	    .rl		= rl,
	    .flags	= EXPR_EXEC_ALIGN,
	});
}

static int
parser_simple_decl_active(const struct parser *pr)
{
	return pr->pr_nblocks > 0 &&
	    pr->pr_simple.nstmt == 0 &&
	    pr->pr_simple.ndecl == 1 &&
	    pr->pr_simple.decl != NULL;
}

static int
parser_simple_decl_enter(struct parser *pr, unsigned int flags)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct doc *dc;
	int error;

	if ((pr->pr_op->op_flags & OPTIONS_SIMPLE) == 0)
		return -1;

	if (pr->pr_simple.ndecl++ > 0)
		return 1;

	pr->pr_simple.decl = simple_decl_enter(lx, pr->pr_op);
	dc = doc_alloc(DOC_CONCAT, NULL);
	lexer_peek_enter(lx, &s);
	error = parser_decl1(pr, dc, flags);
	lexer_peek_leave(lx, &s);
	doc_free(dc);
	if (error & GOOD)
		simple_decl_leave(pr->pr_simple.decl);
	simple_decl_free(pr->pr_simple.decl);
	pr->pr_simple.decl = NULL;
	return 1;
}

static void
parser_simple_decl_leave(struct parser *pr, int simple)
{
	if (simple == 1)
		pr->pr_simple.ndecl--;
}
