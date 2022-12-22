#include "parser.h"

#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "buffer.h"
#include "doc.h"
#include "error.h"
#include "expr.h"
#include "lexer.h"
#include "options.h"
#include "ruler.h"
#include "simple-decl.h"
#include "simple-stmt.h"
#include "style.h"
#include "token.h"

/*
 * Return values for parser exec routines. Only one of the following may be
 * returned but disjoint values are favored allowing the caller to catch
 * multiple return values.
 */
#define GOOD	0x00000001u
#define FAIL	0x00000002u
#define NONE	0x00000004u
#define BRCH	0x00000008u
#define HALT	(FAIL | NONE | BRCH)

enum parser_peek {
	PARSER_PEEK_FUNCDECL	= 1,
	PARSER_PEEK_FUNCIMPL	= 2,
};

struct parser {
	const char		*pr_path;
	struct error		*pr_er;
	const struct options	*pr_op;
	const struct style	*pr_st;
	struct lexer		*pr_lx;
	struct buffer		*pr_bf;		/* scratch buffer */
	unsigned int		 pr_error;
	unsigned int		 pr_nblocks;	/* # stmt blocks */
	unsigned int		 pr_nindent;	/* # indented stmt blocks */

	struct {
		struct simple_stmt	*stmt;
		struct simple_decl	*decl;
		int			 nstmt;
		int			 ndecl;
	} pr_simple;
};

struct parser_exec_decl_braces_arg {
	struct doc	*dc;
	struct ruler	*rl;
	unsigned int	 indent;
	unsigned int	 col;
	unsigned int	 flags;
#define PARSER_EXEC_DECL_BRACES_ENUM		0x00000001u
#define PARSER_EXEC_DECL_BRACES_TRIM		0x00000002u
/* Remove the given indent before emitting the right brace. */
#define PARSER_EXEC_DECL_BRACES_DEDENT		0x00000004u
/* Perform conditional indentation. */
#define PARSER_EXEC_DECL_BRACES_INDENT_MAYBE	0x00000008u
};

struct parser_exec_decl_init_arg {
	struct doc		*dc;
	struct doc		*out;
	struct doc		*width;
	struct ruler		*rl;
	const struct token	*semi;
	unsigned int		 indent;
	unsigned int		 flags;
/* Insert space before assignment operator. */
#define PARSER_EXEC_DECL_INIT_ASSIGN		0x00000001u
};

struct parser_exec_func_proto_arg {
	struct doc		*dc;
	struct ruler		*rl;
	const struct token	*type;
	struct doc		*out;
	unsigned int		 flags;
#define PARSER_EXEC_FUNC_PROTO_IMPL		0x00000001u
};

struct parser_exec_stmt_block_arg {
	struct doc	*head;
	struct doc	*tail;
	struct doc	*rbrace;
	unsigned int	 flags;
#define PARSER_EXEC_STMT_BLOCK_SWITCH		0x00000001u
#define PARSER_EXEC_STMT_BLOCK_TRIM		0x00000002u
};

/* Honor grouped declarations. */
#define PARSER_EXEC_DECL_BREAK			0x00000001u
/* Emit hard line after declaration(s). */
#define PARSER_EXEC_DECL_LINE			0x00000002u
/* Parsing of declarations on root level. */
#define PARSER_EXEC_DECL_ROOT			0x00000004u

static int	parser_exec1(struct parser *, struct doc *);

static int	parser_exec_extern(struct parser *, struct doc *);

static int	parser_exec_decl(struct parser *, struct doc *, unsigned int);
static int	parser_exec_decl1(struct parser *, struct doc *, unsigned int);
static int	parser_exec_decl2(struct parser *, struct doc *,
    struct ruler *, unsigned int);
static int	parser_exec_decl_init(struct parser *,
    struct parser_exec_decl_init_arg *);
static int	parser_exec_decl_init_assign(struct parser *,
    struct doc *, struct parser_exec_decl_init_arg *);
static int	parser_exec_decl_braces(struct parser *, struct doc *,
    unsigned int, unsigned int);
static int	parser_exec_decl_braces1(struct parser *,
    struct parser_exec_decl_braces_arg *);
static int	parser_exec_decl_braces_field(struct parser *,
    struct parser_exec_decl_braces_arg *, struct doc *, const struct token *);
static int	parser_exec_decl_cpp(struct parser *, struct doc *,
    struct ruler *, unsigned int);
static int	parser_exec_decl_cppx(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_cppdefs(struct parser *, struct doc *);

static int	parser_exec_expr(struct parser *, struct doc *, struct doc **,
    const struct token *, unsigned int, unsigned int);

static int	parser_exec_func_decl(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_func_impl(struct parser *, struct doc *);
static int	parser_exec_func_impl1(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_func_proto(struct parser *,
    struct parser_exec_func_proto_arg *);
static int	parser_exec_func_arg(struct parser *, struct doc *,
    struct doc **, const struct token *);

#define PARSER_EXEC_STMT_EXPR_DOWHILE		0x00000001u

static int	parser_exec_stmt(struct parser *, struct doc *);
static int	parser_exec_stmt1(struct parser *, struct doc *);
static int	parser_exec_stmt_block(struct parser *,
    struct parser_exec_stmt_block_arg *);
static int	parser_exec_stmt_if(struct parser *, struct doc *);
static int	parser_exec_stmt_for(struct parser *, struct doc *);
static int	parser_exec_stmt_dowhile(struct parser *, struct doc *);
static int	parser_exec_stmt_return(struct parser *, struct doc *);
static int	parser_exec_stmt_expr(struct parser *, struct doc *);
static int	parser_exec_stmt_kw_expr(struct parser *, struct doc *,
    const struct token *, unsigned int);
static int	parser_exec_stmt_label(struct parser *, struct doc *);
static int	parser_exec_stmt_case(struct parser *, struct doc *);
static int	parser_exec_stmt_goto(struct parser *, struct doc *);
static int	parser_exec_stmt_switch(struct parser *, struct doc *);
static int	parser_exec_stmt_while(struct parser *, struct doc *);
static int	parser_exec_stmt_break(struct parser *, struct doc *);
static int	parser_exec_stmt_continue(struct parser *, struct doc *);
static int	parser_exec_stmt_cpp(struct parser *, struct doc *);
static int	parser_exec_stmt_semi(struct parser *, struct doc *);
static int	parser_exec_stmt_asm(struct parser *, struct doc *);

static int	parser_exec_type(struct parser *, struct doc *,
    const struct token *, struct ruler *);

static int	parser_exec_attributes(struct parser *, struct doc *,
    struct doc **, enum doc_type);

static int	parser_simple_active(const struct parser *);

static int	parser_simple_decl_active(const struct parser *);
static int	parser_simple_decl_enter(struct parser *, unsigned int);
static void	parser_simple_decl_leave(struct parser *, int);

static int		 parser_simple_stmt_enter(struct parser *);
static void		 parser_simple_stmt_leave(struct parser *, int);
static struct doc	*parser_simple_stmt_block(struct parser *,
    struct doc *);
static struct doc	*parser_simple_stmt_ifelse_enter(struct parser *,
    struct doc *, void **);
static void		 parser_simple_stmt_ifelse_leave(struct parser *,
    void *);

static int		parser_peek_cppx(struct parser *);
static int		parser_peek_cpp_init(struct parser *);
static int		parser_peek_decl(struct parser *);
static enum parser_peek	parser_peek_func(struct parser *, struct token **);
static int		parser_peek_func_line(struct parser *);
static int		parser_peek_line(struct parser *, const struct token *);

static struct token	*parser_peek_braces_expr_stop(struct parser *,
    const struct token *);

static void		parser_token_trim_before(const struct parser *,
    struct token *);
static void		parser_token_trim_after(const struct parser *,
    struct token *);
static unsigned int	parser_width(struct parser *, const struct doc *);

#define parser_fail(a) \
	parser_fail0((a), __func__, __LINE__)
static int	parser_fail0(struct parser *, const char *, int);

static int	parser_get_error(const struct parser *);
static int	parser_good(const struct parser *);
static int	parser_none(const struct parser *);
static void	parser_reset(struct parser *);

static int	iscdefs(const char *, size_t);

struct parser *
parser_alloc(const char *path, struct lexer *lx, struct error *er,
    const struct style *st, const struct options *op)
{
	struct parser *pr;

	pr = ecalloc(1, sizeof(*pr));
	pr->pr_path = path;
	pr->pr_er = er;
	pr->pr_st = st;
	pr->pr_op = op;
	pr->pr_lx = lx;
	pr->pr_bf = buffer_alloc(1024);
	return pr;
}

void
parser_free(struct parser *pr)
{
	if (pr == NULL)
		return;

	buffer_free(pr->pr_bf);
	free(pr);
}

struct buffer *
parser_exec(struct parser *pr, size_t sizhint)
{
	struct buffer *bf = NULL;
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	unsigned int flags = DOC_EXEC_TRIM;
	int error = 0;

	dc = doc_alloc(DOC_CONCAT, NULL);

	for (;;) {
		struct doc *concat;
		struct token *tk;

		concat = doc_alloc(DOC_CONCAT, dc);

		/* Always emit EOF token as it could have dangling tokens. */
		if (lexer_if(lx, LEXER_EOF, &tk)) {
			doc_token(tk, concat);
			error = 0;
			break;
		}

		error = parser_exec1(pr, concat);
		if (error & GOOD) {
			lexer_stamp(lx);
		} else if (error & BRCH) {
			if (!lexer_branch(lx))
				break;
			parser_reset(pr);
		} else if (error & (FAIL | NONE)) {
			int r;

			r = lexer_recover(lx);
			if (r == 0)
				break;
			while (r-- > 0)
				doc_remove_tail(dc);
			parser_reset(pr);
		}
	}
	if (error) {
		parser_fail(pr);
		goto out;
	}

	bf = buffer_alloc(sizhint);
	if (pr->pr_op->op_flags & OPTIONS_DIFFPARSE)
		flags |= DOC_EXEC_DIFF;
	if (trace(pr->pr_op, 'd'))
		flags |= DOC_EXEC_TRACE;
	doc_exec(dc, pr->pr_lx, bf, pr->pr_st, pr->pr_op, flags);

out:
	doc_free(dc);
	return bf;
}

/*
 * Callback routine invoked by expression parser while encountering an invalid
 * expression.
 */
int
parser_expr_recover(const struct expr_exec_arg *ea, struct doc *dc, void *arg)
{
	struct parser *pr = arg;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	int recovered = 0;

	if (lexer_peek_if_type(lx, &tk,
	    (ea->flags & EXPR_EXEC_TYPE) ? LEXER_TYPE_CAST : 0)) {
		struct token *nx, *pv;

		if (!lexer_back(lx, &pv))
			goto out;
		nx = token_next(tk);
		if (pv != NULL && nx != NULL &&
		    (pv->tk_type == TOKEN_LPAREN ||
		     pv->tk_type == TOKEN_COMMA ||
		     pv->tk_type == TOKEN_SIZEOF) &&
		    (nx->tk_type == TOKEN_RPAREN ||
		     nx->tk_type == TOKEN_COMMA ||
		     nx->tk_type == LEXER_EOF)) {
			if (parser_exec_type(pr, dc, tk, NULL) & GOOD)
				recovered = 1;
		}
	} else if (lexer_if_flags(lx, TOKEN_FLAG_BINARY, &tk)) {
		struct token *pv;

		pv = token_prev(tk);
		if (pv != NULL &&
		    (pv->tk_type == TOKEN_LPAREN ||
		     pv->tk_type == TOKEN_COMMA)) {
			doc_token(tk, dc);
			recovered = 1;
		}
	} else if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		int error;

		error = parser_exec_decl_braces(pr, dc,
		    ea->indent,
		    PARSER_EXEC_DECL_BRACES_DEDENT |
		    PARSER_EXEC_DECL_BRACES_INDENT_MAYBE);
		if (error & GOOD)
			recovered = 1;
	}

out:
	return recovered;
}

int
parser_expr_recover_cast(void *arg)
{
	struct parser *pr = arg;

	return lexer_if_type(pr->pr_lx, NULL, LEXER_TYPE_CAST);
}

static int
parser_exec1(struct parser *pr, struct doc *dc)
{
	int error;

	error = parser_exec_decl(pr, dc,
	    PARSER_EXEC_DECL_BREAK | PARSER_EXEC_DECL_LINE |
	    PARSER_EXEC_DECL_ROOT);
	if (error == NONE)
		error = parser_exec_func_impl(pr, dc);
	if (error == NONE)
		error = parser_exec_extern(pr, dc);
	return error;
}

static int
parser_exec_extern(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_EXTERN, NULL) &&
	    lexer_if(lx, TOKEN_STRING, NULL) &&
	    lexer_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_EXTERN, &tk))
		doc_token(tk, dc);
	doc_literal(" ", dc);
	if (lexer_expect(lx, TOKEN_STRING, &tk))
		doc_token(tk, dc);
	doc_literal(" ", dc);
	if (lexer_expect(lx, TOKEN_LBRACE, &tk))
		doc_token(tk, dc);
	doc_alloc(DOC_HARDLINE, dc);
	for (;;) {
		int error;

		error = parser_exec1(pr, dc);
		if (error & NONE)
			break;
		if (error & HALT)
			return error;
	}
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, dc);
	doc_alloc(DOC_HARDLINE, dc);
	return parser_good(pr);
}

static int
parser_exec_decl(struct parser *pr, struct doc *dc, unsigned int flags)
{
	int error, simple;

	simple = parser_simple_decl_enter(pr, flags);
	error = parser_exec_decl1(pr, dc, flags);
	parser_simple_decl_leave(pr, simple);
	return error;
}

static int
parser_exec_decl1(struct parser *pr, struct doc *dc, unsigned int flags)
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

		error = parser_exec_decl2(pr, decl, &rl, flags);
		if (error & (FAIL | NONE)) {
			if (line != NULL)
				doc_remove(line, decl);
			break;
		}
		ndecl++;
		if (error & BRCH)
			break;

		line = doc_alloc(DOC_HARDLINE, decl);

		if (flags & PARSER_EXEC_DECL_BREAK) {
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

	if (flags & PARSER_EXEC_DECL_LINE)
		doc_alloc(DOC_HARDLINE, decl);
	return parser_good(pr);
}

static int
parser_exec_decl2(struct parser *pr, struct doc *dc, struct ruler *rl,
    unsigned int flags)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat;
	struct token *beg, *end, *fun, *semi, *tk;
	enum parser_peek peek;
	int error;

	if (parser_exec_decl_cppdefs(pr, dc) & GOOD)
		return parser_good(pr);

	if (!lexer_peek_if_type(lx, &end, 0)) {
		/* No type found, this declaration could make use of cpp. */
		return parser_exec_decl_cpp(pr, dc, rl, flags);
	}

	/*
	 * Presence of a type does not necessarily imply that this a declaration
	 * since it could be a function declaration or implementation.
	 */
	if ((peek = parser_peek_func(pr, &fun))) {
		if (peek == PARSER_PEEK_FUNCDECL)
			return parser_exec_func_decl(pr, dc, rl, fun);
		return parser_none(pr);
	}

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	if (!lexer_peek(lx, &beg))
		return parser_fail(pr);
	if (parser_simple_decl_active(pr))
		simple_decl_type(pr->pr_simple.decl, beg, end);
	if (parser_exec_type(pr, concat, end, rl) & (FAIL | NONE))
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
		if (parser_exec_decl(pr, indent, 0) & FAIL)
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
		error = parser_exec_decl_braces(pr, concat, w,
		    PARSER_EXEC_DECL_BRACES_ENUM |
		    PARSER_EXEC_DECL_BRACES_TRIM);
		if (error & HALT)
			return parser_fail(pr);
	}

	if (!lexer_peek_until(lx, TOKEN_SEMI, &semi))
		return parser_fail(pr);

	struct parser_exec_decl_init_arg arg = {
		.dc	= concat,
		.width	= dc,
		.rl	= rl,
		.semi	= semi,
		.indent	= style(pr->pr_st, IndentWidth),
		.flags	= PARSER_EXEC_DECL_INIT_ASSIGN,
	};
	error = parser_exec_decl_init(pr, &arg);
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
parser_exec_decl_init(struct parser *pr,
    struct parser_exec_decl_init_arg *arg)
{
	struct doc *concat, *dc, *indent;
	struct lexer *lx = pr->pr_lx;
	struct ruler_indent *cookie = NULL;
	int ninit = 0;
	int error;

	indent = ruler_indent(arg->rl, arg->dc, &cookie);
	dc = doc_alloc(DOC_CONCAT, indent);
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	for (;;) {
		struct doc *expr = NULL;
		struct token *comma, *tk;

		if (lexer_peek(lx, &tk) && tk == arg->semi)
			break;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_token(tk, concat);
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				doc_literal(" ", concat);
		} else if (((error = parser_exec_decl_init_assign(
		    pr, concat, arg)) & NONE) == 0) {
			if (error & HALT)
				return parser_fail(pr);
		} else if (lexer_if(lx, TOKEN_LSQUARE, &tk) ||
		    lexer_if(lx, TOKEN_LPAREN, &tk)) {
			int rhs = tk->tk_type == TOKEN_LSQUARE ?
			    TOKEN_RSQUARE : TOKEN_RPAREN;

			doc_token(tk, concat);
			/* Let the remaning tokens hang of the expression. */
			error = parser_exec_expr(pr, concat, &expr, NULL,
			    0, 0);
			if (error & HALT)
				expr = concat;
			if (lexer_expect(lx, rhs, &tk))
				doc_token(tk, expr);
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				doc_literal(" ", concat);
		} else if (lexer_if(lx, TOKEN_COLON, &tk)) {
			/* Handle bitfields. */
			doc_token(tk, concat);
			if (lexer_expect(lx, TOKEN_LITERAL, &tk))
				doc_token(tk, concat);
		} else if (lexer_if(lx, TOKEN_COMMA, &comma)) {
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
			arg->out = NULL;
		} else if (lexer_if_flags(lx,
		    TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE, &tk)) {
			doc_token(tk, concat);
			doc_literal(" ", concat);
		} else if (lexer_if(lx, TOKEN_STAR, &tk)) {
			doc_token(tk, concat);
		} else if (parser_exec_attributes(pr, concat, NULL,
		    DOC_LINE) & GOOD) {
			if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
				doc_literal(" ", concat);
		} else {
			break;
		}
		ninit++;
	}
	if (ninit == 0) {
		ruler_remove(arg->rl, cookie);
		doc_remove(indent, arg->dc);
	}

	return parser_good(pr);
}

static int
parser_exec_decl_init_assign(struct parser *pr, struct doc *dc,
    struct parser_exec_decl_init_arg *arg)
{
	struct doc *dedent;
	struct lexer *lx = pr->pr_lx;
	struct token *assign;
	int error;

	if (!lexer_if_flags(lx, TOKEN_FLAG_ASSIGN, &assign))
		return parser_none(pr);

	if (arg->flags & PARSER_EXEC_DECL_INIT_ASSIGN)
		doc_literal(" ", dc);
	doc_token(assign, dc);
	doc_literal(" ", dc);

	dedent = doc_alloc(DOC_CONCAT, ruler_dedent(arg->rl, dc, NULL));
	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		error = parser_exec_decl_braces(pr, dedent, arg->indent, 0);
		if (error & (FAIL | NONE))
			return parser_fail(pr);
	} else {
		struct token *pv, *stop;
		unsigned int flags = 0;
		unsigned int w;
		int doalign = 1;

		/* Never break before the assignment operator. */
		if (!parser_simple_decl_active(pr) &&
		    (pv = token_prev(assign)) != NULL &&
		    token_has_line(pv, 1)) {
			parser_token_trim_after(pr, pv);
			token_add_optline(assign);
			doalign = 0;
		}

		/*
		 * Honor hard line after assignment which must be emitted inside
		 * the expression document to get indentation right.
		 */
		if (token_has_line(assign, 1)) {
			flags |= EXPR_EXEC_HARDLINE;
			doalign = 0;
		}

		lexer_peek_until_comma(lx, arg->semi, &stop);
		if (doalign && style(pr->pr_st, AlignAfterOpenBracket) == Align)
			w = parser_width(pr, arg->width);
		else
			w = style(pr->pr_st, ContinuationIndentWidth);
		error = parser_exec_expr(pr, dedent, &arg->out, stop, w, flags);
		if (error & HALT)
			return parser_fail(pr);
	}

	return parser_good(pr);
}

static int
parser_exec_decl_braces(struct parser *pr, struct doc *dc, unsigned int indent,
    unsigned int flags)
{
	struct ruler rl;
	struct doc *concat;
	int error;

	ruler_init(&rl, 0, RULER_ALIGN_TABS | RULER_REQUIRE_TABS);
	concat = doc_alloc(DOC_CONCAT, dc);
	error = parser_exec_decl_braces1(pr,
	    &(struct parser_exec_decl_braces_arg){
		.dc	= concat,
		.rl	= &rl,
		.indent	= indent,
		.col	= 0,
		.flags	= flags,
	});
	ruler_exec(&rl);
	ruler_free(&rl);
	return error;
}

static int
parser_exec_decl_braces1(struct parser *pr,
    struct parser_exec_decl_braces_arg *arg)
{
	struct doc *braces, *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace, *tk;
	unsigned int w = 0;
	int align = 1;
	int error, hasline;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return parser_fail(pr);

	/*
	 * If any column is followed by a hard line, do not align but
	 * instead respect existing hard line(s).
	 */
	if (parser_peek_line(pr, rbrace))
		align = 0;

	braces = doc_alloc(DOC_CONCAT, arg->dc);

	if (!lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		return parser_fail(pr);
	hasline = token_has_line(lbrace, 1);
	if (arg->flags & PARSER_EXEC_DECL_BRACES_TRIM)
		parser_token_trim_after(pr, lbrace);
	doc_token(lbrace, braces);

	if (lexer_peek_if(lx, TOKEN_RBRACE, NULL)) {
		/* Honor spaces in empty braces. */
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);
		goto out;
	}

	if (hasline) {
		int val = arg->indent;

		if (arg->flags & PARSER_EXEC_DECL_BRACES_INDENT_MAYBE)
			val |= DOC_INDENT_NEWLINE;
		indent = doc_alloc_indent(val, braces);
		doc_alloc(DOC_HARDLINE, indent);
	} else {
		if (token_has_spaces(lbrace))
			doc_literal(" ", braces);

		/*
		 * Take note of the width of the document, must be accounted for
		 * while performing alignment.
		 */
		w = parser_width(pr, braces);
		indent = doc_alloc_indent(w, braces);
	}

	for (;;) {
		struct doc *expr = NULL;
		struct doc *concat;
		struct token *comma, *nx, *pv;

		if (!lexer_peek(lx, &tk) || tk->tk_type == LEXER_EOF)
			return parser_fail(pr);
		if (tk == rbrace)
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));

		if ((arg->flags & PARSER_EXEC_DECL_BRACES_ENUM) ||
		    lexer_peek_if(lx, TOKEN_PERIOD, NULL) ||
		    lexer_peek_if(lx, TOKEN_LSQUARE, NULL)) {
			error = parser_exec_decl_braces_field(pr, arg, concat,
			    rbrace);
			if (error & HALT)
				return parser_fail(pr);
		} else if (lexer_peek_if(lx, TOKEN_LBRACE, &nx)) {
			struct doc *dc = arg->dc;
			unsigned int col = arg->col;

			arg->dc = concat;
			if (parser_exec_decl_braces1(pr, arg) & HALT)
				return parser_fail(pr);
			arg->dc = dc;
			/*
			 * If the nested braces are positioned on the same line
			 * as the braces currently being parsed, do not restore
			 * the column as we're still on the same row in terms of
			 * alignment.
			 */
			if (token_cmp(lbrace, nx) < 0)
				arg->col = col;
		} else if (parser_peek_cppx(pr) || parser_peek_cpp_init(pr)) {
			error = parser_exec_decl_cppx(pr, concat, arg->rl);
			if (error & HALT)
				return parser_fail(pr);
		} else {
			struct token *stop;

			stop = parser_peek_braces_expr_stop(pr, rbrace);
			error = parser_exec_expr(pr, concat, &expr, stop, 0, 0);
			if (error & HALT)
				return parser_fail(pr);
		}
		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			if (expr == NULL)
				expr = concat;
			doc_token(comma, expr);

			if (align) {
				arg->col++;
				w += parser_width(pr, concat);
				ruler_insert(arg->rl, comma, concat,
				    arg->col, w, 0);
				w = 0;
				goto next;
			}
		}

		if (!lexer_back(lx, &pv) || !lexer_peek(lx, &nx)) {
			return parser_fail(pr);
		} else if (token_has_spaces(pv) &&
		    (nx != rbrace || token_cmp(pv, rbrace) == 0)) {
			doc_literal(" ", concat);
		} else if (nx == rbrace) {
			/*
			 * Put the last hard line outside to get indentation
			 * right.
			 */
			if (token_cmp(pv, rbrace) < 0) {
				if (arg->flags &
				    PARSER_EXEC_DECL_BRACES_DEDENT) {
					braces = doc_alloc_indent(-arg->indent,
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
		if (((arg->flags & PARSER_EXEC_DECL_BRACES_ENUM) == 0) &&
		    lexer_back(lx, &nx) && token_has_line(nx, 2))
			ruler_exec(arg->rl);
	}

out:
	if (lexer_expect(lx, TOKEN_RBRACE, &rbrace)) {
		parser_token_trim_after(pr, rbrace);
		doc_token(rbrace, braces);
	}
	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL) &&
	    !lexer_peek_if(lx, TOKEN_COMMA, NULL) &&
	    !lexer_peek_if(lx, TOKEN_RBRACE, NULL) &&
	    !lexer_peek_if(lx, TOKEN_RPAREN, NULL))
		doc_literal(" ", braces);

	return parser_good(pr);
}

static int
parser_exec_decl_braces_field(struct parser *pr,
    struct parser_exec_decl_braces_arg *arg, struct doc *dc,
    const struct token *rbrace)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk = NULL;
	struct token *stop;
	int error;

	lexer_peek_until_comma(lx, rbrace, &stop);

	for (;;) {
		struct doc *expr = NULL;

		if (lexer_if(lx, TOKEN_LSQUARE, &tk)) {
			doc_token(tk, dc);
			error = parser_exec_expr(pr, dc, &expr, NULL,
			    0, 0);
			if (error & HALT)
				return parser_fail(pr);
			if (lexer_expect(lx, TOKEN_RSQUARE, &tk))
				doc_token(tk, expr);
		} else if (lexer_if(lx, TOKEN_PERIOD, &tk)) {
			struct token *equal;

			doc_token(tk, dc);
			if (lexer_expect(lx, TOKEN_IDENT, &tk))
				doc_token(tk, dc);

			/* Correct alignment, must occur after the ident. */
			if (lexer_peek_if(lx, TOKEN_EQUAL, &equal) &&
			    token_has_tabs(equal))
				token_move_suffixes_if(equal, tk, TOKEN_SPACE);
		} else if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_token(tk, dc);

			/*
			 * Only applicable to enum declarations making use of
			 * preprocessor directives.
			 */
			if (lexer_if(lx, TOKEN_LPAREN, &tk)) {
				doc_token(tk, dc);
				error = parser_exec_expr(pr, dc, &expr,
				    NULL, 0, 0);
				if (error & FAIL)
					return parser_fail(pr);
				if (error & HALT)
					expr = dc;
				if (lexer_expect(lx, TOKEN_RPAREN, &tk))
					doc_token(tk, expr);
			}

			/*
			 * Only applicable to enum declarations which are
			 * allowed to omit any initialization, alignment is not
			 * desired in such scenario.
			 */
			if (lexer_peek_if(lx, TOKEN_COMMA, NULL) ||
			    (lexer_peek_if(lx, TOKEN_RBRACE, &tk) &&
			     tk == rbrace))
				goto out;
		} else {
			break;
		}
	}
	if (tk == NULL)
		return parser_fail(pr);

	ruler_insert(arg->rl, tk, dc, 1, parser_width(pr, dc), 0);

	error = parser_exec_decl_init(pr, &(struct parser_exec_decl_init_arg){
	    .dc		= dc,
	    .width	= dc,
	    .rl		= arg->rl,
	    .semi	= stop,
	    .indent	= arg->indent,
	    .flags	= 0
	});
	if (error & (FAIL | NONE))
		return parser_fail(pr);

out:
	return parser_good(pr);
}

/*
 * Parse a declaration making use of preprocessor directives such as the ones
 * provided by queue(3).
 */
static int
parser_exec_decl_cpp(struct parser *pr, struct doc *dc, struct ruler *rl,
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
		else if ((flags & PARSER_EXEC_DECL_ROOT) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		else if (lexer_if(lx, TOKEN_IDENT, &ident) &&
		    (token_cmp(macro, ident) == 0 ||
		     lexer_if(lx, TOKEN_SEMI, NULL)))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (!peek && (flags & PARSER_EXEC_DECL_ROOT)) {
		lexer_peek_enter(lx, &s);
		if (lexer_if(lx, TOKEN_IDENT, &end) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		lexer_peek_leave(lx, &s);
	}
	if (!peek) {
		if (parser_peek_cppx(pr))
			return parser_exec_decl_cppx(pr, dc, rl);
		return parser_none(pr);
	}

	if (parser_exec_type(pr, dc, end, rl) & (FAIL | NONE))
		return parser_fail(pr);

	if (!lexer_peek_until(lx, TOKEN_SEMI, &semi))
		return parser_fail(pr);
	hasident = !lexer_peek_if(lx, TOKEN_EQUAL, NULL);
	error = parser_exec_decl_init(pr, &(struct parser_exec_decl_init_arg){
	    .dc		= dc,
	    .width	= dc,
	    .rl		= rl,
	    .semi	= semi,
	    .indent	= style(pr->pr_st, IndentWidth),
	    .flags	= hasident ? PARSER_EXEC_DECL_INIT_ASSIGN : 0,
	});
	if (error & (FAIL | NONE))
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, expr);

	return parser_good(pr);
}

static int
parser_exec_decl_cppx(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *tk;
	unsigned int col = 0;
	unsigned int indent = 0;
	unsigned int w;

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	while (lexer_if_flags(lx, TOKEN_FLAG_STORAGE, &tk)) {
		doc_token(tk, concat);
		doc_alloc(DOC_LINE, concat);
	}
	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		doc_token(tk, concat);
	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, concat);

	/*
	 * Take note of the width of the document up to the first argument, must
	 * be accounted for while performing alignment.
	 */
	w = parser_width(pr, concat);

	if (style(pr->pr_st, AlignAfterOpenBracket) == Align)
		indent = w;

	for (;;) {
		struct doc *expr = NULL;
		struct doc *arg;
		struct token *stop;
		int error;

		if (lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			break;

		arg = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

		lexer_peek_until_comma(lx, rparen, &stop);
		error = parser_exec_expr(pr, arg, &expr, stop,
		    indent, EXPR_EXEC_TYPE);
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_if(lx, TOKEN_COMMA, &tk)) {
			doc_token(tk, expr);
			w += parser_width(pr, arg);
			ruler_insert(rl, tk, expr, ++col, w, 0);
			w = 0;
		}
	}

	if (lexer_expect(lx, TOKEN_RPAREN, &tk))
		doc_token(tk, dc);

	return parser_good(pr);
}

/*
 * Parse usage of macros from cdefs.h, such as __BEGIN_HIDDEN_DECLS.
 */
static int
parser_exec_decl_cppdefs(struct parser *pr, struct doc *dc)
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

static int
parser_exec_expr(struct parser *pr, struct doc *dc, struct doc **expr,
    const struct token *stop, unsigned int indent, unsigned int flags)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.dc		= dc,
		.stop		= stop,
		.indent		= indent,
		.flags		= flags,
		.callbacks	= {
			.recover	= parser_expr_recover,
			.recover_cast	= parser_expr_recover_cast,
			.arg		= pr,
		},
	};
	struct doc *ex;

	ex = expr_exec(&ea);
	if (ex == NULL)
		return parser_none(pr);
	if (expr != NULL)
		*expr = ex;
	return parser_good(pr);
}

static int
parser_exec_func_decl(struct parser *pr, struct doc *dc, struct ruler *rl,
    const struct token *type)
{
	struct parser_exec_func_proto_arg arg = {
		.rl	= rl,
		.type	= type,
		.flags	= 0,
	};
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	arg.dc = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	if (parser_exec_func_proto(pr, &arg) & (FAIL | NONE))
		return parser_fail(pr);

	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, arg.out);

	return parser_good(pr);
}

static int
parser_exec_func_impl(struct parser *pr, struct doc *dc)
{
	struct ruler rl;
	struct token *type;
	int error;

	if (parser_peek_func(pr, &type) != PARSER_PEEK_FUNCIMPL)
		return parser_none(pr);

	ruler_init(&rl, 1, RULER_ALIGN_FIXED);
	error = parser_exec_func_impl1(pr, dc, &rl, type);
	if (error & GOOD)
		ruler_exec(&rl);
	ruler_free(&rl);
	return error;
}

static int
parser_exec_func_impl1(struct parser *pr, struct doc *dc, struct ruler *rl,
    const struct token *type)
{
	struct parser_exec_func_proto_arg arg = {
		.dc	= dc,
		.rl	= rl,
		.type	= type,
		.flags	= PARSER_EXEC_FUNC_PROTO_IMPL,
	};
	struct lexer *lx = pr->pr_lx;
	int error;

	if (parser_exec_func_proto(pr, &arg) & (FAIL | NONE))
		return parser_fail(pr);
	if (!lexer_peek_if(lx, TOKEN_LBRACE, NULL))
		return parser_fail(pr);

	if (style_brace_wrapping(pr->pr_st, AfterFunction))
		doc_alloc(DOC_HARDLINE, dc);
	else
		doc_literal(" ", dc);
	error = parser_exec_stmt_block(pr, &(struct parser_exec_stmt_block_arg){
	    .head	= dc,
	    .tail	= dc,
	    .rbrace	= NULL,
	    .flags	= 0,
	});
	if (error & (FAIL | NONE))
		return parser_fail(pr);
	doc_alloc(DOC_HARDLINE, dc);
	if (parser_peek_func_line(pr))
		doc_alloc(DOC_HARDLINE, dc);

	return parser_good(pr);
}

/*
 * Parse a function prototype, i.e. return type, identifier, arguments and
 * optional attributes. The caller is expected to already have parsed the
 * return type.
 */
static int
parser_exec_func_proto(struct parser *pr,
    struct parser_exec_func_proto_arg *arg)
{
	struct doc *dc = arg->dc;
	struct doc *attributes, *concat, *group, *indent, *kr;
	struct lexer *lx = pr->pr_lx;
	struct token *lparen, *rparen, *tk;
	unsigned int s;
	int nkr = 0;
	int error;

	if (parser_exec_type(pr, dc, arg->type, arg->rl) & (FAIL | NONE))
		return parser_fail(pr);

	s = style(pr->pr_st, AlwaysBreakAfterReturnType);
	if ((s == All || s == TopLevel) ||
	    ((arg->flags & PARSER_EXEC_FUNC_PROTO_IMPL) &&
	     (s == AllDefinitions || s == TopLevelDefinitions)))
		doc_alloc(DOC_HARDLINE, dc);

	/*
	 * The function identifier and arguments are intended to fit on a single
	 * line.
	 */
	group = doc_alloc(DOC_GROUP, dc);
	concat = doc_alloc(DOC_CONCAT, group);

	if (arg->type->tk_flags & TOKEN_FLAG_TYPE_FUNC) {
		/* Function returning function pointer. */
		if (lexer_expect(lx, TOKEN_LPAREN, &lparen))
			doc_token(lparen, concat);
		if (lexer_expect(lx, TOKEN_STAR, &tk))
			doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_IDENT, &tk))
			doc_token(tk, concat);
		if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN,
		    &rparen))
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_LPAREN, &lparen))
			doc_token(lparen, concat);
		while (parser_exec_func_arg(pr, concat, NULL, rparen) & GOOD)
			continue;
		if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
			doc_token(rparen, concat);
		if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
			doc_token(rparen, concat);
	} else if (lexer_expect(lx, TOKEN_IDENT, &tk)) {
		parser_token_trim_after(pr, tk);
		doc_token(tk, concat);
	} else {
		doc_remove(group, dc);
		return parser_fail(pr);
	}

	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_LPAREN, &lparen)) {
		parser_token_trim_after(pr, lparen);
		parser_token_trim_before(pr, rparen);
		parser_token_trim_after(pr, rparen);
		doc_token(lparen, concat);
	}
	if (style(pr->pr_st, AlignAfterOpenBracket) == Align) {
		indent = doc_alloc_indent(DOC_INDENT_WIDTH, concat);
	} else {
		indent = doc_alloc_indent(
		    style(pr->pr_st, ContinuationIndentWidth), concat);
	}
	while (parser_exec_func_arg(pr, indent, &arg->out, rparen) & GOOD)
		continue;
	/* Can be empty if arguments are absent. */
	if (arg->out == NULL)
		arg->out = concat;
	if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
		doc_token(rparen, arg->out);

	/* Recognize K&R argument declarations. */
	kr = doc_alloc(DOC_GROUP, dc);
	indent = doc_alloc_indent(style(pr->pr_st, IndentWidth), kr);
	doc_alloc(DOC_HARDLINE, indent);
	if (parser_exec_decl(pr, indent, 0) & GOOD)
		nkr++;
	if (nkr == 0)
		doc_remove(kr, dc);

	attributes = doc_alloc(DOC_GROUP, dc);
	indent = doc_alloc_indent(style(pr->pr_st, IndentWidth), attributes);
	error = parser_exec_attributes(pr, indent, &arg->out, DOC_HARDLINE);
	if (error & HALT)
		doc_remove(attributes, dc);

	return parser_good(pr);
}

/*
 * Parse one function argument as part of either a declaration or
 * implementation.
 */
static int
parser_exec_func_arg(struct parser *pr, struct doc *dc, struct doc **out,
    const struct token *rparen)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *pv = NULL;
	struct token *tk, *type;
	int error = 0;

	if (!lexer_peek_if_type(lx, &type, LEXER_TYPE_ARG))
		return parser_none(pr);

	/*
	 * Let each argument begin with a soft line, causing a line to be
	 * emitted immediately if the argument does not fit instead of breaking
	 * the argument.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_alloc(DOC_SOFTLINE, concat);
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_OPTIONAL, concat));

	if (parser_exec_type(pr, concat, type, NULL) & (FAIL | NONE))
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

		if (parser_exec_attributes(pr, concat, NULL, DOC_LINE) & GOOD)
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

static int
parser_exec_stmt(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	int simple = -1;
	int error;

	if (lexer_peek_if(lx, TOKEN_IF, NULL) ||
	    lexer_peek_if(lx, TOKEN_FOR, NULL) ||
	    lexer_peek_if(lx, TOKEN_WHILE, NULL) ||
	    lexer_peek_if(lx, TOKEN_IDENT, NULL))
		simple = parser_simple_stmt_enter(pr);
	error = parser_exec_stmt1(pr, dc);
	parser_simple_stmt_leave(pr, simple);
	return error;
}

static int
parser_exec_stmt1(struct parser *pr, struct doc *dc)
{
	struct parser_exec_stmt_block_arg ps = {
		.head	= dc,
		.tail	= dc,
		.flags	= PARSER_EXEC_STMT_BLOCK_TRIM,
		.rbrace	= NULL,
	};

	/*
	 * Most likely statement comes first with some crucial exceptions:
	 *
	 *     1. Detect blocks before expressions as the expression parser can
	 *        also detect blocks through recovery using the parser.
	 *
	 *     2. Detect expressions before declarations as functions calls
	 *        would otherwise be treated as declarations. This in turn is
	 *        caused by parser_exec_decl() being able to detect declarations
	 *        making use of preprocessor directives such as the ones
	 *        provided by queue(3).
	 */
	if ((parser_exec_stmt_block(pr, &ps) & GOOD) ||
	    (parser_exec_stmt_expr(pr, dc) & GOOD) ||
	    (parser_exec_stmt_if(pr, dc) & GOOD) ||
	    (parser_exec_stmt_return(pr, dc) & GOOD) ||
	    (parser_exec_decl(pr, dc, PARSER_EXEC_DECL_BREAK) & GOOD) ||
	    (parser_exec_stmt_case(pr, dc) & GOOD) ||
	    (parser_exec_stmt_break(pr, dc) & GOOD) ||
	    (parser_exec_stmt_goto(pr, dc) & GOOD) ||
	    (parser_exec_stmt_for(pr, dc) & GOOD) ||
	    (parser_exec_stmt_while(pr, dc) & GOOD) ||
	    (parser_exec_stmt_label(pr, dc) & GOOD) ||
	    (parser_exec_stmt_switch(pr, dc) & GOOD) ||
	    (parser_exec_stmt_continue(pr, dc) & GOOD) ||
	    (parser_exec_stmt_asm(pr, dc) & GOOD) ||
	    (parser_exec_stmt_dowhile(pr, dc) & GOOD) ||
	    (parser_exec_stmt_semi(pr, dc) & GOOD) ||
	    (parser_exec_stmt_cpp(pr, dc) & GOOD))
		return parser_good(pr);
	return parser_none(pr);
}

/*
 * Parse a block statement wrapped in braces.
 */
static int
parser_exec_stmt_block(struct parser *pr, struct parser_exec_stmt_block_arg *ps)
{
	struct doc *concat, *dc, *indent, *line;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *nx, *rbrace, *tk;
	int isswitch = ps->flags & PARSER_EXEC_STMT_BLOCK_SWITCH;
	int doindent = !isswitch && pr->pr_simple.nstmt == 0;
	int nstmt = 0;
	int error;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return parser_none(pr);

	/*
	 * Remove semi before emitting the right brace in order to honor
	 * optional lines.
	 */
	nx = token_next(rbrace);
	if (nx != NULL && nx->tk_type == TOKEN_SEMI &&
	    (pr->pr_op->op_flags & OPTIONS_SIMPLE))
		lexer_remove(lx, nx, 1);

	if (doindent)
		pr->pr_nindent++;
	pr->pr_nblocks++;

	dc = parser_simple_stmt_block(pr, ps->tail);

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
	if ((ps->flags & PARSER_EXEC_STMT_BLOCK_TRIM) ||
	    (token_has_line(lbrace, 2) && parser_peek_decl(pr)))
		parser_token_trim_after(pr, lbrace);
	doc_token(lbrace, ps->head);

	if (isswitch)
		indent = dc;
	else
		indent = doc_alloc_indent(style(pr->pr_st, IndentWidth), dc);
	line = doc_alloc(DOC_HARDLINE, indent);
	while ((error = parser_exec_stmt(pr, indent)) & GOOD) {
		nstmt++;
		if (lexer_peek(lx, &tk) && tk == rbrace)
			break;
		doc_alloc(DOC_HARDLINE, indent);
	}
	/* Do not keep the hard line if the statement block is empty. */
	if (nstmt == 0 && (error & BRCH) == 0)
		doc_remove(line, indent);

	doc_alloc(DOC_HARDLINE, ps->tail);

	/*
	 * The right brace and any following statement is expected to fit on a
	 * single line.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, ps->tail));
	if (lexer_expect(lx, TOKEN_RBRACE, &tk)) {
		if (lexer_peek_if(lx, TOKEN_ELSE, NULL))
			parser_token_trim_after(pr, tk);
		doc_token(tk, concat);
	}
	if (lexer_if(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);
	ps->rbrace = concat;

	if (doindent)
		pr->pr_nindent--;
	pr->pr_nblocks--;

	return parser_good(pr);
}

static int
parser_exec_stmt_if(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk, *tkelse, *tkif;

	if (!lexer_peek_if(lx, TOKEN_IF, &tk))
		return parser_none(pr);

	if (parser_exec_stmt_kw_expr(pr, dc, tk, 0) & (FAIL | NONE))
		return parser_fail(pr);

	while (lexer_peek_if(lx, TOKEN_ELSE, &tkelse)) {
		int error;

		if (lexer_back(lx, &tk) && tk->tk_type == TOKEN_RBRACE)
			doc_literal(" ", dc);
		else
			doc_alloc(DOC_HARDLINE, dc);
		if (!lexer_expect(lx, TOKEN_ELSE, &tk))
			break;
		doc_token(tk, dc);
		doc_literal(" ", dc);

		if (lexer_peek_if(lx, TOKEN_IF, &tkif) &&
		    token_cmp(tkelse, tkif) == 0) {
			error = parser_exec_stmt_kw_expr(pr, dc, tkif, 0);
			if (error & (FAIL | NONE))
				return parser_fail(pr);
		} else {
			if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
				error = parser_exec_stmt(pr, dc);
				if (error & FAIL)
					return parser_fail(pr);
			} else {
				void *simple = NULL;

				dc = doc_alloc_indent(
				    style(pr->pr_st, IndentWidth), dc);
				doc_alloc(DOC_HARDLINE, dc);

				dc = parser_simple_stmt_ifelse_enter(pr, dc,
				    &simple);
				error = parser_exec_stmt(pr, dc);
				parser_simple_stmt_ifelse_leave(pr, simple);
				if (error & (FAIL | NONE))
					parser_fail(pr);
			}

			/* Terminate if/else chain. */
			break;
		}
	}

	return parser_good(pr);
}

static int
parser_exec_stmt_for(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *expr = NULL;
	struct doc *space = NULL;
	struct doc *loop;
	struct token *semi, *tk;
	unsigned int flags, w;
	int error;

	if (!lexer_if(lx, TOKEN_FOR, &tk))
		return parser_none(pr);

	loop = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(tk, loop);
	doc_literal(" ", loop);

	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, loop);

	if (style(pr->pr_st, AlignOperands) == Align)
		w = parser_width(pr, dc);
	else
		w = style(pr->pr_st, ContinuationIndentWidth);

	/* Declarations are allowed in the first expression. */
	if (parser_exec_decl(pr, loop, 0) & NONE) {
		error = parser_exec_expr(pr, loop, &expr, NULL,
		    w, 0);
		if (error & (FAIL | BRCH))
			return parser_fail(pr);
		/* Let the semicolon hang of the expression unless empty. */
		if (error & NONE)
			expr = loop;
		if (lexer_expect(lx, TOKEN_SEMI, &semi))
			doc_token(semi, expr);
	} else {
		expr = loop;
	}
	space = lexer_back(lx, &semi) && !token_has_line(semi, 1) ?
	    doc_literal(" ", expr) : NULL;

	/*
	 * If the expression does not fit, break after the semicolon if the
	 * previous expression was not empty.
	 */
	flags = expr != loop ? EXPR_EXEC_SOFTLINE : 0;
	/* Let the semicolon hang of the expression unless empty. */
	error = parser_exec_expr(pr, loop, &expr, NULL,
	    w, flags);
	if (error & (FAIL | BRCH))
		return parser_fail(pr);
	if (error & NONE) {
		/* Expression empty, remove the space. */
		if (space != NULL)
			doc_remove(space, expr);
		expr = loop;
	}
	space = NULL;
	if (lexer_expect(lx, TOKEN_SEMI, &semi)) {
		doc_token(semi, expr);
		if (!token_has_line(semi, 1))
			space = doc_literal(" ", expr);
	}

	/*
	 * If the expression does not fit, break after the semicolon if
	 * the previous expression was not empty.
	 */
	flags = expr != loop ? EXPR_EXEC_SOFTLINE : 0;
	/* Let the semicolon hang of the expression unless empty. */
	error = parser_exec_expr(pr, loop, &expr, NULL,
	    w, flags);
	if (error & (FAIL | BRCH))
		return parser_fail(pr);
	if (error & NONE) {
		/* Expression empty, remove the space. */
		if (space != NULL)
			doc_remove(space, expr);
		expr = loop;
	}
	if (lexer_expect(lx, TOKEN_RPAREN, &tk))
		doc_token(tk, expr);

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_literal(" ", expr);
	} else {
		dc = doc_alloc_indent(style(pr->pr_st, IndentWidth), dc);
		doc_alloc(DOC_HARDLINE, dc);
	}
	return parser_exec_stmt(pr, dc);
}

static int
parser_exec_stmt_dowhile(struct parser *pr, struct doc *dc)
{
	struct parser_exec_stmt_block_arg ps = {
		.head	= dc,
		.tail	= dc,
		.flags	= PARSER_EXEC_STMT_BLOCK_TRIM,
		.rbrace	= NULL,
	};
	struct lexer *lx = pr->pr_lx;
	struct doc *concat = dc;
	struct token *tk;
	int error;

	if (!lexer_if(lx, TOKEN_DO, &tk))
		return parser_none(pr);

	doc_token(tk, concat);
	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_literal(" ", concat);
		error = parser_exec_stmt_block(pr, &ps);
		/*
		 * The following while statement is intended to fit on the same
		 * line as the right brace.
		 */
		concat = ps.rbrace;
		doc_literal(" ", concat);
	} else {
		struct doc *indent;

		indent = doc_alloc_indent(style(pr->pr_st, IndentWidth),
		    concat);
		doc_alloc(DOC_HARDLINE, indent);
		error = parser_exec_stmt(pr, indent);
		doc_alloc(DOC_HARDLINE, concat);
	}
	if (error & HALT)
		return parser_fail(pr);

	if (lexer_peek_if(lx, TOKEN_WHILE, &tk)) {
		return parser_exec_stmt_kw_expr(pr, concat, tk,
		    PARSER_EXEC_STMT_EXPR_DOWHILE);
	}
	return parser_fail(pr);
}

static int
parser_exec_stmt_return(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat, *expr;
	struct token *tk;

	if (!lexer_if(lx, TOKEN_RETURN, &tk))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	parser_token_trim_after(pr, tk);
	doc_token(tk, concat);
	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL)) {
		int error;
		unsigned int w;

		doc_literal(" ", concat);
		if (style(pr->pr_st, AlignOperands) == Align)
			w = parser_width(pr, dc);
		else
			w = style(pr->pr_st, ContinuationIndentWidth);
		error = parser_exec_expr(pr, concat, &expr, NULL,
		    w, EXPR_EXEC_NOPARENS);
		if (error & HALT)
			return parser_fail(pr);
	} else {
		expr = concat;
	}
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, expr);
	return parser_good(pr);
}

static int
parser_exec_stmt_expr(struct parser *pr, struct doc *dc)
{
	const struct expr_exec_arg ea = {
		.st		= pr->pr_st,
		.op		= pr->pr_op,
		.lx		= pr->pr_lx,
		.dc		= NULL,
		.stop		= NULL,
		.flags		= 0,
		.callbacks	= {
			.recover	= parser_expr_recover,
			.recover_cast	= parser_expr_recover_cast,
			.arg		= pr,
		},
	};
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct doc *expr = NULL;
	struct token *ident, *lparen, *nx, *rparen, *semi;
	int peek = 0;
	int error;

	if (lexer_peek_if_type(lx, NULL, 0))
		return parser_none(pr);
	if (!lexer_peek_until(lx, TOKEN_SEMI, &semi))
		return parser_none(pr);

	lexer_peek_enter(lx, &s);
	if (expr_peek(&ea) && lexer_pop(lx, &nx) && nx == semi)
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	/*
	 * Do not confuse a loop construct hidden behind cpp followed by a
	 * statement which is a sole statement:
	 *
	 * 	foreach()
	 * 		func();
	 */
	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_peek_if(lx, TOKEN_LPAREN, &lparen) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen) &&
	    token_cmp(lparen, rparen) == 0 &&
	    lexer_pop(lx, &nx) && nx != semi &&
	    token_cmp(ident, nx) < 0 && token_cmp(nx, semi) <= 0)
		peek = 0;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	error = parser_exec_expr(pr, dc, &expr, NULL,
	    style(pr->pr_st, ContinuationIndentWidth), 0);
	if (error & HALT)
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_SEMI, &semi))
		doc_token(semi, expr);
	return parser_good(pr);
}

/*
 * Parse a statement consisting of a keyword, expression wrapped in parenthesis
 * and followed by additional nested statement(s).
 */
static int
parser_exec_stmt_kw_expr(struct parser *pr, struct doc *dc,
    const struct token *type, unsigned int flags)
{
	struct doc *expr = NULL;
	struct doc *stmt;
	struct lexer *lx = pr->pr_lx;
	struct token *lparen, *rparen, *tk;
	unsigned int w = 0;
	int error;

	if (!lexer_expect(lx, type->tk_type, &tk) ||
	    !lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_fail(pr);
	parser_token_trim_before(pr, rparen);
	parser_token_trim_after(pr, rparen);

	stmt = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(tk, stmt);
	if (type->tk_type != TOKEN_IDENT)
		doc_literal(" ", stmt);

	if (style(pr->pr_st, AlignOperands) == Align) {
		/*
		 * Take note of the width before emitting the left parenthesis
		 * as it could be followed by comments, which must not affect
		 * alignment.
		 */
		w = parser_width(pr, dc) + 1;
	} else {
		w = style(pr->pr_st, ContinuationIndentWidth);
	}

	if (lexer_expect(lx, TOKEN_LPAREN, &lparen)) {
		struct doc *optional = stmt;

		if (token_has_suffix(lparen, TOKEN_COMMENT)) {
			optional = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_OPTIONAL, stmt));
		}
		doc_token(lparen, optional);
	}

	/*
	 * The tokens after the expression must be nested underneath the same
	 * expression since we want to fit everything until the following
	 * statement on a single line.
	 */
	error = parser_exec_expr(pr, stmt, &expr, rparen, w, 0);
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
		doc_token(rparen, expr);
	}

	if (flags & PARSER_EXEC_STMT_EXPR_DOWHILE) {
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
		return parser_good(pr);
	}

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		struct parser_exec_stmt_block_arg ps = {
			.head	= expr,
			.tail	= dc,
			.flags	= PARSER_EXEC_STMT_BLOCK_TRIM,
			.rbrace	= NULL,
		};

		if (type->tk_type == TOKEN_SWITCH)
			ps.flags |= PARSER_EXEC_STMT_BLOCK_SWITCH;
		doc_literal(" ", expr);
		return parser_exec_stmt_block(pr, &ps);
	} else {
		struct doc *indent;
		void *simple = NULL;

		indent = doc_alloc_indent(style(pr->pr_st, IndentWidth), dc);
		doc_alloc(DOC_HARDLINE, indent);
		if (type->tk_type == TOKEN_IF) {
			indent = parser_simple_stmt_ifelse_enter(pr, indent,
			    &simple);
		}
		error = parser_exec_stmt(pr, indent);
		if (type->tk_type == TOKEN_IF)
			parser_simple_stmt_ifelse_leave(pr, simple);
		return error;
	}
}

static int
parser_exec_stmt_label(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct doc *dedent;
	struct lexer *lx = pr->pr_lx;
	struct token *colon, *ident, *nx;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if(lx, TOKEN_COLON, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	dedent = doc_alloc_dedent(dc);
	if (lexer_expect(lx, TOKEN_IDENT, &ident)) {
		struct doc *label;

		label = doc_token(ident, dedent);
		/*
		 * Honor indentation before label but make sure to emit it right
		 * before the label. Necessary when the label is prefixed with
		 * comment(s).
		 */
		if (token_has_indent(ident))
			doc_append_before(doc_literal(" ", NULL), label);
	}

	if (lexer_expect(lx, TOKEN_COLON, &colon)) {
		doc_token(colon, dedent);

		/*
		 * A label is not necessarily followed by a hard line, there
		 * could be another statement on the same line.
		 */
		if (lexer_peek(lx, &nx) && token_cmp(colon, nx) == 0) {
			struct doc *indent;

			indent = doc_alloc_indent(DOC_INDENT_FORCE, dc);
			return parser_exec_stmt(pr, indent);
		}
	}

	return parser_good(pr);
}

static int
parser_exec_stmt_case(struct parser *pr, struct doc *dc)
{
	struct doc *indent, *lhs;
	struct lexer *lx = pr->pr_lx;
	struct token *kw, *tk;

	if (!lexer_if(lx, TOKEN_CASE, &kw) && !lexer_if(lx, TOKEN_DEFAULT, &kw))
		return parser_none(pr);

	lhs = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(kw, lhs);
	if (!lexer_peek_until(lx, TOKEN_COLON, NULL))
		return parser_fail(pr);
	if (kw->tk_type == TOKEN_CASE) {
		int error;

		doc_alloc(DOC_LINE, lhs);
		error = parser_exec_expr(pr, lhs, NULL, NULL, 0, 0);
		if (error & HALT)
			return parser_fail(pr);
	}
	if (!lexer_expect(lx, TOKEN_COLON, &tk))
		return parser_fail(pr);
	parser_token_trim_after(pr, tk);
	doc_token(tk, lhs);

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_alloc(DOC_LINE, lhs);
		if (parser_exec_stmt(pr, dc) & FAIL)
			return parser_fail(pr);
	}

	indent = doc_alloc_indent(style(pr->pr_st, IndentWidth), dc);
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

		if (parser_exec_stmt(pr, indent) & HALT) {
			/* No statement, remove the line. */
			doc_remove(line, indent);
			break;
		}
	}

	return parser_good(pr);
}

static int
parser_exec_stmt_goto(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat;
	struct token *tk;

	if (!lexer_if(lx, TOKEN_GOTO, &tk))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(tk, concat);
	doc_alloc(DOC_LINE, concat);
	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		doc_token(tk, concat);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);
	return parser_good(pr);
}

static int
parser_exec_stmt_switch(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (!lexer_peek_if(lx, TOKEN_SWITCH, &tk))
		return parser_none(pr);
	return parser_exec_stmt_kw_expr(pr, dc, tk, 0);
}

static int
parser_exec_stmt_while(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (!lexer_peek_if(lx, TOKEN_WHILE, &tk))
		return parser_none(pr);
	return parser_exec_stmt_kw_expr(pr, dc, tk, 0);
}

static int
parser_exec_stmt_break(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (!lexer_peek_if(lx, TOKEN_BREAK, NULL))
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_BREAK, &tk))
		doc_token(tk, dc);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, dc);
	return parser_good(pr);
}

static int
parser_exec_stmt_continue(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (!lexer_peek_if(lx, TOKEN_CONTINUE, &tk))
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_CONTINUE, &tk))
		doc_token(tk, dc);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, dc);
	return parser_good(pr);
}

/*
 * Parse statement hidden behind cpp, such as a loop construct from queue(3).
 */
static int
parser_exec_stmt_cpp(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *ident;

	if (!lexer_peek_if(lx, TOKEN_IDENT, &ident))
		return parser_none(pr);
	return parser_exec_stmt_kw_expr(pr, dc, ident, 0);
}

static int
parser_exec_stmt_semi(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *semi;

	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
		return parser_none(pr);

	if (lexer_expect(lx, TOKEN_SEMI, &semi))
		doc_token(semi, dc);
	return parser_good(pr);
}

static int
parser_exec_stmt_asm(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct doc *concat, *opt;
	struct token *assembly, *colon, *rparen, *tk;
	int peek = 0;
	int nops = 0;
	int error, i;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_ASSEMBLY, &assembly)) {
		lexer_if(lx, TOKEN_VOLATILE, NULL);
		if (lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	parser_token_trim_before(pr, rparen);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	if (lexer_expect(lx, TOKEN_ASSEMBLY, &assembly))
		doc_token(assembly, concat);
	if (lexer_if(lx, TOKEN_VOLATILE, &tk)) {
		doc_alloc(DOC_LINE, concat);
		doc_token(tk, concat);
		if (token_has_spaces(tk))
			doc_alloc(DOC_LINE, concat);
	}
	opt = doc_alloc_indent(style(pr->pr_st, ContinuationIndentWidth),
	    doc_alloc(DOC_OPTIONAL, dc));
	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, opt);

	/* instructions */
	if (!lexer_peek_until(lx, TOKEN_COLON, &colon))
		return parser_fail(pr);
	error = parser_exec_expr(pr, opt, NULL, colon, 0, 0);
	if (error & HALT)
		return parser_fail(pr);

	/* output and input operands */
	for (i = 0; i < 2; i++) {
		if (!lexer_if(lx, TOKEN_COLON, &colon))
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		if (i == 0 || nops > 0)
			doc_alloc(DOC_LINE, concat);
		doc_token(colon, concat);
		if (!lexer_peek_if(lx, TOKEN_COLON, NULL))
			doc_alloc(DOC_LINE, concat);

		error = parser_exec_expr(pr, concat, NULL, NULL,
		    0, EXPR_EXEC_ASM);
		if (error & FAIL)
			return parser_fail(pr);
		nops = error & GOOD;
	}

	/* clobbers */
	if (lexer_if(lx, TOKEN_COLON, &tk)) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, opt));
		doc_token(tk, concat);
		if (!lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			doc_alloc(DOC_LINE, concat);
		error = parser_exec_expr(pr, concat, NULL, rparen, 0, 0);
		if (error & FAIL)
			return parser_fail(pr);
	}

	if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
		doc_token(rparen, concat);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);

	return parser_good(pr);
}

/*
 * Parse token(s) denoting a type.
 */
static int
parser_exec_type(struct parser *pr, struct doc *dc, const struct token *end,
    struct ruler *rl)
{
	struct lexer *lx = pr->pr_lx;
	const struct token *align = NULL;
	struct token *beg;
	unsigned int nspaces = 0;

	if (!lexer_peek(lx, &beg))
		return parser_fail(pr);

	if (rl != NULL) {
		/*
		 * Find the first non pointer token starting from the end, this
		 * is where the ruler alignment must be performed.
		 */
		align = end->tk_token != NULL ? end->tk_token : end;
		for (;;) {
			if (align->tk_type != TOKEN_STAR)
				break;

			nspaces++;
			if (align == beg)
				break;
			align = token_prev(align);
			if (align == NULL)
				break;
		}

		/*
		 * No alignment wanted if the first non-pointer token is
		 * followed by a semi.
		 */
		if (align != NULL) {
			const struct token *nx;

			nx = token_next(align);
			if (nx != NULL && nx->tk_type == TOKEN_SEMI)
				align = NULL;
		}
	}

	if (pr->pr_op->op_flags & OPTIONS_SIMPLE) {
		struct token *tk = beg;
		int ntokens = 0;

		for (;;) {
			struct token *nx;

			nx = token_next(tk);
			if (ntokens > 0 &&
			    tk->tk_type == TOKEN_STATIC &&
			    token_is_moveable(tk)) {
				token_move_prefixes(beg, tk);
				token_move_suffixes(tk, beg);
				lexer_move_before(lx, beg, tk);
			}
			if (tk == end)
				break;
			tk = nx;
			ntokens++;
		}
	}

	for (;;) {
		struct doc *concat;
		struct token *tk;
		int didalign = 0;

		if (!lexer_pop(lx, &tk))
			return parser_fail(pr);
		parser_token_trim_after(pr, tk);

		if (tk->tk_flags & TOKEN_FLAG_TYPE_ARGS) {
			struct doc *indent;
			struct token *lparen = tk;
			struct token *rparen;
			unsigned int w;

			doc_token(lparen, dc);
			if (style(pr->pr_st, AlignAfterOpenBracket) == Align)
				w = parser_width(pr, dc);
			else
				w = style(pr->pr_st, ContinuationIndentWidth);
			indent = doc_alloc_indent(w, dc);
			while (parser_exec_func_arg(pr, indent, NULL, end) &
			    GOOD)
				continue;
			if (lexer_expect(lx, TOKEN_RPAREN, &rparen))
				doc_token(rparen, dc);
			break;
		}

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, concat);

		if (tk == align) {
			if (token_is_decl(tk, TOKEN_ENUM) ||
			    token_is_decl(tk, TOKEN_STRUCT) ||
			    token_is_decl(tk, TOKEN_UNION)) {
				doc_alloc(DOC_LINE, concat);
			} else {
				ruler_insert(rl, tk, concat, 1,
				    parser_width(pr, dc), nspaces);
			}
			didalign = 1;
		}

		if (tk == end)
			break;

		if (!didalign) {
			struct lexer_state s;
			struct token *nx;

			lexer_peek_enter(lx, &s);
			if (tk->tk_type != TOKEN_STAR &&
			    tk->tk_type != TOKEN_LPAREN &&
			    tk->tk_type != TOKEN_LSQUARE &&
			    lexer_pop(lx, &nx) &&
			    (nx->tk_type != TOKEN_LPAREN ||
			     lexer_if(lx, TOKEN_STAR, NULL)) &&
			    nx->tk_type != TOKEN_LSQUARE &&
			    nx->tk_type != TOKEN_RSQUARE &&
			    nx->tk_type != TOKEN_RPAREN &&
			    nx->tk_type != TOKEN_COMMA)
				doc_alloc(DOC_LINE, concat);
			lexer_peek_leave(lx, &s);
		}
	}

	return parser_good(pr);
}

static int
parser_exec_attributes(struct parser *pr, struct doc *dc, struct doc **out,
    enum doc_type linetype)
{
	struct doc *concat = NULL;
	struct lexer *lx = pr->pr_lx;
	int nattributes = 0;

	if (!lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		return NONE;

	doc_alloc(linetype, dc);
	for (;;) {
		struct token *tk;
		int error;

		if (!lexer_if(lx, TOKEN_ATTRIBUTE, &tk))
			break;

		if (nattributes > 0)
			doc_alloc(linetype, concat);
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(DOC_SOFTLINE, concat);
		doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, concat);
		error = parser_exec_expr(pr, concat, NULL, NULL, 0, 0);
		if (error & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, concat);
		nattributes++;
	}
	if (out != NULL)
		*out = concat;

	return parser_good(pr);
}

/*
 * Called while entering a section of the source code with one or many
 * statements potentially wrapped in curly braces ahead. The statements
 * will silently be formatted in order to determine if each statement fits on a
 * single line, making the curly braces redundant and thus removed. Otherwise,
 * curly braces will be added around all covered statements for consistency.
 * Once this routine returns, parsing continues as usual.
 *
 * The return value is used to signal when a nested statement is entered which
 * is ignored as only one scope is handled at a time. The same return value must
 * later on be passed to parser_simple_stmt_leave().
 */
static int
parser_simple_stmt_enter(struct parser *pr)
{
	struct lexer_state s;
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	int error;

	if ((pr->pr_op->op_flags & OPTIONS_SIMPLE) == 0)
		return 0;
	if (++pr->pr_simple.nstmt > 1)
		return 1;

	pr->pr_simple.stmt = simple_stmt_enter(lx, pr->pr_st, pr->pr_op);
	dc = doc_alloc(DOC_CONCAT, NULL);
	lexer_peek_enter(lx, &s);
	error = parser_exec_stmt1(pr, dc);
	lexer_peek_leave(lx, &s);
	doc_free(dc);
	if (error & GOOD)
		simple_stmt_leave(pr->pr_simple.stmt);
	simple_stmt_free(pr->pr_simple.stmt);
	pr->pr_simple.stmt = NULL;
	pr->pr_simple.nstmt--;

	return 0;
}

static void
parser_simple_stmt_leave(struct parser *pr, int simple)
{
	if (simple == 1)
		pr->pr_simple.nstmt--;
}

static struct doc *
parser_simple_stmt_block(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace;

	/* Ignore nested statements, they will be handled later on. */
	if ((pr->pr_op->op_flags & OPTIONS_SIMPLE) == 0 ||
	    pr->pr_simple.nstmt != 1)
		return dc;

	if (!lexer_peek_if(lx, TOKEN_LBRACE, &lbrace) ||
	    !lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return dc;

	return simple_stmt_block(pr->pr_simple.stmt, lbrace, rbrace,
	    pr->pr_nindent * style(pr->pr_st, IndentWidth));
}

static struct doc *
parser_simple_stmt_ifelse_enter(struct parser *pr, struct doc *dc,
    void **cookie)
{
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace;

	if (pr->pr_simple.nstmt != 1 || !lexer_peek(lx, &lbrace))
		return dc;
	return simple_stmt_ifelse_enter(pr->pr_simple.stmt, lbrace,
	    (pr->pr_nindent + 1) * style(pr->pr_st, IndentWidth), cookie);
}

static void
parser_simple_stmt_ifelse_leave(struct parser *pr, void *cookie)
{
	struct lexer *lx = pr->pr_lx;
	struct token *rbrace;

	if (cookie == NULL || !lexer_peek(lx, &rbrace))
		return;
	simple_stmt_ifelse_leave(pr->pr_simple.stmt, rbrace, cookie);
}

static int
parser_simple_active(const struct parser *pr)
{
	return pr->pr_simple.stmt != NULL || pr->pr_simple.decl != NULL;
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
	error = parser_exec_decl1(pr, dc, flags);
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

/*
 * Returns non-zero if the next tokens denotes a X macro. That is, something
 * that looks like a function call but is not followed by a semicolon nor comma
 * if being part of an initializer. One example are the macros provided by
 * RBT_PROTOTYPE(9).
 */
static int
parser_peek_cppx(struct parser *pr)
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
	return peek;
}

/*
 * Returns non-zero if the next tokens denotes an initializer making use of cpp.
 */
static int
parser_peek_cpp_init(struct parser *pr)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *comma, *ident, *rparen;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen) &&
	    lexer_if(lx, TOKEN_COMMA, &comma)) {
		const struct token *nx, *pv;

		pv = token_prev(ident);
		nx = token_next(comma);
		if (pv != NULL && token_cmp(pv, ident) < 0 &&
		    nx != NULL && token_cmp(nx, comma) > 0)
			peek = 1;
	}
	lexer_peek_leave(lx, &s);
	return peek;
}

static int
parser_peek_decl(struct parser *pr)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct doc *dc;
	int error;

	dc = doc_alloc(DOC_CONCAT, NULL);
	lexer_peek_enter(lx, &s);
	error = parser_exec_decl(pr, dc, 0);
	lexer_peek_leave(lx, &s);
	doc_free(dc);
	return error & GOOD;
}

/*
 * Returns non-zero if the next tokens denotes a function. The type argument
 * points to the last token of the return type.
 */
static enum parser_peek
parser_peek_func(struct parser *pr, struct token **type)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	enum parser_peek peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if_type(lx, type, 0)) {
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
			peek = PARSER_PEEK_FUNCDECL;
		else if (lexer_if(lx, TOKEN_LBRACE, NULL))
			peek = PARSER_PEEK_FUNCIMPL;
		else if (lexer_if_type(lx, NULL, 0))
			peek = PARSER_PEEK_FUNCIMPL;	/* K&R */
	}
out:
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Returns non-zero if the right brace of a function implementation can be
 * followed by a hard line.
 */
static int
parser_peek_func_line(struct parser *pr)
{
	struct lexer *lx = pr->pr_lx;
	struct token *cpp, *ident, *rbrace;
	struct lexer_state s;
	int annotated = 0;

	if (lexer_peek_if(lx, LEXER_EOF, NULL) || !lexer_back(lx, &rbrace))
		return 0;

	if (lexer_peek_if_prefix_flags(lx, TOKEN_FLAG_CPP, &cpp))
		return cpp->tk_lno - rbrace->tk_lno > 1;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL) &&
	    lexer_if(lx, TOKEN_SEMI, NULL) &&
	    ident->tk_lno - rbrace->tk_lno == 1)
		annotated = 1;
	lexer_peek_leave(lx, &s);
	if (annotated)
		return 0;

	return 1;
}

/*
 * Returns non-zero if any token up to the given stop token exclusively is
 * followed by a hard line.
 */
static int
parser_peek_line(struct parser *pr, const struct token *stop)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *pv = NULL;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	for (;;) {
		struct token *tk;

		if (!lexer_pop(lx, &tk))
			return parser_fail(pr);
		if (tk == stop)
			break;

		if (pv != NULL && token_cmp(tk, pv) > 0) {
			peek = 1;
			break;
		}
		pv = tk;
	}
	lexer_peek_leave(lx, &s);

	return peek;
}

static struct token *
parser_peek_braces_expr_stop(struct parser *pr, const struct token *rbrace)
{
	struct lexer *lx = pr->pr_lx;
	struct token *stop = NULL;
	struct token *comma;

	lexer_peek_until(lx, TOKEN_LBRACE, &stop);
	if (lexer_peek_until_comma(lx, rbrace, &comma)) {
		if (stop == NULL)
			stop = comma;
		else if (token_cmp(comma, stop) < 0 ||
		    (token_cmp(comma, stop) == 0 &&
		     comma->tk_cno < stop->tk_cno))
			stop = comma;
	}
	return stop;
}

static int
parser_fail0(struct parser *pr, const char *fun, int lno)
{
	struct buffer *bf;
	struct token *tk;

	if (parser_get_error(pr))
		goto out;
	pr->pr_error = 1;

	bf = error_begin(pr->pr_er);
	buffer_printf(bf, "%s: ", pr->pr_path);
	if (trace(pr->pr_op, 'l'))
		buffer_printf(bf, "%s:%d: ", fun, lno);
	buffer_printf(bf, "error at ");
	if (lexer_back(pr->pr_lx, &tk))
		buffer_printf(bf, "%s\n", lexer_serialize(pr->pr_lx, tk));
	else
		buffer_printf(bf, "(null)\n");
	error_end(pr->pr_er);

out:
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return FAIL;
}

/*
 * Remove any preceding hard line(s) from the given token, unless the token has
 * prefixes intended to be emitted.
 */
static void
parser_token_trim_before(const struct parser *pr, struct token *tk)
{
	struct token *pv;

	/* Avoid side effects in simple mode. */
	if (parser_simple_active(pr))
		return;

	if (!token_is_moveable(tk))
		return;
	pv = token_prev(tk);
	if (pv != NULL)
		token_trim(pv);
}

/*
 * Remove any subsequent hard line(s) from the given token.
 */
static void
parser_token_trim_after(const struct parser *pr, struct token *tk)
{
	/* Avoid side effects in simple mode. */
	if (parser_simple_active(pr))
		return;
	token_trim(tk);
}

/*
 * Returns the width of the given document.
 */
static unsigned int
parser_width(struct parser *pr, const struct doc *dc)
{
	return doc_width(dc, pr->pr_bf, pr->pr_st, pr->pr_op);
}

static int
parser_get_error(const struct parser *pr)
{
	return pr->pr_error || lexer_get_error(pr->pr_lx);
}

static int
parser_good(const struct parser *pr)
{
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return parser_get_error(pr) ? FAIL : GOOD;
}

static int
parser_none(const struct parser *pr)
{
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return parser_get_error(pr) ? FAIL : NONE;
}

static void
parser_reset(struct parser *pr)
{
	error_reset(pr->pr_er);
	pr->pr_error = 0;
}

static int
iscdefs(const char *str, size_t len)
{
	size_t i;

	if (len < 2 || strncmp(str, "__", 2) != 0)
		return 0;
	for (i = 2; i < len; i++) {
		unsigned char c = str[i];

		if (!isupper(c) && !isdigit(c) && c != '_')
			return 0;
	}
	return 1;
}
