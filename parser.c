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
	struct error		*pr_er;
	const struct config	*pr_cf;
	struct lexer		*pr_lx;
	struct buffer		*pr_bf;
	unsigned int		 pr_error;
};

struct parser_exec_func_proto_arg {
	struct doc		*pa_dc;
	struct ruler		*pa_rl;
	const struct token	*pa_type;
	enum doc_type		 pa_line;
	struct doc		*pa_out;
};

#define PARSER_EXEC_DECL_FLAG_BREAK	0x00000001u

#define PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM	0x00000001u

static int	parser_exec_decl(struct parser *, struct doc *, unsigned int);
static int	parser_exec_decl1(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_init(struct parser *, struct doc *,
    const struct token *, int);
static int	parser_exec_decl_braces(struct parser *, struct doc *);
static int	parser_exec_decl_braces1(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_braces_fields(struct parser *, struct doc *,
    struct ruler *, const struct token *, unsigned int);
static int	parser_exec_decl_braces_field(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_decl_cpp(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_cppx(struct parser *, struct doc *,
    struct ruler *);

static int	parser_exec_expr(struct parser *, struct doc *, struct doc **,
    const struct token *, unsigned int flags);

static int	parser_exec_func_decl(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_func_impl(struct parser *, struct doc *);
static int	parser_exec_func_proto(struct parser *,
    struct parser_exec_func_proto_arg *);
static int	parser_exec_func_proto1(struct parser *,
    struct parser_exec_func_proto_arg *);
static int	parser_exec_func_arg(struct parser *, struct doc *,
    struct doc **, const struct token *);

#define PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH	0x00000001u

#define PARSER_EXEC_STMT_EXPR_FLAG_DOWHILE	0x00000001u

static int	parser_exec_stmt(struct parser *, struct doc *);
static int	parser_exec_stmt1(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_block(struct parser *, struct doc *,
    struct doc *, unsigned int);
static int	parser_exec_stmt_expr(struct parser *, struct doc *,
    const struct token *, unsigned int);
static int	parser_exec_stmt_label(struct parser *, struct doc *);
static int	parser_exec_stmt_case(struct parser *, struct doc *,
    const struct token *);

static int	parser_exec_type(struct parser *, struct doc *,
    const struct token *, struct ruler *);

static int	parser_exec_attributes(struct parser *, struct doc *,
    struct doc **, unsigned int, enum doc_type);

static enum parser_peek	parser_peek_func(struct parser *, struct token **);
static int		parser_peek_line(struct parser *, const struct token *);

static unsigned int	parser_width(struct parser *, const struct doc *);

#define parser_error(a) \
	__parser_error((a), __func__, __LINE__)
static int	__parser_error(struct parser *, const char *, int);

static int	parser_halted(const struct parser *);
static int	parser_ok(const struct parser *);
static void	parser_reset(struct parser *);

struct parser *
parser_alloc(const char *path, struct error *er, const struct config *cf)
{
	struct parser *pr;
	struct lexer *lx;

	lx = lexer_alloc(path, er, cf);
	if (lx == NULL)
		return NULL;

	pr = calloc(1, sizeof(*pr));
	if (pr == NULL)
		err(1, NULL);
	pr->pr_path = path;
	pr->pr_er = er;
	pr->pr_cf = cf;
	pr->pr_lx = lx;
	return pr;
}

void
parser_free(struct parser *pr)
{
	if (pr == NULL)
		return;

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
	struct lexer_recover_markers lm;
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	struct token *seek;
	int error = 0;

	pr->pr_bf = buffer_alloc(lexer_get_buffer(lx)->bf_siz);
	dc = doc_alloc(DOC_CONCAT, NULL);

	if (!lexer_peek(lx, &seek))
		seek = NULL;

	lexer_recover_enter(&lm);
	for (;;) {
		struct doc *concat;
		struct token *tk;
		int r;

		concat = doc_alloc(DOC_CONCAT, dc);

		/* Always emit EOF token as it could have dangling tokens. */
		if (lexer_if(lx, TOKEN_EOF, &tk)) {
			doc_token(tk, concat);
			break;
		}

		lexer_recover_mark(lx, &lm);

		if (parser_exec_decl(pr, concat, PARSER_EXEC_DECL_FLAG_BREAK) &&
		    parser_exec_func_impl(pr, concat))
			error = 1;
		else if (parser_halted(pr))
			error = 1;
		else
			doc_alloc(DOC_HARDLINE, concat);

		if (error && (r = lexer_recover(lx, &lm))) {
			while (r-- > 0)
				doc_remove_tail(dc);
			parser_reset(pr);
			error = 0;
		} else if (lexer_branch(lx, &seek, NULL)) {
			lexer_recover_purge(&lm);
			parser_reset(pr);
			error = 0;
		} else if (error) {
			break;
		} else {
			if (!lexer_peek(lx, &seek))
				seek = NULL;
		}
	}
	lexer_recover_leave(&lm);
	if (error) {
		doc_free(dc);
		parser_error(pr);
		return NULL;
	}

	doc_exec(dc, pr->pr_bf, pr->pr_cf);
	doc_free(dc);
	return pr->pr_bf;
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
struct doc *
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
	} else if (lexer_peek_if_type(lx, &tk)) {
		struct token *nx, *pv;

		if (!lexer_peek(lx, &pv))
			return NULL;
		pv = TAILQ_PREV(pv, token_list, tk_entry);
		nx = TAILQ_NEXT(tk, tk_entry);
		if (pv != NULL && nx != NULL &&
		    (pv->tk_type == TOKEN_LPAREN ||
		     pv->tk_type == TOKEN_COMMA ||
		     pv->tk_type == TOKEN_SIZEOF) &&
		    (nx->tk_type == TOKEN_RPAREN ||
		     nx->tk_type == TOKEN_COMMA ||
		     nx->tk_type == TOKEN_EOF)) {
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

static int
parser_exec_decl(struct parser *pr, struct doc *dc, unsigned int flags)
{
	struct lexer_recover_markers lm;
	struct doc *concat;
	struct ruler rl;
	struct doc *line = NULL;
	struct lexer *lx = pr->pr_lx;
	int ndecl = 0;

	concat = doc_alloc(DOC_CONCAT, dc);
	memset(&rl, 0, sizeof(rl));
	ruler_init(&rl);

	lexer_recover_enter(&lm);
	for (;;) {
		struct token *tk;

		lexer_recover_mark(lx, &lm);

		if (parser_exec_decl1(pr, concat, &rl)) {
			int r;

			if (parser_halted(pr) && (r = lexer_recover(lx, &lm))) {
				/*
				 * Let the ruler align what we got so far as
				 * some documents it might refer to are about
				 * the be removed.
				 */
				ruler_exec(&rl);
				while (r-- > 0)
					doc_remove_tail(concat);
				parser_reset(pr);
				continue;
			}

			if (line != NULL)
				doc_remove(line, concat);
			break;
		}
		ndecl++;

		line = doc_alloc(DOC_HARDLINE, concat);

		/*
		 * Honor an empty line which denotes the end of this block of
		 * declarations.
		 */
		if ((flags & PARSER_EXEC_DECL_FLAG_BREAK) &&
		    lexer_back(lx, &tk) && token_has_line(tk, 0))
			break;

		/*
		 * Ending a branch also denotes the end of this block of
		 * declarations.
		 */
		if (lexer_is_branch_end(lx)) {
			doc_remove(line, concat);
			break;
		}

		/* Take the next branch if available. */
		if (lexer_branch(lx, NULL, NULL))
			lexer_recover_purge(&lm);
	}
	lexer_recover_leave(&lm);
	if (ndecl == 0)
		doc_remove(concat, dc);
	else
		ruler_exec(&rl);
	ruler_free(&rl);

	return ndecl > 0 ? parser_ok(pr) : PARSER_NOTHING;
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

	if (!lexer_peek_if_type(lx, &end)) {
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

	if (token_is_decl(end, TOKEN_STRUCT) ||
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
		while (parser_exec_decl(pr, indent, 0) == PARSER_OK)
			continue;

		doc_alloc(DOC_HARDLINE, concat);
		if (lexer_expect(lx, TOKEN_RBRACE, &tk))
			doc_token(tk, concat);

		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
			doc_literal(" ", concat);
	} else if (token_is_decl(end, TOKEN_ENUM)) {
		struct token *rbrace;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_literal(" ", concat);
			doc_token(tk, concat);
		}
		if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
			return parser_error(pr);

		if (parser_exec_decl_braces_fields(pr, concat, rl, rbrace,
		    PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM))
			return parser_error(pr);

		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
			doc_literal(" ", concat);
	}

	if (parser_exec_decl_init(pr, concat, NULL, 0))
		return parser_error(pr);

	parser_exec_attributes(pr, concat, NULL, pr->pr_cf->cf_tw, DOC_LINE);

out:
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);
	return parser_ok(pr);
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
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				doc_literal(" ", dc);
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
				    estop != NULL ? estop : stop, 0))
					return parser_error(pr);
			}
		} else if (lexer_if(lx, TOKEN_LSQUARE, &tk) ||
		    lexer_if(lx, TOKEN_LPAREN, &tk)) {
			enum token_type rhs = tk->tk_type == TOKEN_LSQUARE ?
			    TOKEN_RSQUARE : TOKEN_RPAREN;

			doc_token(tk, dc);
			/* Let the remaning tokens hang of the expression. */
			if (parser_exec_expr(pr, dc, &expr, NULL, 0) ==
			    PARSER_NOTHING)
				expr = dc;
			if (lexer_expect(lx, rhs, &tk))
				doc_token(tk, expr);
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				doc_literal(" ", dc);
		} else if (lexer_if(lx, TOKEN_COLON, &tk)) {
			/* Handle bitfields. */
			doc_token(tk, dc);
			if (lexer_expect(lx, TOKEN_LITERAL, &tk))
				doc_token(tk, dc);
		} else if (lexer_if(lx, TOKEN_COMMA, &tk) ||
		    lexer_if_flags(lx,
		    TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE, &tk)) {
			doc_token(tk, dc);
			doc_literal(" ", dc);
		} else if (lexer_if(lx, TOKEN_STAR, &tk)) {
			doc_token(tk, dc);
		} else {
			break;
		}
	}

	return parser_ok(pr);
}

static int
parser_exec_decl_braces(struct parser *pr, struct doc *dc)
{
	struct ruler rl;
	struct doc *concat;
	int error;

	memset(&rl, 0, sizeof(rl));
	ruler_init(&rl);
	concat = doc_alloc(DOC_CONCAT, dc);
	doc_alloc_optional(1, concat);
	error = parser_exec_decl_braces1(pr, concat, &rl);
	doc_alloc_optional(-1, concat);
	ruler_exec(&rl);
	ruler_free(&rl);
	return error;
}

static int
parser_exec_decl_braces1(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct doc *concat = NULL;
	struct doc *line = NULL;
	struct doc *braces, *expr, *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace, *tk;
	unsigned int col = 0;
	unsigned int w = 0;
	int align = 1;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return parser_error(pr);

	if (parser_exec_decl_braces_fields(pr, dc, rl, rbrace, 0) == PARSER_OK)
		return parser_ok(pr);

	/*
	 * If any column is followed by a hard line, do not align but
	 * instead respect existing hard line(s).
	 */
	if (parser_peek_line(pr, rbrace))
		align = 0;

	braces = doc_alloc(DOC_CONCAT, dc);

	if (lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		doc_token(lbrace, braces);

	if (lexer_peek_if(lx, TOKEN_RBRACE, NULL))
		goto out;

	if (token_has_line(lbrace, 1)) {
		indent = doc_alloc_indent(pr->pr_cf->cf_tw, braces);
		doc_alloc(DOC_HARDLINE, indent);
	} else {
		doc_literal(" ", braces);

		/*
		 * Take note of the width of the document, must be accounted for
		 * while performing alignment.
		 */
		w = parser_width(pr, braces);
		indent = doc_alloc_indent(w, braces);
	}

	for (;;) {
		struct token *comma;

		if (lexer_is_branch(lx))
			break;

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

			if (parser_exec_expr(pr, concat, &expr, tk,
			    EXPR_EXEC_FLAG_NOINDENT))
				return parser_error(pr);
		}

		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			doc_token(comma, expr);

			if (lexer_peek(lx, &tk) && tk == rbrace)
				break;

			if (align) {
				col++;
				ruler_insert(rl, comma, concat, col,
				    parser_width(pr, concat) + w, 0);
				w = 0;
			} else if (!token_has_line(comma, 1)) {
				doc_literal(" ", concat);
			} else {
				doc_alloc(DOC_HARDLINE, concat);
			}
			if (token_has_line(comma, 0))
				ruler_exec(rl);
		} else {
			line = doc_alloc(DOC_HARDLINE, concat);
		}
	}
	if (line != NULL)
		doc_remove(line, concat);

	doc_literal(" ", braces);
out:
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, braces);

	return parser_ok(pr);
}

static int
parser_exec_decl_braces_fields(struct parser *pr, struct doc *dc,
    struct ruler *rl, const struct token *rbrace, unsigned int flags)
{
	struct doc *line;
	struct doc *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if ((flags & PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM) == 0) {
		struct lexer_state s;
		int peek = 0;

		lexer_peek_enter(lx, &s);
		if (lexer_if(lx, TOKEN_LBRACE, NULL) &&
		    (lexer_if(lx, TOKEN_LSQUARE, NULL) ||
		     lexer_if(lx, TOKEN_PERIOD, NULL)))
			peek = 1;
		lexer_peek_leave(lx, &s);
		if (!peek)
			return PARSER_NOTHING;
	}

	if (lexer_expect(lx, TOKEN_LBRACE, &tk))
		doc_token(tk, dc);

	indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
	line = doc_alloc(DOC_HARDLINE, indent);

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

	doc_alloc(DOC_HARDLINE, dc);
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, dc);

	return parser_ok(pr);
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
			if (parser_exec_expr(pr, dc, &expr, NULL, 0))
				return parser_error(pr);
			if (lexer_expect(lx, TOKEN_RSQUARE, &tk))
				doc_token(tk, expr);
		} else if (lexer_if(lx, TOKEN_PERIOD, &tk)) {
			doc_token(tk, dc);
			if (lexer_expect(lx, TOKEN_IDENT, &tk))
				doc_token(tk, dc);
		} else if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_token(tk, dc);

			/*
			 * Only applicable to enum declarations making use of
			 * preprocessor directives.
			 */
			if (lexer_if(lx, TOKEN_LPAREN, &tk)) {
				doc_token(tk, dc);
				if (parser_exec_expr(pr, dc, &expr, NULL, 0) ==
				    PARSER_NOTHING)
					expr = dc;
				if (lexer_expect(lx, TOKEN_RPAREN, &tk))
					doc_token(tk, expr);
			}

			/*
			 * Only applicable to enum declarations which are
			 * allowed to omit any initialization, alignment is not
			 * desired in such scenario.
			 */
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
		if (token_has_line(tk, 0))
			ruler_exec(rl);
	}

	return parser_ok(pr);
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
			struct lexer_state ss;

			for (;;) {
				if (!lexer_if(lx, TOKEN_STAR, &tk))
					break;
				end = tk;
			}

			lexer_peek_enter(lx, &ss);
			if (lexer_if(lx, TOKEN_IDENT, NULL) &&
			    (lexer_if(lx, TOKEN_LSQUARE, NULL) ||
			     lexer_if(lx, TOKEN_SEMI, NULL) ||
			     lexer_if(lx, TOKEN_EQUAL, NULL) ||
			     lexer_if(lx, TOKEN_COMMA, NULL)))
				iscpp = 1;
			else if (lexer_if(lx, TOKEN_EQUAL, NULL) &&
			    lexer_if(lx, TOKEN_LBRACE, NULL))
				iscpp = 1;
			lexer_peek_leave(lx, &ss);
		}

		/*
		 * Detect X macro, must be followed by nothing at all or by
		 * another macro.
		 */
		if (!iscpp &&
		    (lexer_if(lx, TOKEN_EOF, NULL) ||
		     (lexer_if(lx, TOKEN_IDENT, NULL) &&
		      lexer_if(lx, TOKEN_LPAREN, NULL)))) {
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

	return parser_ok(pr);
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
		if (parser_exec_expr(pr, arg, &expr, stop, 0))
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

	return parser_ok(pr);
}

/*
 * Parse an expression. Returns zero if one was found.
 */
static int
parser_exec_expr(struct parser *pr, struct doc *dc, struct doc **expr,
    const struct token *stop, unsigned int flags)
{
	const struct expr_exec_arg ea = {
		.ea_cf		= pr->pr_cf,
		.ea_lx		= pr->pr_lx,
		.ea_dc		= dc,
		.ea_stop	= stop,
		.ea_recover	= parser_exec_expr_recover,
		.ea_arg		= pr,
		.ea_flags	= flags,
	};
	struct doc *ex;

	ex = expr_exec(&ea);
	if (ex == NULL)
		return PARSER_NOTHING;
	if (expr != NULL)
		*expr = ex;
	return parser_ok(pr);
}

/*
 * Parse a function declaration. Returns zero if one was found.
 */
static int
parser_exec_func_decl(struct parser *pr, struct doc *dc, struct ruler *rl,
    const struct token *type)
{
	struct parser_exec_func_proto_arg pa = {
		.pa_dc		= dc,
		.pa_rl		= rl,
		.pa_type	= type,
		.pa_line	= DOC_SOFTLINE,
	};
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (parser_exec_func_proto(pr, &pa))
		return parser_error(pr);

	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, pa.pa_out);

	return parser_ok(pr);
}

/*
 * Parse a function implementation. Returns zero if one was found.
 */
static int
parser_exec_func_impl(struct parser *pr, struct doc *dc)
{
	struct parser_exec_func_proto_arg pa = {
		.pa_dc		= dc,
		.pa_rl		= NULL,
		.pa_type	= NULL,
		.pa_line	= DOC_HARDLINE,
	};
	struct lexer *lx = pr->pr_lx;
	struct token *type;

	if (parser_peek_func(pr, &type) != PARSER_PEEK_FUNCIMPL)
		return PARSER_NOTHING;
	pa.pa_type = type;

	if (parser_exec_func_proto(pr, &pa))
		return parser_error(pr);
	if (!lexer_peek_if(lx, TOKEN_LBRACE, NULL))
		return parser_error(pr);

	doc_alloc(DOC_HARDLINE, dc);
	if (parser_exec_stmt_block(pr, dc, dc, 0))
		return parser_error(pr);
	if (!lexer_peek_if(lx, TOKEN_EOF, NULL))
		doc_alloc(DOC_HARDLINE, dc);

	return parser_ok(pr);
}

/*
 * Parse a function prototype, i.e. return type, identifier, arguments and
 * optional attributes. The caller is expected to already have parsed the
 * return type. Returns zero if one was found.
 */
static int
parser_exec_func_proto(struct parser *pr, struct parser_exec_func_proto_arg *pa)
{
	int error;

	lexer_trim_enter(pr->pr_lx);
	error = parser_exec_func_proto1(pr, pa);
	lexer_trim_leave(pr->pr_lx);
	return error;
}

static int
parser_exec_func_proto1(struct parser *pr,
    struct parser_exec_func_proto_arg *pa)
{
	struct doc *dc = pa->pa_dc;
	struct doc *indent, *kr;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *tk;
	int error = 1;

	if (parser_exec_type(pr, dc, pa->pa_type, pa->pa_rl))
		return parser_error(pr);

	/*
	 * Hard line implies function implementation in which the identifier
	 * must never be indented.
	 */
	indent = pa->pa_line == DOC_HARDLINE ?
	    dc : doc_alloc_indent(pr->pr_cf->cf_sw, dc);
	doc_alloc(pa->pa_line, indent);

	if (pa->pa_type->tk_flags & TOKEN_FLAG_TYPE_FUNC) {
		/* Function returning a function pointer. */
		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, indent);
		if (lexer_expect(lx, TOKEN_STAR, &tk))
			doc_token(tk, indent);
		if (lexer_expect(lx, TOKEN_IDENT, &tk))
			doc_token(tk, indent);
		if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
			return parser_error(pr);
		while (parser_exec_func_arg(pr, indent, NULL, rparen) ==
		    PARSER_OK)
			continue;
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, indent);
	} else if (lexer_expect(lx, TOKEN_IDENT, &tk)) {
		doc_token(tk, indent);
	}

	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_error(pr);

	indent = doc_alloc_indent(pr->pr_cf->cf_sw, dc);
	while (parser_exec_func_arg(pr, indent, &pa->pa_out, rparen) ==
	    PARSER_OK)
		continue;

	/* Recognize K&R argument declarations. */
	kr = doc_alloc(DOC_GROUP, dc);
	indent = doc_alloc_indent(pr->pr_cf->cf_tw, kr);
	doc_alloc(DOC_HARDLINE, indent);
	while (parser_exec_decl(pr, indent, 0) == PARSER_OK)
		error = 0;
	if (error)
		doc_remove(kr, dc);

	parser_exec_attributes(pr, dc, &pa->pa_out, pr->pr_cf->cf_tw,
	    DOC_HARDLINE);

	return parser_ok(pr);
}

/*
 * Parse one function argument as part of either a declaration or
 * implementation. Returns zero if an argument was consumed.
 */
static int
parser_exec_func_arg(struct parser *pr, struct doc *dc, struct doc **out,
    const struct token *rparen)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	struct token *end = NULL;
	struct token *pv = NULL;
	struct token *tk;
	int error = 0;

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
	if (lexer_peek_if_type(lx, &end) &&
	    parser_exec_type(pr, concat, end, NULL) == PARSER_NOTHING)
		return parser_error(pr);

	/* Put the argument identifier in its own group to trigger a refit. */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, concat));
	if (out != NULL)
		*out = concat;

	/* Put a line between the type and identifier when wanted. */
	if (end != NULL && end->tk_type != TOKEN_STAR &&
	    !lexer_peek_if(lx, TOKEN_COMMA, NULL) &&
	    !lexer_peek_if(lx, TOKEN_RPAREN, NULL) &&
	    !lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		doc_alloc(DOC_LINE, concat);

	for (;;) {
		if (lexer_peek_if(lx, TOKEN_EOF, NULL))
			return parser_error(pr);

		if (parser_exec_attributes(pr, concat, NULL, 0, DOC_LINE) ==
		    PARSER_OK)
			break;

		if (lexer_if(lx, TOKEN_COMMA, &tk)) {
			doc_token(tk, concat);
			doc_alloc(DOC_LINE, concat);
			break;
		}

		if (!lexer_pop(lx, &tk)) {
			error = parser_error(pr);
			break;
		}
		/* Identifiers must be separated. */
		if (pv != NULL && pv->tk_type == TOKEN_IDENT &&
		    tk->tk_type == TOKEN_IDENT)
			doc_alloc(DOC_LINE, concat);
		doc_token(tk, concat);
		if (tk == rparen)
			return PARSER_NOTHING;
		pv = tk;
	}

	/* Take the next branch if available. */
	if (lexer_branch(lx, NULL, NULL))
		error = 0;
	return error ? error : parser_ok(pr);
}

static int
parser_exec_stmt(struct parser *pr, struct doc *dc)
{
	return parser_exec_stmt1(pr, dc, NULL);
}

static int
parser_exec_stmt1(struct parser *pr, struct doc *dc, const struct token *stop)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk, *tmp;

	if (parser_exec_stmt_block(pr, dc, dc, 0) == PARSER_OK)
		return parser_ok(pr);

	if (lexer_peek_if(lx, TOKEN_IF, &tk)) {
		struct token *kw;
		int rbrace = 0;

		if (parser_exec_stmt_expr(pr, dc, tk, 0))
			return parser_error(pr);

		if (lexer_back(lx, &tk) && tk->tk_type == TOKEN_RBRACE)
			rbrace = 1;
		if (lexer_if(lx, TOKEN_ELSE, &kw)) {
			struct token *nx;

			if (rbrace)
				doc_literal(" ", dc);
			else
				doc_alloc(DOC_HARDLINE, dc);
			doc_token(kw, dc);
			if (lexer_peek_if(lx, TOKEN_IF, &nx) &&
			    token_cmp(kw, nx) == 0) {
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
			return parser_exec_stmt1(pr, dc, stop);
		}

		return parser_ok(pr);
	}

	if (lexer_peek_if(lx, TOKEN_WHILE, &tk) ||
	    lexer_peek_if(lx, TOKEN_SWITCH, &tk))
		return parser_exec_stmt_expr(pr, dc, tk, 0);

	if (lexer_if(lx, TOKEN_FOR, &tk)) {
		struct doc *expr = NULL;
		struct doc *loop, *space;
		unsigned int flags;

		loop = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, loop);
		doc_literal(" ", loop);

		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, loop);

		/* Declarations are allowed in the first expression. */
		if (parser_exec_decl(pr, loop, 0) == PARSER_NOTHING) {
			/* Let the semicolon hang of the expression unless empty. */
			if (parser_exec_expr(pr, loop, &expr, NULL, 0) ==
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
		flags = expr != loop ? EXPR_EXEC_FLAG_SOFTLINE : 0;
		/* Let the semicolon hang of the expression unless empty. */
		if (parser_exec_expr(pr, loop, &expr, NULL, flags)) {
			/* Expression empty, remove the space. */
			doc_remove(space, expr);
			expr = loop;
		}
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
		space = doc_literal(" ", expr);

		if (lexer_is_branch(lx)) {
			doc_alloc(DOC_HARDLINE, loop);
			return parser_ok(pr);
		}

		/*
		 * If the expression does not fit, break after the semicolon if
		 * the previous expression was not empty.
		 */
		flags = expr != loop ? EXPR_EXEC_FLAG_SOFTLINE : 0;
		/* Let the semicolon hang of the expression unless empty. */
		if (parser_exec_expr(pr, loop, &expr, NULL, flags)) {
			/* Expression empty, remove the space. */
			doc_remove(space, expr);
			expr = loop;
		}
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, expr);

		if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
			doc_literal(" ", expr);
		} else {
			dc = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
			doc_alloc(DOC_HARDLINE, dc);
		}
		return parser_exec_stmt1(pr, dc, stop);
	}

	if (parser_exec_stmt_case(pr, dc, stop) == PARSER_OK)
		return parser_ok(pr);

	if (lexer_if(lx, TOKEN_DO, &tk)) {
		int error;

		doc_token(tk, dc);
		if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
			doc_literal(" ", dc);
			error = parser_exec_stmt_block(pr, dc, dc, 0);
			doc_literal(" ", dc);
		} else {
			struct doc *indent;

			indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
			doc_alloc(DOC_HARDLINE, indent);
			error = parser_exec_stmt1(pr, indent, stop);
			doc_alloc(DOC_HARDLINE, dc);
		}
		if (error)
			return parser_error(pr);

		if (lexer_peek_if(lx, TOKEN_WHILE, &tk))
			return parser_exec_stmt_expr(pr, dc, tk,
			    PARSER_EXEC_STMT_EXPR_FLAG_DOWHILE);
		return parser_error(pr);
	}

	if (lexer_if(lx, TOKEN_BREAK, &tk) ||
	    lexer_if(lx, TOKEN_CONTINUE, &tk)) {
		doc_token(tk, dc);
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, dc);
		return parser_ok(pr);
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
			if (parser_exec_expr(pr, concat, NULL, NULL, 0))
				doc_remove(line, concat);
		}
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, concat);
		return parser_ok(pr);
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
		return parser_ok(pr);
	}

	if (parser_exec_stmt_label(pr, dc) == PARSER_OK) {
		struct token *t1, *t2;

		/*
		 * A label is not necessarily followed by a hard line, there
		 * could be another statement on the same line.
		 */
		if (lexer_back(lx, &t1) && lexer_peek(lx, &t2) && t2 != stop &&
		    token_cmp(t1, t2) == 0) {
			struct doc *indent;

			indent = doc_alloc_indent(DOC_INDENT_FORCE, dc);
			return parser_exec_stmt1(pr, indent, stop);
		}

		return parser_ok(pr);
	}

	if (lexer_if(lx, TOKEN_SEMI, &tk)) {
		doc_token(tk, dc);
		return parser_ok(pr);
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
	if (!lexer_peek_if_type(lx, NULL) &&
	    lexer_peek_until_stop(lx, TOKEN_SEMI, stop, &tk)) {
		const struct expr_exec_arg ea = {
			.ea_cf		= pr->pr_cf,
			.ea_lx		= lx,
			.ea_dc		= NULL,
			.ea_stop	= NULL,
			.ea_recover	= parser_exec_expr_recover,
			.ea_arg		= pr,
			.ea_flags	= 0,
		};
		struct lexer_state s;
		struct doc *expr;
		int peek = 0;

		lexer_peek_enter(lx, &s);
		if (expr_peek(&ea) && lexer_peek_if(lx, TOKEN_SEMI, NULL))
			peek = 1;
		lexer_peek_leave(lx, &s);

		if (peek) {
			if (parser_exec_expr(pr, dc, &expr, NULL, 0))
				return parser_error(pr);
			if (lexer_expect(lx, TOKEN_SEMI, &tk))
				doc_token(tk, expr);
			if (lexer_is_branch(lx))
				doc_alloc(DOC_HARDLINE, dc);
			return parser_ok(pr);
		}
	}

	if (parser_exec_decl(pr, dc, PARSER_EXEC_DECL_FLAG_BREAK) == PARSER_OK)
		return parser_ok(pr);

	/*
	 * Last resort, see if this is a loop construct hidden behind cpp such
	 * as queue(3).
	 */
	if (lexer_peek_if(lx, TOKEN_IDENT, &tk))
		return parser_exec_stmt_expr(pr, dc, tk, 0);

	return PARSER_NOTHING;
}

/*
 * Parse a block statement wrapped in braces.
 */
static int
parser_exec_stmt_block(struct parser *pr, struct doc *head, struct doc *tail,
    unsigned int flags)
{
	struct doc *indent, *line;
	struct lexer *lx = pr->pr_lx;
	struct token *rbrace, *seek, *pv, *tk;
	int nstmt = 0;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return PARSER_NOTHING;

	/* Do not honor empty lines before the closing right brace. */
	pv = TAILQ_PREV(rbrace, token_list, tk_entry);
	if (pv != NULL)
		token_trim(pv, TOKEN_SPACE, 0);

	if (lexer_expect(lx, TOKEN_LBRACE, &tk))
		doc_token(tk, head);

	if (!lexer_peek(lx, &seek))
		seek = NULL;

	indent = flags & PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH ? tail :
	    doc_alloc_indent(pr->pr_cf->cf_tw, tail);
	line = doc_alloc(DOC_HARDLINE, indent);
	while (parser_exec_stmt1(pr, indent, rbrace) == PARSER_OK) {
		nstmt++;

		if (lexer_branch(lx, &seek, NULL))
			continue;
		if (!lexer_peek(lx, &seek))
			seek = NULL;

		if (lexer_peek_if(lx, TOKEN_RBRACE, &tk) && tk == rbrace)
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

	return parser_ok(pr);
}

/*
 * Parse a statement consisting of a keyword, expression wrapped in parenthesis
 * and following statement(s). Returns zero if such sequence was consumed.
 */
static int
parser_exec_stmt_expr(struct parser *pr, struct doc *dc,
    const struct token *type, unsigned int flags)
{
	struct doc *expr, *stmt;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *stop, *tk;

	if (!lexer_expect(lx, type->tk_type, &tk) ||
	    !lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_error(pr);

	stmt = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(tk, stmt);
	if (type->tk_type != TOKEN_IDENT)
		doc_literal(" ", stmt);

	/*
	 * The tokens after the expression must be nested underneath the same
	 * expression since we want to fit everything until the following
	 * statement on a single line.
	 */
	stop = TAILQ_NEXT(rparen, tk_entry);
	if (parser_exec_expr(pr, stmt, &expr, stop, EXPR_EXEC_FLAG_PARENS))
		return parser_error(pr);

	if (lexer_is_branch(lx))
		return parser_ok(pr);

	if (flags & PARSER_EXEC_STMT_EXPR_FLAG_DOWHILE) {
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
		return parser_ok(pr);
	}

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		int isswitch = type->tk_type == TOKEN_SWITCH;

		doc_literal(" ", expr);
		return parser_exec_stmt_block(pr, expr, dc,
		    isswitch ? PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH : 0);
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
	return parser_ok(pr);
}

static int
parser_exec_stmt_case(struct parser *pr, struct doc *dc,
    const struct token *stop)
{
	struct doc *indent, *lhs;
	struct lexer *lx = pr->pr_lx;
	struct token *kw, *tk;

	if (!lexer_if(lx, TOKEN_CASE, &kw) && !lexer_if(lx, TOKEN_DEFAULT, &kw))
		return PARSER_NOTHING;

	lhs = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(kw, lhs);
	if (!lexer_peek_until(lx, TOKEN_COLON, NULL))
		return parser_error(pr);
	if (kw->tk_type == TOKEN_CASE) {
		doc_alloc(DOC_LINE, lhs);
		if (parser_exec_expr(pr, lhs, NULL, NULL, 0) == PARSER_NOTHING)
			return parser_error(pr);
	}
	if (lexer_expect(lx, TOKEN_COLON, &tk))
		doc_token(tk, lhs);

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_alloc(DOC_LINE, lhs);
		return parser_exec_stmt1(pr, dc, stop);
	}

	indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
	for (;;) {
		struct doc *line;
		struct token *nx;

		if (lexer_peek_if(lx, TOKEN_CASE, NULL) ||
		    lexer_peek_if(lx, TOKEN_DEFAULT, NULL))
			break;

		if (!lexer_peek(lx, &nx))
			return parser_error(pr);

		/*
		 * Allow following statement(s) to be placed on the same line as
		 * the case/default keyword.
		 */
		if (token_cmp(kw, nx) == 0)
			line = doc_literal(" ", indent);
		else
			line = doc_alloc(DOC_HARDLINE, indent);

		if (parser_exec_stmt1(pr, indent, stop)) {
			/* No statement, remove the line. */
			doc_remove(line, indent);
			break;
		}
		if (nx->tk_type == TOKEN_BREAK)
			break;
	}

	return parser_ok(pr);
}

/*
 * Parse token(s) denoting a type. Returns zero if a type was consumed.
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
		return parser_error(pr);

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
			align = TAILQ_PREV(align, token_list, tk_entry);
			if (align == NULL)
				break;
		}
	}

	for (;;) {
		struct doc *concat;
		struct token *tk;
		int didalign = 0;

		if (!lexer_pop(lx, &tk))
			return parser_error(pr);

		if (tk->tk_flags & TOKEN_FLAG_TYPE_ARGS) {
			struct doc *indent;

			doc_token(tk, dc);
			indent = doc_alloc_indent(pr->pr_cf->cf_sw, dc);
			while (parser_exec_func_arg(pr, indent, NULL, end) ==
			    PARSER_OK)
				continue;
			break;
		}

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

	return parser_ok(pr);
}

static int
parser_exec_attributes(struct parser *pr, struct doc *dc, struct doc **out,
    unsigned int indent, enum doc_type linetype)
{
	struct doc *concat = NULL;
	struct lexer *lx = pr->pr_lx;

	if (!lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		return PARSER_NOTHING;

	if (indent > 0)
		dc = doc_alloc_indent(indent, dc);
	for (;;) {
		struct token *tk;

		if (!lexer_if(lx, TOKEN_ATTRIBUTE, &tk))
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(linetype, concat);
		doc_token(tk, concat);
		if (lexer_expect(lx, TOKEN_LPAREN, &tk))
			doc_token(tk, concat);
		if (parser_exec_expr(pr, concat, NULL, NULL, 0))
			return parser_error(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, concat);
	}
	if (out != NULL)
		*out = concat;

	return parser_ok(pr);
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
	if (lexer_if_type(lx, type)) {
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

		for (;;) {
			if (!lexer_if(lx, TOKEN_ATTRIBUTE, NULL) ||
			    !lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL))
				break;
		}

		if (lexer_if(lx, TOKEN_SEMI, NULL))
			peek = PARSER_PEEK_FUNCDECL;
		else if (lexer_if(lx, TOKEN_LBRACE, NULL))
			peek = PARSER_PEEK_FUNCIMPL;
		else if (lexer_if_type(lx, NULL))
			peek = PARSER_PEEK_FUNCIMPL;	/* K&R */
	}
out:
	lexer_peek_leave(lx, &s);
	return peek;
}

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
			return parser_error(pr);
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

static int
__parser_error(struct parser *pr, const char *fun, int lno)
{
	struct token *tk;

	if (parser_halted(pr))
		return 1;
	pr->pr_error = 1;

	error_write(pr->pr_er, "%s: ", pr->pr_path);
	if (pr->pr_cf->cf_verbose > 0)
		error_write(pr->pr_er, "%s:%d: ", fun, lno);
	error_write(pr->pr_er, "%s", "error at ");
	if (lexer_back(pr->pr_lx, &tk)) {
		char *str;

		str = token_sprintf(tk);
		error_write(pr->pr_er, "%s\n", str);
		free(str);
	} else {
		error_write(pr->pr_er, "%s", "(null)\n");
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

static int
parser_ok(const struct parser *pr)
{
	if (lexer_get_error(pr->pr_lx))
		return PARSER_NOTHING;
	return PARSER_OK;
}

static void
parser_reset(struct parser *pr)
{
	error_reset(pr->pr_er);
	pr->pr_error = 0;
}
