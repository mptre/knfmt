#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/* Sentinel used to signal that the parser consumed something. */
#define PARSER_OK	0
/* Sentinel used to signal that nothing was found. */
#define PARSER_NOTHING	1

enum parser_peek {
	PARSER_PEEK_FUNCDECL	= 1,
	PARSER_PEEK_FUNCIMPL	= 2,
};

struct parser {
	const char		*pr_path;
	const struct config	*pr_cf;
	struct lexer		*pr_lx;
	struct buffer		*pr_bf;
	struct doc		*pr_dc;
	unsigned int		 pr_error;
	unsigned int		 pr_expr;
	unsigned int		 pr_switch;
	unsigned int		 pr_dowhile;
};

static int	parser_exec_decl(struct parser *, struct doc *, int);
static int	parser_exec_decl1(struct parser *, struct doc *, struct ruler *);
static int	parser_exec_decl_init(struct parser *, struct doc *,
    const struct token *, int);
static int	parser_exec_decl_braces(struct parser *, struct doc *);
static int	parser_exec_decl_braces1(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_braces_fields(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_decl_braces_field(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_decl_cpp(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_cppx(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_func_ptr(struct parser *, struct doc *);

static int	parser_exec_expr(struct parser *, struct doc *, struct doc **,
    const struct token *);

static int	parser_exec_func_decl(struct parser *, struct doc *,
    struct ruler *, struct token *);
static int	parser_exec_func_impl(struct parser *, struct doc *);
static int	parser_exec_func_proto(struct parser *, struct doc *,
    enum doc_type);
static int	parser_exec_func_arg(struct parser *, struct doc *,
    const struct token *);

static int	parser_exec_stmt(struct parser *, struct doc *);
static int	parser_exec_stmt1(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_block(struct parser *, struct doc *,
    struct doc *);
static int	parser_exec_stmt_expr(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_label(struct parser *, struct doc *);

static int	parser_exec_type(struct parser *, struct doc *, struct token *,
    struct ruler *);

static int	parser_exec_attributes(struct parser *, struct doc *,
    unsigned int, enum doc_type);

static struct doc	*parser_exec_expr_recover(void *);

static enum parser_peek	parser_peek_func(struct parser *, struct token **);

static unsigned int	parser_width(struct parser *, const struct doc *);

#define parser_error(a) \
	__parser_error((a), __func__, __LINE__)
static int	__parser_error(struct parser *, const char *, int);

static int	parser_halted(const struct parser *);

struct parser *
parser_alloc(const char *path, const struct config *cf)
{
	struct parser *pr;
	struct lexer *lex;

	lex = lexer_alloc(path, cf);
	if (lex == NULL)
		return NULL;

	pr = calloc(1, sizeof(*pr));
	if (pr == NULL)
		err(1, NULL);
	pr->pr_path = path;
	pr->pr_cf = cf;
	pr->pr_lx = lex;
	return pr;
}

void
parser_free(struct parser *pr)
{
	if (pr == NULL)
		return;

	doc_free(pr->pr_dc);
	lexer_free(pr->pr_lx);
	buffer_free(pr->pr_bf);
	free(pr);
}

struct lexer *
parser_get_lexer(struct parser *pr)
{
	return pr->pr_lx;
}

const struct buffer *
parser_exec(struct parser *pr)
{
	struct lexer *lx = pr->pr_lx;
	int error = 0;

	pr->pr_bf = buffer_alloc(lexer_get_buffer(lx)->bf_siz);
	pr->pr_dc = doc_alloc(DOC_CONCAT, NULL);

	for (;;) {
		struct token *tk;

		/* Always emit EOF token as it could have dangling tokens. */
		if (lexer_if(lx, TOKEN_EOF, &tk)) {
			doc_token(tk, pr->pr_dc);
			break;
		}

		if (parser_exec_decl(pr, pr->pr_dc, 1) &&
		    parser_exec_func_impl(pr, pr->pr_dc)) {
			error = 1;
			break;
		} else if (parser_halted(pr)) {
			error = 1;
			break;
		} else {
			doc_alloc(DOC_HARDLINE, pr->pr_dc);
		}
	}
	if (error) {
		parser_error(pr);
		return NULL;
	}

	doc_exec(pr->pr_dc, pr->pr_bf, pr->pr_cf);
	return pr->pr_bf;
}

static int
parser_exec_decl(struct parser *pr, struct doc *dc, int align)
{
	struct ruler rl;
	struct doc *line = NULL;
	struct lexer *lx = pr->pr_lx;
	int ndecl = 0;

	memset(&rl, 0, sizeof(rl));
	ruler_init(&rl, align);
	for (;;) {
		struct token *tk;

		if (parser_exec_decl1(pr, dc, &rl)) {
			if (line != NULL)
				doc_remove(line, dc);
			break;
		}
		ndecl++;

		line = doc_alloc(DOC_HARDLINE, dc);

		/*
		 * Honor an empty line which denotes the end of this block of
		 * declarations.
		 */
		if (lexer_back(lx, &tk) && token_has_line(tk))
			break;
	}
	ruler_exec(&rl);
	ruler_free(&rl);

	return ndecl > 0 ? PARSER_OK : PARSER_NOTHING;
}

static int
parser_exec_decl1(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat;
	struct token *beg, *end, *fun, *tk;
	enum parser_peek peek;

	if (!lexer_peek(lx, &beg))
		return PARSER_NOTHING;

	if (!lexer_peek_type(lx, &end, 0)) {
		/* No type found, this declaration could make use of cpp. */
		return parser_exec_decl_cpp(pr, dc, rl);
	}

	/*
	 * Presence of a type does not necessarily imply that this a declaration
	 * since it could be a function declaration or implementation.
	 */
	if ((peek = parser_peek_func(pr, &fun))) {
		if (peek == PARSER_PEEK_FUNCDECL)
			return parser_exec_func_decl(pr, dc, rl, fun);
		return PARSER_NOTHING;
	}

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	if (parser_exec_type(pr, concat, end, rl))
		return parser_error(pr);

	/* Presence of semicolon implies that this declaration is done. */
	if (lexer_peek_if(lx, TOKEN_SEMI, NULL))
		goto out;

	if (parser_exec_decl_func_ptr(pr, concat) == PARSER_OK) {
		/* nothing */
	} else if (token_is_decl(end, TOKEN_STRUCT) ||
	    token_is_decl(end, TOKEN_UNION)) {
		struct doc *indent;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_literal(" ", concat);
			doc_token(tk, concat);
		}
		if (lexer_expect(lx, TOKEN_LBRACE, &tk))
			doc_token(tk, concat);

		indent = doc_alloc_indent(pr->pr_cf->cf_tw, concat);
		doc_alloc(DOC_HARDLINE, indent);
		while (parser_exec_decl(pr, indent, 1) == PARSER_OK)
			continue;

		doc_alloc(DOC_HARDLINE, concat);
		if (lexer_expect(lx, TOKEN_RBRACE, &tk))
			doc_token(tk, concat);

		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
			doc_literal(" ", concat);
	} else if (token_is_decl(end, TOKEN_ENUM)) {
		struct token *rbrace;
		int isempty;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_literal(" ", concat);
			doc_token(tk, concat);
		}
		if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
			return parser_error(pr);
		if (lexer_expect(lx, TOKEN_LBRACE, &tk))
			doc_token(tk, concat);
		isempty = lexer_peek_if(lx, TOKEN_RBRACE, NULL);

		if (parser_exec_decl_braces_fields(pr, concat, rl, rbrace))
			return parser_error(pr);

		/*
		 * The enum declaration can be empty if it consists solely of
		 * preprocessor directives.
		 */
		if (!isempty)
			doc_alloc(DOC_HARDLINE, concat);
		if (lexer_expect(lx, TOKEN_RBRACE, &tk))
			doc_token(tk, concat);

		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
			doc_literal(" ", concat);
	}

	if (parser_exec_decl_init(pr, concat, NULL, 0))
		return parser_error(pr);

	parser_exec_attributes(pr, concat, pr->pr_cf->cf_tw, DOC_LINE);

out:
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);
	return PARSER_OK;
}

/*
 * Parse any initialization as part of a declaration.
 */
static int
parser_exec_decl_init(struct parser *pr, struct doc *dc,
    const struct token *stop, int didalign)
{
	struct lexer *lx = pr->pr_lx;

	for (;;) {
		struct doc *expr;
		struct token *tk;

		if (parser_halted(pr))
			return parser_error(pr);

		if (lexer_peek(lx, &tk) && tk == stop)
			break;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_token(tk, dc);
		} else if (lexer_if_flags(lx, TOKEN_FLAG_ASSIGN, &tk)) {
			if (!didalign)
				doc_literal(" ", dc);
			doc_token(tk, dc);
			doc_literal(" ", dc);

			if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
				if (parser_exec_decl_braces(pr, dc))
					return parser_error(pr);
			} else {
				struct token *estop = NULL;

				if (stop == NULL)
					(void)lexer_peek_until_loose(lx,
					    TOKEN_COMMA, NULL, &estop);
				if (parser_exec_expr(pr, dc, NULL,
					    estop != NULL ? estop : stop))
					return parser_error(pr);
			}
		} else if (lexer_if(lx, TOKEN_LSQUARE, &tk)) {
			doc_token(tk, dc);
			/* Let the remaning tokens hang of the expression. */
			if (parser_exec_expr(pr, dc, &expr, NULL) ==
			    PARSER_NOTHING)
				expr = dc;
			if (lexer_expect(lx, TOKEN_RSQUARE, &tk))
				doc_token(tk, expr);
		} else if (lexer_if(lx, TOKEN_COLON, &tk)) {
			/* Handle bitfields. */
			doc_token(tk, dc);
			if (lexer_expect(lx, TOKEN_LITERAL, &tk))
				doc_token(tk, dc);
		} else if (lexer_if(lx, TOKEN_COMMA, &tk) ||
		    lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER, &tk)) {
			doc_token(tk, dc);
			doc_literal(" ", dc);
		} else if (lexer_if(lx, TOKEN_STAR, &tk)) {
			doc_token(tk, dc);
		} else {
			break;
		}
	}

	return PARSER_OK;
}

static int
parser_exec_decl_braces(struct parser *pr, struct doc *dc)
{
	struct ruler rl;
	int error;

	memset(&rl, 0, sizeof(rl));
	ruler_init(&rl, 1);
	error = parser_exec_decl_braces1(pr, dc, &rl);
	ruler_exec(&rl);
	ruler_free(&rl);
	return error;

}

static int
parser_exec_decl_braces1(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct lexer_state s;
	struct doc *concat = NULL;
	struct doc *line = NULL;
	struct doc *expr, *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace, *pv, *tk;
	unsigned int col = 0;
	int align = 1;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return parser_error(pr);
	if (lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		doc_token(lbrace, dc);

	if (lexer_peek_if(lx, TOKEN_LSQUARE, NULL) ||
	    lexer_peek_if(lx, TOKEN_PERIOD, NULL)) {
		if (parser_exec_decl_braces_fields(pr, dc, rl, rbrace))
			return parser_error(pr);
		doc_alloc(DOC_HARDLINE, dc);
	}

	if (lexer_peek_if(lx, TOKEN_RBRACE, NULL))
		goto out;

	/*
	 * If any column is followed by a hard line, do not align but
	 * instead respect existing hard line(s).
	 */
	lexer_peek_enter(lx, &s);
	pv = lbrace;
	for (;;) {
		if (!lexer_pop(lx, &tk))
			return parser_error(pr);
		if (tk == rbrace)
			break;

		if (token_cmp(tk, pv) > 0) {
			align = 0;
			break;
		}
		pv = tk;
	}
	lexer_peek_leave(lx, &s);

	if (align) {
		indent = dc;
		doc_literal(" ", indent);
	} else {
		indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
		doc_alloc(DOC_HARDLINE, indent);
	}

	for (;;) {
		struct token *comma;

		if (!lexer_peek(lx, &tk) || tk->tk_type == TOKEN_EOF)
			return parser_error(pr);
		if (tk == rbrace)
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));

		if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
			if (parser_exec_decl_braces1(pr, concat, rl))
				return parser_error(pr);
			expr = concat;
		} else {
			if (!lexer_peek_until_loose(lx, TOKEN_COMMA, rbrace,
				    &tk))
				tk = rbrace;

			if (parser_exec_expr(pr, concat, &expr, tk))
				return parser_error(pr);
		}

		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			doc_token(comma, expr);

			if (align) {
				if (lexer_peek(lx, &tk) && tk != rbrace) {
					unsigned int w;

					/*
					 * Let the first column account for the
					 * already emitted the left brace and
					 * space.
					 */
					w = parser_width(pr, concat);
					if (++col == 1)
						w += 2;
					ruler_insert(rl, comma, concat, col, w,
					    0);
				}
			} else {
				struct token *nx;

				if (lexer_peek(lx, &nx) &&
				    token_cmp(nx, tk) > 0)
					line = doc_alloc(DOC_HARDLINE, concat);
				else
					doc_literal(" ", concat);
			}
			if (token_has_line(comma))
				ruler_exec(rl);
		} else {
			line = doc_alloc(DOC_HARDLINE, concat);
		}
	}
	if (line != NULL)
		doc_remove(line, concat);
	if (align)
		doc_literal(" ", dc);
	else
		doc_alloc(DOC_HARDLINE, dc);

out:
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, dc);

	return PARSER_OK;
}

static int
parser_exec_decl_braces_fields(struct parser *pr, struct doc *dc,
    struct ruler *rl, const struct token *rbrace)
{
	struct doc *line = NULL;
	struct doc *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
	doc_alloc(DOC_HARDLINE, indent);

	for (;;) {
		struct doc *concat;

		if (!lexer_peek(lx, &tk) || tk->tk_type == TOKEN_EOF)
			return parser_error(pr);
		if (tk == rbrace) {
			ruler_exec(rl);
			break;
		}

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));
		if (parser_exec_decl_braces_field(pr, concat, rl, rbrace))
			return parser_error(pr);
		line = doc_alloc(DOC_HARDLINE, indent);
	}
	if (line != NULL)
		doc_remove(line, indent);

	return PARSER_OK;
}

static int
parser_exec_decl_braces_field(struct parser *pr, struct doc *dc,
    struct ruler *rl, const struct token *rbrace)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	const struct token *stop;

	for (;;) {
		struct doc *expr;

		if (lexer_if(lx, TOKEN_LSQUARE, &tk)) {
			doc_token(tk, dc);
			if (parser_exec_expr(pr, dc, &expr, NULL))
				return parser_error(pr);
			if (lexer_expect(lx, TOKEN_RSQUARE, &tk))
				doc_token(tk, expr);
		} else if (lexer_if(lx, TOKEN_PERIOD, &tk)) {
			doc_token(tk, dc);
			if (lexer_expect(lx, TOKEN_IDENT, &tk))
				doc_token(tk, dc);
		} else if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			/*
			 * Only applicable to enum declarations which are
			 * allowed to omit any initialization, alignment is not
			 * desired in such scenario.
			 */
			doc_token(tk, dc);
			if (lexer_peek_if(lx, TOKEN_COMMA, NULL))
				goto comma;
		} else {
			break;
		}
	}

	ruler_insert(rl, tk, dc, 1, parser_width(pr, dc), 0);

	stop = rbrace;
	if (lexer_peek_until_loose(lx, TOKEN_COMMA, rbrace, &tk))
		stop = tk;
	if (parser_exec_decl_init(pr, dc, stop, 1))
		return parser_error(pr);

comma:
	if (lexer_if(lx, TOKEN_COMMA, &tk)) {
		doc_token(tk, dc);
		if (token_has_line(tk))
			ruler_exec(rl);
	}

	return PARSER_OK;
}

/*
 * Parse a declaration making use of preprocessor directives such as the ones
 * provided by queue(3):
 *
 * 	TAILQ_HEAD(x, y);
 * 	TAILQ_HEAD(x, y) z;
 * 	TAILQ_HEAD(x, y) z[S];
 * 	TAILQ_HEAD(x, y) *z;
 * 	TAILQ_HEAD(x, y) z = TAILQ_HEAD_INITIALIZER(z);
 *
 * In addition, detect X macros such as the ones provided by RBT_PROTOTYPE(9),
 * note the absence of a trailing semicolon:
 *
 * 	RBT_PROTOTYPE(x, y)
 *
 * In addition, detect various macros:
 *
 * 	UMQ_FIXED_EP_DEF() = {
 */
static int
parser_exec_decl_cpp(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *end, *tk;
	struct doc *expr = dc;
	int iscpp = 0;
	int isxmacro = 0;

	lexer_peek_enter(lx, &s);
	while (lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE,
		    NULL))
		continue;
	if (lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &end)) {
		if (lexer_if(lx, TOKEN_SEMI, NULL)) {
			iscpp = 1;
		} else {
			for (;;) {
				if (!lexer_if(lx, TOKEN_STAR, &tk))
					break;
				end = tk;
			}

			if (lexer_if(lx, TOKEN_IDENT, NULL) &&
			    (lexer_if(lx, TOKEN_LSQUARE, NULL) ||
			     lexer_if(lx, TOKEN_SEMI, NULL) ||
			     lexer_if(lx, TOKEN_EQUAL, NULL)))
				iscpp = 1;
			else if (lexer_if(lx, TOKEN_EQUAL, NULL) &&
			    lexer_if(lx, TOKEN_LBRACE, NULL))
				iscpp = 1;
		}

		/* Handle X macros lacking semicolons. */
		if (!iscpp && lexer_peek(lx, &tk) && token_cmp(tk, end) > 0) {
			iscpp = 1;
			isxmacro = 1;
		}
	}
	lexer_peek_leave(lx, &s);
	if (!iscpp)
		return PARSER_NOTHING;
	if (isxmacro)
		return parser_exec_decl_cppx(pr, dc, rl);

	if (parser_exec_type(pr, dc, end, rl))
		return parser_error(pr);

	if (parser_exec_decl_init(pr, dc, NULL, 0))
		return parser_error(pr);
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, expr);

	return PARSER_OK;
}

/*
 * Parse a preprocessor directive known as a X macro, a construct that looks
 * like a function call but without any trailing semicolon.
 */
static int
parser_exec_decl_cppx(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *rbrace, *tk;
	unsigned int col = 0;
	unsigned int w;

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		doc_token(tk, concat);
	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rbrace))
		return parser_error(pr);
	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, concat);

	/*
	 * Take note of the width of the document up to the first argument, must
	 * be accounted for while performing alignment.
	 */
	w = parser_width(pr, concat);

	for (;;) {
		struct doc *arg, *expr;
		struct token *stop;

		if (lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			break;

		arg = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

		if (!lexer_peek_until_loose(lx, TOKEN_COMMA, rbrace, &stop))
			stop = rbrace;
		if (parser_exec_expr(pr, arg, &expr, stop))
			return parser_error(pr);
		if (lexer_if(lx, TOKEN_COMMA, &tk)) {
			doc_token(tk, expr);
			col++;
			ruler_insert(rl, tk, expr, col,
			    parser_width(pr, arg) + w, 0);
			w = 0;
		}
	}

	if (lexer_expect(lx, TOKEN_RPAREN, &tk))
		doc_token(tk, dc);

	return PARSER_OK;
}

static int
parser_exec_decl_func_ptr(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *tk;

	if (!lexer_peek_func_ptr(lx))
		return PARSER_NOTHING;

	for (;;) {
		if (lexer_peek_if(lx, TOKEN_EOF, NULL))
			return parser_error(pr);

		if (!lexer_pop(lx, &tk))
			return parser_error(pr);
		doc_token(tk, dc);

		if (lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
			break;
	}
	while (parser_exec_func_arg(pr, dc, rparen) == PARSER_OK)
		continue;

	return PARSER_OK;
}

/*
 * Parse an expression. Returns zero if one was found.
 */
static int
parser_exec_expr(struct parser *pr, struct doc *dc, struct doc **expr,
    const struct token *stop)
{
	struct expr_arg ea = {
		.ea_cf		= pr->pr_cf,
		.ea_lx		= pr->pr_lx,
		.ea_dc		= NULL,
		.ea_stop	= stop,
		.ea_recover	= parser_exec_expr_recover,
		.ea_arg		= pr,
	};
	struct doc *ex, *group, *indent;

	group = doc_alloc(DOC_GROUP, dc);
	indent = doc_alloc_indent(pr->pr_cf->cf_sw, group);
	if (pr->pr_expr > 0)
		doc_alloc(DOC_SOFTLINE, indent);
	ea.ea_dc = indent;
	ex = expr_exec(&ea);
	if (ex == NULL) {
		doc_remove(group, dc);
		return PARSER_NOTHING;
	}
	if (expr != NULL)
		*expr = ex;
	return PARSER_OK;
}

/*
 * Parse a function declaration. Returns zero if one was found.
 */
static int
parser_exec_func_decl(struct parser *pr, struct doc *dc, struct ruler *rl,
    struct token *type)
{
	struct doc *noline;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	noline = doc_alloc(DOC_NOLINE, dc);
	if (parser_exec_type(pr, noline, type, rl) ||
	    parser_exec_func_proto(pr, noline, DOC_SOFTLINE))
		return parser_error(pr);

	if (!lexer_if(lx, TOKEN_SEMI, &tk))
		return parser_error(pr);
	doc_token(tk, noline);

	return PARSER_OK;
}

/*
 * Parse a function implementation. Returns zero if one was found.
 */
static int
parser_exec_func_impl(struct parser *pr, struct doc *dc)
{
	struct doc *noline;
	struct lexer *lx = pr->pr_lx;
	struct token *type;

	if (parser_peek_func(pr, &type) != PARSER_PEEK_FUNCIMPL)
		return PARSER_NOTHING;

	noline = doc_alloc(DOC_NOLINE, dc);
	if (parser_exec_type(pr, noline, type, NULL))
		return parser_error(pr);
	if (parser_exec_func_proto(pr, noline, DOC_HARDLINE))
		return parser_error(pr);
	if (!lexer_peek_if(lx, TOKEN_LBRACE, NULL))
		return parser_error(pr);

	doc_alloc(DOC_HARDLINE, dc);
	if (parser_exec_stmt_block(pr, dc, dc))
		return parser_error(pr);
	if (!lexer_peek_if(lx, TOKEN_EOF, NULL))
		doc_alloc(DOC_HARDLINE, dc);

	return PARSER_OK;
}

/*
 * Parse a function prototype, i.e. identifier, arguments and optional attributes.
 * The caller is expected to already have parsed the return type. Returns zero
 * if one was found.
 */
static int
parser_exec_func_proto(struct parser *pr, struct doc *dc,
    enum doc_type linetype)
{
	struct doc *indent, *kr;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *tk;
	int error = 1;

	/*
	 * Hard line implies function implementation in which the identifier
	 * must never be indented.
	 */
	indent = linetype == DOC_HARDLINE ? dc :
	    doc_alloc_indent(pr->pr_cf->cf_sw, dc);
	doc_alloc(linetype, indent);
	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		doc_token(tk, indent);

	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_error(pr);

	indent = doc_alloc_indent(pr->pr_cf->cf_sw, dc);
	while (parser_exec_func_arg(pr, indent, rparen) == PARSER_OK)
		continue;

	/* Recognize K&R argument declarations. */
	kr = doc_alloc(DOC_GROUP, dc);
	indent = doc_alloc_indent(pr->pr_cf->cf_tw, kr);
	doc_alloc(DOC_HARDLINE, indent);
	while (parser_exec_decl(pr, indent, 0) == PARSER_OK)
		error = 0;
	if (error)
		doc_remove(kr, dc);

	parser_exec_attributes(pr, dc, pr->pr_cf->cf_tw, DOC_HARDLINE);

	return PARSER_OK;
}

/*
 * Parse one function argument as part of either a declaration or
 * implementation. Returns zero if an argument was consumed.
 */
static int
parser_exec_func_arg(struct parser *pr, struct doc *dc,
    const struct token *rparen)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *end = NULL;
	struct token *tk;

	/* Consume any left parenthesis before emitting a soft line. */
	if (lexer_if(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, dc);

	/*
	 * Let each argument begin with a soft line, causing a line to be
	 * emitted immediately if the argument does not fit instead of breaking
	 * the argument.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_alloc(DOC_SOFTLINE, concat);

	/* A type will missing when emitting the final right parenthesis. */
	if (lexer_peek_type(lx, &end, 0) &&
	    parser_exec_type(pr, concat, end, NULL) == PARSER_NOTHING)
		return parser_error(pr);

	/* Put the argument identifier in its own group to trigger a refit. */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, concat));

	/*
	 * Put a line between the type and argument identifier unless it's a
	 * pointer or an unnamed argument.
	 */
	if (end != NULL && end->tk_type != TOKEN_STAR &&
	    !lexer_peek_if(lx, TOKEN_COMMA, NULL) &&
	    !lexer_peek_if(lx, TOKEN_RPAREN, NULL))
		doc_alloc(DOC_LINE, concat);

	for (;;) {
		if (lexer_peek_if(lx, TOKEN_EOF, NULL))
			return parser_error(pr);

		if (parser_exec_attributes(pr, concat, 0, DOC_LINE) ==
		    PARSER_OK)
			break;

		if (parser_exec_decl_func_ptr(pr, concat) == PARSER_OK)
			continue;

		if (lexer_if(lx, TOKEN_COMMA, &tk)) {
			doc_token(tk, concat);
			doc_alloc(DOC_LINE, concat);
			return PARSER_OK;
		}

		if (!lexer_pop(lx, &tk))
			return parser_error(pr);
		doc_token(tk, concat);
		if (tk == rparen)
			return PARSER_NOTHING;
	}

	return PARSER_OK;
}

static int
parser_exec_stmt(struct parser *pr, struct doc *dc)
{
	return parser_exec_stmt1(pr, dc, NULL);
}

static int
parser_exec_stmt1(struct parser *pr, struct doc *dc, const struct token *end)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk, *tmp;

	if (!parser_exec_stmt_block(pr, dc, dc))
		return PARSER_OK;

	if (lexer_peek_if(lx, TOKEN_IF, &tk)) {
		int rbrace = 0;

		if (parser_exec_stmt_expr(pr, dc, tk))
			return parser_error(pr);

		if (lexer_back(lx, &tk) && tk->tk_type == TOKEN_RBRACE)
			rbrace = 1;
		if (lexer_if(lx, TOKEN_ELSE, &tk)) {
			if (rbrace)
				doc_literal(" ", dc);
			else
				doc_alloc(DOC_HARDLINE, dc);
			doc_token(tk, dc);
			if (lexer_peek_if(lx, TOKEN_IF, NULL)) {
				doc_literal(" ", dc);
			} else {
				if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
					doc_literal(" ", dc);
				} else {
					dc = doc_alloc_indent(pr->pr_cf->cf_tw,
					    dc);
					doc_alloc(DOC_HARDLINE, dc);
				}
			}
			return parser_exec_stmt1(pr, dc, end);
		}

		return PARSER_OK;
	}

	if (lexer_peek_if(lx, TOKEN_WHILE, &tk) ||
	    lexer_peek_if(lx, TOKEN_SWITCH, &tk) ||
	    lexer_peek_if_flags(lx, TOKEN_FLAG_FOREACH, &tk))
		return parser_exec_stmt_expr(pr, dc, tk);

	if (lexer_if(lx, TOKEN_FOR, &tk)) {
		struct doc *expr = NULL;
		struct doc *loop, *space;

		loop = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, loop);
		doc_literal(" ", loop);

		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, loop);

		/* Declarations are allowed in the first expression. */
		if (parser_exec_decl(pr, loop, 0) == PARSER_NOTHING) {
			/* Let the semicolon hang of the expression unless empty. */
			if (parser_exec_expr(pr, loop, &expr, NULL) ==
			    PARSER_NOTHING)
				expr = loop;
			if (lexer_expect(lx, TOKEN_SEMI, &tk))
				doc_token(tk, expr);
		} else {
			expr = loop;
		}
		space = doc_literal(" ", expr);

		/*
		 * If the expression does not fit, break after the semicolon if
		 * the previous expression was not empty.
		 */
		if (expr != loop)
			pr->pr_expr = 1;
		/* Let the semicolon hang of the expression unless empty. */
		if (parser_exec_expr(pr, loop, &expr, NULL)) {
			/* Expression empty, remove the space. */
			doc_remove(space, expr);
			expr = loop;
		}
		pr->pr_expr = 0;
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
		space = doc_literal(" ", expr);

		/*
		 * If the expression does not fit, break after the semicolon if
		 * the previous expression was not empty.
		 */
		if (expr != loop)
			pr->pr_expr = 1;
		/* Let the semicolon hang of the expression unless empty. */
		if (parser_exec_expr(pr, loop, &expr, NULL)) {
			/* Expression empty, remove the space. */
			doc_remove(space, expr);
			expr = loop;
		}
		pr->pr_expr = 0;
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, expr);

		if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
			doc_literal(" ", expr);
		} else {
			dc = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
			doc_alloc(DOC_HARDLINE, dc);
		}
		return parser_exec_stmt1(pr, dc, end);
	}

	if (lexer_if(lx, TOKEN_CASE, &tk) || lexer_if(lx, TOKEN_DEFAULT, &tk)) {
		struct doc *lhs;

		lhs = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, lhs);
		if (!lexer_peek_until(lx, TOKEN_COLON, &tmp))
			return parser_error(pr);
		if (tk->tk_type == TOKEN_CASE) {
			doc_alloc(DOC_LINE, lhs);
			parser_exec_expr(pr, lhs, NULL, NULL);
		}
		if (lexer_expect(lx, TOKEN_COLON, &tk))
			doc_token(tk, lhs);

		if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
			doc_alloc(DOC_LINE, lhs);
			parser_exec_stmt1(pr, dc, end);
		} else {
			struct doc *indent;

			indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
			for (;;) {
				struct doc *line;
				int dobreak = 0;

				if (lexer_peek_if(lx, TOKEN_CASE, NULL) ||
				    lexer_peek_if(lx, TOKEN_DEFAULT, NULL))
					break;

				if (lexer_peek_if(lx, TOKEN_BREAK, NULL))
					dobreak = 1;
				line = doc_alloc(DOC_HARDLINE, indent);
				if (parser_exec_stmt1(pr, indent, end)) {
					/* No statement, remove the line. */
					doc_remove(line, indent);
					break;
				}
				if (dobreak)
					break;
			}
		}

		return PARSER_OK;
	}

	if (lexer_if(lx, TOKEN_DO, &tk)) {
		int error;

		doc_token(tk, dc);
		if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
			doc_literal(" ", dc);
			error = parser_exec_stmt_block(pr, dc, dc);
			doc_literal(" ", dc);
		} else {
			struct doc *indent;

			indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
			doc_alloc(DOC_HARDLINE, indent);
			error = parser_exec_stmt1(pr, indent, end);
			doc_alloc(DOC_HARDLINE, dc);
		}
		if (error)
			return parser_error(pr);

		pr->pr_dowhile = 1;
		if (lexer_peek_if(lx, TOKEN_WHILE, &tk))
			error = parser_exec_stmt_expr(pr, dc, tk);
		else
			error = parser_error(pr);
		pr->pr_dowhile = 0;
		if (error)
			return parser_error(pr);
		return PARSER_OK;
	}

	if (lexer_if(lx, TOKEN_BREAK, &tk) ||
	    lexer_if(lx, TOKEN_CONTINUE, &tk)) {
		doc_token(tk, dc);
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, dc);
		return PARSER_OK;
	}

	if (lexer_if(lx, TOKEN_RETURN, &tk)) {
		struct doc *concat;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, concat);
		if (lexer_peek_until(lx, TOKEN_SEMI, &tmp)) {
			struct doc *line;

			/*
			 * Only insert a space after the return keyword if a
			 * expression is present.
			 */
			line = doc_literal(" ", concat);
			if (parser_exec_expr(pr, concat, NULL, NULL))
				doc_remove(line, concat);
		}
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, concat);
		return PARSER_OK;
	}

	if (lexer_if(lx, TOKEN_GOTO, &tk)) {
		struct doc *concat;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, concat);
		doc_alloc(DOC_LINE, concat);
		if (lexer_expect(lx, TOKEN_IDENT, &tk))
			doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, concat);
		return PARSER_OK;
	}

	if (parser_exec_stmt_label(pr, dc) == PARSER_OK) {
		struct token *t1, *t2;

		/*
		 * A label is not necessarily followed by a hard line, there
		 * could be another statement on the same line.
		 */
		if (lexer_back(lx, &t1) && lexer_peek(lx, &t2) && t2 != end &&
		    token_cmp(t1, t2) == 0) {
			struct doc *indent;

			indent = doc_alloc_indent(DOC_INDENT_FORCE, dc);
			return parser_exec_stmt1(pr, indent, end);
		}

		return PARSER_OK;
	}

	if (lexer_if(lx, TOKEN_SEMI, &tk)) {
		doc_token(tk, dc);
		return PARSER_OK;
	}

	/*
	 * Note, the ordering of operations is of importance here. Interpret the
	 * following tokens as an expression if the same expression spans to the
	 * first semi colon. Calling parser_exec_decl() first and treating
	 * everything else as an expression has the side effect of treating
	 * function calls as declarations. This in turn is caused by
	 * parser_exec_decl() being able to detect declarations making use of
	 * preprocessor directives such as the ones provided by queue(3).
	 */
	if (!lexer_peek_type(lx, NULL, 0) &&
	    lexer_peek_until_stop(lx, TOKEN_SEMI, end, &tk)) {
		struct expr_arg ea = {
			.ea_cf		= pr->pr_cf,
			.ea_lx		= lx,
			.ea_dc		= NULL,
			.ea_stop	= NULL,
			.ea_recover	= parser_exec_expr_recover,
			.ea_arg		= pr,
		};
		struct lexer_state s;
		struct doc *expr;
		int peek = 0;

		lexer_peek_enter(lx, &s);
		if (expr_peek(&ea, 1) && lexer_peek_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		lexer_peek_leave(lx, &s);

		if (peek && !parser_exec_expr(pr, dc, &expr, NULL)) {
			if (lexer_expect(lx, TOKEN_SEMI, &tk))
				doc_token(tk, expr);
			return PARSER_OK;
		}
	}

	if (!parser_exec_decl(pr, dc, 0))
		return PARSER_OK;

	return PARSER_NOTHING;
}

/*
 * Parse a block statement wrapped in braces.
 */
static int
parser_exec_stmt_block(struct parser *pr, struct doc *head, struct doc *tail)
{
	struct doc *indent, *line;
	struct lexer *lx = pr->pr_lx;
	struct token *end, *tk;
	int nstmt = 0;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &end))
		return PARSER_NOTHING;

	if (lexer_expect(lx, TOKEN_LBRACE, &tk))
		doc_token(tk, head);

	if (pr->pr_switch > 0) {
		/*
		 * Reset the switch indicator in order to get indentation right
		 * for nested blocks.
		 */
		pr->pr_switch = 0;
		indent = tail;
	} else {
		indent = doc_alloc_indent(pr->pr_cf->cf_tw, tail);
	}
	line = doc_alloc(DOC_HARDLINE, indent);
	while (parser_exec_stmt1(pr, indent, end) == PARSER_OK) {
		nstmt++;

		if (lexer_peek_if(lx, TOKEN_RBRACE, &tk) && tk == end)
			break;

		if (lexer_back(lx, &tk) && tk->tk_type == TOKEN_RBRACE &&
		    lexer_peek_if(lx, TOKEN_ELSE, NULL))
			doc_literal(" ", indent);
		else
			doc_alloc(DOC_HARDLINE, indent);
	}
	/* Do not keep the hard line if the statement block is empty. */
	if (nstmt == 0)
		doc_remove(line, indent);

	doc_alloc(DOC_HARDLINE, tail);
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, tail);
	if (lexer_if(lx, TOKEN_SEMI, &tk))
		doc_token(tk, tail);

	return PARSER_OK;
}

/*
 * Parse a statement consisting of a keyword, expression wrapped in parenthesis
 * and following statement(s). Returns zero if such sequence was consumed.
 */
static int
parser_exec_stmt_expr(struct parser *pr, struct doc *dc,
    const struct token *type)
{
	struct doc *expr, *stmt;
	struct lexer *lx = pr->pr_lx;
	struct token *end, *rparen, *tk;

	if (!lexer_expect(lx, type->tk_type, &tk) ||
	    !lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &end))
		return parser_error(pr);

	stmt = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(tk, stmt);
	if ((type->tk_flags & TOKEN_FLAG_FOREACH) == 0)
		doc_literal(" ", stmt);

	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_error(pr);
	/*
	 * The tokens after the expression must be nested underneath the same
	 * expression since we want to fit everything until the following
	 * statement on a single line.
	 */
	if (parser_exec_expr(pr, stmt, &expr, rparen))
		return parser_error(pr);

	if (type->tk_type == TOKEN_WHILE && pr->pr_dowhile > 0) {
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
		return PARSER_OK;
	}

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		int error;

		doc_literal(" ", expr);
		/*
		 * Signal to parser_exec_stmt_block() that no indentation must
		 * be added since that's desired for switch cases.
		 */
		if (type->tk_type == TOKEN_SWITCH)
			pr->pr_switch = 1;
		error = parser_exec_stmt_block(pr, expr, dc);
		return error;
	} else {
		struct doc *indent;

		indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
		doc_alloc(DOC_HARDLINE, indent);
		return parser_exec_stmt(pr, indent);
	}
}

static int
parser_exec_stmt_label(struct parser *pr, struct doc *dc)
{
	struct lexer_state s;
	struct doc *dedent;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &tk) && lexer_if(lx, TOKEN_COLON, &tk))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return PARSER_NOTHING;

	dedent = doc_alloc_dedent(DOC_DEDENT_NONE, dc);
	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		doc_token(tk, dedent);
	if (lexer_expect(lx, TOKEN_COLON, &tk))
		doc_token(tk, dedent);
	return PARSER_OK;
}

/*
 * Parse token(s) denoting a type. Returns zero if a type was consumed.
 */
static int
parser_exec_type(struct parser *pr, struct doc *dc, struct token *end,
    struct ruler *rl)
{
	struct lexer *lx = pr->pr_lx;
	struct token *align = NULL;
	struct token *beg, *tk;
	unsigned int nspaces = 0;

	if (!lexer_peek(lx, &beg))
		return parser_error(pr);

	if (rl != NULL) {
		/*
		 * Find the first non pointer token starting from the end, this
		 * is where the ruler alignment must be performed.
		 */
		align = end;
		for (;;) {
			if (align->tk_type != TOKEN_STAR)
				break;

			nspaces++;

			if (align == beg)
				break;
			align = TAILQ_PREV(align, token_list, tk_entry);
			if (align == NULL)
				break;
		}
	}

	for (;;) {
		struct doc *concat;
		int didalign = 0;

		if (!lexer_pop(lx, &tk))
			return parser_error(pr);

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, concat);

		if (tk == align) {
			ruler_insert(rl, tk, concat, 1, parser_width(pr, dc),
			    nspaces);
			didalign = 1;
		}

		if (tk == end)
			break;

		if (!didalign) {
			struct token *nx;

			if (tk->tk_type != TOKEN_STAR &&
			    tk->tk_type != TOKEN_LPAREN &&
			    lexer_peek(lx, &nx) &&
			    nx->tk_type != TOKEN_LPAREN &&
			    nx->tk_type != TOKEN_RPAREN &&
			    nx->tk_type != TOKEN_COMMA)
				doc_alloc(DOC_LINE, concat);
		}
	}

	return PARSER_OK;
}

static int
parser_exec_attributes(struct parser *pr, struct doc *dc, unsigned int indent,
    enum doc_type linetype)
{
	struct lexer *lx = pr->pr_lx;

	if (!lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		return PARSER_NOTHING;

	if (indent > 0)
		dc = doc_alloc_indent(indent, dc);
	for (;;) {
		struct doc *concat;
		struct token *tk;

		if (!lexer_if(lx, TOKEN_ATTRIBUTE, &tk))
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(linetype, concat);
		doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, concat);
		if (parser_exec_expr(pr, concat, NULL, NULL))
			return parser_error(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, concat);
	}
	return PARSER_OK;
}

/*
 * Callback routine invoked by expression parser while encountering an invalid
 * expression. This can happen while encountering one of the following
 * constructs:
 *
 * 	type argument, i.e. sizeof
 * 	binary operator used as an argument, i.e. timercmp(3)
 * 	cast expression followed by brace initializer
 */
static struct doc *
parser_exec_expr_recover(void *arg)
{
	struct doc *dc = NULL;
	struct parser *pr = arg;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (lexer_if_flags(lx, TOKEN_FLAG_BINARY, &tk)) {
		struct token *pv;

		pv = TAILQ_PREV(tk, token_list, tk_entry);
		if (pv != NULL &&
		    (pv->tk_type == TOKEN_LPAREN ||
		     pv->tk_type == TOKEN_COMMA)) {
			dc = doc_alloc(DOC_CONCAT, NULL);
			doc_token(tk, dc);
		}
	} else if (lexer_peek_type(lx, &tk, 0)) {
		struct token *nx, *pv;

		if (!lexer_peek(lx, &pv))
			return NULL;
		pv = TAILQ_PREV(pv, token_list, tk_entry);
		nx = TAILQ_NEXT(tk, tk_entry);
		if (pv == NULL || nx == NULL)
			return NULL;
		if ((pv->tk_type == TOKEN_LPAREN ||
			    pv->tk_type == TOKEN_COMMA ||
			    pv->tk_type == TOKEN_SIZEOF) &&
		    (nx->tk_type == TOKEN_RPAREN ||
		     nx->tk_type == TOKEN_COMMA)) {
			dc = doc_alloc(DOC_CONCAT, NULL);
			if (parser_exec_type(pr, dc, tk, NULL)) {
				doc_free(dc);
				return NULL;
			}
		}
	} else if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		struct doc *indent;

		/*
		 * Since the document will be appended inside an expression
		 * document, compensate for indentation added by
		 * parser_exec_expr().
		 */
		dc = doc_alloc(DOC_GROUP, NULL);
		indent = doc_alloc_indent(-pr->pr_cf->cf_sw, dc);
		if (parser_exec_decl_braces(pr, indent)) {
			doc_free(dc);
			return NULL;
		}
	}

	return dc;
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
	if (lexer_peek_type(lx, type, 1) && lexer_if(lx, TOKEN_IDENT, NULL) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL)) {
		for (;;) {
			if (!lexer_if(lx, TOKEN_ATTRIBUTE, NULL) ||
			    !lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL))
				break;
		}

		if (lexer_if(lx, TOKEN_SEMI, NULL))
			peek = PARSER_PEEK_FUNCDECL;
		else if (lexer_if(lx, TOKEN_LBRACE, NULL))
			peek = PARSER_PEEK_FUNCIMPL;
		else if (lexer_peek_type(lx, NULL, 1))
			peek = PARSER_PEEK_FUNCIMPL;
	}
	lexer_peek_leave(lx, &s);

	return peek;
}

static int
__parser_error(struct parser *pr, const char *fun, int lno)
{
	struct token *tk;

	if (pr->pr_error)
		return 1;
	pr->pr_error = 1;

#if 0
	doc_exec(pr->pr_dc, pr->pr_bf, pr->pr_cf);
#endif

	fprintf(stderr, "%s: ", pr->pr_path);
	if (pr->pr_cf->cf_verbose > 0)
		fprintf(stderr, "%s:%d: ", fun, lno);
	fprintf(stderr, "error at ");
	if (lexer_back(pr->pr_lx, &tk)) {
		char *str;

		str = token_sprintf(tk);
		fprintf(stderr, "%s\n", str);
		free(str);
	} else {
		fprintf(stderr, "(null)\n");
	}
	return 1;
}

/*
 * Returns the width of the given document.
 */
static unsigned int
parser_width(struct parser *pr, const struct doc *dc)
{
	return doc_width(dc, pr->pr_bf, pr->pr_cf);
}

static int
parser_halted(const struct parser *pr)
{
	return pr->pr_error || lexer_get_error(pr->pr_lx);
}
