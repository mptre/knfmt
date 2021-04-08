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

#define test_lexer_peek_type(a, b)					\
	__test_lexer_peek_type((a), (b), "test_lexer_peek_type",	\
		__LINE__);						\
	if (xflag && error) goto out
static int	__test_lexer_peek_type(const char *, const char *,
    const char *, int);

struct parser_stub {
	char		 ps_path[PATH_MAX];
	struct parser	*ps_pr;
	struct lexer	*ps_lx;
	int		 ps_fd[2];
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
	error |= test_expr_exec("x / y", "((x) / (y))");
	error |= test_expr_exec("x % y", "((x) % (y))");
	error |= test_expr_exec("!x", "(!(x))");
	error |= test_expr_exec("~x", "(~(x))");
	error |= test_expr_exec("++x", "(++(x))");
	error |= test_expr_exec("x++", "((x)++)");
	error |= test_expr_exec("--x", "(--(x))");
	error |= test_expr_exec("x--", "((x)--)");
	error |= test_expr_exec("-x", "(-(x))");
	error |= test_expr_exec("+x", "(+(x))");
	error |= test_expr_exec("(int)x", "((int)(x))");
	error |= test_expr_exec("()", "()");
	error |= test_expr_exec("(struct s)x", "((struct s)(x))");
	error |= test_expr_exec("(struct s*)x", "((struct s *)(x))");
	error |= test_expr_exec("(char const* char*)x",
	    "((char const *char *)(x))");
	error |= test_expr_exec("(foo_t)x", "((foo_t)(x))");
	error |= test_expr_exec("(foo_t *)x", "((foo_t *)(x))");
	error |= test_expr_exec("x * y", "((x) * (y))");
	error |= test_expr_exec("x & y", "((x) & (y))");
	error |= test_expr_exec("sizeof x", "(sizeof (x))");
	error |= test_expr_exec("sizeof char", "(sizeof (char))");
	error |= test_expr_exec("sizeof char *", "(sizeof char *)");
	error |= test_expr_exec("sizeof struct s", "(sizeof ((struct) (s)))");
	error |= test_expr_exec("sizeof(x)", "(sizeof((x)))");
	error |= test_expr_exec("sizeof(*x)", "(sizeof((*(x))))");
	error |= test_expr_exec("(x)", "((x))");
	error |= test_expr_exec("x()", "((x)())");
	error |= test_expr_exec("x(\"x\" X)", "((x)(((\"x\") (X))))");
	error |= test_expr_exec("x[1 + 2]", "((x)[((1) + (2))])");
	error |= test_expr_exec("x->y", "((x)->(y))");
	error |= test_expr_exec("x.y", "((x).(y))");

	error |= test_lexer_peek_type("void", "void");
	error |= test_lexer_peek_type("void *", "void *");
	error |= test_lexer_peek_type("void *p", "void *");
	error |= test_lexer_peek_type("size_t", "size_t");
	error |= test_lexer_peek_type("size_t)", "size_t");
	error |= test_lexer_peek_type("size_t s)", "size_t");
	error |= test_lexer_peek_type("size_t *", "size_t *");
	error |= test_lexer_peek_type("size_t *p", "size_t *");
	error |= test_lexer_peek_type("void main(int)", "void");
	error |= test_lexer_peek_type("void main(int);", "void");
	error |= test_lexer_peek_type("static foo void", "static foo void");
	error |= test_lexer_peek_type("static void foo", "static void foo");
	error |= test_lexer_peek_type("static inline foo(", "static inline");
	error |= test_lexer_peek_type("void foo f(void)", "void foo");
	error |= test_lexer_peek_type("char[]", "char");
	error |= test_lexer_peek_type("void)", "void");
	error |= test_lexer_peek_type("void,", "void");
	error |= test_lexer_peek_type("struct {,", "struct");
	error |= test_lexer_peek_type("struct s,", "struct s");
	error |= test_lexer_peek_type("struct s {,", "struct s");
	error |= test_lexer_peek_type("struct s *,", "struct s *");
	error |= test_lexer_peek_type("struct s **,", "struct s * *");
	error |= test_lexer_peek_type("struct s ***,", "struct s * * *");
	error |= test_lexer_peek_type("union {,", "union");
	error |= test_lexer_peek_type("union u,", "union u");
	error |= test_lexer_peek_type("union u {,", "union u");
	error |= test_lexer_peek_type("const* char*", "const * char *");
	error |= test_lexer_peek_type("char x[]", "char");
	error |= test_lexer_peek_type("va_list ap;", "va_list");
	error |= test_lexer_peek_type("struct s *M(v)", "struct s *");
	error |= test_lexer_peek_type("const struct filterops *const sysfilt_ops[]",
	    "const struct filterops * const");
	error |= test_lexer_peek_type("long __guard_local __attribute__",
	    "long");
	error |= test_lexer_peek_type("unsigned int f:1", "unsigned int");
	error |= test_lexer_peek_type("usbd_status (*v)(void)", "usbd_status");
	error |= test_lexer_peek_type("register char", "register char");

out:
	return error;
}

static int
__test_expr_exec(const char *src, const char *exp, const char *fun, int lno)
{
	struct parser_stub ps;
	struct buffer *bf = NULL;
	struct doc *concat, *group;
	const char *act;
	int error = 0;

	parser_stub_create(&ps, src);
	group = doc_alloc(DOC_GROUP, NULL);
	concat = doc_alloc(DOC_CONCAT, group);
	if (expr_exec(ps.ps_lx, concat, NULL, &cf) == NULL) {
		warnx("%s:%d: expr_exec() failure", fun, lno);
		error = 1;
		goto out;
	}

	bf = buffer_alloc(128);
	doc_exec(group, bf, &cf);
	if (bf == NULL) {
		warnx("%s:%d: doc_exec() failure", fun, lno);
		error = 1;
		goto out;
	}
	act = buffer_ptr(bf);
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
__test_lexer_peek_type(const char *src, const char *exp, const char *fun,
    int lno)
{
	struct buffer *bf = NULL;
	struct parser_stub ps;
	struct token *end, *tk;
	const char *act;
	int error = 0;
	int ntokens = 0;

	parser_stub_create(&ps, src);

	if (!lexer_peek_type(ps.ps_lx, &end, 0)) {
		warnx("%s:%d: lexer_peek_type() failure", fun, lno);
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
	act = buffer_ptr(bf);
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

static void
parser_stub_create(struct parser_stub *ps, const char *src)
{
	ssize_t len, n;

	if (pipe(ps->ps_fd) == -1)
		err(1, "pipe");
	len = strlen(src);
	n = write(ps->ps_fd[1], src, len);
	if (n == -1)
		err(1, "write");
	if (n != len)
		errx(1, "write: %ld < %ld", n, len);
	close(ps->ps_fd[1]);
	ps->ps_fd[1] = -1;

	len = sizeof(ps->ps_path);
	n = snprintf(ps->ps_path, len, "/dev/fd/%d", ps->ps_fd[0]);
	if (n < 0 || n > len)
		errc(1, ENAMETOOLONG, "%s", __func__);

	ps->ps_pr = parser_alloc(ps->ps_path, &cf);
	ps->ps_lx = parser_get_lexer(ps->ps_pr);
}

static void
parser_stub_destroy(struct parser_stub *ps)
{
	parser_free(ps->ps_pr);
	ps->ps_pr = NULL;

	if (ps->ps_fd[0] != -1)
		close(ps->ps_fd[0]);
	ps->ps_fd[0] = -1;
	if (ps->ps_fd[1] != -1)
		close(ps->ps_fd[1]);
	ps->ps_fd[1] = -1;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: t [-x]\n");
	exit(1);
}
