#include "expr.h"

#include "config.h"

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "libks/buffer.h"
#include "libks/consistency.h"
#include "libks/vector.h"

#include "alloc.h"
#include "cdefs.h"
#include "doc.h"
#include "lexer.h"
#include "options.h"
#include "ruler.h"
#include "simple.h"
#include "style.h"
#include "token.h"

enum expr_mode {
	EXPR_MODE_EXEC,
	EXPR_MODE_PEEK,
};

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

#define FOR_EXPR_TYPES(OP)						\
	OP(EXPR_UNARY)							\
	OP(EXPR_BINARY)							\
	OP(EXPR_TERNARY)						\
	OP(EXPR_PREFIX)							\
	OP(EXPR_POSTFIX)						\
	OP(EXPR_PARENS)							\
	OP(EXPR_SQUARES)						\
	OP(EXPR_FIELD)							\
	OP(EXPR_CALL)							\
	OP(EXPR_ARG)							\
	OP(EXPR_CAST)							\
	OP(EXPR_SIZEOF)							\
	OP(EXPR_CONCAT)							\
	OP(EXPR_LITERAL)						\
	OP(EXPR_RECOVER)

enum expr_type {
#define OP(type) type,
	FOR_EXPR_TYPES(OP)
#undef OP
};

struct expr {
	enum expr_type	 ex_type;
	struct token	*ex_tk;
	struct expr	*ex_lhs;
	struct expr	*ex_rhs;
	struct token	*ex_tokens[2];

	union {
		struct expr		*ex_ternary;
		struct doc		*ex_dc;
		int			 ex_sizeof;
		VECTOR(struct expr *)	 ex_concat;
	};
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
#define es_flags	es_ea.flags

	const struct expr_rule	*es_er;
	struct token		*es_tk;
	struct buffer		*es_bf;
	unsigned int		 es_depth;
	unsigned int		 es_nassign;	/* # nested binary assignments */
	unsigned int		 es_ncalls;	/* # nested calls */
	unsigned int		 es_noparens;	/* parens indent disabled */
	unsigned int		 es_col;	/* ruler column */
};

static struct expr	*expr_exec1(struct expr_state *, enum expr_pc);
static struct expr	*expr_exec_recover(struct expr_state *);
static struct expr	*expr_exec_recover_cast(struct expr_state *);

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

static int	expr_exec_peek(struct expr_state *, struct token **);

static struct expr	*expr_alloc(enum expr_type, const struct expr_state *);
static void		 expr_free(struct expr *);

static struct doc	*expr_doc(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_unary(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_binary(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_parens(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_field(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_call(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_arg(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_sizeof(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_concat(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_ternary(struct expr *, struct expr_state *,
    struct doc *);
static struct doc	*expr_doc_recover(struct expr *, struct expr_state *,
    struct doc *);

#define expr_doc_align(a, b, c, d) \
	expr_doc_align0((a), (b), (c), (d), __func__, __LINE__)
static struct doc	*expr_doc_align0(struct expr *,
    struct expr_state *, struct doc *, unsigned int, const char *, int);

#define expr_doc_align_disable(a, b, c, d) \
	expr_doc_align_disable0((a), (b), (c), (d), __func__, __LINE__)
static struct doc	*expr_doc_align_disable0(struct expr *,
    struct expr_state *, struct doc *, unsigned int, const char *, int);

static void	expr_doc_align_init(struct expr_state *,
    struct doc_minimize *, size_t);

static struct doc	*expr_doc_indent_parens(const struct expr_state *,
    struct doc *);
static int		 expr_doc_has_spaces(const struct expr *);
static unsigned int	 expr_doc_width(struct expr_state *,
    const struct doc *);

#define expr_doc_soft(a, b, c, d) \
	expr_doc_soft0((a), (b), (c), (d), __func__, __LINE__)
static struct doc	*expr_doc_soft0(struct expr *, struct expr_state *,
    struct doc *, int, const char *, int);

static void	expr_state_init(struct expr_state *,
    const struct expr_exec_arg *, enum expr_mode);
static void	expr_state_reset(struct expr_state *);

static const struct expr_rule	*expr_find_rule(const struct token *, int);

static void	token_move_next_line(struct token *);
static void	token_move_prev_line(struct token *);

static const char	*expr_type_str(enum expr_type);

static const struct expr_rule rules[] = {
	{ PC0 | PCUNARY,	0,	TOKEN_IDENT,			expr_exec_literal },
	{ PC0 | PCUNARY,	0,	TOKEN_LITERAL,			expr_exec_literal },
	{ PC0 | PCUNARY,	0,	TOKEN_STRING,			expr_exec_literal },
	{ PC1,			0,	TOKEN_COMMA,			expr_exec_binary },
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
	{ PC12,			0,	TOKEN_IDENT,			expr_exec_concat },
	{ PC12,			0,	TOKEN_LITERAL,			expr_exec_concat },
	{ PC12,			0,	TOKEN_STRING,			expr_exec_concat },
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

/* Table for constant time expr rules lookup. */
static const struct expr_rule *table_rules[TOKEN_NONE + 1][2];

/*
 * Weights for emitted softline(s) through expr_doc_soft(). Several softline(s)
 * can be emitted per expression in which the one with highest weight is
 * favored, removing the others.
 */
static const struct {
	int arg;
	int binary;
	int call;
	int call_args;
	int ternary;
} soft_weights = {
#define SOFT_MAX 3
	.arg		= SOFT_MAX,	/* after comma, before argument */
	.binary		= SOFT_MAX - 1,	/* after binary operator */
	.call		= SOFT_MAX - 1,	/* before call */
	.call_args	= SOFT_MAX - 1,	/* after lparen, before call arguments */
	.ternary	= SOFT_MAX - 1,	/* before ternary true/false expr */
};

void
expr_init(void)
{
	size_t nrules = sizeof(rules) / sizeof(rules[0]);
	size_t i;

	for (i = 0; i < nrules; i++) {
		const struct expr_rule *er = &rules[i];

		table_rules[er->er_type][(er->er_pc & PCUNARY) ? 1 : 0] = er;
	}
}

void
expr_shutdown(void)
{
}

struct doc *
expr_exec(const struct expr_exec_arg *ea)
{
	struct expr_state es;
	struct doc *dc, *expr, *indent, *optional;
	struct expr *ex;

	expr_state_init(&es, ea, EXPR_MODE_EXEC);

	ex = expr_exec1(&es, PC0);
	if (ex == NULL)
		return NULL;
	if (lexer_get_error(ea->lx)) {
		expr_free(ex);
		return NULL;
	}

	dc = doc_max_lines(1, ea->dc);
	dc = doc_alloc(DOC_SCOPE, dc);
	dc = doc_alloc(DOC_GROUP, dc);
	optional = doc_alloc(DOC_OPTIONAL, dc);
	if (ea->indent > 0)
		indent = doc_indent(ea->indent, optional);
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
expr_peek(const struct expr_exec_arg *ea, struct token **tk)
{
	struct expr_state es;
	struct lexer_state s;
	struct expr *ex;
	struct lexer *lx = ea->lx;
	int peek = 0;

	expr_state_init(&es, ea, EXPR_MODE_PEEK);

	lexer_peek_enter(lx, &s);
	ex = expr_exec1(&es, PC0);
	if (ex != NULL && lexer_get_error(lx) == 0 && lexer_back(lx, tk))
		peek = 1;
	lexer_peek_leave(lx, &s);
	expr_free(ex);
	expr_state_reset(&es);
	return peek;
}

static struct expr *
expr_exec1(struct expr_state *es, enum expr_pc pc)
{
	const struct expr_rule *er;
	struct expr *ex = NULL;
	struct token *tk;

	if (!expr_exec_peek(es, &tk))
		return NULL;

	/* Only consider unary operators. */
	er = expr_find_rule(tk, 1);
	if (er == NULL || tk->tk_type == TOKEN_IDENT) {
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
		ex = er->er_func(es, NULL);
	}
	if (ex == NULL)
		return NULL;

	for (;;) {
		struct expr *tmp;

		if (!expr_exec_peek(es, &tk))
			break;

		/* Only consider binary operators. */
		er = expr_find_rule(tk, 0);
		if (er == NULL)
			break;
		es->es_er = er;

		if (pc >= PC(er->er_pc))
			break;

		if (!lexer_pop(es->es_lx, &es->es_tk))
			break;
		tmp = er->er_func(es, ex);
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
	const struct expr_exec_arg *ea = &es->es_ea;
	struct doc *dc;
	struct expr *ex;

	dc = doc_alloc(DOC_CONCAT, NULL);
	if (!ea->callbacks.recover(ea, dc, ea->callbacks.arg)) {
		doc_free(dc);
		return NULL;
	}
	ex = expr_alloc(EXPR_RECOVER, es);
	ex->ex_dc = dc;
	return ex;
}

static struct expr *
expr_exec_recover_cast(struct expr_state *es)
{
	const struct expr_exec_arg *ea = &es->es_ea;
	struct doc *dc;
	struct expr *ex;

	dc = doc_alloc(DOC_CONCAT, NULL);
	if (!ea->callbacks.recover_cast(ea, dc, ea->callbacks.arg)) {
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
	struct expr **dst;
	struct expr *ex, *rhs;

	assert(lhs != NULL);

	if (lhs->ex_type == EXPR_CONCAT) {
		ex = lhs;
	} else {
		ex = expr_alloc(EXPR_CONCAT, es);
		if (VECTOR_INIT(ex->ex_concat))
			err(1, NULL);
		dst = VECTOR_ALLOC(ex->ex_concat);
		if (dst == NULL)
			err(1, NULL);
		*dst = lhs;
	}
	rhs = expr_exec_literal(es, NULL);
	dst = VECTOR_ALLOC(ex->ex_concat);
	if (dst == NULL)
		err(1, NULL);
	*dst = rhs;
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
		struct expr *cast;

		cast = expr_exec_recover_cast(es);
		if (cast != NULL) {
			ex = expr_alloc(EXPR_CAST, es);
			ex->ex_tokens[0] = tk;	/* ( */
			ex->ex_lhs = cast;
			if (lexer_expect(es->es_lx, TOKEN_RPAREN, &tk))
				ex->ex_tokens[1] = tk;	/* ) */
			ex->ex_rhs = expr_exec1(es, PC(es->es_er->er_pc));
			return ex;
		}

		ex = expr_alloc(EXPR_PARENS, es);
		ex->ex_lhs = expr_exec1(es, PC0);
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
		ex->ex_lhs = expr_exec1(es, PC0);
		if (lexer_expect(es->es_lx, TOKEN_RPAREN, &tk))
			ex->ex_tokens[1] = tk;	/* ) */
	} else {
		ex->ex_lhs = expr_exec1(es, PC(es->es_er->er_pc));
	}

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

static int
expr_exec_peek(struct expr_state *es, struct token **tk)
{
	struct lexer *lx = es->es_lx;

	if (lexer_get_error(lx) ||
	    !lexer_peek(lx, &es->es_tk) ||
	    es->es_tk == es->es_ea.stop ||
	    es->es_tk->tk_type == LEXER_EOF)
		return 0;

	*tk = es->es_tk;
	return 1;
}

static struct expr *
expr_alloc(enum expr_type type, const struct expr_state *es)
{
	struct expr *ex;

	ex = ecalloc(1, sizeof(*ex));
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
		while (!VECTOR_EMPTY(ex->ex_concat)) {
			struct expr **tail;

			tail = VECTOR_POP(ex->ex_concat);
			expr_free(*tail);
		}
		VECTOR_FREE(ex->ex_concat);
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
	doc_annotate(group, expr_type_str(ex->ex_type));
	concat = doc_alloc(DOC_CONCAT, group);

	/*
	 * Testing backdoor wrapping each expression in parenthesis used for
	 * validation of operator precedence.
	 */
	if (es->es_op->op_flags.test && ex->ex_type != EXPR_PARENS)
		doc_literal("(", concat);

	switch (ex->ex_type) {
	case EXPR_UNARY:
		concat = expr_doc_unary(ex, es, concat);
		break;

	case EXPR_BINARY:
		concat = expr_doc_binary(ex, es, concat);
		break;

	case EXPR_TERNARY:
		concat = expr_doc_ternary(ex, es, concat);
		break;

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

	case EXPR_PARENS:
		concat = expr_doc_parens(ex, es, concat);
		break;

	case EXPR_SQUARES:
		if (ex->ex_lhs != NULL)
			concat = expr_doc(ex->ex_lhs, es, concat);
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], concat);	/* [ */
		if (ex->ex_rhs != NULL)
			concat = expr_doc(ex->ex_rhs, es, concat);
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], concat);	/* ] */
		break;

	case EXPR_FIELD:
		concat = expr_doc_field(ex, es, concat);
		break;

	case EXPR_CALL:
		concat = expr_doc_call(ex, es, concat);
		break;

	case EXPR_ARG:
		concat = expr_doc_arg(ex, es, concat);
		break;

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
		concat = expr_doc_sizeof(ex, es, concat);
		break;

	case EXPR_CONCAT:
		concat = expr_doc_concat(ex, es, concat);
		break;

	case EXPR_LITERAL:
		doc_token(ex->ex_tk, concat);
		break;

	case EXPR_RECOVER:
		concat = expr_doc_recover(ex, es, concat);
		break;
	}

	/* Testing backdoor, see above. */
	if (es->es_op->op_flags.test && ex->ex_type != EXPR_PARENS)
		doc_literal(")", concat);

	es->es_depth--;

	return concat;
}

static struct doc *
expr_doc_unary(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	doc_token(ex->ex_tk, dc);
	if (ex->ex_lhs != NULL)
		dc = expr_doc(ex->ex_lhs, es, dc);
	return dc;
}

static struct doc *
expr_doc_binary(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	const struct style *st = es->es_st;
	int doalign = style(st, AlignOperands) == Align;

	if (ex->ex_tk->tk_flags & TOKEN_FLAG_ASSIGN) {
		struct doc *lhs;

		es->es_nassign++;
		lhs = expr_doc(ex->ex_lhs, es, dc);
		doc_literal(" ", lhs);
		doc_token(ex->ex_tk, lhs);
		doc_literal(" ", lhs);
		if (ex->ex_rhs != NULL) {
			if (doalign) {
				dc = token_has_line(ex->ex_tk, 1) ?
				    expr_doc_align_disable(ex, es, dc, 0) :
				    expr_doc_align(ex, es, dc, 0);
			}

			/*
			 * Same semantics as variable declarations, do not break
			 * after the assignment operator.
			 */
			if (es->es_nassign > 1) {
				dc = expr_doc_soft(ex->ex_rhs, es, dc,
				    soft_weights.binary);
			} else {
				dc = expr_doc(ex->ex_rhs, es, dc);
			}
		}
		es->es_nassign--;
	} else if (style(st, BreakBeforeBinaryOperators) == NonAssignment ||
	    style(st, BreakBeforeBinaryOperators) == All) {
		struct doc *lhs;
		int dospace;

		if (doalign)
			dc = doc_indent(DOC_INDENT_WIDTH, dc);

		token_move_next_line(ex->ex_tk);
		lhs = expr_doc(ex->ex_lhs, es, dc);
		dospace = expr_doc_has_spaces(ex);
		if (dospace)
			doc_alloc(DOC_LINE, lhs);
		dc = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(DOC_SOFTLINE, dc);
		doc_token(ex->ex_tk, dc);
		if (dospace)
			doc_literal(" ", dc);
		if (ex->ex_rhs != NULL)
			dc = expr_doc(ex->ex_rhs, es, dc);
	} else {
		struct doc *lhs;
		int dospace;

		if (doalign)
			dc = doc_indent(DOC_INDENT_WIDTH, dc);

		token_move_prev_line(ex->ex_tk);
		lhs = expr_doc(ex->ex_lhs, es, dc);
		dospace = expr_doc_has_spaces(ex);
		if (dospace)
			doc_literal(" ", lhs);
		doc_token(ex->ex_tk, lhs);
		dc = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		/*
		 * If the operator is followed by a trailing comment and a new
		 * line, ensure that the new line is honored even when optional
		 * new line(s) are ignored.
		 */
		if (token_has_suffix(ex->ex_tk, TOKEN_COMMENT) &&
		    token_has_line(ex->ex_tk, 1))
			doc_alloc(DOC_HARDLINE, lhs);
		else if (dospace)
			doc_alloc(DOC_LINE, dc);
		if (ex->ex_rhs != NULL) {
			dc = expr_doc_soft(ex->ex_rhs, es, dc,
			    soft_weights.binary);
		}
	}

	return dc;
}

static struct doc *
expr_doc_parens(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	SIMPLE_COOKIE simple = {0};

	if (simple_enter(es->es_ea.si, SIMPLE_EXPR_PARENS,
	    es->es_depth == 1 ? 0 : SIMPLE_IGNORE, &simple)) {
		if (ex->ex_lhs != NULL)
			dc = expr_doc(ex->ex_lhs, es, dc);
	} else {
		struct token *pv;

		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], dc);	/* ( */
		if (ex->ex_tokens[1] != NULL &&
		    (pv = token_prev(ex->ex_tokens[1])) != NULL)
			token_trim(pv);

		if (style(es->es_st, AlignAfterOpenBracket) == Align)
			dc = doc_indent(DOC_INDENT_WIDTH, dc);
		else
			dc = expr_doc_indent_parens(es, dc);
		if (ex->ex_lhs != NULL)
			dc = expr_doc(ex->ex_lhs, es, dc);
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], dc);	/* ) */
	}

	return dc;
}

static struct doc *
expr_doc_field(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	dc = expr_doc(ex->ex_lhs, es, dc);
	token_trim(ex->ex_tk);
	doc_token(ex->ex_tk, dc);
	if (ex->ex_rhs != NULL)
		dc = expr_doc(ex->ex_rhs, es, dc);
	return dc;
}

static struct doc *
expr_doc_call(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	struct token *lparen = ex->ex_tokens[0];
	struct token *rparen = ex->ex_tokens[1];
	int fold_rparens = 0;

	es->es_ncalls++;

	es->es_noparens++;
	if (es->es_ncalls > 1)
		dc = expr_doc_soft(ex->ex_lhs, es, dc, soft_weights.call);
	else
		dc = expr_doc(ex->ex_lhs, es, dc);
	es->es_noparens--;

	doc_token(lparen, dc);
	if (ex->ex_rhs != NULL) {
		if (rparen != NULL) {
			struct token *pv;

			/* Try to not break before the closing parens. */
			pv = token_prev(rparen);
			if (pv != NULL && !token_has_c99_comment(pv)) {
				token_trim(pv);
				fold_rparens = 1;
			}
		}

		if (style(es->es_st, AlignAfterOpenBracket) == Align) {
			unsigned int indent;

			indent = es->es_ea.indent | DOC_INDENT_NEWLINE;
			dc = token_has_line(lparen, 1) ?
			    expr_doc_align_disable(ex, es, dc, indent) :
			    expr_doc_align(ex, es, dc, indent);
		}
		dc = expr_doc_soft(ex->ex_rhs, es, dc, soft_weights.call_args);
	}
	if (!fold_rparens) {
		/*
		 * Must break before closing parens, indentation for the outer
		 * scope is expected.
		 */
		dc = doc_dedent(es->es_ea.indent, dc);
	}
	if (rparen != NULL)
		doc_token(rparen, dc);

	es->es_ncalls--;

	return dc;
}

static struct doc *
expr_doc_arg(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	struct doc *lhs = dc;

	if (ex->ex_lhs != NULL)
		lhs = expr_doc(ex->ex_lhs, es, dc);
	doc_token(ex->ex_tk, lhs);
	if (es->es_flags & EXPR_EXEC_ALIGN) {
		unsigned int w;

		w = expr_doc_width(es, es->es_col == 0 ? es->es_dc : lhs);
		ruler_insert(es->es_ea.rl, ex->ex_tk, lhs, ++es->es_col, w, 0);
	} else {
		doc_alloc(DOC_LINE, lhs);
	}
	if (ex->ex_rhs != NULL)
		dc = expr_doc_soft(ex->ex_rhs, es, dc, soft_weights.arg);
	return dc;
}

static struct doc *
expr_doc_sizeof(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	doc_token(ex->ex_tk, dc);
	if (ex->ex_sizeof) {
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], dc); /* ( */
	} else {
		SIMPLE_COOKIE simple = {0};

		if (simple_enter(es->es_ea.si, SIMPLE_EXPR_SIZEOF, 0, &simple))
			doc_literal("(", dc);
		else
			doc_literal(" ", dc);
	}
	if (ex->ex_lhs != NULL)
		dc = expr_doc(ex->ex_lhs, es, dc);
	if (ex->ex_sizeof) {
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], dc); /* ) */
	} else {
		SIMPLE_COOKIE simple = {0};

		if (simple_enter(es->es_ea.si, SIMPLE_EXPR_SIZEOF, 0, &simple))
			doc_literal(")", dc);
	}
	return dc;
}

static struct doc *
expr_doc_concat(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	struct token *pv;
	size_t i, n;
	int doalign;

	pv = token_prev(ex->ex_concat[0]->ex_tk);
	doalign = style(es->es_st, AlignOperands) == Align &&
	    !token_has_line(pv, 1);
	if (doalign)
		dc = expr_doc_align(ex, es, dc, 0);
	n = VECTOR_LENGTH(ex->ex_concat);
	for (i = 0; i < n; i++) {
		struct expr *e = ex->ex_concat[i];
		struct doc *tmp;

		tmp = expr_doc(e, es, dc);
		if (i + 1 < n)
			doc_alloc(DOC_LINE, tmp);
		/* Nest subsequent expressions under the first one. */
		if (i == 0)
			dc = tmp;
	}
	return dc;
}

static struct doc *
expr_doc_ternary(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	if (style(es->es_st, BreakBeforeTernaryOperators) == True) {
		struct doc *cond, *lhs, *rhs;

		if (ex->ex_tokens[0] != NULL)
			token_move_next_line(ex->ex_tokens[0]);
		if (ex->ex_tokens[1] != NULL)
			token_move_next_line(ex->ex_tokens[1]);
		cond = expr_doc(ex->ex_lhs, es, dc);
		doc_alloc(DOC_LINE, cond);
		if (style(es->es_st, AlignOperands) == Align)
			dc = expr_doc_align(ex, es, dc, 0);

		lhs = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(DOC_SOFTLINE, lhs);
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], lhs);	/* ? */
		/* The lhs expression can be empty, GNU extension. */
		if (ex->ex_rhs != NULL) {
			doc_literal(" ", lhs);
			lhs = expr_doc_soft(ex->ex_rhs, es, lhs,
			    soft_weights.ternary);
			doc_alloc(DOC_LINE, lhs);
		}

		rhs = doc_alloc(DOC_CONCAT, doc_alloc(DOC_GROUP, dc));
		doc_alloc(DOC_SOFTLINE, rhs);
		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], rhs);	/* : */
		doc_literal(" ", rhs);
		return expr_doc(ex->ex_ternary, es, rhs);
	} else {
		struct doc *ternary;

		if (ex->ex_tokens[0] != NULL)
			token_move_prev_line(ex->ex_tokens[0]);
		if (ex->ex_tokens[1] != NULL)
			token_move_prev_line(ex->ex_tokens[1]);
		ternary = expr_doc(ex->ex_lhs, es, dc);
		doc_alloc(DOC_LINE, ternary);
		if (ex->ex_tokens[0] != NULL)
			doc_token(ex->ex_tokens[0], ternary);	/* ? */

		/* The true expression can be empty, GNU extension. */
		if (ex->ex_rhs != NULL) {
			doc_alloc(DOC_LINE, ternary);
			ternary = expr_doc_soft(ex->ex_rhs, es, dc,
			    soft_weights.ternary);
			doc_alloc(DOC_LINE, ternary);
		} else {
			ternary = dc;
		}

		if (ex->ex_tokens[1] != NULL)
			doc_token(ex->ex_tokens[1], ternary);	/* : */
		doc_alloc(DOC_LINE, ternary);

		return expr_doc_soft(ex->ex_ternary, es, dc,
		    soft_weights.ternary);
	}
}

static struct doc *
expr_doc_recover(struct expr *ex, struct expr_state *es, struct doc *dc)
{
	if (ex->ex_tk->tk_type == TOKEN_LBRACE)
		dc = expr_doc_align_disable(ex, es, dc, 0);

	doc_append(ex->ex_dc, dc);
	/*
	 * The concat document is now responsible for freeing the recover
	 * document.
	 */
	ex->ex_dc = NULL;
	return dc;
}

/*
 * Favor alignment with what we got so far on the current line, assuming it does
 * not cause exceesive new line(s). Otherwise, fallback to regular indentation.
 */
static struct doc *
expr_doc_align0(struct expr *UNUSED(ex), struct expr_state *es, struct doc *dc,
    unsigned int indent, const char *fun, int lno)
{
	struct doc_minimize minimizers[2];

	expr_doc_align_init(es, minimizers, 2);
	minimizers[0].indent = DOC_INDENT_WIDTH;
	minimizers[1].indent = indent;
	return doc_minimize0(minimizers, 2, dc, fun, lno);
}

static struct doc *
expr_doc_align_disable0(struct expr *UNUSED(ex), struct expr_state *es,
    struct doc *dc, unsigned int indent, const char *fun, int lno)
{
	struct doc_minimize minimizers[2];

	expr_doc_align_init(es, minimizers, 2);
	minimizers[1].indent = indent;
	minimizers[1].flags |= DOC_MINIMIZE_FORCE;
	return doc_minimize0(minimizers, 2, dc, fun, lno);
}

static void
expr_doc_align_init(struct expr_state *UNUSED(es),
    struct doc_minimize *minimizers, size_t nminimizers)
{
	size_t i;

	memset(minimizers, 0, sizeof(*minimizers) * nminimizers);
	for (i = 0; i < nminimizers; i++)
		minimizers[i].type = DOC_MINIMIZE_INDENT;
}

static struct doc *
expr_doc_indent_parens(const struct expr_state *es, struct doc *dc)
{
	if (es->es_noparens > 0)
		return dc;
	return doc_indent(DOC_INDENT_PARENS, dc);
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
	if (es->es_bf == NULL) {
		es->es_bf = buffer_alloc(1024);
		if (es->es_bf == NULL)
			err(1, NULL);
	}
	return doc_width(&(struct doc_exec_arg){
	    .dc	= dc,
	    .bf	= es->es_bf,
	    .st	= es->es_st,
	    .op	= es->es_op,
	});
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

	if (es->es_flags & EXPR_EXEC_NOSOFT)
		return expr_doc(ex, es, dc);

	dc = doc_alloc0(DOC_CONCAT, doc_alloc0(DOC_GROUP, dc, 0, fun, lno),
	    0, fun, lno);
	softline = doc_alloc0(DOC_SOFTLINE, dc, weight, fun, lno);
	parent = doc_alloc(DOC_CONCAT, dc);
	concat = expr_doc(ex, es, parent);
	/*
	 * Honor the soft line with the highest weight. Using greater than or
	 * equal is of importance as we want to maximize column utilisation,
	 * effectively favoring nested soft line(s).
	 */
	if (weight < SOFT_MAX && doc_max(parent) >= weight)
		doc_remove(softline, dc);

	return concat;
}

static void
expr_state_init(struct expr_state *es, const struct expr_exec_arg *ea,
    enum expr_mode MAYBE_UNUSED(mode))
{
	ASSERT_CONSISTENCY(mode == EXPR_MODE_EXEC, ea->si);
	ASSERT_CONSISTENCY(ea->flags & EXPR_EXEC_ALIGN, ea->rl);
	ASSERT_CONSISTENCY(mode == EXPR_MODE_EXEC, ea->dc);

	memset(es, 0, sizeof(*es));
	es->es_ea = *ea;
}

static void
expr_state_reset(struct expr_state *es)
{
	buffer_free(es->es_bf);
}

static const struct expr_rule *
expr_find_rule(const struct token *tk, int unary)
{
	return table_rules[tk->tk_type][unary];
}

/*
 * Move the token to the next line if not already correctly placed.
 */
static void
token_move_next_line(struct token *tk)
{
	struct token *nx, *pv;

	if (token_trim(tk) == 0)
		return;

	pv = token_prev(tk);
	token_move_suffixes(tk, pv);
	token_add_optline(pv);

	nx = token_next(tk);
	token_move_prefixes(nx, tk);
}

/*
 * Move the token to the previous line if not already correctly placed.
 */
static void
token_move_prev_line(struct token *tk)
{
	struct token *pv;
	unsigned int lno;

	pv = token_prev(tk);
	lno = tk->tk_lno - pv->tk_lno;
	if (token_trim(pv) > 0 && lno == 1) {
		token_move_suffixes(pv, tk);
		token_add_optline(tk);
	}
}

static const char *
expr_type_str(enum expr_type type)
{
	switch (type) {
#define OP(type) case type: return #type;
	FOR_EXPR_TYPES(OP)
#undef OP
	}
	return NULL;
}
