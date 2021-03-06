#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

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
	PARSER_PEEK_CPPX	= 3,
	PARSER_PEEK_CPPINIT	= 4,
};

struct parser {
	const char		*pr_path;
	struct error		*pr_er;
	const struct config	*pr_cf;
	struct lexer		*pr_lx;
	struct buffer		*pr_bf;
	unsigned int		 pr_error;
	unsigned int		 pr_nblocks;	/* # stmt blocks */
	unsigned int		 pr_nindent;	/* # indented stmt blocks */

	struct {
		struct simple_stmt	*se_stmt;
		struct simple_decl	*se_decl;
		int			 se_nstmt;
		int			 se_ndecl;
	} pr_simple;
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

struct parser_exec_stmt_block_arg {
	struct doc	*ps_head;
	struct doc	*ps_tail;
	struct doc	*ps_rbrace;
	unsigned int	 ps_flags;
#define PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH	0x00000001u
#define PARSER_EXEC_STMT_BLOCK_FLAG_TRIM	0x00000002u
};

/* Honor grouped declarations. */
#define PARSER_EXEC_DECL_FLAG_BREAK	0x00000001u
/* Emit hard line after declaration(s). */
#define PARSER_EXEC_DECL_FLAG_LINE	0x00000002u
/* Parsing of declarations on root level. */
#define PARSER_EXEC_DECL_FLAG_ROOT	0x00000004u

#define PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM	0x00000001u
#define PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_TRIM	0x00000002u

static int	parser_exec_decl(struct parser *, struct doc *, unsigned int);
static int	parser_exec_decl1(struct parser *, struct doc *, unsigned int);
static int	parser_exec_decl2(struct parser *, struct doc *,
    struct ruler *, unsigned int);
static int	parser_exec_decl_init(struct parser *, struct doc *,
    struct ruler *, const struct token *, int);
static int	parser_exec_decl_braces(struct parser *, struct doc *);
static int	parser_exec_decl_braces1(struct parser *,
    struct parser_exec_decl_braces_arg *);
static int	parser_exec_decl_braces_fields(struct parser *, struct doc *,
    const struct token *, unsigned int);
static int	parser_exec_decl_braces_field(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_decl_cpp(struct parser *, struct doc *,
    struct ruler *, unsigned int);
static int	parser_exec_decl_cppx(struct parser *, struct doc *,
    struct ruler *);

static int	parser_exec_expr(struct parser *, struct doc *, struct doc **,
    const struct token *, unsigned int flags);

static int	parser_exec_func_decl(struct parser *, struct doc *,
    struct ruler *, const struct token *);
static int	parser_exec_func_impl(struct parser *, struct doc *);
static int	parser_exec_func_proto(struct parser *,
    struct parser_exec_func_proto_arg *);
static int	parser_exec_func_arg(struct parser *, struct doc *,
    struct doc **, const struct token *);

#define PARSER_EXEC_STMT_EXPR_FLAG_DOWHILE	0x00000001u

static int	parser_exec_stmt(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt1(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_block(struct parser *,
    struct parser_exec_stmt_block_arg *);
static int	parser_exec_stmt_if(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_for(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_dowhile(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_return(struct parser *, struct doc *);
static int	parser_exec_stmt_expr(struct parser *, struct doc *,
    const struct token *, unsigned int);
static int	parser_exec_stmt_label(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_case(struct parser *, struct doc *,
    const struct token *);
static int	parser_exec_stmt_goto(struct parser *, struct doc *);

static int	parser_exec_type(struct parser *, struct doc *,
    const struct token *, struct ruler *);

static int	parser_exec_attributes(struct parser *, struct doc *,
    struct doc **, unsigned int, enum doc_type);

static int	parser_simple_active(const struct parser *);

static int	parser_simple_decl_active(const struct parser *);
static int	parser_simple_decl_enter(struct parser *, unsigned int);
static void	parser_simple_decl_leave(struct parser *, int);

static int		 parser_simple_stmt_enter(struct parser *,
    const struct token *);
static void		 parser_simple_stmt_leave(struct parser *, int);
static struct doc	*parser_simple_stmt_block(struct parser *,
    struct doc *);
static void		*parser_simple_stmt_ifelse_enter(struct parser *);
static void		 parser_simple_stmt_ifelse_leave(struct parser *,
    void *);

static enum parser_peek	parser_peek_cppx(struct parser *);
static enum parser_peek	parser_peek_cpp_init(struct parser *);
static enum parser_peek	parser_peek_func(struct parser *, struct token **);
static int		parser_peek_expr(struct parser *, const struct token *);
static int		parser_peek_line(struct parser *, const struct token *);

static void		parser_trim_brace(struct token *);
static unsigned int	parser_width(struct parser *, const struct doc *);

#define parser_fail(a) \
	__parser_fail((a), __func__, __LINE__)
static int	__parser_fail(struct parser *, const char *, int);

static int	parser_get_error(const struct parser *);
static int	parser_good(const struct parser *);
static int	parser_none(const struct parser *);
static void	parser_reset(struct parser *);

struct parser *
parser_alloc(const char *path, struct lexer *lx, struct error *er,
    const struct config *cf)
{
	struct parser *pr;

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

	buffer_free(pr->pr_bf);
	free(pr);
}

const struct buffer *
parser_exec(struct parser *pr)
{
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	int error = 0;

	pr->pr_bf = buffer_alloc(lexer_get_buffer(lx)->bf_siz);
	dc = doc_alloc(DOC_CONCAT, NULL);

	for (;;) {
		struct doc *concat;
		struct token *tk;

		concat = doc_alloc(DOC_CONCAT, dc);

		/* Always emit EOF token as it could have dangling tokens. */
		if (lexer_if(lx, TOKEN_EOF, &tk)) {
			doc_token(tk, concat);
			error = 0;
			break;
		}

		error = parser_exec_decl(pr, concat,
		    PARSER_EXEC_DECL_FLAG_BREAK | PARSER_EXEC_DECL_FLAG_LINE |
		    PARSER_EXEC_DECL_FLAG_ROOT);
		if (error == NONE)
			error = parser_exec_func_impl(pr, concat);

		if (error & GOOD) {
			lexer_stamp(lx);
		} else if (error & BRCH) {
			if (!lexer_branch(lx))
				break;
			parser_reset(pr);
		} else if (error & (FAIL | NONE)) {
			int r;

			r = lexer_recover(lx);
			if (r > 0) {
				while (r-- > 0)
					doc_remove_tail(dc);
				parser_reset(pr);
			} else {
				break;
			}
		}
	}
	if (error) {
		doc_free(dc);
		parser_fail(pr);
		return NULL;
	}

	doc_exec(dc, pr->pr_lx, pr->pr_bf, pr->pr_cf, 0);
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
			int error;

			dc = doc_alloc(DOC_CONCAT, NULL);
			error = parser_exec_type(pr, dc, tk, NULL);
			if (error & (FAIL | NONE)) {
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
		if (parser_exec_decl_braces(pr, indent) & (FAIL | NONE)) {
			doc_free(dc);
			return NULL;
		}
	}

	return dc;
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
	ruler_init(&rl, 0);

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

		if (flags & PARSER_EXEC_DECL_FLAG_BREAK) {
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

	if (flags & PARSER_EXEC_DECL_FLAG_LINE)
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
		simple_decl_type(pr->pr_simple.se_decl, beg, end);
	if (parser_exec_type(pr, concat, end, rl) & (FAIL | NONE))
		return parser_fail(pr);

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
			return parser_fail(pr);
		parser_trim_brace(rbrace);
		if (lexer_expect(lx, TOKEN_LBRACE, &lbrace)) {
			token_trim(lbrace);
			doc_token(lbrace, concat);
		}

		indent = doc_alloc_indent(pr->pr_cf->cf_tw, concat);
		doc_alloc(DOC_HARDLINE, indent);
		while (parser_exec_decl(pr, indent, 0) & GOOD)
			continue;
		doc_alloc(DOC_HARDLINE, concat);

		if (lexer_expect(lx, TOKEN_RBRACE, &tk))
			doc_token(tk, concat);

		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL) &&
		    !lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
			doc_literal(" ", concat);
	} else if (token_is_decl(end, TOKEN_ENUM)) {
		struct token *rbrace;
		int error;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_literal(" ", concat);
			doc_token(tk, concat);
		}

		if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
			return parser_fail(pr);
		parser_trim_brace(rbrace);

		error = parser_exec_decl_braces_fields(pr, concat, rbrace,
		    PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_ENUM |
		    PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_TRIM);
		if (error & (FAIL | NONE))
			return parser_fail(pr);

		if (!lexer_peek_if(lx, TOKEN_SEMI, NULL))
			doc_literal(" ", concat);
	}

	if (!lexer_peek_until(lx, TOKEN_SEMI, &semi))
		return parser_fail(pr);
	if (parser_exec_decl_init(pr, concat, rl, semi, 0) & (FAIL | NONE))
		return parser_fail(pr);

	parser_exec_attributes(pr, concat, NULL, pr->pr_cf->cf_tw, DOC_LINE);

out:
	if (lexer_expect(lx, TOKEN_SEMI, &semi)) {
		doc_token(semi, concat);
		if (parser_simple_decl_active(pr))
			simple_decl_semi(pr->pr_simple.se_decl, semi);
	}
	return parser_good(pr);
}

/*
 * Parse any initialization as part of a declaration.
 */
static int
parser_exec_decl_init(struct parser *pr, struct doc *dc, struct ruler *rl,
    const struct token *semi, int didalign)
{
	struct doc *concat;
	struct lexer *lx = pr->pr_lx;
	int error;

	dc = doc_alloc(DOC_CONCAT, ruler_indent(rl, dc));
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	for (;;) {
		struct doc *expr = NULL;
		struct token *assign, *comma, *tk;

		if (lexer_peek(lx, &tk) && tk == semi)
			break;

		if (lexer_if(lx, TOKEN_IDENT, &tk)) {
			doc_token(tk, concat);
			if (lexer_peek_if(lx, TOKEN_IDENT, NULL))
				doc_literal(" ", concat);
		} else if (lexer_if_flags(lx, TOKEN_FLAG_ASSIGN, &assign)) {
			struct doc *dedent;

			if (!didalign)
				doc_literal(" ", concat);
			doc_token(assign, concat);
			doc_literal(" ", concat);

			dedent = doc_alloc(DOC_CONCAT,
			    ruler_dedent(rl, concat));
			if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
				error = parser_exec_decl_braces(pr, dedent);
				if (error & (FAIL | NONE))
					return parser_fail(pr);
			} else {
				const struct token *stop = semi;
				unsigned int flags = 0;

				/*
				 * Honor hard line after assignment which must
				 * be emitted inside the expression document to
				 * get indentation right.
				 */
				if (token_has_line(assign, 1))
					flags |= EXPR_EXEC_FLAG_HARDLINE;

				if (lexer_peek_until_loose(lx, TOKEN_COMMA,
				    semi, &tk))
					stop = tk;
				error = parser_exec_expr(pr, dedent, NULL,
				    stop, flags);
				if (error & HALT)
					return parser_fail(pr);
			}
		} else if (lexer_if(lx, TOKEN_LSQUARE, &tk) ||
		    lexer_if(lx, TOKEN_LPAREN, &tk)) {
			enum token_type rhs = tk->tk_type == TOKEN_LSQUARE ?
			    TOKEN_RSQUARE : TOKEN_RPAREN;

			doc_token(tk, concat);
			/* Let the remaning tokens hang of the expression. */
			error = parser_exec_expr(pr, concat, &expr, NULL, 0);
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
				simple_decl_comma(pr->pr_simple.se_decl, comma);
			concat = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_GROUP, dc));
		} else if (lexer_if_flags(lx,
		    TOKEN_FLAG_QUALIFIER | TOKEN_FLAG_STORAGE, &tk)) {
			doc_token(tk, concat);
			doc_literal(" ", concat);
		} else if (lexer_if(lx, TOKEN_STAR, &tk)) {
			doc_token(tk, concat);
		} else {
			break;
		}
	}

	return parser_good(pr);
}

static int
parser_exec_decl_braces(struct parser *pr, struct doc *dc)
{
	struct parser_exec_decl_braces_arg pb;
	struct ruler rl;
	struct doc *concat;
	int error;

	ruler_init(&rl, 0);
	concat = doc_alloc(DOC_CONCAT, dc);
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
	struct doc *braces, *indent;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace, *tk;
	unsigned int w = 0;
	int align = 1;
	int error;

	if (!lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return parser_fail(pr);

	error = parser_exec_decl_braces_fields(pr, pb->pb_dc, rbrace, 0);
	if (error & GOOD)
		return parser_good(pr);

	/*
	 * If any column is followed by a hard line, do not align but
	 * instead respect existing hard line(s).
	 */
	if (parser_peek_line(pr, rbrace))
		align = 0;

	braces = doc_alloc(DOC_CONCAT, pb->pb_dc);

	if (!lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		return parser_fail(pr);
	doc_token(lbrace, braces);

	if (lexer_peek_if(lx, TOKEN_RBRACE, NULL))
		goto out;

	if (token_has_line(lbrace, 1)) {
		indent = doc_alloc_indent(pr->pr_cf->cf_tw, braces);
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

		if (!lexer_peek(lx, &tk) || tk->tk_type == TOKEN_EOF)
			return parser_fail(pr);
		if (tk == rbrace)
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));

		if (lexer_peek_if(lx, TOKEN_LBRACE, &nx)) {
			struct doc *dc = pb->pb_dc;
			unsigned int col = pb->pb_col;

			pb->pb_dc = concat;
			if (parser_exec_decl_braces1(pr, pb) & HALT)
				return parser_fail(pr);
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
			error = parser_exec_decl_cppx(pr, concat, pb->pb_rl);
			if (error & HALT)
				return parser_fail(pr);
			expr = concat;
		} else {
			if (!lexer_peek_until_loose(lx, TOKEN_COMMA, rbrace,
			    &tk))
				tk = rbrace;

			error = parser_exec_expr(pr, concat, &expr, tk,
			    EXPR_EXEC_FLAG_NOINDENT);
			if (error & HALT)
				return parser_fail(pr);
		}

		if (lexer_if(lx, TOKEN_COMMA, &comma)) {
			doc_token(comma, expr);

			if (align) {
				pb->pb_col++;
				w += parser_width(pr, concat);
				ruler_insert(pb->pb_rl, comma, concat,
				    pb->pb_col, w, 0);
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
			if (token_cmp(pv, rbrace) < 0)
				doc_alloc(DOC_HARDLINE, braces);
		} else {
			doc_alloc(DOC_HARDLINE, indent);
		}

next:
		if (lexer_back(lx, &nx) && token_has_line(nx, 2))
			ruler_exec(pb->pb_rl);
	}

out:
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, braces);

	return parser_good(pr);
}

static int
parser_exec_decl_braces_fields(struct parser *pr, struct doc *dc,
    const struct token *rbrace, unsigned int flags)
{
	struct ruler rl;
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
			return parser_none(pr);
	}

	ruler_init(&rl, 0);

	if (!lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		return parser_fail(pr);
	doline = token_has_line(lbrace, 1);
	/* Avoid side effects in simple mode. */
	if ((flags & PARSER_EXEC_DECL_BRACES_FIELDS_FLAG_TRIM) &&
	    !parser_simple_active(pr))
		token_trim(lbrace);
	doc_token(lbrace, dc);

	indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
	if (doline)
		line = doc_alloc(DOC_HARDLINE, indent);
	else
		doc_literal(" ", indent);

	for (;;) {
		struct doc *concat;
		int error;

		if (!lexer_peek(lx, &tk) || tk->tk_type == TOKEN_EOF) {
			ruler_free(&rl);
			return parser_fail(pr);
		}
		if (tk == rbrace)
			break;

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, indent));
		error = parser_exec_decl_braces_field(pr, concat, &rl, rbrace);
		if (error & HALT) {
			ruler_free(&rl);
			return parser_fail(pr);
		}
		if (doline)
			line = doc_alloc(DOC_HARDLINE, indent);
		else
			doc_literal(" ", indent);
	}
	if (line != NULL)
		doc_remove(line, indent);
	ruler_exec(&rl);
	ruler_free(&rl);

	if (doline)
		doc_alloc(DOC_HARDLINE, dc);
	if (lexer_expect(lx, TOKEN_RBRACE, &tk))
		doc_token(tk, dc);

	return parser_good(pr);
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
		struct doc *expr = NULL;
		int error;

		if (lexer_if(lx, TOKEN_LSQUARE, &tk)) {
			doc_token(tk, dc);
			error = parser_exec_expr(pr, dc, &expr, NULL, 0);
			if (error & HALT)
				return parser_fail(pr);
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
				error = parser_exec_expr(pr, dc, &expr,
				    NULL, 0);
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

	if (parser_exec_decl_init(pr, dc, rl, stop, 1) & (FAIL | NONE))
		return parser_fail(pr);

comma:
	if (lexer_if(lx, TOKEN_COMMA, &tk))
		doc_token(tk, dc);
	else if (!lexer_peek(lx, &tk) || tk != stop)
		return parser_fail(pr);

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
	int iscpp = 0;

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
			iscpp = 1;
		else if ((flags & PARSER_EXEC_DECL_FLAG_ROOT) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			iscpp = 1;
		else if (lexer_peek_until(lx, TOKEN_IDENT, &ident) &&
		    token_cmp(macro, ident) == 0)
			iscpp = 1;
		else if (lexer_if(lx, TOKEN_IDENT, NULL) &&
		    lexer_if(lx, TOKEN_SEMI, NULL))
			iscpp = 1;
	}
	lexer_peek_leave(lx, &s);
	if (!iscpp) {
		if (parser_peek_cppx(pr))
			return parser_exec_decl_cppx(pr, dc, rl);
		return parser_none(pr);
	}

	if (parser_exec_type(pr, dc, end, rl) & (FAIL | NONE))
		return parser_fail(pr);

	if (!lexer_peek_until(lx, TOKEN_SEMI, &semi))
		return parser_fail(pr);
	if (parser_exec_decl_init(pr, dc, rl, semi, 0) & (FAIL | NONE))
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
	struct token *rbrace, *tk;
	unsigned int col = 0;
	unsigned int w;

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

	if (lexer_expect(lx, TOKEN_IDENT, &tk))
		doc_token(tk, concat);
	if (!lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rbrace))
		return parser_fail(pr);
	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, concat);

	/*
	 * Take note of the width of the document up to the first argument, must
	 * be accounted for while performing alignment.
	 */
	w = parser_width(pr, concat);

	for (;;) {
		struct doc *expr = NULL;
		struct doc *arg;
		struct token *stop;
		int error;

		if (lexer_peek_if(lx, TOKEN_RPAREN, NULL))
			break;

		arg = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));

		if (!lexer_peek_until_loose(lx, TOKEN_COMMA, rbrace, &stop))
			stop = rbrace;
		error = parser_exec_expr(pr, arg, &expr, stop, 0);
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
		return parser_none(pr);
	if (expr != NULL)
		*expr = ex;
	return parser_good(pr);
}

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
	if (parser_exec_func_proto(pr, &pf) & (FAIL | NONE))
		return parser_fail(pr);

	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, pf.pf_out);

	return parser_good(pr);
}

static int
parser_exec_func_impl(struct parser *pr, struct doc *dc)
{
	struct parser_exec_func_proto_arg pf = {
		.pf_dc		= dc,
		.pf_rl		= NULL,
		.pf_type	= NULL,
		.pf_line	= DOC_HARDLINE,
	};
	struct parser_exec_stmt_block_arg ps = {
		.ps_head	= dc,
		.ps_tail	= dc,
		.ps_flags	= 0,
		.ps_rbrace	= NULL,
	};
	struct lexer *lx = pr->pr_lx;
	struct token *type;

	if (parser_peek_func(pr, &type) != PARSER_PEEK_FUNCIMPL)
		return parser_none(pr);
	pf.pf_type = type;

	if (parser_exec_func_proto(pr, &pf) & (FAIL | NONE))
		return parser_fail(pr);
	if (!lexer_peek_if(lx, TOKEN_LBRACE, NULL))
		return parser_fail(pr);

	doc_alloc(DOC_HARDLINE, dc);
	if (parser_exec_stmt_block(pr, &ps) & (FAIL | NONE))
		return parser_fail(pr);

	doc_alloc(DOC_HARDLINE, dc);
	if (!lexer_peek_if(lx, TOKEN_EOF, NULL)) {
		struct token *px, *tk;

		if (!lexer_back(lx, &tk) ||
		    !lexer_peek_if_prefix_flags(lx, TOKEN_FLAG_CPP, &px) ||
		    px->tk_lno - tk->tk_lno > 1)
			doc_alloc(DOC_HARDLINE, dc);
	}

	return parser_good(pr);
}

/*
 * Parse a function prototype, i.e. return type, identifier, arguments and
 * optional attributes. The caller is expected to already have parsed the
 * return type.
 */
static int
parser_exec_func_proto(struct parser *pr, struct parser_exec_func_proto_arg *pf)
{
	struct doc *dc = pf->pf_dc;
	struct doc *concat, *indent, *kr;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *tk;
	int error = 1;
	int isimpl = pf->pf_line == DOC_HARDLINE;

	if (parser_exec_type(pr, dc, pf->pf_type, pf->pf_rl) & (FAIL | NONE))
		return parser_fail(pr);
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
			return parser_fail(pr);
		while (parser_exec_func_arg(pr, indent, NULL, rparen) & GOOD)
			continue;
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, indent);
	} else if (lexer_expect(lx, TOKEN_IDENT, &tk)) {
		token_trim(tk);
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
		return parser_fail(pr);
	parser_trim_brace(rparen);

	indent = doc_alloc_indent(pr->pr_cf->cf_sw, concat);
	while (parser_exec_func_arg(pr, indent, &pf->pf_out, rparen) & GOOD)
		continue;

	/* Recognize K&R argument declarations. */
	kr = doc_alloc(DOC_GROUP, dc);
	indent = doc_alloc_indent(pr->pr_cf->cf_tw, kr);
	doc_alloc(DOC_HARDLINE, indent);
	while (parser_exec_decl(pr, indent, 0) & GOOD)
		error = 0;
	if (error)
		doc_remove(kr, dc);

	parser_exec_attributes(pr, dc, &pf->pf_out, pr->pr_cf->cf_tw,
	    DOC_HARDLINE);

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
	struct token *end = NULL;
	struct token *pv = NULL;
	struct token *tk;
	int error = 0;

	/* Consume any left parenthesis before emitting a soft line. */
	if (lexer_if(lx, TOKEN_LPAREN, &tk)) {
		token_trim(tk);
		doc_token(tk, dc);
	}

	/*
	 * Let each argument begin with a soft line, causing a line to be
	 * emitted immediately if the argument does not fit instead of breaking
	 * the argument.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_alloc(DOC_SOFTLINE, concat);
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_OPTIONAL, concat));

	/* A type will missing when emitting the final right parenthesis. */
	if (lexer_peek_if_type(lx, &end, LEXER_TYPE_FLAG_ARG) &&
	    parser_exec_type(pr, concat, end, NULL) & (FAIL | NONE))
		return parser_fail(pr);

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
			return parser_fail(pr);

		if (parser_exec_attributes(pr, concat, NULL, 0,
		    DOC_LINE) & GOOD)
			break;

		if (lexer_if(lx, TOKEN_COMMA, &tk)) {
			doc_token(tk, concat);
			doc_alloc(DOC_LINE, concat);
			break;
		}

		if (!lexer_pop(lx, &tk)) {
			error = parser_fail(pr);
			break;
		}
		/* Identifiers must be separated. */
		if (pv != NULL && pv->tk_type == TOKEN_IDENT &&
		    tk->tk_type == TOKEN_IDENT)
			doc_alloc(DOC_LINE, concat);
		doc_token(tk, concat);
		if (tk == rparen)
			return parser_none(pr);
		pv = tk;
	}

	return error ? error : parser_good(pr);
}

static int
parser_exec_stmt(struct parser *pr, struct doc *dc, const struct token *rbrace)
{
	struct lexer *lx = pr->pr_lx;
	int simple = -1;
	int error;

	if (lexer_peek_if(lx, TOKEN_IF, NULL) ||
	    lexer_peek_if(lx, TOKEN_FOR, NULL) ||
	    lexer_peek_if(lx, TOKEN_WHILE, NULL) ||
	    lexer_peek_if(lx, TOKEN_IDENT, NULL))
		simple = parser_simple_stmt_enter(pr, rbrace);
	error = parser_exec_stmt1(pr, dc, rbrace);
	parser_simple_stmt_leave(pr, simple);
	return error;
}

static int
parser_exec_stmt1(struct parser *pr, struct doc *dc, const struct token *rbrace)
{
	struct parser_exec_stmt_block_arg ps = {
		.ps_head	= dc,
		.ps_tail	= dc,
		.ps_flags	= PARSER_EXEC_STMT_BLOCK_FLAG_TRIM,
		.ps_rbrace	= NULL,
	};
	struct lexer *lx = pr->pr_lx;
	struct token *tk;

	if (parser_exec_stmt_block(pr, &ps) & GOOD)
		return parser_good(pr);
	if (parser_exec_stmt_if(pr, dc, rbrace) & GOOD)
		return parser_good(pr);
	if (parser_exec_stmt_return(pr, dc) & GOOD)
		return parser_good(pr);
	if (parser_exec_stmt_for(pr, dc, rbrace) & GOOD)
		return parser_good(pr);
	if (parser_exec_stmt_dowhile(pr, dc, rbrace) & GOOD)
		return parser_good(pr);
	if (parser_exec_stmt_case(pr, dc, rbrace) & GOOD)
		return parser_good(pr);
	if (parser_exec_stmt_goto(pr, dc) & GOOD)
		return parser_good(pr);
	if (parser_exec_stmt_label(pr, dc, rbrace) & GOOD)
		return parser_good(pr);

	if (lexer_peek_if(lx, TOKEN_WHILE, &tk) ||
	    lexer_peek_if(lx, TOKEN_SWITCH, &tk))
		return parser_exec_stmt_expr(pr, dc, tk, 0);

	if (lexer_if(lx, TOKEN_BREAK, &tk) ||
	    lexer_if(lx, TOKEN_CONTINUE, &tk)) {
		doc_token(tk, dc);
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, dc);
		return parser_good(pr);
	}

	if (lexer_if(lx, TOKEN_SEMI, &tk)) {
		doc_token(tk, dc);
		return parser_good(pr);
	}

	/*
	 * Note, the ordering of operations is of importance here. Interpret the
	 * following tokens as an expression if the same expression spans to the
	 * first semi colon. Calling parser_exec_decl() first and treating
	 * everything else as an expression has the unwanted effect of treating
	 * function calls as declarations. This in turn is caused by
	 * parser_exec_decl() being able to detect declarations making use of
	 * preprocessor directives such as the ones provided by queue(3).
	 */
	if (parser_peek_expr(pr, rbrace)) {
		struct doc *expr = NULL;

		if (parser_exec_expr(pr, dc, &expr, NULL, 0) & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
		return parser_good(pr);
	}

	if (parser_exec_decl(pr, dc, PARSER_EXEC_DECL_FLAG_BREAK) & GOOD)
		return parser_good(pr);

	/*
	 * Last resort, see if this is a loop construct hidden behind cpp such
	 * as queue(3).
	 */
	if (lexer_peek_if(lx, TOKEN_IDENT, &tk))
		return parser_exec_stmt_expr(pr, dc, tk, 0);

	return parser_none(pr);
}

/*
 * Parse a block statement wrapped in braces.
 */
static int
parser_exec_stmt_block(struct parser *pr, struct parser_exec_stmt_block_arg *ps)
{
	struct lexer_state s;
	struct doc *concat, *dc, *indent, *line;
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace, *tk;
	int doswitch = ps->ps_flags & PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH;
	int doindent = !doswitch && pr->pr_simple.se_nstmt == 0;
	int nstmt = 0;
	int peek = 0;
	int error;

	lexer_peek_enter(lx, &s);
	if (lexer_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace)) {
		struct token *semi;

		if ((pr->pr_cf->cf_flags & CONFIG_FLAG_SIMPLE) &&
		    lexer_peek_if(lx, TOKEN_SEMI, &semi))
			lexer_remove(lx, semi, 1);
		peek = 1;
	}
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	if (doindent)
		pr->pr_nindent++;
	pr->pr_nblocks++;

	dc = parser_simple_stmt_block(pr, ps->ps_tail);

	parser_trim_brace(rbrace);

	if (!lexer_expect(lx, TOKEN_LBRACE, &lbrace))
		return parser_fail(pr);
	/*
	 * Optionally remove empty lines after the opening left brace.
	 * An empty line is however allowed in the beginning of a
	 * function implementation, a convention used by some when the
	 * function lacks local variables.
	 */
	if (ps->ps_flags & PARSER_EXEC_STMT_BLOCK_FLAG_TRIM)
		token_trim(lbrace);
	doc_token(lbrace, ps->ps_head);

	indent = doswitch ? dc : doc_alloc_indent(pr->pr_cf->cf_tw, dc);
	line = doc_alloc(DOC_HARDLINE, indent);
	while ((error = parser_exec_stmt(pr, indent, rbrace)) & GOOD) {
		nstmt++;
		if (lexer_peek_if(lx, TOKEN_RBRACE, &tk) && tk == rbrace)
			break;
		doc_alloc(DOC_HARDLINE, indent);
	}
	/* Do not keep the hard line if the statement block is empty. */
	if (nstmt == 0 && (error & BRCH) == 0)
		doc_remove(line, indent);

	doc_alloc(DOC_HARDLINE, ps->ps_tail);

	/*
	 * The right brace and any following statement is expected to fit on a
	 * single line.
	 */
	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, ps->ps_tail));
	if (lexer_expect(lx, TOKEN_RBRACE, &tk)) {
		if (lexer_peek_if(lx, TOKEN_ELSE, NULL))
			token_trim(tk);
		doc_token(tk, concat);
	}
	if (lexer_if(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);
	ps->ps_rbrace = concat;

	if (doindent)
		pr->pr_nindent--;
	pr->pr_nblocks--;

	return parser_good(pr);
}

static int
parser_exec_stmt_if(struct parser *pr, struct doc *dc,
    const struct token *rbrace)
{
	struct lexer *lx = pr->pr_lx;
	struct token *tk, *tkelse, *tkif;

	if (!lexer_peek_if(lx, TOKEN_IF, &tk))
		return parser_none(pr);

	if (parser_exec_stmt_expr(pr, dc, tk, 0) & (FAIL | NONE))
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
			error = parser_exec_stmt_expr(pr, dc, tkif, 0);
			if (error & (FAIL | NONE))
				return parser_fail(pr);
		} else {
			if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
				error = parser_exec_stmt(pr, dc, rbrace);
				if (error & FAIL)
					return parser_fail(pr);
			} else {
				void *simple;

				dc = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
				doc_alloc(DOC_HARDLINE, dc);

				simple = parser_simple_stmt_ifelse_enter(pr);
				error = parser_exec_stmt(pr, dc, rbrace);
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
parser_exec_stmt_for(struct parser *pr, struct doc *dc,
    const struct token *rbrace)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *expr = NULL;
	struct doc *loop, *space;
	struct token *tk;
	unsigned int flags;
	int error;

	if (!lexer_if(lx, TOKEN_FOR, &tk))
		return parser_none(pr);

	loop = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(tk, loop);
	doc_literal(" ", loop);

	if (lexer_expect(lx, TOKEN_LPAREN, &tk))
		doc_token(tk, loop);

	/* Declarations are allowed in the first expression. */
	if (parser_exec_decl(pr, loop, 0) & NONE) {
		error = parser_exec_expr(pr, loop, &expr, NULL, 0);
		if (error & (FAIL | BRCH))
			return parser_fail(pr);
		/* Let the semicolon hang of the expression unless empty. */
		if (error & NONE)
			expr = loop;
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
	} else {
		expr = loop;
	}
	space = doc_literal(" ", expr);

	/*
	 * If the expression does not fit, break after the semicolon if the
	 * previous expression was not empty.
	 */
	flags = expr != loop ? EXPR_EXEC_FLAG_SOFTLINE : 0;
	/* Let the semicolon hang of the expression unless empty. */
	error = parser_exec_expr(pr, loop, &expr, NULL, flags);
	if (error & (FAIL | BRCH))
		return parser_fail(pr);
	if (error & NONE) {
		/* Expression empty, remove the space. */
		doc_remove(space, expr);
		expr = loop;
	}
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, expr);
	space = doc_literal(" ", expr);

	/*
	 * If the expression does not fit, break after the semicolon if
	 * the previous expression was not empty.
	 */
	flags = expr != loop ? EXPR_EXEC_FLAG_SOFTLINE : 0;
	/* Let the semicolon hang of the expression unless empty. */
	error = parser_exec_expr(pr, loop, &expr, NULL, flags);
	if (error & (FAIL | BRCH))
		return parser_fail(pr);
	if (error & NONE) {
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
	return parser_exec_stmt(pr, dc, rbrace);
}

static int
parser_exec_stmt_dowhile(struct parser *pr, struct doc *dc,
    const struct token *rbrace)
{
	struct parser_exec_stmt_block_arg ps = {
		.ps_head	= dc,
		.ps_tail	= dc,
		.ps_flags	= PARSER_EXEC_STMT_BLOCK_FLAG_TRIM,
		.ps_rbrace	= NULL,
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
		concat = ps.ps_rbrace;
		doc_literal(" ", concat);
	} else {
		struct doc *indent;

		indent = doc_alloc_indent(pr->pr_cf->cf_tw, concat);
		doc_alloc(DOC_HARDLINE, indent);
		error = parser_exec_stmt(pr, indent, rbrace);
		doc_alloc(DOC_HARDLINE, concat);
	}
	if (error & (FAIL | NONE))
		return parser_fail(pr);

	if (lexer_peek_if(lx, TOKEN_WHILE, &tk))
		return parser_exec_stmt_expr(pr, concat, tk,
		    PARSER_EXEC_STMT_EXPR_FLAG_DOWHILE);
	return parser_fail(pr);
}

static int
parser_exec_stmt_return(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct doc *concat;
	struct token *tk;

	if (!lexer_if(lx, TOKEN_RETURN, &tk))
		return parser_none(pr);

	concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
	doc_token(tk, concat);
	if (!lexer_peek_if(lx, TOKEN_SEMI, NULL)) {
		int error;

		doc_literal(" ", concat);
		error = parser_exec_expr(pr, concat, NULL, NULL,
		    EXPR_EXEC_FLAG_NOPARENS);
		if (error & HALT)
			return parser_fail(pr);
	}
	if (lexer_expect(lx, TOKEN_SEMI, &tk))
		doc_token(tk, concat);
	return parser_good(pr);
}

/*
 * Parse a statement consisting of a keyword, expression wrapped in parenthesis
 * and following statement(s).
 */
static int
parser_exec_stmt_expr(struct parser *pr, struct doc *dc,
    const struct token *type, unsigned int flags)
{
	struct doc *expr = NULL;
	struct doc *stmt;
	struct lexer *lx = pr->pr_lx;
	struct token *rparen, *stop, *tk;

	if (!lexer_expect(lx, type->tk_type, &tk) ||
	    !lexer_peek_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, &rparen))
		return parser_fail(pr);
	/* Never break after the right parenthesis. */
	token_trim(rparen);

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
	if (parser_exec_expr(pr, stmt, &expr, stop, 0) & HALT)
		return parser_fail(pr);

	if (flags & PARSER_EXEC_STMT_EXPR_FLAG_DOWHILE) {
		if (lexer_expect(lx, TOKEN_SEMI, &tk))
			doc_token(tk, expr);
		return parser_good(pr);
	}

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		struct parser_exec_stmt_block_arg ps = {
			.ps_head	= expr,
			.ps_tail	= dc,
			.ps_flags	= PARSER_EXEC_STMT_BLOCK_FLAG_TRIM,
			.ps_rbrace	= NULL,
		};

		if (type->tk_type == TOKEN_SWITCH)
			ps.ps_flags |= PARSER_EXEC_STMT_BLOCK_FLAG_SWITCH;
		doc_literal(" ", expr);
		return parser_exec_stmt_block(pr, &ps);
	} else {
		struct doc *indent;
		void *simple = NULL;
		int error;

		indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
		doc_alloc(DOC_HARDLINE, indent);
		if (type->tk_type == TOKEN_IF)
			simple = parser_simple_stmt_ifelse_enter(pr);
		error = parser_exec_stmt(pr, indent, NULL);
		if (type->tk_type == TOKEN_IF)
			parser_simple_stmt_ifelse_leave(pr, simple);
		return error;
	}
}

static int
parser_exec_stmt_label(struct parser *pr, struct doc *dc,
    const struct token *rbrace)
{
	struct lexer_state s;
	struct doc *dedent;
	struct lexer *lx = pr->pr_lx;
	struct token *ident, *nx, *tk;
	int peek = 0;

	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if(lx, TOKEN_COLON, NULL))
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return parser_none(pr);

	dedent = doc_alloc_dedent(dc);
	if (lexer_expect(lx, TOKEN_IDENT, &tk)) {
		struct doc *label;

		label = doc_token(tk, dedent);
		/*
		 * Honor indentation before label but make sure to emit it right
		 * before the label. Necessary when the label is prefixed with
		 * comment(s).
		 */
		if (token_has_indent(ident))
			doc_append_before(doc_literal(" ", NULL), label);
	}
	if (lexer_expect(lx, TOKEN_COLON, &tk))
		doc_token(tk, dedent);
	else
		return parser_fail(pr);

	/*
	 * A label is not necessarily followed by a hard line, there could be
	 * another statement on the same line.
	 */
	if (lexer_back(lx, &tk) && lexer_peek(lx, &nx) && nx != rbrace &&
	    token_cmp(tk, nx) == 0) {
		struct doc *indent;

		indent = doc_alloc_indent(DOC_INDENT_FORCE, dc);
		return parser_exec_stmt(pr, indent, rbrace);
	}

	return parser_good(pr);
}

static int
parser_exec_stmt_case(struct parser *pr, struct doc *dc,
    const struct token *rbrace)
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
		doc_alloc(DOC_LINE, lhs);
		if (parser_exec_expr(pr, lhs, NULL, NULL, 0) & HALT)
			return parser_fail(pr);
	}
	if (!lexer_expect(lx, TOKEN_COLON, &tk))
		return parser_fail(pr);
	token_trim(tk);
	doc_token(tk, lhs);

	if (lexer_peek_if(lx, TOKEN_LBRACE, NULL)) {
		doc_alloc(DOC_LINE, lhs);
		if (parser_exec_stmt(pr, dc, rbrace) & FAIL)
			return parser_fail(pr);
	}

	indent = doc_alloc_indent(pr->pr_cf->cf_tw, dc);
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

		if (parser_exec_stmt(pr, indent, rbrace) & HALT) {
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
			align = TAILQ_PREV(align, token_list, tk_entry);
			if (align == NULL)
				break;
		}

		/*
		 * No alignment wanted if the first non-pointer token is
		 * followed by a semi.
		 */
		if (align != NULL) {
			const struct token *nx;

			nx = TAILQ_NEXT(align, tk_entry);
			if (nx != NULL && nx->tk_type == TOKEN_SEMI)
				align = NULL;
		}
	}

	for (;;) {
		struct doc *concat;
		struct token *tk;
		int didalign = 0;

		if (!lexer_pop(lx, &tk))
			return parser_fail(pr);
		token_trim(tk);

		if (tk->tk_flags & TOKEN_FLAG_TYPE_ARGS) {
			struct doc *indent;

			doc_token(tk, dc);
			indent = doc_alloc_indent(pr->pr_cf->cf_sw, dc);
			while (parser_exec_func_arg(pr, indent, NULL, end) &
			    GOOD)
				continue;
			break;
		}

		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_token(tk, concat);

		if (tk == align) {
			if (token_is_decl(tk, TOKEN_ENUM) ||
			    token_is_decl(tk, TOKEN_STRUCT) ||
			    token_is_decl(tk, TOKEN_UNION))
				doc_alloc(DOC_LINE, concat);
			else
				ruler_insert(rl, tk, concat, 1,
				    parser_width(pr, dc), nspaces);
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
    unsigned int indent, enum doc_type linetype)
{
	struct doc *concat = NULL;
	struct lexer *lx = pr->pr_lx;

	if (!lexer_peek_if(lx, TOKEN_ATTRIBUTE, NULL))
		return NONE;

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
		if (parser_exec_expr(pr, concat, NULL, NULL, 0) & HALT)
			return parser_fail(pr);
		if (lexer_expect(lx, TOKEN_RPAREN, &tk))
			doc_token(tk, concat);
	}
	if (out != NULL)
		*out = concat;

	return parser_good(pr);
}

static int
parser_simple_active(const struct parser *pr)
{
	return pr->pr_simple.se_nstmt > 0 || pr->pr_simple.se_ndecl > 0;
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
parser_simple_stmt_enter(struct parser *pr, const struct token *rbrace)
{
	struct lexer_state s;
	struct doc *dc;
	struct lexer *lx = pr->pr_lx;
	int error;

	if ((pr->pr_cf->cf_flags & CONFIG_FLAG_SIMPLE) == 0)
		return 0;
	if (++pr->pr_simple.se_nstmt > 1)
		return 1;

	pr->pr_simple.se_stmt = simple_stmt_enter(lx, pr->pr_cf);
	dc = doc_alloc(DOC_CONCAT, NULL);
	lexer_peek_enter(lx, &s);
	error = parser_exec_stmt1(pr, dc, rbrace);
	lexer_peek_leave(lx, &s);
	doc_free(dc);
	if (error & GOOD)
		simple_stmt_leave(pr->pr_simple.se_stmt);
	simple_stmt_free(pr->pr_simple.se_stmt);
	pr->pr_simple.se_stmt = NULL;
	pr->pr_simple.se_nstmt--;

	return 0;
}

static void
parser_simple_stmt_leave(struct parser *pr, int simple)
{
	if (simple == 1)
		pr->pr_simple.se_nstmt--;
}

static struct doc *
parser_simple_stmt_block(struct parser *pr, struct doc *dc)
{
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace, *rbrace;

	/* Ignore nested statements, they will be handled later on. */
	if ((pr->pr_cf->cf_flags & CONFIG_FLAG_SIMPLE) == 0 ||
	    pr->pr_simple.se_nstmt != 1)
		return dc;

	if (!lexer_peek_if(lx, TOKEN_LBRACE, &lbrace) ||
	    !lexer_peek_if_pair(lx, TOKEN_LBRACE, TOKEN_RBRACE, &rbrace))
		return dc;

	return simple_stmt_block(pr->pr_simple.se_stmt, lbrace, rbrace,
	    pr->pr_nindent * pr->pr_cf->cf_tw);
}

static void *
parser_simple_stmt_ifelse_enter(struct parser *pr)
{
	struct lexer *lx = pr->pr_lx;
	struct token *lbrace;

	if (pr->pr_simple.se_nstmt != 1 || !lexer_peek(lx, &lbrace))
		return NULL;
	return simple_stmt_ifelse_enter(pr->pr_simple.se_stmt, lbrace,
	    pr->pr_nindent * pr->pr_cf->cf_tw);
}

static void
parser_simple_stmt_ifelse_leave(struct parser *pr, void *cookie)
{
	struct lexer *lx = pr->pr_lx;
	struct token *rbrace;

	if (cookie == NULL || !lexer_peek(lx, &rbrace))
		return;
	simple_stmt_ifelse_leave(pr->pr_simple.se_stmt, rbrace, cookie);
}

static int
parser_simple_decl_active(const struct parser *pr)
{
	return pr->pr_nblocks > 0 &&
	    pr->pr_simple.se_nstmt == 0 &&
	    pr->pr_simple.se_ndecl == 1 &&
	    pr->pr_simple.se_decl != NULL;
}

static int
parser_simple_decl_enter(struct parser *pr, unsigned int flags)
{
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct doc *dc;
	int error;

	if ((pr->pr_cf->cf_flags & CONFIG_FLAG_SIMPLE) == 0)
		return -1;

	if (pr->pr_simple.se_ndecl++ > 0)
		return 1;

	pr->pr_simple.se_decl = simple_decl_enter(lx, pr->pr_cf);
	dc = doc_alloc(DOC_CONCAT, NULL);
	lexer_peek_enter(lx, &s);
	error = parser_exec_decl1(pr, dc, flags);
	lexer_peek_leave(lx, &s);
	doc_free(dc);
	if (error & GOOD)
		simple_decl_leave(pr->pr_simple.se_decl);
	simple_decl_free(pr->pr_simple.se_decl);
	pr->pr_simple.se_decl = NULL;
	return 1;
}

static void
parser_simple_decl_leave(struct parser *pr, int simple)
{
	if (simple == 1)
		pr->pr_simple.se_ndecl--;
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

static int
parser_peek_expr(struct parser *pr, const struct token *stop)
{
	const struct expr_exec_arg ea = {
		.ea_cf		= pr->pr_cf,
		.ea_lx		= pr->pr_lx,
		.ea_dc		= NULL,
		.ea_stop	= stop,
		.ea_recover	= parser_exec_expr_recover,
		.ea_arg		= pr,
		.ea_flags	= 0,
	};
	struct lexer_state s;
	struct lexer *lx = pr->pr_lx;
	struct token *ident, *nx, *semi;
	int peek = 0;

	if (lexer_peek_if_type(lx, NULL, 0))
		return 0;
	if (!lexer_peek_until_stop(lx, TOKEN_SEMI, stop, &semi))
		return 0;

	lexer_peek_enter(lx, &s);
	if (expr_peek(&ea) && lexer_pop(lx, &nx) && nx == semi)
		peek = 1;
	lexer_peek_leave(lx, &s);
	if (!peek)
		return 0;

	/*
	 * Do not confuse a loop construct hidden behind cpp followed by a
	 * statement which is a sole expression:
	 *
	 * 	foreach()
	 * 		func();
	 */
	lexer_peek_enter(lx, &s);
	if (lexer_if(lx, TOKEN_IDENT, &ident) &&
	    lexer_if_pair(lx, TOKEN_LPAREN, TOKEN_RPAREN, NULL) &&
	    lexer_pop(lx, &nx) && nx != semi &&
	    token_cmp(ident, nx) < 0 && token_cmp(nx, semi) <= 0)
		peek = 0;
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

static int
__parser_fail(struct parser *pr, const char *fun, int lno)
{
	struct token *tk;

	if (parser_get_error(pr))
		goto out;
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

out:
	if (lexer_is_branch(pr->pr_lx))
		return BRCH;
	return FAIL;
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
		token_trim(pv);
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
