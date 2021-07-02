#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * Precedence, from lowest to highest.
 */
enum expr_pc {
	PC0,		/* {} literal */
	PC1,		/* , */
	PC2,		/* = += -= *= /= %= <<= >>= &= ^= |= */
	PC3,		/* ?: */
	PC4,		/* || */
	PC5,		/* && */
	PC6,		/* | */
	PC7,		/* ^ */
	PC8,		/* & */
	PC9,		/* == != */
	PC10,		/* < <= > >= */
	PC11,		/* << >> */
	PC12,		/* + - */
	PC13,		/* * / % */
	PC14,		/* ! ~ ++ -- - (cast) * & sizeof */
	PC15,		/* () [] -> . */
};

#define PCUNARY		0x80000000u
#define PC(pc)		((pc) & ~PCUNARY)

enum expr_type {
	EXPR_UNARY,
	EXPR_BINARY,
	EXPR_TERNARY,
	EXPR_PREFIX,
	EXPR_POSTFIX,
	EXPR_PARENS,
	EXPR_SQUARES,
	EXPR_FIELD,
	EXPR_CALL,
	EXPR_ARG,
	EXPR_CAST,
	EXPR_SIZEOF,
	EXPR_CONCAT,
	EXPR_LITERAL,
	EXPR_RECOVER,
	EXPR_BRANCH,
};

struct expr {
	enum expr_type		 ex_type;
	const struct token	*ex_tk;
	struct expr		*ex_lhs;
	struct expr		*ex_rhs;
	struct token		*ex_beg;
	struct token		*ex_end;
	struct token		*ex_tokens[2];

	union {
		struct expr	*ex_ternary;
		struct doc	*ex_dc;
		int		 ex_sizeof;
	};
};

struct expr_state;

struct expr_rule {
	enum expr_pc	 er_pc;
	int		 er_rassoc;
	enum token_type	 er_type;
	struct expr	*(*er_func)(struct expr_state *, struct expr *);
};

struct expr_state {
	const struct config		*es_cf;
	const struct expr_exec_arg	*es_ea;
	const struct expr_rule		*es_er;
	const struct token		*es_stop;
	struct lexer			*es_lx;
	struct token			*es_tk;
	unsigned int			 es_nest;	/* number of nested expressions */
	unsigned int			 es_parens;	/* number of nested parenthesis */
	unsigned int			 es_soft;	/* number of soft lines */
};

static struct expr	*expr_exec1(struct expr_state *, enum expr_pc);
static struct expr	*expr_exec_recover(struct expr_state *);

static struct expr	*expr_exec_binary(struct expr_state *, struct expr *);
static struct expr	*expr_exec_concat(struct expr_state *, struct expr *);
static struct expr	*expr_exec_field(struct expr_state *, struct expr *);
static struct expr	*expr_exec_literal(struct expr_state *, struct expr *);
static struct expr	*expr_exec_parens(struct expr_state *, struct expr *);
static struct expr	*expr_exec_prepost(struct expr_state *, struct expr *);
static struct expr	*expr_exec_sizeof(struct expr_state *, struct expr *);
static struct expr	*expr_exec_squares(struct expr_state *, struct expr *);
static struct expr	*expr_exec_ternary(struct expr_state *, struct expr *);
static struct expr	*expr_exec_unary(struct expr_state *, struct expr *);

static struct expr	*expr_alloc(enum expr_type, const struct expr_state *);
static void		 expr_free(struct expr *);

static struct doc	*expr_doc(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_indent_parens(const struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_tokens(const struct expr *, struct doc *);

#define expr_doc_soft(a, b) \
	__expr_doc_soft((a), (b), __func__, __LINE__)
static struct doc	*__expr_doc_soft(const struct expr_state *,
    struct doc *, const char *, int);

static void	expr_state_init(struct expr_state *,
    const struct expr_exec_arg *);

static const struct expr_rule	*expr_rule_find(const struct token *, int);

static int	iscast(struct expr_state *);
static int	isliteral(const struct token *);

static const struct expr_rule	rules[] = {
	{ PC0 | PCUNARY,	0,	TOKEN_LITERAL,	expr_exec_literal },
	{ PC1,			0,	TOKEN_LITERAL,	expr_exec_concat },

	{ PC1,			0,	TOKEN_COMMA,			expr_exec_binary },
	{ PC1 | PCUNARY,	0,	TOKEN_COMMA,			expr_exec_binary },
	{ PC1,			0,	TOKEN_ELLIPSIS,			expr_exec_binary },
	{ PC2,			1,	TOKEN_EQUAL,			expr_exec_binary },
	{ PC2,			1,	TOKEN_PLUSEQUAL,		expr_exec_binary },
	{ PC2,			1,	TOKEN_MINUSEQUAL,		expr_exec_binary },
	{ PC2,			1,	TOKEN_STAREQUAL,		expr_exec_binary },
	{ PC2,			1,	TOKEN_SLASHEQUAL,		expr_exec_binary },
	{ PC2,			1,	TOKEN_PERCENTEQUAL,		expr_exec_binary },
	{ PC2,			1,	TOKEN_LESSLESSEQUAL,		expr_exec_binary },
	{ PC2,			1,	TOKEN_GREATERGREATEREQUAL,	expr_exec_binary },
	{ PC2,			1,	TOKEN_AMPEQUAL,			expr_exec_binary },
	{ PC2,			1,	TOKEN_CARETEQUAL,		expr_exec_binary },
	{ PC2,			1,	TOKEN_PIPEEQUAL,		expr_exec_binary },
	{ PC3,			1,	TOKEN_QUESTION,			expr_exec_ternary },
	{ PC4,			0,	TOKEN_PIPEPIPE,			expr_exec_binary },
	{ PC5,			0,	TOKEN_AMPAMP,			expr_exec_binary },
	{ PC6,			0,	TOKEN_PIPE,			expr_exec_binary },
	{ PC7,			0,	TOKEN_CARET,			expr_exec_binary },
	{ PC8,			0,	TOKEN_AMP,			expr_exec_binary },
	{ PC9,			0,	TOKEN_EQUALEQUAL,		expr_exec_binary },
	{ PC9,			0,	TOKEN_EXCLAIMEQUAL,		expr_exec_binary },
	{ PC10,			0,	TOKEN_LESS,			expr_exec_binary },
	{ PC10,			0,	TOKEN_LESSEQUAL,		expr_exec_binary },
	{ PC10,			0,	TOKEN_GREATER,			expr_exec_binary },
	{ PC10,			0,	TOKEN_GREATEREQUAL,		expr_exec_binary },
	{ PC11,			0,	TOKEN_LESSLESS,			expr_exec_binary },
	{ PC11,			0,	TOKEN_GREATERGREATER,		expr_exec_binary },
	{ PC12,			0,	TOKEN_PLUS,			expr_exec_binary },
	{ PC12,			0,	TOKEN_MINUS,			expr_exec_binary },
	{ PC13,			0,	TOKEN_STAR,			expr_exec_binary },
	{ PC13,			0,	TOKEN_SLASH,			expr_exec_binary },
	{ PC13,			0,	TOKEN_PERCENT,			expr_exec_binary },
	{ PC14 | PCUNARY,	1,	TOKEN_EXCLAIM,			expr_exec_unary },
	{ PC14 | PCUNARY,	1,	TOKEN_TILDE,			expr_exec_unary },
	{ PC14,			1,	TOKEN_PLUSPLUS,			expr_exec_prepost },
	{ PC14 | PCUNARY,	1,	TOKEN_PLUSPLUS,			expr_exec_prepost },
	{ PC14 | PCUNARY,	1,	TOKEN_PLUS,			expr_exec_unary },
	{ PC14,			1,	TOKEN_MINUSMINUS,		expr_exec_prepost },
	{ PC14 | PCUNARY,	1,	TOKEN_MINUSMINUS,		expr_exec_prepost },
	{ PC14 | PCUNARY,	1,	TOKEN_MINUS,			expr_exec_unary },
	{ PC14 | PCUNARY,	1,	TOKEN_STAR,			expr_exec_unary },
	{ PC14 | PCUNARY,	1,	TOKEN_AMP,			expr_exec_unary },
	{ PC14 | PCUNARY,	1,	TOKEN_SIZEOF,			expr_exec_sizeof },
	{ PC15,			0,	TOKEN_LPAREN,			expr_exec_parens },
	{ PC15 | PCUNARY,	0,	TOKEN_LPAREN,			expr_exec_parens },
	{ PC15,			0,	TOKEN_LSQUARE,			expr_exec_squares },
	{ PC15,			0,	TOKEN_ARROW,			expr_exec_field },
	{ PC15,			0,	TOKEN_PERIOD,			expr_exec_field },
};

struct doc *
expr_exec(const struct expr_exec_arg *ea)
{
	struct expr_state es;
	struct doc *dc, *expr, *indent;
	struct expr *ex;

	expr_state_init(&es, ea);

	ex = expr_exec1(&es, PC0);
	if (ex == NULL)
		return NULL;
	if (lexer_get_error(es.es_lx)) {
		expr_free(ex);
		return NULL;
	}

	dc = doc_alloc(DOC_GROUP, ea->ea_dc);
	indent = (ea->ea_flags & EXPR_EXEC_FLAG_NOINDENT) == 0 ?
	    doc_alloc_indent(ea->ea_cf->cf_sw, dc) : doc_alloc(DOC_CONCAT, dc);
	doc_alloc_optional(1, indent);
	if (ea->ea_flags & EXPR_EXEC_FLAG_SOFTLINE)
		doc_alloc(DOC_SOFTLINE, indent);
	expr = expr_doc(ex, &es, indent);
	doc_alloc_optional(-1, expr);
	expr_free(ex);
	return expr;
}

int
expr_peek(const struct expr_exec_arg *ea)
{
	struct expr_state es;
	struct expr *ex;
	int peek = 0;
	int error;

	expr_state_init(&es, ea);
	ex = expr_exec1(&es, PC0);
	error = lexer_get_error(es.es_lx);
	if (ex != NULL && error == 0)
		peek = 1;
	expr_free(ex);
	return peek;
}

static struct expr *
expr_exec1(struct expr_state *es, enum expr_pc pc)
{
	const struct expr_rule *er;
	struct expr *ex = NULL;

	if (lexer_is_branch(es->es_lx))
		return expr_alloc(EXPR_BRANCH, es);

	if (lexer_get_error(es->es_lx) ||
	    (!lexer_peek(es->es_lx, &es->es_tk) || es->es_tk == es->es_stop))
		return NULL;

	/* Only consider unary operators. */
	er = expr_rule_find(es->es_tk, 1);
	if (er == NULL || er->er_type == TOKEN_LITERAL) {
		/*
		 * Even if a literal operator was found, let the parser recover
		 * before continuing. Otherwise, pointer types can be
		 * erroneously treated as a multiplication expression.
		 */
		ex = expr_exec_recover(es);
		if (ex == NULL && er == NULL)
			return NULL;
	}
	if (ex == NULL) {
		es->es_er = er;
		if (!lexer_pop(es->es_lx, &es->es_tk))
			return NULL;
		ex = es->es_er->er_func(es, NULL);
	}
	if (ex == NULL)
		return NULL;

	for (;;) {
		struct expr *tmp;

		if (!lexer_peek(es->es_lx, &es->es_tk) ||
		    es->es_tk == es->es_stop)
			break;

		/* Only consider binary operators. */
		er = expr_rule_find(es->es_tk, 0);
		if (er == NULL)
			break;
		es->es_er = er;

		if (pc >= PC(er->er_pc))
			break;

		if (!lexer_pop(es->es_lx, &es->es_tk))
			break;
		tmp = es->es_er->er_func(es, ex);
		if (tmp == NULL)
			return NULL;
		if (lexer_get_error(es->es_lx)) {
			expr_free(tmp);
			return NULL;
		}
		ex = tmp;
	}

	return ex;
}

static struct expr *
expr_exec_recover(struct expr_state *es)
{
	struct doc *dc;
	struct expr *ex;

	if (es->es_ea->ea_recover == NULL)
		return NULL;

	dc = es->es_ea->ea_recover(es->es_ea->ea_arg);
	if (dc == NULL)
		return NULL;

	ex = expr_alloc(EXPR_RECOVER, es);
	ex->ex_dc = dc;
	return ex;
}

static struct expr *
expr_exec_binary(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;
	struct token *pv;
	enum expr_pc pc;
	int iscomma = es->es_tk->tk_type == TOKEN_COMMA;

	/* Never break before the binary operator. */
	pv = TAILQ_PREV(es->es_tk, token_list, tk_entry);
	token_trim(pv, TOKEN_SPACE, TOKEN_FLAG_OPTLINE);

	pc = PC(es->es_er->er_pc);
	if (es->es_er->er_rassoc)
		pc--;

	ex = expr_alloc(iscomma ? EXPR_ARG : EXPR_BINARY, es);
	ex->ex_lhs = lhs;
	ex->ex_rhs = expr_exec1(es, pc);
	if (ex->ex_rhs == NULL) {
		expr_free(ex);
		return NULL;
	}
	return ex;
}

static struct expr *
expr_exec_concat(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;

	assert(lhs != NULL);

	ex = expr_alloc(EXPR_CONCAT, es);
	ex->ex_lhs = lhs;
	ex->ex_rhs = expr_exec_literal(es, NULL);
	return ex;
}

static struct expr *
expr_exec_field(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;

	assert(lhs != NULL);

	ex = expr_alloc(EXPR_FIELD, es);
	ex->ex_lhs = lhs;
	ex->ex_rhs = expr_exec1(es, PC(es->es_er->er_pc));
	if (ex->ex_rhs == NULL) {
		expr_free(ex);
		return NULL;
	}
	return ex;
}

static struct expr *
expr_exec_literal(struct expr_state *es, struct expr *MAYBE_UNUSED(lhs))
{
	assert(lhs == NULL);

	return expr_alloc(EXPR_LITERAL, es);
}

static struct expr *
expr_exec_parens(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;
	struct token *tk = es->es_tk;

	if (lhs == NULL) {
		if (iscast(es)) {
			ex = expr_alloc(EXPR_CAST, es);
			if (lexer_back(es->es_lx, &tk))
				ex->ex_tokens[0] = tk;	/* ( */
			/* Let the parser emit the type. */
			ex->ex_lhs = expr_exec_recover(es);
			if (ex->ex_lhs == NULL) {
				expr_free(ex);
				return NULL;
			}
			if (lexer_expect(es->es_lx, TOKEN_RPAREN, &tk))
				ex->ex_tokens[1] = tk;	/* ) */
			ex->ex_rhs = expr_exec1(es, PC0);
			if (ex->ex_rhs == NULL) {
				expr_free(ex);
				return NULL;
			}
			return ex;
		}

		ex = expr_alloc(EXPR_PARENS, es);
		ex->ex_lhs = expr_exec1(es, PC0);
		if (ex->ex_lhs == NULL) {
			expr_free(ex);
			return NULL;
		}
	} else {
		ex = expr_alloc(EXPR_CALL, es);
		ex->ex_lhs = lhs;
		ex->ex_rhs = expr_exec1(es, PC0);
	}
	ex->ex_tokens[0] = tk;	/* ( */

	if (lexer_expect(es->es_lx, TOKEN_RPAREN, &tk))
		ex->ex_tokens[1] = tk;	/* ) */

	return ex;
}

static struct expr *
expr_exec_prepost(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;

	if (lhs == NULL) {
		ex = expr_alloc(EXPR_PREFIX, es);
		ex->ex_lhs = expr_exec1(es, PC(es->es_er->er_pc));
		if (ex->ex_lhs == NULL) {
			expr_free(ex);
			return NULL;
		}
	} else {
		ex = expr_alloc(EXPR_POSTFIX, es);
		ex->ex_lhs = lhs;
	}
	return ex;
}

static struct expr *
expr_exec_sizeof(struct expr_state *es, struct expr *MAYBE_UNUSED(lhs))
{
	struct expr *ex;
	struct token *tk;

	assert(lhs == NULL);

	ex = expr_alloc(EXPR_SIZEOF, es);
	if (lexer_if(es->es_lx, TOKEN_LPAREN, &tk)) {
		ex->ex_tokens[0] = tk;	/* ( */
		ex->ex_sizeof = 1;
	}

	ex->ex_lhs = expr_exec1(es, PC0);
	if (ex->ex_lhs == NULL) {
		expr_free(ex);
		return NULL;
	}

	if (ex->ex_sizeof && lexer_expect(es->es_lx, TOKEN_RPAREN, &tk))
		ex->ex_tokens[1] = tk;	/* ) */

	return ex;
}

static struct expr *
expr_exec_squares(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;
	struct token *tk;

	assert(lhs != NULL);

	ex = expr_alloc(EXPR_SQUARES, es);
	ex->ex_tokens[0] = es->es_tk;	/* [ */
	ex->ex_lhs = lhs;
	ex->ex_rhs = expr_exec1(es, PC0);
	if (ex->ex_rhs == NULL) {
		expr_free(ex);
		return NULL;
	}

	if (lexer_expect(es->es_lx, TOKEN_RSQUARE, &tk))
		ex->ex_tokens[1] = tk;	/* ] */

	return ex;
}

static struct expr *
expr_exec_ternary(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;
	struct token *tk;

	ex = expr_alloc(EXPR_TERNARY, es);
	ex->ex_tokens[0] = es->es_tk;	/* ? */
	ex->ex_lhs = lhs;
	ex->ex_rhs = expr_exec1(es, PC0);
	if (lexer_expect(es->es_lx, TOKEN_COLON, &tk))
		ex->ex_tokens[1] = tk;	/* : */
	ex->ex_ternary = expr_exec1(es, PC0);
	if (ex->ex_ternary == NULL) {
		expr_free(ex);
		return NULL;
	}
	return ex;
}

static struct expr *
expr_exec_unary(struct expr_state *es, struct expr *MAYBE_UNUSED(lhs))
{
	struct expr *ex;

	assert(lhs == NULL);
	assert(es->es_er->er_pc & PCUNARY);

	ex = expr_alloc(EXPR_UNARY, es);
	ex->ex_lhs = expr_exec1(es, PC(es->es_er->er_pc));
	if (ex->ex_lhs == NULL) {
		expr_free(ex);
		return NULL;
	}
	return ex;
}

static struct expr *
expr_alloc(enum expr_type type, const struct expr_state *es)
{
	struct expr *ex;

	ex = calloc(1, sizeof(*ex));
	if (ex == NULL)
		err(1, NULL);
	ex->ex_type = type;
	ex->ex_tk = es->es_tk;
	return ex;
}

static void
expr_free(struct expr *ex)
{
	if (ex == NULL)
		return;

	expr_free(ex->ex_lhs);
	expr_free(ex->ex_rhs);
	if (ex->ex_type == EXPR_TERNARY)
		expr_free(ex->ex_ternary);
	else if (ex->ex_type == EXPR_RECOVER)
		doc_free(ex->ex_dc);
	free(ex);
}

static struct doc *
expr_doc(struct expr *ex, struct expr_state *es, struct doc *parent)
{
	struct doc *concat, *group;

	group = doc_alloc(DOC_GROUP, parent);
	concat = doc_alloc(DOC_CONCAT, group);

	/*
	 * Testing backdoor wrapping each expression in parenthesis used for
	 * validation of operator precedence.
	 */
	if ((es->es_cf->cf_flags & CONFIG_FLAG_TEST) &&
	    ex->ex_type != EXPR_PARENS)
		doc_literal("(", concat);

	switch (ex->ex_type) {
	case EXPR_UNARY:
		if (es->es_nest > 0)
			concat = expr_doc_soft(es, concat);
		doc_token(ex->ex_tk, concat);
		if (ex->ex_lhs != NULL)
			expr_doc(ex->ex_lhs, es, concat);
		break;

	case EXPR_BINARY: {
		struct doc *lhs;

		lhs = expr_doc(ex->ex_lhs, es, concat);
		doc_literal(" ", lhs);
		doc_token(ex->ex_tk, lhs);

		if (ex->ex_tk->tk_flags & TOKEN_FLAG_ASSIGN) {
			doc_literal(" ", concat);
		} else {
			concat = doc_alloc(DOC_CONCAT,
			    doc_alloc(DOC_GROUP, concat));
			doc_alloc(DOC_LINE, concat);
		}
		concat = expr_doc(ex->ex_rhs, es, concat);

		break;
	}

	case EXPR_TERNARY: {
		struct doc *ternary;

		ternary = expr_doc(ex->ex_lhs, es, concat);
		doc_alloc(DOC_LINE, ternary);
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], ternary);	/* ? */
		if (ex->ex_rhs != NULL)
			doc_alloc(DOC_LINE, ternary);

		/* The true expression can be empty, GNU extension. */
		ternary = expr_doc_soft(es, concat);
		if (ex->ex_rhs != NULL) {
			ternary = expr_doc(ex->ex_rhs, es, ternary);
			doc_alloc(DOC_LINE, ternary);
		}
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], ternary);	/* : */
		doc_alloc(DOC_LINE, ternary);

		ternary = expr_doc_soft(es, concat);
		concat = expr_doc(ex->ex_ternary, es, ternary);
		break;
	}

	case EXPR_PREFIX:
		doc_token(ex->ex_tk, concat);
		expr_doc(ex->ex_lhs, es, concat);
		break;

	case EXPR_POSTFIX:
		expr_doc(ex->ex_lhs, es, concat);
		doc_token(ex->ex_tk, concat);
		break;

	case EXPR_PARENS:
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* ( */
		if (ex->ex_lhs != NULL) {
			es->es_parens++;
			concat = expr_doc_indent_parens(es, concat);
			concat = expr_doc(ex->ex_lhs, es, concat);
			es->es_parens--;
		}
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ) */
		break;

	case EXPR_SQUARES:
		/* Do not break the left expression. */
		es->es_soft++;
		concat = expr_doc(ex->ex_lhs, es, concat);
		es->es_soft--;
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* [ */
		concat = expr_doc_soft(es, concat);
		concat = expr_doc(ex->ex_rhs, es, concat);
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ] */
		break;

	case EXPR_FIELD:
		concat = expr_doc_soft(es, concat);
		concat = expr_doc(ex->ex_lhs, es, concat);
		doc_token(ex->ex_tk, concat);
		concat = expr_doc(ex->ex_rhs, es, concat);
		break;

	case EXPR_CALL:
		/* Do not break the left expression. */
		es->es_soft++;
		concat = expr_doc(ex->ex_lhs, es, concat);
		es->es_soft--;
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* ( */
		if (ex->ex_rhs != NULL) {
			es->es_nest++;
			concat = expr_doc(ex->ex_rhs, es, concat);
			es->es_nest--;
		}
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ) */
		break;

	case EXPR_ARG: {
		struct doc *lhs = concat;

		if (ex->ex_lhs != NULL)
			lhs = expr_doc(ex->ex_lhs, es, concat);
		doc_token(ex->ex_tk, lhs);
		doc_alloc(DOC_LINE, lhs);
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, concat));
		doc_alloc(DOC_SOFTLINE, concat);
		es->es_soft++;
		concat = expr_doc(ex->ex_rhs, es, concat);
		es->es_soft--;
		break;
	}

	case EXPR_CAST:
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* ( */
		expr_doc(ex->ex_lhs, es, concat);
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ) */
		expr_doc(ex->ex_rhs, es, concat);
		break;

	case EXPR_SIZEOF:
		doc_token(ex->ex_tk, concat);
		if (ex->ex_sizeof) {
			if (ex->ex_tokens[0] != NULL)
				doc_token(ex->ex_tokens[0], concat);	/* ( */
		} else {
			doc_literal(" ", concat);
		}
		if (ex->ex_lhs != NULL)
			concat = expr_doc(ex->ex_lhs, es, concat);
		else
			expr_doc_tokens(ex, concat);
		if (ex->ex_sizeof) {
			if (ex->ex_tokens[1] != NULL)
				doc_token(ex->ex_tokens[1], concat);	/* ) */
		}
		break;

	case EXPR_CONCAT:
		concat = expr_doc(ex->ex_lhs, es, concat);
		doc_alloc(DOC_LINE, concat);
		concat = expr_doc(ex->ex_rhs, es, concat);
		break;

	case EXPR_LITERAL:
		doc_token(ex->ex_tk, concat);
		break;

	case EXPR_RECOVER:
		doc_append(ex->ex_dc, concat);
		/*
		 * The concat document is now responsible for freeing the
		 * recover document.
		 */
		ex->ex_dc = NULL;
		break;

	case EXPR_BRANCH:
		break;
	}

	/* Testing backdoor, see above. */
	if ((es->es_cf->cf_flags & CONFIG_FLAG_TEST) &&
	    ex->ex_type != EXPR_PARENS)
		doc_literal(")", concat);

	return concat;
}

static struct doc *
expr_doc_indent_parens(const struct expr_state *es, struct doc *dc)
{
	if ((es->es_ea->ea_flags & EXPR_EXEC_FLAG_PARENS) == 0 ||
	    es->es_parens > 1)
		return doc_alloc_indent(DOC_INDENT_PARENS, dc);
	return dc;
}

static struct doc *
expr_doc_tokens(const struct expr *ex, struct doc *dc)
{
	struct doc *concat;
	const struct token *tk = ex->ex_beg;
	int i = 0;
	int line = 1;

	for (;;) {
		concat = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		if (i++ > 0) {
			if (line)
				doc_alloc(DOC_LINE, concat);
			line = tk->tk_type != TOKEN_STAR;
		}

		doc_token(tk, concat);
		if (tk == ex->ex_end)
			break;

		tk = TAILQ_NEXT(tk, tk_entry);
		if (tk == NULL)
			break;
	}

	/*
	 * Nest any following tokens under the last group in order cause a
	 * refit.
	 */
	return doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, concat));
}

/*
 * Emit a soft line unless an expression above us has signalled that a more
 * suitable one has already been emitted.
 */
static struct doc *
__expr_doc_soft(const struct expr_state *es, struct doc *dc, const char *fun,
    int lno)
{
	if (es->es_soft > 0)
		return dc;

	dc = __doc_alloc(DOC_CONCAT, __doc_alloc(DOC_GROUP, dc, 0, fun, lno),
	    0, fun, lno);
	__doc_alloc(DOC_SOFTLINE, dc, 0, fun, lno);
	return dc;
}

static void
expr_state_init(struct expr_state *es, const struct expr_exec_arg *ea)
{
	memset(es, 0, sizeof(*es));
	es->es_cf = ea->ea_cf;
	es->es_ea = ea;
	es->es_lx = ea->ea_lx;
	es->es_stop = ea->ea_stop;
}

static const struct expr_rule *
expr_rule_find(const struct token *tk, int unary)
{
	static unsigned int nrules = sizeof(rules) / sizeof(*rules);
	unsigned int i;

	if (isliteral(tk))
		return unary ? &rules[0] : &rules[1];

	for (i = 2; i < nrules; i++) {
		const struct expr_rule *er = &rules[i];
		int isunary = (er->er_pc & PCUNARY) ? 1 : 0;

		if (er->er_type == tk->tk_type && unary == isunary)
			return &rules[i];
	}
	return NULL;
}

/*
 * Returns non-zero if a cast is present. The lexer must be positioned at the
 * beginning of an expression wrapped in parenthesis.
 */
static int
iscast(struct expr_state *es)
{
	struct lexer_state s;
	int cast = 0;

	lexer_peek_enter(es->es_lx, &s);
	if (lexer_if_type(es->es_lx, NULL) &&
	    lexer_if(es->es_lx, TOKEN_RPAREN, NULL) &&
	    expr_peek(es->es_ea))
		cast = 1;
	lexer_peek_leave(es->es_lx, &s);

	return cast;
}

static int
isliteral(const struct token *tk)
{
	return tk->tk_type == TOKEN_IDENT ||
	    tk->tk_type == TOKEN_LITERAL ||
	    tk->tk_type == TOKEN_STRING;
}
