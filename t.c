#include "config.h"

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"
#include "diff.h"
#include "doc.h"
#include "error.h"
#include "expr.h"
#include "lexer.h"
#include "options.h"
#include "parser.h"
#include "style.h"
#include "token.h"
#include "util.h"

struct context;

#define test_expr_exec(a, b)						\
	test_expr_exec0(cx, (a), (b), "test_expr_exec", __LINE__);	\
	if (xflag && error) goto out;					\
	context_reset(cx)
static int	test_expr_exec0(struct context *, const char *, const char *,
    const char *, int);

#define test_lexer_peek_if_type(a, b)					\
	test_lexer_peek_if_type0(cx, (a), (b), 0,			\
		"test_lexer_peek_if_type", __LINE__);			\
	if (xflag && error) goto out;					\
	context_reset(cx)
#define test_lexer_peek_if_type_flags(a, b, c)				\
	test_lexer_peek_if_type0(cx, (b), (c), (a),			\
		"test_lexer_peek_if_type", __LINE__);			\
	if (xflag && error) goto out;					\
	context_reset(cx)
static int	test_lexer_peek_if_type0(struct context *, const char *,
    const char *, unsigned int, const char *, int);

#define test_lexer_read(a, b)						\
	test_lexer_read0(cx, (a), (b), "test_lexer_read",		\
		__LINE__);						\
	if (xflag && error) goto out;					\
	context_reset(cx)
static int	test_lexer_read0(struct context *, const char *, const char *,
    const char *, int);

#define test_strwidth(a, b, c)						\
	test_strwidth0((a), (b), (c), "test_strwidth", __LINE__);	\
	if (xflag && error) goto out;					\
	context_reset(cx)
static int	test_strwidth0(const char *, size_t, size_t,
    const char *, int);

struct context {
	struct options	 cx_op;
	struct buffer	*cx_bf;
	struct error	*cx_er;
	struct style	*cx_st;
	struct lexer	*cx_lx;
	struct parser	*cx_pr;
};

static struct context	*context_alloc(void);
static void		 context_free(struct context *);
static void		 context_init(struct context *, const char *);
static void		 context_reset(struct context *);

static __dead void	usage(void);

int
main(int argc, char *argv[])
{
	struct context *cx;
	int error = 0;
	int xflag = 0;
	int ch;

	while ((ch = getopt(argc, argv, "x")) != -1) {
		switch (ch) {
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	}

	lexer_init();
	diff_init();
	cx = context_alloc();

	error |= test_expr_exec("1", "(1)");
	error |= test_expr_exec("x", "(x)");
	error |= test_expr_exec("\"x\"", "(\"x\")");
	error |= test_expr_exec("\"x\" \"y\"", "((\"x\") (\"y\"))");
	error |= test_expr_exec("x(1, 2)", "((x)(((1), (2))))");
	error |= test_expr_exec("x = 1", "((x) = (1))");
	error |= test_expr_exec("x += 1", "((x) += (1))");
	error |= test_expr_exec("x -= 1", "((x) -= (1))");
	error |= test_expr_exec("x *= 1", "((x) *= (1))");
	error |= test_expr_exec("x /= 1", "((x) /= (1))");
	error |= test_expr_exec("x %= 1", "((x) %= (1))");
	error |= test_expr_exec("x <<= 1", "((x) <<= (1))");
	error |= test_expr_exec("x >>= 1", "((x) >>= (1))");
	error |= test_expr_exec("x &= 1", "((x) &= (1))");
	error |= test_expr_exec("x ^= 1", "((x) ^= (1))");
	error |= test_expr_exec("x |= 1", "((x) |= (1))");
	error |= test_expr_exec("x ? 1 : 0", "((x) ? (1) : (0))");
	error |= test_expr_exec("x ?: 0", "((x) ?: (0))");
	error |= test_expr_exec("x || y", "((x) || (y))");
	error |= test_expr_exec("x && y", "((x) && (y))");
	error |= test_expr_exec("x | y", "((x) | (y))");
	error |= test_expr_exec("x|y", "((x)|(y))");
	error |= test_expr_exec("x ^ y", "((x) ^ (y))");
	error |= test_expr_exec("x & y", "((x) & (y))");
	error |= test_expr_exec("x == y", "((x) == (y))");
	error |= test_expr_exec("x != y", "((x) != (y))");
	error |= test_expr_exec("x < y", "((x) < (y))");
	error |= test_expr_exec("x <= y", "((x) <= (y))");
	error |= test_expr_exec("x > y", "((x) > (y))");
	error |= test_expr_exec("x >= y", "((x) >= (y))");
	error |= test_expr_exec("x << y", "((x) << (y))");
	error |= test_expr_exec("x >> y", "((x) >> (y))");
	error |= test_expr_exec("x + y", "((x) + (y))");
	error |= test_expr_exec("x - y", "((x) - (y))");
	error |= test_expr_exec("x * y", "((x) * (y))");
	error |= test_expr_exec("x*y", "((x)*(y))");
	error |= test_expr_exec("x / y", "((x) / (y))");
	error |= test_expr_exec("x/y", "((x)/(y))");
	error |= test_expr_exec("x % y", "((x) % (y))");
	error |= test_expr_exec("!x", "(!(x))");
	error |= test_expr_exec("~x", "(~(x))");
	error |= test_expr_exec("++x", "(++(x))");
	error |= test_expr_exec("x++", "((x)++)");
	error |= test_expr_exec("--x", "(--(x))");
	error |= test_expr_exec("x--", "((x)--)");
	error |= test_expr_exec("-x", "(-(x))");
	error |= test_expr_exec("+x", "(+(x))");
	error |= test_expr_exec("(int)x", "(((int))(x))");
	error |= test_expr_exec("(struct s)x", "(((struct s))(x))");
	error |= test_expr_exec("(struct s*)x", "(((struct s *))(x))");
	error |= test_expr_exec("(char const* char*)x",
	    "(((char const *char *))(x))");
	error |= test_expr_exec("(foo_t)x", "(((foo_t))(x))");
	error |= test_expr_exec("(foo_t *)x", "(((foo_t *))(x))");
	error |= test_expr_exec("x * y", "((x) * (y))");
	error |= test_expr_exec("x & y", "((x) & (y))");
	error |= test_expr_exec("sizeof x", "(sizeof (x))");
	error |= test_expr_exec("sizeof x * y", "((sizeof (x)) * (y))");
	error |= test_expr_exec("sizeof char", "(sizeof (char))");
	error |= test_expr_exec("sizeof char *", "(sizeof (char *))");
	error |= test_expr_exec("sizeof struct s", "(sizeof (struct s))");
	error |= test_expr_exec("sizeof(x)", "(sizeof((x)))");
	error |= test_expr_exec("sizeof(*x)", "(sizeof((*(x))))");
	error |= test_expr_exec("(x)", "((x))");
	error |= test_expr_exec("x()", "((x)())");
	error |= test_expr_exec("x(\"x\" X)", "((x)(((\"x\") (X))))");
	error |= test_expr_exec("x[1 + 2]", "((x)[((1) + (2))])");
	error |= test_expr_exec("x->y", "((x)->(y))");
	error |= test_expr_exec("x.y", "((x).(y))");

	error |= test_lexer_peek_if_type("void", "void");
	error |= test_lexer_peek_if_type("void *", "void *");
	error |= test_lexer_peek_if_type("void *p", "void *");
	error |= test_lexer_peek_if_type("size_t", "size_t");
	error |= test_lexer_peek_if_type("size_t s)", "size_t");
	error |= test_lexer_peek_if_type("size_t *", "size_t *");
	error |= test_lexer_peek_if_type("size_t *p", "size_t *");
	error |= test_lexer_peek_if_type("void main(int)", "void");
	error |= test_lexer_peek_if_type("void main(int);", "void");
	error |= test_lexer_peek_if_type("static foo void", "static foo void");
	error |= test_lexer_peek_if_type("static void foo", "static void foo");
	error |= test_lexer_peek_if_type("void foo f(void)", "void foo");
	error |= test_lexer_peek_if_type("char[]", "char [ ]");
	error |= test_lexer_peek_if_type("struct wsmouse_param[]",
	    "struct wsmouse_param [ ]");
	error |= test_lexer_peek_if_type("void)", "void");
	error |= test_lexer_peek_if_type("void,", "void");
	error |= test_lexer_peek_if_type("struct {,", "struct");
	error |= test_lexer_peek_if_type("struct s,", "struct s");
	error |= test_lexer_peek_if_type("struct s {,", "struct s");
	error |= test_lexer_peek_if_type("struct s *,", "struct s *");
	error |= test_lexer_peek_if_type("struct s **,", "struct s * *");
	error |= test_lexer_peek_if_type("struct s ***,", "struct s * * *");
	error |= test_lexer_peek_if_type("union {,", "union");
	error |= test_lexer_peek_if_type("union u,", "union u");
	error |= test_lexer_peek_if_type("union u {,", "union u");
	error |= test_lexer_peek_if_type("const* char*", "const * char *");
	error |= test_lexer_peek_if_type("char x[]", "char");
	error |= test_lexer_peek_if_type("va_list ap;", "va_list");
	error |= test_lexer_peek_if_type("struct s *M(v)", "struct s *");
	error |= test_lexer_peek_if_type(
	    "const struct filterops *const sysfilt_ops[]",
	    "const struct filterops * const");
	error |= test_lexer_peek_if_type("long __guard_local __attribute__",
	    "long");
	error |= test_lexer_peek_if_type("unsigned int f:1", "unsigned int");
	error |= test_lexer_peek_if_type("usbd_status (*v)(void)",
	    "usbd_status ( * v ) ( void )");
	error |= test_lexer_peek_if_type("register char", "register char");
	error |= test_lexer_peek_if_type("...", "...");
	error |= test_lexer_peek_if_type("int (*f[])(void)",
	    "int ( * f [ ] ) ( void )");
	error |= test_lexer_peek_if_type("int (* volatile f)(void);",
	    "int ( * volatile f ) ( void )");
	error |= test_lexer_peek_if_type("int (**f)(void);",
	    "int ( * * f ) ( void )");
	error |= test_lexer_peek_if_type("int (*f(void))(void)", "int");
	error |= test_lexer_peek_if_type("void (*f[1])(void)",
	    "void ( * f [ 1 ] ) ( void )");
	error |= test_lexer_peek_if_type("void (*)",
	    "void ( * )");
	error |= test_lexer_peek_if_type("char (*v)[1]", "char");
	error |= test_lexer_peek_if_type("P256_POINT (*table)[16]",
	    "P256_POINT");
	error |= test_lexer_peek_if_type("STACK_OF(X509_EXTENSION) x",
	    "STACK_OF ( X509_EXTENSION ) x");
	error |= test_lexer_peek_if_type("STACK_OF(X509_EXTENSION) *",
	    "STACK_OF ( X509_EXTENSION ) *");
	error |= test_lexer_peek_if_type("const STACK_OF(X509_EXTENSION)*",
	    "const STACK_OF ( X509_EXTENSION ) *");

	error |= test_lexer_peek_if_type_flags(LEXER_TYPE_CAST,
	    "const foo_t)", "const foo_t");
	error |= test_lexer_peek_if_type_flags(LEXER_TYPE_ARG,
	    "const foo_t)", "const");
	error |= test_lexer_peek_if_type_flags(LEXER_TYPE_ARG,
	    "size_t)", "size_t");

	error |= test_lexer_read("<", "LESS");
	error |= test_lexer_read("<x", "LESS IDENT");
	error |= test_lexer_read("<=", "LESSEQUAL");
	error |= test_lexer_read("<<", "LESSLESS");
	error |= test_lexer_read("<<=", "LESSLESSEQUAL");

	error |= test_lexer_read(".", "PERIOD");
	error |= test_lexer_read("..", "PERIOD PERIOD");
	error |= test_lexer_read("...", "ELLIPSIS");
	error |= test_lexer_read(".x", "PERIOD IDENT");

	error |= test_strwidth("int", 0, 3);
	error |= test_strwidth("int\tx", 0, 9);
	error |= test_strwidth("int\tx", 3, 9);
	error |= test_strwidth("int\nx", 0, 1);
	error |= test_strwidth("int\n", 0, 0);

out:
	context_free(cx);
	diff_shutdown();
	lexer_shutdown();
	return error;
}

static int
test_expr_exec0(struct context *cx, const char *src, const char *exp,
    const char *fun, int lno)
{
	struct buffer *bf = NULL;
	struct doc *expr, *group;
	const char *act;
	int error = 0;

	context_init(cx, src);

	group = doc_alloc(DOC_GROUP, NULL);
	expr = expr_exec(&(const struct expr_exec_arg){
	    .st		= cx->cx_st,
	    .op		= &cx->cx_op,
	    .lx		= cx->cx_lx,
	    .dc		= doc_alloc(DOC_CONCAT, group),
	    .stop	= NULL,
	    .recover	= parser_exec_expr_recover,
	    .arg	= cx->cx_pr,
	    .flags	= 0,
	});
	if (expr == NULL) {
		fprintf(stderr, "%s:%d: expr_exec() failure\n", fun, lno);
		error = 1;
		goto out;
	}

	bf = buffer_alloc(128);
	doc_exec(group, cx->cx_lx, bf, cx->cx_st, &cx->cx_op, 0);
	buffer_putc(bf, '\0');
	act = bf->bf_ptr;
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "%s:%d:\n\texp \"%s\"\n\tgot \"%s\"\n",
		    fun, lno, exp, act);
		error = 1;
	}

out:
	doc_free(group);
	buffer_free(bf);
	return error;
}

static int
test_lexer_peek_if_type0(struct context *cx, const char *src, const char *exp,
    unsigned int flags,
    const char *fun, int lno)
{
	struct buffer *bf = NULL;
	struct token *end, *tk;
	const char *act;
	int error = 0;
	int ntokens = 0;

	context_init(cx, src);

	if (!lexer_peek_if_type(cx->cx_lx, &end, flags)) {
		fprintf(stderr, "%s:%d: lexer_peek_if_type() failure\n",
		    fun, lno);
		error = 1;
		goto out;
	}

	bf = buffer_alloc(128);
	for (;;) {
		if (!lexer_pop(cx->cx_lx, &tk))
			errx(1, "%s:%d: out of tokens", fun, lno);

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		buffer_puts(bf, tk->tk_str, tk->tk_len);

		if (tk == end)
			break;
	}
	buffer_putc(bf, '\0');
	act = bf->bf_ptr;
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "%s:%d:\n\texp \"%s\"\n\tgot \"%s\"\n",
		    fun, lno, exp, act);
		error = 1;
	}

out:
	buffer_free(bf);
	return error;
}

static int
test_lexer_read0(struct context *cx, const char *src, const char *exp,
    const char *fun, int lno)
{
	struct buffer *bf = NULL;
	const char *act;
	int error = 0;
	int ntokens = 0;

	context_init(cx, src);

	bf = buffer_alloc(128);
	for (;;) {
		struct token *tk;
		const char *end;
		char *str;

		if (!lexer_pop(cx->cx_lx, &tk))
			errx(1, "%s:%d: out of tokens", fun, lno);
		if (tk->tk_type == LEXER_EOF)
			break;

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		str = token_sprintf(tk);
		/* Strip of the token position and verbatim representation. */
		end = strchr(str, '<');
		buffer_puts(bf, str, end - str);
		free(str);
	}
	buffer_putc(bf, '\0');
	act = bf->bf_ptr;
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "%s:%d:\n\texp \"%s\"\n\tgot \"%s\"\n",
		    fun, lno, exp, act);
		error = 1;
	}

	buffer_free(bf);
	return error;
}

static int
test_strwidth0(const char *str, size_t pos, size_t exp,
    const char *fun, int lno)
{
	size_t act;

	act = strwidth(str, strlen(str), pos);
	if (exp != act) {
		fprintf(stderr, "%s:%d:\n\texp %zu\n\tgot %zu\n",
		    fun, lno, exp, act);
		return 1;
	}
	return 0;
}

static struct context *
context_alloc(void)
{
	struct context *cx;

	cx = calloc(1, sizeof(*cx));
	if (cx == NULL)
		err(1, NULL);

	options_init(&cx->cx_op);
	cx->cx_op.op_flags |= OPTIONS_TEST;
	cx->cx_bf = buffer_alloc(128);
	return cx;
}

static void
context_free(struct context *cx)
{
	if (cx == NULL)
		return;

	context_reset(cx);
	buffer_free(cx->cx_bf);
	free(cx);
}

static void
context_init(struct context *cx, const char *src)
{
	static const char *path = "test.c";

	buffer_puts(cx->cx_bf, src, strlen(src));
	cx->cx_er = error_alloc(0);
	cx->cx_st = style_parse(NULL, &cx->cx_op);
	cx->cx_lx = lexer_alloc(&(const struct lexer_arg){
	    .path	= path,
	    .bf		= cx->cx_bf,
	    .er		= cx->cx_er,
	    .diff	= NULL,
	    .op		= &cx->cx_op,
	});
	cx->cx_pr = parser_alloc(path, cx->cx_lx, cx->cx_er, cx->cx_st,
	    &cx->cx_op);
}

static void
context_reset(struct context *cx)
{
	buffer_reset(cx->cx_bf);

	if (cx->cx_er != NULL) {
		error_flush(cx->cx_er, 1);
		error_free(cx->cx_er);
		cx->cx_er = NULL;
	}

	style_free(cx->cx_st);
	cx->cx_st = NULL;

	parser_free(cx->cx_pr);
	cx->cx_pr = NULL;

	lexer_free(cx->cx_lx);
	cx->cx_lx = NULL;

	parser_free(cx->cx_pr);
	cx->cx_pr = NULL;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: t [-x]\n");
	exit(1);
}
