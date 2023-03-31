#include "config.h"

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "alloc.h"
#include "buffer.h"
#include "clang.h"
#include "doc.h"
#include "error.h"
#include "lexer.h"
#include "options.h"
#include "parser-expr.h"
#include "parser-priv.h"
#include "parser-type.h"
#include "parser.h"
#include "style.h"
#include "token.h"
#include "util.h"

struct context;

#define test(e) do {							\
	error |= (e);							\
	if (xflag && error) goto out;					\
	context_reset(cx);						\
} while (0)

#define test_parser_expr(a, b) \
	test(test_parser_expr0(cx, (a), (b), __LINE__))
static int	test_parser_expr0(struct context *, const char *, const char *,
    int);

#define test_parser_type_peek(a, b) \
	test(test_parser_type_peek0(cx, (a), (b), 0, __LINE__))
#define test_parser_type_peek_flags(a, b, c) \
	test(test_parser_type_peek0(cx, (b), (c), (a), __LINE__))
static int	test_parser_type_peek0(struct context *, const char *,
    const char *, unsigned int, int);

#define test_lexer_read(a, b) \
	test(test_lexer_read0(cx, (a), (b), __LINE__))
static int	test_lexer_read0(struct context *, const char *, const char *,
    int);

#define test_strwidth(a, b, c) \
	test(test_strwidth0((a), (b), (c), __LINE__))
static int	test_strwidth0(const char *, size_t, size_t, int);

struct context {
	struct options	 op;
	struct buffer	*bf;
	struct error	*er;
	struct style	*st;
	struct clang	*cl;
	struct lexer	*lx;
	struct parser	*pr;
};

static void	usage(void) __attribute__((__noreturn__));

static struct context	*context_alloc(void);
static void		 context_free(struct context *);
static void		 context_init(struct context *, const char *);
static void		 context_reset(struct context *);

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

	clang_init();
	cx = context_alloc();

	test_parser_expr("1", "(1)");
	test_parser_expr("x", "(x)");
	test_parser_expr("\"x\"", "(\"x\")");
	test_parser_expr("\"x\" \"y\"", "((\"x\") (\"y\"))");
	test_parser_expr("x(1, 2)", "((x)(((1), (2))))");
	test_parser_expr("x = 1", "((x) = (1))");
	test_parser_expr("x += 1", "((x) += (1))");
	test_parser_expr("x -= 1", "((x) -= (1))");
	test_parser_expr("x *= 1", "((x) *= (1))");
	test_parser_expr("x /= 1", "((x) /= (1))");
	test_parser_expr("x %= 1", "((x) %= (1))");
	test_parser_expr("x <<= 1", "((x) <<= (1))");
	test_parser_expr("x >>= 1", "((x) >>= (1))");
	test_parser_expr("x &= 1", "((x) &= (1))");
	test_parser_expr("x ^= 1", "((x) ^= (1))");
	test_parser_expr("x |= 1", "((x) |= (1))");
	test_parser_expr("x ? 1 : 0", "((x) ? (1) : (0))");
	test_parser_expr("x ?: 0", "((x) ?: (0))");
	test_parser_expr("x || y", "((x) || (y))");
	test_parser_expr("x && y", "((x) && (y))");
	test_parser_expr("x | y", "((x) | (y))");
	test_parser_expr("x|y", "((x)|(y))");
	test_parser_expr("x ^ y", "((x) ^ (y))");
	test_parser_expr("x & y", "((x) & (y))");
	test_parser_expr("x == y", "((x) == (y))");
	test_parser_expr("x != y", "((x) != (y))");
	test_parser_expr("x < y", "((x) < (y))");
	test_parser_expr("x <= y", "((x) <= (y))");
	test_parser_expr("x > y", "((x) > (y))");
	test_parser_expr("x >= y", "((x) >= (y))");
	test_parser_expr("x << y", "((x) << (y))");
	test_parser_expr("x >> y", "((x) >> (y))");
	test_parser_expr("x + y", "((x) + (y))");
	test_parser_expr("x - y", "((x) - (y))");
	test_parser_expr("x * y", "((x) * (y))");
	test_parser_expr("x*y", "((x)*(y))");
	test_parser_expr("x / y", "((x) / (y))");
	test_parser_expr("x/y", "((x)/(y))");
	test_parser_expr("x % y", "((x) % (y))");
	test_parser_expr("!x", "(!(x))");
	test_parser_expr("~x", "(~(x))");
	test_parser_expr("++x", "(++(x))");
	test_parser_expr("x++", "((x)++)");
	test_parser_expr("--x", "(--(x))");
	test_parser_expr("x--", "((x)--)");
	test_parser_expr("-x", "(-(x))");
	test_parser_expr("+x", "(+(x))");
	test_parser_expr("(int)x", "(((int))(x))");
	test_parser_expr("(struct s)x", "(((struct s))(x))");
	test_parser_expr("(struct s*)x", "(((struct s *))(x))");
	test_parser_expr("(char const* char*)x", "(((char const *char *))(x))");
	test_parser_expr("(foo_t)x", "(((foo_t))(x))");
	test_parser_expr("(foo_t *)x", "(((foo_t *))(x))");
	test_parser_expr("(pc) & ~mask", "(((pc)) & (~(mask)))");
	test_parser_expr("(a), (b)", "(((a)), ((b)))");
	test_parser_expr("x * y", "((x) * (y))");
	test_parser_expr("x & y", "((x) & (y))");
	test_parser_expr("sizeof x", "(sizeof (x))");
	test_parser_expr("sizeof x * y", "((sizeof (x)) * (y))");
	test_parser_expr("sizeof char", "(sizeof (char))");
	test_parser_expr("sizeof char *c", "((sizeof (char)) * (c))");
	test_parser_expr("sizeof struct s", "(sizeof (struct s))");
	test_parser_expr("sizeof(x)", "(sizeof((x)))");
	test_parser_expr("sizeof(*x)", "(sizeof((*(x))))");
	test_parser_expr("(x)", "((x))");
	test_parser_expr("x()", "((x)())");
	test_parser_expr("x(\"x\" X)", "((x)(((\"x\") (X))))");
	test_parser_expr("x[1 + 2]", "((x)[((1) + (2))])");
	test_parser_expr("x->y", "((x)->(y))");
	test_parser_expr("x.y", "((x).(y))");

	test_parser_type_peek("void", "void");
	test_parser_type_peek("void *", "void *");
	test_parser_type_peek("void *p", "void *");
	test_parser_type_peek("size_t", "size_t");
	test_parser_type_peek("size_t s)", "size_t");
	test_parser_type_peek("size_t *", "size_t *");
	test_parser_type_peek("size_t *p", "size_t *");
	test_parser_type_peek("void main(int)", "void");
	test_parser_type_peek("void main(int);", "void");
	test_parser_type_peek("static foo void", "static foo void");
	test_parser_type_peek("static void foo", "static void foo");
	test_parser_type_peek("void foo f(void)", "void foo");
	test_parser_type_peek("char[]", "char [ ]");
	test_parser_type_peek("struct wsmouse_param[]",
	    "struct wsmouse_param [ ]");
	test_parser_type_peek("void)", "void");
	test_parser_type_peek("void,", "void");
	test_parser_type_peek("struct {,", "struct");
	test_parser_type_peek("struct s,", "struct s");
	test_parser_type_peek("struct s {,", "struct s");
	test_parser_type_peek("struct s *,", "struct s *");
	test_parser_type_peek("struct s **,", "struct s * *");
	test_parser_type_peek("struct s ***,", "struct s * * *");
	test_parser_type_peek("union {,", "union");
	test_parser_type_peek("union u,", "union u");
	test_parser_type_peek("union u {,", "union u");
	test_parser_type_peek("const* char*", "const * char *");
	test_parser_type_peek("char x[]", "char");
	test_parser_type_peek("va_list ap;", "va_list");
	test_parser_type_peek("struct s *M(v)", "struct s *");
	test_parser_type_peek(
	    "const struct filterops *const sysfilt_ops[]",
	    "const struct filterops * const");
	test_parser_type_peek("long __guard_local __attribute__", "long");
	test_parser_type_peek("unsigned int f:1", "unsigned int");
	test_parser_type_peek("usbd_status (*v)(void)",
	    "usbd_status ( * v ) ( void )");
	test_parser_type_peek("register char", "register char");
	test_parser_type_peek("...", "...");
	test_parser_type_peek("int (*f[])(void)", "int ( * f [ ] ) ( void )");
	test_parser_type_peek("int (* volatile f)(void);",
	    "int ( * volatile f ) ( void )");
	test_parser_type_peek("int (**f)(void);", "int ( * * f ) ( void )");
	test_parser_type_peek("int (*f(void))(void)", "int");
	test_parser_type_peek("void (*f[1])(void)",
	    "void ( * f [ 1 ] ) ( void )");
	test_parser_type_peek("void (*)", "void ( * )");
	test_parser_type_peek("char (*v)[1]", "char");
	test_parser_type_peek("P256_POINT (*table)[16]", "P256_POINT");
	test_parser_type_peek("STACK_OF(X509_EXTENSION) x",
	    "STACK_OF ( X509_EXTENSION ) x");
	test_parser_type_peek("STACK_OF(X509_EXTENSION) *",
	    "STACK_OF ( X509_EXTENSION ) *");
	test_parser_type_peek("const STACK_OF(X509_EXTENSION)*",
	    "const STACK_OF ( X509_EXTENSION ) *");

	test_parser_type_peek_flags(PARSER_TYPE_CAST,
	    "const foo_t)", "const foo_t");
	test_parser_type_peek_flags(PARSER_TYPE_ARG,
	    "const foo_t)", "const");
	test_parser_type_peek_flags(PARSER_TYPE_ARG,
	    "size_t)", "size_t");
	test_parser_type_peek_flags(PARSER_TYPE_EXPR,
	    "const foo_t)", "const foo_t");

	test_lexer_read("<", "LESS");
	test_lexer_read("<x", "LESS IDENT");
	test_lexer_read("<=", "LESSEQUAL");
	test_lexer_read("<<", "LESSLESS");
	test_lexer_read("<<=", "LESSLESSEQUAL");

	test_lexer_read(".", "PERIOD");
	test_lexer_read("..", "PERIOD PERIOD");
	test_lexer_read("...", "ELLIPSIS");
	test_lexer_read(".x", "PERIOD IDENT");

	test_lexer_read("restrict", "RESTRICT");
	test_lexer_read("__restrict", "RESTRICT");

	test_lexer_read("__volatile", "VOLATILE");
	test_lexer_read("__volatile__", "VOLATILE");

	test_strwidth("int", 0, 3);
	test_strwidth("int\tx", 0, 9);
	test_strwidth("int\tx", 3, 9);
	test_strwidth("int\nx", 0, 1);
	test_strwidth("int\n", 0, 0);

out:
	context_free(cx);
	clang_shutdown();
	return error;
}

static void
usage(void)
{
	fprintf(stderr, "usage: t [-x]\n");
	exit(1);
}

static int
test_parser_expr0(struct context *cx, const char *src, const char *exp, int lno)
{
	struct buffer *bf = NULL;
	struct doc *concat, *expr;
	const char *act;
	int error;

	context_init(cx, src);

	concat = doc_alloc(DOC_CONCAT, NULL);
	error = parser_expr(cx->pr, &expr, &(struct parser_expr_arg){
	    .dc	= concat,
	});
	if (error & HALT) {
		fprintf(stderr, "parser_expr:%d: want %x, got %x\n", lno,
		    (unsigned int)GOOD, (unsigned int)error);
		error = 1;
		goto out;
	}
	error = 0;

	bf = buffer_alloc(128);
	if (bf == NULL)
		err(1, NULL);
	doc_exec(&(struct doc_exec_arg){
	    .dc	= concat,
	    .bf	= bf,
	    .st	= cx->st,
	    .op	= &cx->op,
	});
	buffer_putc(bf, '\0');
	act = buffer_get_ptr(bf);
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "parser_expr:%d:\n\texp \"%s\"\n\tgot \"%s\"\n",
		    lno, exp, act);
		error = 1;
	}

out:
	doc_free(concat);
	buffer_free(bf);
	return error;
}

static int
test_parser_type_peek0(struct context *cx, const char *src, const char *exp,
    unsigned int flags, int lno)
{
	struct buffer *bf = NULL;
	struct token *end, *tk;
	const char *act;
	int error = 0;
	int ntokens = 0;

	context_init(cx, src);

	if (!parser_type_peek(cx->pr, &end, flags)) {
		fprintf(stderr, "parser_type_peek:%d: want 1, got 0\n", lno);
		error = 1;
		goto out;
	}

	bf = buffer_alloc(128);
	if (bf == NULL)
		err(1, NULL);
	for (;;) {
		if (!lexer_pop(cx->lx, &tk))
			errx(1, "parser_type_peek:%d: out of tokens", lno);

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		buffer_puts(bf, tk->tk_str, tk->tk_len);

		if (tk == end)
			break;
	}
	buffer_putc(bf, '\0');
	act = buffer_get_ptr(bf);
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "parser_type_peek:%d:\n"
		    "\texp \"%s\"\n\tgot \"%s\"\n",
		    lno, exp, act);
		error = 1;
	}

out:
	buffer_free(bf);
	return error;
}

static int
test_lexer_read0(struct context *cx, const char *src, const char *exp,
    int lno)
{
	struct buffer *bf = NULL;
	const char *act;
	int error = 0;
	int ntokens = 0;

	context_init(cx, src);

	bf = buffer_alloc(128);
	if (bf == NULL)
		err(1, NULL);
	for (;;) {
		struct token *tk;
		const char *end, *str;
		size_t tokenlen;

		if (!lexer_pop(cx->lx, &tk))
			errx(1, "lexer_read:%d: out of tokens", lno);
		if (tk->tk_type == LEXER_EOF)
			break;

		if (ntokens++ > 0)
			buffer_putc(bf, ' ');
		str = lexer_serialize(cx->lx, tk);
		/* Strip of the token position and verbatim representation. */
		end = strchr(str, '<');
		tokenlen = (size_t)(end - str);
		buffer_puts(bf, str, tokenlen);
	}
	buffer_putc(bf, '\0');
	act = buffer_get_ptr(bf);
	if (strcmp(exp, act) != 0) {
		fprintf(stderr, "lexer_read:%d:\n\texp \"%s\"\n\tgot \"%s\"\n",
		    lno, exp, act);
		error = 1;
	}

	buffer_free(bf);
	return error;
}

static int
test_strwidth0(const char *str, size_t pos, size_t exp, int lno)
{
	size_t act;

	act = strwidth(str, strlen(str), pos);
	if (exp != act) {
		fprintf(stderr, "strwidth:%d:\n\texp %zu\n\tgot %zu\n",
		    lno, exp, act);
		return 1;
	}
	return 0;
}

static struct context *
context_alloc(void)
{
	struct context *cx;

	cx = ecalloc(1, sizeof(*cx));
	options_init(&cx->op);
	cx->op.op_flags.test = 1;
	cx->bf = buffer_alloc(128);
	if (cx->bf == NULL)
		err(1, NULL);
	return cx;
}

static void
context_free(struct context *cx)
{
	if (cx == NULL)
		return;

	context_reset(cx);
	buffer_free(cx->bf);
	free(cx);
}

static void
context_init(struct context *cx, const char *src)
{
	static const char *path = "test.c";

	buffer_puts(cx->bf, src, strlen(src));
	cx->er = error_alloc(0);
	cx->st = style_parse(NULL, &cx->op);
	cx->cl = clang_alloc(&cx->op, cx->st);
	cx->lx = lexer_alloc(&(const struct lexer_arg){
	    .path	= path,
	    .bf		= cx->bf,
	    .er		= cx->er,
	    .op		= &cx->op,
	    .callbacks	= {
		.read		= clang_read,
		.serialize	= token_serialize,
		.arg		= cx->cl,
	    },
	});
	cx->pr = parser_alloc(path, cx->lx, cx->er, cx->st,
	    &cx->op);
}

static void
context_reset(struct context *cx)
{
	buffer_reset(cx->bf);

	if (cx->er != NULL) {
		error_flush(cx->er, 1);
		error_free(cx->er);
		cx->er = NULL;
	}

	style_free(cx->st);
	cx->st = NULL;

	parser_free(cx->pr);
	cx->pr = NULL;

	lexer_free(cx->lx);
	cx->lx = NULL;

	clang_free(cx->cl);
	cx->cl = NULL;

	parser_free(cx->pr);
	cx->pr = NULL;
}
