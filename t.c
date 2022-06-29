#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define test_expr_exec(a, b)						\
	__test_expr_exec((a), (b), "test_expr_exec", __LINE__);		\
	if (xflag && error) goto out
static int	__test_expr_exec(const char *, const char *, const char *, int);

#define test_lexer_peek_if_type(a, b)					\
	__test_lexer_peek_if_type((a), (b), 0,				\
		"test_lexer_peek_if_type", __LINE__);			\
	if (xflag && error) goto out
#define test_lexer_peek_if_type_flags(a, b, c)				\
	__test_lexer_peek_if_type((b), (c), (a),			\
		"test_lexer_peek_if_type", __LINE__);			\
	if (xflag && error) goto out
static int	__test_lexer_peek_if_type(const char *, const char *,
    unsigned int, const char *, int);

#define test_lexer_read(a, b)						\
	__test_lexer_read((a), (b), "test_lexer_read",			\
		__LINE__);						\
	if (xflag && error) goto out
static int	__test_lexer_read(const char *, const char *, const char *,
    int);

struct parser_stub {
	char		 ps_path[PATH_MAX];
	struct error	 ps_er;
	struct file	*ps_fe;
	struct buffer	*ps_bf;
	struct lexer	*ps_lx;
	struct parser	*ps_pr;
};

static void	parser_stub_create(struct parser_stub *, const char *);
static void	parser_stub_destroy(struct parser_stub *);

static __dead void	usage(void);

static struct config	cf;

int
main(int argc, char *argv[])
{
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

	config_init(&cf);
	cf.cf_flags |= CONFIG_FLAG_TEST;

	lexer_init();
	diff_init();

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
	error |= test_lexer_peek_if_type("static inline foo(", "static inline");
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

	error |= test_lexer_peek_if_type_flags(LEXER_TYPE_FLAG_CAST,
	    "const foo_t)", "const foo_t");
	error |= test_lexer_peek_if_type_flags(LEXER_TYPE_FLAG_ARG,
	    "const foo_t)", "const");
	error |= test_lexer_peek_if_type_flags(LEXER_TYPE_FLAG_ARG,
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

out:
	diff_shutdown();
	lexer_shutdown();
	return error;
}

static int
__test_expr_exec(const char *src, const char *exp, const char *fun, int lno)
{
	struct expr_exec_arg ea = {
		.ea_cf		= &cf,
		.ea_lx		= NULL,
		.ea_dc		= NULL,
		.ea_stop	= NULL,
		.ea_recover	= parser_exec_expr_recover,
		.ea_arg		= NULL,
		.ea_flags	= 0,
	};
	struct parser_stub ps;
	struct buffer *bf = NULL;
	struct doc *group;
	const char *act;
	int error = 0;

	parser_stub_create(&ps, src);
	group = doc_alloc(DOC_GROUP, NULL);
	ea.ea_lx = ps.ps_lx;
	ea.ea_dc = doc_alloc(DOC_CONCAT, group);
	ea.ea_arg = ps.ps_pr;
	if (expr_exec(&ea) == NULL) {
		warnx("%s:%d: expr_exec() failure", fun, lno);
		error = 1;
		goto out;
	}

	bf = buffer_alloc(128);
	doc_exec(group, ps.ps_lx, bf, &cf, 0);
	buffer_appendc(bf, '\0');
	act = bf->bf_ptr;
	if (strcmp(exp, act)) {
		warnx("%s:%d:\n\texp\t\"%s\"\n\tgot\t\"%s\"", fun, lno, exp,
		    act);
		error = 1;
	}

out:
	doc_free(group);
	buffer_free(bf);
	parser_stub_destroy(&ps);
	return error;
}

static int
__test_lexer_peek_if_type(const char *src, const char *exp, unsigned int flags,
    const char *fun, int lno)
{
	struct parser_stub ps;
	struct buffer *bf = NULL;
	struct token *end, *tk;
	const char *act;
	int error = 0;
	int ntokens = 0;

	parser_stub_create(&ps, src);

	if (!lexer_peek_if_type(ps.ps_lx, &end, flags)) {
		warnx("%s:%d: lexer_peek_if_type() failure", fun, lno);
		error = 1;
		goto out;
	}

	bf = buffer_alloc(128);
	for (;;) {
		if (!lexer_pop(ps.ps_lx, &tk))
			errx(1, "%s:%d: out of tokens", fun, lno);

		if (ntokens++ > 0)
			buffer_appendc(bf, ' ');
		buffer_append(bf, tk->tk_str, tk->tk_len);

		if (tk == end)
			break;
	}
	buffer_appendc(bf, '\0');
	act = bf->bf_ptr;
	if (strcmp(exp, act)) {
		warnx("%s:%d:\n\texp\t\"%s\"\n\tgot\t\"%s\"", fun, lno, exp,
		    act);
		error = 1;
	}

out:
	buffer_free(bf);
	parser_stub_destroy(&ps);
	return error;
}

static int
__test_lexer_read(const char *src, const char *exp, const char *fun, int lno)
{
	struct buffer *bf = NULL;
	struct parser_stub ps;
	const char *act;
	int error = 0;
	int ntokens = 0;

	parser_stub_create(&ps, src);

	bf = buffer_alloc(128);
	for (;;) {
		struct token *tk;
		const char *end;
		char *str;

		if (!lexer_pop(ps.ps_lx, &tk))
			errx(1, "%s:%d: out of tokens", fun, lno);
		if (tk->tk_type == TOKEN_EOF)
			break;

		if (ntokens++ > 0)
			buffer_appendc(bf, ' ');
		str = token_sprintf(tk);
		/* Strip of the token position and verbatim representation. */
		end = strchr(str, '<');
		buffer_append(bf, str, end - str);
		free(str);
	}
	buffer_appendc(bf, '\0');
	act = bf->bf_ptr;
	if (strcmp(exp, act)) {
		warnx("%s:%d:\n\texp\t\"%s\"\n\tgot\t\"%s\"", fun, lno, exp,
		    act);
		error = 1;
	}

	buffer_free(bf);
	parser_stub_destroy(&ps);
	return error;
}

static void
parser_stub_create(struct parser_stub *ps, const char *src)
{
	error_init(&ps->ps_er, &cf);
	ps->ps_bf = buffer_alloc(128);
	buffer_append(ps->ps_bf, src, strlen(src));
	ps->ps_fe = file_alloc("test.c");
	ps->ps_lx = lexer_alloc(ps->ps_fe, ps->ps_bf, &ps->ps_er, &cf);
	ps->ps_pr = parser_alloc(ps->ps_path, ps->ps_lx, &ps->ps_er, &cf);
}

static void
parser_stub_destroy(struct parser_stub *ps)
{
	error_flush(&ps->ps_er);
	error_close(&ps->ps_er);

	parser_free(ps->ps_pr);
	ps->ps_pr = NULL;

	lexer_free(ps->ps_lx);
	ps->ps_lx = NULL;

	file_free(ps->ps_fe);

	buffer_free(ps->ps_bf);
	ps->ps_bf = NULL;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: t [-x]\n");
	exit(1);
}
