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
	PARSER_PEEK_ELSE	= 3,
	PARSER_PEEK_ELSEIF	= 4,
	PARSER_PEEK_CPPX	= 5,
	PARSER_PEEK_CPPINIT	= 6,
};

struct parser {
	const struct file	*pr_fe;
	struct error		*pr_er;
	const struct config	*pr_cf;
	struct lexer		*pr_lx;
	struct buffer		*pr_bf;
	unsigned int		 pr_error;
};

struct parser_exec_func_proto_arg {
	struct doc		*pf_dc;
	struct ruler		*pf_rl;
	const struct token	*pf_type;
	enum doc_type		 pf_line;
	struct doc		*pf_out;
};

struct parser_exec_decl_braces_arg {
	struct doc	*pb_dc;
	struct ruler	*pb_rl;
	unsigned int	 pb_col;
};

#define PARSER_EXEC_DECL_FLAG_BREAK	0x00000001u
#define PARSER_EXEC_DECL_FLAG_LINE	0x00000002u

#define PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM	0x00000001u
#define PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_TRIM	0x00000002u

static int	parser_exec_decl(struct parser *, struct doc *, unsigned int);
static int	parser_exec_decl1(struct parser *, struct doc *,
    struct ruler *);
static int	parser_exec_decl_init(struct parser *, struct doc *,
    const struct token *, int);
static int	parser_exec_decl_braces(struct parser *, struct doc *);
static int	parser_exec_decl_braces1(struct parser *,
    struct parser_exec_decl_braces_arg *);
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
#define PARSER_EXEC_STMT_BLOCK_FLAG_TRIM	0x00000002u

#define PARSER_EXEC_STMT_EXPR_FLAG_DOWHILE	0x00000001u
#define PARSER_EXEC_STMT_EXPR_FLAG_ELSE		0x00000002u

static int	parser_exec_stmt(struct parser *, struct doc *,
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

static enum parser_peek	parser_peek_cppx(struct parser *);
static enum parser_peek	parser_peek_cpp_init(struct parser *);
static enum parser_peek	parser_peek_func(struct parser *, struct token **);
static enum parser_peek	parser_peek_else(struct parser *, struct token **);
static int		parser_peek_line(struct parser *, const struct token *);

static void		parser_trim_brace(struct token *);
static unsigned int	parser_width(struct parser *, const struct doc *);

#define parser_error(a) \
	__parser_error((a), __func__, __LINE__)
static int	__parser_error(struct parser *, const char *, int);

static int	parser_halted(const struct parser *);
static int	parser_ok(const struct parser *);
static void	parser_reset(struct parser *);

struct parser *
parser_alloc(const struct file *fe, struct error *er, const struct config *cf)
{
	struct parser *pr;
	struct lexer *lx;

	lx = lexer_alloc(fe, er, cf);
	if (lx == NULL)
		return NULL;

	pr = calloc(1, sizeof(*pr));
	if (pr == NULL)
		err(1, NULL);
	pr->pr_fe = fe;
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

		if (parser_exec_decl(pr, concat,
		    PARSER_EXEC_DECL_FLAG_BREAK | PARSER_EXEC_DECL_FLAG_LINE) &&
		    parser_exec_func_impl(pr, concat))
			error = 1;
		else if (parser_halted(pr))
			error = 1;

		if (error && (r = lexer_recover(lx, &lm))) {
			while (r-- > 0)
				doc_remove_tail(dc);
			parser_reset(pr);
			error = 0;
		} else if (lexer_branch(lx, &seek)) {
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

	doc_exec(dc, pr->pr_lx, pr->pr_bf, pr->pr_cf);
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
parser_exec_expr_recover(unsigned int flags, void *arg)
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
	} else if (lexer_peek_if_type(lx, &tk,
	    (flags & EXPR_RECOVER_FLAG_CAST) ? LEXER_TYPE_FLAG_CAST : 0)) {
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
	struct doc *decl;
	struct ruler rl;
	struct doc *line = NULL;
	struct lexer *lx = pr->pr_lx;
	int ndecl = 0;

	decl = doc_alloc(DOC_CONCAT, dc);
	memset(&rl, 0, sizeof(rl));
	ruler_init(&rl);

	lexer_recover_enter(&lm);
	for (;;) {
		struct doc *concat, *group;
		struct token *tk;

		lexer_recover_mark(lx, &lm);

		group = doc_alloc(DOC_GROUP, decl);
		concat = doc_alloc(DOC_CONCAT, group);
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
				doc_remove(line, decl);
			doc_remove(group, decl);
			break;
		}
		ndecl++;

		line = doc_alloc(DOC_HARDLINE, decl);

		if (flags & PARSER_EXEC_DECL_FLAG_BREAK) {
			/*
			 * Honor empty line(s) which denotes the end of this
			 * block of declarations.
			 */
			if (lexer_back(lx, &tk) && token_has_line(tk, 0))
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

		/* Take the next branch if available. */
		if (lexer_branch(lx, NULL))
			lexer_recover_purge(&lm);
	}
	lexer_recover_leave(&lm);
	if (ndecl == 0)
		doc_remove(decl, dc);
	else
		ruler_exec(&rl);
	ruler_free(&rl);

	if (ndecl == 0)
		return PARSER_NOTHING;

	if (flags & PARSER_EXEC_DECL_FLAG_LINE)
		doc_alloc(DOC_HARDLINE, decl);
	return parser_ok(pr);
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

	if (!lexer_peek_if_type(lx, &end, 0)) {
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
		struct token *lbrace, *rbrace;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_literal(" ", concat);
			doc_token(tk, concat);
		}

		if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
			return parser_error(pr);
		parser_trim_brace(rbrace);
		if (lexer_expect(lx, TOKEN_LBRACE, &lbrace)) {
			token_trim(lbrace, TOKEN_SPACE, 0);
			doc_token(lbrace, concat);
		}

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
		parser_trim_brace(rbrace);

		if (parser_exec_decl_braces_fields(pr, concat, rl, rbrace,
		    PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM |
		    PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_TRIM))
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
		struct token *assign, *tk;

		if (parser_halted(pr))
			return parser_error(pr);

		if (lexer_peek(lx, &tk) && tk == stop)
			break;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_token(tk, dc);
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				doc_literal(" ", dc);
		} else if (lexer_if_flags(lx, TOKEN_FLAG_ASSIGN, &assign)) {
			if (!didalign)
				doc_literal(" ", dc);
			doc_token(assign, dc);
			doc_literal(" ", dc);

			if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
				if (parser_exec_decl_braces(pr, dc))
					return parser_error(pr);
			} else {
				struct token *estop = NULL;
				unsigned int flags = 0;

				if (stop == NULL)
					(void)lexer_peek_until_loose(lx,
					    TOKEN_COMMA, NULL, &estop);

				/*
				 * Honor hard line after assignment which must
				 * be emitted inside the expression document to
				 * get indentation right.
				 */
				if (token_has_line(assign, 1))
					flags |= EXPR_EXEC_FLAG_HARDLINE;

				if (parser_exec_expr(pr, dc, NULL,
				    estop != NULL ? estop : stop, flags))
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
		} else if (lexer_if(lx, TOKEN_COMMA, &tk)) {
			doc_token(tk, dc);
			doc_alloc(DOC_LINE, dc);
		} else if (lexer_if_flags(lx,
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
	struct parser_exec_decl_braces_arg pb;
	struct ruler rl;
	struct doc *concat, *optional;
	int error;

	memset(&rl, 0, sizeof(rl));
	ruler_init(&rl);
	optional = doc_alloc_optional(DOC_OPTIONAL_STICKY, dc);
	concat = doc_alloc(DOC_CONCAT, optional);
	pb.pb_dc = concat;
	pb.pb_rl = &rl;
	pb.pb_col = 0;
	error = parser_exec_decl_braces1(pr, &pb);
	ruler_exec(&rl);
	ruler_free(&rl);
	return error;
}

static int
parser_exec_decl_braces1(struct parser *pr,
    struct parser_exec_decl_braces_arg *pb)
{
	struct doc *line = NULL;
	struct doc *braces, *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace, *tk;
	unsigned int w = 0;
	int align = 1;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return parser_error(pr);

	if (parser_exec_decl_braces_fields(pr, pb->pb_dc, pb->pb_rl,
	    rbrace, 0) == PARSER_OK)
		return parser_ok(pr);

	/*
	 * If any column is followed by a hard line, do not align but
	 * instead respect existing hard line(s).
	 */
	if (parser_peek_line(pr, rbrace))
		align = 0;

	braces = doc_alloc(DOC_CONCAT, pb->pb_dc);

	if (!lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		return parser_error(pr);
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
		struct doc *expr = NULL;
		struct doc *concat;
		struct token *comma, *nx;

		if (lexer_is_branch(lx))
			break;

		if (!lexer_peek(lx, &tk) || tk->tk_type == TOKEN_EOF)
			return parser_error(pr);
		if (tk == rbrace)
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));

		if (lexer_peek_if(lx, TOKEN_LBRACE, &nx)) {
			struct doc *dc = pb->pb_dc;
			unsigned int col = pb->pb_col;

			pb->pb_dc = concat;
			if (parser_exec_decl_braces1(pr, pb))
				return parser_error(pr);
			pb->pb_dc = dc;
			/*
			 * If the nested braces are positioned on the same line
			 * as the braces currently being parsed, do not restore
			 * the column as we're still on the same row in terms of
			 * alignment.
			 */
			if (token_cmp(lbrace, nx) < 0)
				pb->pb_col = col;

			expr = concat;
		} else if (parser_peek_cppx(pr) || parser_peek_cpp_init(pr)) {
			if (parser_exec_decl_cppx(pr, concat, pb->pb_rl))
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
				pb->pb_col++;
				w += parser_width(pr, concat);
				ruler_insert(pb->pb_rl, comma, concat,
				    pb->pb_col, w, 0);
				w = 0;
			} else if (!token_has_line(comma, 1)) {
				doc_literal(" ", concat);
			} else {
				doc_alloc(DOC_HARDLINE, indent);
			}
			if (token_has_line(comma, 0))
				ruler_exec(pb->pb_rl);
		} else {
			line = doc_alloc(DOC_HARDLINE, indent);
		}
	}
	if (line != NULL)
		doc_remove(line, indent);

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
	struct doc *line = NULL;
	struct doc *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *tk;
	int doline;

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

	if (!lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		return parser_error(pr);
	doline = token_has_line(lbrace, 1);
	if (flags & PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_TRIM)
		token_trim(lbrace, TOKEN_SPACE, 0);
	doc_token(lbrace, dc);

	indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
	if (doline)
		line = doc_alloc(DOC_HARDLINE, indent);
	else
		doc_literal(" ", indent);

	for (;;) {
		struct doc *concat;

		if (lexer_is_branch(lx))
			break;

		if (!lexer_peek(lx, &tk) || tk->tk_type == TOKEN_EOF)
			return parser_error(pr);
		if (tk == rbrace) {
			ruler_exec(rl);
			break;
		}

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));
		if (parser_exec_decl_braces_field(pr, concat, rl, rbrace))
			return parser_error(pr);
		if (doline)
			line = doc_alloc(DOC_HARDLINE, indent);
		else
			line = doc_literal(" ", indent);
	}
	if (line != NULL)
		doc_remove(line, indent);

	if (doline)
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
	struct token *tk = NULL;
	struct token *comma;
	const struct token *stop;

	stop = lexer_peek_until_loose(lx, TOKEN_COMMA, rbrace, &comma) ?
	    comma : rbrace;

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
			if (tk == NULL) {
				/*
				 * Nothing found so far, the field name could be
				 * omitted.
				 */
				(void)parser_exec_expr(pr, dc, NULL, stop, 0);
				goto comma;
			}
			break;
		}
	}

	ruler_insert(rl, tk, dc, 1, parser_width(pr, dc), 0);

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
 * In addition, detect various macros:
 *
 * 	UMQ_FIXED_EP_DEF() = {
 */
static int
parser_exec_decl_cpp(struct parser *pr, struct doc *dc, struct ruler *rl)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *end, *ident, *tk;
	struct doc *expr = dc;
	int semi = 1;
	int iscpp = 0;

	lexer_peek_enter(lx, &s);
	while (lexer_if_flags(lx, TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE,
	    NULL))
		continue;
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
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
			     lexer_if(lx, TOKEN_EQUAL, NULL) ||
			     lexer_if(lx, TOKEN_COMMA, NULL)))
				iscpp = 1;
			else if (lexer_if(lx, TOKEN_EQUAL, NULL) &&
			    lexer_if(lx, TOKEN_LBRACE, NULL))
				iscpp = 1;
		}
	}
	lexer_peek_leave(lx, &s);
	if (!iscpp) {
		if (parser_peek_cppx(pr))
			return parser_exec_decl_cppx(pr, dc, rl);
		return PARSER_NOTHING;
	}

	if (parser_exec_type(pr, dc, end, rl))
		return parser_error(pr);

	if (parser_exec_decl_init(pr, dc, NULL, 0))
		return parser_error(pr);
	if (semi && lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, expr);

	return parser_ok(pr);
}

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
			w += parser_width(pr, arg);
			ruler_insert(rl, tk, expr, ++col, w, 0);
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
	struct parser_exec_func_proto_arg pf = {
		.pf_rl		= rl,
		.pf_type	= type,
		.pf_line	= DOC_SOFTLINE,
	};
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	pf.pf_dc = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	if (parser_exec_func_proto(pr, &pf))
		return parser_error(pr);

	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, pf.pf_out);

	return parser_ok(pr);
}

/*
 * Parse a function implementation. Returns zero if one was found.
 */
static int
parser_exec_func_impl(struct parser *pr, struct doc *dc)
{
	struct parser_exec_func_proto_arg pf = {
		.pf_dc		= dc,
		.pf_rl		= NULL,
		.pf_type	= NULL,
		.pf_line	= DOC_HARDLINE,
	};
	struct lexer *lx = pr->pr_lx;
	struct token *type;

	if (parser_peek_func(pr, &type) != PARSER_PEEK_FUNCIMPL)
		return PARSER_NOTHING;
	pf.pf_type = type;

	if (parser_exec_func_proto(pr, &pf))
		return parser_error(pr);
	if (!lexer_peek_if(lx, TOKEN_LBRACE, NULL))
		return parser_error(pr);

	doc_alloc(DOC_HARDLINE, dc);
	if (parser_exec_stmt_block(pr, dc, dc, 0))
		return parser_error(pr);

	doc_alloc(DOC_HARDLINE, dc);
	if (!lexer_peek_if(lx, TOKEN_EOF, NULL)) {
		struct token *tk, *px;

		if (!lexer_back(lx, &tk) ||
		    !lexer_peek_if_prefix_flags(lx, TOKEN_FLAG_CPP, &px) ||
		    px->tk_lno - tk->tk_lno > 1)
			doc_alloc(DOC_HARDLINE, dc);
	}

	return parser_ok(pr);
}

/*
 * Parse a function prototype, i.e. return type, identifier, arguments and
 * optional attributes. The caller is expected to already have parsed the
 * return type. Returns zero if one was found.
 */
static int
parser_exec_func_proto(struct parser *pr, struct parser_exec_func_proto_arg *pf)
{
	int error;

	lexer_trim_enter(pr->pr_lx);
	error = parser_exec_func_proto1(pr, pf);
	lexer_trim_leave(pr->pr_lx);
	return error;
}

static int
parser_exec_func_proto1(struct parser *pr,
    struct parser_exec_func_proto_arg *pf)
{
	struct doc *dc = pf->pf_dc;
	struct doc *concat, *indent, *kr;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *tk;
	int error = 1;
	int isimpl = pf->pf_line == DOC_HARDLINE;

	if (parser_exec_type(pr, dc, pf->pf_type, pf->pf_rl))
		return parser_error(pr);
	/*
	 * Hard line after the return type is only wanted for function
	 * implementations.
	 */
	if (isimpl)
		doc_alloc(DOC_HARDLINE, dc);

	/*
	 * The function identifier and arguments are intended to fit on a single
	 * line.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	indent = isimpl ? concat : doc_alloc_indent(pr->pr_cf->cf_sw, concat);

	if (pf->pf_type->tk_flags & TOKEN_FLAG_TYPE_FUNC) {
		/* Function returning a function pointer. */
		if (!isimpl)
			doc_alloc(pf->pf_line, indent);
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
		if (isimpl) {
			doc_token(tk, indent);
		} else {
			struct doc *ident;

			ident = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_GROUP, indent));
			doc_alloc(pf->pf_line, ident);
			doc_token(tk, ident);
		}
	}

	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_error(pr);

	indent = doc_alloc_indent(pr->pr_cf->cf_sw, concat);
	while (parser_exec_func_arg(pr, indent, &pf->pf_out, rparen) ==
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

	parser_exec_attributes(pr, dc, &pf->pf_out, pr->pr_cf->cf_tw,
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
	if (lexer_peek_if_type(lx, &end, LEXER_TYPE_FLAG_ARG) &&
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
	if (lexer_branch(lx, NULL))
		error = 0;
	return error ? error : parser_ok(pr);
}

static int
parser_exec_stmt(struct parser *pr, struct doc *dc, const struct token *stop)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk, *tmp;

	if (parser_exec_stmt_block(pr, dc, dc,
	    PARSER_EXEC_STMT_BLOCK_FLAG_TRIM) == PARSER_OK)
		return parser_ok(pr);

	if (lexer_peek_if(lx, TOKEN_IF, &tk)) {
		struct token *tkelse;
		enum parser_peek peek;

		if (parser_exec_stmt_expr(pr, dc, tk, 0))
			return parser_error(pr);

		while ((peek = parser_peek_else(pr, &tkelse))) {
			lexer_back(lx, &tk);

			if (lexer_back(lx, &tk) && tk->tk_type == TOKEN_RBRACE)
				doc_literal(" ", dc);
			else
				doc_alloc(DOC_HARDLINE, dc);

			if (peek == PARSER_PEEK_ELSEIF) {
				if (parser_exec_stmt_expr(pr, dc, tkelse,
				    PARSER_EXEC_STMT_EXPR_FLAG_ELSE))
					return parser_error(pr);
			} else {
				if (lexer_expect(lx, TOKEN_ELSE, &tk))
					doc_token(tk, dc);
				if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
					doc_literal(" ", dc);
				} else {
					dc = doc_alloc_indent(pr->pr_cf->cf_tw,
					    dc);
					doc_alloc(DOC_HARDLINE, dc);
				}
				if (parser_exec_stmt(pr, dc, stop))
					return parser_error(pr);
			}
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
		return parser_exec_stmt(pr, dc, stop);
	}

	if (parser_exec_stmt_case(pr, dc, stop) == PARSER_OK)
		return parser_ok(pr);

	if (lexer_if(lx, TOKEN_DO, &tk)) {
		int error;

		doc_token(tk, dc);
		if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
			doc_literal(" ", dc);
			error = parser_exec_stmt_block(pr, dc, dc,
			    PARSER_EXEC_STMT_BLOCK_FLAG_TRIM);
			doc_literal(" ", dc);
		} else {
			struct doc *indent;

			indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
			doc_alloc(DOC_HARDLINE, indent);
			error = parser_exec_stmt(pr, indent, stop);
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
		struct token *nx;

		/*
		 * A label is not necessarily followed by a hard line, there
		 * could be another statement on the same line.
		 */
		if (lexer_back(lx, &tk) && lexer_peek(lx, &nx) && nx != stop &&
		    token_cmp(tk, nx) == 0) {
			struct doc *indent;

			indent = doc_alloc_indent(DOC_INDENT_FORCE, dc);
			return parser_exec_stmt(pr, indent, stop);
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
	if (!lexer_peek_if_type(lx, NULL, 0) &&
	    lexer_peek_until_stop(lx, TOKEN_SEMI, stop, &tk)) {
		const struct expr_exec_arg ea = {
			.ea_cf		= pr->pr_cf,
			.ea_lx		= lx,
			.ea_dc		= NULL,
			.ea_stop	= tk,
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
	struct doc *concat, *indent, *line;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace, *seek, *tk;
	int nstmt = 0;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return PARSER_NOTHING;
	parser_trim_brace(rbrace);

	if (lexer_expect(lx, TOKEN_LBRACE, &lbrace)) {
		/*
		 * Optionally remove empty lines after the opening left brace.
		 * An empty line is however allowed in the beginning of a
		 * function implementation, a convention used by some when the
		 * function lacks local variables.
		 */
		if (flags & PARSER_EXEC_STMT_BLOCK_FLAG_TRIM)
			token_trim(lbrace, TOKEN_SPACE, 0);
		doc_token(lbrace, head);
	}

	if (!lexer_peek(lx, &seek))
		seek = NULL;

	indent = flags & PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH ? tail :
	    doc_alloc_indent(pr->pr_cf->cf_tw, tail);
	line = doc_alloc(DOC_HARDLINE, indent);
	while (parser_exec_stmt(pr, indent, rbrace) == PARSER_OK) {
		nstmt++;

		if (lexer_branch(lx, &seek))
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

	/*
	 * The right brace and any following statement is expected to fit on a
	 * single line.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, tail));
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, concat);
	if (lexer_if(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);

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
	struct token *tkelse = NULL;
	struct token *rparen, *stop, *tk;

	if (flags & PARSER_EXEC_STMT_EXPR_FLAG_ELSE) {
		if (lexer_expect(lx, TOKEN_ELSE, &tk))
			tkelse = tk;
	}

	if (!lexer_expect(lx, type->tk_type, &tk) ||
	    !lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_error(pr);
	/* Never break after the right parenthesis. */
	token_trim(rparen, TOKEN_SPACE, TOKEN_FLAG_OPTLINE);

	stmt = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	if (tkelse != NULL) {
		doc_token(tkelse, stmt);
		doc_literal(" ", stmt);
	}
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
		unsigned int fflags = PARSER_EXEC_STMT_BLOCK_FLAG_TRIM;

		if (type->tk_type == TOKEN_SWITCH)
			fflags |= PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH;
		doc_literal(" ", expr);
		return parser_exec_stmt_block(pr, expr, dc, fflags);
	} else {
		struct doc *indent;

		indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
		doc_alloc(DOC_HARDLINE, indent);
		return parser_exec_stmt(pr, indent, NULL);
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

	dedent = doc_alloc_dedent(dc);
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
		return parser_exec_stmt(pr, dc, stop);
	}

	indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
	for (;;) {
		struct doc *line;
		struct token *nx;

		if (lexer_is_branch(lx))
			break;

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

		if (parser_exec_stmt(pr, indent, stop)) {
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
 * Returns non-zero if the next tokens denotes a X macro. That is, something
 * that looks like a function call but is not followed by a semicolon nor comma
 * if being part of an initializer. One example are the macros provided by
 * RBT_PROTOTYPE(9).
 */
static enum parser_peek
parser_peek_cppx(struct parser *pr)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident, *rparen;
	enum parser_peek peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen)) {
		const struct token *nx, *pv;

		/*
		 * The previous token must not reside on the same line as the
		 * identifier. The next token must reside on the next line and
		 * have the same or less indentation. This is of importance in
		 * order to not confuse loop constructs hidden behind cpp.
		 */
		pv = TAILQ_PREV(ident, token_list, tk_entry);
		nx = TAILQ_NEXT(rparen, tk_entry);
		if ((pv == NULL || token_cmp(pv, ident) < 0) &&
		    (nx == NULL || (token_cmp(nx, rparen) > 0 &&
		     nx->tk_cno <= ident->tk_cno)))
			peek = PARSER_PEEK_CPPX;
	}
	lexer_peek_leave(lx, &s);
	return peek;
}

/*
 * Returns non-zero if the next tokens denotes an initializer making use of cpp.
 */
static enum parser_peek
parser_peek_cpp_init(struct parser *pr)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *comma, *ident, *rparen;
	enum parser_peek peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen) &&
	    lexer_if(lx, TOKEN_COMMA, &comma)) {
		const struct token *nx, *pv;

		pv = TAILQ_PREV(ident, token_list, tk_entry);
		nx = TAILQ_NEXT(comma, tk_entry);
		if (pv != NULL && token_cmp(pv, ident) < 0 &&
		    nx != NULL && token_cmp(nx, comma) > 0)
			peek = PARSER_PEEK_CPPINIT;
	}
	lexer_peek_leave(lx, &s);
	return peek;
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

		for (;;) {
			if (!lexer_if(lx, TOKEN_ATTRIBUTE, NULL) ||
			    !lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL))
				break;
		}

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

static enum parser_peek
parser_peek_else(struct parser *pr, struct token **tk)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *tkelse, *tkif;
	enum parser_peek peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_ELSE, &tkelse)) {
		if (lexer_if(lx, TOKEN_IF, &tkif) &&
		    token_cmp(tkelse, tkif) == 0) {
			peek = PARSER_PEEK_ELSEIF;
			*tk = tkif;
		} else {
			peek = PARSER_PEEK_ELSE;
			*tk = tkelse;
		}
	}
	lexer_peek_leave(lx, &s);
	return peek;
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

	error_write(pr->pr_er, "%s: ", pr->pr_fe->fe_path);
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
 * Trim hard line(s) from the given right brace as part of block, unless the
 * right brace is preceeded with prefixes intended to be emitted.
 */
static void
parser_trim_brace(struct token *rbrace)
{
	struct token *pv;

	pv = TAILQ_PREV(rbrace, token_list, tk_entry);
	if (pv != NULL &&
	    !token_has_prefix(rbrace, TOKEN_COMMENT) &&
	    !token_has_prefix(rbrace, TOKEN_CPP))
		token_trim(pv, TOKEN_SPACE, 0);
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
