#include "expr.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "cdefs.h"
#include "doc.h"
#include "lexer.h"
#include "options.h"
#include "style.h"
#include "token.h"

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
	EXPR_ASM,
};

struct expr {
	enum expr_type		 ex_type;
	struct token		*ex_tk;
	struct expr		*ex_lhs;
	struct expr		*ex_rhs;
	struct token		*ex_beg;
	struct token		*ex_end;
	struct token		*ex_tokens[2];

	union {
		struct expr		*ex_ternary;
		struct doc		*ex_dc;
		int			 ex_sizeof;
		TAILQ_HEAD(, expr)	 ex_concat;
	};

	TAILQ_ENTRY(expr)	 ex_entry;
};

struct expr_state;

struct expr_rule {
	enum expr_pc	 er_pc;
	int		 er_rassoc;
	int		 er_type;
	struct expr	*(*er_func)(struct expr_state *, struct expr *);
};

struct expr_state {
	struct expr_exec_arg	 es_ea;
#define es_st		es_ea.st
#define es_op		es_ea.op
#define es_lx		es_ea.lx
#define es_dc		es_ea.dc
#define es_stop		es_ea.stop
#define es_flags	es_ea.flags

	const struct expr_rule	*es_er;
	struct token		*es_tk;
	struct buffer		*es_bf;
	unsigned int		 es_depth;
	unsigned int		 es_nassign;	/* # nested binary assignments */
	unsigned int		 es_noparens;	/* parens indent disabled */
};

static struct expr	*expr_exec1(struct expr_state *, enum expr_pc);
static struct expr	*expr_exec_recover(struct expr_state *, unsigned int);

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
static struct doc	*expr_doc_call(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_binary(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_indent_parens(const struct expr_state *,
    struct doc *);
static int		 expr_doc_has_spaces(const struct expr *);
static unsigned int	 expr_doc_width(struct expr_state *,
    const struct doc *);

#define expr_doc_soft(a, b, c, d) \
	expr_doc_soft0((a), (b), (c), (d), __func__, __LINE__)
static struct doc	*expr_doc_soft0(struct expr *, struct expr_state *,
    struct doc *, int, const char *, int);

static void	expr_state_reset(struct expr_state *);

static const struct expr_rule	*expr_rule_find(const struct token *, int);

static int	iscast(struct expr_state *);
static int	isliteral(const struct token *);

static const char	*strexpr(enum expr_type);

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
	struct doc *dc, *expr, *indent, *optional;
	struct expr *ex;

	memset(&es, 0, sizeof(es));
	es.es_ea = *ea;

	ex = expr_exec1(&es, PC0);
	if (ex == NULL)
		return NULL;
	if (lexer_get_error(ea->lx)) {
		expr_free(ex);
		return NULL;
	}

	dc = doc_alloc(DOC_GROUP, ea->dc);
	optional = doc_alloc(DOC_OPTIONAL, dc);
	if (ea->indent > 0)
		indent = doc_alloc_indent(ea->indent, optional);
	else
		indent = doc_alloc(DOC_CONCAT, optional);
	if (ea->flags & EXPR_EXEC_SOFTLINE)
		doc_alloc(DOC_SOFTLINE, indent);
	if (ea->flags & EXPR_EXEC_HARDLINE) {
		doc_alloc(DOC_HARDLINE, indent);
		/* Needed since the hard line will disable optional line(s). */
		indent = doc_alloc(DOC_OPTIONAL, indent);
	}
	expr = expr_doc(ex, &es, indent);
	expr_free(ex);
	expr_state_reset(&es);
	return expr;
}

int
expr_peek(const struct expr_exec_arg *ea)
{
	struct expr_state es;
	struct expr *ex;
	int peek = 0;
	int error;

	memset(&es, 0, sizeof(es));
	es.es_ea = *ea;

	ex = expr_exec1(&es, PC0);
	error = lexer_get_error(es.es_lx);
	if (ex != NULL && error == 0)
		peek = 1;
	expr_free(ex);
	expr_state_reset(&es);
	return peek;
}

static struct expr *
expr_exec1(struct expr_state *es, enum expr_pc pc)
{
	const struct expr_rule *er;
	struct expr *ex = NULL;

	if (lexer_get_error(es->es_lx) ||
	    (!lexer_peek(es->es_lx, &es->es_tk) || es->es_tk == es->es_stop))
		return NULL;

	/* Only consider unary operators. */
	er = expr_rule_find(es->es_tk, 1);
	if (er == NULL || es->es_tk->tk_type == TOKEN_IDENT) {
		/*
		 * Even if a literal operator was found, let the parser recover
		 * before continuing. Otherwise, pointer types can be
		 * erroneously treated as a multiplication expression.
		 */
		ex = expr_exec_recover(es, 0);
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
expr_exec_recover(struct expr_state *es, unsigned int flags)
{
	struct doc *concat, *dc;
	struct expr *ex;
	int error;

	if (es->es_ea.recover == NULL)
		return NULL;

	dc = doc_alloc0(DOC_INDENT, NULL, -es->es_ea.indent, __func__,
	    __LINE__);
	concat = doc_alloc(DOC_CONCAT, dc);
	es->es_ea.flags |= flags;
	error = es->es_ea.recover(&es->es_ea, concat);
	es->es_ea.flags &= ~flags;
	if (error) {
		doc_free(dc);
		return NULL;
	}

	ex = expr_alloc(EXPR_RECOVER, es);
	ex->ex_dc = dc;
	return ex;
}

static struct expr *
expr_exec_binary(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex;
	enum expr_pc pc;
	int iscomma = es->es_tk->tk_type == TOKEN_COMMA;

	pc = PC(es->es_er->er_pc);
	if (es->es_er->er_rassoc)
		pc--;

	ex = expr_alloc(iscomma ? EXPR_ARG : EXPR_BINARY, es);
	ex->ex_lhs = lhs;
	ex->ex_rhs = expr_exec1(es, pc);
	return ex;
}

static struct expr *
expr_exec_concat(struct expr_state *es, struct expr *lhs)
{
	struct expr *ex, *rhs;

	assert(lhs != NULL);

	if (lhs->ex_type == EXPR_CONCAT) {
		ex = lhs;
	} else {
		ex = expr_alloc(EXPR_CONCAT, es);
		TAILQ_INIT(&ex->ex_concat);
		TAILQ_INSERT_TAIL(&ex->ex_concat, lhs, ex_entry);
	}
	rhs = expr_exec_literal(es, NULL);
	TAILQ_INSERT_TAIL(&ex->ex_concat, rhs, ex_entry);
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
			ex->ex_lhs = expr_exec_recover(es, EXPR_EXEC_CAST);
			if (lexer_expect(es->es_lx, TOKEN_RPAREN, &tk))
				ex->ex_tokens[1] = tk;	/* ) */
			ex->ex_rhs = expr_exec1(es, PC(es->es_er->er_pc));
			return ex;
		}

		ex = expr_alloc(EXPR_PARENS, es);
		ex->ex_lhs = expr_exec1(es, PC0);
	} else if (es->es_flags & EXPR_EXEC_ASM) {
		ex = expr_alloc(EXPR_ASM, es);
		ex->ex_lhs = lhs;
		/* Nested parenthesis must be handled as usual. */
		es->es_flags &= ~EXPR_EXEC_ASM;
		ex->ex_rhs = expr_exec1(es, PC0);
		es->es_flags |= EXPR_EXEC_ASM;
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

	ex->ex_lhs = expr_exec1(es, PC(es->es_er->er_pc));

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
	if (ex->ex_type == EXPR_TERNARY) {
		expr_free(ex->ex_ternary);
	} else if (ex->ex_type == EXPR_CONCAT) {
		struct expr *concat;

		while ((concat = TAILQ_FIRST(&ex->ex_concat)) != NULL) {
			TAILQ_REMOVE(&ex->ex_concat, concat, ex_entry);
			expr_free(concat);
		}
	} else if (ex->ex_type == EXPR_RECOVER) {
		doc_free(ex->ex_dc);
	}
	free(ex);
}

static struct doc *
expr_doc(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	struct doc *concat, *group;

	es->es_depth++;

	group = doc_alloc(DOC_GROUP, dc);
	doc_annotate(group, strexpr(ex->ex_type));
	concat = doc_alloc(DOC_CONCAT, group);

	/*
	 * Testing backdoor wrapping each expression in parenthesis used for
	 * validation of operator precedence.
	 */
	if ((es->es_op->op_flags & OPTIONS_TEST) &&
	    ex->ex_type != EXPR_PARENS)
		doc_literal("(", concat);

	switch (ex->ex_type) {
	case EXPR_UNARY:
		doc_token(ex->ex_tk, concat);
		if (style(es->es_st, AlignOperands) == Align)
			concat = doc_alloc_indent(expr_doc_width(es, concat),
			    concat);
		if (ex->ex_lhs != NULL)
			concat = expr_doc(ex->ex_lhs, es, concat);
		break;

	case EXPR_BINARY:
		concat = expr_doc_binary(ex, es, concat);
		break;

	case EXPR_TERNARY: {
		struct doc *ternary;

		ternary = expr_doc(ex->ex_lhs, es, concat);
		doc_alloc(DOC_LINE, ternary);
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], ternary);	/* ? */
		if (ex->ex_rhs != NULL)
			doc_alloc(DOC_LINE, ternary);

		/* The true expression can be empty, GNU extension. */
		ternary = concat;
		if (ex->ex_rhs != NULL) {
			ternary = expr_doc_soft(ex->ex_rhs, es, ternary, 2);
			doc_alloc(DOC_LINE, ternary);
		}

		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], ternary);	/* : */
		doc_alloc(DOC_LINE, ternary);
		concat = expr_doc_soft(ex->ex_ternary, es, ternary, 2);
		break;
	}

	case EXPR_PREFIX:
		doc_token(ex->ex_tk, concat);
		if (ex->ex_lhs != NULL)
			expr_doc(ex->ex_lhs, es, concat);
		break;

	case EXPR_POSTFIX:
		if (ex->ex_lhs != NULL)
			expr_doc(ex->ex_lhs, es, concat);
		doc_token(ex->ex_tk, concat);
		break;

	case EXPR_PARENS: {
		int noparens;

		noparens = es->es_depth == 1 &&
		    (es->es_op->op_flags & OPTIONS_SIMPLE) &&
		    (es->es_flags & EXPR_EXEC_NOPARENS);
		if (!noparens && ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* ( */
		if (style(es->es_st, AlignAfterOpenBracket) == Align)
			concat = doc_alloc_indent(1, concat);
		else
			concat = expr_doc_indent_parens(es, concat);
		if (ex->ex_lhs != NULL)
			concat = expr_doc(ex->ex_lhs, es, concat);
		if (!noparens && ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ) */
		break;
	}

	case EXPR_SQUARES:
		if (ex->ex_lhs != NULL)
			concat = expr_doc(ex->ex_lhs, es, concat);
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* [ */
		if (ex->ex_rhs != NULL)
			concat = expr_doc_soft(ex->ex_rhs, es, concat, 1);
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ] */
		break;

	case EXPR_FIELD:
		concat = expr_doc(ex->ex_lhs, es, concat);
		doc_token(ex->ex_tk, concat);
		if (ex->ex_rhs != NULL)
			concat = expr_doc(ex->ex_rhs, es, concat);
		break;

	case EXPR_CALL:
		concat = expr_doc_call(ex, es, concat);
		break;

	case EXPR_ARG: {
		struct doc *lhs = concat;

		if (ex->ex_lhs != NULL)
			lhs = expr_doc(ex->ex_lhs, es, concat);
		doc_token(ex->ex_tk, lhs);
		doc_alloc(DOC_LINE, lhs);
		if (ex->ex_rhs != NULL)
			concat = expr_doc_soft(ex->ex_rhs, es, concat, 3);
		break;
	}

	case EXPR_CAST:
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* ( */
		if (ex->ex_lhs != NULL)
			expr_doc(ex->ex_lhs, es, concat);
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ) */
		if (ex->ex_rhs != NULL)
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
		if (ex->ex_sizeof) {
			if (ex->ex_tokens[1] != NULL)
				doc_token(ex->ex_tokens[1], concat);	/* ) */
		}
		break;

	case EXPR_CONCAT: {
		struct expr *e;
		struct doc *tmp = NULL;

		TAILQ_FOREACH(e, &ex->ex_concat, ex_entry) {
			tmp = expr_doc(e, es, concat);
			if (TAILQ_NEXT(e, ex_entry) != NULL)
				doc_alloc(DOC_LINE, concat);
		}
		if (tmp != NULL)
			concat = tmp;
		break;
	}

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

	case EXPR_ASM: {
		struct token *lparen = ex->ex_tokens[0];
		struct token *rparen = ex->ex_tokens[1];

		expr_doc(ex->ex_lhs, es, concat);
		doc_alloc(DOC_LINE, concat);
		doc_token(lparen, concat);
		if (ex->ex_rhs != NULL)
			expr_doc(ex->ex_rhs, es, concat);
		doc_token(rparen, concat);
		break;
	}
	}

	/* Testing backdoor, see above. */
	if ((es->es_op->op_flags & OPTIONS_TEST) &&
	    ex->ex_type != EXPR_PARENS)
		doc_literal(")", concat);

	es->es_depth--;

	return concat;
}

static struct doc *
expr_doc_binary(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	const struct style *st = es->es_st;

	if (ex->ex_tk->tk_flags & TOKEN_FLAG_ASSIGN) {
		struct doc *lhs;

		es->es_nassign++;
		lhs = expr_doc(ex->ex_lhs, es, dc);
		doc_literal(" ", lhs);
		doc_token(ex->ex_tk, lhs);
		doc_literal(" ", lhs);
		if (style(st, AlignOperands) == Align) {
			int w;

			if (!token_has_line(ex->ex_tk, 1)) {
				w = expr_doc_width(es, dc);
				dc = doc_alloc_indent(w, dc);
			} else {
				w = -es->es_ea.indent +
				    style(st, ContinuationIndentWidth);
				dc = doc_alloc_indent(w, dc);
			}
		}
		if (ex->ex_rhs != NULL) {
			/*
			 * Same semantics as variable declarations, do not break
			 * after the assignment operator.
			 */
			if (es->es_nassign > 1)
				dc = expr_doc_soft(ex->ex_rhs, es, dc, 2);
			else
				dc = expr_doc(ex->ex_rhs, es, dc);
		}
		es->es_nassign--;
	} else if (style(st, BreakBeforeBinaryOperators) == NonAssignment) {
		struct token *nx, *pv;
		unsigned int lno;
		int dospace;

		pv = token_prev(ex->ex_tk);
		nx = token_next(ex->ex_tk);
		lno = nx->tk_lno - ex->ex_tk->tk_lno;
		if (token_trim(ex->ex_tk) > 0 && lno == 1) {
			/* Move operator to next line. */
			token_move_suffixes(ex->ex_tk, pv);
			token_add_optline(pv);
		}

		expr_doc(ex->ex_lhs, es, dc);
		dospace = expr_doc_has_spaces(ex);
		if (dospace)
			doc_alloc(DOC_LINE, dc);
		dc = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(DOC_SOFTLINE, dc);
		doc_token(ex->ex_tk, dc);
		if (dospace)
			doc_literal(" ", dc);
		if (lno <= 1)
			dc = doc_alloc_indent(expr_doc_width(es, dc), dc);
		if (ex->ex_rhs != NULL)
			dc = expr_doc(ex->ex_rhs, es, dc);
	} else {
		struct doc *lhs;
		struct token *pv;
		unsigned int lno;
		int dospace;

		pv = token_prev(ex->ex_tk);
		lno = ex->ex_tk->tk_lno - pv->tk_lno;
		if (token_trim(pv) > 0 && lno == 1) {
			/* Move operator to previous line. */
			token_move_suffixes(pv, ex->ex_tk);
			token_add_optline(ex->ex_tk);
		}

		lhs = expr_doc(ex->ex_lhs, es, dc);
		dospace = expr_doc_has_spaces(ex);
		if (dospace)
			doc_literal(" ", lhs);
		doc_token(ex->ex_tk, lhs);
		dc = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		if (dospace)
			doc_alloc(DOC_LINE, dc);
		if (ex->ex_rhs != NULL)
			dc = expr_doc_soft(ex->ex_rhs, es, dc, 2);
	}

	return dc;
}

static struct doc *
expr_doc_call(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	struct doc *parent = dc;
	struct token *lparen = ex->ex_tokens[0];
	struct token *rparen = ex->ex_tokens[1];

	es->es_noparens++;
	dc = expr_doc(ex->ex_lhs, es, dc);
	es->es_noparens--;
	if (lparen != NULL)
		doc_token(lparen, dc);
	if (style(es->es_st, AlignAfterOpenBracket) == Align) {
		int w;

		if (lparen == NULL || !token_has_line(lparen, 1)) {
			w = expr_doc_width(es, parent);
			dc = doc_alloc_indent(w, dc);
		} else {
			w = -es->es_ea.indent +
			    style(es->es_st, ContinuationIndentWidth);
			dc = doc_alloc_indent(w, dc);
		}
	}
	if (ex->ex_rhs != NULL) {
		if (rparen != NULL) {
			struct token *pv;

			/* Never break before the closing parens. */
			pv = token_prev(rparen);
			if (pv != NULL)
				token_trim(pv);
		}

		dc = expr_doc_soft(ex->ex_rhs, es, dc, 2);
	}
	if (rparen != NULL)
		doc_token(rparen, dc);
	return dc;
}

static struct doc *
expr_doc_indent_parens(const struct expr_state *es, struct doc *dc)
{
	if (es->es_noparens > 0)
		return dc;
	return doc_alloc_indent(DOC_INDENT_PARENS, dc);
}

static int
expr_doc_has_spaces(const struct expr *ex)
{
	struct token *pv;

	/*
	 * Only applicable to binary operators where spaces around it are
	 * permitted.
	 */
	if ((ex->ex_tk->tk_flags & TOKEN_FLAG_SPACE) == 0)
		return 1;

	if (token_has_spaces(ex->ex_tk))
		return 1;
	pv = token_prev(ex->ex_tk);
	if (token_has_spaces(pv))
		return 1;
	return 0;
}

static unsigned int
expr_doc_width(struct expr_state *es, const struct doc *dc)
{
	if (es->es_bf == NULL)
		es->es_bf = buffer_alloc(1024);
	return doc_width(dc, es->es_bf, es->es_st, es->es_op);
}

/*
 * Insert a soft line before the given expression, unless a more suitable one is
 * nested under the same expression.
 */
static struct doc *
expr_doc_soft0(struct expr *ex, struct expr_state *es, struct doc *dc,
    int weight, const char *fun, int lno)
{
	struct doc *concat, *parent, *softline;

	dc = doc_alloc0(DOC_CONCAT, doc_alloc0(DOC_GROUP, dc, 0, fun, lno),
	    0, fun, lno);
	softline = doc_alloc0(DOC_SOFTLINE, dc, weight, fun, lno);
	parent = doc_alloc(DOC_CONCAT, dc);
	concat = expr_doc(ex, es, parent);
	/*
	 * Honor the soft line with the highest weight. Using greater than or
	 * equal is of importance as we want to maximize the column utilisation.
	 */
	if (doc_max(parent) >= weight)
		doc_remove(softline, dc);

	return concat;
}

static void
expr_state_reset(struct expr_state *es)
{
	buffer_free(es->es_bf);
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
	if (lexer_if_type(es->es_lx, NULL, LEXER_TYPE_CAST) &&
	    lexer_if(es->es_lx, TOKEN_RPAREN, NULL) &&
	    expr_peek(&es->es_ea))
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

static const char *
strexpr(enum expr_type type)
{
	switch (type) {
#define CASE(t) case t: return #t
	CASE(EXPR_UNARY);
	CASE(EXPR_BINARY);
	CASE(EXPR_TERNARY);
	CASE(EXPR_PREFIX);
	CASE(EXPR_POSTFIX);
	CASE(EXPR_PARENS);
	CASE(EXPR_SQUARES);
	CASE(EXPR_FIELD);
	CASE(EXPR_CALL);
	CASE(EXPR_ARG);
	CASE(EXPR_CAST);
	CASE(EXPR_SIZEOF);
	CASE(EXPR_CONCAT);
	CASE(EXPR_LITERAL);
	CASE(EXPR_RECOVER);
	CASE(EXPR_ASM);
	}
	return NULL;
}
